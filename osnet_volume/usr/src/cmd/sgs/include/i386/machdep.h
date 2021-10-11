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
 *
 * Global include file for all sgs SPARC machine dependent macros, constants
 * and declarations.
 */

#ifndef	_MACHDEP_H
#define	_MACHDEP_H

#pragma ident	"@(#)machdep.h	1.34	99/11/09 SMI"

#include <link.h>
#include <machelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Elf header information.
 */
#ifdef	_ELF64
#define	M_MACH			EM_NONE		/* not yet ready for ia64 */
#define	M_CLASS			ELFCLASS64
#else
#define	M_MACH			EM_386
#define	M_CLASS			ELFCLASS32
#endif	/* _ELF64 */

#define	M_MACHPLUS		M_MACH
#define	M_DATA			ELFDATA2LSB
#define	M_FLAGSPLUS_MASK	0
#define	M_FLAGSPLUS		0

/*
 * Page boundary Macros: truncate to previous page boundary and round to
 * next page boundary (refer to generic macros in ../sgs.h also).
 */
#define	M_PTRUNC(X)	((X) & ~(syspagsz - 1))
#define	M_PROUND(X)	(((X) + syspagsz - 1) & ~(syspagsz - 1))

/*
 * Segment boundary macros: truncate to previous segment boundary and round
 * to next page boundary.
 */
#ifndef	M_SEGSIZE
#define	M_SEGSIZE	ELF_386_MAXPGSZ
#endif

#define	M_STRUNC(X)	((X) & ~(M_SEGSIZE - 1))
#define	M_SROUND(X)	(((X) + M_SEGSIZE - 1) & ~(M_SEGSIZE - 1))


/*
 * Other machine dependent entities
 */
#define	M_SEGM_ALIGN	ELF_386_MAXPGSZ

#ifdef	_ELF64
/*
 * Values for IA64 objects
 */
#define	M_WORD_ALIGN	8
#define	M_SEGM_ORIGIN	(Addr)0x100000000 /* default first segment offset */

#define	M_PLT_ENTSIZE	64		/* plt entry size in bytes */
#define	M_PLT_ALIGN	M_WORD_ALIGN * 2 /* alignment of .plt section */
#define	M_PLT_INSSIZE	6		/* single plt instruction size */
#define	M_PLT_RESERVSZ	80		/* PLT[0] reserved */

#define	M_GOT_XLINKMAP	0		/* got index for link map */
#define	M_GOT_XRTLD	1		/* got index for rtbinder */
#define	M_GOT_XRTLDGP	2		/* got index for ld.so.1 GP val */
#define	M_GOT_XNumber	3		/* reserved no. of got entries */
#define	M_GOT_ENTSIZE	8		/* got entry size in bytes */
#else
/*
 * Values for IA32 objects
 */

/*
 * Instruction encodings.
 */
#define	M_INST_JMP		0xe9
#define	M_INST_PUSHL		0x68
#define	M_SPECIAL_INST		0xff
#define	M_PUSHL_DISP		0x35
#define	M_PUSHL_REG_DISP	0xb3
#define	M_JMP_DISP_IND		0x25
#define	M_JMP_REG_DISP_IND	0xa3

#define	M_BIND_ADJ		2	/* adjustment for end of */
					/*	elf_rtbndr() address */
#define	M_WORD_ALIGN	4
#define	M_STACK_GAP	(0x08000000)
#define	M_STACK_PGS	(0x00048000)
#define	M_SEGM_ORIGIN	(Addr)(M_STACK_GAP + M_STACK_PGS)

/*
 * Plt and Got information; the first few .got and .plt entries are reserved
 *	PLT[0]	jump to dynamic linker
 *	GOT[0]	address of _DYNAMIC
 */
#define	M_PLT_ENTSIZE	16		/* plt entry size in bytes */
#define	M_PLT_ALIGN	M_WORD_ALIGN	/* alignment of .plt section */
#define	M_PLT_INSSIZE	6		/* single plt instruction size */
#define	M_PLT_RESERVSZ	M_PLT_ENTSIZE	/* PLT[0] reserved */

