#include <stdio.h>     // Standard I/O functions
#include <stdlib.h>    // Standard library definitions for convenience functions and memory allocation
#include <string.h>    // String operations like strlen and strcpy
#include <time.h>      // Time manipulation functions like mktime
#include <stdint.h>    //for uint8_t etc
#include <fcntl.h>     // File control options for open
#include <unistd.h>    // POSIX constants and system calls like close
#include <sys/types.h> // Definitions for system types like off_t
#include <sys/stat.h>  // Definitions for file status like fstat
#include <stdbool.h>   //for boolean
#include <wchar.h>     // for wprintf
#include <locale.h>     //temporary to fix the wprintf issue

// struct definition to represent BOOT SECTOR of FAT16 file system
typedef struct __attribute__((__packed__))
{
    uint8_t BS_jmpBoot[3];    // x86 jump instr. to boot code
    uint8_t BS_OEMName[8];    // What created the filesystem
    uint16_t BPB_BytsPerSec;  // Bytes per Sector
    uint8_t BPB_SecPerClus;   // Sectors per Cluster
    uint16_t BPB_RsvdSecCnt;  // Reserved Sector Count
    uint8_t BPB_NumFATs;      // Number of copies of FAT
    uint16_t BPB_RootEntCnt;  // FAT12/FAT16: size of root DIR
    uint16_t BPB_TotSec16;    // Sectors, may be 0, see below
    uint8_t BPB_Media;        // Media type, e.g. fixed
    uint16_t BPB_FATSz16;     // Sectors in FAT (FAT12 or FAT16)
    uint16_t BPB_SecPerTrk;   // Sectors per Track
    uint16_t BPB_NumHeads;    // Number of heads in disk
    uint32_t BPB_HiddSec;     // Hidden Sector count
    uint32_t BPB_TotSec32;    // Sectors if BPB_TotSec16 == 0
    uint8_t BS_DrvNum;        // 0 = floppy, 0x80 = hard disk
    uint8_t BS_Reserved1;     //
    uint8_t BS_BootSig;       // Should = 0x29
    uint32_t BS_VolID;        // 'Unique' ID for volume
    uint8_t BS_VolLab[11];    // Non zero terminated string
    uint8_t BS_FilSysType[8]; // e.g. 'FAT16 ' (Not 0 term.)
} BootSector;

// struct definition to represent a DIRECTORY ENTRY in a FAT16 file system
typedef struct __attribute__((__packed__))
{
    uint8_t DIR_Name[11];     // Non zero terminated string
    uint8_t DIR_Attr;         // File attributes
    uint8_t DIR_NTRes;        // Used by Windows NT, ignore
    uint8_t DIR_CrtTimeTenth; // Tenths of sec. 0...199
    uint16_t DIR_CrtTime;     // Creation Time in 2s intervals
    uint16_t DIR_CrtDate;     // Date file created
    uint16_t DIR_LstAccDate;  // Date of last read or write
    uint16_t DIR_FstClusHI;   // Top 16 bits file's 1st cluster
    uint16_t DIR_WrtTime;     // Time of last write
    uint16_t DIR_WrtDate;     // Date of last write
    uint16_t DIR_FstClusLO;   // Lower 16 bits file's 1st cluster
    uint32_t DIR_FileSize;    // File size in bytes
} DirectoryEntry;

// struct definition to represent a volume
typedef struct
{
    int fd;
    BootSector bootSector;
} Volume;

// struct definition to represent an open file.
typedef struct
{
    Volume *volume;          // Volume where the file resides
    DirectoryEntry dirEntry; // Directory entry of the file
    uint32_t fileSize;       // Size of the file
    uint32_t filePosition;   // Current position in the file
    uint16_t currentCluster; // Current cluster in the file chain
} File;

// struct definition to represent LONG DIRECTORY ENTRY
typedef struct __attribute__((__packed__))
{
    uint8_t LDIR_Ord;        // Order/ position in sequence/ set
    uint8_t LDIR_Name1[10];  // First 5 UNICODE characters
    uint8_t LDIR_Attr;       // = ATTR_LONG_NAME (xx001111)
    uint8_t LDIR_Type;       // Should = 0
    uint8_t LDIR_Chksum;     // Checksum of short name
    uint8_t LDIR_Name2[12];  // Middle 6 UNICODE characters
    uint16_t LDIR_FstClusLO; // MUST be zero
    uint8_t LDIR_Name3[4];   // Last 2 UNICODE characters
} LongDirectoryEntry;

