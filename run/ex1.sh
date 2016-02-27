#!/bin/bash

# This test vector deals with the operations alloc / free inodes.
# It defines a storage device with 16 blocks and formats it with an inode table of 8 inodes.
# It starts by allocating all the inodes and testing several error conditions. Then, it frees all the
# allocated inodes in the reverse order of allocation while still testing different error conditions.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 16
./mkfs_sofs13_bin -n SOFS13 -i 8 -z myDisk
./testifuncs13 -b -l 600,700 -L testVector1.rst myDisk <testVector1.cmd
