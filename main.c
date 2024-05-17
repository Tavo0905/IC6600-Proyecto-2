#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/getopt_core.h>

#define BLOCK_SIZE 262144 // 256 kB
int verbose = 0;

typedef struct FatEntry
{
    char filename[12];           // Nombre del archivo
    unsigned int starting_block; // Bloque inicial
    unsigned int num_blocks;     // Tamanno en bloques
    unsigned int file_size;      // Tamanno en bytes
    unsigned char is_empty;      // Flag que indica si esta vacio
} FatEntry;

typedef struct FatTable
{
    FatEntry entries[256]; // 256 registros en el FAT
} FatTable;

typedef struct TarHeader
{
    char magic_number[6];       // Numero magico para identificar el TAR
    char version_number[2];     // No. de version
    char user_name[100];        // Nombre de usuario
    char group_name[100];       // Nombre del grupo
    char modification_time[12]; // Hora de modificacion
    char checksum[8];           // Checksum
    char file_size[12];         // Tamanno en bytes
    char block_size[12];        // Tamanno en bloques
    char linked_tar_file[100];  // Nombre del archivo TAR
    char prefix[155];           // Prefijo para los archivos
} TarHeader;

void initializeFatTable(FatTable *fatTable)
{
    for (int i = 0; i < 256; i++)
    {
        fatTable->entries[i].is_empty = 1;
        fatTable->entries[i].starting_block = 0;
        fatTable->entries[i].num_blocks = 0;
        strcpy(fatTable->entries[i].filename, "\0");
        fatTable->entries[i].file_size = 0;
    }
}

int findEmptyFatEntry(FatTable *fatTable)
{
    for (int i = 0; i < 256; i++)
    {
        if (fatTable->entries[i].is_empty)
        {
            return i;
        }
    }
    return -1;
}

