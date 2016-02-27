/**
 *  \file sofs_basicoper.c (implementation file)
 *
 *  \brief Set of operations to manage the file system internal data structures.
 *
 *
 *  The aim is to provide an unique storage location when the file system is in operation.
 *
 *  The operations are:
 *      \li load the contents of the superblock into internal storage
 *      \li get a pointer to the contents of the superblock
 *      \li store the contents of the superblock resident in internal storage to the storage device
 *      \li convert the inode number, which translates to an entry of the table of inodes, into the logical number (the
 *          ordinal, starting at zero, of the succession blocks that the table of inodes comprises) and the offset of
 *          the block where it is stored
 *      \li load the contents of a specific block of the table of inodes into internal storage
 *      \li get a pointer to the contents of a specific block of the table of inodes
 *      \li store the contents of the block of the table of inodes resident in internal storage to the storage device
 *      \li convert the reference of an entry to the table of cluster-to-inode mapping into the logic number and offset
 *          of the block where it is stored
 *      \li load the contents of a specific block of the table of cluster-to-inode mapping into internal storage
 *      \li get a pointer to the contents of a specific block of the table of cluster-to-inode mapping
 *      \li store the contents of the block of the table of cluster-to-inode mapping resident in internal storage to
 *          the storage device
 *      \li convert the reference to a data cluster, which translates to an entry of the bitmap table to free data clusters,
 *          into the logical number (the ordinal, starting at zero, of the succession blocks that the bitmap table to free
 *          data clusters comprises), the byte offset of the block where it is stored and the bit offset within the byte
 *      \li load the contents of a specific block of the bitmap table to free data clusters into internal storage
 *      \li get a pointer to the contents of a specific block of the bitmap table to free data clusters
 *      \li store the contents of the block of the bitmap table to free data clusters resident in internal storage
 *          to the storage device
 *      \li convert the logical number (the ordinal, starting at zero, of the succession blocks that the bitmap table to
 *          free data clusters comprises), the byte offset of the block where it is stored and the bit offset within the
 *          byte to a reference to a data cluster
 *      \li convert a byte position in the data continuum (information content) of a file, into the index of the element
 *          of the list of direct references and the offset within it where it is stored
 *      \li load the contents of a specific cluster of the table of single indirect references to data clusters into
 *          internal storage.
 *      \li get a pointer to the contents of a specific cluster of the table of single indirect references to data
 *          clusters
 *      \li store the contents of a specific cluster of the table of single indirect references to data clusters
 *          resident in internal storage to the storage device
 *      \li load the contents of a specific cluster of the table of direct references to data clusters into internal
 *          storage
 *      \li get a pointer to the contents of a specific cluster of the table of direct references to data clusters
 *      \li store the contents of a specific cluster of the table of direct references to data clusters resident in
 *          internal storage to the storage device.
 *
 *  \author António Rui Borges - August 2010 - September 2013
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#include "sofs_probe.h"
#include "sofs_const.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_direntry.h"

/*
 *  Internal data structure
 */

/** \brief Storage area for superblock */
static SOSuperBlock sb;
/** \brief area validation: -1 - an error has occurred while reading or writing superblock data
 *                           0 - superblock data has not been read yet
 *                           1 - superblock data has already been read
 */
static int sbLoaded = 0;
/** \brief status of reading or writing superblock data */
static int sbError = 0;

/** \brief storage area for one block of the table of inodes */
static SOInode inode[IPB];
/** \brief validation area: -2 - an error occurred while reading or writing a data block
 *                          -1 - no block of the table of inodes has been read yet
 *                           * - logical block number of table of inodes that has been read
 */
static int nBlkInTLoaded = -1;
/** \brief status of reading or writing a data block of the table of inodes */
static int intError = 0;

/** \brief storage area for a block of the table of cluster-to-inode mapping */
static uint32_t blockCTInMT[RPB];
/** \brief validation area: -2 - an error occurred while reading or writing a data block
 *                          -1 - no block of the table of cluster-to-inode mapping has been read yet
 *                           * - logic block number of table of cluster-to-inode mapping that has been read
 */
static int nBlkCTInMTLoaded = -1;
/** \brief status of reading or writing a data block of the table of cluster-to-inode mapping */
static int ctinmtError = 0;

