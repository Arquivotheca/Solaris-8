/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 * Hashed key data base library.
 */

#ifndef _NDBM_H
#define	_NDBM_H

#pragma ident	"@(#)ndbm.h	1.12	96/09/16 SMI"	/* SVr4.0 1.1	*/

#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * flags to dbm_store()
 */
#define	DBM_INSERT	0
#define	DBM_REPLACE	1

#define	_PBLKSIZ 1024
#define	_DBLKSIZ 4096

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	PBLKSIZ _PBLKSIZ
#define	DBLKSIZ _DBLKSIZ
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

typedef struct {
	int	dbm_dirf;		/* open directory file */
	int	dbm_pagf;		/* open page file */
	int	dbm_flags;		/* flags, see below */
	long	dbm_maxbno;		/* last ``bit'' in dir file */
	long	dbm_bitno;		/* current bit number */
	long	dbm_hmask;		/* hash mask */
	long	dbm_blkptr;		/* current block for dbm_nextkey */
	int	dbm_keyptr;		/* current key for dbm_nextkey */
	long	dbm_blkno;		/* current page to read/write */
	long	dbm_pagbno;		/* current page in pagbuf */
	char	dbm_pagbuf[_PBLKSIZ];	/* page file block buffer */
	long	dbm_dirbno;		/* current block in dirbuf */
	char	dbm_dirbuf[_DBLKSIZ];	/* directory file block buffer */
} DBM;

#if defined(_XPG4_2)
typedef struct {
	void	*dptr;
	size_t	dsize;
} datum;
#else
typedef struct {
	char	*dptr;
	long	dsize;
} datum;
#endif

#ifdef	__STDC__
DBM	*dbm_open(const char *, int, mode_t);
void	dbm_close(DBM *);
datum	dbm_fetch(DBM *, datum);
datum	dbm_firstkey(DBM *);
datum	dbm_nextkey(DBM *);
int	dbm_delete(DBM *, datum);
int	dbm_store(DBM *, datum, datum, int);
int	dbm_clearerr(DBM *);
int	dbm_error(DBM *);
#else
DBM	*dbm_open();
void	dbm_close();
datum	dbm_fetch();
datum	dbm_firstkey();
datum	dbm_nextkey();
int	dbm_delete();
int	dbm_store();
int	dbm_clearerr();
int	dbm_error();
#endif

#define	_DBM_RDONLY	0x1	/* data base open read-only */
#define	_DBM_IOERR	0x2	/* data base I/O error */

#define	dbm_rdonly(__db)	((__db)->dbm_flags & _DBM_RDONLY)
#define	dbm_error(__db)		((__db)->dbm_flags & _DBM_IOERR)
/* use this one at your own risk! */
#define	dbm_clearerr(__db)	((__db)->dbm_flags &= ~_DBM_IOERR)
/* for fstat(2) */
#define	dbm_dirfno(__db)	((__db)->dbm_dirf)
#define	dbm_pagfno(__db)	((__db)->dbm_pagf)

#ifdef	__cplusplus
}
#endif

#endif	/* _NDBM_H */
