#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <omp.h>

int sudoku[9][9];

void* verifyRows(void* arg) {
    for (int i = 0; i < 9; i++) {
        int row[10] = {0};
        for (int j = 0; j < 9; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || row[num] != 0) {
                return (void*)0;
            }
            row[num] = 1;
        }
    }
    return (void*)1;
}

void* verifyColumns(void* arg) {
    long tid = (long)pthread_self();
    printf("El thread que ejecuta el método para verificar las columnas es: %ld\n", tid);

    #pragma omp parallel for
    for (int i = 0; i < 9; i++) {
        int col[10] = {0};
        for (int j = 0; j < 9; j++) {
            int num = sudoku[j][i];
            if (num < 1 || num > 9 || col[num] != 0) {
                #pragma omp critical
                {
                    printf("Columna %d no válida\n", i + 1);
                }
                return (void*)0;
            }
            col[num] = 1;
        }
    }
    printf("Todas las columnas son válidas.\n");
    return (void*)1;
}

void* verifySubgrid(void* arg) {
    int section = (intptr_t)arg;
    int startRow = (section / 3) * 3;
    int startCol = (section % 3) * 3;
    int check[10] = {0};
    for (int i = startRow; i < startRow + 3; i++) {
        for (int j = startCol; j < startCol + 3; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || check[num] != 0) {
                printf("Subcuadrícula %d no válida\n", section + 1);
                return (void*)0;
            }
            check[num] = 1;
        }
    }
    printf("Subcuadrícula %d es válida.\n", section + 1);
    return (void*)1;
}

void executePsCommand() {
    char command[256];
    sprintf(command, "ps -p %d -lLf", getpid());
    system(command);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <archivo_solucion_sudoku>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir el archivo");
        return EXIT_FAILURE;
    }

    char* file_content = mmap(NULL, 81, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED) {
        perror("Error al mapear el archivo");
        close(fd);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            sudoku[i][j] = file_content[i * 9 + j] - '0';
        }
    }

    munmap(file_content, 81);
    close(fd);

    omp_set_nested(1);
    omp_set_num_threads(4);

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            if (verifyColumns(NULL) == (void*)0) {
                printf("Solución de Sudoku inválida: falló la verificación de columnas.\n");
                return EXIT_FAILURE;
            }
        }

        #pragma omp section
        {
            for (int i = 0; i < 9; i++) {
                if (verifySubgrid((void*)(intptr_t)i) == (void*)0) {
                    printf("Solución de Sudoku inválida: falló la verificación de subcuadrículas.\n");
                    return EXIT_FAILURE;
                }
            }
        }
    }

    if (verifyRows(NULL) == (void*)0) {
        printf("Solución de Sudoku inválida: falló la verificación de filas.\n");
        return EXIT_FAILURE;
    }

    printf("Sudoku resuelto!\n");

    printf("Antes de terminar, el estado de este proceso y sus threads es:\n");
    executePsCommand();

    return EXIT_SUCCESS;
}
