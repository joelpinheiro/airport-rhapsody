/**
 *  \file soFreeInode.c (implementation file)
 *
 *  \author Jose Mendes
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/**
 *  \brief Free the referenced inode.
 *
 *  The inode must be in use, belong to one of the legal file types and have no directory entries associated with it
 *  (refcount = 0). The inode is marked free in the dirty state and inserted in the list of free inodes.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be freed.
 *
 *  The only affected fields are:
 *     \li the free flag of mode field, which is set
 *     \li the <em>time of last file modification</em> and <em>time of last file access</em> fields, which change their
 *         meaning: they are replaced by the <em>prev</em> and <em>next</em> pointers in the double-linked list of free
 *         inodes.
 * *
 *  \param node number of the inode to be freed
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ESBTINPINVAL, if the table of inodes metadata in the superblock is inconsistent
 *  \return -\c ETINDLLINVAL, if the double-linked list of free inodes is inconsistent
 *  \return -\c EFININVAL, if a free inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soFreeInode (uint32_t nInode)
{
	soColorProbe (612, "07;31", "soFreeInode (%"PRIu32")\n", nInode);

	int error;

	SOSuperBlock *sb;
	SOInode *arr;
	SOInode *tail;
	SOInode *p_inode;

	uint32_t p_blk;
	uint32_t p_blkTail;
	uint32_t p_offseTail;
	uint32_t p_offset;
	uint32_t next;

	if (nInode == 0)
		return -EINVAL;

	if ((error = soLoadSuperBlock() ) != 0)
		return error;

	/* Verifies the Get and Load from the SuperBlock */
	if ((sb = soGetSuperBlock() ) == NULL)
			return -EIO;


	/* After check, does the Load and Get from the iNode */
	if ((error = soConvertRefInT(nInode, &p_blk, &p_offset)) != 0)
		return error;



	/* Load of iNode */
	if((error = soLoadBlockInT(p_blk)) != 0)
   			return error;

	p_inode = soGetBlockInT();

   	if( (error = soQCheckInodeIU(sb,&p_inode[p_offset])) != 0)
   		return error;

	next = p_inode[p_offset].vD2.next;

	/* Actualize the double linked list with the freed iNode */
	if (sb -> ifree == 0)
	{
		p_inode[p_offset].mode |= INODE_FREE;
		p_inode[p_offset].vD1.prev = p_inode[p_offset].vD2.next = NULL_INODE;
		sb->ihead = sb->itail = nInode;

		if ((error = soStoreBlockInT() ) != 0)
			return error;
	}

	/* Store the iNode on the disk, loads and gets the iNode, and stores the contents of iTail */
	else
	{
		p_inode[p_offset].vD1.prev = sb -> itail;
		p_inode[p_offset].vD2.next = NULL_INODE;
		p_inode[p_offset].owner = 0;
		p_inode[p_offset].group = 0;
		p_inode[p_offset].mode |= INODE_FREE;

		if ((error = soStoreBlockInT()) != 0)
			return error;

		if ((error = soConvertRefInT(sb->itail, &p_blkTail, &p_offseTail)) != 0)
			return error;

		if ((error = soLoadBlockInT(p_blkTail)) != 0)
			return error;

		if ((arr = soGetBlockInT() ) == NULL)
			return -EIO;

		arr[p_offseTail].vD2.next = nInode;
		sb -> itail = nInode;

		if ((error = soStoreBlockInT() ) != 0)
			return error;
	}

	sb -> ifree++;

	/* Stores the SuperBlock */
	if ((error = soStoreSuperBlock() ) != 0)
		return error;

	return 0;
}
