/**
 *  \file soHandleFileCluster.c (implementation file)
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

/* Allusion to internal functions */

static int soHandleDirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                           uint32_t *p_outVal);
static int soHandleSIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                              uint32_t *p_outVal);
static int soHandleDIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                              uint32_t *p_outVal);
static int soMapDCtoIn (uint32_t nInode, uint32_t nClust);
static int soUnmapDCtoIn (uint32_t nInode, uint32_t nClust);

/**
 *  \brief Handle of a file data cluster.
 *
 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.
 *
 *  Several operations are available and can be applied to the file data cluster whose logical number is given.
 *
 *  The list of valid operations is
 *
 *    \li GET:        get the logical number of the referenced data cluster
 *    \li ALLOC:      allocate a new data cluster and associate it to the inode which describes the file
 *    \li FREE:       free the referenced data cluster
 *    \li FREE_CLEAN: free the referenced data cluster and dissociate it from the inode which describes the file
 *    \li CLEAN:      dissociate the referenced data cluster from the inode which describes the file.
 *
 *  Depending on the operation, the field <em>clucount</em> and the lists of direct references, single indirect
 *  references and double indirect references to data clusters of the inode associated to the file are updated.
 *
 *  Thus, the inode must be in use and belong to one of the legal file types for the operations GET, ALLOC, FREE and
 *  FREE_CLEAN and must be free in the dirty state for the operation CLEAN.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the logical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL when the function is called)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the requested operation is invalid or the <em>pointer to outVal</em> is \c NULL when it
 *                      should not be (GET / ALLOC)
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{
  soColorProbe (413, "07;31", "soHandleFileCluster (%"PRIu32", %"PRIu32", %"PRIu32", %p)\n",
                nInode, clustInd, op, p_outVal);

		// funcao criada por Joao Ribeiro 60634 */
  int error, status, inode_status;
  SOSuperBlock *p_sb;
  SOInode p_inode;

  // carrega o superbloco
  if((error = soLoadSuperBlock()) != 0)
    return error;
  p_sb = soGetSuperBlock();

  // inode index verification
  if(nInode < 0 || nInode > p_sb->itotal)
    return -EINVAL;

  // valid operation verification
  if(op != GET && op != ALLOC && op != FREE && op != FREE_CLEAN && op != CLEAN)
    return -EINVAL;

  //valid out value verification
  if(p_outVal == NULL && (op == GET || op == ALLOC))
    return -EINVAL;
  if(p_outVal != NULL && (op == FREE || op == FREE_CLEAN || op == CLEAN))
    return -EINVAL; 

  if(op == CLEAN)
    inode_status = 1;
  else
    inode_status = 0;

  // carrega o inode
  if((error = soReadInode(&p_inode, nInode, inode_status)) != 0) return error;

  // validacao de consistencia
  if(op != CLEAN)
    if ((error = soQCheckInodeIU(p_sb, &p_inode)) != 0)
      return error;

  if(op == CLEAN)
    if((error = soQCheckFDInode(p_sb, &p_inode)) != 0)
      return error;

  // para referencias directas
  if(clustInd < N_DIRECT)
    status = soHandleDirect(p_sb, nInode, &p_inode, clustInd, op, p_outVal);

  // para referencias simplesmente indirectas
  else
  {
    if(clustInd < N_DIRECT + RPC)
      status = soHandleSIndirect(p_sb, nInode, &p_inode, clustInd, op, p_outVal);
    // para referencias duplamente indirectas
    else
      status = soHandleDIndirect(p_sb, nInode, &p_inode, clustInd, op, p_outVal);
  }

  /////////////////////////////////////////////////
  // guarda o inode, caso seja necessario
  if(op != GET && status == 0)
    if((error = soWriteInode(&p_inode, nInode, inode_status)) != 0)
      return error;

  // guarda o superbloco
  if((error = soStoreSuperBlock()) != 0)
    return error;
  return status;
}


