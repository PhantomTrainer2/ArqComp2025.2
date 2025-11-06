
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <cuda_runtime.h>
#include "matrix_lib.h"
#include "timer.h"

static void print_first256(const Matrix *m, const char *title) {
    printf("\n--- Exibindo os primeiros 256 elementos de: %s ---\n", title);
    unsigned long total = m->height * m->width;
    unsigned long limit = total < 256 ? total : 256;
    for (unsigned long i = 0; i < limit; ++i) {
        printf("%10.2f ", m->h_rows[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    if (limit % 8 != 0) printf("\n");
}

static int load_matrix(Matrix *m, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("Erro ao abrir arquivo de entrada"); return 0; }
    size_t need = (size_t)m->height * (size_t)m->width;
    size_t got = fread(m->h_rows, sizeof(float), need, f);
    fclose(f);
    if (got != need) {
        fprintf(stderr, "Erro de leitura em %s: esperado %zu, lido %zu\n", filename, need, got);
        return 0;
    }
    return 1;
}

static int save_matrix(const Matrix *m, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) { perror("Erro ao abrir arquivo de saída"); return 0; }
    size_t cnt = (size_t)m->height * (size_t)m->width;
    size_t wrote = fwrite(m->h_rows, sizeof(float), cnt, f);
    fclose(f);
    if (wrote != cnt) { fprintf(stderr, "Erro de escrita em %s\n", filename); return 0; }
    return 1;
}

int main(int argc, char **argv) {
    printf("\nDados do CPU:\n\n");
    system("lscpu");

    struct timeval overall_t0, overall_t1, t0, t1;
    gettimeofday(&overall_t0, NULL);

    // Expected:
    // 1: scalar
    // 2: Ah, 3: Aw, 4: Bh, 5: Bw
    // 6: threads_per_block
    // 7: max_blocks_per_grid
    // 8: max_mem_mib
    // 9: fileA, 10: fileB, 11: out1, 12: out2
    if (argc != 13) {
        fprintf(stderr, "Uso: %s <scalar> <Ah> <Aw> <Bh> <Bw> <threads_per_block> <max_blocks_per_grid> <max_mem_mib> <fileA> <fileB> <out1> <out2>\n", argv[0]);
        return 1;
    }

    float scalar_value       = static_cast<float>(atof(argv[1]));
    unsigned long Ah         = strtoul(argv[2],  NULL, 10);
    unsigned long Aw         = strtoul(argv[3],  NULL, 10);
    unsigned long Bh         = strtoul(argv[4],  NULL, 10);
    unsigned long Bw         = strtoul(argv[5],  NULL, 10);
    int threads_per_block    = atoi(argv[6]);
    int max_blocks_per_grid  = atoi(argv[7]);
    size_t max_mem_bytes     = (size_t)strtoull(argv[8], NULL, 10) * 1024ULL * 1024ULL;
    const char *fileA        = argv[9];
    const char *fileB        = argv[10];
    const char *file_out1    = argv[11];
    const char *file_out2    = argv[12];

    if (Aw != Bh) {
        fprintf(stderr, "Erro: Dimensões incompatíveis: Aw (%lu) != Bh (%lu)\n", Aw, Bh);
        return 1;
    }

    // Configure launch params (falls back to defaults if invalid)
    int ok_grid = set_grid_size(threads_per_block, max_blocks_per_grid);
    if (!ok_grid) {
        fprintf(stderr, "Aviso: valores de grid inválidos; usando defaults (256 threads/block, 4096 blocks/grid)\n");
    }

    // Host allocations
    Matrix A{Ah, Aw, nullptr, nullptr, FULL_ALLOC};
    Matrix B{Bh, Bw, nullptr, nullptr, FULL_ALLOC};
    Matrix C{Ah, Bw, nullptr, nullptr, FULL_ALLOC};

    size_t bytesA = (size_t)Ah * (size_t)Aw * sizeof(float);
    size_t bytesB = (size_t)Bh * (size_t)Bw * sizeof(float);
    size_t bytesC = (size_t)Ah * (size_t)Bw * sizeof(float);

    A.h_rows = (float*)aligned_alloc(32, bytesA);
    B.h_rows = (float*)aligned_alloc(32, bytesB);
    C.h_rows = (float*)aligned_alloc(32, bytesC);
    if (!A.h_rows || !B.h_rows || !C.h_rows) {
        fprintf(stderr, "Falha ao alocar memória na CPU.\n");
        return 1;
    }
    memset(C.h_rows, 0, bytesC);

    // Load input files
    if (!load_matrix(&A, fileA) || !load_matrix(&B, fileB)) {
        free(A.h_rows); free(B.h_rows); free(C.h_rows);
        return 1;
    }

    // Decide device allocation strategy (Observation 2)
    // Try FULL for all matrices
    int strategy = 0; // 0 = FULL, 1 = partial A/C
    if (bytesA + bytesB + bytesC <= max_mem_bytes) {
        strategy = 0;
        A.alloc_mode = FULL_ALLOC;
        B.alloc_mode = FULL_ALLOC;
        C.alloc_mode = FULL_ALLOC;
    } else {
        // Try B full + 1 row(A) + 1 row(C)
        size_t bytesArow = (size_t)Aw * sizeof(float);
        size_t bytesCrow = (size_t)Bw * sizeof(float);
        if (bytesB + bytesArow + bytesCrow <= max_mem_bytes) {
            strategy = 1;
            A.alloc_mode = PARTIAL_ALLOC;
            B.alloc_mode = FULL_ALLOC;
            C.alloc_mode = PARTIAL_ALLOC;
        } else {
            fprintf(stderr, "Erro: Memória de GPGPU insuficiente para alocação (nem FULL, nem parcial B+linhas de A/C).\n");
            free(A.h_rows); free(B.h_rows); free(C.h_rows);
            return 1;
        }
    }

    // Allocate device memory according to strategy
    if (strategy == 0) {
        cudaMalloc((void**)&A.d_rows, bytesA);
        cudaMalloc((void**)&B.d_rows, bytesB);
        cudaMalloc((void**)&C.d_rows, bytesC);
    } else {
        size_t bytesArow = (size_t)Aw * sizeof(float);
        size_t bytesCrow = (size_t)Bw * sizeof(float);
        cudaMalloc((void**)&A.d_rows, bytesArow);
        cudaMalloc((void**)&B.d_rows, bytesB);
        cudaMalloc((void**)&C.d_rows, bytesCrow);
    }

    if (!A.d_rows || !B.d_rows || !C.d_rows) {
        fprintf(stderr, "Falha ao alocar memória no device.\n");
        if (A.d_rows) cudaFree(A.d_rows);
        if (B.d_rows) cudaFree(B.d_rows);
        if (C.d_rows) cudaFree(C.d_rows);
        free(A.h_rows); free(B.h_rows); free(C.h_rows);
        return 1;
    }

    // ---- scalar_matrix_mult ----
    printf("\nExecutando scalar_matrix_mult...\n");
    gettimeofday(&t0, NULL);
    int ok1 = scalar_matrix_mult(scalar_value, &A);
    gettimeofday(&t1, NULL);
    if (!ok1) {
        fprintf(stderr, "Erro em scalar_matrix_mult.\n");
    } else {
        print_first256(&A, "Matriz A (após multiplicação por escalar)");
        printf("Tempo scalar_matrix_mult: %f ms\n", timedifference_msec(t0, t1));
        if (!save_matrix(&A, file_out1)) {
            fprintf(stderr, "Falha ao salvar %s\n", file_out1);
        } else {
            printf("Resultado 1 salvo em %s\n", file_out1);
        }
    }

    // ---- matrix_matrix_mult ----
    printf("\nExecutando matrix_matrix_mult...\n");
    // Zera C no host antes de produzir o resultado
    memset(C.h_rows, 0, bytesC);
    gettimeofday(&t0, NULL);
    int ok2 = matrix_matrix_mult(&A, &B, &C);
    gettimeofday(&t1, NULL);
    if (!ok2) {
        fprintf(stderr, "Erro em matrix_matrix_mult.\n");
    } else {
        print_first256(&C, "Matriz C (resultado final)");
        printf("Tempo matrix_matrix_mult: %f ms\n", timedifference_msec(t0, t1));
        if (!save_matrix(&C, file_out2)) {
            fprintf(stderr, "Falha ao salvar %s\n", file_out2);
        } else {
            printf("Resultado 2 salvo em %s\n", file_out2);
        }
    }

    // Cleanup
    if (A.d_rows) cudaFree(A.d_rows);
    if (B.d_rows) cudaFree(B.d_rows);
    if (C.d_rows) cudaFree(C.d_rows);

    free(A.h_rows); free(B.h_rows); free(C.h_rows);

    gettimeofday(&overall_t1, NULL);
    printf("\nTempo total (overall): %f ms\n", timedifference_msec(overall_t0, overall_t1));
    return 0;
}
