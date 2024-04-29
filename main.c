#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bits/getopt_core.h>

void create_tar(char* tar_filename, char* files[], int num_files);
void extract_tar(char* tar_filename);
void list_tar(char* tar_filename);
void delete_from_tar(char* tar_filename, char* file_to_delete);
void update_tar(char* tar_filename, char* file_to_update);
void pack_tar(char* tar_filename);

int main(int argc, char *argv[]) {
    char* tar_filename = NULL;
    char* files[argc];
    int num_files = 0;

    
    int opt;
    while ((opt = getopt(argc, argv, "cxtruf:p")) != -1) {
        switch (opt) {
            case 'c':
                tar_filename = argv[optind];
                for (int i = optind + 1; i < argc; i++) {
                    files[num_files++] = argv[i];
                }
                create_tar(tar_filename, files, num_files);
                break;
            case 'x':
                extract_tar(argv[optind]);
                break;
            case 't':
                list_tar(argv[optind]);
                break;
            case 'r':
                // Implementar función para el flag -r
                break;
            case 'u':
                // Implementar función para el flag -u
                break;
            case 'f':
                // Implementar función para el flag -f
                break;
            case 'p':
                pack_tar(argv[optind]);
                break;
            default:
                fprintf(stderr, "Uso: %s [-cxtur] [-f archivo_tar] [-p] [archivo...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    return 0;
}

void create_tar(char* tar_filename, char* files[], int num_files) {
    // Implementar la creación de un archivo TAR
}

void extract_tar(char* tar_filename) {
    // Implementar la extracción de un archivo TAR
}

void list_tar(char* tar_filename) {
    // Implementar la lista de un archivo TAR
}

void delete_from_tar(char* tar_filename, char* file_to_delete) {
    // Implementar la eliminación de un archivo de un archivo TAR
}

void update_tar(char* tar_filename, char* file_to_update) {
    // Implementar la actualización de un archivo en un archivo TAR
}

void pack_tar(char* tar_filename) {
    // Implementar la desfragmentación de un archivo TAR
}