/**
 *  \brief Handle of a file data cluster which belongs to the direct references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE /FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soHandleDirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                           uint32_t *p_outVal)
{


                   /* insert your code here */
	uint32_t error;

	// store the data cluster from the table of direct references
	if(op == GET) // OP == 0
	{
        *p_outVal = p_inode->d[clustInd];
        return 0;
	}
	// if the position on the table of direct references is not empty returns an error
	if(op == ALLOC) // OP == 1
	{
		//means that the position on the table of direct references is already free
		if(p_inode->d[clustInd] != NULL_CLUSTER)
			return -EDCARDYIL;

		// alloc a data cluster to put on the table of direct references
		if((error = soAllocDataCluster(p_outVal)))
			return error;

		//insert the logical number of the data cluster on the table of direct references
		p_inode->d[clustInd] = *p_outVal;

		//increments the total number of data clusters attached to the file
		p_inode->clucount+=1;

		if((error = soMapDCtoIn (nInode, *p_outVal)) != 0)
			return error;
                
	}
	if(op == FREE) // OP == 2
	{
		if(p_inode->d[clustInd] == NULL_CLUSTER)
                  return -EDCNOTIL;

		if((error = soFreeDataCluster(p_inode->d[clustInd])) != 0)
			return error;
	}
	if(op == FREE_CLEAN) // OP == 2
	{
		if(p_inode->d[clustInd] == NULL_CLUSTER)
			return -EDCNOTIL;

		//the operation is FREE so it free the data cluster
		if((error = soFreeDataCluster(p_inode->d[clustInd])))
			return error;

		if((error = soUnmapDCtoIn(nInode, p_inode->d[clustInd])) != 0)
			return error;

		//clean the position on the table of direct references
		p_inode->d[clustInd]=NULL_CLUSTER;

		//decrements the total number of data clusters attached to the file
		p_inode->clucount-=1;


	}
	if(op == CLEAN) // OP == 2
	{
                if(p_inode->d[clustInd] == NULL_CLUSTER)
			return -EDCNOTIL;
		//decrements the total number of data clusters attached to the file
		p_inode->clucount-=1;
		if((error = soUnmapDCtoIn(nInode, p_inode->d[clustInd])) != 0)
			return error;
		p_inode->d[clustInd] = NULL_CLUSTER;

	}
  return 0;
}

/**
 *  \brief Handle of a file data cluster which belongs to the single indirect references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
*  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */// flag for allocating new indirect references cluster


static int soHandleSIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                              uint32_t *p_outVal)
{	/*funcao criada por Joao Ribeiro 60634 */

  int error, iflag, i;
  SODataClust *p_clt;
  uint32_t pcn, n_cluster;

  switch(op)
  {
    case GET:
    {
      // checks if p_outVal is allocated
      if(p_outVal == NULL)
        return -EIO;

      // if the indirect references cluster is not allocated
      if(p_inode->i1 == NULL_CLUSTER)		   
      {
        *p_outVal = NULL_CLUSTER;
        iflag = 1;
	break;
      }

      // calculates the phisical cluster number
      pcn = p_sb->dzone_start + p_inode->i1 * BLOCKS_PER_CLUSTER;
        
      // loads and retrieves cluster references
      if((error = soLoadDirRefClust(pcn)) != 0)	
        return error;
      p_clt = soGetDirRefClust();

      // sets out value
      *p_outVal = p_clt->ref[clustInd-N_DIRECT];
      
      break;
    }

    case ALLOC:
    {
      // checks if p_outVal is allocated
      if(p_outVal == NULL)
        return -EIO;

      // flag for indirect cluster reference cluster not allocated
      iflag = 0;
      // if the indirect cluster reference cluster is not allocated
      if(p_inode->i1 == NULL_CLUSTER)
      {
        // allocates indirect cluster reference cluster
        if((error = soAllocDataCluster(&n_cluster)) != 0)
          return error;
        p_inode->i1 = n_cluster;
        // maps new cluster to inode
        if((error = soMapDCtoIn (nInode, n_cluster)) != 0)
          return error;
        // increments inode cluster count
        p_inode->clucount++;
        iflag = 1;
      }
      // calculates the phisical cluster number
      pcn = p_sb->dzone_start + p_inode->i1 * BLOCKS_PER_CLUSTER;

      // loads and retrieves cluster references
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_clt = soGetDirRefClust();

      // if the indirect cluster reference cluster was allocated
      if(iflag)
        // sets NULL_CLUSTER reference in all cluster references of the indirect references cluster
        for(i = 0; i < RPC; i++)
          p_clt->ref[i] = NULL_CLUSTER;

      // checks if cluster is alredy allocated
      if(p_clt->ref[clustInd-N_DIRECT] != NULL_CLUSTER)
        return -EDCARDYIL;

      // saves cluster (alloc may alter it)
      if((error = soStoreDirRefClust()) != 0)
        return error;

      // allocates cluster
      if((error = soAllocDataCluster(&n_cluster)) != 0)
        return error;

      // loads cluster again
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_clt = soGetDirRefClust();

      p_clt->ref[clustInd-N_DIRECT] = n_cluster;
      // increments inode cluster count
      p_inode->clucount++;

      // maps new cluster to inode      
      if((error = soMapDCtoIn (nInode, n_cluster)) != 0)
        return error;
      // stores indirect references cluster
      if((error = soStoreDirRefClust()) != 0)
        return error;

      // sets return value
      *p_outVal =p_clt->ref[clustInd-N_DIRECT];
      break;
    }

    case FREE:
    case FREE_CLEAN:
    case CLEAN:
    {
      // checks the indirect cluster references cluster
      if(p_inode->i1 == NULL_CLUSTER)
        return -EDCNOTIL;

      // loads and retrieves the cluster references
      pcn = p_sb->dzone_start + p_inode->i1 * BLOCKS_PER_CLUSTER;

      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_clt = soGetDirRefClust();

      n_cluster = clustInd-N_DIRECT;
      // checks if there is valid cluster reference
      if(p_clt->ref[n_cluster] == NULL_CLUSTER)
        return -EDCNOTIL;

      // if op is free, frees cluster
      if(op != CLEAN)
        if((error = soFreeDataCluster(p_clt->ref[n_cluster])) != 0)
          return error;

      if(op == CLEAN || op == FREE_CLEAN)
      {
        // unmaps cluster from inode
        if((error = soUnmapDCtoIn(nInode, p_clt->ref[n_cluster])) != 0)
  	  return error;
        // cleans the data cluster
        p_clt->ref[n_cluster] = NULL_CLUSTER;     
        // decrements cluster count in inode
        p_inode->clucount--;

        // stores change to ref cluster
        if((error = soStoreDirRefClust()) != 0)
          return error;

        // checks if direct references cluster has 0 references
        iflag = 1;
        for(i = 0; i < RPC; i++)
          if(p_clt->ref[i] != NULL_CLUSTER)
            iflag = 0;

        // if cluster has 0 references, cleans  and frees it
        if(iflag)
        {
          if((error = soUnmapDCtoIn(nInode, p_inode->i1)) != 0)
            return error;

          if((error = soFreeDataCluster(p_inode->i1)) != 0)
            return error;

          p_inode->i1 = NULL_CLUSTER;
          p_inode->clucount--;
        }
      }
   
      p_outVal = NULL;
      break;
    }
    default:
    {
      return -EINVAL;
    }
  }

  return 0;
}


