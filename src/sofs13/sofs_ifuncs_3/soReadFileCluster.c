/**
 *  \file soReadFileCluster.c (implementation file)
 *
 *  \author Joel Pinheiro
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
 *  \brief Read a specific data cluster.
 *
 *  Data is read from a specific data cluster which is supposed to belong to an inode associated to a file (a regular
 *  file, a directory or a symbolic link). Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  If the cluster has not been allocated yet, the returned data will consist of a cluster whose byte stream contents
 *  is filled with the character null (ascii code 0).
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where data is to be read from
 *  \param buff pointer to the buffer where data must be read into
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the <em>pointer to the buffer area</em> is \c NULL
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReadFileCluster (uint32_t nInode, uint32_t clustInd, void *buff)
{
	soColorProbe (411, "07;31", "soReadFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

	uint32_t error;
	SOSuperBlock *p_sb;
	SOInode inode;
	uint32_t p_outVal;
	uint32_t nBloco,offset;

	// o número do nó-i tem que ter valores válidos
	if(nInode >= p_sb->itotal || nInode < 0)
		return -EINVAL;

	// o índice para a tabela de referências têm que ter valores válidos
	if(clustInd >= MAX_FILE_CLUSTERS || clustInd < 0)
		return -EINVAL;

	// o ponteiro para a região de armazenamento não pode ser nulo
	if(buff == NULL)
		return -EINVAL;

	if((error = soLoadSuperBlock()) != 0)
	{
		return error;
	}

	p_sb = soGetSuperBlock();

	// Validação de consistência
	soConvertRefInT(nInode, &nBloco, &offset);
	soLoadBlockInT(nBloco);
	SOInode *p_inode = soGetBlockInT();

	// o nó-i referenciado tem que estar em uso e estar associado a um tipo válido
	if((error = soQCheckInodeIU(p_sb,  p_inode)) != 0)
	{
		return error;
	}

	// obter o número lógico do cluster
	if((error = soHandleFileCluster(nInode, clustInd, GET, &p_outVal)))
		return error;

	int i;
	// se o cluster não estiver alocado tem que se colocar tudo a zero
	if(p_outVal == NULL_CLUSTER)
	{
		for(i=0; i<BSLPC; i++)
			((unsigned char *)buff)[i] = '\0';
	}
	// se o cluster estiver alocado temos que retorná-lo para um ponteiro e lê-lo para o buffer
	else
			soReadCacheCluster(p_outVal*BLOCKS_PER_CLUSTER + p_sb->dzone_start, buff);

	// guarda as alterações
	if((error = soStoreSuperBlock()))
		return error;

	return 0;
	}
