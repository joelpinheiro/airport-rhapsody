/**
 *  \file soGetDirEntryByPath.c (implementation file)
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

/* Allusion to internal function */

static int soTraversePath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt);

/** \brief Number of symbolic links in the path */

static uint32_t nSymLinks = 0;

/** \brief Old directory inode number */

static uint32_t oldNInodeDir = 0;

/**
 *  \brief Get an entry by path.
 *
 *  The directory hierarchy of the file system is traversed to find an entry whose name is the rightmost component of
 *  <tt>ePath</tt>. The path is supposed to be absolute and each component of <tt>ePath</tt>, with the exception of the
 *  rightmost one, should be a directory name or symbolic link name to a path.
 *
 *  The process that calls the operation must have execution (x) permission on all the components of the path with
 *  exception of the rightmost one.
 *
 *  \param ePath pointer to the string holding the name of the path
 *  \param p_nInodeDir pointer to the location where the number of the inode associated to the directory that holds the
 *                     entry is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the entry is to be stored
 *                     (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL
 *  \return -\c ENAMETOOLONG, if the path or any of the path components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative and it is not a symbolic link
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soGetDirEntryByPath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt)
{
  soColorProbe (311, "07;31", "soGetDirEntryByPath (\"%s\", %p, %p)\n", ePath, p_nInodeDir, p_nInodeDir);

  int err;
  uint32_t nInodeDir;				// directory
  uint32_t nInodeEnt;				// entry

  /* checks if the pointer to the string with the path is null */
  if (ePath == NULL)
	  return -EINVAL;

  /* Checks if the ePath exceeds the MAX_Path */
  if (strlen(ePath) > MAX_PATH)
	  return -ENAMETOOLONG;

  nSymLinks = 0;

  /* Updates the iNode */
  if ((err = soTraversePath(ePath, &nInodeDir, &nInodeEnt)) != 0)
	  return err;

  if (p_nInodeDir != NULL)
	  *p_nInodeDir = nInodeDir;

  if (p_nInodeEnt != NULL)
	  *p_nInodeEnt = nInodeEnt;

  return 0;
}

/**
 *  \brief Traverse the path.
 *
 *  \param ePath pointer to the string holding the name of the path
 *  \param p_nInodeDir pointer to the location where the number of the inode associated to the directory that holds the
 *                     entry is to be stored
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the entry is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ENAMETOOLONG, if any of the path components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative and it is not a symbolic link
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soTraversePath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt)
{
	int err;
	char *p_name;						// directory name
	char path[MAX_PATH +1];				// maximum size of the directory
	char *p_path;						// path of the directory
	char name[MAX_PATH +1];				// maximum size of the directory name
	char symPath[BSLPC];				// path of the SymLink

	SOInode inode;
	uint32_t nInodeEnt;
	uint32_t nInodeDir;

	strncpy((char *)path, (char *)ePath, MAX_PATH + 1);
	p_path = dirname(path);
	strncpy((char *) name, (char *) ePath, MAX_PATH +1);
	p_name = basename(name);


	/* checks if the name don't pass the maxSize */
	if (strlen(p_name) > MAX_NAME)
		return -ENAMETOOLONG;

	/* checks if is a relative path */
	if ((nSymLinks == 0) && (p_path[0] != '/') )
		return -ERELPATH;

	/* checks if '/' was replaced by '.'   */
	if (strcmp(p_name, "/") == 0)
		strcpy(p_name, ".");

	/* checks if the path is root   */
	if (strcmp(p_path, "/") == 0)
		nInodeDir = 0;

	/* the nInodeDir value changes to the last nInodeDir   */
	else if (strcmp(p_path, ".") == 0)
		nInodeDir = oldNInodeDir;

	/* update the iNode   */
	else
	{
		if ((err = soTraversePath(p_path, &nInodeDir, &nInodeEnt) ) != 0)
			return err;

		nInodeDir = nInodeEnt;
	}

	/* reads the iNode with the entry */
	if ((err = soReadInode(&inode, nInodeDir, IUIN) ) != 0)
		return err;

	/* check for permission to execute */
	if ((err = soAccessGranted(nInodeDir,X) ) !=0 )
		return err;

	/* saves the iNode number with the current directory */
	if ((err = soGetDirEntryByName(nInodeDir, p_name, &nInodeEnt, NULL) ) != 0)
		return err;

	/* reads the iNode on the entry  */
	if ((err = soReadInode(&inode, nInodeEnt, IUIN) ) != 0)
		return err;

	/* checks if the file is SYMLINK  */
	if ((inode.mode & INODE_SYMLINK) == INODE_SYMLINK)
	{
		if (nSymLinks >= 1)
			return -ELOOP;

		nSymLinks++;

		/* check for permission to execute */
		if ((err = soAccessGranted(nInodeEnt, X) ) != 0)
			return err;

		/* read the path of SYMLINK  */
		if ((err = soReadFileCluster(nInodeEnt, 0, symPath) ) != 0)
			return err;

		/* path can't be greater than max size */
		if (strlen(symPath) > MAX_PATH)
			return -ENAMETOOLONG;

		oldNInodeDir = nInodeDir;

		if ((err = soTraversePath(symPath, &nInodeDir, &nInodeEnt) ) != 0)
			return err;
	}

	*p_nInodeEnt = nInodeEnt;
	*p_nInodeDir = nInodeDir;

	return 0;
}
