  /**
 *  \file soCleanInode.c (implementation file)
 *
 *  \author Jose Mendes
 */

#define CLEAN_INODE
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
#ifdef CLEAN_INODE
#include "sofs_ifuncs_3.h"
#endif

/** \brief inode in use status */
#define IUIN  0
/** \brief free inode in dirty state status */
#define FDIN  1

/* allusion to internal functions */

int soReadInode (SOInode *p_inode, uint32_t nInode, uint32_t status);

/**
 *  \brief Clean an inode.
 *
 *  The inode must be free in the dirty state.
 *  The inode is supposed to be associated to a file, a directory, or a symbolic link which was previously deleted.
 *
 *  This function cleans the list of data cluster references.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be cleaned.
 *
 *  \param nInode number of the inode
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soCleanInode (uint32_t nInode)
{
  soColorProbe (513, "07;31", "soCleanInode (%"PRIu32")\n", nInode);


  int err = 0;

  if ((err = soLoadSuperBlock() ) != 0)
	  return err;

  SOSuperBlock *sb = soGetSuperBlock();

  // validates the parameters
  if (nInode >= sb->itotal || nInode <= 0)
		return -EINVAL;

  if((err = soHandleFileClusters(nInode,0, CLEAN))!=0)
	return err;
  /*
  // reads the iNode that's on nInode (assumes it's in a free-dirty state
  SOInode p_no;
  if ((err = (soReadInode (&p_no, nInode, FDIN))) != 0)
	  return err;

  if ((err = (soHandleFileCluster (nInode, 0, FDIN))) != 0)
	  return err;

  p_no.refcount = 2;
  p_no.owner = 0;
  p_no.group = 0;
  p_no.size = 0;
  p_no.clucount = 0;

  p_no.i1 = NULL_CLUSTER;
  p_no.i2 = NULL_CLUSTER;

  // re-read the iNode for changes
  if((err = (soReadInode(&p_no, nInode, FDIN))) != 0)
  {
	  printf("Returning with error code: %d\n",err);
	  return err;
  }

  // writes on the iNode on the iNode Table
  if (( err = (soWriteInode (&p_no, nInode, FDIN))) != 0)
  {
	  printf("Returning with error code: %d\n",err);
      return err;
  }

  */
  // Checks the SB on the end
  if((err = soQCheckSuperBlock(sb) ) !=0 )
	  return err;

  	if((err = soStoreSuperBlock() ) !=0 )
  		return err;

  return 0;
}
