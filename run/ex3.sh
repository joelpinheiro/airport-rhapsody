#!/bin/bash

# This test vector deals with the operations alloc / free data clusters.
# It defines a storage device with 20 blocks and formats it with an inode table of 8 inodes.
# It starts by allocating all the data clusters and testing the error condition. Then, it frees
# all the allocated data clusters in the reverse order of allocation while still testing different
# error conditions.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 20
./mkfs_sofs13_bin -n SOFS13 -i 8 -z myDisk
./testifuncs13 -b -l 600,700 -L testVector3.rst myDisk <testVector3.cmd
