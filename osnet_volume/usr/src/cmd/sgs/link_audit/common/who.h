/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)who.h	1.2	97/11/23 SMI"

#ifndef WHO_DOT_H
#define	WHO_DOT_H

#include <link.h>
#include <sys/regset.h>
#include <sys/frame.h>
#include <sys/elf.h>

#if defined(__sparcv9)
#define	Elf_Ehdr	Elf64_Ehdr
#define	Elf_Phdr	Elf64_Phdr
#define	Elf_Shdr	Elf64_Shdr
#define	Elf_Sym		Elf64_Sym
#define	elf_getshdr	elf64_getshdr
#else
#define	Elf_Ehdr	Elf32_Ehdr
#define	Elf_Phdr	Elf32_Phdr
#define	Elf_Shdr	Elf32_Shdr
#define	Elf_Sym		Elf32_Sym
#define	elf_getshdr	elf32_getshdr
#endif


typedef struct objinfo {
	caddr_t			o_lpc;		/* low PC */
	caddr_t			o_hpc;		/* high PC */
	int			o_fd;		/* file descriptor */
	Elf *			o_elf;		/* Elf pointer */
	Elf_Sym *		o_syms;		/* symbol table */
	uint_t			o_symcnt;	/* # of symbols */
	const char *		o_strs;		/* symbol string  table */
	Link_map *		o_lmp;
	uint_t			o_flags;	
	struct objinfo *	o_next;
} Objinfo;

#define	FLG_OB_NOSYMS	0x0001		/* no symbols available for obj */
#define	FLG_OB_FIXED	0x0002		/* fixed address object */


#if defined(sparc) || defined(__sparc) || defined(__sparcv9)
#define FLUSHWIN() asm("ta 3");
#define FRAME_PTR_INDEX 1
#define SKIP_FRAMES 0
#endif

#if defined(i386) || defined(__i386)
#define FLUSHWIN() 
#define FRAME_PTR_INDEX 3
#define SKIP_FRAMES 1
#endif

#ifndef	STACK_BIAS
#define	STACK_BIAS	0
#endif

#endif /* WHO_DOT_H */