/** \brief storage area for one block of the bitmap table to free data clusters */
static unsigned char bMap[BLOCK_SIZE];
/** \brief validation area: -2 - an error occurred while reading or writing a data block
 *                          -1 - no block of the bitmap table to free data clusters has been read yet
 *                           * - logical block number of the bitmap table to free data clusters that has been read
 */
static int nBlkBMapTLoaded = -1;
/** \brief status of reading or writing a data block of the bitmap table to free data clusters */
static int bmaptError = 0;

/** \brief storage area for a cluster of single indirect references to data clusters */
static SODataClust sngIndRefClust;
/** \brief validation area: -2 - an error occurred while reading or writing a data cluster
 *                          -1 - no cluster of single indirect references to data clusters has been read yet
 *                           * - physical cluster number of single indirect references to data clusters that has been
 *                               read
 */
static int nClustSIRef = 0;
/** \brief status of reading or writing a cluster of single indirect references to data clusters */
static int sircError = 0;

/** \brief storage area for a cluster of direct references to data clusters */
static SODataClust dirRefClust;
/** \brief validation area: -2 - an error occurred while reading or writing a data cluster
 *                          -1 - no cluster of direct references to data clusters has been read yet
 *                           * - physical cluster number of direct references to data clusters that has been read
 */
static int nClustDRef = 0;
/** \brief status of reading or writing a cluster of direct references to data clusters */
static int drcError = 0;

/**
 *  \brief Load the contents of the superblock into internal storage.
 *
 *  Any type of previous error on loading / storing the superblock data will disable the operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on a previous
 *                       store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadSuperBlock (void)
{
  soColorProbe (711, "07;31", "soLoadSuperBlock ()\n");

  int stat;                                      /* status of operation */

  if (sbError != 0) return sbError;              /* a previous error has occurred */
  if (sbLoaded == 1) return 0;                   /* superblock has already been read */
  stat = soReadCacheBlock (0, &sb);
  if (stat == 0)
     sbLoaded = 1;                               /* operation carried out with success */
     else { sbLoaded = -1;
            sbError = stat;                      /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of the superblock.
 *
 *  Any type of previous error on loading / storing the superblock data will disable the operation.
 *
 *  \return pointer to the superblock , on success
 *  \return -\c NULL, if the superblock was not previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

SOSuperBlock *soGetSuperBlock (void)
{
  soColorProbe (712, "07;31", "soGetSuperBlock ()\n");

  if (sbLoaded == 1)
     return &sb;
     else return NULL;
}

/**
 *  \brief Store the contents of the superblock resident in internal storage to the storage device.
 *
 *  Any type of previous / current error on loading / storing the superblock data will disable the operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on the
 *                       current or a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreSuperBlock (void)
{
  soColorProbe (713, "07;31", "soStoreSuperBlock ()\n");

  int stat;                                      /* status of operation */

  if (sbError != 0) return sbError;              /* a previous error has occurred */
  if (sbLoaded == 0)
     { sbLoaded = -1;
       sbError = -ELIBBAD;                       /* superblock has not been read yet */
       return sbError;
     }
  stat = soWriteCacheBlock (0, &sb);
  if (stat != 0)
     { sbLoaded = -1;
       sbError = stat;                           /* an error has occurred while writing */
     }

  return stat;
}

