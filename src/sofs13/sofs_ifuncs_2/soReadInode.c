/**
 *  \file soReadInode.c (implementation file)
 *
 *  \author
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
 *  \brief Read specific inode data from the table of inodes.
 *
 *  The inode must be either in use and belong to one of the legal file types or be free in the dirty state.
 *  Upon reading, the <em>time of last file access</em> field is set to current time, if the inode is in use.
 *
 *  \param p_inode pointer to the buffer where inode data must be read into
 *  \param nInode number of the inode to be read from
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

int soReadInode (SOInode *p_inode, uint32_t nInode, uint32_t status)
{
  soColorProbe (511, "07;31", "soReadInode (%p, %"PRIu32", %"PRIu32")\n", p_inode, nInode, status);

	/* funcao criada por Joao Ribeiro */
  int error, i;
  SOSuperBlock* p_sb;
  uint32_t p_nBlk, p_offset, blockin;
  SOInode* cr_inode;

  // VALIDACAO DE CONFORMIDADE
  
  // verifica se status e valido
  if(status != IUIN && status != FDIN)
    return -EINVAL;

  // verifica se o inode foi allocado
  if(p_inode == NULL)
    return -EINVAL;

  // carrega o superbloco
  if((error = soLoadSuperBlock()) != 0)
    return error;
  p_sb = soGetSuperBlock();

  // verifica se nInode e um valor valido
  blockin = p_sb->itable_size * IPB;
  if(nInode > blockin || nInode < 0)
    return -EINVAL;


  // calculo de n. de bloco e offset
  if((error = soConvertRefInT(nInode, &p_nBlk, &p_offset)) != 0)
    return error;

  // carregamento do inode
  if((error = soLoadBlockInT(p_nBlk)) != 0)
    return error;
  cr_inode = soGetBlockInT() + p_offset;



  // VALIDACAO DE CONSISTENCIA
  // validacao para inode livre em dirty state
  if(status == FDIN)
  {
    if(!((cr_inode->mode >> 12) & 0x01))
      return -EFDININVAL;
    if((error = soQCheckFDInode(p_sb, cr_inode)) != 0)
      return error;
  }
  
  // validacao para inode em uso
  else if(status == IUIN)
  {
    if(!(((cr_inode->mode >> 9) & 0x01)
    || ((cr_inode->mode >> 10) & 0x01)
    || ((cr_inode->mode >> 11) & 0x01)))
      return -EIUININVAL;
    if((error = soQCheckInodeIU (p_sb, cr_inode)) != 0)
      return error;

  }

  //  leitura do inode
  // actualiza o ultimo tempo de acesso caso necessario
  if(status == IUIN)
    cr_inode->vD1.atime = time(NULL);
  p_inode->mode = cr_inode->mode;
  p_inode->refcount = cr_inode->refcount;
  p_inode->owner = cr_inode->owner;
  p_inode->group = cr_inode->group;
  p_inode->size = cr_inode->size;
  p_inode->clucount = cr_inode->clucount;
  p_inode->vD1 = cr_inode->vD1;
  p_inode->vD2 = cr_inode->vD2;
  p_inode->i1 = cr_inode->i1;
  p_inode->i2 = cr_inode->i2;
  for(i = 0; i < N_DIRECT; i++)
    p_inode->d[i] = cr_inode->d[i];

  // guarda alteracoes
  if((error = soStoreBlockInT()) != 0)
    return error;

  return 0;
}