//function to determine if a directory entry is for a long file name
bool isLongNameEntry(const DirectoryEntry *entry)
{
    return (entry->DIR_Attr == 0x0F);
}


//extracts uicode characters from a long directory entry and inputs it into a buffer
void parseLongNameEntry(const LongDirectoryEntry *longEntry, wchar_t *longNameBuffer, int position)
{
    int j = 0;//counter to keep track of character pos.
    for (int i = 0; i < 5; i++) //looping through the first 5 UNICODE characters
    {
        longNameBuffer[position + j++] = (wchar_t)longEntry->LDIR_Name1[i * 2];//extracting first byte and converting to wide character for storage
    }
    for (int i = 0; i < 6; i++)//looping through the next 6
    {
        longNameBuffer[position + j++] = (wchar_t)longEntry->LDIR_Name2[i * 2];//extracting first byte and converting to wide character for storage
    }
    for (int i = 0; i < 2; i++)//looping through the last 2
    {
        longNameBuffer[position + j++] = (wchar_t)longEntry->LDIR_Name3[i * 2];//extracting the last 2 characters 
    }//longNameBuffer should contain the complete UNICODE name, why isnt it printing?
}

//prints information about the long directory entry
void printLongDirectoryEntry(const LongDirectoryEntry *longEntry)
{
    wchar_t longName[14]; // Buffer to hold the name part

    // Printing order and attribute
    printf("Long Entry Order: %u\n", longEntry->LDIR_Ord);

    printf("Long Entry Attribute: ");
    if (longEntry->LDIR_Attr & 0x01)
        printf("Read-Only ");
    if (longEntry->LDIR_Attr & 0x02)
        printf("Hidden ");
    if (longEntry->LDIR_Attr & 0x04)
        printf("System ");
    if (longEntry->LDIR_Attr & 0x08)
        printf("Volume Label ");
    if (longEntry->LDIR_Attr & 0x10)
        printf("SubDirectory ");
    if (longEntry->LDIR_Attr & 0x20)
        printf("Archive ");
    printf("\n");

    //Assmebling the long file name from the three parts
    int j = 0;//counter to keep track of character pos
    for (int i = 0; i < 5; i++)
    {
        longName[j++] = (wchar_t)longEntry->LDIR_Name1[i * 2];//same as before
    }
    for (int i = 0; i < 6; i++)
    {
        longName[j++] = (wchar_t)longEntry->LDIR_Name2[i * 2];
    }
    for (int i = 0; i < 2; i++)
    {
        longName[j++] = (wchar_t)longEntry->LDIR_Name3[i * 2];
    }
    longName[j] = L'\0'; // Null-terminate the long file name

    wprintf(L"Long Entry Name: %ls\n", longName);// this should be printing the long file name but it isn't, still debugging.

    // Printing other fields
    printf("Long Entry Type: %u\n", longEntry->LDIR_Type);
    printf("Long Entry Checksum: 0x%02x\n", longEntry->LDIR_Chksum);
    printf("Long Entry First Cluster: %u\n", longEntry->LDIR_FstClusLO);
    printf("\n");
}

//opens a disk image and returns the file descriptor.
int openDiskImage(const char *filepath)
{
    // opening the FAT disk for reading
    int fileDesc = open(filepath, O_RDONLY);
    if (fileDesc < 0)
    {
        perror("Error opening file\n");
    }
    return fileDesc;
}

//reads a specific number of bytes from a given offset in the disk image
ssize_t readFromDiskImage(int fd, off_t offset, void *buffer, size_t numBytes)
{
    // Move the file pointer to the specific offset
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        perror("Error seeking in file\n");
        return -1;
    }
    return read(fd, buffer, numBytes);
}

//just closes the disk image
void closeDiskImage(int fd)
{
    close(fd);
}

//function that reads and returns the boot sector of the disk image
BootSector readBootSector(int fd)
{
    BootSector bs;
    if (readFromDiskImage(fd, 0, &bs, sizeof(BootSector)) != sizeof(BootSector))
    {
        perror("Error reading BootSector");
        exit(EXIT_FAILURE);
    }
    return bs;
}

