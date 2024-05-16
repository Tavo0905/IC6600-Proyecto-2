#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/getopt_core.h>

#define BLOCK_SIZE 262144 // 256 kB block size

typedef struct FatEntry {
    char filename[12]; // 12-character filename
    unsigned int starting_block; // Starting block number of the file
    unsigned int num_blocks; // Number of blocks required for the file
    unsigned int file_size;  // File size in bytes (added)
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
        fatTable->entries[i].num_blocks = 0;
        strcpy(fatTable->entries[i].filename, "\0");
        fatTable->entries[i].file_size = 0; // Initialize file size to 0 (added)
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

void saveFatTableToFile(FatTable *fatTable, FILE *tarFile) {
    fwrite(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void loadFatTableFromFile(FatTable *fatTable, FILE *tarFile) {
    fread(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void printFatTable(FatTable *fatTable) {
    printf("-------------------------------------------------------------------------------------\n");
    printf("| %-20s | %-12s | %-12s | %-15s | %-10s |\n", "Filename", "First Block", "Block Size", "File Size", "Is Empty");
    printf("|----------------------|--------------|--------------|-----------------|------------|\n");

    for (int i = 0; i < 20; i++) {
        FatEntry entry = fatTable->entries[i];
        printf("| %-20s | %12d | %12d | %15d | %10d |\n", entry.filename, entry.starting_block, entry.num_blocks, entry.file_size, entry.is_empty);
    }
    printf("-------------------------------------------------------------------------------------\n");
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

    // Get file size
    fseek(sourceFile, 0, SEEK_END);
    int file_size = ftell(sourceFile);
    fseek(sourceFile, 0, SEEK_SET);

    // Calculate the number of blocks required
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Find the last occupied block of the previous file
    int lastOccupiedBlock = -1;
    for (int i = 0; i < 256; i++) {
        if (!fatTable->entries[i].is_empty && i < fatTable->entries[i].starting_block + fatTable->entries[i].num_blocks) {
            lastOccupiedBlock = i;
        }
    }

    // Calculate the starting block for the current file
    int starting_block;
    if (lastOccupiedBlock == -1) {
        starting_block = 0; // No previous files, start from block 0
    } else {
        starting_block = fatTable->entries[lastOccupiedBlock].starting_block + fatTable->entries[lastOccupiedBlock].num_blocks;
    }

    // Update FAT table for the current file (including file size)
    int currentBlock = findEmptyFatEntry(fatTable);
    if (currentBlock == -1) {
        printf("Error: FAT table full\n");
        fclose(sourceFile);
        return;
    }

    fatTable->entries[currentBlock].is_empty = 0;
    fatTable->entries[currentBlock].starting_block = starting_block;
    fatTable->entries[currentBlock].num_blocks = num_blocks;
    strcpy(fatTable->entries[currentBlock].filename, filename);
    fatTable->entries[currentBlock].file_size = file_size;
    int offset = 0;

    // Read file blocks and write them to the TAR file
    char buffer[BLOCK_SIZE];
    while (fread(buffer, 1, BLOCK_SIZE, sourceFile) > 0) {
        fwrite(buffer, 1, BLOCK_SIZE, tarFile);
        offset += BLOCK_SIZE;

        // Check if the current block is the last block
        if (offset == file_size) {
            break;
        }

        // Update FAT table for the next block
        currentBlock = findEmptyFatEntry(fatTable);
        if (currentBlock == -1) {
            printf("Error: FAT table full\n");
            break;
        }

        // fatTable->entries[currentBlock].is_empty = 0;
        // fatTable->entries[currentBlock].starting_block = currentBlock;
        // fatTable->entries[currentBlock].num_blocks = num_blocks;
    }


    fclose(sourceFile);
    printf("Archivo agregado al TAR: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable->entries[currentBlock].starting_block, num_blocks);
}

void readTarFile(char *tarFilename) {
    FILE *tarFile = fopen(tarFilename, "rb");
    if (!tarFile) {
        printf("Error opening TAR file: %s\n", tarFilename);
        return;
    }

    // Calculate the offset after the FatTable and TarHeader
    int offset = sizeof(FatTable) + sizeof(TarHeader);

    // Read and store the FAT table
    FatTable fatTable;
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    fread(&fatTable, sizeof(FatTable), 1, tarFile);

    // Loop through each file entry in the TAR based on FAT table
    for (int i = 0; i < 256; i++) {
        if (fatTable.entries[i].is_empty) {
            continue; // Skip empty FAT entries
        }

        // Extract file data based on FAT information
        char filename[13]; // Account for null terminator
        strncpy(filename, fatTable.entries[i].filename, 12);
        filename[12] = '\0'; // Ensure null termination
        int file_size = fatTable.entries[i].file_size;  // Use file size from FAT entry
        int starting_block = fatTable.entries[i].starting_block;

        // Open output file for writing
        FILE *outFile = fopen(filename, "wb");
        if (!outFile) {
            printf("Error creating output file: %s\n", filename);
            continue;
        }

        // Extract file data block by block
        int bytes_read = 0;
        int currentBlock = starting_block;
        char buffer[BLOCK_SIZE];
        while (bytes_read < file_size) {
            // Calculate remaining bytes to read for the last block
            int bytes_to_read = (file_size - bytes_read) < BLOCK_SIZE ? (file_size - bytes_read) : BLOCK_SIZE;

            // Seek to the starting position of the current block, considering offset
            fseek(tarFile, offset + starting_block * BLOCK_SIZE + bytes_read, SEEK_SET);

            // Read data from the TAR file
            int bytes_actually_read = fread(buffer, 1, bytes_to_read, tarFile);

            // Check for read errors or unexpected end of file
            if (bytes_actually_read < bytes_to_read && feof(tarFile)) {
                printf("Error reading file: Unexpected end of file in block %d\n", currentBlock);
                break;
            } else if (bytes_actually_read != bytes_to_read) {
                printf("Error reading file: Failed to read %d bytes from block %d\n", bytes_to_read, currentBlock);
                break;
            }

            // Write data to the output file
            fwrite(buffer, 1, bytes_actually_read, outFile);

            // Update bytes read and current block
            bytes_read += bytes_actually_read;
            currentBlock++;
        }

        fclose(outFile);
        printf("Archivo extraído: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable.entries[i].starting_block, fatTable.entries[i].num_blocks);
    }

    fclose(tarFile);
}

void listTar(char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb");
    if (!tarFile) {
        printf("Error opening TAR file: %s\n", tar_filename);
        return;
    }

    // Read and store the FAT table
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    printFatTable(&fatTable);

    fclose(tarFile);
}

void deleteFileFromTar(char *filename, char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile) {
        printf("Error opening TAR file: %s\n", tar_filename);
        return;
    }

    // Read and store the FAT table
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    // Find the entry for the file in the FAT table
    int fileIndex = -1;
    for (int i = 0; i < 256; i++) {
        if (!fatTable.entries[i].is_empty && strcmp(fatTable.entries[i].filename, filename) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        printf("File not found in TAR: %s\n", filename);
        fclose(tarFile);
        return;
    }

    // Mark the entry as empty
    fatTable.entries[fileIndex].is_empty = 1;
    // fatTable.entries[fileIndex].starting_block = 0;
    // fatTable.entries[fileIndex].num_blocks = 0;
    fatTable.entries[fileIndex].file_size = 0;
    strcpy(fatTable.entries[fileIndex].filename, "");

    // Update the FAT table in the TAR file
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    saveFatTableToFile(&fatTable, tarFile);

    fclose(tarFile);
    printf("File deleted from TAR: %s\n", filename);
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
            case 'd':
                delete = 1;
                break;
            // case '-': // Manejar los flags largos (--delete)
            //     if (strcmp(optarg, "delete") == 0) {
            //         delete = 1;
            //     }
            //     break;
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
                fprintf(stderr, "Uso: %s [-cxtdurv] [-f archivo_tar] [archivo(s)]\n", argv[0]);
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

            printFatTable(&fatTable);

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
        listTar(tarFilename);
    } else if (delete) {
        deleteFileFromTar(argv[optind], tarFilename);
        // Implementar la función para borrar desde un archivo TAR
        // printf("Borrar desde el archivo TAR: %s\n", tarFilename);
    } else if (update) {
        // Implementar la función para actualizar el contenido del archivo TAR
        printf("Actualizar contenido del archivo TAR: %s\n", tarFilename);
    } else if (append) {
        // Implementar la función para agregar contenido a un archivo TAR
        printf("Agregar contenido al archivo TAR: %s\n", tarFilename);
    }

    return 0;
}
