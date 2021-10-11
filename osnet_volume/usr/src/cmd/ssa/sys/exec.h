
/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
 * SPARCstorage Array exec.h file
 */

#ifndef	_P_EXEC
#define	_P_EXEC

#pragma ident	"@(#)exec.h	1.2	94/12/08 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * format of the exec header
 * known by kernel and by user programs
 */
struct exec {
#ifdef sun
	unsigned char	a_dynamic:1;	/* has a __DYNAMIC */
	unsigned char	a_toolversion:7; /* ver of toolset used to create */
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
#endif sun


#ifdef	__cplusplus
}
#endif

#endif	/* _P_EXEC */
