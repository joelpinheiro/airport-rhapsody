

    /**
     *  \file soAllocInode.c (implementation file)
     *
     *  \author Joel Pinheiro
     */

    #include <stdio.h>
    #include <errno.h>
    #include <inttypes.h>
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

    /**
     *  \brief Allocate a free inode.
     *
     *  The inode is retrieved from the list of free inodes, marked in use, associated to the legal file type passed as
     *  a parameter and generally initialized.  It must be free and if is free in the dirty state, it has to be cleaned
     *  first.
     *
     *  Upon initialization, the new inode has:
     *     \li the field mode set to the given type, while the free flag and the permissions are reset
     *     \li the owner and group fields set to current userid and groupid
     *     \li the <em>prev</em> and <em>next</em> fields, pointers in the double-linked list of free inodes, change their
     *         meaning: they are replaced by the <em>time of last file modification</em> and <em>time of last file
     *         access</em> which are set to current time
     *     \li the reference fields set to NULL_CLUSTER
     *     \li all other fields reset.

     *  \param type the inode type (it must represent either a regular file, or a directory, or a symbolic link)
     *  \param p_nInode pointer to the location where the number of the just allocated inode is to be stored
     *
     *  \return <tt>0 (zero)</tt>, on success
     *  \return -\c EINVAL, if the <em>type</em> is illegal or the <em>pointer to inode number</em> is \c NULL
     *  \return -\c ENOSPC, if the list of free inodes is empty
     *  \return -\c ESBTINPINVAL, if the table of inodes metadata in the superblock is inconsistent
     *  \return -\c ETINDLLINVAL, if the double-linked list of free inodes is inconsistent
     *  \return -\c EFININVAL, if a free inode is inconsistent
     *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
     *  \return -\c EBADF, if the device is not already opened
     *  \return -\c EIO, if it fails reading or writing
     *  \return -<em>other specific error</em> issued by \e lseek system call
     */

    int soAllocInode (uint32_t type, uint32_t* p_nInode)
    {
            soColorProbe (611, "07;31", "soAllocInode (%"PRIu32", %p)\n", type, p_nInode);

        	SOInode *array;
        	SOSuperBlock *p_sb;
        	uint32_t numBlock, offset, next;
        	int status, i;

            // Se o type não existe ou ponteiro p_nInode é nulo
        	if((type !=  INODE_DIR && type != INODE_FILE && type != INODE_SYMLINK) || p_nInode == NULL)
        		return -EINVAL;

        	// Superbloco na memória
        	if((status = soLoadSuperBlock()) != 0)
        		return status;

        	// Ponteiro do superbloco
        	if (!(p_sb = soGetSuperBlock()))
        		return -EIO;

        	// Tem de haver pelo menos um nó livre na lista de nós livres
        	if(p_sb->ifree == 0)
        	{
        		return -ENOSPC;
        	}

        	// Verifica se a tabela de inodes esta consistente
        	if ((status = soQCheckSuperBlock(p_sb)) != 0)
        	{
        		return status;
        	}
        	*p_nInode = p_sb->ihead;

        	// Obter nº bloco e o offset do inode
        	if ((status = soConvertRefInT(p_sb->ihead, &numBlock, &offset)))
        	{
        		return status;
        	}

        	// Ler o bloco do primeiro nó livre
        	if ((status = soLoadBlockInT(numBlock)))
        	{
        		return status;
        	}

        	array = soGetBlockInT();
        	next = array[offset].vD2.next;

        	// Preenchimento
        	array[offset].mode = type | 0x0;
        	array[offset].refcount = 0;
        	array[offset].owner = getuid();
        	array[offset].group = getgid();
        	array[offset].size = 0;
        	array[offset].clucount = 0;

        	for (i = 0; i < N_DIRECT; i++)
        	{
        		array[offset].d[i] = NULL_CLUSTER;
        	}

        	array[offset].i1 = NULL_CLUSTER;
        	array[offset].i2 = NULL_CLUSTER;

        	array[offset].vD1.atime = time(NULL);
        	array[offset].vD2.mtime = time(NULL);

        	// Algoritmo
        	if (p_sb->ifree == 1)
        	{
            	// A lista só tem um elemento
        		p_sb->ihead = NULL_INODE;
        		p_sb->itail = NULL_INODE;
        		if ((status = soStoreBlockInT()) != 0)
        		{
        			return status;
        		}
        	}
        	else{
        		// A lista tem dois ou mais elementos

        		// Actualiza ihead do bloco
        		p_sb->ihead = next;

        		// Guarda o inode
        		if ((status = soStoreBlockInT()) != 0)
        		{
        			return status;
        		}
        		// Nº Bloco e Offset inode head
        		if ((status = soConvertRefInT (next, &numBlock, &offset)) != 0)
        		{
        			return status;
        		}
        		// Como a lista não está vazia, vamos alterar o ihead
        		if ((status = soLoadBlockInT(numBlock)) != 0)
        		{
        			return status;
        		}
        		array = soGetBlockInT();
        		array[offset].vD1.prev = NULL_INODE;

        		// Guarda o inode
        		if ((status = soStoreBlockInT()) != 0)
        		{
        			return status;
        		}
        	}

        	// Decrementa o numero de free inodes
        	p_sb->ifree--;

        	// Guarda inode
        	if ((status = soStoreSuperBlock()) != 0)
        	{
        		return status;
        	}
        return 0;
    }


