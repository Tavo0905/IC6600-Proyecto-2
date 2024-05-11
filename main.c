#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bits/getopt_core.h>

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libtar.h>

void create_tar(char* tar_filename, char* files[], int num_files);
void extract_tar(char* tar_filename);
void list_tar(char* tar_filename);
void delete_from_tar(char* tar_filename, char* file_to_delete);
void update_tar(char* tar_filename, char* file_to_update);
void pack_tar(char* tar_filename);

int main(int argc, char *argv[]) {
    // printf("Comando STAR: %d\n", argc);
    
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
                    // printf("FILE %d: %s\n", num_files-1, files[num_files-1]);
                    // printf("FILE %d (%d): %s | %s\n", num_files-1, i, argv[i], files[num_files-1]);
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
    // Create a tar file
    int tar_fd = open(tar_filename, O_WRONLY | O_CREAT, 0644);
    if (tar_fd < 0) {
        fprintf(stderr, "Error creating tar file '%s': %s\n", tar_filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Initialize tar
    TAR *tar = NULL;
    if (tar_fdopen(&tar, tar_fd, NULL, NULL, O_WRONLY, 0644, TAR_GNU) != 0) {
        fprintf(stderr, "Error opening tar archive: %s\n", strerror(errno));
        close(tar_fd);
        exit(EXIT_FAILURE);
    }

    // Add each file to the tar
    for (int i = 0; i < num_files; i++) {
        if (tar_append_file(tar, files[i], NULL) < 0) {
            fprintf(stderr, "Error adding file '%s' to tar: %s\n", files[i], strerror(errno));
            tar_close(tar);
            close(tar_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Close the tar file
    tar_close(tar);
    close(tar_fd);

    printf("Tar file '%s' created successfully containing the specified files\n", tar_filename);
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
