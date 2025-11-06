
#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
#include "matrix_lib.h"

// -------------------- Error handling --------------------
#ifndef CUDA_CHECK
#define CUDA_CHECK(expr) do {                                      \
    cudaError_t _err = (expr);                                     \
    if (_err != cudaSuccess) {                                     \
        fprintf(stderr, "CUDA error %s at %s:%d: %s\n",            \
                #expr, __FILE__, __LINE__, cudaGetErrorString(_err)); \
        return 0;                                                  \
    }                                                              \
} while(0)
#endif

// -------------------- Global launch configuration --------------------
static int g_threads_per_block = 256;
static int g_max_blocks_per_grid = 4096;

int set_grid_size(int threads_per_block, int max_blocks_per_grid) {
    // Query device limits
    cudaDeviceProp prop{};
    int dev = 0;
    if (cudaGetDevice(&dev) != cudaSuccess) dev = 0;
    cudaGetDeviceProperties(&prop, dev);

    int ok = 1;
    if (threads_per_block <= 0 || threads_per_block > prop.maxThreadsPerBlock) ok = 0;
    if (max_blocks_per_grid <= 0 || max_blocks_per_grid > prop.maxGridSize[0]) ok = 0;

    if (!ok) {
        // Fallback to defaults requested in the spec
        g_threads_per_block = 256;
        g_max_blocks_per_grid = 4096;
        return 0; // indicate error -> defaults in effect
    }

    g_threads_per_block = threads_per_block;
    g_max_blocks_per_grid = max_blocks_per_grid;
    return 1;
}

void compute_launch_config(size_t total_elems, dim3 *grid, dim3 *block) {
    int tpb = g_threads_per_block;
    size_t blocks = (total_elems + tpb - 1) / tpb;
    if ((long long)blocks > g_max_blocks_per_grid) blocks = g_max_blocks_per_grid;
    *block = dim3(tpb, 1, 1);
    *grid  = dim3((unsigned)blocks, 1, 1);
}

// -------------------- Kernels --------------------

// Scalar multiply: y = alpha * y  (in-place)
__global__ void k_scalar_mult(float *y, float alpha, size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;
    for (size_t i = idx; i < n; i += stride) {
        y[i] *= alpha;
    }
}

// C[MxN] = A[MxK] * B[KxN]  (1D linearized, each thread computes one element)
__global__ void k_matmul_full(const float *A, const float *B, float *C,
                              unsigned long M, unsigned long N, unsigned long K) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = (size_t)M * (size_t)N;
    if (idx >= total) return;
    unsigned long i = idx / N; // row in C
    unsigned long j = idx % N; // col in C
    float acc = 0.0f;
    const float *Arow = A + i * K;
    for (unsigned long k = 0; k < K; ++k) {
        acc += Arow[k] * B[k * N + j];
    }
    C[i * N + j] = acc;
}

// Computes one full output row: Crow = Arow[K] * B[KxN]
__global__ void k_matmul_row(const float *Arow, const float *B, float *Crow,
                             unsigned long N, unsigned long K) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;
    for (size_t j = idx; j < N; j += stride) {
        float acc = 0.0f;
        for (unsigned long k = 0; k < K; ++k) {
            acc += Arow[k] * B[k * N + j];
        }
        Crow[j] = acc;
    }
}

// -------------------- Library functions --------------------

