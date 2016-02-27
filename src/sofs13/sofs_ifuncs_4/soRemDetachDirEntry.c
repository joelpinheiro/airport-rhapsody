/**
 *  \file soAddAttDirEntry.c (implementation file)
 *
 *  \author Joel Pinheiro
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

/* Allusion to external functions */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);
int soCheckDirectoryEmptiness (uint32_t nInodeDir);

/** \brief operation remove a generic entry from a directory */
#define REM         0
/** \brief operation detach a generic entry from a directory */
#define DETACH      1

/**
 *  \brief Remove / detach a generic entry from a directory.
 *
 *  The entry whose name is <tt>eName</tt> is removed / detached from the directory associated with the inode whose
 *  number is <tt>nInodeDir</tt>. Thus, the inode must be in use and belong to the directory type.
 *
 *  Removal of a directory entry means exchanging the first and the last characters of the field <em>name</em>.
 *  Detachment of a directory entry means filling all the characters of the field <em>name</em> with the \c NULL
 *  character and making the field <em>nInode</em> equal to \c NULL_INODE.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should exist an entry in the directory whose <em>name</em> field is <tt>eName</tt>.
 *
 *  Whenever the operation is removal and the type of the inode associated to the entry to be removed is of directory
 *  type, the operation can only be carried out if the directory is empty.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be removed / detached and, when required, of
 *  the inode associated to the directory are updated.
 *
 *  The file described by the inode associated to the entry to be removed / detached is only deleted from the file
 *  system if the <em>refcount</em> field becomes zero (there are no more hard links associated to it) and the operation
 *  is removal. In this case, the data clusters that store the file contents and the inode itself must be freed.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be removed / detached
 *  \param op type of operation (REM / DETACH)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name or no operation of the defined class is described
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>eName</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c ENOTEMPTY, if the entry with <tt>eName</tt> describes a non-empty directory
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRemDetachDirEntry (uint32_t nInodeDir, const char *eName, uint32_t op)
{
  soColorProbe (314, "07;31", "soRemDetachDirEntry (%"PRIu32", \"%s\", %"PRIu32")\n", nInodeDir, eName, op);

                          /* insert your code here */

  /*
   * Validação de conformidade
• os números dos nós-i associados ao directório de remoção / anulação e à en-
trada têm que ser válidos e o primeiro tem que estar efectivamente associado a
um ficheiro onde o processo que invoca a operação tem permissões de execu-
ção e de escrita

• o ponteiro para o string associado ao campo name da entrada a remover /
anular não pode ser nulo, nem representar um string que não seja um nome de
ficheiro válido

• tem que existir no directório uma entrada cujo campo name seja igual ao
nome do string que é passado como parâmetro de entrada

• se a entrada a remover corresponder a um directório, este tem que estar vazio

• só são permitidas operações de remoção ou de anulação.
   * */

	uint32_t nInodeEnt, index;
	uint32_t error, i, k;
	SOInode inodeDir, inodeEnt;
    SODataClust cluster, cluster1;

	// nao e possivel remover entradas ("." and "..") no dir
	if(!strcmp(eName,".") || !strcmp(eName,".."))
		return -EPERM;

	// le o nInodeDir para a memória inodeDir
	if((error = soReadInode(&inodeDir, nInodeDir, op)))
		return error;

	// verificamos se inodeDir é um directorio
	if((inodeDir.mode & INODE_TYPE_MASK) != INODE_DIR)
		return -ENOTDIR;

    // permissoes de execução e escrita
    if((error = soAccessGranted(nInodeDir, W)))
		return error;
    if((error = soAccessGranted(nInodeDir, X)))
		return error;

	if((error = soGetDirEntryByName(nInodeDir, eName, &nInodeEnt, &index)))
		return error;

    // verifica se o eName existe na directoria
    if(nInodeEnt == NULL_INODE)
    		return -ENOENT;

	i = index/DPC;
	index=index%DPC;

	if((error = soReadFileCluster (nInodeDir, (uint32_t) i, cluster.de)))
		return error;

	if((error = soReadInode(&inodeEnt, nInodeEnt, op)))
		return error;

	if(op == REM)	// REMOVER
	{
		if(inodeEnt.mode & INODE_DIR)
			if(soCheckDirectoryEmptiness(nInodeEnt))
				return -ENOTEMPTY;

		// troca o último caracter com o primeiro
		cluster.de[index].name[MAX_NAME] = cluster.de[index].name[0];
		cluster.de[index].name[0] = '\0';

	}
    else if(op == DETACH) 	// DETEACH
    {
		for(k=0; k<=MAX_NAME; k++)
			cluster.de[index].name[k] = '\0';
		cluster.de[index].nInode=NULL_CLUSTER;

		if(inodeEnt.mode & INODE_DIR)
		{
			if((error = soReadFileCluster(nInodeEnt, 0, cluster1.de)))
				return error;

			// remover o ".."
			for(k=0; k<2; k++)
				cluster1.de[1].name[k] = '\0';
			cluster1.de[1].nInode=NULL_CLUSTER;
		}
	}

	inodeEnt.refcount--;
    if(inodeEnt.mode & INODE_DIR)
		inodeDir.refcount--;
	if((inodeEnt.mode & INODE_DIR) && op==REM)
		inodeEnt.refcount--;

    soWriteInode(&inodeEnt, nInodeEnt, op);
    soWriteInode(&inodeDir, nInodeDir, op);

    if(op==REM && (inodeEnt.refcount == 0 || (inodeEnt.refcount==1 && (inodeEnt.mode & INODE_DIR))))
    {
		if((error=soHandleFileClusters(nInodeEnt, 0, op)))
			return error;
		if((error=soFreeInode(nInodeEnt)))
			return error;
    }

    if((error = soWriteFileCluster (nInodeDir, (uint32_t) i, cluster.de)))
		return error;

  return 0;


  return -ENOSYS;
}
