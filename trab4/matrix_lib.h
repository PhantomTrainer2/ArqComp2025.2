#ifndef MATRIX_LIB
#define MATRIX_LIB

typedef struct matrix {
    unsigned long int height;
    unsigned long int width;
    float *rows;
} Matrix;

void set_number_threads(int num_threads);
int scalar_matrix_mult(float scalar_value, Matrix *matrix);
int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC);

#endif
