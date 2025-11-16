#ifndef MATRIX_LIB_CUDA_H
#define MATRIX_LIB_CUDA_H

#include <stddef.h>

/* Modo de alocação na GPGPU */
#define PARTIAL_ALLOC 0
#define FULL_ALLOC    1

typedef struct matrix {
    unsigned long int height;   /* número de linhas (múltiplo de 8) */
    unsigned long int width;    /* número de colunas (múltiplo de 8) */
    float *h_rows;              /* ponteiro na memória da CPU */
    float *d_rows;              /* ponteiro na memória da GPGPU */
    int   alloc_mode;           /* FULL_ALLOC ou PARTIAL_ALLOC */
} Matrix;

/* Configuração global de grid (usada internamente pela biblioteca) */
int set_grid_size(int threads_per_block, int max_blocks_per_grid);

/* Operações principais da biblioteca */
int scalar_matrix_mult(float scalar_value, Matrix *matrix);
int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC);

#endif /* MATRIX_LIB_CUDA_H */
