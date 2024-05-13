#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bits/getopt_core.h>

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FAT_ENTRY_SIZE 256 // Tamaño de cada entrada FAT (puede ajustarse según las necesidades)
#define MAX_BLOCKS 1000 // Número máximo de bloques en el archivo TAR
#define TAR_BLOCKSIZE 262144

typedef struct {
    char filename[100]; // Nombre del archivo
    off_t block_offset; // Desplazamiento del bloque de inicio dentro del archivo TAR
    off_t block_count;  // Número de bloques ocupados por el archivo
} FatEntry;

void create_tar(char* tar_filename);
void extract_tar(char* tar_filename);
void list_tar(char* tar_filename);
void delete_from_tar(char* tar_filename, char* file_to_delete);
void update_tar(char* tar_filename, char* file_to_update);
void append_to_tar(char* tar_filename, char* files[], int num_files);
void pack_tar(char* tar_filename);
void write_header(int tar_fd, const char *filename, off_t size);
void write_file(int tar_fd, const char *filename, FatEntry *fat_entry);
void write_fat_entries(int tar_fd, FatEntry *fat_entries, int num_entries, off_t total_blocks);

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
                create_tar(tar_filename);
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
                tar_filename = optarg;
                for (int i = optind; i < argc; i++) {
                    files[num_files++] = argv[i];
                }
                append_to_tar(tar_filename, files, num_files);
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

void create_tar(char* tar_filename) {
    // Crea un archivo tar vacío
    int tar_fd = open(tar_filename, O_WRONLY | O_CREAT, 0644);
    if (tar_fd < 0) {
        fprintf(stderr, "Error creando el archivo TAR '%s': %s\n", tar_filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(tar_fd);
    printf("Archivo TAR '%s' creado exitosamente\n", tar_filename);
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

void append_to_tar(char* tar_filename, char* files[], int num_files) {
    // Abre el archivo tar para agregar archivos
    int tar_fd = open(tar_filename, O_WRONLY | O_APPEND);
    if (tar_fd < 0) {
        fprintf(stderr, "Error abriendo el archivo TAR '%s': %s\n", tar_filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Crear la estructura FAT para los nuevos archivos
    FatEntry fat_entries[num_files];
    off_t current_offset = lseek(tar_fd, 0, SEEK_END);
    off_t total_blocks = 0;

    // Escribir cada archivo en el archivo TAR y registrar su posición en la estructura FAT
    for (int i = 0; i < num_files; i++) {
        write_file(tar_fd, files[i], &fat_entries[i]);
        total_blocks += fat_entries[i].block_count;
    }

    // Escribir la estructura FAT al final del archivo TAR
    write_fat_entries(tar_fd, fat_entries, num_files, total_blocks);

    // Cerrar el archivo TAR
    close(tar_fd);

    printf("Archivos agregados al archivo TAR '%s' exitosamente\n", tar_filename);
}

void pack_tar(char* tar_filename) {
    // Implementar la desfragmentación de un archivo TAR
}

void write_header(int tar_fd, const char *filename, off_t size) {
    char header[TAR_BLOCKSIZE];
    memset(header, 0, TAR_BLOCKSIZE);
    snprintf(header, TAR_BLOCKSIZE, "%s", filename);
    snprintf(header + 124, 12, "%011o", (unsigned int)size);
    write(tar_fd, header, TAR_BLOCKSIZE);
}

void write_file(int tar_fd, const char *filename, FatEntry *fat_entry) {
    // Escribir un archivo en el archivo TAR y actualizar la entrada FAT
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        fprintf(stderr, "Error abriendo el archivo '%s': %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct stat st;
    stat(filename, &st);
    off_t file_size = st.st_size;

    // Escribir el encabezado del archivo
    write_header(tar_fd, filename, file_size);

    // Escribir los datos del archivo
    char buffer[TAR_BLOCKSIZE];
    ssize_t bytes_read;
    off_t total_bytes_written = 0;
    off_t block_count = 0;
    while ((bytes_read = read(file_fd, buffer, TAR_BLOCKSIZE)) > 0) {
        write(tar_fd, buffer, bytes_read);
        total_bytes_written += bytes_read;
        if (total_bytes_written % TAR_BLOCKSIZE == 0) {
            block_count++;
        }
    }

    // Escribir los bloques de padding si es necesario
    int padding_size = TAR_BLOCKSIZE - (file_size % TAR_BLOCKSIZE);
    if (padding_size != TAR_BLOCKSIZE) {
        char padding[TAR_BLOCKSIZE];
        memset(padding, 0, TAR_BLOCKSIZE);
        write(tar_fd, padding, padding_size);
        block_count++;
    }

    // Actualizar la entrada FAT
    strncpy(fat_entry->filename, filename, sizeof(fat_entry->filename) - 1);
    off_t current_offset = lseek(tar_fd, 0, SEEK_CUR);
    fat_entry->block_offset = current_offset - total_bytes_written - padding_size;
    fat_entry->block_count = block_count;

    close(file_fd);
}

void write_fat_entries(int tar_fd, FatEntry *fat_entries, int num_entries, off_t total_blocks) {
    // Escribir las entradas FAT al inicio del archivo TAR
    off_t current_block = 0;
    for (int i = 0; i < num_entries; i++) {
        while (current_block < fat_entries[i].block_offset / TAR_BLOCKSIZE) {
            // Escribir un bloque libre
            char entry_info[256];
            memset(entry_info, 0, 256);
            snprintf(entry_info, 256, "-1 0\n");
            write(tar_fd, entry_info, strlen(entry_info));
            current_block++;
        }

        char entry_info[256];
        memset(entry_info, 0, 256);
        snprintf(entry_info, 256, "%lld %lld\n", fat_entries[i].block_offset / TAR_BLOCKSIZE, fat_entries[i].block_count);
        write(tar_fd, entry_info, strlen(entry_info));
        current_block += fat_entries[i].block_count;
    }

    while (current_block < total_blocks) {
        // Escribir un bloque libre
        char entry_info[256];
        memset(entry_info, 0, 256);
        snprintf(entry_info, 256, "-1 0\n");
        write(tar_fd, entry_info, strlen(entry_info));
        current_block++;
    }
}
