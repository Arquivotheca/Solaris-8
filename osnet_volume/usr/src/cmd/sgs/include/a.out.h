/*
 *	Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)a.out.h	1.3	92/09/15 SMI"

#ifndef	A_DOT_OUT_DOT_H
#define	A_DOT_OUT_DOT_H

struct exec {
#ifdef	sun
	unsigned char	a_dynamic:1;	/* has a __DYNAMIC */
	unsigned char	a_toolversion:7; /* version of toolset used to create */
					/*	this file */
	unsigned char	a_machtype;	/* machine type */
	unsigned short	a_magic;	/* magic number */
#else
	unsigned long	a_magic;	/* magic number */
#endif
	unsigned long	a_text;		/* size of text segment */
	unsigned long	a_data;		/* size of initialized data */
	unsigned long	a_bss;		/* size of uninitialized data */
	unsigned long	a_syms;		/* size of symbol table */
	unsigned long	a_entry;	/* entry point */
	unsigned long	a_trsize;	/* size of text relocation */
	unsigned long	a_drsize;	/* size of data relocation */
};

/*
 * Macros for identifying an a.out format file.
 */
#define	M_SPARC	3			/* runs only on SPARC */
#define	OMAGIC	0407			/* old impure format */
#define	NMAGIC	0410			/* read-only text */
#define	ZMAGIC	0413			/* demand load format */

#define	N_BADMAG(x) \
	((x).a_magic != OMAGIC && (x).a_magic != NMAGIC && \
	(x).a_magic != ZMAGIC)

/*
 * Page size for a.out (used to overide machdep.h definition).
 */
#ifndef	M_SEGSIZE
#define	M_SEGSIZE	0x2000		/* 8k */
#endif

#endif
