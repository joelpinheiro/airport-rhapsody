/**
 *  \file soHandleFileClusters.c (implementation file)
 *
 *  \author	João Nascimento & José Sequeira
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
 *  \brief Handle all data clusters from the list of references starting at a given point.
 *
 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.
 *
 *  Several operations are available and can be applied to the file data clusters starting from the index to the list of
 *  direct references which is given.
 *
 *  The list of valid operations is
 *
 *    \li FREE:       free all data clusters starting from the referenced data cluster
 *    \li FREE_CLEAN: free all data clusters starting from the referenced data cluster and dissociate them from the
 *                    inode which describes the file
 *    \li CLEAN:      dissociate all data clusters starting from the referenced data cluster from the inode which
 *                    describes the file.
 *
 *  Depending on the operation, the field <em>clucount</em> and the lists of direct references, single indirect
 *  references and double indirect references to data clusters of the inode associated to the file are updated.
 *
 *  Thus, the inode must be in use and belong to one of the legal file types for the operations FREE and FREE_CLEAN and
 *  must be free in the dirty state for the operation CLEAN.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustIndIn index to the list of direct references belonging to the inode which is referred (it contains the
 *                    index of the first data cluster to be processed)
 *  \param op operation to be performed (FREE, FREE AND CLEAN, CLEAN)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the requested operation is invalid
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soHandleFileClusters (uint32_t nInode, uint32_t clustIndIn, uint32_t op)
{
  soColorProbe (414, "07;31", "soHandleFileClusters (%"PRIu32", %"PRIu32", %"PRIu32")\n", nInode, clustIndIn, op);

	/*Declaracao de Variaveis*/
	SOSuperBlock *p_sb;
	SODataClust *p_ref;
	SODataClust ref1,ref2;
	uint32_t nclust;
	int stat, offset,idx;	
	SOInode p_inode;

	/*Validacao de parametros*/
	/* Leitura de Superblock e criacao de ponteiro do mesmo*/
	if((stat = soLoadSuperBlock())!=0)
		return stat;
	p_sb = soGetSuperBlock();
	
	
     
	/* Validacao do nInode */
	if(nInode >= p_sb->itotal)
		return -EINVAL;
	
	     
	/* Validacao da op*/
	if(op != FREE && op != FREE_CLEAN && op != CLEAN)
		return -EINVAL;
	
	
	if(op !=CLEAN){
		if((stat = soReadInode(&p_inode, nInode, IUIN)) !=0)
			return stat;
	}
	else{
		if((stat = soReadInode(&p_inode, nInode, FDIN)) !=0)
			return stat;
	}
	     
	/* Verifica se ClustIndIn está fora do range permitido*/ 
	if(clustIndIn >= MAX_FILE_CLUSTERS)
		return -EINVAL;

	/*Execucao da operacao pretendida*/
	//Duplamente Indirectas
	if(p_inode.i2!= NULL_CLUSTER){
		nclust = (p_sb->dzone_start + (p_inode.i2*BLOCKS_PER_CLUSTER));
		if((stat = soLoadSngIndRefClust(nclust))!=0)
			return stat;
		
		p_ref = soGetSngIndRefClust();
		ref1 = *p_ref;
		if(clustIndIn >= N_DIRECT+RPC){
				idx = (clustIndIn - (N_DIRECT+RPC))/RPC;
				offset = (clustIndIn -(N_DIRECT+RPC))%RPC;

		}
		else{
			idx =offset= 0;
		}

		for(; idx<RPC;idx++){
			if(ref1.ref[idx]!=NULL_CLUSTER){
				nclust = p_sb->dzone_start + (ref1.ref[idx]*BLOCKS_PER_CLUSTER);
				if((stat = soLoadSngIndRefClust(nclust))!=0)
					return stat;
				p_ref= soGetSngIndRefClust();
				ref2 = *p_ref;

				for(; offset<RPC; offset++){
					if(ref2.ref[offset]!= NULL_CLUSTER)
						if((stat = soHandleFileCluster(nInode, (offset+(idx*RPC) + RPC + N_DIRECT),op,NULL))!=-0) 
							return stat;

				}
		

			}
			offset = 0;
		}

	}
	//Indirectas
	if(p_inode.i1!=NULL_CLUSTER && clustIndIn < N_DIRECT +RPC){
		nclust = (p_sb->dzone_start + (p_inode.i1*BLOCKS_PER_CLUSTER));
		if((stat = soLoadSngIndRefClust(nclust))!=0)
			return stat;
		p_ref = soGetSngIndRefClust();
		ref1 = *p_ref;
		if(clustIndIn >= N_DIRECT){
			idx = clustIndIn - N_DIRECT;
		}
		else
			idx = 0;

		for(; idx<RPC; idx++){
			if(ref1.ref[idx] != NULL_CLUSTER){
				if((stat = soHandleFileCluster(nInode, idx + N_DIRECT,op,NULL))!=0)
					return stat;
			
			}

		}
	}
	//Directas
	if(clustIndIn < N_DIRECT){
		for(;clustIndIn<N_DIRECT; clustIndIn++){
			if(p_inode.d[clustIndIn]!= NULL_CLUSTER){
				if((stat = soHandleFileCluster(nInode,clustIndIn,op, NULL)) != 0)
					return stat;
			}
		}
	}
	
	

  return 0;
}
