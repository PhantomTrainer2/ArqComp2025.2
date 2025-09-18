#include "matrix_lib.h"
#include <stddef.h>
#include <immintrin.h> // Header para as instruções AVX e FMA

// Função para multiplicar uma matriz por um escalar usando AVX
int scalar_matrix_mult(float scalar_value, struct matrix *matrix) {
    if (matrix == NULL || matrix->rows == NULL) {
        return 0; // Retorna erro se a matriz ou seus dados forem nulos
    }

    unsigned long int total_elements = matrix->height * matrix->width;
    
    // Cria um vetor de 256 bits com o valor escalar repetido 8 vezes
    __m256 scalar_vector = _mm256_set1_ps(scalar_value);

    // Itera sobre a matriz, processando 8 floats (256 bits) de cada vez
    for (unsigned long int i = 0; i < total_elements; i += 8) {
        // Carrega 8 floats da matriz para um vetor AVX
        __m256 matrix_vector = _mm256_loadu_ps(&matrix->rows[i]);
        
        // Multiplica os dois vetores (8 floats da matriz * 8 floats do escalar)
        __m256 result_vector = _mm256_mul_ps(matrix_vector, scalar_vector);
        
        // Armazena o resultado de volta na matriz
        _mm256_storeu_ps(&matrix->rows[i], result_vector);
    }
    
    return 1; // Sucesso
}

// Função para multiplicar duas matrizes usando o algoritmo otimizado com AVX e FMA
int matrix_matrix_mult(struct matrix *matrixA, struct matrix *matrixB, struct matrix *matrixC) {
    if (matrixA == NULL || matrixB == NULL || matrixC == NULL ||
        matrixA->rows == NULL || matrixB->rows == NULL || matrixC->rows == NULL) {
        return 0; // Retorna erro em caso de ponteiros nulos
    }

    // Validação das dimensões para multiplicação
    if (matrixA->width != matrixB->height) {
        return 0;
    }
    if (matrixC->height != matrixA->height || matrixC->width != matrixB->width) {
        return 0;
    }

    unsigned long int total_elements_C = matrixC->height * matrixC->width;
    
    // Otimização: Inicializa a matriz C com zeros usando AVX
    __m256 zero_vector = _mm256_setzero_ps();
    for (unsigned long int i = 0; i < total_elements_C; i += 8) {
        _mm256_storeu_ps(&matrixC->rows[i], zero_vector);
    }

    // Algoritmo de multiplicação de matriz otimizado (i, k, j) com FMA
    for (unsigned long int i = 0; i < matrixA->height; i++) {
        for (unsigned long int k = 0; k < matrixA->width; k++) {
            // Carrega o elemento de A e o replica 8 vezes em um vetor AVX
            __m256 scalar_A = _mm256_set1_ps(matrixA->rows[i * matrixA->width + k]);
            
            // Itera sobre as colunas de B e C, processando 8 floats por vez
            for (unsigned long int j = 0; j < matrixB->width; j += 8) {
                // Carrega 8 floats da linha k de B
                __m256 vector_B = _mm256_loadu_ps(&matrixB->rows[k * matrixB->width + j]);
                
                // Carrega 8 floats da linha i de C (valores acumulados)
                __m256 vector_C = _mm256_loadu_ps(&matrixC->rows[i * matrixC->width + j]);
                
                // Operação Fused Multiply-Add: C = (A * B) + C
                vector_C = _mm256_fmadd_ps(scalar_A, vector_B, vector_C);
                
                // Armazena o resultado acumulado de volta em C
                _mm256_storeu_ps(&matrixC->rows[i * matrixC->width + j], vector_C);
            }
        }
    }

    return 1; // Sucesso
}