#define	M_GOT_XDYNAMIC	0		/* got index for _DYNAMIC */
#define	M_GOT_XLINKMAP	1		/* got index for link map */
#define	M_GOT_XRTLD	2		/* got index for rtbinder */
#define	M_GOT_XNumber	3		/* reserved no. of got entries */
#define	M_GOT_ENTSIZE	4		/* got entry size in bytes */
#endif	/* _ELF64 */


/*
 * ld support interface
 */
#ifdef _ELF64
#define	lds_start	ld_start64
#define	lds_atexit	ld_atexit64
#define	lds_file	ld_file64
#define	lds_section	ld_section64
#else  /* Elf32 */
#define	lds_start	ld_start
#define	lds_atexit	ld_atexit
#define	lds_file	ld_file
#define	lds_section	ld_section
#endif /* !_ELF64 */


/*
 * Make machine class dependent functions transparent to the common code
 */
#ifdef	_ELF64
#define	ELF_R_TYPE	ELF64_R_TYPE
#define	ELF_R_INFO	ELF64_R_INFO
#define	ELF_R_SYM	ELF64_R_SYM
#define	ELF_R_TYPE_DATA	ELF64_R_TYPE_DATA
#define	ELF_R_TYPE_INFO	ELF64_R_TYPE_INFO
#define	ELF_ST_BIND	ELF64_ST_BIND
#define	ELF_ST_TYPE	ELF64_ST_TYPE
#define	ELF_ST_INFO	ELF64_ST_INFO
#define	ELF_M_SYM	ELF64_M_SYM
#define	ELF_M_SIZE	ELF64_M_SIZE
#define	ELF_M_INFO	ELF64_M_INFO
#define	elf_checksum	elf64_checksum
#define	elf_fsize	elf64_fsize
#define	elf_getehdr	elf64_getehdr
#define	elf_getphdr	elf64_getphdr
#define	elf_newehdr	elf64_newehdr
#define	elf_newphdr	elf64_newphdr
#define	elf_getshdr	elf64_getshdr
#define	elf_xlatetof	elf64_xlatetof
#define	elf_xlatetom	elf64_xlatetom
#else
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELF_R_INFO	ELF32_R_INFO
#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#define	ELF_ST_INFO	ELF32_ST_INFO
#define	ELF_M_SYM	ELF32_M_SYM
#define	ELF_M_SIZE	ELF32_M_SIZE
#define	ELF_M_INFO	ELF32_M_INFO
#define	elf_checksum	elf32_checksum
#define	elf_fsize	elf32_fsize
#define	elf_getehdr	elf32_getehdr
#define	elf_getphdr	elf32_getphdr
#define	elf_newehdr	elf32_newehdr
#define	elf_newphdr	elf32_newphdr
#define	elf_getshdr	elf32_getshdr
#define	elf_xlatetof	elf32_xlatetof
#define	elf_xlatetom	elf32_xlatetom
#endif /* _ELF64 */


/*
 * Make relocation types transparent to the common code
 */
#define	M_REL_DT_TYPE	DT_REL		/* .dynamic entry */
#define	M_REL_DT_SIZE	DT_RELSZ	/* .dynamic entry */
#define	M_REL_DT_ENT	DT_RELENT	/* .dynamic entry */
#define	M_REL_DT_COUNT	DT_RELCOUNT	/* .dynamic entry */


/*
 * With the introduction of the IA64 architecture it is now
 * possible that the type of the output Relocations will
 * vary depending on the type of file being created.  For
 * IA64 it's Rel for a dynamic object & Rela for relocatable
 * objects.  These macros define that.
 */
#ifdef _ELF64
#define	M_DYNREL_SHT_TYPE	SHT_REL
#define	M_OBJREL_SHT_TYPE	SHT_RELA
#else	/* _ELF64 */
#define	M_DYNREL_SHT_TYPE	SHT_REL
#define	M_OBJREL_SHT_TYPE	SHT_REL
#endif	/* _ELF64 */

