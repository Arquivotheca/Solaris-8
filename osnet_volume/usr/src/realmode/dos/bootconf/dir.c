/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * dir.c -- directory routines
 */

#ident	"<@(#)dir.c	1.8	97/04/30 SMI>"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dos.h>
#include <errno.h>

#include "debug.h"
#include "dir.h"

/*
 * opendir -- Open directory for reading:
 *
 * Allocates a buffer to hold the directory search state required by
 * DOS and returns its address to the caller.  This becomes the handle
 * for subsequent readdir() operations.
 *
 * Returns a null pointer (with "errno" set) if something goes wrong.
 */

DIR *
opendir(const char *pn)
{

	int x;
	DIR *dp = 0;
	struct _stat buf;

	if (((x = _stat(pn, &buf)) != 0) ||
	    ((buf.st_mode & _S_IFMT) != _S_IFDIR)) {
		/*
		 * Either the file does not exist or it's not a directory.
		 * The stat() routine sets "errno" in the former case, we
		 * take care of the latter here.
		 */
		if (!x)
			errno = ENOTDIR;

	} else if (!(dp = (DIR *)malloc(sizeof (DIR) + strlen(pn) + 5))) {
		/*
		 * Can't get memory for the directory state buffer.
		 * Set "errno" to indicate low memory condition.
		 */
		errno = ENOMEM;
	} else {
		/*
		 * We've got a state buffer.  Initialize the entry offset
		 * to zero and copy the path name in for FindFirst (see
		 * readdir, below).  Also, make sure to remove any trailing
		 * backslashes before appending the *.*!
		 */
		char *cp;
		dp->de.d_off = 0;

		strncpy(cp = dp->dir, pn, PATH_MAX - 5);
		cp += strlen(dp->dir);
		while (*--cp == '\\')
			;
		strcpy(cp+1, "\\*.*");
	}

	return (dp);
}

/*
 * readdir -- Read next directory entry:
 *
 * This routine uses "FindFirst" or "FindNext" to locate the next entry
 * in the DOS directory open on "dp".  It converts the directory name
 * into the form expected of POSIX programs and returns a pointer to the
 * "dirent" structure we're keeping in the DIR struct.
 *
 * Returns a null pointer when we reach the end of the directory.
 */

struct dirent *
readdir(DIR *dp)
{
	if (dp->de.d_off <= 0) {
		/*
		 * If we're just starting, use the DOS "FindFirst" function
		 * to locate the first directory entry to be returned to the
		 * caller.
		 */
		if (_dos_findfirst(dp->dir, _A_RDONLY | _A_SYSTEM | _A_SUBDIR,
		    &dp->f) != 0)
			return (NULL);
	} else {
		/*
		 * Search is already in progress, use the DOS "FindNext"
		 * function to locate the next entry.
		 */
		if (_dos_findnext(&dp->f) != 0)
			return (NULL);
	}
	/* d_off turns out to be the ordinal number of the file */
	dp->de.d_off++;
	dp->de.d_name = dp->f.name;
	return (&dp->de);
}

#ifndef __lint
/*
 * XXX currently no one is using this function. i was going to remove
 * rewinddir for lints sake but, someone indicated that rewinddir() might
 * be useful in the future so it's being left in so that lint won't see it.
 */
/*
 * rewinddir -- Reset directory search:
 *
 * Resets the search of the directory open on "dp" to the beginning of
 * the directory by simply clearing the directory offset counter.
 */

void
rewinddir(DIR *dp)
{
	dp->de.d_off = 0;
}
#endif

/*
 * closedir -- Close directory:
 *
 * All we need to here is free up the work buffer that we allocated
 * to hold intermediate search state.
 */

void
closedir(DIR *dp)
{

	free(dp);
}