/**
 *  \brief Convert the inode number, which translates to an entry of the inode table, into the logical number (the
 *         ordinal, starting at zero, of the succession blocks that the table of inodes comprises) and the offset of
 *         the block where it is stored.
 *
 *  \param nInode inode number
 *  \param p_nBlk pointer to the location where the logical block number is to be stored
 *  \param p_offset pointer to the location where the offset is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if inode number is out of range or any of the pointers is \c NULL
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on a previous
 *                       store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soConvertRefInT (uint32_t nInode, uint32_t *p_nBlk, uint32_t *p_offset)
{
  soColorProbe (714, "07;31", "soConvertRefInT (%"PRIu32", %p, %p)\n", nInode, p_nBlk, p_offset);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((nInode >= sb.itotal) || (p_nBlk == NULL) || (p_offset == NULL))
     return -EINVAL;

  *p_nBlk = nInode / IPB;
  *p_offset = nInode % IPB;

  return 0;
}

/**
 *  \brief Load the contents of a specific block of the table of inodes into internal storage.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \param nBlk logical number of the block to be read
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if logical block number is out of range
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadBlockInT (uint32_t nBlk)
{
  soColorProbe (715, "07;31", "soLoadBlockInT (%"PRIu32")\n", nBlk);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if (nBlk >= sb.itable_size) return -EINVAL;

  if (intError != 0) return intError;            /* a previous error has occurred */
  if (nBlk == nBlkInTLoaded) return 0;           /* the block has already been read */
  stat = soReadCacheBlock (sb.itable_start + nBlk, inode);
  if (stat == 0)
     nBlkInTLoaded = nBlk;                       /* operation carried out with success */
     else { nBlkInTLoaded = -1;
            intError = stat;                     /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of a specific block of the table of inodes.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

SOInode *soGetBlockInT (void)
{
  soColorProbe (716, "07;31", "soGetBlockInT ()\n");

  if (nBlkInTLoaded >= 0)
     return inode;
     else return NULL;
}

/**
 *  \brief Store the contents of the block of the table of inodes resident in internal storage to the storage device.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreBlockInT (void)
{
  soColorProbe (717, "07;31", "soStoreBlockInT ()\n");

  int stat;                                      /* status of operation */

  if (intError != 0) return intError;            /* a previous error has occurred */
  if (nBlkInTLoaded < 0)
     { nBlkInTLoaded = -2;
       intError = -ELIBBAD;                      /* no block of the bitmap table of inodes has not been
                                                    read yet */
       return intError;
     }
  stat = soWriteCacheBlock (sb.itable_start + nBlkInTLoaded, inode);
  if (stat != 0)
     { nBlkInTLoaded = -2;
       intError = stat;                          /* an error has occurred while writing */
     }

  return stat;
}

/**
 *  \brief Convert the reference of an entry to the table of cluster-to-inode mapping into the logic number and offset
 *         of the block where it is stored.
 *
 *  The reference is supposed to be logical:
 *
 *                    ref = (nClust - dzone_start) div BLOCKS_PER_CLUSTER
 *
 *  where nClust stands for physical cluster number (or physical number of first block of the cluster).
 *
 *  \param ref reference of an entry of the table of cluster-to-inode mapping
 *  \param p_nBlk pointer to the location where the logic block number is to be stored
 *  \param p_offset pointer to the location where the offset is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if ref is out of range or any of the pointers is \c NULL
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on a previous
 *                       store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soConvertRefCInMT (uint32_t ref, uint32_t *p_nBlk, uint32_t *p_offset)
{
  soColorProbe (718, "07;31", "soConvertRefCInMT (%"PRIu32", %p, %p)\n", ref, p_nBlk, p_offset);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((ref >= sb.dzone_total) || (p_nBlk == NULL) || (p_offset == NULL))
     return -EINVAL;

  *p_nBlk = ref / RPB;
  *p_offset = ref % RPB;

  return 0;
}

/**
 *  \brief Load the contents of a specific block of the table of cluster-to-inode mapping into internal storage.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \param nBlk logic number of the block to be read
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if logic block number is out of range
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadBlockCTInMT (uint32_t nBlk)
{
  soColorProbe (719, "07;31", "soLoadBlockCTInMT (%"PRIu32")\n", nBlk);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if (nBlk >= sb.ciutable_size) return -EINVAL;

  if (ctinmtError != 0) return ctinmtError;      /* a previous error has occurred */
  if (nBlk == nBlkCTInMTLoaded) return 0;        /* the block has already been read */
  stat = soReadCacheBlock (sb.ciutable_start + nBlk, blockCTInMT);
  if (stat == 0)
     nBlkCTInMTLoaded = nBlk;                    /* operation carried out with success */
     else { nBlkCTInMTLoaded = -2;
            ctinmtError = stat;                  /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of a specific block of the table of cluster-to-inode mapping.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

uint32_t *soGetBlockCTInMT (void)
{
  soColorProbe (720, "07;31", "soGetBlockCTInMT ()\n");

  if (nBlkCTInMTLoaded >= 0)
     return blockCTInMT;
     else return NULL;
}

/**
 *  \brief Store the contents of the block of the table of cluster-to-inode mapping resident in internal storage to the
 *         storage device.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or no block of the table of cluster-to-inode mapping was
 *              previously loaded
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreBlockCTInMT (void)
{
  soColorProbe (721, "07;31", "soStoreBlockCTInMT ()\n");

  int stat;                                      /* status of operation */

  if (ctinmtError != 0) return ctinmtError;      /* a previous error has occurred */
  if (nBlkCTInMTLoaded < 0)
     { nBlkCTInMTLoaded = -2;
       ctinmtError = -ELIBBAD;                   /* no block of the table of cluster-to-inode mapping has not been
                                                    read yet */
       return ctinmtError;
     }
  stat = soWriteCacheBlock (sb.ciutable_start + nBlkCTInMTLoaded, blockCTInMT);
  if (stat != 0)
     { nBlkCTInMTLoaded = -2;
       ctinmtError = stat;                       /* an error has occurred while writing */
     }

  return stat;
}