void saveFatTableToFile(FatTable *fatTable, FILE *tarFile)
{
    fwrite(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void loadFatTableFromFile(FatTable *fatTable, FILE *tarFile)
{
    fread(fatTable->entries, sizeof(FatEntry), 256, tarFile);
}

void printFatTable(FatTable *fatTable)
{
    printf("-------------------------------------------------------------------------------------\n");
    printf("| %-20s | %-12s | %-12s | %-15s | %-10s |\n", "Filename", "First Block", "Block Size", "File Size", "Is Empty");
    printf("|----------------------|--------------|--------------|-----------------|------------|\n");

    for (int i = 0; i < 20; i++)
    {
        FatEntry entry = fatTable->entries[i];
        printf("| %-20s | %12d | %12d | %15d | %10d |\n", entry.filename, entry.starting_block, entry.num_blocks, entry.file_size, entry.is_empty);
    }
    printf("-------------------------------------------------------------------------------------\n");
}

void createEmptyTar(char *tarFilename)
{
    FILE *tarFile = fopen(tarFilename, "wb");
    if (!tarFile)
    {
        printf("ERROR: no se pudo crear el archivo %s\n", tarFilename);
        exit(-1);
    }

    // FAT
    FatTable fatTable;
    initializeFatTable(&fatTable);

    // Guardar el FAT en el TAR
    saveFatTableToFile(&fatTable, tarFile);

    // Guardar el TAR Header
    TarHeader tarHeader;
    memset(&tarHeader, 0, sizeof(TarHeader));
    strcpy(tarHeader.magic_number, "ustar");  // Numero magico
    fwrite(&tarHeader, sizeof(TarHeader), 1, tarFile);

    fclose(tarFile);
}

void writeFileToTar(char *filename, FILE *tarFile, FatTable *fatTable)
{
    FILE *sourceFile = fopen(filename, "rb");
    if (!sourceFile)
    {
        printf("ERROR: No se encontro el archivo %s\n", filename);
        return;
    }

    if (verbose == 2)
    {
        printf("Obteniendo el tamano de %s...\n", filename);
    }

    // Obtener tamanno (bytes)
    fseek(sourceFile, 0, SEEK_END);
    int file_size = ftell(sourceFile);
    fseek(sourceFile, 0, SEEK_SET);

    if (verbose == 2)
    {
        printf("Calculando la cantidad de bloques requeridos para %s...\n", filename);
    }

    // Calcular tamanno (bloques)
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (verbose == 2)
    {
        printf("Ajustando posiciones dentro del FAT...\n");
    }

    // Ultimo bloque ocupado
    int lastOccupiedBlock = -1;
    for (int i = 0; i < 256; i++)
    {
        if (!fatTable->entries[i].is_empty && i < fatTable->entries[i].starting_block + fatTable->entries[i].num_blocks)
        {
            lastOccupiedBlock = i;
        }
    }

    // Calcular el bloque inicial
    int starting_block;
    if (lastOccupiedBlock == -1)
    {
        starting_block = 0;
    }
    else
    {
        starting_block = fatTable->entries[lastOccupiedBlock].starting_block + fatTable->entries[lastOccupiedBlock].num_blocks;
    }

    // Actualizar FAT
    int currentBlock = findEmptyFatEntry(fatTable);
    if (currentBlock == -1)
    {
        printf("ERROR: Se ha superado la cantidad maxima de datos.\n");
        fclose(sourceFile);
        return;
    }

    if (verbose == 2)
    {
        printf("Actualizando estructura FAT...\n");
    }

    fatTable->entries[currentBlock].is_empty = 0;
    fatTable->entries[currentBlock].starting_block = starting_block;
    fatTable->entries[currentBlock].num_blocks = num_blocks;
    strcpy(fatTable->entries[currentBlock].filename, filename);
    fatTable->entries[currentBlock].file_size = file_size;
    int offset = 0;

    if (verbose == 2)
    {
        printf("Leyendo el contenido del archivo...\n");
    }
    // Usar buffer para escribir el archivo en el TAR
    char buffer[BLOCK_SIZE];
    while (fread(buffer, 1, BLOCK_SIZE, sourceFile) > 0)
    {
        fwrite(buffer, 1, BLOCK_SIZE, tarFile);
        offset += BLOCK_SIZE;

        if (offset == file_size)
        {
            break;
        }

        currentBlock = findEmptyFatEntry(fatTable);
        if (currentBlock == -1)
        {
            printf("ERROR: Se ha superado la cantidad maxima de datos.\n");
            break;
        }
    }

    fclose(sourceFile);
    if (verbose == 2)
    {
        printf("Archivo agregado al TAR: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable->entries[currentBlock].starting_block, num_blocks);
    }
}

void readTarFile(char *tarFilename)
{
    FILE *tarFile = fopen(tarFilename, "rb");
    if (!tarFile)
    {
        printf("ERROR: No se encontro el archivo TAR: %s\n", tarFilename);
        return;
    }

    if (verbose == 2)
    {
        printf("Archivo %s cargado conexito.\n\n", tarFilename);
    }

    // Calcular el offset despues del inicio del TAR
    int offset = sizeof(FatTable) + sizeof(TarHeader);

    // FAT
    FatTable fatTable;
    fseek(tarFile, 0, SEEK_SET);
    fread(&fatTable, sizeof(FatTable), 1, tarFile);

    if (verbose == 2)
    {
        printf("Extrayendo estructura FAT...\n\n");
    }

    // Ciclar por todos los registros
    for (int i = 0; i < 256; i++)
    {
        if (fatTable.entries[i].is_empty)
        {
            continue; // Saltarse los vacios
        }

        // Obtener informacion del FAT
        char filename[13];
        strncpy(filename, fatTable.entries[i].filename, 12);
        filename[12] = '\0';
        int file_size = fatTable.entries[i].file_size;
        int starting_block = fatTable.entries[i].starting_block;

        // Archivo por extraer
        FILE *outFile = fopen(filename, "wb");
        if (!outFile)
        {
            printf("ERROR: no se pudo extraer el archivo %s\n", filename);
            continue;
        }

        if (verbose == 2)
        {
            printf("Extrayendo el contenido del archivo %s.\n", filename);
        }
        // Extract el archivo por bloques
        int bytes_read = 0;
        int currentBlock = starting_block;
        char buffer[BLOCK_SIZE];
        while (bytes_read < file_size)
        {
            int bytes_to_read = (file_size - bytes_read) < BLOCK_SIZE ? (file_size - bytes_read) : BLOCK_SIZE;
            fseek(tarFile, offset + starting_block * BLOCK_SIZE + bytes_read, SEEK_SET);
            int bytes_actually_read = fread(buffer, 1, bytes_to_read, tarFile);
            
            if (bytes_actually_read < bytes_to_read && feof(tarFile))
            {
                printf("ERROR: EOF encontrado dentro del bloque %d.\n", currentBlock);
                break;
            }
            else if (bytes_actually_read != bytes_to_read)
            {
                printf("ERROR: No se pudo leer los bytes %d del bloque %d.\n", bytes_to_read, currentBlock);
                break;
            }

            fwrite(buffer, 1, bytes_actually_read, outFile);

            bytes_read += bytes_actually_read;
            currentBlock++;
        }

        fclose(outFile);
        if (verbose == 1)
        {
            printf("Archivo extraído: %s\n", filename);
        }
        else if (verbose == 2)
        {
            printf("Archivo extraído: %s, Tamaño: %d bytes, Bloques iniciales: %d, Bloques: %d\n", filename, file_size, fatTable.entries[i].starting_block, fatTable.entries[i].num_blocks);
        }
    }

    fclose(tarFile);

    if (verbose == 2)
    {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
}

void listTar(char *tar_filename)
{
    FILE *tarFile = fopen(tar_filename, "rb");
    if (!tarFile)
    {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0)
    {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2)
    {
        printf("Extrayendo estructura FAT...\n");
    }
    // FAT
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    if (verbose == 2)
    {
        printf("Imprimiendo tabla de archivos...\n");
    }
    printFatTable(&fatTable);
    if (verbose == 2)
    {
        printf("Tabla de archivos desplegada con exito.\n");
    }

    fclose(tarFile);
    if (verbose == 2)
    {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
}

void deleteFileFromTar(char *filename, char *tar_filename)
{
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile)
    {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0)
    {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2)
    {
        printf("Extrayendo estructura FAT...\n");
    }
    // FAT
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    // Buscar archivo
    int fileIndex = -1;
    for (int i = 0; i < 256; i++)
    {
        if (!fatTable.entries[i].is_empty && strcmp(fatTable.entries[i].filename, filename) == 0)
        {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        printf("ERROR: Archivo no encontrado dentro del TAR: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose > 0)
    {
        printf("Eliminando archivo %s...\n", filename);
    }
    // Marcar registro como vacio
    fatTable.entries[fileIndex].is_empty = 1;
    fatTable.entries[fileIndex].file_size = 0;
    strcpy(fatTable.entries[fileIndex].filename, "");

    // Actualizar TAR
    fseek(tarFile, 0, SEEK_SET);
    saveFatTableToFile(&fatTable, tarFile);

    if (verbose == 2)
    {
        printf("Actualizando la estructura FAT...\n");
    }

    fclose(tarFile);
    if (verbose == 2)
    {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }
    printf("Archivo eliminado del TAR: %s\n", filename);
}

void updateFileFromTar(char *filename, char *tar_filename)
{
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile)
    {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0)
    {
        printf("Archivo %s cargado conexito.\n\n", tar_filename);
    }

    if (verbose == 2)
    {
        printf("Extrayendo estructura FAT...\n");
    }
    // FAT
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);

    if (verbose == 2)
    {
        printf("Buscando archivo: %s...\n", filename);
    }
    // Buscar archivo
    int fileIndex = -1;
    for (int i = 0; i < 256; i++)
    {
        if (!fatTable.entries[i].is_empty && strcmp(fatTable.entries[i].filename, filename) == 0)
        {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        printf("ERROR: Archivo no encontrado dentro del TAR: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose > 0)
    {
        printf("Actualizando informacion de: %s...\n", filename);
    }
    // Abrir nueva version del archivo
    FILE *newFile = fopen(filename, "rb");
    if (!newFile)
    {
        printf("Error opening new version of the file: %s\n", filename);
        fclose(tarFile);
        return;
    }

    if (verbose == 2)
    {
        printf("Calculando la cantidad de bloques requeridos para %s...\n", filename);
    }
    // Calcular el numero de bloques del archivo
    fseek(newFile, 0, SEEK_END);
    int newFileSize = ftell(newFile);
    fseek(newFile, 0, SEEK_SET);
    int newNumBlocks = (newFileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Comparar la cantidad de bloques del actualizado con el original
    if (newNumBlocks != fatTable.entries[fileIndex].num_blocks)
    {
        printf("Error: The number of blocks of the new file does not match the number specified in the FAT table.\n");
        fclose(newFile);
        fclose(tarFile);
        return;
    }


    if (verbose == 2)
    {
        printf("Ubicando el archivo dentro del TAR...\n");
    }
    // Ir al bloque inicial
    int startingBlock = fatTable.entries[fileIndex].starting_block;
    fseek(tarFile, sizeof(FatTable) + sizeof(TarHeader) + (startingBlock * BLOCK_SIZE), SEEK_SET);

    // Actualizar contenido del archivo
    char buffer[BLOCK_SIZE];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, BLOCK_SIZE, newFile)) > 0)
    {
        fwrite(buffer, 1, bytesRead, tarFile);
    }

    if (verbose == 2)
    {
        printf("Actualizando la estructura FAT...\n");
    }

    // Actualizar FAT
    fatTable.entries[fileIndex].file_size = newFileSize;
    fseek(tarFile, 0, SEEK_SET);
    saveFatTableToFile(&fatTable, tarFile);

    fclose(newFile);
    fclose(tarFile);

    if (verbose == 2)
    {
        printf("\nArchivo TAR cerrado exitosamente.\n\n");
    }

    printf("Archivo modifocado en TAR: %s\n", filename);
}

void packTar(char *tar_filename)
{
    FILE *tarFile = fopen(tar_filename, "rb+");
    if (!tarFile)
    {
        printf("ERROR: No se encontro el archivo %s.\n", tar_filename);
        return;
    }

    if (verbose > 0)
    {
        printf("Archivo %s cargado correctamente.\n\n", tar_filename);
    }

    if (verbose == 2)
    {
        printf("Extrayendo estructura FAT...\n");
    }
    // FAT
    FatTable fatTable;
    loadFatTableFromFile(&fatTable, tarFile);


    int emptyBlockOffset = 0;

    // Iterar registros del FAT
    for (int i = 0; i < 256; i++)
    {
        if (fatTable.entries[i].is_empty)
        {
            // Aumentar el offset de bloques vacios
            emptyBlockOffset += fatTable.entries[i].num_blocks;
        }
        else
        {
            // Ajustar el bloque inicial del archivo por la cantidad de bloques vacios anteriores
            fatTable.entries[i].starting_block -= emptyBlockOffset;
        }
    }

    // Eliminar registros vacios
    int j = 0;
    for (int i = 0; i < 256; i++)
    {
        if (!fatTable.entries[i].is_empty)
        {
            // Mover registros no vacios
            fatTable.entries[j] = fatTable.entries[i];
            j++;
        }
    }
    // Limpiar registros restantes
    for (; j < 256; j++)
    {
        memset(&fatTable.entries[j], 0, sizeof(FatEntry));
    }

    // Actualizar FAT
    fseek(tarFile, 0, SEEK_SET);
    saveFatTableToFile(&fatTable, tarFile);

    fclose(tarFile);

    printf("\nArchivo TAR compactado exitosamente.\n\n");
}

int main(int argc, char *argv[])
{
    int opt;
    int create = 0, extract = 0, list = 0, delete = 0, update = 0, append = 0, pack = 0;
    char *tarFilename = NULL;
    char *filename = NULL;

    // Procesar los argumentos de la línea de comandos
    while ((opt = getopt(argc, argv, "cxtduvrpf:")) != -1)
    {
        switch (opt)
        {
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
    if ((create + extract + list + delete +update + append + pack) != 1)
    {
        fprintf(stderr, "Debe especificar exactamente una operación (-c, -x, -t, -d, -u, -r, -p).\n");
        return 1;
    }

    // Ejecutar la operación especificada
    if (create)
    {

        if (verbose > 0)
        {
            printf("Creando archivo TAR...\n");
        }
        createEmptyTar(tarFilename);

        if (verbose == 2)
        {
            printf("Archivo TAR creado.\n\n");
            printf("Agregando archivos seleccionados...\n");
        }

        // Si hay archivos adicionales para agregar al archivo TAR recién creado
        if (optind < argc)
        {
            // Abrir el archivo TAR en modo de actualización ("rb+")
            FILE *tarFile = fopen(tarFilename, "rb+");
            if (!tarFile)
            {
                printf("Error abriendo archivo TAR: %s\n", tarFilename);
                return 1;
            }

            // Leer la FAT table del archivo TAR
            FatTable fatTable;
            loadFatTableFromFile(&fatTable, tarFile);
            if (verbose == 2)
            {
                printf("Extrayendo estructura FAT...\n");
            }

            // Iterar sobre los archivos adicionales para agregarlos al archivo TAR
            for (int i = optind; i < argc; i++)
            {
                // Agregar el archivo al archivo TAR
                writeFileToTar(argv[i], tarFile, &fatTable);
                if (verbose == 1)
                {
                    printf("Archivo agregado al TAR: %s\n", argv[i]);
                }
            }

            // Guardar la FAT table actualizada en el archivo TAR
            fseek(tarFile, 0, SEEK_SET);
            saveFatTableToFile(&fatTable, tarFile);
            if (verbose == 2)
            {
                printf("Actualizando la estructura FAT...\n\n");
            }

            fclose(tarFile);
            if (verbose == 2)
            {
                printf("\nArchivo TAR cerrado exitosamente.\n\n");
            }
            if (verbose > 0)
            {
                printf("Archivos agregados a %s\n", tarFilename);
            }
        }
        else
        {
            if (verbose > 0)
            {
                printf("Archivo TAR creado: %s\n", tarFilename);
            }
        }
    }
    else if (extract)
    {
        if (verbose > 0)
        {
            printf("Extrayendo archivos de: %s\n\n", tarFilename);
        }
        readTarFile(tarFilename);
        if (verbose > 0)
        {
            printf("Archivos extraidos de: %s\n\n", tarFilename);
        }
    }
    else if (list)
    {
        listTar(tarFilename);
    }
    else if (delete)
    {
        deleteFileFromTar(argv[optind], tarFilename);
    }
    else if (update)
    {
        updateFileFromTar(argv[optind], tarFilename);
    }
    else if (append)
    {
        // Abrir el archivo TAR en modo de actualización ("rb+")
        FILE *tarFile = fopen(tarFilename, "rb+");
        if (!tarFile)
        {
            printf("Error abriendo archivo TAR: %s\n", tarFilename);
            return 1;
        }

        if (verbose > 0)
        {
            printf("Archivo %s cargado conexito.\n\n", tarFilename);
        }

        if (verbose == 2)
        {
            printf("Extrayendo estructura FAT...\n");
        }
        // Leer la FAT table del archivo TAR
        FatTable fatTable;
        loadFatTableFromFile(&fatTable, tarFile);

        if (verbose == 2)
        {
            printf("Encontrando un espacio libre...\n");
        }
        // Encontrar un conjunto de bloques libres seguidos para almacenar el archivo
        int startingBlock = -1;
        int numBlocksRequired = -1;
        int lastOccupiedBlock = -1;
        for (int i = 0; i < 256; i++)
        {
            if (!fatTable.entries[i].is_empty)
            {
                lastOccupiedBlock = i;
            }
            if (fatTable.entries[i].is_empty || i == lastOccupiedBlock + 1)
            {
                int numConsecutiveEmptyBlocks = 1;
                for (int j = i + 1; j < 256; j++)
                {
                    if (fatTable.entries[j].is_empty)
                    {
                        numConsecutiveEmptyBlocks++;
                        if (numConsecutiveEmptyBlocks >= numBlocksRequired)
                        {
                            startingBlock = i;
                            numBlocksRequired = numConsecutiveEmptyBlocks;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if (verbose == 2)
        {
            printf("Ubicando archivo dentro de TAR...\n");
        }
        // Si se encontró un conjunto de bloques libres seguidos
        if (startingBlock != -1 && numBlocksRequired != -1)
        {
            // Iterar sobre los archivos adicionales para agregarlos al archivo TAR
            for (int i = optind; i < argc; i++)
            {
                // Agregar el archivo al archivo TAR
                writeFileToTar(argv[i], tarFile, &fatTable);
            }

            if (verbose == 2)
            {
                printf("Actualizando la estructura FAT...\n\n");
            }
            // Guardar la FAT table actualizada en el archivo TAR
            fseek(tarFile, 0, SEEK_SET);
            saveFatTableToFile(&fatTable, tarFile);

            fclose(tarFile);
            printf("Archivo(s) agregado(s) a %s\n", tarFilename);
        }
        else
        {
            printf("No se encontró un conjunto de bloques libres seguidos para almacenar el archivo.\n");
            fclose(tarFile);
            return 1;
        }
    }
    else if (pack)
    {
        packTar(tarFilename);
    }

    return 0;
}