/**
 *  \brief Handle of a file data cluster which belongs to the double indirect references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soHandleDIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                              uint32_t *p_outVal)
{		/* funcao criada por Joao Ribeiro 60634 */

  int error, iflagSI, iflagD, i;
  uint32_t pcn, kSI, kD, n_cluster, refSI, refD;
  SODataClust *p_cltD, *p_cltSI;

  if(op == ALLOC)
  {
      iflagSI = 0; iflagD = 0;
      // checks if p_outVal is allocated
      if(p_outVal == NULL)
        return -EIO;
      //checks if single indirect refs cluster is allocated
      if(p_inode->i2 == NULL_CLUSTER)
      {
        if((error = soAllocDataCluster(&n_cluster)) != 0)
          return error;
        iflagSI = 1;
        p_inode->i2 = n_cluster;
        p_inode->clucount++;
        // maps new cluster
        if((error = soMapDCtoIn (nInode, n_cluster)) != 0)
          return error;
      }
      // calculates cluster phisical number
      pcn = p_sb->dzone_start + p_inode->i2 * BLOCKS_PER_CLUSTER;
      // loads single indirect references cluster
      if((error = soLoadSngIndRefClust(pcn)) != 0)
        return error;
      p_cltSI = soGetSngIndRefClust();

      // if single indirect references cluster has been allocated, put null references
      if(iflagSI)
        for(i = 0; i < RPC; i++)
          p_cltSI->ref[i] = NULL_CLUSTER;

      // saves refs cluster (alloc may change it)
      if((error = soStoreSngIndRefClust()) != 0)
        return error;

      // calculates single indirect references index
      kSI = (clustInd-N_DIRECT-RPC)/RPC;

      // checks if direct reference cluster is not allocated, if not, allocates it
      refSI = p_cltSI->ref[kSI];
      if(refSI == NULL_CLUSTER)
      {
        // allocs new cluster
        if((error = soAllocDataCluster(&n_cluster)) != 0)
          return error;
        // loads it again
        pcn = p_sb->dzone_start + p_inode->i2 * BLOCKS_PER_CLUSTER;
        if((error = soLoadSngIndRefClust(pcn)) != 0)
          return error;
        p_cltSI = soGetSngIndRefClust();

        iflagD = 1;
        p_cltSI->ref[kSI] = n_cluster;
        refSI = n_cluster;
        p_inode->clucount++;

        // stores single ind refs cluster
        if((error = soStoreSngIndRefClust()) != 0)
          return error;

        // map new cluster
        if((error = soMapDCtoIn (nInode, n_cluster)) != 0)
          return error;
      }    

      // calculates direct references cluster phisical number
      pcn = p_sb->dzone_start + refSI * BLOCKS_PER_CLUSTER;
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_cltD = soGetDirRefClust();

      // if the direct reference cluster has been allocated, fills the references of that cluster with NULL_CLUSTER
      if(iflagD)
        for(i = 0; i < RPC; i++)
          p_cltD->ref[i] = NULL_CLUSTER;

      // saves dir refs cluster
      if((error = soStoreDirRefClust()) != 0)
        return error;

      // calculates index for direct references
      kD = clustInd-N_DIRECT-(RPC*(kSI+1));
      // checks if cluster is alredy allocated
      refD = p_cltD->ref[kD];
      if(refD != NULL_CLUSTER)
        return -EDCARDYIL;

      // allocs data cluster
      if((error = soAllocDataCluster(&n_cluster)) != 0)
        return error;

      // loads dir refs cluster again
      pcn = p_sb->dzone_start + refSI * BLOCKS_PER_CLUSTER;
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_cltD = soGetDirRefClust();

      p_cltD->ref[kD] = n_cluster;
      p_inode->clucount++;

      // stores direct references cluster
      if((error = soStoreDirRefClust()) != 0)
        return error;

      // maps new cluster
      if((error = soMapDCtoIn (nInode,n_cluster)) != 0)
        return error;

      // returns cluster reference
      *p_outVal = n_cluster;
      return 0;
  }
  else if (op == GET)
  {
     // checks if p_outVal is allocated
     if(p_outVal == NULL)
       return -EIO;

      // checks single indirect references data cluster
      if(p_inode->i2 == NULL_CLUSTER)
      {
        *p_outVal = NULL_CLUSTER;
        return 0;
      }

      // calculates phisical cluster number
      pcn = p_sb->dzone_start + p_inode->i2*BLOCKS_PER_CLUSTER;
      // loads single indirect references cluster
      if((error = soLoadSngIndRefClust(pcn)) != 0)
        return error;
      p_cltSI = soGetSngIndRefClust();

      // calculates single indirect reference index
      kSI = (clustInd-N_DIRECT-RPC)/RPC;
      // checks single indirect references cluster
      if(p_cltSI->ref[kSI] == NULL_CLUSTER)
      {
        *p_outVal = NULL_CLUSTER;
        return 0;
      }

      // calculates phisical cluster number
      pcn = p_sb->dzone_start + (p_cltSI->ref[kSI] * BLOCKS_PER_CLUSTER);
      // loads sdirect references data cluster
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_cltD = soGetDirRefClust();

      // calculates direct reference index
      kD = clustInd-N_DIRECT-(RPC*(kSI+1));
      // checks data cluster
      if(p_cltD->ref[kD] == NULL_CLUSTER)
      {
        *p_outVal = NULL_CLUSTER;
        return 0;
      }

      // return data cluster reference
      *p_outVal = p_cltD->ref[kD];
  }
  else
  {
      // checks if single indirect references cluster is allocated
      if(p_inode->i2 == NULL_CLUSTER)
        return -EDCNOTIL;

      // calculates phisical number single indirect references cluster
      pcn = p_sb->dzone_start + p_inode->i2 * BLOCKS_PER_CLUSTER;
      // loads single indirect references cluster
      if((error = soLoadSngIndRefClust(pcn)) != 0)
        return error;
      p_cltSI = soGetSngIndRefClust();

      // calculates single indirect references index
      kSI = (clustInd-N_DIRECT-RPC)/RPC;
      // checks if direct references cluster is allocated
      if(p_cltSI->ref[kSI] == NULL_CLUSTER)
        return -EDCNOTIL;

      // calculates phisical number of direct references cluster
      pcn = p_sb->dzone_start + p_cltSI->ref[kSI] * BLOCKS_PER_CLUSTER;
      // loads direct references cluster
      if((error = soLoadDirRefClust(pcn)) != 0)
        return error;
      p_cltD = soGetDirRefClust();

      // calculates direct references index
      kD = clustInd-N_DIRECT-(RPC*(kSI+1));
      // checks if the cluster is allocated
      if(p_cltD->ref[kD] == NULL_CLUSTER)
        return -EDCNOTIL;

      if(op == FREE || op == FREE_CLEAN)
      {
        // frees data cluster
        if((error = soFreeDataCluster(p_cltD->ref[kD])) != 0)
          return error;
      }

      if(op == CLEAN || op == FREE_CLEAN)
      {
        // clean the cluster
        // unmaps cluster cluster
        if((error = soUnmapDCtoIn(nInode, p_cltD->ref[kD])) != 0)
          return error;
        p_cltD->ref[kD] = NULL_CLUSTER;
        p_inode->clucount--;

	// stores changes to direct refs cluster
        if((error = soStoreDirRefClust()) != 0)
          return error;

        // checks if direct references cluster has 0 references
        iflagD = 1;
        for(i = 0; i < RPC; i++)
          if(p_cltD->ref[i] != NULL_CLUSTER)
            iflagD = 0;
	
        // if direct references cluster has 0 references, cleans it
        if(iflagD)
        {
 	  // frees the unused ref cluster
          if((error = soFreeDataCluster(p_cltSI->ref[kSI])) != 0)
            return error;
          // unmaps cluster from inode
          if((error = soUnmapDCtoIn(nInode, p_cltSI->ref[kSI])) != 0)
            return error;
	  // clean the cluster
	  p_cltSI->ref[kSI] = NULL_CLUSTER;
          // decrement inode cluster count
          p_inode->clucount--;
          // stores changes to ind refs cluster
          if((error = soStoreSngIndRefClust()) != 0)
            return error;

        }
        pcn = p_sb->dzone_start + p_inode->i2 * BLOCKS_PER_CLUSTER;
        if((error = soLoadSngIndRefClust(pcn)) != 0)
          return error;
        p_cltSI = soGetSngIndRefClust();

        // checks if single indirect references cluster has 0 references
        iflagSI = 1;
        for(i = 0; i < RPC; i++)
          if(p_cltSI->ref[i] != NULL_CLUSTER)
            iflagSI = 0;

        // if cluster has 0 references, cleans it
        if(iflagSI)
        {
	  // frees the unused reference cluster
	  if((error = soFreeDataCluster(p_inode->i2)) != 0)
	    return error;
          // unmaps the cluster from the inode
          if((error = soUnmapDCtoIn(nInode, p_inode->i2)) != 0)
	    return error;

	  // cleans the cluster
	  p_inode->i2 = NULL_CLUSTER;
	  // decrements number of cluster in inode
	  p_inode->clucount--;
	}
      }
      // returns null value
      p_outVal = NULL;
  }
  return 0;
}