/**
 *  \brief Convert the reference to a data cluster, which translates to an entry of the bitmap table to free data clusters,
 *         into the logical number (the ordinal, starting at zero, of the succession blocks that the bitmap table to free
 *         data clusters comprises), the byte offset of the block where it is stored and the bit offset within the byte.
 *
 *  \param ref index to the bitmap table to free data clusters
 *  \param p_nBlk pointer to the location where the logical block number is to be stored
 *  \param p_byteOff pointer to the location where the byte offset is to be stored
 *  \param p_bitOff pointer to the location where the bit offset is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if nRef is out of range or any of the pointers is \c NULL
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on a previous
 *                       store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soConvertRefBMapT (uint32_t ref, uint32_t *p_nBlk, uint32_t *p_byteOff, uint32_t *p_bitOff)
{
  soColorProbe (722, "07;31", "soConvertRefBMapT (%"PRIu32", %p, %p, %p)\n", ref, p_nBlk, p_byteOff, p_bitOff);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((ref >= sb.dzone_total) || (p_nBlk == NULL) || (p_byteOff == NULL) || (p_bitOff == NULL))
     return -EINVAL;

  *p_nBlk = ref / (8 * BLOCK_SIZE);
  *p_byteOff = (ref % (8 * BLOCK_SIZE)) / 8;
  *p_bitOff = (ref % (8 * BLOCK_SIZE)) % 8;

  return 0;
}

/**
 *  \brief Load the contents of a specific block of the bitmap table to free data clusters into internal storage.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \param nBlk logical number of the block to be read
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if logical block number is out of range
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadBlockBMapT (uint32_t nBlk)
{
  soColorProbe (723, "07;31", "soLoadBlockBMapT (%"PRIu32")\n", nBlk);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if (nBlk >= sb.fctable_size) return -EINVAL;

  if (bmaptError != 0) return bmaptError;        /* a previous error has occurred */
  if (nBlk == nBlkBMapTLoaded) return 0;         /* the block has already been read */
  stat = soReadCacheBlock (sb.fctable_start + nBlk, bMap);
  if (stat == 0)
     nBlkBMapTLoaded = nBlk;                     /* operation carried out with success */
     else { nBlkBMapTLoaded = -1;
            bmaptError = stat;                   /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of a specific block of the bitmap table to free data clusters.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

unsigned char *soGetBlockBMapT (void)
{
  soColorProbe (724, "07;31", "soGetBlockBMapT ()\n");

  if (nBlkBMapTLoaded >= 0)
     return bMap;
     else return NULL;
}

/**
 *  \brief Store the contents of the block of the bitmap table to free data clusters resident in internal storage
 *         to the storage device.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreBlockBMapT (void)
{
  soColorProbe (725, "07;31", "soStoreBlockBMapT ()\n");

  int stat;                                      /* status of operation */

  if (bmaptError != 0) return bmaptError;        /* a previous error has occurred */
  if (nBlkBMapTLoaded < 0)
     { nBlkBMapTLoaded = -2;
       bmaptError = -ELIBBAD;                    /* no block of the bitmap table of inodes has not been
                                                    read yet */
       return bmaptError;
     }
  stat = soWriteCacheBlock (sb.fctable_start + nBlkBMapTLoaded, bMap);
  if (stat != 0)
     { nBlkBMapTLoaded = -2;
       bmaptError = stat;                        /* an error has occurred while writing */
     }

  return stat;
}

