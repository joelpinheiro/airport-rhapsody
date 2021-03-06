/**
 *  \file soWriteInode.c (implementation file)
 *
 *  \author Joel Pinheiro
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

/**
 *  \brief Write specific inode data to the table of inodes.
 *
 *  The inode may be either in use and belong to one of the legal file types or be free in the dirty state.
 *  Upon writing, the <em>time of last file modification</em> and <em>time of last file access</em> fields are set to
 *  current time, if the inode is in use.
 *
 *  \param p_inode pointer to the buffer containing the data to be written from
 *  \param nInode number of the inode to be written into
 *  \param status inode status (in use / free in the dirty state)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>buffer pointer</em> is \c NULL or the <em>inode number</em> is out of range or the
 *                      inode status is invalid
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWriteInode (SOInode *p_inode, uint32_t nInode, uint32_t status)
{
  soColorProbe (512, "07;31", "soWriteInode (%p, %"PRIu32", %"PRIu32")\n", p_inode, nInode, status);


  int error; // output dos erros
  uint32_t nBloco, offset;

  if((error = soLoadSuperBlock()) != 0)
	  return error;

  SOSuperBlock * p_sb = soGetSuperBlock();

  // nInode deve ser superior a 0 (raiz) ou maior/igual ao itotal do sb
  if(nInode < 0 || nInode >= p_sb->itotal)
  {
	  return -EINVAL;
  }

  // nó-i não está em uso nem no estado sujo
  if(status != IUIN && status != FDIN)
  {
	  return -EINVAL;
  }

  if(p_inode == NULL)
  {
	  return -EINVAL;
  }

  if(status == IUIN){	// verifica consistência do nó-i em uso
		if((error = soQCheckInodeIU(p_sb,  p_inode)) != 0)
		{
 			return error;
		}
  }
  else					// verifica consistência do nó-i em dirty state
  {
		if((error = soQCheckFDInode(p_sb,p_inode)) != 0)
		{
			return error;
		}
  }

  	// bloco e offset do nó i a ser escrito
	soConvertRefInT(nInode, &nBloco, &offset);
	soLoadBlockInT(nBloco);
	SOInode *inode = soGetBlockInT();

	// escreve nó-i
	inode[offset] = *p_inode;

	// se nó-i em uso, actualiza tempo de acesso e tempo de modificação com o tempo atual -> NULL
	if(status == IUIN)
	{
		inode[offset].vD1.atime = time(NULL);
		inode[offset].vD2.mtime = time(NULL);
	}

	if((error = soStoreBlockInT ()))
		return error;

  return 0;
}
