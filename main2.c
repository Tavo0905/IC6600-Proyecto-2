#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/getopt_core.h>

#define BLOCK_SIZE 262144 // 256 kB block size
int verbose = 0;

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
        printf("ERROR: no se pudo crear el archivo %s\n", tarFilename);
        exit(-1);
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
}

void writeFileToTar(char *filename, FILE *tarFile, FatTable *fatTable) {
    FILE *sourceFile = fopen(filename, "rb");
    if (!sourceFile) {
        printf("ERROR: No se encontro el archivo %s\n", filename);
        return;
    }

    if (verbose == 2) {
        printf("Obteniendo el tamano de %s...\n", filename);
    }

    // Get file size
    fseek(sourceFile, 0, SEEK_END);
    int file_size = ftell(sourceFile);
    fseek(sourceFile, 0, SEEK_SET);

    if (verbose == 2) {
        printf("Calculando la cantidad de bloques requeridos para %s...\n", filename);
    }

    // Calculate the number of blocks required
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (verbose == 2) {
        printf("Ajustando posiciones dentro del FAT...\n");
    }

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
        printf("ERROR: Se ha superado la cantidad maxima de datos.\n");
        fclose(sourceFile);
        return;
    }

    if (verbose == 2) {
        printf("Actualizando estructura FAT...\n");
    }

    fatTable->entries[currentBlock].is_empty = 0;
    fatTable->entries[currentBlock].starting_block = starting_block;
    fatTable->entries[currentBlock].num_blocks = num_blocks;
    strcpy(fatTable->entries[currentBlock].filename, filename);
    fatTable->entries[currentBlock].file_size = file_size;
    int offset = 0;

    if (verbose == 2) {
        printf("Leyendo el contenido del archivo...\n");
    }
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
            printf("ERROR: Se ha superado la cantidad maxima de datos.\n");
            break;
        }
    }


    fclose(sourceFile);
    if (verbose == 2) {
        printf("Archivo agregado al TAR: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable->entries[currentBlock].starting_block, num_blocks);
    }
}

void readTarFile(char *tarFilename) {
    FILE *tarFile = fopen(tarFilename, "rb");
    if (!tarFile) {
        printf("ERROR: No se encontro el archivo TAR: %s\n", tarFilename);
        return;
    }

    if (verbose == 2) {
        printf("Archivo %s cargado conexito.\n\n", tarFilename);
    }

    // Calculate the offset after the FatTable and TarHeader
    int offset = sizeof(FatTable) + sizeof(TarHeader);

    // Read and store the FAT table
    FatTable fatTable;
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    fread(&fatTable, sizeof(FatTable), 1, tarFile);

    if (verbose == 2) {
        printf("Extrayendo estructura FAT...\n\n");
    }

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
            printf("ERROR: no se pudo extraer el archivo %s\n", filename);
            continue;
        }

        if (verbose == 2) {
            printf("Extrayendo el contenido del archivo %s.\n", filename);
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
                printf("ERROR: EOF encontrado dentro del bloque %d.\n", currentBlock);
                break;
            } else if (bytes_actually_read != bytes_to_read) {
                printf("ERROR: No se pudo leer los bytes %d del bloque %d.\n", bytes_to_read, currentBlock);
                break;
            }

            // Write data to the output file
            fwrite(buffer, 1, bytes_actually_read, outFile);

            // Update bytes read and current block
            bytes_read += bytes_actually_read;
            currentBlock++;
        }

        fclose(outFile);
        if (verbose == 1) {
            printf("Archivo extraído: %s\n", filename);
        } else if (verbose == 2) {
            printf("Archivo extraído: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable.entries[i].starting_block, fatTable.entries[i].num_blocks);
        }
    }

    fclose(tarFile);

    if (verbose == 2) {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
}

