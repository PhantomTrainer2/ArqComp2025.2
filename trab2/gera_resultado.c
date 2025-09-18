#include <stdio.h>
#include <stdlib.h>

int gera_resultado(int argc, char *argv[]) {
    // Valida o número de argumentos na linha de comando
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo.dat> <altura> <largura>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s result1.dat 8 16\n", argv[0]);
        return 1;
    }

    // Lê os argumentos
    const char *filename = argv[1];
    unsigned long height = strtoul(argv[2], NULL, 10);
    unsigned long width = strtoul(argv[3], NULL, 10);

    if (height == 0 || width == 0) {
        fprintf(stderr, "Erro: A altura e a largura devem ser maiores que zero.\n");
        return 1;
    }

    // Abre o arquivo binário para leitura
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        return 1;
    }

    // Aloca memória para armazenar os dados da matriz
    unsigned long total_elements = height * width;
    float *matrix_data = (float*) malloc(total_elements * sizeof(float));
    if (matrix_data == NULL) {
        fprintf(stderr, "Erro: Falha ao alocar memória.\n");
        fclose(file);
        return 1;
    }

    // Lê os dados do arquivo para a memória
    size_t elements_read = fread(matrix_data, sizeof(float), total_elements, file);

    if (elements_read != total_elements) {
        fprintf(stderr, "Aviso: O arquivo continha %zu elementos, mas o esperado era %lu.\n", elements_read, total_elements);
        fprintf(stderr, "A matriz pode ser exibida incorretamente ou de forma incompleta.\n");
    }

    // Fecha o arquivo
    fclose(file);

    // Exibe a matriz na tela
    printf("Exibindo matriz %lux%lu do arquivo '%s':\n\n", height, width, filename);
    for (unsigned long i = 0; i < height; i++) {
        for (unsigned long j = 0; j < width; j++) {
            // Imprime cada elemento formatado com espaço fixo
            printf("%10.2f ", matrix_data[i * width + j]);
        }
        // Pula para a próxima linha ao final de cada linha da matriz
        printf("\n");
    }

    // Libera a memória alocada
    free(matrix_data);

    return 0;
}
