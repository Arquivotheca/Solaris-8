/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	VPATH assumptions:
 *		VPATH is the environment variable containing the view path
 *		where each path name is followed by ':', '\n', or '\0'.
 *		Embedded blanks are considered part of the path.
 */

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vp.h	1.1	99/01/11 SMI"

#define	MAXPATH	200		/* max length for entire name */

extern char	**vpdirs;	/* directories (including current) in */
				/* view path */
extern	int	vpndirs;	/* number of directories in view path */

extern void vpinit(char *);
extern int vpaccess(char *path, mode_t amode);
