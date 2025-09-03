#include "matrix_lib.h"
#include <stddef.h>

int scalar_matrix_mult(float scalar_value, struct matrix *matrix) {
    if (matrix == NULL || matrix->rows == NULL) {
        return 0;
    }
    unsigned long int i, j;
    for (i = 0; i < matrix->height; i++) {
        for (j = 0; j < matrix->width; j++) {
            matrix->rows[i * matrix->width + j] *= scalar_value;
        }
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

    for (unsigned long int i = 0; i < matrixC->height * matrixC->width; ++i) {
        matrixC->rows[i] = 0.0f;
    }


    unsigned long int i, j, k;
    for (i = 0; i < matrixA->height; i++) {
        for (k = 0; k < matrixA->width; k++) {

            float elemA = matrixA->rows[i * matrixA->width + k];
            for (j = 0; j < matrixB->width; j++) {
                matrixC->rows[i * matrixC->width + j] += elemA * matrixB->rows[k * matrixB->width + j];
            }
        }
    }

    return 1; // Sucesso
}
