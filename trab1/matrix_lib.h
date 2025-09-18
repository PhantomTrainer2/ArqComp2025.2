struct matrix {
    unsigned long int height; // Número de linhas
    unsigned long int width;  // Número de colunas
    float *rows;              // Ponteiro para os dados da matriz (armazenados em linha)
};

int scalar_matrix_mult(float scalar_value, struct matrix *matrix);
int matrix_matrix_mult(struct matrix *matrixA, struct matrix * matrixB, struct matrix * matrixC);
