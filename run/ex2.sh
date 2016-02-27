#!/bin/bash

# This test vector deals with the operations alloc / free inodes.
# It defines a storage device with 18 blocks and formats it with an inode table of 24 inodes.
# It starts by allocating successive inodes until there are no more inodes. Then, it frees all
# the allocated inodes in a sequence where an inode stored in a different block of the table is
# taken in succession. Finally, it procedes to allocate two inodes.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 18
./mkfs_sofs13_bin -n SOFS13 -i 24 -z myDisk
./testifuncs13 -b -l 600,700 -L testVector2.rst myDisk <testVector2.cmd
