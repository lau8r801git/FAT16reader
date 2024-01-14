# FAT16reader
Overview:

This C program provides functionality to interact with a FAT16 file system image. It includes operations like reading the boot sector, directory entries, files, and handling long filenames. The code is structured around various data structures and functions to manipulate FAT16 file system entities.

Dependencies:

Standard C libraries: stdio.h, stdlib.h, string.h, time.h, stdint.h, fcntl.h, unistd.h, sys/types.h, sys/stat.h, stdbool.h, wchar.h, locale.h

Data Structures:

BootSector: Represents the boot sector of a FAT16 file system.

DirectoryEntry: Represents a directory entry in a FAT16 file system.

Volume: Represents a volume, encapsulating a file descriptor and a BootSector.

File: Represents an open file, including details like size, position, and cluster information.

LongDirectoryEntry: Represents a long directory entry for handling long filenames.

Key Functions:

openDiskImage: Opens a disk image and returns the file descriptor.

readFromDiskImage: Reads a specific number of bytes from a given offset in the disk image.

closeDiskImage: Closes the disk image.

readBootSector: Reads and returns the boot sector of the disk image.

allocateBuffer: Allocates and returns a buffer of a specified size.

printBSInfo: Prints information from the boot sector.

loadFAT: Loads the file allocation table from memory.

getClusterChain: Builds a chain of clusters given a specified cluster.

printDirectoryEntry: Prints the details of a directory entry.

readDirectory: Reads and prints entries from a root directory.

clusterToSector: Converts a cluster number to a corresponding sector number.

nextCluster: Retrieves the next cluster in the chain from the FAT.

readSector: Reads a sector from the volume.

openFile: Opens a file given a directory entry and volume.

seekFile: Sets the file position in the open file.

readFile: Reads data from the file into a buffer.

closeFile: Closes a file.

Usage Example:

Opening and reading from a disk image:

const char *filepath = "<path_to_fat16_img>";
int fileDesc = openDiskImage(filepath);

Reading and printing the boot sector:

BootSector bootSector = readBootSector(fileDesc);
printBSInfo(&bootSector);

Reading a directory and its entries:

readDirectory(fileDesc, &bootSector);
Handling files:

Opening a file:

DirectoryEntry myFileDirEntry;
// Set up myFileDirEntry details...
File *myFile = openFile(&volume, &myFileDirEntry);

Reading from a file:

char *fileBuffer = allocateBuffer(fileBufferLength);
size_t fileBytesRead = readFile(myFile, fileBuffer, fileBufferLength);

Closing a file:

closeFile(myFile);

Closing the disk image:

closeDiskImage(fileDesc);

Notes
The program is designed to handle FAT16 file system images.
It is crucial to correctly set the file path to the FAT16 image.
Memory allocation is used extensively; ensure to free the allocated memory after use.
The code includes handling of long file names in the FAT16 file system.
Error checking is implemented for file operations to handle potential issues during runtime.
