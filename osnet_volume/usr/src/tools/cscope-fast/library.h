/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* library function return value declarations */

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)library.h	1.1	99/01/11 SMI"


/* private library */
char	*compath(char *pathname);
char	*getwd(char *dir);
char	*logdir(char *name);
char	*mygetenv(char *variable, char *deflt);
char	*mygetwd(char *dir);

/* alloc.c */
char	*stralloc(char *s);
void	*mymalloc(size_t size);
void	*mycalloc(size_t nelem, size_t size);
void	*myrealloc(void *p, size_t size);

/* mypopen.c */
FILE	*mypopen(char *cmd, char *mode);
int	mypclose(FILE *ptr);

/* vp*.c */
FILE	*vpfopen(char *filename, char *type);
void	vpinit(char *currentdir);
int	vpopen(char *path, int oflag);
struct stat;
int	vpstat(char *path, struct stat *statp);

/* standard C library */
#include <stdlib.h>
#include <string.h>	/* string functions */
