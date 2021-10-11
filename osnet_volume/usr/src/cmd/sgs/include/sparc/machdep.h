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

#pragma ident	"@(#)machdep.h	1.44	99/05/04 SMI"

#include <link.h>
#include <machelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Elf header information.
 */
#ifdef _ELF64
#define	M_MACH			EM_SPARCV9
#define	M_CLASS			ELFCLASS64
#else
#define	M_MACH			EM_SPARC
#define	M_CLASS			ELFCLASS32
#endif
#define	M_MACHPLUS		EM_SPARC32PLUS
#define	M_DATA			ELFDATA2MSB
#define	M_FLAGSPLUS_MASK	EF_SPARC_32PLUS_MASK
#define	M_FLAGSPLUS		EF_SPARC_32PLUS

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
#define	M_SEGSIZE	ELF_SPARC_MAXPGSZ
#endif
#define	M_STRUNC(X)	((X) & ~(M_SEGSIZE - 1))
#define	M_SROUND(X)	(((X) + M_SEGSIZE - 1) & ~(M_SEGSIZE - 1))


/*
 * Instruction encodings.
 */
#define	M_SAVESP64	0x9de3bfc0	/* save %sp, -64, %sp */
#define	M_CALL		0x40000000
#define	M_JMPL		0x81c06000	/* jmpl %g1 + simm13, %g0 */
#define	M_SETHIG0	0x01000000	/* sethi %hi(val), %g0 */
#define	M_SETHIG1	0x03000000	/* sethi %hi(val), %g1 */
#define	M_STO7G1IM	0xde206000	/* st	 %o7,[%g1 + %lo(val)] */
#define	M_SUBFPSPG1	0x8227800e	/* sub	%fp,%sp,%g1 */
#define	M_NOP		0x01000000	/* sethi 0, %o0 (nop) */
#define	M_BA_A		0x30800000	/* ba,a */
#define	M_MOVO7TOG1	0x8210000f	/* mov %o7, %g1 */
#define	M_MOVI7TOG1	0x8210001f	/* mov %i7, %g1 */
#define	M_BA_A_XCC	0x30680000	/* ba,a %xcc */
#define	M_JMPL_G5G0	0x81c16000	/* jmpl %g5 + 0, %g0 */
#define	M_XNOR_G5G1	0x82396000	/* xnor	%g5, 0, %g1 */


#define	M_BIND_ADJ	4		/* adjustment for end of */
					/*	elf_rtbndr() address */


/*
 * Plt and Got information; the first few .got and .plt entries are reserved
 *	PLT[0]	jump to dynamic linker
 *	GOT[0]	address of _DYNAMIC
 */
#define	M_PLT_INSSIZE	4		/* single plt instruction size */
#define	M_GOT_XDYNAMIC	0		/* got index for _DYNAMIC */
#define	M_GOT_XNumber	1		/* reserved no. of got entries */

#ifdef _ELF64
#define	M_PLT_XNumber	4		/* reserved no. of plt entries */
#define	M_PLT_ENTSIZE	32		/* plt entry size in bytes */
#define	M_PLT_ALIGN	M_PLT_ENTSIZE	/* alignment of .plt section */
#define	M_PLT_RESERVSZ	4 * M_PLT_ENTSIZE /* first 4 plt's reserved */
#define	M_GOT_ENTSIZE	8		/* got entry size in bytes */
#define	M_GOT_MAXSMALL	1024		/* maximum no. of small gots */
#else /* Elf32 */
#define	M_PLT_XNumber	4		/* reserved no. of plt entries */
#define	M_PLT_ENTSIZE	12		/* plt entry size in bytes */
#define	M_PLT_ALIGN	M_WORD_ALIGN	/* alignment of .plt section */
#define	M_PLT_RESERVSZ	4 * M_PLT_ENTSIZE /* first 4 plt's reserved */
#define	M_GOT_ENTSIZE	4		/* got entry size in bytes */
#define	M_GOT_MAXSMALL	2048		/* maximum no. of small gots */
#endif /* _ELF64 */
					/* transition flags for got sizing */
#define	M_GOT_LARGE	(Word)(-M_GOT_MAXSMALL - 1)
#define	M_GOT_SMALL	(Word)(-M_GOT_MAXSMALL - 2)


/*
 * Other machine dependent entities
 */
#ifdef _ELF64
#define	M_SEGM_ALIGN	ELF_SPARCV9_MAXPGSZ
/*
 * Put 64-bit programs above 4 gigabytes to help insure correctness,
 * so any 64-bit programs that truncate pointers will fault now instead of
 * corrupting itself and dying mysteriously.
 */
