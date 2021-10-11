/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_ELFRD_H
#define	_ELFRD_H

#pragma ident	"@(#)elfrd.h	1.2	96/01/16 SMI"

struct libpath *add_libpath(struct libpath *, char *, int);
struct libpath *stk_libpath(struct libpath *, char *, int);
struct libpath *pop_libpath(struct libpath *);


struct sobj {
	char *so_name;
	struct sobj *so_next;
};

struct libpath {
	char *lp_path;
	struct libpath *lp_next;
	int lp_level;
};


#define	ERR_NOERROR	0
#define	ERR_NOFILE	-1
#define	ERR_NOELFVER	-2
#define	ERR_NOELFBEG	-3
#define	ERR_NOELFEHD	-4
#define	ERR_NOELFSHD	-5
#define	ERR_NOELFSDT	-6
#define	ERR_NOELFNAM	-7
#define	ERR_HASHFULL	-8


#define	GSO_ADDEXCLD	1

#ifdef MAIN
#define	EXTERN
#else
#define	EXTERN	extern
#endif

EXTERN struct libpath *libp, libp_hd;

#undef EXTERN

#endif /* _ELFRD_H */
