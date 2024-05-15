#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/getopt_core.h>

#define BLOCK_SIZE 262144 // 256 kB block size

typedef struct FatEntry {
    char filename[12]; // 12-character filename
    unsigned short file_size; // File size in bytes
    unsigned short starting_block; // Starting block number of the file
    unsigned char is_empty; // Empty flag (1 for empty, 0 for occupied)
} FatEntry;

typedef struct FatTable {
    FatEntry entries[256]; // FAT table with 256 entries
} FatTable;

typedef struct TarHeader {
    char magic_number[6]; // Magic number to identify the TAR file
    char version_number[2]; // Version number
    char user_name[100]; // User name
    char group_name[100]; // Group name
    char modification_time[12]; // Modification time in octal
    char checksum[8]; // Checksum for header integrity
    char file_size[12]; // File size in bytes (in octal)
    char block_size[12]; // Block size in bytes (in octal)
    char linked_tar_file[100]; // Linked TAR file name
    char prefix[155]; // Prefix for file names
} TarHeader;

void initializeFatTable(FatTable *fatTable) {
    for (int i = 0; i < 256; i++) {
        fatTable->entries[i].is_empty = 1;
        fatTable->entries[i].starting_block = 0;
        fatTable->entries[i].file_size = 0;
        strcpy(fatTable->entries[i].filename, "\0");
    }
}

int findEmptyFatEntry(FatTable *fatTable) {
    for (int i = 0; i < 256; i++) {
        if (fatTable->entries[i].is_empty) {
            return i;
        }
    }
    return -1; // No empty entries found
}

void printFatTable(FatTable *fatTable) {
    printf("FAT Table:\n");
    printf("  Filename  | File Size | Starting Block | Is Empty\n");
    printf("------------|------------|-----------------|----------\n");

    for (int i = 0; i < 10; i++) {
        FatEntry entry = fatTable->entries[i];
        printf("  %-12s | %10d | %15d | %d\n", entry.filename, entry.file_size, entry.starting_block, entry.is_empty);
    }
}