#define	M_SEGM_ORIGIN	(Addr)0x100000000  /* default first segment offset */
#define	M_WORD_ALIGN	8
#else
#define	M_SEGM_ALIGN	ELF_SPARC_MAXPGSZ
#define	M_SEGM_ORIGIN	(Addr)0x10000	/* default first segment offset */
#define	M_WORD_ALIGN	4
#endif



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
#ifdef _ELF64
#define	ELF_R_TYPE	ELF64_R_TYPE_ID
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
#else	/* Elf32 */
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELF_R_INFO	ELF32_R_INFO
#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_M_SYM	ELF32_M_SYM
#define	ELF_M_SIZE	ELF32_M_SIZE
#define	ELF_M_INFO	ELF32_M_INFO
/* Elf64 can hide extra offset in r_info */
#define	ELF_R_TYPE_DATA(x)	(0)
#define	ELF_R_TYPE_INFO(xoff, type)	(type)
#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#define	ELF_ST_INFO	ELF32_ST_INFO
#define	elf_checksum	elf32_checksum
#define	elf_fsize	elf32_fsize
#define	elf_getehdr	elf32_getehdr
#define	elf_getphdr	elf32_getphdr
#define	elf_newehdr	elf32_newehdr
#define	elf_newphdr	elf32_newphdr
#define	elf_getshdr	elf32_getshdr
#endif	/* Elf32 */


/*
 * Make relocation types transparent to the common code
 */
#define	M_REL_DT_TYPE	DT_RELA		/* .dynamic entry */
#define	M_REL_DT_SIZE	DT_RELASZ	/* .dynamic entry */
#define	M_REL_DT_ENT	DT_RELAENT	/* .dynamic entry */
#define	M_REL_DT_COUNT	DT_RELACOUNT	/* .dynamic entry */

#define	M_DYNREL_SHT_TYPE	SHT_RELA
#define	M_OBJREL_SHT_TYPE	SHT_RELA

#define	M_REL_CONTYPSTR	conv_reloc_SPARC_type_str

/*
 * Make common relocation types transparent to the common code
 */
#define	M_R_NONE	R_SPARC_NONE
#define	M_R_GLOB_DAT	R_SPARC_GLOB_DAT
#define	M_R_COPY	R_SPARC_COPY
#define	M_R_RELATIVE	R_SPARC_RELATIVE
#define	M_R_JMP_SLOT	R_SPARC_JMP_SLOT
#define	M_R_REGISTER	R_SPARC_REGISTER
#define	M_R_FPTR	R_SPARC_NONE


/*
 * Make regester symbols transparent to common code
 */
#define	M_DT_REGISTER	DT_SPARC_REGISTER
/*
 * PLTRESERVE is not relevant on sparc
 */
#define	M_DT_PLTRESERVE	0xffffffff


/*
 * Make plt section information transparent to the common code.
 */
#define	M_PLT_SHF_FLAGS	(SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR)

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
 *  o	the got, dynamic, and plt are grouped together as these may also be
 *	accessed first by ld.so.1 to perform relocations, fill in DT_DEBUG
 *	(executables only), and .plt[0].
 *
 *  o	unknown sections (stabs and all that crap) go at the end.
 *
 * Note that .bss is given the largest identifier.  This insures that if any
 * unknown sections become associated to the same segment as the .bss, the
 * .bss section is always the last section in the segment.
 */
#define	M_ID_NULL	0x00
#define	M_ID_USER	0x01

#define	M_ID_INTERP	0x02			/* SHF_ALLOC */
#define	M_ID_HASH	0x03
#define	M_ID_DYNSYM	0x04
#define	M_ID_DYNSTR	0x05
#define	M_ID_VERSION	0x06
#define	M_ID_REL	0x07
#define	M_ID_TEXT	0x08			/* SHF_ALLOC + SHF_EXECINSTR */
#define	M_ID_DATA	0x09

#define	M_ID_GOT	0x02			/* SHF_ALLOC + SHF_WRITE */
#define	M_ID_SYMINFO	0x03
#define	M_ID_PLT	0x04
#define	M_ID_DYNAMIC	0x05
#define	M_ID_BSS	0xff

#define	M_ID_SYMTAB	0x02
#define	M_ID_STRTAB	0x03
#define	M_ID_NOTE	0x04

#define	M_ID_UNKNOWN	0xfe

#ifdef	__cplusplus
}
#endif

#endif /* _MACHDEP_H */
