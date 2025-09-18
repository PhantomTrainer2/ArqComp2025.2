#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "matrix_lib.h"
#include "timer.h"
#include <immintrin.h>


void print_matrix(struct matrix *m, const char *title) {

    char texto[256];
    snprintf(texto, sizeof(texto), "\n--- Exibindo os primeiros 256 elementos de: %s ---\n", title);
    
    printf("%s", texto);

    for (unsigned long i = 0; i < 256; i++) {
        char element_str[32];
        snprintf(element_str, sizeof(element_str), "%10.2f ", m->rows[i]);
        
        printf("%s", element_str);

        if ((i + 1) % 8 == 0 || i == 256 - 1) {
            printf("\n");
        }
    }
    printf("\n");
}


int load_matrix_from_file(struct matrix *m, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Erro ao abrir arquivo de entrada");
        return 0;
    }
    size_t elements_to_read = m->height * m->width;
    size_t elements_read = fread(m->rows, sizeof(float), elements_to_read, file);
    fclose(file);

    if (elements_read != elements_to_read) {
        fprintf(stderr, "Erro de leitura em %s: esperado %zu elementos, lido %zu\n", filename, elements_to_read, elements_read);
        return 0;
    }
    return 1;
}


int save_matrix_to_file(struct matrix *m, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Erro ao abrir arquivo de saída");
        return 0;
    }
    size_t elements_to_write = m->height * m->width;
    size_t elements_written = fwrite(m->rows, sizeof(float), elements_to_write, file);
    fclose(file);

    if (elements_written != elements_to_write) {
        fprintf(stderr, "Erro de escrita em %s\n", filename);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {

    printf("\nDados do CPU:\n\n");
    system("lscpu");
    struct timeval overall_t1, overall_t2, op_start, op_stop;
    
    gettimeofday(&overall_t1, NULL);

    if (argc != 10) {
        fprintf(stderr, "Uso: %s Valor HMat1 WMat1 HMat2 WMat2 ArqMat1 ArqMat2 ArqRes1 ArqRes2\n", argv[0]);
        return 1;
    }

    float scalar_value = atof(argv[1]);
    
    struct matrix A, B, C;
    A.height = strtoul(argv[2], NULL, 10);
    A.width  = strtoul(argv[3], NULL, 10);
    B.height = strtoul(argv[4], NULL, 10);
    B.width  = strtoul(argv[5], NULL, 10);

    const char *file_A_in = argv[6];
    const char *file_B_in = argv[7];
    const char *file_res1_out = argv[8];
    const char *file_res2_out = argv[9];
    
    if (A.width != B.height) {
        fprintf(stderr, "Erro: Dimensões incompatíveis para multiplicação. A.width (%lu) != B.height (%lu).\n", A.width, B.height);
        return 1;
    }

    A.rows = (float*) malloc(A.height * A.width * sizeof(float));
    B.rows = (float*) malloc(B.height * B.width * sizeof(float));
    C.height = A.height;
    C.width = B.width;
    C.rows = (float*) malloc(C.height * C.width * sizeof(float));

    if (A.rows == NULL || B.rows == NULL || C.rows == NULL) {
        fprintf(stderr, "Erro: Falha na alocação de memória para as matrizes.\n");
        free(A.rows); free(B.rows); free(C.rows);
        return 1;
    }

    printf("\nCarregando matrizes dos arquivos...\n");
    if (!load_matrix_from_file(&A, file_A_in) || !load_matrix_from_file(&B, file_B_in)) {
        free(A.rows); free(B.rows); free(C.rows);
        return 1;
    }
    printf("Matrizes carregadas com sucesso.\n\n");

    print_matrix(&A, "Matriz A (Original)");
    print_matrix(&B, "Matriz B (Original)");

    printf("Executando scalar_matrix_mult...\n");
    gettimeofday(&op_start, NULL);
    if (scalar_matrix_mult(scalar_value, &A)) {
        gettimeofday(&op_stop, NULL);
        
        print_matrix(&A, "Matriz A (Apos Multiplicacao por Escalar)");
        
        char tempo1[128];
        snprintf(tempo1, sizeof(tempo1), "Tempo de execucao do scalar_matrix_mult: %f ms\n", timedifference_msec(op_start, op_stop));
        printf("%s", tempo1);
        
        if (save_matrix_to_file(&A, file_res1_out)) {
             printf("Resultado 1 salvo em %s\n", file_res1_out);
        }
    } else {
        fprintf(stderr, "Erro na execucao de scalar_matrix_mult.\n");
    }

    printf("\nExecutando matrix_matrix_mult...\n");
    gettimeofday(&op_start, NULL);
    if (matrix_matrix_mult(&A, &B, &C)) {
        gettimeofday(&op_stop, NULL);
        
        // <<< IMPRIME A MATRIZ C (RESULTADO FINAL) >>>
        print_matrix(&C, "Matriz C (Resultado Final)");

        char tempo2[128];
        snprintf(tempo2, sizeof(tempo2), "Tempo de execucao do matrix_matrix_mult: %f ms\n", timedifference_msec(op_start, op_stop));
        printf("%s", tempo2);

        if (save_matrix_to_file(&C, file_res2_out)) {
             printf("Resultado 2 salvo em %s\n", file_res2_out);
        }
    } else {
        fprintf(stderr, "Erro na execucao de matrix_matrix_mult.\n");
    }
    
    free(A.rows);
    free(B.rows);
    free(C.rows);

    gettimeofday(&overall_t2, NULL);
    char tempo3[128];
    snprintf(tempo3, sizeof(tempo3), "\nTempo de execucao total: %f ms\n", timedifference_msec(overall_t1, overall_t2));
    printf("%s", tempo3);

    return 0;
}
