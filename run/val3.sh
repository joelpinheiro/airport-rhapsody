#!/bin/bash

# This test vector checks if a large pdf file can be copied to the root directory.
# Basic system calls involved: readdir, mknode, read and write.

echo -e '\n**** Creating the storage device.****\n'
./createEmptyFile myDisk 1000
echo -e '\n**** Converting the storage device into a SOFS13 file system.****\n'
./mkfs_sofs13 -i 56 -z myDisk
echo -e '\n**** Mounting the storage device as a SOFS13 file system.****\n'
./mount_sofs13 myDisk mnt
echo -e '\n**** Copying the text file.****\n'
cp "SO - manipulação de entradas de directórios.pdf" mnt
echo -e '\n**** Listing the root directory.****\n'
ls -la mnt
echo -e '\n**** Checking if the file was copied correctly.****\n'
diff "SO - manipulação de entradas de directórios.pdf" "mnt/SO - manipulação de entradas de directórios.pdf"
echo -e '\n**** Getting the file attributes.****\n'
stat mnt/"SO - manipulação de entradas de directórios.pdf"
echo -e '\n**** Unmounting the storage device.****\n'
sleep 1
fusermount -u mnt
