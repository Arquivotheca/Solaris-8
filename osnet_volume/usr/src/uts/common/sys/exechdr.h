/*
 * Copyright (c) 1985-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_EXECHDR_H
#define	_SYS_EXECHDR_H

#pragma ident	"@(#)exechdr.h	1.17	96/09/29 SMI"

#include <sys/inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * format of the exec header
 * known by kernel and by user programs
 */
struct exec {
#ifdef sun
	unsigned	a_dynamic:1;	/* has a __DYNAMIC */
	unsigned	a_toolversion:7; /* version of toolset used to */
					/* create this file */
	unsigned char	a_machtype;	/* machine type */
	uint16_t 	a_magic;	/* magic number */
#else
	uint32_t	a_magic;	/* magic number */
#endif
	uint32_t	a_text;		/* size of text segment */
	uint32_t	a_data;		/* size of initialized data */
	uint32_t	a_bss;		/* size of uninitialized data */
	uint32_t	a_syms;		/* size of symbol table */
	uint32_t	a_entry;	/* entry point */
	uint32_t	a_trsize;	/* size of text relocation */
	uint32_t	a_drsize;	/* size of data relocation */
};

#define	OMAGIC	0407		/* old impure format */
#define	NMAGIC	0410		/* read-only text */
#define	ZMAGIC	0413		/* demand load format */

/* machine types */

#ifdef sun
#define	M_OLDSUN2	0	/* old sun-2 executable files */
#define	M_68010		1	/* runs on either 68010 or 68020 */
#define	M_68020		2	/* runs only on 68020 */
#define	M_SPARC		3	/* runs only on SPARC */

#define	TV_SUN2_SUN3	0
#define	TV_SUN4		1
#endif	/* sun */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_EXECHDR_H */
