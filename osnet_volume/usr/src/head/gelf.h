/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_GELF_H
#define	_GELF_H

#pragma ident	"@(#)gelf.h	1.8	99/01/14 SMI"

#include <libelf.h>
#include <sys/link.h>

#ifdef	__cplusplus
extern "C" {
#endif


#if !(defined(_LP64) || ((__STDC__ - 0 == 0) && (!defined(_NO_LONGLONG))))
#error "64-bit integer types are required by gelf."
#endif


/*
 * Class-independent ELF API for Elf utilties.  This is
 * for manipulating Elf32 and Elf64 specific information
 * in a format common to both classes.
 */


typedef Elf64_Addr	GElf_Addr;
typedef Elf64_Half	GElf_Half;
typedef Elf64_Off	GElf_Off;
typedef Elf64_Sword	GElf_Sword;
typedef Elf64_Sxword	GElf_Sxword;
typedef Elf64_Word	GElf_Word;
typedef Elf64_Xword	GElf_Xword;

typedef Elf64_Ehdr	GElf_Ehdr;
typedef	Elf64_Move	GElf_Move;
typedef Elf64_Phdr	GElf_Phdr;
typedef Elf64_Shdr	GElf_Shdr;
typedef Elf64_Sym	GElf_Sym;
typedef	Elf64_Syminfo	GElf_Syminfo;
typedef Elf64_Rela	GElf_Rela;
typedef Elf64_Rel	GElf_Rel;
typedef Elf64_Dyn	GElf_Dyn;

/*
 * The processing of versioning information can stay the
 * same because both the Elf32 and Elf64 structures are
 * of equal sizes.
 */
typedef Elf64_Verdef	GElf_Verdef;
typedef Elf64_Verdaux	GElf_Verdaux;
typedef Elf64_Verneed	GElf_Verneed;
typedef Elf64_Vernaux	GElf_Vernaux;
typedef Elf64_Versym	GElf_Versym;

/*
 * move.m_info is encoded using the 64bit fields in Gelf.
 */
#define	GELF_M_SYM	ELF64_M_SYM
#define	GELF_M_SIZE	ELF64_M_SIZE

/*
 * sym.st_info field is same size for Elf32 and Elf64.
 */
#define	GELF_ST_BIND	ELF64_ST_BIND
#define	GELF_ST_TYPE	ELF64_ST_TYPE
#define	GELF_ST_INFO	ELF64_ST_INFO


/*
 * Elf64 r_info may have data field in type id's word,
 * so GELF_R_TYPE is defined as ELF64_R_TYPE_ID in order
 * to isolate the proper bits for the true type id.
 */
#define	GELF_R_TYPE		ELF64_R_TYPE_ID
#define	GELF_R_SYM		ELF64_R_SYM
#define	GELF_R_INFO		ELF64_R_INFO
#define	GELF_R_TYPE_DATA	ELF64_R_TYPE_DATA
#define	GELF_R_TYPE_ID		ELF64_R_TYPE_ID
#define	GELF_R_TYPE_INFO	ELF64_R_TYPE_INFO



int		gelf_getclass(Elf* elf);
size_t		gelf_fsize(Elf * elf, Elf_Type type, size_t count,
				unsigned ver);
GElf_Ehdr *	gelf_getehdr(Elf * elf, GElf_Ehdr * dst);
int		gelf_update_ehdr(Elf * elf, GElf_Ehdr * src);
unsigned long	gelf_newehdr(Elf * elf, int elfclass);
GElf_Phdr *	gelf_getphdr(Elf * elf, int ndx, GElf_Phdr * dst);
int		gelf_update_phdr(Elf * elf, int ndx, GElf_Phdr * src);
unsigned long	gelf_newphdr(Elf * elf, size_t phnum);
GElf_Shdr *	gelf_getshdr(Elf_Scn * scn,  GElf_Shdr * dst);
int		gelf_update_shdr(Elf_Scn * scn, GElf_Shdr * src);
Elf_Data *	gelf_xlatetof(Elf * elf, Elf_Data * dst,
				const Elf_Data * src, unsigned encode);
Elf_Data *	gelf_xlatetom(Elf * elf, Elf_Data * dst,
				const Elf_Data * src, unsigned encode);

GElf_Sym *	gelf_getsym(Elf_Data * data, int ndx, GElf_Sym * dst);
int		gelf_update_sym(Elf_Data * dest, int ndx, GElf_Sym * src);
GElf_Syminfo *	gelf_getsyminfo(Elf_Data * data, int ndx, GElf_Syminfo * dst);
int		gelf_update_syminfo(Elf_Data * dest, int ndx,
			GElf_Syminfo * src);
GElf_Move *	gelf_getmove(Elf_Data * data, int ndx, GElf_Move * src);
int		gelf_update_move(Elf_Data * dest, int ndx, GElf_Move * src);
GElf_Dyn *	gelf_getdyn(Elf_Data * src, int ndx, GElf_Dyn * dst);
int		gelf_update_dyn(Elf_Data * dst, int ndx, GElf_Dyn * src);
GElf_Rela *	gelf_getrela(Elf_Data * src, int ndx, GElf_Rela * dst);
int		gelf_update_rela(Elf_Data * dst, int ndx, GElf_Rela * src);
GElf_Rel *	gelf_getrel(Elf_Data * src, int ndx, GElf_Rel * dst);
int		gelf_update_rel(Elf_Data * dst, int ndx, GElf_Rel * src);
long		gelf_checksum(Elf *);


#ifdef	__cplusplus
}
#endif

#endif	/* _GELF_H */
