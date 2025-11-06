
#ifndef MATRIX_LIB_CUDA_H
#define MATRIX_LIB_CUDA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Allocation modes
#define PARTIAL_ALLOC 0
#define FULL_ALLOC    1

typedef struct matrix {
    unsigned long int height;   // rows (multiple of 8)
    unsigned long int width;    // cols (multiple of 8)
    float *h_rows;              // host pointer
    float *d_rows;              // device pointer (may be full size or only one row if PARTIAL_ALLOC)
    int alloc_mode;             // FULL_ALLOC (1) or PARTIAL_ALLOC (0)
} Matrix;

// Grid configuration (set by set_grid_size, used by kernels)
int set_grid_size(int threads_per_block, int max_blocks_per_grid);

// Core ops (must be called with device memory already allocated in matrix->d_rows as specified)
int scalar_matrix_mult(float scalar_value, Matrix *matrix);

int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC);

// Utility for mapping linear launch sizes
void compute_launch_config(size_t total_elems, dim3 *grid, dim3 *block);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_LIB_CUDA_H