void listTar(char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb");
    if (!tarFile) {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0) {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2) {
        printf("Extrayendo estructura FAT...\n");
    }
    // Read and store the FAT table
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    if (verbose == 2) {
        printf("Imprimiendo tabla de archivos...\n");
    }
    printFatTable(&fatTable);
    if (verbose == 2) {
        printf("Tabla de archivos desplegada con exito.\n");
    }

    fclose(tarFile);
    if (verbose == 2) {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
}

void deleteFileFromTar(char *filename, char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile) {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0) {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2) {
        printf("Extrayendo estructura FAT...\n");
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
        printf("ERROR: Archivo no encontrado dentro del TAR: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose > 0) {
        printf("Eliminando archivo %s...\n", filename);
    }
    // Mark the entry as empty
    fatTable.entries[fileIndex].is_empty = 1;
    fatTable.entries[fileIndex].file_size = 0;
    strcpy(fatTable.entries[fileIndex].filename, "");

    // Update the FAT table in the TAR file
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    saveFatTableToFile(&fatTable, tarFile);

    if (verbose == 2) {
        printf("Actualizando la estructura FAT...\n");
    }

    fclose(tarFile);
    if (verbose == 2) {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
    printf("Archivo eliminado del TAR: %s\n", filename);
}

void updateFileFromTar(char *filename, char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile) {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0) {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2) {
        printf("Extrayendo estructura FAT...\n");
    }
    // Read and store the FAT table
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    if (verbose == 2) {
        printf("Buscando archivo: %s...\n", filename);
    }
    // Find the entry for the file in the FAT table
    int fileIndex = -1;
    for (int i = 0; i < 256; i++) {
        if (!fatTable.entries[i].is_empty && strcmp(fatTable.entries[i].filename, filename) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1) {
        printf("ERROR: Archivo no encontrado dentro del TAR: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose >0) {
        printf("Actualizando informacion de: %s...\n", filename);
    }
    // Open the new version of the file for updating
    FILE *newFile = fopen(filename, "rb");
    if (!newFile) {
        printf("Error opening new version of the file: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose == 2) {
        printf("Calculando la cantidad de bloques requeridos para %s...\n", filename);
    }
    // Calculate the number of blocks required for the new file
    fseek(newFile, 0, SEEK_END);
    int newFileSize = ftell(newFile);
    fseek(newFile, 0, SEEK_SET);
    int newNumBlocks = (newFileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Compare the number of blocks with the original file
    if (newNumBlocks != fatTable.entries[fileIndex].num_blocks) {
        printf("Error: The number of blocks of the new file does not match the number specified in the FAT table.\n");
        fclose(newFile);
        fclose(tarFile);
        return;
    }

    // Calculate the starting block of the file
    int startingBlock = fatTable.entries[fileIndex].starting_block;

    if (verbose == 2) {
        printf("Ubicando el archivo dentro del TAR...\n");
    }
    // Move to the starting block in the TAR file
    fseek(tarFile, sizeof(FatTable) + sizeof(TarHeader) + (startingBlock * BLOCK_SIZE), SEEK_SET);

    // Update the content of the TAR file with the content of the new file
    char buffer[BLOCK_SIZE];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, BLOCK_SIZE, newFile)) > 0) {
        fwrite(buffer, 1, bytesRead, tarFile);
    }

    if (verbose == 2) {
        printf("Actualizando la estructura FAT...\n");
    }
    // Update file size in the FAT table entry
    fatTable.entries[fileIndex].file_size = newFileSize;

    // Update the FAT table in the TAR file
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    saveFatTableToFile(&fatTable, tarFile);

    fclose(newFile);
    fclose(tarFile);

    if (verbose == 2) {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }

    printf("Archivo modifocado en TAR: %s\n", filename);
}

void packTar(char *tar_filename) {
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile) {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0) {
        printf("Archivo %s cargado correctamente.\n\n", tar_filename);
    }

    if (verbose == 2) {
        printf("Extrayendo estructura FAT...\n");
    }
    // Read the FAT table from the TAR file
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    // Variable to track the total size of empty blocks to be subtracted
    int emptyBlockOffset = 0;

    // Iterate through the FAT table to pack empty blocks
    for (int i = 0; i < 256; i++) {
        if (fatTable.entries[i].is_empty) {
            // Add the block size of the empty entry to the offset
            emptyBlockOffset += fatTable.entries[i].num_blocks;
        } else {
            // Adjust the first block of subsequent files after the empty space
            fatTable.entries[i].starting_block -= emptyBlockOffset;
        }
    }

    // Remove empty entries from the FAT table
    int j = 0; // Index for non-empty entries
    for (int i = 0; i < 256; i++) {
        if (!fatTable.entries[i].is_empty) {
            // Copy non-empty entry to the beginning of the FAT table
            fatTable.entries[j] = fatTable.entries[i];
            j++;
        }
    }
    // Clear remaining entries in the FAT table
    for (; j < 256; j++) {
        memset(&fatTable.entries[j], 0, sizeof(FatEntry));
    }

    // Save the updated FAT table back to the TAR file
    fseek(tarFile, 0, SEEK_SET); // Rewind to the beginning of the file
    saveFatTableToFile(&fatTable, tarFile);

    fclose(tarFile);

    printf("\nArchivo TAR compactado exitosamente.\n\n");
}

int main(int argc, char *argv[]) {
    int opt;
    int create = 0, extract = 0, list = 0, delete = 0, update = 0, append = 0, pack = 0;
    char *tarFilename = NULL;
    char *filename = NULL;

    // Procesar los argumentos de la línea de comandos
    while ((opt = getopt(argc, argv, "cxtduvrpf:")) != -1) {
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
            case 'u':
                update = 1;
                break;
            case 'v':
                verbose++;
                break;
            case 'r':
                append = 1;
                break;
            case 'p':
                pack = 1;
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
    if ((create + extract + list + delete + update + append + pack) != 1) {
        fprintf(stderr, "Debe especificar exactamente una operación (-c, -x, -t, -d, -u, -r, -p).\n");
        return 1;
    }

    // Ejecutar la operación especificada
    if (create) {

        if (verbose > 0) {
            printf("Creando archivo TAR...\n");
        }
        createEmptyTar(tarFilename);

        if (verbose == 2) {
            printf("Archivo TAR creado.\n\n");
            printf("Agregando archivos seleccionados...\n");
        }

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
            if (verbose == 2) {
                printf("Extrayendo estructura FAT...\n");
            }

            // Iterar sobre los archivos adicionales para agregarlos al archivo TAR
            for (int i = optind; i < argc; i++) {
                // Agregar el archivo al archivo TAR
                writeFileToTar(argv[i], tarFile, &fatTable);
                if (verbose == 1) {
                    printf("Archivo agregado al TAR: %s\n", argv[i]);
                }
            }

            // Guardar la FAT table actualizada en el archivo TAR
            fseek(tarFile, 0, SEEK_SET);
            saveFatTableToFile(&fatTable, tarFile);
            if (verbose == 2) {
                printf("Actualizando la estructura FAT...\n\n");
            }

            fclose(tarFile);
            if (verbose == 2) {
                printf("\nArchivo TAR cerrado exitosamente.\n\n");
            }
            if (verbose > 0) {
                printf("Archivos agregados a %s\n", tarFilename);
            }
        } else {
            if (verbose > 0) {
                printf("Archivo TAR creado: %s\n", tarFilename);
            }
        }
    } else if (extract) {
        if (verbose > 0) {
            printf("Extrayendo archivos de: %s\n\n", tarFilename);
        }
        readTarFile(tarFilename);
        if (verbose > 0) {
            printf("Archivos extraidos de: %s\n\n", tarFilename);
        }
    } else if (list) {
        listTar(tarFilename);
    } else if (delete) {
        deleteFileFromTar(argv[optind], tarFilename);
    } else if (update) {
        updateFileFromTar(argv[optind], tarFilename);
    } else if (append) {
        // Abrir el archivo TAR en modo de actualización ("rb+")
        FILE *tarFile = fopen(tarFilename, "rb+");
        if (!tarFile) {
            printf("Error abriendo archivo TAR: %s\n", tarFilename);
            return 1;
        }

        if (verbose > 0) {
            printf("Archivo %s cargado conexito.\n\n", tarFilename);
        }

        if (verbose == 2) {
            printf("Extrayendo estructura FAT...\n");
        }
        // Leer la FAT table del archivo TAR
        FatTable fatTable;
        loadFatTableFromFile(&fatTable, tarFile);

        if (verbose == 2) {
            printf("Encontrando un espacio libre...\n");
        }
        // Encontrar un conjunto de bloques libres seguidos para almacenar el archivo
        int startingBlock = -1;
        int numBlocksRequired = -1;
        int lastOccupiedBlock = -1;
        for (int i = 0; i < 256; i++) {
            if (!fatTable.entries[i].is_empty) {
                lastOccupiedBlock = i;
            }
            if (fatTable.entries[i].is_empty || i == lastOccupiedBlock + 1) {
                int numConsecutiveEmptyBlocks = 1;
                for (int j = i + 1; j < 256; j++) {
                    if (fatTable.entries[j].is_empty) {
                        numConsecutiveEmptyBlocks++;
                        if (numConsecutiveEmptyBlocks >= numBlocksRequired) {
                            startingBlock = i;
                            numBlocksRequired = numConsecutiveEmptyBlocks;
                        }
                    } else {
                        break;
                    }
                }
            }
        }

        if (verbose == 2) {
            printf("Ubicando archivo dentro de TAR...\n");
        }
        // Si se encontró un conjunto de bloques libres seguidos
        if (startingBlock != -1 && numBlocksRequired != -1) {
            // Iterar sobre los archivos adicionales para agregarlos al archivo TAR
            for (int i = optind; i < argc; i++) {
                // Agregar el archivo al archivo TAR
                writeFileToTar(argv[i], tarFile, &fatTable);
            }

            if (verbose == 2) {
                printf("Actualizando la estructura FAT...\n\n");
            }
            // Guardar la FAT table actualizada en el archivo TAR
            fseek(tarFile, 0, SEEK_SET);
            saveFatTableToFile(&fatTable, tarFile);

            fclose(tarFile);
            printf("Archivo(s) agregado(s) a %s\n", tarFilename);
        } else {
            printf("No se encontró un conjunto de bloques libres seguidos para almacenar el archivo.\n");
            fclose(tarFile);
            return 1;
        }
    } else if (pack) {
        packTar(tarFilename);
    }

    return 0;
}
