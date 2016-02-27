#!/bin/bash

# This test vector deals mainly with operations get a directory entry by name, add / remove directory
# entries, rename directory entries and check a directory status of emptiness.
# It defines a storage device with 100 blocks and formats it with 72 inodes.
# It starts by allocating seven inodes, associated to regular files and directories, and organize them
# in a hierarchical faction. Then it proceeds by renaming some of the entries and ends by removing all
# of them.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs13_bin -n SOFS13 -i 72 -z myDisk
./testifuncs13 -b -l 300,700 -L testVector12.rst myDisk <testVector12.cmd