/**
 *  \brief Convert the logical number (the ordinal, starting at zero, of the succession blocks that the bitmap table to
 *         free data clusters comprises), the byte offset of the block where it is stored and the bit offset within the
 *         byte to a reference to a data cluster.
 *
 *  \param nBlk logical block number of the bitmap table to free data clusters
 *  \param byteOff byte offset of the block where it is stored
 *  \param bitOff bit offset within the byte
 *  \param p_ref pointer to the location where the reference to the data cluster is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if nBlk, byteOff or bitOff are out of range or the pointer is \c NULL
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock was not previously loaded on a previous
 *                       store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soConvertBMapTRef (uint32_t nBlk, uint32_t byteOff, uint32_t bitOff, uint32_t *p_ref)
{
  soColorProbe (726, "07;31", "soConvertBMapTRef (%"PRIu32", %"PRIu32", %"PRIu32", %p)\n", nBlk, byteOff, bitOff, p_ref);

  int stat;                                      /* status of operation */
  uint32_t tmp;                                  /* temporary storage for computed value */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((nBlk >= sb.fctable_size) || (byteOff >= BLOCK_SIZE) || (bitOff >= 8) || (p_ref == NULL))
     return -EINVAL;

  tmp = (BLOCK_SIZE * nBlk + byteOff) * 8 + bitOff;
  if (tmp >= sb.dzone_total) return -EINVAL;
  *p_ref = tmp;

  return 0;
}

/**
 *  \brief Convert a byte position in the data continuum (information content) of a file, into the index of the element
 *         of the list of direct references and the offset within it where it is stored.
 *
 *  The byte position is defined as:
 *
 *                    p = clustInd * BSLPC + offset
 *
 *  where \e clustInd stands for the index of the element of the list of direct references and \e offset for the
 *           byte position within it.
 *
 *  \param p byte position in the data continuum
 *  \param p_clustInd pointer to the location where the index to the list of direct references is to be stored
 *  \param p_offset pointer to the location where the offset is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if byte position is out of range or any of the pointers is \c NULL
 */

int soConvertBPIDC (uint32_t p, uint32_t *p_clustInd, uint32_t *p_offset)
{
  soColorProbe (727, "07;31", "soConvertBPIDC (%"PRIu32", %p, %p)\n", p, p_clustInd, p_offset);

  if ((p >= MAX_FILE_SIZE) || (p_clustInd == NULL) || (p_offset == NULL))
     return -EINVAL;

  *p_clustInd = p / BSLPC;
  *p_offset = p % BSLPC;

  return 0;
}

