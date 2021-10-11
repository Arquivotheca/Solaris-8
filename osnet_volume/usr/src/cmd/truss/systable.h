/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "@(#)systable.h	1.6	97/12/23 SMI"	/* SVr4.0 1.2	*/

struct systable {
	const char *name;	/* name of system call */
	short	nargs;		/* number of arguments */
	char	rval[2];	/* return value types */
	char	arg[8];		/* argument types */
};

/* the system call table */
extern const struct systable systable[];


struct sysalias {
	const char *name;	/* alias name of system call */
	int	number;		/* number of system call */
};

extern const struct sysalias sysalias[];

/*
 * Function prototypes.
 */
extern	const char *errname(int);
extern	const struct systable *subsys(int, int);
extern	const char *sysname(int, int);
extern	const char *rawsigname(int);
extern	const char *signame(int);
extern	const char *rawfltname(int);
extern	const char *fltname(int);
