/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _DBM_H
#define	_DBM_H

#pragma ident	"@(#)dbm.h	1.6	97/06/04 SMI"	/* SVr4.0 1.2	*/

#ifdef __cplusplus
extern "C" {
#endif

#define	PBLKSIZ	1024
#define	DBLKSIZ	4096
#define	BYTESIZ	8

#ifndef	NULL
#define	NULL	((char *) 0)
#endif

long	bitno;
long	maxbno;
long	blkno;
long	hmask;

char	pagbuf[PBLKSIZ];
char	dirbuf[DBLKSIZ];

int	dirf;
int	pagf;
int	dbrdonly;

#ifndef	DATUM
typedef	struct
{
	char	*dptr;
	int	dsize;
} datum;
#define	DATUM
#endif

#ifdef __STDC__
datum	fetch(datum);
datum	makdatum(char *, int);
datum	firstkey(void);
datum	nextkey(datum);
datum	firsthash(long);
long	calchash(datum);
long	hashinc(long);
#else
datum	fetch();
datum	makdatum();
datum	firstkey();
datum	nextkey();
datum	firsthash();
long	calchash();
long	hashinc();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _DBM_H */