#ifdef	_ELF64
/*
 * Make common relocation types transparent to the common code.
 */
#define	M_REL_CONTYPSTR	conv_reloc_ia64_type_str
#define	M_R_NONE	R_IA_64_NONE
#define	M_R_GLOB_DAT	R_IA_64_DIR64LSB
#define	M_R_COPY	R_IA_64_NONE
#define	M_R_RELATIVE	R_IA_64_REL64LSB /* need fixing */
#define	M_R_JMP_SLOT	R_IA_64_IPLTLSB
#define	M_R_FPTR	R_IA_64_FPTR64LSB

#else	/* _ELF64 */

#define	M_REL_CONTYPSTR	conv_reloc_386_type_str
#define	M_R_NONE	R_386_NONE
#define	M_R_GLOB_DAT	R_386_GLOB_DAT
#define	M_R_COPY	R_386_COPY
#define	M_R_RELATIVE	R_386_RELATIVE
#define	M_R_JMP_SLOT	R_386_JMP_SLOT
#define	M_R_FPTR	R_386_NONE
#endif	/* _ELF64 */

#define	M_R_REGISTER	M_R_NONE

/*
 * DT_REGISTER is not valid on i386
 */
#define	M_DT_REGISTER	0xffffffff
#ifdef	_ELF64
#define	M_DT_PLTRESERVE	DT_IA_64_PLT_RESERVE
#else	/* _ELF64 */
#define	M_DT_PLTRESERVE	0xfffffffe
#endif

/*
 * Make plt section information transparent to the common code.
 */
#define	M_PLT_SHF_FLAGS	(SHF_ALLOC | SHF_EXECINSTR)


/*
 * Make data segment information transparent to the common code.
 */
#define	M_DATASEG_PERM	(PF_R | PF_W | PF_X)

/*
 * Define a set of identifies for special sections.  These allow the sections
 * to be ordered within the output file image.
 *
 *  o	null identifies indicate that this section does not need to be added
 *	to the output image (ie. shared object sections or sections we're
 *	going to recreate (sym tables, string tables, relocations, etc.));
 *
 *  o	any user defined section will be first in the associated segment.
 *
 *  o	the hash, dynsym, dynstr and rel's are grouped together as these
 *	will all be accessed first by ld.so.1 to perform relocations.
 *
 *  o	the got and dynamic are grouped together as these may also be
 *	accessed first by ld.so.1 to perform relocations, fill in DT_DEBUG
 *	(executables only), and .got[0].
 *
 *  o	unknown sections (stabs and all that crap) go at the end.
 *
 * Note that .bss is given the largest identifier.  This insures that if any
 * unknown sections become associated to the same segment as the .bss, the
 * .bss section is always the last section in the segment.
 */
#define	M_ID_NULL	0x00
#define	M_ID_UNWIND	0x01		/* ia64: SHT_IA_64_UNWIND sections */
#define	M_ID_USER	0x02

#define	M_ID_INTERP	0x03			/* SHF_ALLOC */
#define	M_ID_HASH	0x04
#define	M_ID_DYNSYM	0x05
#define	M_ID_DYNSTR	0x06
#define	M_ID_VERSION	0x07
#define	M_ID_REL	0x08
#define	M_ID_PLT	0x09
#define	M_ID_TEXT	0x0a			/* SHF_ALLOC + SHF_EXECINSTR */
#define	M_ID_DATA	0x0b

#define	M_ID_GOT	0x02			/* SHF_ALLOC + SHF_WRITE */
#define	M_ID_SYMINFO	0x03
#define	M_ID_DYNAMIC	0x04
#define	M_ID_BSS	0xff

#define	M_ID_SYMTAB	0x02
#define	M_ID_STRTAB	0x03
#define	M_ID_NOTE	0x04

#define	M_ID_UNKNOWN	0xfe

#ifdef	__cplusplus
}
#endif

#endif /* _MACHDEP_H */
