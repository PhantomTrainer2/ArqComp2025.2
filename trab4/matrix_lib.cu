#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cuda_runtime.h>
#include "matrix_lib.h"

/* ============================================================
 * Macros de erro CUDA
 * ============================================================ */
#ifndef CUDA_CHECK
#define CUDA_CHECK(expr)                                                  \
    do {                                                                  \
        cudaError_t _err = (expr);                                        \
        if (_err != cudaSuccess) {                                        \
            fprintf(stderr,                                               \
                    "CUDA error %s at %s:%d: %s\n",                       \
                    #expr, __FILE__, __LINE__, cudaGetErrorString(_err)); \
            return 0;                                                     \
        }                                                                 \
    } while (0)
#endif

/* ============================================================
 * Configuração global de grid (usada pela biblioteca)
 * ============================================================ */
static int g_threads_per_block   = 256;
static int g_max_blocks_per_grid = 4096;

int set_grid_size(int threads_per_block, int max_blocks_per_grid)
{
    cudaDeviceProp prop;
    int dev = 0;
    int ok  = 1;

    memset(&prop, 0, sizeof(cudaDeviceProp));

    if (cudaGetDevice(&dev) != cudaSuccess) {
        dev = 0;
    }
    cudaGetDeviceProperties(&prop, dev);

    if (threads_per_block <= 0 ||
        threads_per_block > prop.maxThreadsPerBlock) {
        ok = 0;
    }

    if (max_blocks_per_grid <= 0 ||
        max_blocks_per_grid > prop.maxGridSize[0]) {
        ok = 0;
    }

    if (!ok) {
        g_threads_per_block   = 256;
        g_max_blocks_per_grid = 4096;
        return 0;  /* erro -> usa defaults */
    }

    g_threads_per_block   = threads_per_block;
    g_max_blocks_per_grid = max_blocks_per_grid;
    return 1;
}

/* Helper 1D para scalar / matmul FULL / modo parcial */
static void compute_launch_config(size_t total_elems,
                                  dim3 *grid, dim3 *block)
{
    int    tpb    = g_threads_per_block;
    size_t blocks = (total_elems + (size_t)tpb - 1U) / (size_t)tpb;

    if ((long long)blocks > (long long)g_max_blocks_per_grid) {
        blocks = (size_t)g_max_blocks_per_grid;
    }

    block->x = (unsigned int)tpb;
    block->y = 1U;
    block->z = 1U;

    grid->x = (unsigned int)blocks;
    grid->y = 1U;
    grid->z = 1U;
}

/* ============================================================
 * Kernels
 * ============================================================ */

/* y = alpha * y (in-place), 1D grid-stride */
__global__ void k_scalar_mult(float *y, float alpha, size_t n)
{
    size_t idx    = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = (size_t)blockDim.x * (size_t)gridDim.x;

    while (idx < n) {
        y[idx] *= alpha;
        idx += stride;
    }
}

/* Matmul FULL:
 * C[MxN] = A[MxK] * B[KxN]
 * 1D grid-stride sobre C, cada thread calcula 1+ elementos
 */
__global__ void k_matmul_full(const float *A,
                              const float *B,
                              float *C,
                              unsigned long M,
                              unsigned long N,
                              unsigned long K)
{
    size_t total = (size_t)M * (size_t)N;
    size_t idx   = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride= (size_t)blockDim.x * (size_t)gridDim.x;

    while (idx < total) {
        unsigned long i = (unsigned long)(idx / (size_t)N);
        unsigned long j = (unsigned long)(idx % (size_t)N);

        float acc = 0.0f;
        unsigned long k;
        const float *Arow = A + i * K;

        for (k = 0; k < K; ++k) {
            acc += Arow[k] * B[k * N + j];
        }

        C[i * N + j] = acc;
        idx += stride;
    }
}

/* Calcula uma linha de C (modo parcial):
 * Crow = Arow[K] * B[KxN]
 */
__global__ void k_matmul_row(const float *Arow,
                             const float *B,
                             float *Crow,
                             unsigned long N,
                             unsigned long K)
{
    size_t idx    = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = (size_t)blockDim.x * (size_t)gridDim.x;

    while (idx < (size_t)N) {
        unsigned long j = (unsigned long)idx;
        float acc = 0.0f;
        unsigned long k;

        for (k = 0; k < K; ++k) {
            acc += Arow[k] * B[k * N + j];
        }
        Crow[j] = acc;

        idx += stride;
    }
}

/* ============================================================
 * Implementação das funções da biblioteca
 * ============================================================ */

int scalar_matrix_mult(float scalar_value, Matrix *matrix)
{
    unsigned long M, N;
    size_t        elems;
    dim3          grid, block;
    unsigned long i;

    if (matrix == NULL ||
        matrix->h_rows == NULL ||
        matrix->d_rows == NULL) {
        return 0;
    }

    M     = matrix->height;
    N     = matrix->width;
    elems = (size_t)M * (size_t)N;

    if (elems == 0U) {
        return 0;
    }

    if (matrix->alloc_mode == FULL_ALLOC) {
        /* matriz inteira na GPU */
        CUDA_CHECK(cudaMemcpy(matrix->d_rows,
                              matrix->h_rows,
                              elems * sizeof(float),
                              cudaMemcpyHostToDevice));

        compute_launch_config(elems, &grid, &block);
        k_scalar_mult<<<grid, block>>>(matrix->d_rows,
                                       scalar_value,
                                       elems);

        if (cudaDeviceSynchronize() != cudaSuccess) {
            return 0;
        }

        CUDA_CHECK(cudaMemcpy(matrix->h_rows,
                              matrix->d_rows,
                              elems * sizeof(float),
                              cudaMemcpyDeviceToHost));
        return 1;
    }
    else { /* PARTIAL_ALLOC: d_rows tem só 1 linha */
        if (matrix->width == 0U) {
            return 0;
        }

        compute_launch_config((size_t)N, &grid, &block);

        for (i = 0; i < M; ++i) {
            float *h_row = matrix->h_rows + i * N;

            CUDA_CHECK(cudaMemcpy(matrix->d_rows,
                                  h_row,
                                  (size_t)N * sizeof(float),
                                  cudaMemcpyHostToDevice));

            k_scalar_mult<<<grid, block>>>(matrix->d_rows,
                                           scalar_value,
                                           (size_t)N);

            if (cudaDeviceSynchronize() != cudaSuccess) {
                return 0;
            }

            CUDA_CHECK(cudaMemcpy(h_row,
                                  matrix->d_rows,
                                  (size_t)N * sizeof(float),
                                  cudaMemcpyDeviceToHost));
        }
        return 1;
    }
}

int matrix_matrix_mult(Matrix *matrixA,
                       Matrix *matrixB,
                       Matrix *matrixC)
{
    unsigned long M, N, K, Kb;
    size_t        elemsA, elemsB, elemsC;
    dim3          grid, block;
    unsigned long i;

    if (matrixA == NULL || matrixB == NULL || matrixC == NULL) {
        return 0;
    }
    if (matrixA->h_rows == NULL ||
        matrixB->h_rows == NULL ||
        matrixC->h_rows == NULL) {
        return 0;
    }
    if (matrixB->d_rows == NULL) {
        return 0;
    }

    M  = matrixA->height;
    K  = matrixA->width;
    Kb = matrixB->height;
    N  = matrixB->width;

    if (K != Kb) {
        return 0;
    }
    if (matrixC->height != M ||
        matrixC->width  != N) {
        return 0;
    }

    /* Caso 1: FULL_ALLOC para A, B e C */
    if (matrixA->alloc_mode == FULL_ALLOC &&
        matrixB->alloc_mode == FULL_ALLOC &&
        matrixC->alloc_mode == FULL_ALLOC) {

        elemsA = (size_t)M * (size_t)K;
        elemsB = (size_t)K * (size_t)N;
        elemsC = (size_t)M * (size_t)N;

        CUDA_CHECK(cudaMemcpy(matrixA->d_rows,
                              matrixA->h_rows,
                              elemsA * sizeof(float),
                              cudaMemcpyHostToDevice));

        CUDA_CHECK(cudaMemcpy(matrixB->d_rows,
                              matrixB->h_rows,
                              elemsB * sizeof(float),
                              cudaMemcpyHostToDevice));

        /* grid e block definidos por set_grid_size */
        compute_launch_config(elemsC, &grid, &block);

        k_matmul_full<<<grid, block>>>(matrixA->d_rows,
                                       matrixB->d_rows,
                                       matrixC->d_rows,
                                       M, N, K);

        if (cudaDeviceSynchronize() != cudaSuccess) {
            return 0;
        }

        CUDA_CHECK(cudaMemcpy(matrixC->h_rows,
                              matrixC->d_rows,
                              elemsC * sizeof(float),
                              cudaMemcpyDeviceToHost));
        return 1;
    }

    /* Caso 2: B FULL, A/C PARTIAL (stream por linha) */
    if (matrixB->alloc_mode == FULL_ALLOC &&
        matrixA->alloc_mode == PARTIAL_ALLOC &&
        matrixC->alloc_mode == PARTIAL_ALLOC) {

        size_t elemsB2   = (size_t)K * (size_t)N;
        size_t bytesArow = (size_t)K * sizeof(float);
        size_t bytesCrow = (size_t)N * sizeof(float);

        /* copia B inteira uma vez */
        CUDA_CHECK(cudaMemcpy(matrixB->d_rows,
                              matrixB->h_rows,
                              elemsB2 * sizeof(float),
                              cudaMemcpyHostToDevice));

        /* grid e block definidos por set_grid_size */
        compute_launch_config((size_t)N, &grid, &block);

        for (i = 0; i < M; ++i) {
            const float *Arow_h = matrixA->h_rows + i * K;
            float       *Crow_h = matrixC->h_rows + i * N;

            CUDA_CHECK(cudaMemcpy(matrixA->d_rows,
                                  Arow_h,
                                  bytesArow,
                                  cudaMemcpyHostToDevice));

            k_matmul_row<<<grid, block>>>(matrixA->d_rows,
                                          matrixB->d_rows,
                                          matrixC->d_rows,
                                          N, K);

            if (cudaDeviceSynchronize() != cudaSuccess) {
                return 0;
            }

            CUDA_CHECK(cudaMemcpy(Crow_h,
                                  matrixC->d_rows,
                                  bytesCrow,
                                  cudaMemcpyDeviceToHost));
        }
        return 1;
    }

    fprintf(stderr,
            "Unsupported alloc_mode combination in matrix_matrix_mult.\n");
    return 0;
}
