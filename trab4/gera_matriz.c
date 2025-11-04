#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <altura> <largura> <valor_inicial> <arquivo_saida>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[4];
    unsigned long height = strtoul(argv[1], NULL, 10);
    unsigned long width = strtoul(argv[2], NULL, 10);
    float value = atof(argv[3]);
    unsigned long count = height * width;

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Não foi possível criar o arquivo");
        return 1;
    }

    for (unsigned long i = 0; i < count; i++) {
        fwrite(&value, sizeof(float), 1, file);
    }

    fclose(file);
    printf("Arquivo '%s' criado com uma matriz %lux%lu de valor %.2f.\n", filename, height, width, value);

    return 0;
}
