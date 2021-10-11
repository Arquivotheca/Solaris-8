/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pathfind.c	1.12	97/03/28 SMI"	/* SVr4.0 1.2.5.3 */

/*LINTLIBRARY*/

#pragma weak pathfind = _pathfind

/*
 * Search the specified path for a file with the specified
 * mode and type.  Return a pointer to the path.  If the
 * file isn't found, return NULL.
 */

#include "synonyms.h"
#ifdef _REENTRANT
#include "mtlib.h"
#include <thread.h>
#endif /* _REENTRANT */
#include <sys/types.h>
#include <libgen.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Mode bit definitions -- see mknod(2)
 * Names of flags duplicate those of test(1)
 */


/* File type: 0170000 */
#define	FFLAG	S_IFREG		/* normal file - also type 0 */
#define	BFLAG	S_IFBLK		/* block special */
#define	CFLAG	S_IFCHR		/* character special */
#define	DFLAG	S_IFDIR		/* directory */
#define	PFLAG	S_IFIFO		/* fifo */

#define	UFLAG	S_ISUID		/* setuid */
#define	GFLAG	S_ISGID		/* setgid */
#define	KFLAG	S_ISVTX		/* sticky bit */

/*
 * Perms: 0700 user, 070 group, 07 other
 * Note that pathfind uses access(2), so no need to hassle
 * with shifts and such
 */
#define	RFLAG	04		/* read */
#define	WFLAG	02		/* write */
#define	XFLAG	01		/* execute */

static int fullck(char *, mode_t, int);

#ifdef _REENTRANT
static char *
_get_cpath(thread_key_t *key)
{
	char *str = NULL;

	if (_thr_getspecific(*key, (void **)&str) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (NULL);
		}
	}
	if (!str) {
		if (_thr_setspecific(*key, (void *)(str = calloc(PATH_MAX,
			sizeof (char)))) != 0) {
			if (str)
				(void) free(str);
			return (NULL);
		}
	}
	return (str);
}
#endif /* _REENTRANT */

char *
pathfind(const char *path, const char *name, const char *mode)
{
#ifdef _REENTRANT
	static thread_key_t key = 0;
	char *cpath = _get_cpath(&key);
#else /* _REENTRANT */
	static char cpath[PATH_MAX];
#endif /* _REENTRANT */
	char *cp;
	mode_t imode;
	int nzflag;

	/* Build imode */
	imode = 0; nzflag = 0;
	if (mode == ((char *)0))
		mode = "";
	for (cp = (char *)mode; *cp; cp++) {
		switch (*cp) {
		case 'r':
			imode |= RFLAG;
			break;
		case 'w':
			imode |= WFLAG;
			break;
		case 'x':
			imode |= XFLAG;
			break;
		case 'b':
			imode |= BFLAG;
			break;
		case 'c':
			imode |= CFLAG;
			break;
		case 'd':
			imode |= DFLAG;
			break;
		case 'f':
			imode |= FFLAG;
			break;
		case 'p':
			imode |= PFLAG;
			break;
		case 'u':
			imode |= UFLAG;
			break;
		case 'g':
			imode |= GFLAG;
			break;
		case 'k':
			imode |= KFLAG;
			break;
		case 's':
			nzflag = 1;
			break;
		default:
			return ((char *)0);
		}
	}

	if (name[0] == '/' || path == ((char *)0) || *path == '\0')
		path = ":";
	while (*path) {
		for (cp = cpath; (/* const */ char *) cp <
			&cpath[PATH_MAX] && (*cp = *path); cp++) {
			path++;
			if (*cp == ':')
				break;
		}
		if ((/* const */ char *) cp + strlen(name) + 2 >=
			&cpath[PATH_MAX])
			continue;
		if (cp != cpath)
			*cp++ = '/';
		*cp = '\0';
		(void) strcat(cp, name);
		if (access(cpath, imode&07) == 0 &&
			fullck(cpath, imode, nzflag))
			return (cpath);
	}

	return ((char *)0);
}

static
fullck(char *name, mode_t mode, int nzflag)
{
	struct stat64 sbuf;
	int xor;

	if ((mode & 0177000) == 0 && nzflag == 0) /* no special info wanted */
		return (1);
	if (stat64(name, &sbuf) == -1)
		return (0);
	xor = (sbuf.st_mode ^ mode) & 077000;	/* see mknod(2) */
	if (mode & 0170000 == 0)
		xor &= ~070000;
	if ((mode & 07000) == 0)
		xor &= ~07000;
	if (xor)
		return (0);
	if (nzflag && sbuf.st_size == 0)
		return (0);
	return (1);
}
