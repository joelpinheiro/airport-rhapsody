/**
 *  \file soFreeDataCluster.c (implementation file)
 *
 *  \author
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
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

/* Allusion to internal functions */

int soDeplete (SOSuperBlock *p_sb);

/**
 *  \brief Free the referenced data cluster.
 *
 *  The cluster is inserted into the insertion cache of free data cluster references. If the cache is full, it has to be
 *  depleted before the insertion may take place. It has to have been previouly allocated (which means that although free,
 *  it will be in the dirty state).
 *
 *  Notice that the first data cluster, supposed to belong to the file system root directory, can never be freed.
 *
 *  \param nClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>data cluster number</em> is out of range
 *  \return -\c EDCNALINVAL, if the data cluster has not been previously allocated
 *  \return -\c ESBDZINVAL, if the data zone metadata in the superblock is inconsistent
 *  \return -\c ESBFCCINVAL, if the free data clusters caches in the superblock are inconsistent
 *  \return -\c EFCTINVAL, if the number of free data clusters is overall inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soFreeDataCluster (uint32_t nClust)
{
	soColorProbe (614, "07;33", "soFreeDataCluster (%"PRIu32")\n", nClust);

	SOSuperBlock *p_sb;
  	uint32_t data_stat;
  	uint32_t ERRO;

  	// Ler SuperBlock
  	if((ERRO = soLoadSuperBlock()) != 0)
  		return ERRO;

  	p_sb = soGetSuperBlock();

  	if(p_sb == NULL)
  		return -EIO;
  	// consistência da zona de dados
  	if ((ERRO = soQCheckDZ(p_sb)) != 0)
  			return ERRO;

  	if( ( nClust > p_sb->dzone_total - 1 ) || ( nClust <= 0 ))
  	  		return -EINVAL;

  	// verificação consistência do SuperBloco
  	if ((ERRO = soQCheckSuperBlock(p_sb)) != 0)
  			return ERRO;

  	// Ver se esta alocado
  	if((ERRO = soQCheckStatDC(p_sb, nClust, &data_stat)) != 0)
    		return ERRO;

  	// Cluster Livre
  	if(data_stat == FREE_CLT)
  		return -EDCNALINVAL;


  	// Verificar se a cache esta cheia
  	if(p_sb->dzone_insert.cache_idx == DZONE_CACHE_SIZE)
  		if((ERRO = soDeplete(p_sb)) !=0)
  			return ERRO;

  	// Guardar o Cluster na Cache
  	p_sb->dzone_insert.cache[p_sb->dzone_insert.cache_idx] = nClust;
  	p_sb->dzone_insert.cache_idx++;
  	p_sb->dzone_free++;


  	// Store no superblock
  	if((ERRO = soStoreSuperBlock()) != 0)
  			return ERRO;

  	return 0;
}

/**
 *  \brief Deplete the insertion cache of references to free data clusters.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soDeplete (SOSuperBlock *p_sb)
{
	/* funcao criada por Joao Ribeiro */
  int n, error;
  unsigned char *fcBMapT;
  uint32_t p_nBlk;
  uint32_t p_byteOff;
  uint32_t p_bitOff;

  for(n = 0;n < p_sb->dzone_insert.cache_idx; n++)
  {
    // devolve referencias para a ultima posicao da cache de insercao
    soConvertRefBMapT(p_sb->dzone_insert.cache[n], &p_nBlk, &p_byteOff, &p_bitOff);

    // carrega bloco da tabela de clusters livres
    if((error = soLoadBlockBMapT(p_nBlk)) != 0)
      return error;
    fcBMapT = soGetBlockBMapT();

    // actualiza os valores da tabela de clusters livres
    fcBMapT[p_byteOff] |= (0x80 >> p_bitOff);

    // poe referencia nula na cache de insercao
    p_sb->dzone_insert.cache[n] = NULL_CLUSTER;

    // guarda alteracoes a tabela de clusters livres
    if((error = soStoreBlockBMapT()) != 0)
      return error;
  }
  // actualiza index da cache de insercao
  p_sb->dzone_insert.cache_idx = 0;

  // verificacao de consistencia do superbloco
  if((error = soStoreSuperBlock()) != 0)
    return error;

  // guarda alteracoes
  if((error = soQCheckDZ(p_sb)) != 0)
    return error;

  return 0;
}
