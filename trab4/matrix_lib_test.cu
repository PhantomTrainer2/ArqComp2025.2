#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <cuda_runtime.h>
#include "matrix_lib.h"
#include "timer.h"

//Compilar: nvcc -o matrix_lib_test matrix_lib_test.cu matrix_lib.cu timer.c
//Rodar: ./matrix_lib_test 5 2048 2048 2048 2048 256 4096 1024 matA.dat matB.dat res1.dat res2.dat

static void print_first256(const Matrix *m, const char *title)
{
    char header[256];
    unsigned long total, limit, i;

    snprintf(header, sizeof(header),
             "\n--- Exibindo os primeiros 256 elementos de: %s ---\n",
             title);
    printf("%s", header);

    total = m->height * m->width;
    limit = (total < 256UL) ? total : 256UL;

    for (i = 0; i < limit; ++i) {
        char element_str[32];
        snprintf(element_str, sizeof(element_str),
                 "%10.2f ", m->h_rows[i]);
        printf("%s", element_str);

        if ((i + 1) % 8 == 0 || i == limit - 1) {
            printf("\n");
        }
    }
    printf("\n");
}

static int load_matrix(Matrix *m, const char *filename)
{
    FILE  *f;
    size_t need, got;

    f = fopen(filename, "rb");
    if (!f) {
        perror("Erro ao abrir arquivo de entrada");
        return 0;
    }

    need = (size_t)m->height * (size_t)m->width;
    got  = fread(m->h_rows, sizeof(float), need, f);
    fclose(f);

    if (got != need) {
        fprintf(stderr,
                "Erro de leitura em %s: esperado %zu, lido %zu\n",
                filename, need, got);
        return 0;
    }
    return 1;
}

