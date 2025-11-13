#ifndef MATRIX_LIB_CUDA_H
#define MATRIX_LIB_CUDA_H

#include <stddef.h>

#define PARTIAL_ALLOC 0
#define FULL_ALLOC    1

typedef struct matrix {
    unsigned long int height;
    unsigned long int width;
    float *h_rows;
    float *d_rows;
    int   alloc_mode;
} Matrix;

int set_grid_size(int threads_per_block, int max_blocks_per_grid);
int scalar_matrix_mult(float scalar_value, Matrix *matrix);
int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC);

#endif /* MATRIX_LIB_CUDA_H */
