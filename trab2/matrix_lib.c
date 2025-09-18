#include "matrix_lib.h"
#include <stddef.h>
#include <immintrin.h>

int scalar_matrix_mult(float scalar_value, struct matrix *matrix) {
    if (matrix == NULL || matrix->rows == NULL) {
        return 0;
    }

    unsigned long int total_elements = matrix->height * matrix->width;
    
    __m256 scalar_vector = _mm256_set1_ps(scalar_value);

    for (unsigned long int i = 0; i < total_elements; i += 8) {
        __m256 matrix_vector = _mm256_loadu_ps(&matrix->rows[i]);
        
        __m256 result_vector = _mm256_mul_ps(matrix_vector, scalar_vector);
        
        _mm256_storeu_ps(&matrix->rows[i], result_vector);
    }
    
    return 1; 
}

int matrix_matrix_mult(struct matrix *matrixA, struct matrix *matrixB, struct matrix *matrixC) {
    if (matrixA == NULL || matrixB == NULL || matrixC == NULL ||
        matrixA->rows == NULL || matrixB->rows == NULL || matrixC->rows == NULL) {
        return 0;
    }

    if (matrixA->width != matrixB->height) {
        return 0;
    }
    if (matrixC->height != matrixA->height || matrixC->width != matrixB->width) {
        return 0;
    }

    unsigned long int total_elements_C = matrixC->height * matrixC->width;
    
    __m256 zero_vector = _mm256_setzero_ps();
    for (unsigned long int i = 0; i < total_elements_C; i += 8) {
        _mm256_storeu_ps(&matrixC->rows[i], zero_vector);
    }

    for (unsigned long int i = 0; i < matrixA->height; i++) {
        for (unsigned long int k = 0; k < matrixA->width; k++) {
            __m256 scalar_A = _mm256_set1_ps(matrixA->rows[i * matrixA->width + k]);
            
            for (unsigned long int j = 0; j < matrixB->width; j += 8) {

                __m256 vector_B = _mm256_loadu_ps(&matrixB->rows[k * matrixB->width + j]);
                
                __m256 vector_C = _mm256_loadu_ps(&matrixC->rows[i * matrixC->width + j]);
                
                vector_C = _mm256_fmadd_ps(scalar_A, vector_B, vector_C);
                
                _mm256_storeu_ps(&matrixC->rows[i * matrixC->width + j], vector_C);
            }
        }
    }

    return 1;
}
