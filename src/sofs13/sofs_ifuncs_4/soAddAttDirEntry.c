/**
 *  \file soAddAttDirEntry.c (implementation file)
 *
 *  \author
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"

/* Allusion to external function */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);

/** \brief operation add a generic entry to a directory */
#define ADD         0
/** \brief operation attach an entry to a directory to a directory */
#define ATTACH      1

/**
 *  \brief Add a generic entry / attach an entry to a directory to a directory.
 *
 *  In the first case, a generic entry whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt> is added
 *  to the directory associated with the inode whose number is <tt>nInodeDir</tt>. Thus, both inodes must be in use and
 *  belong to a legal type, the former, and to the directory type, the latter.
 *
 *  Whenever the type of the inode associated to the entry to be added is of directory type, the directory is initialized
 *  by setting its contents to represent an empty directory.
 *
 *  In the second case, an entry to a directory whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt>
 *  is attached to the directory, the so called <em>base directory</em>, associated to the inode whose number is
 *  <tt>nInodeDir</tt>. The entry to be attached is supposed to represent itself a fully organized directory, the so
 *  called <em>subsidiary directory</em>. Thus, both inodes must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should not already be any entry in the directory whose <em>name</em> field is
 *  <tt>eName</tt>.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be added / updated and, when required, of the
 *  inode associated to the directory are updated. This may also happen to the <em>size</em> field of either or both
 *  inodes.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the entry to be added / attached
 *  \param nInodeEnt number of the inode associated to the entry to be added / attached
 *  \param op type of operation (ADD / ATTACH)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the <em>inode numbers</em> are out of range or the pointer to the string is \c NULL
 *                      or the name string does not describe a file name or no operation of the defined class is described
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> (ADD), or both the inode types (ATTACH),
 *                       are not directories
 *  \return -\c EEXIST, if an entry with the <tt>eName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory where the
 *                      entry is to be added / attached
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory where the entry
 *                     is to be added / attached
 *  \return -\c EMLINK, if the maximum number of hardlinks in either one of inodes has already been attained
 *  \return -\c EFBIG, if the directory where the entry is to be added / attached, has already grown to its maximum size
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCMINVAL, if the mapping association of the data cluster is invalid
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAddAttDirEntry (uint32_t nInodeDir, const char *eName, uint32_t nInodeEnt, uint32_t op)
{
  soColorProbe (313, "07;31", "soAddAttDirEntry (%"PRIu32", \"%s\", %"PRIu32", %"PRIu32")\n", nInodeDir,
                eName, nInodeEnt, op);

  	int estado, i, j;
  	SOInode inodedir, inodeent;
  	uint32_t indice;
  	SODataClust clust1, clust2;
  	SOSuperBlock *p_sb;
	int indicecluster,idir;

  	// SuperBlock
  	if((estado = soLoadSuperBlock()))
  		return estado;
  	p_sb = soGetSuperBlock();

  	// Check nInode
  	if(nInodeDir<0 || nInodeDir >= p_sb->itotal || nInodeEnt<0 || nInodeEnt >= p_sb->itotal)
  		return -EINVAL;

  	// Check eName
	if (eName == NULL) 
		return -EINVAL;

  	// Check eName length
  	if(strlen(eName)>MAX_NAME)
  		return -ENAMETOOLONG;

	if((eName == ".") || (eName == "..")){
		return -EINVAL;
	}

	for(j=0;j<strlen(eName);j++){
		if(eName[j] == '/')
			return -EINVAL;
	}

  	// Read Parent Dir
  	if((estado = soReadInode(&inodedir, nInodeDir, IUIN))!=0)
  		return estado;

 	if(op!=ADD && op!=ATTACH)
  	{
  		  return -EINVAL;
  	}
  	// Check Permits
  	if((estado = soAccessGranted(nInodeDir, X))!=0)
  		return estado;
  	estado = soAccessGranted(nInodeDir, W);
	if(estado == -EACCES)
  		  return -EPERM;

  	else if(estado != 0)
  		  return estado;

  	// Check if Parent Dir is actually a Dir
  	if((inodedir.mode & INODE_TYPE_MASK) != INODE_DIR)
  		return -ENOTDIR;

  	// Check Space
  	if((inodedir.size == MAX_FILE_SIZE))
  		return -EFBIG;

  	// Read Entry to be Added/Attached
  	if((estado = soReadInode(&inodeent, nInodeEnt, IUIN))!= 0)
  		return estado;

  	// Check RefCount limit
  	if((inodedir.refcount == 65535) || (inodeent.refcount == 65535))
  		return -EMLINK;

  	// Make sure there's no entry with eName
  	if(soGetDirEntryByName(nInodeDir, eName, NULL, &indice)!= -ENOENT)
  		return -EEXIST;

  	// Get Cluster blk/index values
  	indicecluster = indice / DPC;
  	idir = indice % DPC;

  	// If Add Operation
  	if(op == ADD)
  	{
  		// If we're adding a Dir
  		if((inodeent.mode & INODE_TYPE_MASK)== INODE_DIR)
  		{

  			// Write '.' and '..'
			clust1.de[0].nInode = nInodeEnt;
			clust1.de[1].nInode = nInodeDir;
			strncpy((char *) clust1.de[1].name, "..",MAX_NAME+1);
			strncpy((char *) clust1.de[0].name, ".",MAX_NAME+1);

			/*  */
  			for(j=2; j<DPC; j++){
					memset(clust1.de[j].name,'\0',MAX_NAME+1);
  					clust1.de[j].nInode = NULL_INODE;
			}

  	

  			// Update Entry iNode

  			if((estado = soWriteFileCluster(nInodeEnt, 0, &clust1))!=0)
  				return estado;

  			if((estado = soReadInode(&inodeent, nInodeEnt, IUIN))!=0)
  				return estado;

  			// Update parent folder iNode
  			inodedir.refcount++;
  			inodeent.size = BSLPC;
  			inodeent.refcount+=2;
  			// Write Cluster

  		}
  		// If we are adding a FILE/LINK
  		else if(((inodeent.mode & INODE_TYPE_MASK)== INODE_SYMLINK) || ((inodeent.mode & INODE_TYPE_MASK)== INODE_FILE))
  		{
  			// Update Entry iNode
  			inodeent.refcount++;

  			if((estado = soWriteInode(&inodeent, nInodeEnt, IUIN))!=0)
  				return estado;
  		}
  	}
  	// If Attach Operation
  	else if(op == ATTACH)
  	{
  		// Check if we're adding a folder
  		if(((inodeent.mode & INODE_TYPE_MASK) == INODE_DIR))
  			return -EIUININVAL;

		if (inodeent.size == 0)
              			  return -1;

  		if((estado = soReadFileCluster(nInodeEnt, 0, &clust1))!=0)
  			return estado;

  		clust1.de[1].nInode = nInodeDir;

  		// Write Cluster
  		if((estado = soWriteFileCluster(nInodeEnt, 0, &clust1))!=0)
  			return estado;

		if((estado =soReadInode(&inodeent, nInodeEnt, IUIN))!=0)
			return estado;
		inodedir.refcount++;
		inodeent.refcount+=2;

		if((estado = soWriteInode(&inodedir,nInodeDir,IUIN))!=0)
			return estado;

		if((estado = soWriteInode(&inodeent, nInodeEnt,IUIN))!=0)
			return estado;


  	}
  	// If invalid operation
  	else
  		return -EINVAL;

	if((estado = soReadFileCluster(nInodeDir, indicecluster, &clust2))!=0)
		return estado;
 
	if(idir == 0){
		for(j =1; j < DPC; j++){
			memset(clust2.de[j].name, '\0', MAX_NAME+1);		
			clust2.de[j].nInode = NULL_INODE;
		}
		inodedir.size = inodedir.size + BSLPC;
	}

	strncpy((char *) clust2.de[idir].name,eName,MAX_NAME+1);
	clust2.de[idir].nInode = nInodeEnt;

	if((estado = soWriteInode(&inodedir, nInodeDir, IUIN))!=0)
		return estado;

	if((estado = soWriteFileCluster(nInodeDir, indicecluster, &clust2))!=0)
		return estado;	

	if((estado = soWriteInode(&inodeent, nInodeEnt,IUIN))!=0)
		return estado;

    return 0;
}