//function that allocates and returns a buffer of a specified size
char *allocateBuffer(size_t numBytes)
{
    char *buffer = malloc(numBytes);
    if (!buffer)
    {
        perror("Error allocating memory");
    }
    return buffer;
}

//function that prints the information from the boot sector
void printBSInfo(const BootSector *bs)
{
    printf("Boot Sector and BIOS Parameter Block Details:\n");
    printf("OEM Name: %.8s\n", bs->BS_OEMName);
    printf("Bytes Per Sector: %u\n", bs->BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", bs->BPB_SecPerClus);
    printf("Reserved Sector Count: %u\n", bs->BPB_RsvdSecCnt);
    printf("Number of FATs: %u\n", bs->BPB_NumFATs);
    printf("Root Entry Count: %u\n", bs->BPB_RootEntCnt);
    printf("Total Sectors (16-bit): %u\n", bs->BPB_TotSec16);
    printf("Media Type: 0x%02x\n", bs->BPB_Media);
    printf("Sectors Per FAT: %u\n", bs->BPB_FATSz16);
    printf("Sectors Per Track: %u\n", bs->BPB_SecPerTrk);
    printf("Number of Heads: %u\n", bs->BPB_NumHeads);
    printf("Hidden Sectors: %u\n", bs->BPB_HiddSec);
    printf("Total Sectors (32-bit): %u\n", bs->BPB_TotSec32);
}

//function that loads the file allocation table from memory
uint16_t *loadFAT(int fd, const BootSector *bs)
{
    size_t fatSize = bs->BPB_FATSz16 * bs->BPB_BytsPerSec;
    uint16_t *fat = malloc(fatSize);
    if (!fat)
    {
        perror("Error allocating memory for FAT");
        return NULL;
    }

    off_t fatOffset = bs->BPB_RsvdSecCnt * bs->BPB_BytsPerSec;
    if (lseek(fd, fatOffset, SEEK_SET) < 0)
    {
        perror("Error seeking to FAT");
        free(fat);
        return NULL;
    }

    if (read(fd, fat, fatSize) != fatSize)
    {
        perror("Error reading FAT");
        free(fat);
        return NULL;
    }

    return fat;
}

//function that builds a chain of clusters given a specified cluster
uint16_t *getClusterChain(int fd, const BootSector *bs, uint16_t startingCluster)
{
    uint16_t *fat = loadFAT(fd, bs);
    if (!fat)
        return NULL;

    // Assuming a maximum sie
    uint16_t *chain = malloc(65536 * sizeof(uint16_t));
    if (!chain)
    {
        free(fat);
        perror("Error allocating memory for cluster chain");
        return NULL;
    }

    int i = 0;
    uint16_t currentCluster = startingCluster;
    while (currentCluster < 0xFFF8)
    {
        chain[i++] = currentCluster;
        currentCluster = fat[currentCluster];
    }

    chain[i] = 0xFFFF;

    free(fat);
    return chain;
}

//function that prints the details of a directory entry
void printDirectoryEntry(const DirectoryEntry *entry)
{

    char fileName[13]; // array to hold the filename

    // checking for unused or deleted or error entry
    if (entry->DIR_Name[0] == 0x00 || entry->DIR_Name[0] == 0xE5)
        return;

    // checking if entry is a regular file or directory or not
    if ((entry->DIR_Attr & 0x08) || (entry->DIR_Attr & 0x02) || (entry->DIR_Attr & 0x04))
        return;

    // checking if filename is valid
    for (int i = 0; i < 11; i++)
    {
        if ((entry->DIR_Name[i] != 0x20) &&
                (entry->DIR_Name[i] < 0x20) ||
            (entry->DIR_Name[i] > 0x7E))
            return;
    }

    // Printing the filename in ASCII, handle non-printable characters
    printf("Filename: ");
    for (int i = 0; i < 11; i++)
    {
        char ch = entry->DIR_Name[i];
        if (ch >= 32 && ch <= 126)
        { // Printable ASCII range
            printf("%c", ch);
        }
        else
        {
            printf("."); // Replace non-printable characters with a dot
        }
    }
    printf("\n");

    // Printing the file attributes
    printf("File Attributes: ");
    if (entry->DIR_Attr & 0x01)
        printf("Read-Only ");
    if (entry->DIR_Attr & 0x02)
        printf("Hidden ");
    if (entry->DIR_Attr & 0x04)
        printf("System ");
    if (entry->DIR_Attr & 0x08)
        printf("Volume Label ");
    if (entry->DIR_Attr & 0x10)
        printf("SubDirectory ");
    if (entry->DIR_Attr & 0x20)
        printf("Archive ");
    printf("\n");

    // printing file size
    printf("File Size: %u\n", entry->DIR_FileSize);

    // printinf first cluster
    printf("First Cluster: %u\n", ((uint32_t)entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO));

    // decoding and printing creation date
    uint16_t date = entry->DIR_CrtDate;
    uint16_t time = entry->DIR_CrtTime;
    printf("Creation Date: %d-%02d-%02d\n",
           ((date >> 9) & 0x7F) + 1980,
           (date >> 5) & 0x0F,
           date & 0x1F);

    printf("Creation Time: %02d:%02d:%02d\n",
           time >> 11,
           (time >> 5) & 0x3F,
           (time & 0x1F) * 2);

    // decoding and printing last write date
    date = entry->DIR_WrtDate;
    time = entry->DIR_WrtTime;
    printf("Last Write Date: %d-%02d-%02d\n",
           ((date >> 9) & 0x7F) + 1980,
           (date >> 5) & 0x0F,
           date & 0x1F);

    printf("Last Write Time: %02d:%02d:%02d\n",
           time >> 11,
           (time >> 5) & 0x3F,
           (time & 0x1F) * 2);
    printf("\n");
}

//function that reads and prints entries from a root directory
void readDirectory(int fd, const BootSector *bs)
{
    size_t rootDirSize = bs->BPB_RootEntCnt * sizeof(DirectoryEntry);//calculating size of root directory 
    off_t rootDirOffset = (bs->BPB_RsvdSecCnt + (bs->BPB_NumFATs * bs->BPB_FATSz16)) * bs->BPB_BytsPerSec;//calculating starting offset

    DirectoryEntry *rootDir = malloc(rootDirSize);//allocate memory for reading the root directory entries from the size
    if (!rootDir)
    {
        perror("Error Allocating Memory for root directory");
        return;
    }

    if (lseek(fd, rootDirOffset, SEEK_SET) < 0)//seeking to the start of the root directory
    {
        perror("Error seeking to root directory");
        free(rootDir);
        return;
    }

    if (read(fd, rootDir, rootDirSize) != rootDirSize)
    {
        perror("Error reading root directory");
        free(rootDir);
        return;
    }

    wchar_t longNameBuffer[256];//buffer for storing longfilenames while processing entries
    memset(longNameBuffer, 0, sizeof(longNameBuffer));

    //iterating through each entry in the root directory
    for (int i = bs->BPB_RootEntCnt - 1; i >= 0; i--)
    {
        if (isLongNameEntry(&rootDir[i]))
        {
            LongDirectoryEntry *longEntry = (LongDirectoryEntry *)&rootDir[i];
            int order = longEntry->LDIR_Ord & ~0x40; // Masking out the last entry flag
            int position = (order - 1) * 13;         // Calculate buffer position for this part of the name
            parseLongNameEntry(longEntry, longNameBuffer, position);
        }
        else
        {
            //if assembly was complete, print it, this is where it might be wrong?
            if (longNameBuffer[0] != L'\0')
            {
                wprintf(L"Long Filename: %ls\n", longNameBuffer);
                printLongDirectoryEntry((LongDirectoryEntry *)&rootDir[i]); // Corrected to pass the right argument
                memset(longNameBuffer, 0, sizeof(longNameBuffer));          // Clear buffer after using
            }
            printDirectoryEntry(&rootDir[i]);//call printing function
        }
    }

    free(rootDir);
}

// below are the functions for task 5

//functions that convert a cluster number to a corresponding sector number
off_t clusterToSector(const Volume *volume, uint16_t cluster)
{
    //calculating the first data sector offset
    uint32_t firstDataSector = volume->bootSector.BPB_RsvdSecCnt + 
                               (volume->bootSector.BPB_NumFATs * volume->bootSector.BPB_FATSz16) + 
                               ((volume->bootSector.BPB_RootEntCnt * 32) + (volume->bootSector.BPB_BytsPerSec - 1)) / 
                                volume->bootSector.BPB_BytsPerSec;
    //calculating the sector number for a given cluster
    return firstDataSector + (cluster - 2) * volume->bootSector.BPB_SecPerClus;
}

//function that retrieves the next cluster in the chain from the FAT file
uint16_t nextCluster(const Volume *volume, uint16_t currentCluster)
{
    uint16_t nextCluster;
    //calculating the offset in the FAT for current cluster
    off_t fatOffset = volume->bootSector.BPB_RsvdSecCnt * volume->bootSector.BPB_BytsPerSec + currentCluster * sizeof(uint16_t);
    //seek position in the FAT
    if (lseek(volume->fd, fatOffset, SEEK_SET) < 0)
    {
        perror("Error seeking in FAT");
        return 0xFFFF; 
    }
    //read the next cluster from the FAT
    if (read(volume->fd, &nextCluster, sizeof(uint16_t)) != sizeof(uint16_t))
    {
        perror("Error reading from FAT");
        return 0xFFFF; 
    }
    return nextCluster;
}

ssize_t readSector(const Volume *volume, off_t sector, void *buffer)
{
    //Calculating the offset byte for the sector
    off_t offset = sector * volume->bootSector.BPB_BytsPerSec;
    //reading the sector data into buffer
    return readFromDiskImage(volume->fd, offset, buffer, volume->bootSector.BPB_BytsPerSec);
}

// function opens file given directory entry and volume
extern File *openFile(Volume *vol, DirectoryEntry *entry)
{
    File *file = malloc(sizeof(File));
    if (!file)
    {
        perror("Error allocating memory for this file");
        return NULL;
    }
    //fill struct
    file->currentCluster = ((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO;
    file->fileSize = entry->DIR_FileSize;
    file->filePosition = 0;
    file->volume = vol;
    return file;
}

//function that sets the file position int he open file
extern off_t seekFile(File *file, off_t offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET: //setting position relative to the start of the file
        file->filePosition = offset;
        break;
    case SEEK_CUR: //setting position relative to the current position
        file->filePosition += offset;
        break;
    case SEEK_END: //setting position relative to the end of the file
        file->filePosition = file->fileSize + offset;
        break;
    default:
        perror("Seek is invalid");
        return -1;
    }
    return file->filePosition;
}

// Read data from the file into a buffer
extern size_t readFile(File *file, void *buffer, size_t length)
{
    if (file->filePosition >= file->fileSize)
    {
        return 0; // end of file reached
    }

    size_t bytesRead = 0;
    uint8_t *buf = (uint8_t *)buffer;

    while (length > 0 && file->filePosition < file->fileSize)
    {
        off_t sector = clusterToSector(file->volume, file->currentCluster);
        off_t sectorOffset = file->filePosition % file->volume->bootSector.BPB_BytsPerSec;
        off_t clusterOffset = file->filePosition % (file->volume->bootSector.BPB_BytsPerSec * file->volume->bootSector.BPB_SecPerClus);
        size_t bytesToRead = file->volume->bootSector.BPB_BytsPerSec - sectorOffset;

        if (bytesToRead > length)
        {
            bytesToRead = length;
        }

        // Ensure not to read beyond the file size
        if (bytesRead + bytesToRead > file->fileSize)
        {
            bytesToRead = file->fileSize - bytesRead;
        }

        // Temporary buffer for reading a sector
        uint8_t tempBuf[file->volume->bootSector.BPB_BytsPerSec];
        if (readSector(file->volume, sector + (clusterOffset / file->volume->bootSector.BPB_BytsPerSec), tempBuf) < 0)
        {
            perror("Error reading sector");
            break;
        }

        // Copying the required part of the sector into the buffer
        memcpy(buf + bytesRead, tempBuf + sectorOffset, bytesToRead);

        bytesRead += bytesToRead;
        file->filePosition += bytesToRead;
        length -= bytesToRead;

        // Moving to the next cluster
        if (clusterOffset + bytesToRead >= file->volume->bootSector.BPB_BytsPerSec * file->volume->bootSector.BPB_SecPerClus)
        {
            file->currentCluster = nextCluster(file->volume, file->currentCluster);
        }
    }

    return bytesRead;
}

// closing file
extern void closeFile(File *file)
{
    free(file);
}

//task 7 trial

#if 0
//helper function for reading directory content into buffer
DirectoryEntry* readDirectoryContent(int fd, const BootSector* bs, uint16_t firstCluster, size_t* dirSize){
    off_t offset = clusterToSector(Volume, firstCluster) * bs->BPB_BytsPerSec; //calculatiung starting cluster of directory
    *dirSize = bs -> BPB_RootEntCnt * sizeof(DirectoryEntry); //calculating dir size

    DirectoryEntry* dir = malloc(*dirSize); //allocating memory of buffer

    if(!dir){
        perror("Error allocating memory for dir content");
        return NULL;
    }

    if (lseek(fd, offset, SEEK_SET)<0){
        perror("ERror seeking to dir");
        free(dir);
        return NULL;
    }

    if(read(fd, dir, *dirSize) != *dirSize){
        perror("Error reading dir");
        free(dir);
        return NULL;
    }

    return dir;
}

void fatNameToString(const uint8_t *fatName, char *str){
    int i,j;
    for (i=0, j=0; i<8 && fatName[i] != ' ';i++, j++){
        str[j] =fatName[i];
    }
    if (fatName[8] != ' '){
        str[j++] = '.';
        for (i=8;i<11 && fatName[i] != ' '; i++, j++){
            str[j] = fatName[i];
        }
    }
    str[j] = '\0';
}

///function to compare fat16 filename with c string
bool compareFatName(const uint8_t *fatName, const char *str){
    char temp [13];
    fatNameToString(fatName, temp);
    return strcmp(temp, str) == 0;
}

//function to find directory entry by name
DirectoryEntry * findDirectoryEntry (const Volume * volume, const DirectoryEntry *directory, size_t dirSize, const char *name){
    for (size_t i=0<dirSize/sizeof(DirectoryEntry);; i++){
        if(directory[i].DIR_Name[0] == 0x00){//no more entries
            break;
        }
        if(directory[i].DIR_Name[0] == 0xE5){//unusured entry
            continue;
        }
        if (!isLongNameEntry(&directory[i] && compareFatName(directory[i].DIR_Name, name))){
        return (DirectoryEntry *)&directory[i];
        }
    }
    return NULL;//entry not found
}


DirectoryEntry *followPath(Volume *volume, const char *path){
    if (path == NULL || strcmp(path,"/") == 0 ){
        //assume its the root directory, read
        size_t rootDirSize;
        return readDirectoryContent(volume->fd, &volume->bootSector, 0, &rootDirSize); //cluster is set to 0 for rootDir
    }

    char pathCopy[strlen(path)+1];
    strcpy(pathCopy, path);

    char *token = strtok(pathCopy, "/");
    size_t currentDirSize;
    DirectoryEntry* currentDir = readDirectoryContent(volume->fd, &volume->bootSector, 0, &currentDirSize); //reading the rootDirectory first

    while (token != NULL){
        DirectoryEntry *entry = findDirectoryEntry(volume, currentDir,currentDirSize, token);
        if(entry == NULL){
            free(currentDir);
            return NULL;
        }

        if (entry->DIR_Attr & 0x10){ //rmb 0x10 is attribute=directory
            //read the new dir data
            free(currentDir);//freeing the buffer
            currentDir=readDirectoryContent(volume->fd,&volume->bootSector, entry->DIR_FstClusLO, &currentDirSize);
            if (currentDir == NULL){
                perror("error reading new dir");
                return NULL;
            }
        } else if (strtok(NULL, "/") == NULL){// file AND the last token in the path
            DirectoryEntry* foundEntry = malloc(sizeof(DirectoryEntry));
            memcpy(foundEntry, entry, sizeof(DirectoryEntry));
            free(currentDir);
            return foundEntry;
        }
        token = strtok(NULL, "/");
    }
    free(currentDir);
    return NULL;
}

#endif

int main()
{
    setlocale(LC_ALL, "");
    printf("\n");
    // defining the path to the FAT file
    const char *filepath = "/home/laur1/h-drive/scc211/FAT16/fat16.img";

    // open FAT disk image
    int fileDesc = openDiskImage(filepath);
    if (fileDesc < 0)
    {
        return EXIT_FAILURE;
    }

    // Task 1: demonstrate data reading
    off_t offset = 0;
    size_t numBytes = 512;
    char *buffer = allocateBuffer(numBytes);
    if (!buffer)
    {
        closeDiskImage(fileDesc);
        return EXIT_FAILURE;
    }

    ssize_t bytesRead = readFromDiskImage(fileDesc, offset, buffer, numBytes);
    if (bytesRead < 0)
    {
        free(buffer);
        closeDiskImage(fileDesc);
        return EXIT_FAILURE;
    }

    // print the data read
    printf("Data read from offset %ld:\n", offset);
    for (size_t i = 0; i < bytesRead; i++)
    {
        printf(" %02x ", (unsigned char)buffer[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    free(buffer);

    // printing blanks for readability
    printf("\n");

    // read boot sector (task 2)
    BootSector bootSector = readBootSector(fileDesc);

    printBSInfo(&bootSector);

    // linebreak for readability
    printf("\n");

    // task 3 reading and printing cluster chain
    uint16_t startingCluster = 1304;

    // getting the cluster chain starting from startingCluster
    uint16_t *clusterChain = getClusterChain(fileDesc, &bootSector, startingCluster);
    if (!clusterChain)
    {
        closeDiskImage(fileDesc);
        return EXIT_FAILURE;
    }

    // printing the cluster numbers
    printf("Cluster Chain: \n");
    for (int i = 0; clusterChain[i] != 0xFFFF; i++)
    {
        printf("%u ", clusterChain[i]);
    }
    printf("\n");

    // linebreak for redability
    printf("\n");

    // free thje allocated cluster chain
    free(clusterChain);

    // task 4: reading and printing root directory
    readDirectory(fileDesc, &bootSector);

    // task 5: opening and printing file content

    // Prepare to read a file
    Volume volume = {.fd = fileDesc, .bootSector = bootSector};
    DirectoryEntry myFileDirEntry;
    // Filename
    memcpy(myFileDirEntry.DIR_Name, "SESSIONSTXT", 11);

    // Attributes
    myFileDirEntry.DIR_Attr = 0x20; // Archive

    // File size
    myFileDirEntry.DIR_FileSize = 5000;

    // First cluster
    myFileDirEntry.DIR_FstClusLO = 2457;
    myFileDirEntry.DIR_FstClusHI = 0; //leave as 0

    // Allocate a new buffer for reading file data
    size_t fileBufferLength = 5000; // Adjust as needed
    char *fileBuffer = malloc(fileBufferLength + 1);
    if (!fileBuffer)
    {
        perror("Error allocating read buffer");
        closeDiskImage(volume.fd);
        return EXIT_FAILURE;
    }

    // Open and read the file
    File *myFile = openFile(&volume, &myFileDirEntry);
    if (!myFile)
    {
        perror("Error opening file");
        free(fileBuffer);
        closeDiskImage(volume.fd);
        return EXIT_FAILURE;
    }

    size_t fileBytesRead = readFile(myFile, fileBuffer, fileBufferLength);
    printf("Bytes read: %zu\n", fileBytesRead);

    if (bytesRead > 0)
    {
        fileBuffer[bytesRead] = '\0'; // Null-terminate the string
        printf("File content:\n%s\n", fileBuffer);
    }
    else
    {
        printf("No data read from the file or end of file reached.\n");
    }


    //task 7  given a path with mix of file and directory names output the corresponding file
    //not sure what the brief means in terms of how the filepath is arragned.. /(rootdirectory)/(dir2?)
    //logic:
    //split the fileEntry path into components e.g. SESSIONSTXT and dir1 etc etc would be stored separately
    //use separated components to traverse the directorty struct and find the target file
    //use a function to search for a directory given the name component, use strcomp? but then we need to decode the fileName within the FAT file first


    const char *path = "/SESSIONSTXT/dir1";

    // closing the disk iamge
    closeDiskImage(fileDesc);

    return EXIT_SUCCESS;
}