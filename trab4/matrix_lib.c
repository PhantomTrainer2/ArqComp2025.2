#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>
#include <pthread.h>
#include "matrix_lib.h"

#define VEC_STEP 8

int THREAD_COUNT = 1;

void set_number_threads(int num_threads) {
    THREAD_COUNT = num_threads;
}

int isValidMatrix(Matrix *matrix)
{
    return (matrix != NULL && matrix->rows != NULL && matrix->height % 8 == 0 && matrix->width % 8 == 0);
}

typedef struct {
    float scalar_value;
    Matrix *matrix;
    unsigned long int start_index;
    unsigned long int end_index;
} ScalarArgs;


void* thread_scalar_mult(void* args) {
    ScalarArgs* thread_arg = (ScalarArgs*)args;
    __m256 scalar = _mm256_set1_ps(thread_arg->scalar_value);

    for (int i = thread_arg->start_index; i < thread_arg->end_index; i += VEC_STEP) {
        float* vec_next = thread_arg->matrix->rows + i;
        __m256 vec = _mm256_load_ps(vec_next);
        __m256 result = _mm256_mul_ps(scalar, vec);
        _mm256_store_ps(vec_next, result);
    }

    return NULL;
}


int scalar_matrix_mult(float scalar_value, Matrix* matrix){
    if (!isValidMatrix(matrix)) {
        return 0;
    }

    pthread_t threads[THREAD_COUNT];
    ScalarArgs arguments[THREAD_COUNT];

    unsigned long int matrix_size = matrix->height * matrix->width;
    unsigned long int chunk_size = matrix_size / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        arguments[i].scalar_value = scalar_value;
        arguments[i].matrix = matrix;
        arguments[i].start_index = i * chunk_size;
        arguments[i].end_index = (i == THREAD_COUNT - 1) ? matrix_size : (i + 1) * chunk_size;   
        pthread_create(&threads[i], NULL, thread_scalar_mult, (void *)&arguments[i]);
    }

    for(int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 1;
}

typedef struct {
    Matrix *matrixA;
    Matrix *matrixB;
    Matrix *matrixC;
    unsigned long int start_row;
    unsigned long int end_row;
} MatrixArgs;

void* thread_matrix_mult(void* arg) {
    MatrixArgs* thread_arg = (MatrixArgs*)arg;
    Matrix *matrixA = thread_arg->matrixA;
    Matrix *matrixB = thread_arg->matrixB;
    Matrix *matrixC = thread_arg->matrixC;
    
    unsigned long int a_width = matrixA->width;
    unsigned long int b_width = matrixB->width;
    
    for (unsigned long int i = thread_arg->start_row; i < thread_arg->end_row; i++) {
        float *a_row = &matrixA->rows[i * a_width];
        float *c_row = &matrixC->rows[i * b_width];
        
        for (unsigned long int k = 0; k < a_width; k++) {
            float a_ik = a_row[k];
            __m256 scalar = _mm256_set1_ps(a_ik);
            float *b_row = &matrixB->rows[k * b_width];
            
            for (unsigned long int j = 0; j < b_width; j += VEC_STEP) {
                __m256 vec_b = _mm256_load_ps(&b_row[j]);
                __m256 vec_c = _mm256_load_ps(&c_row[j]);
                vec_c = _mm256_fmadd_ps(scalar, vec_b, vec_c);
                _mm256_store_ps(&c_row[j], vec_c);
            }
        }
    }
    
    return NULL;
}

int matrix_matrix_mult(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC) {
    if (!isValidMatrix(matrixA) || !isValidMatrix(matrixB) || !isValidMatrix(matrixC) ||
        matrixA->width != matrixB->height ||
        matrixC->height != matrixA->height ||
        matrixC->width != matrixB->width) {
        return 0;
    }
    
    pthread_t threads[THREAD_COUNT];
    MatrixArgs thread_args[THREAD_COUNT];
    unsigned long int rows_per_thread = matrixA->height / THREAD_COUNT;
    
    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_args[i].matrixA = matrixA;
        thread_args[i].matrixB = matrixB;
        thread_args[i].matrixC = matrixC;
        thread_args[i].start_row = i * rows_per_thread;
        thread_args[i].end_row = (i == THREAD_COUNT - 1) ? matrixA->height : (i + 1) * rows_per_thread;
        
        pthread_create(&threads[i], NULL, thread_matrix_mult, (void*)&thread_args[i]);
    }
    
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 1;
}
