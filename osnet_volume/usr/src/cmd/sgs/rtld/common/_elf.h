/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__ELF_DOT_H
#define	__ELF_DOT_H

#pragma ident	"@(#)_elf.h	1.35	99/05/04 SMI"

#include	<sys/types.h>
#include	<elf.h>
#include	"_rtld.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Common extern functions for ELF file class.
 */
extern	int		elf_reloc(Rt_map *, int);
extern	void		elf_plt_init(unsigned int *, caddr_t);
extern	int		elf_set_prot(Rt_map *, int);
extern	Rt_map		*elf_obj_file(Lm_list *, const char *);
extern	Rt_map		*elf_obj_fini(Lm_list *, Rt_map *);
extern	int		elf_copy_reloc(char *, Sym *, Rt_map *, void *, Sym *,
				Rt_map *, const void *);
extern	Sym		*elf_find_sym(const char *, Rt_map *, Rt_map **,
				int, unsigned long);
extern	Sym		*elf_lazy_find_sym(Slookup *, Rt_map **, int);
extern	Rt_map		*elf_lazy_load(Rt_map *, uint_t, const char *);
extern	int		elf_rtld_load(Rt_map *);

#if	defined(__sparcv9)
extern	void		elf_plt2_init(unsigned int *, Rt_map *);
#endif

#if	defined(i386)
extern	ulong_t		elf_reloc_relacount(ulong_t, ulong_t, ulong_t,
				ulong_t);
#endif

/*
 * Padinfo
 *
 * Used to track the which PLTpadd entries have been used and
 * to where they are bound.
 *
 * NOTE: these are only currently used for SparcV9
 */
typedef struct pltpadinfo {
	Addr	pp_addr;
	void	*pp_plt;
} Pltpadinfo;

/*
 * Private data for an ELF file class.
 */
typedef struct _rt_elf_private {
	void		*e_symtab;	/* symbol table */
	unsigned int	*e_hash;	/* hash table */
	char		*e_strtab;	/* string table */
	void		*e_reloc;	/* relocation table */
	unsigned int	*e_pltgot;	/* addrs for procedure linkage table */
	void		*e_pltreserve;	/* ia64: DT_IA_64_PLTRESERVE */
	void		*e_dynplt;	/* dynamic plt table - used by prof */
	void		*e_jmprel;	/* plt relocations */
	unsigned long	e_pltrelsize;	/* size of PLT relocation entries */
	unsigned long	e_relsz;	/* size of relocs */
	unsigned long	e_relent;	/* size of base reloc entry */
	unsigned long	e_movesz;	/* size of movetabs */
	unsigned long	e_moveent;	/* size of base movetab entry */
	void		*e_movetab;	/* movetable address */
	Phdr		*e_sunwbss;	/* program header for SUNWBSS */
	unsigned long	e_syment;	/* size of symtab entry */
	unsigned long	e_entry;	/* entry point for file */
	void		*e_phdr;	/* program header of object */
	unsigned short	e_phnum;	/* number of segments */
	unsigned short	e_phentsize;	/* size of phdr entry */
	Verneed		*e_verneed;	/* versions needed by this image and */
	int		e_verneednum;	/*	their associated count */
	Verdef		*e_verdef;	/* versions defined by this image and */
	int		e_verdefnum;	/*	their associated count */
	unsigned long	e_syminent;	/* syminfo entry size */
	void		*e_pltpad;	/* PLTpad table */
	void		*e_pltpadend;	/* end of PLTpad table */
} Rt_elfp;

/*
 * Macros for getting to linker ELF private data.
 */
#define	ELFPRV(X)	((X)->rt_priv)
#define	SYMTAB(X)	(((Rt_elfp *)(X)->rt_priv)->e_symtab)
#define	HASH(X)		(((Rt_elfp *)(X)->rt_priv)->e_hash)
#define	STRTAB(X)	(((Rt_elfp *)(X)->rt_priv)->e_strtab)
#define	REL(X)		(((Rt_elfp *)(X)->rt_priv)->e_reloc)
#define	PLTGOT(X)	(((Rt_elfp *)(X)->rt_priv)->e_pltgot)
#define	MOVESZ(X)	(((Rt_elfp *)(X)->rt_priv)->e_movesz)
#define	MOVEENT(X)	(((Rt_elfp *)(X)->rt_priv)->e_moveent)
#define	MOVETAB(X)	(((Rt_elfp *)(X)->rt_priv)->e_movetab)
#define	DYNPLT(X)	(((Rt_elfp *)(X)->rt_priv)->e_dynplt)
#define	JMPREL(X)	(((Rt_elfp *)(X)->rt_priv)->e_jmprel)
#define	PLTRELSZ(X)	(((Rt_elfp *)(X)->rt_priv)->e_pltrelsize)
#define	RELSZ(X)	(((Rt_elfp *)(X)->rt_priv)->e_relsz)
#define	RELENT(X)	(((Rt_elfp *)(X)->rt_priv)->e_relent)
#define	SYMENT(X)	(((Rt_elfp *)(X)->rt_priv)->e_syment)
#define	ENTRY(X)	(((Rt_elfp *)(X)->rt_priv)->e_entry)
#define	PHDR(X)		(((Rt_elfp *)(X)->rt_priv)->e_phdr)
#define	PHNUM(X)	(((Rt_elfp *)(X)->rt_priv)->e_phnum)
#define	PHSZ(X)		(((Rt_elfp *)(X)->rt_priv)->e_phentsize)
#define	VERNEED(X)	(((Rt_elfp *)(X)->rt_priv)->e_verneed)
#define	VERNEEDNUM(X)	(((Rt_elfp *)(X)->rt_priv)->e_verneednum)
#define	VERDEF(X)	(((Rt_elfp *)(X)->rt_priv)->e_verdef)
#define	VERDEFNUM(X)	(((Rt_elfp *)(X)->rt_priv)->e_verdefnum)
#define	SUNWBSS(X)	(((Rt_elfp *)(X)->rt_priv)->e_sunwbss)
#define	SYMINENT(X)	(((Rt_elfp *)(X)->rt_priv)->e_syminent)
#define	PLTPAD(X)	(((Rt_elfp *)(X)->rt_priv)->e_pltpad)
#define	PLTPADEND(X)	(((Rt_elfp *)(X)->rt_priv)->e_pltpadend)
#define	PLTRESERVE(X)	(((Rt_elfp *)(X)->rt_priv)->e_pltreserve)

#ifdef	__cplusplus
}
#endif

#endif	/* __ELF_DOT_H */