int scalar_matrix_mult(float scalar_value, Matrix *matrix) {
    if (!matrix || !matrix->h_rows || !matrix->d_rows) return 0;
    unsigned long M = matrix->height;
    unsigned long N = matrix->width;
    size_t elems = (size_t)M * (size_t)N;
    if (elems == 0) return 0;

    if (matrix->alloc_mode == FULL_ALLOC) {
        // Copy full matrix host->device
        CUDA_CHECK(cudaMemcpy(matrix->d_rows, matrix->h_rows, elems * sizeof(float), cudaMemcpyHostToDevice));
        dim3 grid, block;
        compute_launch_config(elems, &grid, &block);
        k_scalar_mult<<<grid, block>>>(matrix->d_rows, scalar_value, elems);
        if (cudaDeviceSynchronize() != cudaSuccess) return 0;
        // Copy back
        CUDA_CHECK(cudaMemcpy(matrix->h_rows, matrix->d_rows, elems * sizeof(float), cudaMemcpyDeviceToHost));
        return 1;
    } else { // PARTIAL_ALLOC: 'd_rows' holds only one row
        if (matrix->width == 0) return 0;
        dim3 grid, block;
        compute_launch_config(N, &grid, &block);
        for (unsigned long i = 0; i < M; ++i) {
            float *h_row = matrix->h_rows + i * N;
            CUDA_CHECK(cudaMemcpy(matrix->d_rows, h_row, N * sizeof(float), cudaMemcpyHostToDevice));
            k_scalar_mult<<<grid, block>>>(matrix->d_rows, scalar_value, N);
            if (cudaDeviceSynchronize() != cudaSuccess) return 0;
            CUDA_CHECK(cudaMemcpy(h_row, matrix->d_rows, N * sizeof(float), cudaMemcpyDeviceToHost));
        }
        return 1;
    }
}

int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC) {
    if (!matrixA || !matrixB || !matrixC) return 0;
    if (!matrixA->h_rows || !matrixB->h_rows || !matrixC->h_rows) return 0;
    if (!matrixB->d_rows) return 0; // B must have device memory in any viable mode

    unsigned long M = matrixA->height;
    unsigned long K = matrixA->width;
    unsigned long Kb = matrixB->height;
    unsigned long N = matrixB->width;

    if (K != Kb) return 0;
    if (matrixC->height != M || matrixC->width != N) return 0;

    // Case 1: Full allocation for all
    if (matrixA->alloc_mode == FULL_ALLOC && matrixB->alloc_mode == FULL_ALLOC && matrixC->alloc_mode == FULL_ALLOC) {
        size_t elemsA = (size_t)M * (size_t)K;
        size_t elemsB = (size_t)K * (size_t)N;
        size_t elemsC = (size_t)M * (size_t)N;

        // Copy H->D
        CUDA_CHECK(cudaMemcpy(matrixA->d_rows, matrixA->h_rows, elemsA * sizeof(float), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(matrixB->d_rows, matrixB->h_rows, elemsB * sizeof(float), cudaMemcpyHostToDevice));

        dim3 grid, block;
        compute_launch_config(elemsC, &grid, &block);
        k_matmul_full<<<grid, block>>>(matrixA->d_rows, matrixB->d_rows, matrixC->d_rows, M, N, K);
        if (cudaDeviceSynchronize() != cudaSuccess) return 0;

        // Copy back
        CUDA_CHECK(cudaMemcpy(matrixC->h_rows, matrixC->d_rows, elemsC * sizeof(float), cudaMemcpyDeviceToHost));
        return 1;
    }

    // Case 2: B is FULL, A and C are PARTIAL (row buffers)
    if (matrixB->alloc_mode == FULL_ALLOC && matrixA->alloc_mode == PARTIAL_ALLOC && matrixC->alloc_mode == PARTIAL_ALLOC) {
        // Copy B once
        size_t elemsB = (size_t)K * (size_t)N;
        CUDA_CHECK(cudaMemcpy(matrixB->d_rows, matrixB->h_rows, elemsB * sizeof(float), cudaMemcpyHostToDevice));

        // For each row of A: copy row -> device buffer, compute Crow -> copy back
        dim3 grid, block;
        compute_launch_config(N, &grid, &block);

        for (unsigned long i = 0; i < M; ++i) {
            const float *Arow_h = matrixA->h_rows + i * K;
            float *Crow_h = matrixC->h_rows + i * N;

            CUDA_CHECK(cudaMemcpy(matrixA->d_rows, Arow_h, K * sizeof(float), cudaMemcpyHostToDevice));
            k_matmul_row<<<grid, block>>>(matrixA->d_rows, matrixB->d_rows, matrixC->d_rows, N, K);
            if (cudaDeviceSynchronize() != cudaSuccess) return 0;
            CUDA_CHECK(cudaMemcpy(Crow_h, matrixC->d_rows, N * sizeof(float), cudaMemcpyDeviceToHost));
        }
        return 1;
    }

    // Any other combination is unsupported in this work's spec
    fprintf(stderr, "Unsupported alloc_mode combination in matrix_matrix_mult.\n");
    return 0;
}