static int save_matrix(const Matrix *m, const char *filename)
{
    FILE  *f;
    size_t cnt, wrote;

    f = fopen(filename, "wb");
    if (!f) {
        perror("Erro ao abrir arquivo de saída");
        return 0;
    }

    cnt   = (size_t)m->height * (size_t)m->width;
    wrote = fwrite(m->h_rows, sizeof(float), cnt, f);
    fclose(f);

    if (wrote != cnt) {
        fprintf(stderr, "Erro de escrita em %s\n", filename);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    struct timeval overall_t1, overall_t2, op_start, op_stop;

    printf("\nDados do CPU:\n\n");
    system("lscpu");

    /* Uso conforme trabalho 4:
       Valor HMat1 WMat1 HMat2 WMat2 ThreadsPorBloco MaxBlocosPorGrid
       MaxMemMiB ArqMat1 ArqMat2 ArqRes1 ArqRes2
    */
    if (argc != 13) {
        fprintf(stderr,
            "Uso: %s Valor HMat1 WMat1 HMat2 WMat2 ThreadsPorBloco "
            "MaxBlocosPorGrid MaxMemMiB ArqMat1 ArqMat2 ArqRes1 ArqRes2\n",
            argv[0]);
        return 1;
    }

    /* Leitura dos argumentos */
    {
        float         scalar_value;
        unsigned long Ah, Aw, Bh, Bw;
        int           threads_per_block;
        int           max_blocks_per_grid;
        size_t        max_mem_bytes;
        const char   *fileA;
        const char   *fileB;
        const char   *file_out1;
        const char   *file_out2;

        Matrix A;
        Matrix B;
        Matrix C;

        size_t bytesA, bytesB, bytesC;
        int    ok_grid;
        int    strategy; /* 0 = FULL, 1 = parcial A/C */

        scalar_value       = (float)atof(argv[1]);
        Ah                 = strtoul(argv[2],  NULL, 10);
        Aw                 = strtoul(argv[3],  NULL, 10);
        Bh                 = strtoul(argv[4],  NULL, 10);
        Bw                 = strtoul(argv[5],  NULL, 10);
        threads_per_block  = atoi(argv[6]);
        max_blocks_per_grid= atoi(argv[7]);
        max_mem_bytes      = (size_t)strtoull(argv[8], NULL, 10)
                             * 1024ULL * 1024ULL;
        fileA              = argv[9];
        fileB              = argv[10];
        file_out1          = argv[11];
        file_out2          = argv[12];

        if (Aw != Bh) {
            fprintf(stderr,
                    "Erro: Dimensões incompatíveis para multiplicação. "
                    "A.width (%lu) != B.height (%lu).\n", Aw, Bh);
            return 1;
        }

        /* Configuração de grid (pode cair nos defaults) */
        ok_grid = set_grid_size(threads_per_block, max_blocks_per_grid);
        if (!ok_grid) {
            fprintf(stderr,
                "Aviso: valores de grid inválidos; "
                "usando defaults (256 threads/block, 4096 blocks/grid)\n");
        }

        /* Inicializa structs Matrix em estilo C */
        A.height    = Ah;
        A.width     = Aw;
        A.h_rows    = NULL;
        A.d_rows    = NULL;
        A.alloc_mode= FULL_ALLOC;

        B.height    = Bh;
        B.width     = Bw;
        B.h_rows    = NULL;
        B.d_rows    = NULL;
        B.alloc_mode= FULL_ALLOC;

        C.height    = Ah;
        C.width     = Bw;
        C.h_rows    = NULL;
        C.d_rows    = NULL;
        C.alloc_mode= FULL_ALLOC;

        bytesA = (size_t)Ah * (size_t)Aw * sizeof(float);
        bytesB = (size_t)Bh * (size_t)Bw * sizeof(float);
        bytesC = (size_t)Ah * (size_t)Bw * sizeof(float);

        A.h_rows = (float*)aligned_alloc(32U, bytesA);
        B.h_rows = (float*)aligned_alloc(32U, bytesB);
        C.h_rows = (float*)aligned_alloc(32U, bytesC);

        if (A.h_rows == NULL || B.h_rows == NULL || C.h_rows == NULL) {
            fprintf(stderr,
                    "Erro: Falha na alocação de memória para as matrizes.\n");
            free(A.h_rows);
            free(B.h_rows);
            free(C.h_rows);
            return 1;
        }
        memset(C.h_rows, 0, bytesC);

        printf("\nCarregando matrizes dos arquivos...\n");
        if (!load_matrix(&A, fileA) || !load_matrix(&B, fileB)) {
            free(A.h_rows);
            free(B.h_rows);
            free(C.h_rows);
            return 1;
        }
        printf("Matrizes carregadas com sucesso.\n\n");

        print_first256(&A, "Matriz A (Original)");
        print_first256(&B, "Matriz B (Original)");

        /* --------- Estratégia de alocação na GPGPU (Obs. 2) --------- */
        strategy = 0; /* FULL por default */

        if (bytesA + bytesB + bytesC <= max_mem_bytes) {
            strategy       = 0;
            A.alloc_mode   = FULL_ALLOC;
            B.alloc_mode   = FULL_ALLOC;
            C.alloc_mode   = FULL_ALLOC;
        } else {
            size_t bytesArow, bytesCrow;
            bytesArow = (size_t)Aw * sizeof(float);
            bytesCrow = (size_t)Bw * sizeof(float);

            if (bytesB + bytesArow + bytesCrow <= max_mem_bytes) {
                strategy       = 1;
                A.alloc_mode   = PARTIAL_ALLOC;
                B.alloc_mode   = FULL_ALLOC;
                C.alloc_mode   = PARTIAL_ALLOC;
            } else {
                fprintf(stderr,
                    "Erro: Memória de GPGPU insuficiente para alocação "
                    "(nem FULL, nem parcial B+linhas de A/C).\n");
                free(A.h_rows);
                free(B.h_rows);
                free(C.h_rows);
                return 1;
            }
        }

        /* Alocação na GPU conforme estratégia */
        if (strategy == 0) {
            cudaMalloc((void**)&A.d_rows, bytesA);
            cudaMalloc((void**)&B.d_rows, bytesB);
            cudaMalloc((void**)&C.d_rows, bytesC);
        } else {
            size_t bytesArow, bytesCrow;
            bytesArow = (size_t)Aw * sizeof(float);
            bytesCrow = (size_t)Bw * sizeof(float);
            cudaMalloc((void**)&A.d_rows, bytesArow);
            cudaMalloc((void**)&B.d_rows, bytesB);
            cudaMalloc((void**)&C.d_rows, bytesCrow);
        }

        if (A.d_rows == NULL || B.d_rows == NULL || C.d_rows == NULL) {
            fprintf(stderr, "Falha ao alocar memória no device.\n");
            if (A.d_rows) cudaFree(A.d_rows);
            if (B.d_rows) cudaFree(B.d_rows);
            if (C.d_rows) cudaFree(C.d_rows);
            free(A.h_rows);
            free(B.h_rows);
            free(C.h_rows);
            return 1;
        }

        /* --------- Medição de tempos --------- */

        /* Janela do tempo total: engloba as duas operações */
        gettimeofday(&overall_t1, NULL);

        /* ---- scalar_matrix_mult ---- */
        {
            int  ok1;
            char tempo1[128];

            printf("Executando scalar_matrix_mult...\n");
            gettimeofday(&op_start, NULL);
            ok1 = scalar_matrix_mult(scalar_value, &A);
            gettimeofday(&op_stop, NULL);

            if (!ok1) {
                fprintf(stderr,
                        "Erro na execucao de scalar_matrix_mult.\n");
            } else {
                print_first256(&A,
                               "Matriz A (Apos Multiplicacao por Escalar)");

                snprintf(tempo1, sizeof(tempo1),
                    "Tempo de execucao do scalar_matrix_mult: %f ms\n",
                    timedifference_msec(op_start, op_stop));
                printf("%s", tempo1);

                if (save_matrix(&A, file_out1)) {
                    printf("Resultado 1 salvo em %s\n", file_out1);
                } else {
                    fprintf(stderr,
                            "Falha ao salvar %s\n", file_out1);
                }
            }
        }

        /* ---- matrix_matrix_mult ---- */
        {
            int  ok2;
            char tempo2[128];

            printf("\nExecutando matrix_matrix_mult...\n");
            memset(C.h_rows, 0, bytesC);

            gettimeofday(&op_start, NULL);
            ok2 = matrix_matrix_mult(&A, &B, &C);
            gettimeofday(&op_stop, NULL);

            if (!ok2) {
                fprintf(stderr,
                        "Erro na execucao de matrix_matrix_mult.\n");
            } else {
                print_first256(&C, "Matriz C (Resultado Final)");

                snprintf(tempo2, sizeof(tempo2),
                    "Tempo de execucao do matrix_matrix_mult: %f ms\n",
                    timedifference_msec(op_start, op_stop));
                printf("%s", tempo2);

                if (save_matrix(&C, file_out2)) {
                    printf("Resultado 2 salvo em %s\n", file_out2);
                } else {
                    fprintf(stderr,
                            "Falha ao salvar %s\n", file_out2);
                }
            }
        }

        /* Libera device + host */
        if (A.d_rows) cudaFree(A.d_rows);
        if (B.d_rows) cudaFree(B.d_rows);
        if (C.d_rows) cudaFree(C.d_rows);

        free(A.h_rows);
        free(B.h_rows);
        free(C.h_rows);
    }

    gettimeofday(&overall_t2, NULL);
    {
        char tempo3[128];
        snprintf(tempo3, sizeof(tempo3),
                 "\nTempo de execucao total: %f ms\n",
                 timedifference_msec(overall_t1, overall_t2));
        printf("%s", tempo3);
    }

    return 0;
}
