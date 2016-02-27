#!/bin/bash

# This test vector deals mainly with operations alloc / free / clean data clusters through the operation handle file cluster which is always used.
# It defines a storage device with 100 blocks and formats it with 23 data clusters.
# It starts by allocating an inode, then it proceeds by allocating 13 data clusters in all the reference areas (direct, single indirect and
# double indirect). This means that in fact 20 data clusters are allocated.
# Then 3 data clusters are freed and the inode is also freed, leaving it in the dirty state.
# Afterwards, another inode is allocated and 4 data clusters are also allocated (2 in the direct area and 2 in the single indirect area),
# which means that all but one data clusters are presently allocated.
# The aim is to check if the operations are performed correctly even when the allocated data clusters are in the dirty state.
# The showblock_sofs13 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs13_bin -i 40 -z myDisk
./testifuncs13 -b -l 400,700 -L testVector8.rst myDisk <testVector8.cmd
