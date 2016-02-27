/**
 *  \file soAllocDataCluster.c (implementation file)
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

int soReplenish (SOSuperBlock *p_sb);
int soDeplete (SOSuperBlock *p_sb);

/**
 *  \brief Allocate a free data cluster.
 *
 *  The cluster is retrieved from the retrieval cache of free data cluster references. If the cache is empty, it has to
 *  be replenished before the retrieval may take place.  If the data cluster is in the dirty state, it has to be cleaned
 *  first.
 *
 *  \param p_nClust pointer to the location where the logical number of the allocated data cluster is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>pointer to the logical data cluster number</em> is \c NULL
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c ESBDZINVAL, if the data zone metadata in the superblock is inconsistent
 *  \return -\c ESBFCCINVAL, if the free data clusters caches in the superblock are inconsistent
 *  \return -\c EFCTINVAL, if the number of free data clusters is overall inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAllocDataCluster (uint32_t *p_nClust)
{
	soColorProbe (613, "07;33", "soAllocDataCluster (%p)\n", p_nClust);
  
	int err;
	SOSuperBlock *p_sb;
  
	uint32_t *cTInT;
	/* pointer to the location where the contents
	of a block of the mapping table cluster to inode is to be stored */
	uint32_t nBlk;
	/* logic block number of the mapping
	table cluster to inode */
	uint32_t off;
	/* offset within a block of the mapping
	table cluster to inode */



	/*Ponteiro para o SuperBlock*/
	if((err = soLoadSuperBlock()) != 0)
		return -err;

	p_sb = soGetSuperBlock();
	
	if(p_sb == NULL)
		return -EINVAL;
		
	/*Verifica se não há data clusters livres*/
	if(p_sb->dzone_free == 0)
		return -ENOSPC;

	/*Verifica se o cluster não é valido*/
	if(p_nClust == NULL)
		return -EINVAL;
		
	/*Verifica os erros ESBDZINVAL, ESBFCCINVAL, EFCTINVAL, EBADF, EIO e ELIBBAD */
	if((err = soQCheckDZ(p_sb)) != 0)
		return err;
		
	/*Se a cache estiver vazia executa a função replenish*/
	if(p_sb->dzone_retriev.cache_idx == DZONE_CACHE_SIZE)
		if((err = soReplenish(p_sb)) != 0)
			return err;
			
			
	/*Aloca o data cluster*/
	*p_nClust = p_sb->dzone_retriev.cache[p_sb->dzone_retriev.cache_idx];

	if ((err = soConvertRefCInMT (*p_nClust, &nBlk, &off)) != 0)
		return err;
	if ((err = soLoadBlockCTInMT (nBlk)) != 0) return err;
		cTInT = soGetBlockCTInMT ();
	if (cTInT[off] != NULL_INODE) /* check if the data cluster is dirty */
	{ /* it is, clean it */
		if ((err = soCleanDataCluster (cTInT[off], *p_nClust)) != 0)
			return err;
	}

	p_sb->dzone_retriev.cache[p_sb->dzone_retriev.cache_idx] = NULL_CLUSTER;
	p_sb->dzone_retriev.cache_idx++;
	p_sb->dzone_free--;
	
	/*Grava o superblock*/
	if((err = soStoreSuperBlock()) != 0)
		return err;
		

	return 0;
}

/**
 *  \brief Replenish the retrieval cache of references to free data clusters.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReplenish (SOSuperBlock *p_sb)
{   

                     /* insert your code here */
    	uint32_t nclustt = (p_sb->dzone_free < DZONE_CACHE_SIZE) ? p_sb->dzone_free : DZONE_CACHE_SIZE;
	uint32_t pos = p_sb->fctable_pos;
	uint32_t nByte,nBit,nBlk,mask;
	int n = DZONE_CACHE_SIZE - nclustt;
	int stat;
	unsigned char* fcBMapT;

	do{
		soConvertRefBMapT(pos,&nBlk,&nByte,&nBit);
		if((stat = soLoadBlockBMapT(nBlk)) != 0)
			return stat;
		if((fcBMapT = soGetBlockBMapT()) == NULL)
			return -EIO;
		mask = 0x80 >> nBit;
       		if((fcBMapT[nByte]& mask) == mask){
		    	p_sb->dzone_retriev.cache[n] = pos;
			fcBMapT[nByte] &= ~mask;
			n++;
		}
		pos = (pos + 1) % p_sb->dzone_total;
		if ((stat = soStoreBlockBMapT()) != 0) 
			return stat;
	}while((n < DZONE_CACHE_SIZE) && (pos != p_sb->fctable_pos));

	if(n!= DZONE_CACHE_SIZE){
		soDeplete(p_sb);
		do{
			soConvertRefBMapT(pos,&nBlk,&nByte,&nBit);
			if((stat = soLoadBlockBMapT(nByte)) != 0)
				return stat;
			if((fcBMapT = soGetBlockBMapT()) == NULL)
				return -EIO;
			mask = 0x80 >> nBit;

			if((fcBMapT[nByte] & mask) == mask){
				p_sb->dzone_retriev.cache[n] = pos;
				fcBMapT[nByte] &= ~mask;
				n++;
			}
			pos = (pos+1) % p_sb->dzone_total;
			if ((stat = soStoreBlockBMapT()) != 0) 
				return stat;
		}while(n < DZONE_CACHE_SIZE);
	}
	p_sb->dzone_retriev.cache_idx = DZONE_CACHE_SIZE - nclustt;
	p_sb->fctable_pos = pos;



  return 0;
}
