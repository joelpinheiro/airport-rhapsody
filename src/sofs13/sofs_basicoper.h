/**
 *  \file sofs_basicoper.h (interface file)
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
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           that better represents the error cause.
 *           (execute command <em>man errno</em> to get the list of system errors)
 */

#ifndef SOFS_BASICOPER_H_
#define SOFS_BASICOPER_H_

#include <stdint.h>

#include "sofs_superblock.h"
#include "sofs_inode.h"

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

extern int soLoadSuperBlock (void);

/**
 *  \brief Get a pointer to the contents of the superblock.
 *
 *  Any type of previous error on loading / storing the superblock data will disable the operation.
 *
 *  \return pointer to the superblock , on success
 *  \return -\c NULL, if the superblock was not previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

extern SOSuperBlock *soGetSuperBlock (void);

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

extern int soStoreSuperBlock (void);

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

extern int soConvertRefInT (uint32_t nInode, uint32_t *p_nBlk, uint32_t *p_offset);

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

extern int soLoadBlockInT (uint32_t nBlk);

/**
 *  \brief Get a pointer to the contents of a specific block of the table of inodes.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

extern SOInode *soGetBlockInT (void);

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

extern int soStoreBlockInT (void);

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

extern int soConvertRefCInMT (uint32_t ref, uint32_t *p_nBlk, uint32_t *p_offset);

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

extern int soLoadBlockCTInMT (uint32_t nBlk);

/**
 *  \brief Get a pointer to the contents of a specific block of the table of cluster-to-inode mapping.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

extern uint32_t *soGetBlockCTInMT (void);

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

extern int soStoreBlockCTInMT (void);

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

extern int soConvertRefBMapT (uint32_t ref, uint32_t *p_nBlk, uint32_t *p_byteOff, uint32_t *p_bitOff);

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

extern int soLoadBlockBMapT (uint32_t nBlk);

/**
 *  \brief Get a pointer to the contents of a specific block of the bitmap table to free data clusters.
 *
 *  Any type of previous / current error on loading / storing the data block will disable the operation.
 *
 *  \return pointer to the specific block , on success
 *  \return -\c NULL, if no data block was previously loaded or an error on a previous load / store operation has
 *                    occurred
 */

extern unsigned char *soGetBlockBMapT (void);

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

extern int soStoreBlockBMapT (void);

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

extern int soConvertBMapTRef (uint32_t nBlk, uint32_t byteOff, uint32_t bitOff, uint32_t *p_ref);

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

extern int soConvertBPIDC (uint32_t p, uint32_t *p_clustInd, uint32_t *p_offset);

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

extern int soLoadSngIndRefClust (uint32_t nClust);

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

extern SODataClust *soGetSngIndRefClust (void);

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

extern int soStoreSngIndRefClust (void);

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

extern int soLoadDirRefClust (uint32_t nClust);

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

extern SODataClust *soGetDirRefClust (void);

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

extern int soStoreDirRefClust (void);

#endif /* SOFS_BASICOPER_H_ */
