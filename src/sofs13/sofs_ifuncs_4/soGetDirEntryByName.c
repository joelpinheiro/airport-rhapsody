/**
 *  \file soGetDirEntryByName.c (implementation file)
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

/**
 *  \brief Get an entry by name.
 *
 *  The directory contents, seen as an array of directory entries, is parsed to find an entry whose name is
 *  <tt>eName</tt>. Thus, the inode associated to the directory must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must also be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'.
 *
 *  The process that calls the operation must have execution (x) permission on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be located
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the directory entry whose
 *                     name is passed, is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_idx pointer to the location where the index to the directory entry whose name is passed, or the index of
 *               the first entry that is free in the clean state, is to be stored
 *               (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>name</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx)
{
  soColorProbe (312, "07;31", "soGetDirEntryByName (%"PRIu32", \"%s\", %p, %p)\n",
                nInodeDir, eName, p_nInodeEnt, p_idx);

	int stat;
	int i,j;
	SOSuperBlock *p_sb;
	SOInode inode;
	SODataClust clust;
	uint32_t nInodeEnt=0,indice =-1;
	char name[MAX_NAME+1];
	char *base;

	//verificacao de eName
	
	if(eName == NULL)
		return -EINVAL;
	strcpy(name, eName);
	base = basename(name);
	if (strcmp(base, eName) != 0) {
		return -EINVAL;
	}	
	if(strlen(eName) == 0)
		return -EINVAL;
		
	if(strlen(eName)>MAX_NAME)
		return -ENAMETOOLONG;
		
	if(p_nInodeEnt == NULL)
		p_nInodeEnt = &nInodeEnt;
	
	//carregar o superbloco
	if((stat = soLoadSuperBlock()))
		return stat;
		
	if((p_sb = soGetSuperBlock()) == NULL)
		return -ELIBBAD;
		
	//verificar se nInode esta dentro dos limites
	if(nInodeDir >= p_sb->itotal)
		return -EINVAL;

	if((stat = soReadInode(&inode,nInodeDir,IUIN)) != 0)
		return stat;
		
	//verifica se o inode é um directorio
	if((inode.mode & INODE_TYPE_MASK) != INODE_DIR)
		return -ENOTDIR;
		
	//verifica se tem permissao de execuçao
	if((stat = soAccessGranted(nInodeDir,X)) != 0)
		return stat;
	
	
	//validar a consistencia
	if((stat = soQCheckDirCont(p_sb,&inode)) != 0)
		return stat;
		
	
	for(i = 0; i<(inode.size/(DPC*sizeof(SODirEntry))); i++)
	{
		//le o cluster
		if((stat = soReadFileCluster(nInodeDir,i,&clust)) != 0)
			return stat;
			
		//percorre todas as referencias
		for(j=0; j<DPC; j++)
		{
			if(clust.de[j].name[0] == '\0' && clust.de[j].name[1] == '\0')
			{
				if(indice == -1)
					indice = (i * DPC) + j;
			}
			else if(strcmp((const char*) clust.de[j].name, eName) == 0)
			{
				if(p_nInodeEnt != NULL)
					*p_nInodeEnt = clust.de[j].nInode;
				if(p_idx != NULL)
					*p_idx = (i * DPC) +j ;
				return 0;
			}
			

			
		
		}
	}
	
	if(p_nInodeEnt != NULL)
		*p_nInodeEnt = NULL_INODE;
	if(p_idx != NULL){
	
		if(indice == -1)
			*p_idx = j;
		else
			*p_idx = indice;

	}
	return -ENOENT;
	}
