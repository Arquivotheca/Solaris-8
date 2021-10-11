/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef _SA_H
#define	_SA_H

#pragma ident	"@(#)sa.h	1.14	94/01/21 SMI"

/*
 * sa.h contains struct sa and defines variables used in sadc.c and sar.c.
 */

#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct iodevinfo {
	struct iodevinfo *next;
	kstat_t *ksp;
	kstat_t ks;
	kstat_io_t kios;
} iodevinfo_t;

#define	KMEM_SMALL  0		/* small KMEM request index		*/
#define	KMEM_LARGE  1		/* large KMEM request index		*/
#define	KMEM_OSIZE  2		/* outsize KMEM request index		*/
#define	KMEM_NCLASS 3		/* # of KMEM request classes		*/

typedef struct kmeminfo {
	ulong	km_mem[KMEM_NCLASS];	/* amount of mem owned by KMEM	*/
	ulong	km_alloc[KMEM_NCLASS];  /* amount of mem allocated	*/
	ulong	km_fail[KMEM_NCLASS];	/* # of failed requests		*/
} kmeminfo_t;

/*
 * structure sa defines the data structure of system activity data file
 */

struct sa {
	int		valid;		/* non-zero for valid data	*/
	time_t		ts;		/* time stamp			*/

	cpu_sysinfo_t	csi;		/* per-CPU system information	*/
	cpu_vminfo_t	cvmi;		/* per-CPU vm information	*/
	sysinfo_t	si;		/* global system information	*/
	vminfo_t	vmi;		/* global vm information	*/
	kmeminfo_t	kmi;		/* kernel mem allocation info	*/

	ulong_t		szinode;	/* inode table size		*/
	ulong_t		szfile;		/* file table size		*/
	ulong_t		szproc;		/* proc table size		*/
	ulong_t		szlckr;		/* file record lock table size	*/

	ulong_t		mszinode;	/* max inode table size		*/
	ulong_t		mszfile;	/* max file table size		*/
	ulong_t		mszproc;	/* max proc table size		*/
	ulong_t		mszlckr;	/* max file rec lock table size	*/

	ulong_t	niodevs;		/* number of I/O devices	*/

	/* An array of iodevinfo structs come next in the sadc files	*/
};

extern struct sa sa;

#ifdef	__cplusplus
}
#endif

#endif /* _SA_H */
