#!/bin/bash

# This test vector deals mainly with operations write data clusters and clean inode.
# It defines a storage device with 1001 blocks and formats it with a 8 inode table and 249 data clusters.
# It starts by allocating an inode, then it proceeds by allocating 60 data clusters in all the reference areas (direct, single indirect and
# double indirect). This means that in fact 70 data clusters are allocated.
# Then all the data clusters are freed and and 7 inodes are allocated.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 1001
./mkfs_sofs13_bin -i 8 -z myDisk
./testifuncs13 -b -l 400,700 -L testVector11.rst myDisk <testVector11.cmd
