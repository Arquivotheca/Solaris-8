/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *	POSIX directory search routines:
 *
 *	  This file contains source for opendir(), closedir(), readdir(), and
 *	  rewinddir() for DOS.  See function descriptions in ISO/9945-1.
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)readdir.c	1.3	95/05/07 SMI\n"

#include <sys\types.h>
#include <sys\stat.h>

#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

DIR *opendir (const char *pn)
{	/*
	 *  Open directory for reading:
	 *
	 *  Allocates a buffer to hold the directory search state required by
	 *  DOS and returns its address to the caller.  This becomes the handle
	 *  for subsequent readdir() operations.
	 *
	 *  Returns a null pointer (with "errno" set) if something goes wrong.
	 */

	int x;
	DIR *dp = 0;
	struct stat buf;

	if ((x = stat(pn, &buf)) || ((buf.st_mode & _S_IFMT) != _S_IFDIR)) {
		/*
		 *  Either the file does not exist or it's not a directory.  The 
		 *  stat() routine sets "errno" in the former case, we take care of
		 *  the latter here.
		 */

		if (!x) errno = ENOTDIR;

	} else if (!(dp = (DIR *)malloc(sizeof(DIR) + strlen(pn) + 4))) {
		/*
		 *  Can't get memory for the directory state buffer.  Set "errno"
		 *  to indicate low memory condition.
		 */

		errno = ENOMEM;

	} else {
		/*
		 *  We've got a state buffer.  Initialize the entry offset to zero
		 *  and copy the path name in for FindFirst (see readdir, below).
		 *  Also, make sure to remove any trailing backslashes before appending
	     *  the *.*!
		 */

		char *cp;
		dp->de.d_off = 0;
		dp->de.d_name = &dp->dtn;

	    strcpy(cp = dp->dir, pn); cp += strlen(dp->dir);
	    while (*--cp == '\\'); strcpy(cp+1, "\\*.*");
	}

	return(dp);
}

struct dirent *
readdir (DIR *dp)
{	/*
	 *  Read next directory entry:
	 *
	 *  This routine uses "FindFirst" or "FindNext" to locate the next entry
	 *  in the DOS directory open on "dp".  It converts the directory name
	 *  into the form expected of POSIX programs and returns a pointer to the
	 *  "dirent" structure we're keeping in the DIR struct.
	 *
	 *  Returns a null pointer when we reach the end of the directory.
	 */

	int j = 0;
	char _far *dta;
	const int dtaoff = (int)&(((DIR *)0)->dta);
	const int namoff = (int)&(((DIR *)0)->dir);

	_asm {
		;/*
		; *  Set up disk transfer address.  We save the current DTA in the
		; *  "dta" register and use the dta buffer in the DIR struct as
	    ; *  the system DTA while we're reading the directory.
		; */

		push	bx		; Save scratch reg
		mov		ah,2Fh	; Issue "get DTA" syscall
		int		21h
		mov		word ptr [dta],bx
		mov		word ptr [dta+2],es
		mov		dx,dtaoff
		add		dx,dp
		mov		ah,1Ah
		int		21h		; Issue "set DTA" syscall
		pop		bx
	}

	if (dp->de.d_off <= 0) _asm {
		;/*
		; *  If we're just starting, use the DOS "FindFirst" function to locate
		; *  the first directory entry to be returned to the caller.
		; */

		mov		cx,16h		; We want all file types
		mov		dx,dp		; Directory name (with "*.*" appended) goes to
		add		dx,namoff	; .. "dx" register
		mov		ah,4Eh
		int		21h			; Issue the system call
		jnc		ok
		dec		j			; Error (probably EOF)

	} else _asm {
		;/*
		; *  Search is already in progress, use the DOS "FindNext" function
		; *  to locate the next entry.
		; */

		mov		ah,4Fh		; Issue DOS system call
		int		21h
		jnc		ok
		dec		j			; Error (probably EOF)
	ok:
	}

	_asm {
		;/*
		; *  Restore system's DTA pointer to its original value so caller
		; *  can perform I/O in a reasonable manner.
		; */

		push	ds		; Save data segment ptr
		mov 	dx,word ptr [dta] 
		mov		ds,word ptr [dta+2]	
		mov		ah,1Ah
		int 	21h		; Issue DOS system call
		pop		ds		; Restore data segment ptr
	}

	return(j ? 0 : (dp->de.d_off++, &dp->de));
}

void
rewinddir (DIR *dp)
{	/*
	 *  Reset directory search:
	 *
	 *  Resets the search of the directory open on "dp" to the beginning of
	 *  the directory by simply clearing the directory offset counter.
	 */

	dp->de.d_off = 0;
}

void
closedir (DIR *dp)
{	/*
	 *  Close directory:
	 *
	 *  All we need to here is free up the work buffer that we allocated
	 *  to hold intermediate search state.
	 */

	free(dp);
}