/**
 *  \brief Associate the data cluster to the inode which describes the file.
 *
 *  \param nInode number of the inode associated to the file
 *  \param nClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soMapDCtoIn (uint32_t nInode, uint32_t nClust)
{
	int stat;
	SOSuperBlock *p_sb;
	uint32_t *p_Nclust, p_blk, p_off;

	if((stat = soLoadSuperBlock()) !=0)
		return stat;
	p_sb = soGetSuperBlock();
	if(nClust<1 || (nClust > p_sb-> dzone_total -1))
		return -EINVAL;
	if(nInode == 0 || nInode >= (p_sb->itotal))
		return -EINVAL;
	
	if((stat = soConvertRefCInMT(nClust, &p_blk, &p_off))!= 0)
		return stat;
	if((stat = soLoadBlockCTInMT(p_blk)) != 0)  
		return stat;
	
	p_Nclust = soGetBlockCTInMT();
	p_Nclust[p_off] = nInode;
	if((stat = soStoreBlockCTInMT()) != 0)
		return stat;

  	return 0;
}

/**
 *  \brief Dissociate the data cluster from any inode which describes the file.
 *
 *  \param nInode number of the inode associated to the file
 *  \param nClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soUnmapDCtoIn (uint32_t nInode, uint32_t nClust)
{
	int stat;
	SOSuperBlock *p_sb;
	uint32_t *p_Nclust, p_blk, p_off;

	if((stat = soLoadSuperBlock()) !=0)
		return stat;
	p_sb = soGetSuperBlock();
	
	if(nClust<1 || (nClust > p_sb-> dzone_total -1))
		return -EINVAL;

	if(nInode == 0 || nInode >= (p_sb->itotal))
		return -EINVAL;
	
	if((stat = soConvertRefCInMT(nClust, &p_blk, &p_off))!= 0)
		return stat;
	if((stat = soLoadBlockCTInMT(p_blk)) != 0)  
		return stat;

	p_Nclust = soGetBlockCTInMT();

	// verifica se tem ou nao um inode associado
	if (p_Nclust[p_off] != nInode)
		return -EDCMINVAL;

	p_Nclust[p_off] = NULL_INODE;

	if((stat = soStoreBlockCTInMT()) != 0)
		return stat;

        if((stat = soStoreSuperBlock()) != 0)
 		return stat;

  	return 0;
}
