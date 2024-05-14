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
#define TAR_BLOCKSIZE (256 * 1024)

typedef struct {
    char filename[100]; // Nombre del archivo
    off_t block_offset; // Desplazamiento del bloque de inicio dentro del archivo TAR
    off_t block_count;  // Número de bloques ocupados por el archivo
} FatEntry;

void create_tar(char* tar_filename);
void extract_tar(char* tar_filename);
void list_tar(char* tar_filename);
void delete_from_tar(char* tar_filename, char* file_to_delete);
void update_tar(char* tar_filename, char** files_to_update);
void append_to_tar(char* tar_filename, char* files[], int num_files);
void pack_tar(char* tar_filename);
void write_header(int tar_fd, const char *filename, off_t size);
void write_file(int tar_fd, const char *filename, FatEntry *fat_entry);
void write_fat_entries(int tar_fd, FatEntry *fat_entries, int num_entries);
int calculate_file_blocks(const char* filename);

int main(int argc, char *argv[]) {
    char* tar_filename = NULL;
    char* files[argc];
    int num_files = 0;
    
    // Variables para controlar qué opciones se han encontrado
    int c_flag = 0;
    int x_flag = 0;
    int u_flag = 0;
    int f_flag = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "cxtruf:p")) != -1) {
        switch (opt) {
            case 'c':
                c_flag = 1;
                break;
            case 'x':
                x_flag = 1;
                break;
            case 't':
                // Implementar función para el flag -t
                break;
            case 'r':
                // Implementar función para el flag -r
                break;
            case 'u':
                u_flag = 1;
                break;
            case 'f':
                f_flag = 1;
                break;
            case 'p':
                // Implementar función para el flag -p
                break;
            default:
                fprintf(stderr, "Uso: %s [-cxtur] [-f archivo_tar] [-p] [archivo...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
        tar_filename = optarg;
    }
    
    // Si se encontró el flag -f, procesar los archivos
    if (f_flag) {
        for (int i = optind; i < argc; i++) {
            files[num_files++] = argv[i];
        }
    }
    
    // Llamar a las funciones en el orden en que se encontraron los flags
    if (c_flag)
        create_tar(tar_filename);
    if (x_flag)
        extract_tar(tar_filename);
    if (u_flag)
        update_tar(tar_filename, files);
    if (f_flag)
        append_to_tar(tar_filename, files, num_files);

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

void update_tar(char* tar_filename, char** file_to_update) {
    // Implementar la actualización de un archivo en un archivo TAR
}

void append_to_tar(char* tar_filename, char* files[], int num_files) {
    int tar_fd = open(tar_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (tar_fd < 0) {
        fprintf(stderr, "Error opening TAR file '%s': %s\n", tar_filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Calculate total blocks needed for all files
    off_t total_blocks = 0;
    for (int i = 0; i < num_files; i++) {
        int file_blocks = calculate_file_blocks(files[i]);
        total_blocks += file_blocks;
    }

    // Check if total blocks exceed the maximum
    if (total_blocks > MAX_BLOCKS) {
        fprintf(stderr, "Error: Total blocks (%lld) exceed maximum allowed (%d)\n", total_blocks, MAX_BLOCKS);
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize FAT entries
    FatEntry fat_entries[num_files];
    off_t current_offset = 0; // Offset for the first file block
    for (int i = 0; i < num_files; i++) {
        fat_entries[i].block_count = calculate_file_blocks(files[i]);
        fat_entries[i].block_offset = current_offset;
        current_offset += fat_entries[i].block_count;
    }

    // Write FAT entries at the beginning of the TAR archive
    lseek(tar_fd, 0, SEEK_SET); // Move to the beginning of the file
    write_fat_entries(tar_fd, fat_entries, num_files);

    // Now append actual file data with headers
    for (int i = 0; i < num_files; i++) {
        write_file(tar_fd, files[i], &fat_entries[i]);
    }

    close(tar_fd);
    printf("Files added to TAR archive '%s' successfully\n", tar_filename);
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
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        fprintf(stderr, "Error opening file '%s': %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct stat st;
    stat(filename, &st);
    off_t file_size = st.st_size;

    write_header(tar_fd, filename, file_size);

    char buffer[TAR_BLOCKSIZE];
    ssize_t bytes_read;
    off_t total_bytes_written = 0;
    off_t block_count = 0;
    while ((bytes_read = read(file_fd, buffer, TAR_BLOCKSIZE)) > 0) {
        write(tar_fd, buffer, bytes_read);
        total_bytes_written += bytes_read;
        block_count++;
    }

    int padding_size = TAR_BLOCKSIZE - (file_size % TAR_BLOCKSIZE);
    if (padding_size != TAR_BLOCKSIZE) {
        char padding[TAR_BLOCKSIZE];
        memset(padding, 0, TAR_BLOCKSIZE);
        write(tar_fd, padding, padding_size);
        block_count++;
    }

    fat_entry->block_count = block_count; // Set actual block count
    close(file_fd);
}


void write_fat_entries(int tar_fd, FatEntry *fat_entries, int num_entries) {
    char entry_info[256];
    for (int i = 0; i < num_entries; i++) {
        snprintf(entry_info, 256, "%lld %lld\n", fat_entries[i].block_offset, fat_entries[i].block_count);
        write(tar_fd, entry_info, strlen(entry_info));
    }
}

int calculate_file_blocks(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        fprintf(stderr, "Error getting file size for '%s': %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    off_t file_size = st.st_size;
    return (file_size + TAR_BLOCKSIZE - 1) / TAR_BLOCKSIZE; // Round up for padding
}