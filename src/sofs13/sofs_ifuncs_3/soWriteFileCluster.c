/**
 *  \file soWriteFileCluster.c (implementation file)
 *
 *  \author
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"

/** \brief operation get the physical number of the referenced data cluster */
#define GET         0
/** \brief operation allocate a new data cluster and associate it to the inode which describes the file */
#define ALLOC       1
/** \brief operation free the referenced data cluster */
#define FREE        2
/** \brief operation free the referenced data cluster and dissociate it from the inode which describes the file */
#define FREE_CLEAN  3
/** \brief operation dissociate the referenced data cluster from the inode which describes the file */
#define CLEAN       4

/* Allusion to external function */

extern int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);

/**
 *  \brief Write a specific data cluster.
 *
 *  Data is written into the information content of a specific data cluster which is supposed to belong to an inode
 *  associated to a file (a regular file, a directory or a symbolic link). Thus, the inode must be in use and belong
 *  to one of the legal file types.
 *
 *  If the cluster has not been allocated yet, it will be allocated now so that data can be stored there.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where data is to be written into
 *  \param buff pointer to the buffer where data must be written from
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the <em>pointer to the buffer area</em> is \c NULL
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWriteFileCluster (uint32_t nInode, uint32_t clustInd, void *buff)
{
  soColorProbe (412, "07;31", "soWriteFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

  uint32_t ERRO, nBlk, offset, nLogicalDC, nBlocoC;
  SOSuperBlock *p_sb;

  //Ler o superblock
  if((ERRO = soLoadSuperBlock()) != 0)
	  return ERRO;

  p_sb = soGetSuperBlock();

  // erro a
  if(p_sb == NULL)
	  return -EIO;

  // argumentos inválidos
  if(nInode < 0 || nInode >= p_sb->itotal || clustInd < 0 || clustInd >= MAX_FILE_CLUSTERS || buff == NULL)
  		return -EINVAL;

  soConvertRefInT(nInode, &nBlk, &offset);
  soLoadBlockInT(nBlk);
  SOInode *inode = soGetBlockInT();

  //o inode não foi alocado
  if(inode[offset].mode == INODE_FREE)
	  return -EINVAL;

  if((ERRO = soHandleFileCluster(nInode, clustInd, GET,&nLogicalDC)) != 0)
	  return ERRO;

  //data cluster esta alocado? caso não esteja, aloca-o
  if(nLogicalDC == NULL_CLUSTER)
  {
  	if((ERRO = soHandleFileCluster(nInode, clustInd, ALLOC, &nLogicalDC)) != 0)
  		return ERRO;
  }

  // número físico do bloco onde está o cluster onde queremos escrever
  nBlocoC = nLogicalDC*BLOCKS_PER_CLUSTER + p_sb->dzone_start;


  //faz store no superblock
  if((ERRO = soStoreSuperBlock()) != 0)
  	return ERRO;

  //escreve buff no cluster
  if((ERRO = soWriteCacheCluster(nBlocoC, buff)) != 0)
  	return ERRO;

  return 0;
}