void saveFatTableToFile(FatTable *fatTable, FILE *tarFile) {
    printFatTable(fatTable);
    fwrite(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void loadFatTableFromFile(FatTable *fatTable, FILE *tarFile) {
    fread(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void createEmptyTar(char *tarFilename) {
    FILE *tarFile = fopen(tarFilename, "wb");
    if (!tarFile) {
        printf("Error creating TAR file: %s\n", tarFilename);
        return;
    }

    // Initialize an empty FAT table
    FatTable fatTable;
    initializeFatTable(&fatTable);

    // Write the FAT table to the TAR file
    saveFatTableToFile(&fatTable, tarFile);

    // Write an empty TAR header (optional)
    TarHeader tarHeader;
    memset(&tarHeader, 0, sizeof(TarHeader)); // Set all header fields to 0
    strcpy(tarHeader.magic_number, "ustar"); // Set magic number
    fwrite(&tarHeader, sizeof(TarHeader), 1, tarFile);

    fclose(tarFile);
    printf("Empty TAR file created: %s\n", tarFilename);
}


void writeFileToTar(char *filename, FILE *tarFile, FatTable *fatTable) {
    FILE *sourceFile = fopen(filename, "rb");
    if (!sourceFile) {
        printf("Error opening file: %s\n", filename);
        return;
    }

    int file_size = 0;
    char buffer[BLOCK_SIZE];

    // Read file blocks and write them to the TAR file
    int currentBlock = findEmptyFatEntry(fatTable);
    while (fread(buffer, 1, BLOCK_SIZE, sourceFile) > 0) {
        fwrite(buffer, 1, BLOCK_SIZE, tarFile);
        file_size += BLOCK_SIZE;

        // Update FAT table for the next block
        if (currentBlock != -1) {
            fatTable->entries[currentBlock].is_empty = 0;
            fatTable->entries[currentBlock].file_size = file_size;
            fatTable->entries[currentBlock].starting_block = currentBlock;
            strcpy(fatTable->entries[currentBlock].filename, filename);

            currentBlock = findEmptyFatEntry(fatTable);
        } else {
            printf("Error: FAT table full\n");
            break;
        }
    }

    // Update FAT table for the last block or empty blocks
    if (currentBlock != -1) {
        fatTable->entries[currentBlock].is_empty = 0;
        fatTable->entries[currentBlock].file_size = file_size;
        fatTable->entries[currentBlock].starting_block = currentBlock;
    }

    fclose(sourceFile);
}

void readTarFile(char *tarFilename) {
    FILE *tarFile = fopen(tarFilename, "rb");
    if (!tarFile) {
        printf("Error opening TAR file: %s\n", tarFilename);
        return;
    }

    // Read and process the TAR header
    TarHeader tarHeader;
    fread(&tarHeader, sizeof(TarHeader), 1, tarFile);

    // Basic validation of TAR header (optional)
    if (strncmp(tarHeader.magic_number, "ustar", 5) != 0) {
        printf("Error: Invalid TAR file magic number\n");
        fclose(tarFile);
        return;
    }

    // Read and store the FAT table
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    // Loop through each file entry in the TAR based on FAT table
    for (int i = 0; i < 256; i++) {
        if (fatTable.entries[i].is_empty) {
            continue; // Skip empty FAT entries
        }

        // Extract file data based on FAT information
        char filename[13]; // Account for null terminator
        strncpy(filename, fatTable.entries[i].filename, 12);
        filename[12] = '\0'; // Ensure null termination
        int file_size = fatTable.entries[i].file_size;
        int starting_block = fatTable.entries[i].starting_block;

        // Read file blocks and update FAT entry with block information
        int currentBlock = starting_block;
        char buffer[BLOCK_SIZE];
        while (fread(buffer, 1, BLOCK_SIZE, tarFile) > 0) {
            // Update FAT entry with block information
            fatTable.entries[currentBlock].is_empty = 0;
            fatTable.entries[currentBlock].file_size = file_size;
            fatTable.entries[currentBlock].starting_block = currentBlock;

            // Move to the next block
            currentBlock++;

            // Check if file size is reached
            if (file_size <= BLOCK_SIZE) {
                break; // File size is less than or equal to one block
            }

            file_size -= BLOCK_SIZE;
        }

        // Print information about the found file
        printf("Found file: %s, Size: %d bytes, Starting block: %d\n", filename, fatTable.entries[i].file_size, fatTable.entries[i].starting_block);
    }

    fclose(tarFile);
}

int main(int argc, char *argv[]) {
    int opt;
    int create = 0, extract = 0, list = 0, delete = 0, update = 0, verbose = 0, append = 0;
    char *tarFilename = NULL;
    char *filename = NULL;

    // Procesar los argumentos de la línea de comandos
    while ((opt = getopt(argc, argv, "cxtduvrf:")) != -1) {
        switch (opt) {
            case 'c':
                create = 1;
                break;
            case 'x':
                extract = 1;
                break;
            case 't':
                list = 1;
                break;
            case '-': // Manejar los flags largos (--delete)
                if (strcmp(optarg, "delete") == 0) {
                    delete = 1;
                }
                break;
            case 'u':
                update = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'r':
                append = 1;
                break;
            case 'f':
                tarFilename = optarg;
                break;
            default:
                fprintf(stderr, "Uso: %s [-cxtdurv] [-f archivo_tar] [archivo]\n", argv[0]);
                return 1;
        }
    }

    // Verificar la validez de las combinaciones de argumentos
    if ((create + extract + list + delete + update + append) != 1) {
        fprintf(stderr, "Debe especificar exactamente una operación (-c, -x, -t, --delete, -u, -r).\n");
        return 1;
    }

    // Ejecutar la operación especificada
    if (create) {
        createEmptyTar(tarFilename);

        // Si hay archivos adicionales para agregar al archivo TAR recién creado
        if (optind < argc) {
            // Abrir el archivo TAR en modo de actualización ("rb+")
            FILE *tarFile = fopen(tarFilename, "rb+");
            if (!tarFile) {
                printf("Error abriendo archivo TAR: %s\n", tarFilename);
                return 1;
            }

            // Leer la FAT table del archivo TAR
            FatTable fatTable;
            loadFatTableFromFile(&fatTable, tarFile);

            // Iterar sobre los archivos adicionales para agregarlos al archivo TAR
            for (int i = optind; i < argc; i++) {
                // Agregar el archivo al archivo TAR
                writeFileToTar(argv[i], tarFile, &fatTable);
            }

            // Guardar la FAT table actualizada en el archivo TAR
            fseek(tarFile, 0, SEEK_SET);
            saveFatTableToFile(&fatTable, tarFile);

            fclose(tarFile);
            printf("Archivos agregados a %s\n", tarFilename);
        } else {
            printf("Archivo TAR creado: %s\n", tarFilename);
        }
    } else if (extract) {
        readTarFile(tarFilename);
    } else if (list) {
        // Implementar la función para listar el contenido del archivo TAR
        printf("Listar contenido del archivo TAR: %s\n", tarFilename);
    } else if (delete) {
        // Implementar la función para borrar desde un archivo TAR
        printf("Borrar desde el archivo TAR: %s\n", tarFilename);
    } else if (update) {
        // Implementar la función para actualizar el contenido del archivo TAR
        printf("Actualizar contenido del archivo TAR: %s\n", tarFilename);
    } else if (append) {
        // Implementar la función para agregar contenido a un archivo TAR
        printf("Agregar contenido al archivo TAR: %s\n", tarFilename);
    }

    return 0;
}