/**
 *  \brief Load the contents of a specific cluster of the table of single indirect references to data clusters into
 *         internal storage.
 *
 *  Any type of previous / current error on loading / storing a single indirect references cluster will disable the
 *  operation.
 *
 *  \param nClust physical number of the cluster to be read
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if physical cluster number is out of range
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadSngIndRefClust (uint32_t nClust)
{
  soColorProbe (728, "07;31", "soLoadSngIndRefClust (%"PRIu32")\n", nClust);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((nClust < sb.dzone_start) || (((nClust - sb.dzone_start) % BLOCKS_PER_CLUSTER) != 0) ||
      (nClust >= (sb.dzone_start + sb.dzone_total * BLOCKS_PER_CLUSTER)))
     return -EINVAL;

  if (sircError != 0) return sircError;          /* a previous error has occurred */
  if (nClust == nClustSIRef) return 0;           /* the cluster has already been read */
  stat = soReadCacheCluster (nClust, &sngIndRefClust);
  if (stat == 0)
     nClustSIRef = nClust;                       /* operation carried out with success */
     else { nClustSIRef = -2;
            sircError = stat;                    /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of a specific cluster of the table of single indirect references to data
 *         clusters.
 *
 *  Any type of previous / current error on loading / storing a single indirect references cluster will disable the
 *  operation.
 *
 *  \return pointer to the specific cluster , on success
 *  \return -\c NULL, if no cluster was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

SODataClust *soGetSngIndRefClust (void)
{
  soColorProbe (729, "07;31", "soGetSngIndRefClust ()\n");

  if (nClustSIRef >= 0)
     return &sngIndRefClust;
     else return NULL;
}

/**
 *  \brief Store the contents of a specific cluster of the table of single indirect references to data clusters resident
 *         in internal storage to the storage device.
 *
 *  Any type of previous / current error on loading / storing a single indirect references cluster will disable the
 *  operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or no cluster of the table of single indirect references was
 *              previously loaded
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreSngIndRefClust (void)
{
  soColorProbe (730, "07;31", "soStoreSngIndRefClust ()\n");

  int stat;                                      /* status of operation */

  if (sircError != 0) return sircError;          /* a previous error has occurred */
  if (nClustSIRef < 0)
     { nClustSIRef = -2;
       sircError = -ELIBBAD;                     /* no cluster of the table of single indirect references has not been
                                                    read yet */
       return sircError;
     }
  stat = soWriteCacheCluster (nClustSIRef, &sngIndRefClust);
  if (stat != 0)
     { nClustSIRef = -2;
       sircError = stat;                          /* an error has occurred while writing */
     }

  return stat;
}

/**
 *  \brief Load the contents of a specific cluster of the table of direct references to data clusters into internal
 *         storage.
 *
 *  Any type of previous / current error on loading / storing a direct references cluster will disable the
 *  operation.
 *
 *  \param nClust physical number of the cluster to be read
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if physical cluster number is out of range
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on reading or writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or the superblock or a data block was not previously loaded
 *                       on a previous store operation
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLoadDirRefClust (uint32_t nClust)
{
  soColorProbe (731, "07;31", "soLoadDirRefClust (%"PRIu32")\n", nClust);

  int stat;                                      /* status of operation */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  if ((nClust < sb.dzone_start) || (((nClust - sb.dzone_start) % BLOCKS_PER_CLUSTER) != 0) ||
      (nClust >= (sb.dzone_start + sb.dzone_total * BLOCKS_PER_CLUSTER)))
     return -EINVAL;

  if (drcError != 0) return drcError;            /* a previous error has occurred */
  if (nClust == nClustDRef) return 0;            /* the cluster has already been read */
  stat = soReadCacheCluster (nClust, &dirRefClust);
  if (stat == 0)
	  nClustDRef = nClust;                       /* operation carried out with success */
     else { nClustDRef = -2;
            drcError = stat;                     /* an error has occurred while reading */
          }

  return stat;
}

/**
 *  \brief Get a pointer to the contents of a specific cluster of the table of direct references to data clusters.
 *
 *  Any type of previous / current error on loading / storing a direct references cluster will disable the
 *  operation.
 *
 *  \return pointer to the specific cluster , on success
 *  \return -\c NULL, if no cluster was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

SODataClust *soGetDirRefClust (void)
{
  soColorProbe (732, "07;31", "soGetDirRefClust ()\n");

  if (nClustDRef >= 0)
     return &dirRefClust;
     else return NULL;
}

/**
 *  \brief Store the contents of a specific cluster of the table of direct references to data clusters resident
 *         in internal storage to the storage device.
 *
 *  Any type of previous / current error on loading / storing a direct references cluster will disable the
 *  operation.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the buffercache is inconsistent or no cluster of the table of direct references was
 *              previously loaded
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soStoreDirRefClust (void)
{
  soColorProbe (733, "07;31", "soStoreDirRefClust ()\n");

  int stat;                                      /* status of operation */

  if (drcError != 0) return drcError;            /* a previous error has occurred */
  if (nClustDRef < 0)
     { nClustDRef = -2;
       drcError = -ELIBBAD;                      /* no cluster of the table of direct references has not been
                                                    read yet */
       return sircError;
     }
  stat = soWriteCacheCluster (nClustDRef, &dirRefClust);
  if (stat != 0)
     { nClustDRef = -2;
       drcError = stat;                          /* an error has occurred while writing */
     }

  return stat;
}
