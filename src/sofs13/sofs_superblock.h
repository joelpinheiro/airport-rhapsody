/**
 *  \file sofs_superblock.h (interface file)
 *
 *  \brief Definition of the superblock data type.
 *
 *  It specifies the file system metadata which describes its internal architecture.
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author Miguel Oliveira e Silva - September 2009
 *  \author António Rui Borges - September 2010 / September 2013
 */

#ifndef SOFS_SUPERBLOCK_H_
#define SOFS_SUPERBLOCK_H_

#include <stdint.h>

#include "sofs_const.h"

/** \brief sofs13 magic number */
#define MAGIC_NUMBER (0x65FE)

/** \brief sofs13 version number */
#define VERSION_NUMBER (0x2013)

/** \brief maximum length + 1 of volume name */
#define PARTITION_NAME_SIZE (23)

/** \brief constant signaling the file system was properly unmounted the last time it was mounted */
#define PRU 0

/** \brief constant signaling the file system was not properly unmounted the last time it was mounted */
#define NPRU 1

/** \brief reference to a null data block */
#define NULL_BLOCK ((uint32_t)(~0UL))

/** \brief size of cache */
#define DZONE_CACHE_SIZE  (50)

/**
 *  \brief Definition of the reference cache data type.
 *
 *  It describes an easy access temporary storage area within the superblock for references to free data clusters.
 */

struct fCNode
    {
       /** \brief index of the first filled/free array element */
        uint32_t cache_idx;
       /** \brief storage area whose elements are the logical numbers of free data clusters */
        uint32_t cache[DZONE_CACHE_SIZE];
    };

/**
 *  \brief Definition of the superblock data type.
 *
 *  It contains global information about the file system layout, namely the size and the location of the remaining
 *  parts.
 *
 *  It is divided in:
 *     \li <em>header</em> - that contains data about the type, the version, the name, the size in number of physical
 *         blocks and the consistency status
 *     \li <em>inode table metadata</em> - concerning its location, size in number of blocks, total number of inodes and
 *         number of free inodes; the inode table is primarily conceived as an array of inodes, however, the free inodes
 *         form a double-linked circular list whose insertion and retrieval points are also provided (dynamic linear FIFO
 *         that links together all the free inodes, using the inodes themselves as nodes)
 *     \li <em>cluster-to-inode mapping table</em> - concerning its location and size in number of blocks; the cluster-to-
 *         inode mapping table is conceived as an array of references to inodes which expresses the connection of each
 *         data cluster, when allocated, to the file object it belongs to; it is instrumental to the recovery of previous
 *         deleted files and to enhance the determination of consistency of the file system and to provide means for its
 *         repairing
 *     \li <em>data zone metadata</em> - concerning its location, size in total number of data clusters, number of free
 *         data clusters and its three main reference components: the insertion and retrieval caches for a fast temporary
 *         storage of references (static structures resident within the superblock itself) and the location, size in number
 *         of blocks and search point index of the bitmap table to free data clusters, organized as an array of bits each
 *         pointing to the status of allocation of the corresponding data cluster (the search for a free data cluster should
 *         proceed in a circular way always starting at the search point index).
 */

typedef struct soSuperBlock
{
  /* Header */

   /** \brief magic number - file system identification number (should be MAGIC_NUMBER macro value) */
    uint32_t magic;
   /** \brief version number (should be VERSION_NUMBER macro value) */
    uint32_t version;
   /** \brief volume name */
    unsigned char name[PARTITION_NAME_SIZE+1];
   /** \brief total number of blocks in the device */
    uint32_t ntotal;
   /** \brief flag signaling if the file system was properly unmounted the last time it was mounted
    *     \li PRU - if properly unmounted
    *     \li NPRU - if not properly unmounted
    */
    uint32_t mstat;

  /* Inode table metadata */

   /** \brief physical number of the block where the table of inodes starts */
    uint32_t itable_start;
   /** \brief number of blocks that the table of inodes comprises */
    uint32_t itable_size;
   /** \brief total number of inodes */
    uint32_t itotal;
   /** \brief number of free inodes */
    uint32_t ifree;
   /** \brief index of the array element that forms the head of the double-linked list of free inodes
    *        (point of retrieval) */
    uint32_t ihead;
   /** \brief index of the array element that forms the tail of the double-linked list of free inodes
    *        (point of insertion) */
    uint32_t itail;

  /* Cluster-to-inode mapping table metadata (advanced feature) */

   /** \brief number of the first block of the table of cluster-to-inode mapping (each element represents the status of
    *         connection of the corresponding data cluster
    *         \li \c NULL_INODE - the data cluster is free
    *         \li <em>inode number of the file object it belongs to</em> - the data cluster is allocated) */
    uint32_t ciutable_start;
   /** \brief number of blocks of the table of cluster-to-inode mapping */
    uint32_t ciutable_size;

  /* Data zone metadata */

   /** \brief retrieval cache of references (or logical numbers) to free data clusters */
    struct fCNode dzone_retriev;
   /** \brief insertion cache of references (or logical numbers) to free data clusters */
    struct fCNode dzone_insert;
   /** \brief physical number of the block where the bitmap table to free data clusters starts (each bit represents the
    *         status of the corresponding data cluster
    *         \li 1 - the data cluster is free
    *         \li 0 - the data cluster has been allocated or its reference is in one of the caches) */
    uint32_t fctable_start;
   /** \brief number of blocks that the bitmap table to free data clusters comprises */
    uint32_t fctable_size;
   /** \brief search point index for the bitmap table envisaged as an array of bits (circular parsing) */
    uint32_t fctable_pos;
   /** \brief physical number of the block where the data zone starts (physical number of the first data cluster) */
    uint32_t dzone_start;
   /** \brief total number of data clusters */
    uint32_t dzone_total;
   /** \brief number of free data clusters */
    uint32_t dzone_free;

  /* Padded area to ensure superblock structure is BLOCK_SIZE bytes long */

   /** \brief reserved area */
    unsigned char reserved[BLOCK_SIZE - PARTITION_NAME_SIZE - 1 - 18 * sizeof(uint32_t) - 2 * sizeof(struct fCNode)];
} SOSuperBlock;

#endif /* SOFS_SUPERBLOCK_H_ */
