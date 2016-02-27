/**
 *  \file soAccessGranted.c (implementation file)
 *
 *  \author Sequeira
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "sofs_probe.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/** \brief inode in use status */
#define IUIN  0
/** \brief free inode in dirty state status */
#define FDIN  1

/** \brief performing a read operation */
#define R  0x0004
/** \brief performing a write operation */
#define W  0x0002
/** \brief performing an execute operation */
#define X  0x0001

/* allusion to internal functions */

int soReadInode (SOInode *p_inode, uint32_t nInode, uint32_t status);

/**
 *  \brief Check the inode access rights against a given operation.
 *
 *  The inode must to be in use and belong to one of the legal file types.
 *  It checks if the inode mask permissions allow a given operation to be performed.
 *
 *  When the calling process is <em>root</em>, access to reading and/or writing is always allowed and access to
 *  execution is allowed provided that either <em>user</em>, <em>group</em> or <em>other</em> have got execution
 *  permission.
 *
 *  \param nInode number of the inode
 *  \param opRequested operation to be performed:
 *                    a bitwise combination of R, W, and X
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or no operation of the defined class is described
 *  \return -\c EACCES, if the operation is denied
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAccessGranted (uint32_t nInode, uint32_t opRequested)
{
  soColorProbe (514, "07;31", "soAccessGranted (%"PRIu32", %"PRIu32")\n", nInode, opRequested);

  int stat;
  SOSuperBlock *p_sb;
  SOInode *inode, *ainode;
  SOInode checkinode;
  uint32_t p_nBlk, p_offset;

  if((opRequested == 0)||((opRequested & (R|W|X)) != opRequested))
  	  return -EINVAL;

  if((stat = soLoadSuperBlock()) != 0)
	  return stat;

  p_sb = soGetSuperBlock();

  if((nInode < 0) || (nInode > (p_sb->itotal -1)))
	  return -EINVAL;
  if((stat = soQCheckInT(p_sb))!=0)
  	  return stat;
  if((stat = soReadInode(&checkinode, nInode,IUIN))!=0)
	  return stat;
  if((stat = soQCheckInodeIU(p_sb, &checkinode))!=0)
	  return stat;
  if((stat = soConvertRefInT(nInode, &p_nBlk, &p_offset))!= 0)
	  return stat;

  if((stat = soLoadBlockInT(p_nBlk))!= 0)
	  return stat;

  ainode = soGetBlockInT();

  inode = &ainode[p_offset];

  if(inode->owner != 0){
	  if(getuid() == inode->owner){
		  if(((opRequested == R) && ((inode->mode & INODE_RD_USR)>>8) == 1 ) ||
			 ((opRequested == X) && ((inode->mode & INODE_EX_USR)>>6) == 1 ) ||
			 ((opRequested == W) && ((inode->mode & INODE_WR_USR)>>7) == 1 ) ||
			 (((inode->mode & (INODE_RD_USR | INODE_WR_USR | INODE_EX_USR))>>6) == opRequested)){
			  return 0;
		  }
	  }
	  if(getgid() == inode->group){
		  if(((opRequested == R) && ((inode->mode & INODE_RD_GRP)>>5) == 1 ) ||
			 ((opRequested == W) && ((inode->mode & INODE_WR_GRP)>>4) == 1 ) ||
			 ((opRequested == X) && ((inode->mode & INODE_EX_GRP)>>3) == 1 ) ||
			 (((inode->mode & (INODE_RD_GRP | INODE_WR_GRP | INODE_EX_GRP))>>3) == opRequested)){
			  return 0;
		  }
	  }
	  if(((opRequested == R) && ((inode->mode & INODE_RD_OTH)>>2) == 1 ) ||
	     ((opRequested == W) && ((inode->mode & INODE_WR_OTH)>>1) == 1 ) ||
	     ((opRequested == X) && (inode->mode & INODE_EX_OTH) == 1 ) ||
	     ((inode->mode & (INODE_RD_OTH | INODE_WR_OTH | INODE_EX_OTH)) == opRequested)){
		  return 0;
	  }
  }
  else{
	  if((opRequested == R) || (opRequested == W) || (opRequested == (R|W)))
			  return 0;
		  else{
			  if((inode->mode & INODE_EX_USR) >> 6 || (inode->mode & INODE_EX_GRP) >> 3 || (inode->mode & INODE_EX_OTH)){
				  return 0;
			  }
		  }

  }

  return -EACCES;

}
