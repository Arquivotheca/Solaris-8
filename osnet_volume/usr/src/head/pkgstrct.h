/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PKGSTRCT_H
#define	_PKGSTRCT_H

#pragma ident	"@(#)pkgstrct.h	1.13	99/02/18 SMI"	/* SVr4.0 1.9	*/

#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CLSSIZ	64
#define	PKGSIZ	64
#define	ATRSIZ	64

#define	BADFTYPE	'?'
#define	BADMODE		(mode_t)ULONG_MAX
#define	BADOWNER	"?"
#define	BADGROUP	"?"
#define	BADMAJOR	(major_t)ULONG_MAX
#define	BADMINOR	(minor_t)ULONG_MAX
#define	BADCLASS	"none"
#define	BADINPUT	1 /* not EOF */
#define	BADCONT		(-1L)

extern char	*errstr;

struct ainfo {
	char	*local;
	mode_t	mode;
	char	owner[ATRSIZ+1];
	char	group[ATRSIZ+1];
	major_t	major;
	minor_t	minor;
};

struct cinfo {
	long	cksum;
	long	size;
	time_t	modtime;
};

struct pinfo {
	char	status;
	char	pkg[PKGSIZ+1];
	char	editflag;
	char	aclass[ATRSIZ+1];
	struct pinfo
		*next;
};

struct cfent {
	short	volno;
	char	ftype;
	char	pkg_class[CLSSIZ+1];
	int	pkg_class_idx;
	char	*path;
	struct ainfo ainfo;
	struct cinfo cinfo;
	short	npkgs;
	struct pinfo
		*pinfo;
};

/* averify() & cverify() error codes */
#define	VE_EXIST	0x0001
#define	VE_FTYPE	0x0002
#define	VE_ATTR		0x0004
#define	VE_CONT		0x0008
#define	VE_FAIL		0x0010
#define	VE_TIME		0x0020

#ifdef	__cplusplus
}
#endif

#endif	/* _PKGSTRCT_H */
