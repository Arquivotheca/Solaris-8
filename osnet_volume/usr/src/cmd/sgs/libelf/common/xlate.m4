/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xlate.m4	1.14	98/08/28 SMI" 	/* SVr4.0 1.14	*/

#ifdef __STDC__
	#pragma weak	elf32_fsize = _elf32_fsize
	#pragma weak	elf_version = _elf_version
	#pragma weak	elf32_xlatetof = _elf32_xlatetof
	#pragma weak	elf32_xlatetom = _elf32_xlatetom
#endif


#include "syn.h"
#include <memory.h>
#include <libelf.h>
#include <link.h>
#include "decl.h"
#include "msg.h"


/* ELF translation routines
 *	These routines make a subtle implicit assumption.
 *	The file representations of all structures are "packed,"
 *	meaning no implicit padding bytes occur.  This might not
 *	be the case for the memory representations.  Consequently,
 *	the memory representations ALWAYS contain at least as many
 *	bytes as the file representations.  Otherwise, the memory
 *	structures would lose information, meaning they're not
 *	implemented properly.
 *
 *	The words above apply to structures with the same members.
 *	If a future version changes the number of members, the
 *	relative structure sizes for different version must be
 *	tested with the compiler.
 */


#define HI32	0x80000000UL
#define LO31	0x7fffffffUL


/*	These macros create indexes for accessing the bytes of
 *	words and halfwords for ELFCLASS32 data representations
 *	(currently ELFDATA2LSB and ELFDATA2MSB).  In all cases,
 *
 *	w = (((((X_3 << 8) + X_2) << 8) + X_1) << 8) + X_0
 *	h = (X_1 << 8) + X_0
 *
 *	These assume the file representations for Addr, Off,
 *	Sword, and Word use 4 bytes, but the memory def's for
 *	the types may differ.
 *
 *	Naming convention:
 *		..._L	ELFDATA2LSB
 *		..._M	ELFDATA2MSB
 *
 *	enuma_*(n)	define enum names for addr n
 *	enumb_*(n)	define enum names for byte n
 *	enumh_*(n)	define enum names for half n
 *	enumo_*(n)	define enum names for off n
 *	enumw_*(n)	define enum names for word n
 *	tofa(d,s,n)	xlate addr n from mem s to file d
 *	tofb(d,s,n)	xlate byte n from mem s to file d
 *	tofh(d,s,n)	xlate half n from mem s to file d
 *	tofo(d,s,n)	xlate off n from mem s to file d
 *	tofw(d,s,n)	xlate word n from mem s to file d
 *	toma(s,n)	xlate addr n from file s to expression value
 *	tomb(s,n)	xlate byte n from file s to expression value
 *	tomh(s,n)	xlate half n from file s to expression value
 *	tomo(s,n)	xlate off n from file s to expression value
 *	tomw(s,n)	xlate word n from file s to expression value
 *
 *	tof*() macros must move a multi-byte value into a temporary
 *	because ``in place'' conversions are allowed.  If a temp is not
 *	used for multi-byte objects, storing an initial destination byte
 *	may clobber a source byte not yet examined.
 *
 *	tom*() macros compute an expression value from the source
 *	without touching the destination; so they're safe.
 */

define(enuma_L, `$1_L0, $1_L1, $1_L2, $1_L3')dnl
define(enuma_M, `$1_M3, $1_M2, $1_M1, $1_M0')dnl
define(enumb_L, `$1_L')dnl
define(enumb_M, `$1_M')dnl
define(enumh_L, `$1_L0, $1_L1')dnl
define(enumh_M, `$1_M1, $1_M0')dnl
define(enumo_L, `$1_L0, $1_L1, $1_L2, $1_L3')dnl
define(enumo_M, `$1_M3, $1_M2, $1_M1, $1_M0')dnl
define(enumw_L, `$1_L0, $1_L1, $1_L2, $1_L3')dnl
define(enumw_M, `$1_M3, $1_M2, $1_M1, $1_M0')dnl

define(tofa, `{ register Elf32_Addr _t_ = $2;
		($1)[$3`'0] = (unsigned char)_t_,
		($1)[$3`'1] = (unsigned char)(_t_>>8),
		($1)[$3`'2] = (unsigned char)(_t_>>16),
		($1)[$3`'3] = (unsigned char)(_t_>>24); }')dnl
define(tofb, `($1)[$3] = (unsigned char)($2)')dnl
define(tofh, `{ register Elf32_Half _t_ = $2;
		($1)[$3`'0] = (unsigned char)_t_,
		($1)[$3`'1] = (unsigned char)(_t_>>8); }')dnl
define(tofo, `{ register Elf32_Off _t_ = $2;
		($1)[$3`'0] = (unsigned char)_t_,
		($1)[$3`'1] = (unsigned char)(_t_>>8),
		($1)[$3`'2] = (unsigned char)(_t_>>16),
		($1)[$3`'3] = (unsigned char)(_t_>>24); }')dnl
define(tofw, `{ register Elf32_Word _t_ = $2;
		($1)[$3`'0] = (unsigned char)_t_,
		($1)[$3`'1] = (unsigned char)(_t_>>8),
		($1)[$3`'2] = (unsigned char)(_t_>>16),
		($1)[$3`'3] = (unsigned char)(_t_>>24); }')dnl

define(toma, `(((((((Elf32_Addr)($1)[$2`'3]<<8)
		+($1)[$2`'2])<<8)
		+($1)[$2`'1])<<8)
		+($1)[$2`'0])')dnl
define(tomb, `((unsigned char)($1)[$2])')dnl
define(tomh, `(((Elf32_Half)($1)[$2`'1]<<8)+($1)[$2`'0])')dnl
define(tomo, `(((((((Elf32_Off)($1)[$2`'3]<<8)
		+($1)[$2`'2])<<8)
		+($1)[$2`'1])<<8)
		+($1)[$2`'0])')dnl
define(tomw, `(((((((Elf32_Word)($1)[$2`'3]<<8)
		+($1)[$2`'2])<<8)
		+($1)[$2`'1])<<8)
		+($1)[$2`'0])')dnl


/* ELF data object indexes
 *	The enums are broken apart to get around deficiencies
 *	in some compilers.
 */

define(Addr, `
enum
{
	enuma_$1(A)`'ifelse(`$2', `', `', `,
	A_sizeof')
};')

Addr(L)
Addr(M,1)


define(Half, `
enum
{
	enumh_$1(H)`'ifelse(`$2', `', `', `,
	H_sizeof')
};')

Half(L)
Half(M,1)


define(Off, `
enum
{
	enumo_$1(O)`'ifelse(`$2', `', `', `,
	O_sizeof')
};')

Off(L)
Off(M,1)


define(Word, `
enum
{
	enumw_$1(W)`'ifelse(`$2', `', `', `,
	W_sizeof')
};')

Word(L)
Word(M,1)


define(Dyn_1, `
enum
{
	enumw_$1(D1_tag),
	enumw_$1(D1_val)`'ifelse(`$2', `', `', `,
	D1_sizeof')
};')

Dyn_1(L)
Dyn_1(M,1)


#define E1_Nident	16

define(Ehdr_1, `
enum
{
	ifelse(`$2', `', `E1_ident, ')E1_ident_$1_Z = E1_Nident - 1,
	enumh_$1(E1_type),
	enumh_$1(E1_machine),
	enumw_$1(E1_version),
	enuma_$1(E1_entry),
	enumo_$1(E1_phoff),
	enumo_$1(E1_shoff),
	enumw_$1(E1_flags),
	enumh_$1(E1_ehsize),
	enumh_$1(E1_phentsize),
	enumh_$1(E1_phnum),
	enumh_$1(E1_shentsize),
	enumh_$1(E1_shnum),
	enumh_$1(E1_shstrndx)`'ifelse(`$2', `', `', `,
	E1_sizeof')
};')

Ehdr_1(L)
Ehdr_1(M,1)


define(Phdr_1, `
enum
{
	enumw_$1(P1_type),
	enumo_$1(P1_offset),
	enuma_$1(P1_vaddr),
	enuma_$1(P1_paddr),
	enumw_$1(P1_filesz),
	enumw_$1(P1_memsz),
	enumw_$1(P1_flags),
	enumw_$1(P1_align)`'ifelse(`$2', `', `', `,
	P1_sizeof')
};')

Phdr_1(L)
Phdr_1(M,1)


define(Rel_1, `
enum
{
	enuma_$1(R1_offset),
	enumw_$1(R1_info)`'ifelse(`$2', `', `', `,
	R1_sizeof')
};')

Rel_1(L)
Rel_1(M,1)


define(Rela_1, `
enum
{
	enuma_$1(RA1_offset),
	enumw_$1(RA1_info),
	enumw_$1(RA1_addend)`'ifelse(`$2', `', `', `,
	RA1_sizeof')
};')

Rela_1(L)
Rela_1(M,1)


define(Shdr_1, `
enum
{
	enumw_$1(SH1_name),
	enumw_$1(SH1_type),
	enumw_$1(SH1_flags),
	enuma_$1(SH1_addr),
	enumo_$1(SH1_offset),
	enumw_$1(SH1_size),
	enumw_$1(SH1_link),
	enumw_$1(SH1_info),
	enumw_$1(SH1_addralign),
	enumw_$1(SH1_entsize)`'ifelse(`$2', `', `', `,
	SH1_sizeof')
};')

Shdr_1(L)
Shdr_1(M,1)


define(Sym_1, `
enum
{
	enumw_$1(ST1_name),
	enuma_$1(ST1_value),
	enumw_$1(ST1_size),
	enumb_$1(ST1_info),
	enumb_$1(ST1_other),
	enumh_$1(ST1_shndx)`'ifelse(`$2', `', `', `,
	ST1_sizeof')
};')

Sym_1(L)
Sym_1(M,1)

define(Verdef_1, `
enum
{
	enumh_$1(VD1_version),
	enumh_$1(VD1_flags),
	enumh_$1(VD1_ndx),
	enumh_$1(VD1_cnt),
	enumw_$1(VD1_hash),
	enumw_$1(VD1_aux),
	enumw_$1(VD1_next)`'ifelse(`$2', `', `', `,
	VD1_sizeof')
};')

Verdef_1(L)
Verdef_1(M,1)

define(Verdaux_1, `
enum
{
	enuma_$1(VDA1_name),
	enumw_$1(VDA1_next)`'ifelse(`$2', `', `', `,
	VDA1_sizeof')
};')

Verdaux_1(L)
Verdaux_1(M,1)

define(Verneed_1, `
enum
{
	enumh_$1(VN1_version),
	enumh_$1(VN1_cnt),
	enuma_$1(VN1_file),
	enumw_$1(VN1_aux),
	enumw_$1(VN1_next)`'ifelse(`$2', `', `', `,
	VN1_sizeof')
};')

Verneed_1(L)
Verneed_1(M,1)

define(Vernaux_1, `
enum
{
	enumw_$1(VNA1_hash),
	enumh_$1(VNA1_flags),
	enumh_$1(VNA1_other),
	enuma_$1(VNA1_name),
	enumw_$1(VNA1_next)`'ifelse(`$2', `', `', `,
	VNA1_sizeof')
};')

Vernaux_1(L)
Vernaux_1(M,1)


/*	Translation function declarations.
 *
 *		<object>_<data><dver><sver>_tof
 *		<object>_<data><dver><sver>_tom
 *	where
 *		<data>	2L	ELFDATA2LSB
 *			2M	ELFDATA2MSB
 */

static void	addr_2L_tof(), addr_2L_tom(),
		addr_2M_tof(), addr_2M_tom(),
		byte_to(),
		dyn_2L11_tof(), dyn_2L11_tom(),
		dyn_2M11_tof(), dyn_2M11_tom(),
		ehdr_2L11_tof(), ehdr_2L11_tom(),
		ehdr_2M11_tof(), ehdr_2M11_tom(),
		half_2L_tof(), half_2L_tom(),
		half_2M_tof(), half_2M_tom(),
		off_2L_tof(), off_2L_tom(),
		off_2M_tof(), off_2M_tom(),
		phdr_2L11_tof(), phdr_2L11_tom(),
		phdr_2M11_tof(), phdr_2M11_tom(),
		rel_2L11_tof(), rel_2L11_tom(),
		rel_2M11_tof(), rel_2M11_tom(),
		rela_2L11_tof(), rela_2L11_tom(),
		rela_2M11_tof(), rela_2M11_tom(),
		shdr_2L11_tof(), shdr_2L11_tom(),
		shdr_2M11_tof(), shdr_2M11_tom(),
		sword_2L_tof(), sword_2L_tom(),
		sword_2M_tof(), sword_2M_tom(),
		sym_2L11_tof(), sym_2L11_tom(),
		sym_2M11_tof(), sym_2M11_tom(),
		word_2L_tof(), word_2L_tom(),
		word_2M_tof(), word_2M_tom(),
		verdef_2L11_tof(), verdef_2L11_tom(),
		verdef_2M11_tof(), verdef_2M11_tom(),
		verneed_2L11_tof(), verneed_2L11_tom(),
		verneed_2M11_tof(), verneed_2M11_tom();


/*	x32 [dst_version - 1] [src_version - 1] [encode - 1] [type]
 */

static struct
{
	void	(*x_tof)(),
		(*x_tom)();
} x32 [EV_CURRENT] [EV_CURRENT] [ELFDATANUM - 1] [ELF_T_NUM] =
{
	{
		{
			{			/* [1-1][1-1][2LSB-1][.] */
/* BYTE */			{ byte_to, byte_to },
/* ADDR */			{ addr_2L_tof, addr_2L_tom },
/* DYN */			{ dyn_2L11_tof, dyn_2L11_tom },
/* EHDR */			{ ehdr_2L11_tof, ehdr_2L11_tom },
/* HALF */			{ half_2L_tof, half_2L_tom },
/* OFF */			{ off_2L_tof, off_2L_tom },
/* PHDR */			{ phdr_2L11_tof, phdr_2L11_tom },
/* RELA */			{ rela_2L11_tof, rela_2L11_tom },
/* REL */			{ rel_2L11_tof, rel_2L11_tom },
/* SHDR */			{ shdr_2L11_tof, shdr_2L11_tom },
/* SWORD */			{ sword_2L_tof, sword_2L_tom },
/* SYM */			{ sym_2L11_tof, sym_2L11_tom },
/* WORD */			{ word_2L_tof, word_2L_tom },
/* VERDEF */			{ verdef_2L11_tof, verdef_2L11_tom},
/* VERNEED */			{ verneed_2L11_tof, verneed_2L11_tom},
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
			},
			{			/* [1-1][1-1][2MSB-1][.] */
/* BYTE */			{ byte_to, byte_to },
/* ADDR */			{ addr_2M_tof, addr_2M_tom },
/* DYN */			{ dyn_2M11_tof, dyn_2M11_tom },
/* EHDR */			{ ehdr_2M11_tof, ehdr_2M11_tom },
/* HALF */			{ half_2M_tof, half_2M_tom },
/* OFF */			{ off_2M_tof, off_2M_tom },
/* PHDR */			{ phdr_2M11_tof, phdr_2M11_tom },
/* RELA */			{ rela_2M11_tof, rela_2M11_tom },
/* REL */			{ rel_2M11_tof, rel_2M11_tom },
/* SHDR */			{ shdr_2M11_tof, shdr_2M11_tom },
/* SWORD */			{ sword_2M_tof, sword_2M_tom },
/* SYM */			{ sym_2M11_tof, sym_2M11_tom },
/* WORD */			{ word_2M_tof, word_2M_tom },
/* VERDEF */			{ verdef_2M11_tof, verdef_2M11_tom},
/* VERNEED */			{ verneed_2M11_tof, verneed_2M11_tom},
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
			},
		},
	},
};


/*	size [version - 1] [type]
 */

static const struct
{
	size_t	s_filesz,
		s_memsz;
} fmsize [EV_CURRENT] [ELF_T_NUM] =
{
	{					/* [1-1][.] */
/* BYTE */	{ 1, 1 },
/* ADDR */	{ A_sizeof, sizeof(Elf32_Addr) },
/* DYN */	{ D1_sizeof, sizeof(Elf32_Dyn) },
/* EHDR */	{ E1_sizeof, sizeof(Elf32_Ehdr) },
/* HALF */	{ H_sizeof, sizeof(Elf32_Half) },
/* OFF */	{ O_sizeof, sizeof(Elf32_Off) },
/* PHDR */	{ P1_sizeof, sizeof(Elf32_Phdr) },
/* RELA */	{ RA1_sizeof, sizeof(Elf32_Rela) },
/* REL */	{ R1_sizeof, sizeof(Elf32_Rel) },
/* SHDR */	{ SH1_sizeof, sizeof(Elf32_Shdr) },
/* SWORD */	{ W_sizeof, sizeof(Elf32_Sword) },
/* SYM */	{ ST1_sizeof, sizeof(Elf32_Sym) },
/* WORD */	{ W_sizeof, sizeof(Elf32_Word) },
/* VERDEF */	{ 1, 1},	/* because bot VERDEF & VERNEED have varying */
/* VERNEED */	{ 1, 1},	/* sized structures we set their sizes */
				/* to 1 byte */
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
	},
};


/*	memory type [version - 1] [section type]
 */

static const Elf_Type	mtype[EV_CURRENT][SHT_NUM] =
{ 
	{			/* [1-1][.] */
/* NULL */	ELF_T_BYTE,
/* PROGBITS */	ELF_T_BYTE,
/* SYMTAB */	ELF_T_SYM,
/* STRTAB */	ELF_T_BYTE,
/* RELA */	ELF_T_RELA,
/* HASH */	ELF_T_WORD,
/* DYNAMIC */	ELF_T_DYN,
/* NOTE */	ELF_T_BYTE,
/* NOBITS */	ELF_T_BYTE,
/* REL */	ELF_T_REL,
/* SHLIB */	ELF_T_BYTE,
/* DYNSYM */	ELF_T_SYM,
	},
};


size_t
_elf32_entsz(Elf32_Word shtype, unsigned ver)
{
	register Elf_Type	ttype;

	if (shtype >= sizeof(mtype[0]) / sizeof(mtype[0][0])
	|| (ttype = mtype[ver - 1][shtype]) == ELF_T_BYTE)
		return 0;
	return fmsize[ver - 1][ttype].s_filesz;
}


size_t
elf32_fsize(Elf_Type type, size_t count, unsigned ver)
{
	if (--ver >= EV_CURRENT)
	{
		_elf_seterr(EREQ_VER, 0);
		return 0;
	}
	if ((unsigned)type >= ELF_T_NUM)
	{
		_elf_seterr(EREQ_TYPE, 0);
		return 0;
	}
	return fmsize[ver][type].s_filesz * count;
}


size_t
_elf32_msize(Elf_Type type, unsigned ver)
{
	return fmsize[ver - 1][type].s_memsz;
}


Elf_Type
_elf32_mtype(Elf32_Word shtype, unsigned ver)
{
	if (shtype < SHT_NUM)
		return mtype[ver - 1][shtype];

	switch (shtype) {
	case SHT_SUNW_verdef:
		return ELF_T_VDEF;
	case SHT_SUNW_verneed:
		return ELF_T_VNEED;
	case SHT_SUNW_versym:
		return ELF_T_HALF;
	};
	return ELF_T_BYTE;
}


/*
 * XX64	This routine is also used to 'version' interactions with Elf64
 *	applications, but there's no way to figure out if the caller is
 *	asking Elf32 or Elf64 questions, even though it has Elf32
 *	dependencies.  Ick.
 */
unsigned
elf_version(unsigned ver)
{
	register unsigned	j;
	union
	{
		Elf32_Word	w;
		unsigned char	c[W_sizeof];
	} u;



	if (ver == EV_NONE)
		return EV_CURRENT;
	if (ver > EV_CURRENT)
	{
		_elf_seterr(EREQ_VER, 0);
		return EV_NONE;
	}
	(void) mutex_lock(&_elf_globals_mutex);
	if (_elf_work != EV_NONE)
	{
		j = _elf_work;
		_elf_work = ver;
		(void) mutex_unlock(&_elf_globals_mutex);
		return j;
	}
	_elf_work = ver;

	/*	This assumes signed and unsigned objects of the same
	 *	size use the same representation.  It also assumes
	 *	that 4-byte compatibility implies 2-byte compatibility.
	 */

	for (j = 0; j < ELF_T_NUM; ++j)
		if (fmsize[0][j].s_filesz != fmsize[0][j].s_memsz) {
			(void) mutex_unlock(&_elf_globals_mutex);
			return ver;
		}
	u.w = 0x10203;
	/*CONSTANTCONDITION*/
	if (~(Elf32_Word)0 == -(Elf32_Sword)1
	&& tomw(u.c, W_L) == 0x10203)
		_elf_encode = ELFDATA2LSB;
	/*CONSTANTCONDITION*/
	else if (~(Elf32_Word)0 == -(Elf32_Sword)1
	&& tomw(u.c, W_M) == 0x10203)
		_elf_encode = ELFDATA2MSB;

	(void) mutex_unlock(&_elf_globals_mutex);

	return ver;
}


static Elf_Data *
xlate( Elf_Data *dst, const Elf_Data *src, unsigned encode, int	tof)
						/* tof !0 -> xlatetof */
{
	size_t		cnt, dsz, ssz;
	unsigned	type;
	unsigned	dver, sver;
	void		(*f)();
	unsigned	_encode;

	if (dst == 0 || src == 0)
		return 0;
	if (--encode >= (ELFDATANUM - 1))
	{
		_elf_seterr(EREQ_ENCODE, 0);
		return 0;
	}
	if ((dver = dst->d_version - 1) >= EV_CURRENT
	|| (sver = src->d_version - 1) >= EV_CURRENT)
	{
		_elf_seterr(EREQ_VER, 0);
		return 0;
	}
	if ((type = src->d_type) >= ELF_T_NUM)
	{
		_elf_seterr(EREQ_TYPE, 0);
		return 0;
	}

	if (tof)
	{
		dsz = fmsize[dver][type].s_filesz;
		ssz = fmsize[sver][type].s_memsz;
		f = x32[dver][sver][encode][type].x_tof;
	}
	else
	{
		dsz = fmsize[dver][type].s_memsz;
		ssz = fmsize[sver][type].s_filesz;
		f = x32[dver][sver][encode][type].x_tom;
	}
	cnt = src->d_size / ssz;
	if ( dst->d_size < dsz * cnt)
	{
		_elf_seterr(EREQ_DSZ, 0);
		return 0;
	}

	ELFACCESSDATA(_encode, _elf_encode)
	if ((_encode == (encode + 1)) && (dsz == ssz))
	{
		/*
		 *      ld(1) frequently produces empty sections (eg. .dynsym,
		 *      .dynstr, .symtab, .strtab, etc) so that the initial
		 *      output image can be created of the correct size.  Later
		 *      these sections are filled in with the associated data.
		 *      So that we don't have to pre-allocate buffers for
		 *      these segments, allow for the src destination to be 0.
		 */
		if (src->d_buf && src->d_buf != dst->d_buf)
			(void)memcpy(dst->d_buf, src->d_buf, src->d_size);
		dst->d_type = src->d_type;
		dst->d_size = src->d_size;
		return dst;
	}
	if ( cnt )
		(*f)(dst->d_buf, src->d_buf, cnt);
	dst->d_size = dsz * cnt;
	dst->d_type = src->d_type;
	return dst;
}


Elf_Data *
elf32_xlatetof(Elf_Data *dst, const Elf_Data *src, unsigned encode)
{
	return xlate(dst, src, encode, 1);
}


Elf_Data *
elf32_xlatetom(Elf_Data *dst, const Elf_Data *src, unsigned encode)
{
	return xlate(dst, src, encode, 0);
}


/* xlate to file format
 *
 *	..._tof(name, data) -- macros
 *
 *	Recall that the file format must be no larger than the
 *	memory format (equal versions).  Use "forward" copy.
 *	All these routines require non-null, non-zero arguments.
 */

define(addr_tof, `
static void
$1(unsigned char *dst, Elf32_Addr *src, size_t cnt)
{
	register Elf32_Addr	*end = src + cnt;

	do
	{
		tofa(dst, *src, A_$2);
		dst += A_sizeof;
	} while (++src < end);
}')

addr_tof(addr_2L_tof,L)
addr_tof(addr_2M_tof,M)


static void
byte_to(unsigned char *dst, unsigned char *src, size_t cnt)
{
	if (dst != src)
		(void)memcpy(dst, src, cnt);
}


define(dyn_11_tof, `
static void
$1(unsigned char *dst, Elf32_Dyn *src, size_t cnt)
{
	Elf32_Dyn		*end = src + cnt;

	do
	{
		tofw(dst, src->d_tag, D1_tag_$2);
		tofo(dst, src->d_un.d_val, D1_val_$2);
		dst += D1_sizeof;
	} while (++src < end);
}')

dyn_11_tof(dyn_2L11_tof,L)
dyn_11_tof(dyn_2M11_tof,M)


define(ehdr_11_tof, `
static void
$1(unsigned char *dst, Elf32_Ehdr *src, size_t cnt)
{
	Elf32_Ehdr		*end = src + cnt;

	do
	{
		if (&dst[E1_ident] != src->e_ident)
			(void)memcpy(&dst[E1_ident], src->e_ident, E1_Nident);
		tofh(dst, src->e_type, E1_type_$2);
		tofh(dst, src->e_machine, E1_machine_$2);
		tofw(dst, src->e_version, E1_version_$2);
		tofa(dst, src->e_entry, E1_entry_$2);
		tofo(dst, src->e_phoff, E1_phoff_$2);
		tofo(dst, src->e_shoff, E1_shoff_$2);
		tofw(dst, src->e_flags, E1_flags_$2);
		tofh(dst, src->e_ehsize, E1_ehsize_$2);
		tofh(dst, src->e_phentsize, E1_phentsize_$2);
		tofh(dst, src->e_phnum, E1_phnum_$2);
		tofh(dst, src->e_shentsize, E1_shentsize_$2);
		tofh(dst, src->e_shnum, E1_shnum_$2);
		tofh(dst, src->e_shstrndx, E1_shstrndx_$2);
		dst += E1_sizeof;
	} while (++src < end);
}')

ehdr_11_tof(ehdr_2L11_tof,L)
ehdr_11_tof(ehdr_2M11_tof,M)


define(half_tof, `
static void
$1(unsigned char *dst, Elf32_Half *src, size_t cnt)
{
	register Elf32_Half	*end = src + cnt;

	do
	{
		tofh(dst, *src, H_$2);
		dst += H_sizeof;
	} while (++src < end);
}')

half_tof(half_2L_tof,L)
half_tof(half_2M_tof,M)


define(off_tof, `
static void
$1(unsigned char *dst, Elf32_Off *src, size_t cnt)
{
	register Elf32_Off	*end = src + cnt;

	do
	{
		tofo(dst, *src, O_$2);
		dst += O_sizeof;
	} while (++src < end);
}')

off_tof(off_2L_tof,L)
off_tof(off_2M_tof,M)

define(phdr_11_tof, `
static void
$1(unsigned char *dst, Elf32_Phdr *src, size_t cnt)
{
	Elf32_Phdr		*end = src + cnt;

	do
	{
		tofw(dst, src->p_type, P1_type_$2);
		tofo(dst, src->p_offset, P1_offset_$2);
		tofa(dst, src->p_vaddr, P1_vaddr_$2);
		tofa(dst, src->p_paddr, P1_paddr_$2);
		tofw(dst, src->p_filesz, P1_filesz_$2);
		tofw(dst, src->p_memsz, P1_memsz_$2);
		tofw(dst, src->p_flags, P1_flags_$2);
		tofw(dst, src->p_align, P1_align_$2);
		dst += P1_sizeof;
	} while (++src < end);
}')

phdr_11_tof(phdr_2L11_tof,L)
phdr_11_tof(phdr_2M11_tof,M)


define(rel_11_tof, `
static void
$1(unsigned char *dst, Elf32_Rel *src, size_t cnt)
{
	Elf32_Rel		*end = src + cnt;

	do
	{
		tofa(dst, src->r_offset, R1_offset_$2);
		tofw(dst, src->r_info, R1_info_$2);
		dst += R1_sizeof;
	} while (++src < end);
}')

rel_11_tof(rel_2L11_tof,L)
rel_11_tof(rel_2M11_tof,M)


define(rela_11_tof, `
static void
$1(unsigned char *dst, Elf32_Rela *src, size_t cnt)
{
	Elf32_Rela		*end = src + cnt;

	do
	{
		tofa(dst, src->r_offset, RA1_offset_$2);
		tofw(dst, src->r_info, RA1_info_$2);
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1)	/* 2s comp */
		{
			tofw(dst, src->r_addend, RA1_addend_$2);
		}
		else					/* other */
		{
			register Elf32_Word w;

			if (src->r_addend < 0)
			{
				w = - src->r_addend;
				w = ~w + 1;
			}
			else
				w = src->r_addend;
			tofw(dst, w, RA1_addend_$2);
		}
		dst += RA1_sizeof;
	} while (++src < end);
}')

rela_11_tof(rela_2L11_tof,L)
rela_11_tof(rela_2M11_tof,M)


define(shdr_11_tof, `
static void
$1(unsigned char *dst, Elf32_Shdr *src, size_t cnt)
{
	Elf32_Shdr		*end = src + cnt;

	do
	{
		tofw(dst, src->sh_name, SH1_name_$2);
		tofw(dst, src->sh_type, SH1_type_$2);
		tofw(dst, src->sh_flags, SH1_flags_$2);
		tofa(dst, src->sh_addr, SH1_addr_$2);
		tofo(dst, src->sh_offset, SH1_offset_$2);
		tofw(dst, src->sh_size, SH1_size_$2);
		tofw(dst, src->sh_link, SH1_link_$2);
		tofw(dst, src->sh_info, SH1_info_$2);
		tofw(dst, src->sh_addralign, SH1_addralign_$2);
		tofw(dst, src->sh_entsize, SH1_entsize_$2);
		dst += SH1_sizeof;
	} while (++src < end);
}')

shdr_11_tof(shdr_2L11_tof,L)
shdr_11_tof(shdr_2M11_tof,M)


define(sword_tof, `
static void
$1(unsigned char *dst, Elf32_Sword *src, size_t cnt)
{
	register Elf32_Sword	*end = src + cnt;

	do
	{
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1)	/* 2s comp */
		{
			tofw(dst, *src, W_$2);
		}
		else					/* unknown */
		{
			register Elf32_Word w;

			if (*src < 0)
			{
				w = - *src;
				w = ~w + 1;
			}
			else
				w = *src;
			tofw(dst, w, W_$2);
		}
		dst += W_sizeof;
	} while (++src < end);
}')

sword_tof(sword_2L_tof,L)
sword_tof(sword_2M_tof,M)


define(sym_11_tof, `
static void
$1(unsigned char *dst, Elf32_Sym *src, size_t cnt)
{
	Elf32_Sym		*end = src + cnt;

	do
	{
		tofw(dst, src->st_name, ST1_name_$2);
		tofa(dst, src->st_value, ST1_value_$2);
		tofw(dst, src->st_size, ST1_size_$2);
		tofb(dst, src->st_info, ST1_info_$2);
		tofb(dst, src->st_other, ST1_other_$2);
		tofh(dst, src->st_shndx, ST1_shndx_$2);
		dst += ST1_sizeof;
	} while (++src < end);
}')

sym_11_tof(sym_2L11_tof,L)
sym_11_tof(sym_2M11_tof,M)


define(word_tof, `
static void
$1(unsigned char *dst, Elf32_Word *src, size_t cnt)
{
	register Elf32_Word	*end = src + cnt;

	do
	{
		tofw(dst, *src, W_$2);
		dst += W_sizeof;
	} while (++src < end);
}')

word_tof(word_2L_tof,L)
word_tof(word_2M_tof,M)

define(verdef_11_tof, `
static void
$1(unsigned char *dst, Elf32_Verdef *src, size_t cnt)
{
	/* LINTED */
	register Elf32_Verdef		*end = (Elf32_Verdef *)((char *)src +
					    cnt);

	do
	{
		Elf32_Verdef		*next_verdef;
		Elf32_Verdaux		*vaux;
		Elf32_Half		i;
		unsigned char *		vaux_dst;
		unsigned char *		dst_next;

		/* LINTED */
		next_verdef = (Elf32_Verdef *)(src->vd_next ?
		    (char *)src + src->vd_next : (char *)end);
		dst_next = dst + src->vd_next;

		/* LINTED */
		vaux = (Elf32_Verdaux *)((char *)src + src->vd_aux);
		vaux_dst = dst + src->vd_aux;

		/*
		 * Convert auxilary structures
		 */
		for (i = 0; i < src->vd_cnt; i++) {
			Elf32_Verdaux	*vaux_next;
			unsigned char	*vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the next
			 * location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
			vaux_dst_next = vaux_dst + vaux->vda_next;

			tofa(vaux_dst, vaux->vda_name, VDA1_name_$2);
			tofw(vaux_dst, vaux->vda_next, VDA1_next_$2);
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}

		/*
		 * Convert Elf32_Verdef structure.
		 */
		tofh(dst, src->vd_version, VD1_version_$2);
		tofh(dst, src->vd_flags, VD1_flags_$2);
		tofh(dst, src->vd_ndx, VD1_ndx_$2);
		tofh(dst, src->vd_cnt, VD1_cnt_$2);
		tofw(dst, src->vd_hash, VD1_hash_$2);
		tofw(dst, src->vd_aux, VD1_aux_$2);
		tofw(dst, src->vd_next, VD1_next_$2);
		src = next_verdef;
		dst = dst_next;
	} while (src < end);
}')

verdef_11_tof(verdef_2L11_tof, L)
verdef_11_tof(verdef_2M11_tof, M)

define(verneed_11_tof, `
static void
$1(unsigned char *dst, Elf32_Verneed *src, size_t cnt)
{
	/* LINTED */
	register Elf32_Verneed	*end = (Elf32_Verneed *)((char *)src + cnt);

	do
	{
		Elf32_Verneed *		next_verneed;
		Elf32_Vernaux *		vaux;
		Elf32_Half		i;
		unsigned char *		vaux_dst;
		unsigned char *		dst_next;

		/* LINTED */
		next_verneed = (Elf32_Verneed *)(src->vn_next ?
		    (char *)src + src->vn_next : (char *)end);
		dst_next = dst + src->vn_next;

		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)src + src->vn_aux);
		vaux_dst = dst + src->vn_aux;

		/*
		 * Convert auxilary structures first
		 */
		for (i = 0; i < src->vn_cnt; i++) {
			Elf32_Vernaux *	vaux_next;
			unsigned char *	vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the
			 * next location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
			vaux_dst_next = vaux_dst + vaux->vna_next;

			tofw(vaux_dst, vaux->vna_hash, VNA1_hash_$2);
			tofh(vaux_dst, vaux->vna_flags, VNA1_flags_$2);
			tofh(vaux_dst, vaux->vna_other, VNA1_other_$2);
			tofa(vaux_dst, vaux->vna_name, VNA1_name_$2);
			tofw(vaux_dst, vaux->vna_next, VNA1_next_$2);
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}
		/*
		 * Convert Elf32_Verneed structure.
		 */
		tofh(dst, src->vn_version, VN1_version_$2);
		tofh(dst, src->vn_cnt, VN1_cnt_$2);
		tofa(dst, src->vn_file, VN1_file_$2);
		tofw(dst, src->vn_aux, VN1_aux_$2);
		tofw(dst, src->vn_next, VN1_next_$2);
		src = next_verneed;
		dst = dst_next;
	} while (src < end);
}')

verneed_11_tof(verneed_2L11_tof, L)
verneed_11_tof(verneed_2M11_tof, M)


/* xlate to memory format
 *
 *	..._tom(name, data) -- macros
 *
 *	Recall that the memory format may be larger than the
 *	file format (equal versions).  Use "backward" copy.
 *	All these routines require non-null, non-zero arguments.
 */


define(addr_tom, `
static void
$1(Elf32_Addr *dst, unsigned char *src, size_t cnt)
{
	register Elf32_Addr	*end = dst;

	dst += cnt;
	src += cnt * A_sizeof;
	while (dst-- > end)
	{
		src -= A_sizeof;
		*dst = toma(src, A_$2);
	}
}')

addr_tom(addr_2L_tom,L)
addr_tom(addr_2M_tom,M)


define(dyn_11_tom, `
static void
$1(Elf32_Dyn *dst, unsigned char *src, size_t cnt)
{
	Elf32_Dyn		*end = dst + cnt;

	do
	{
		dst->d_tag = tomw(src, D1_tag_$2);
		dst->d_un.d_val = tomw(src, D1_val_$2);
		src += D1_sizeof;
	} while (++dst < end);
}')

dyn_11_tom(dyn_2L11_tom,L)
dyn_11_tom(dyn_2M11_tom,M)


define(ehdr_11_tom, `
static void
$1(Elf32_Ehdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Ehdr		*end = dst;

	dst += cnt;
	src += cnt * E1_sizeof;
	while (dst-- > end)
	{
		src -= E1_sizeof;
		dst->e_shstrndx = tomh(src, E1_shstrndx_$2);
		dst->e_shnum = tomh(src, E1_shnum_$2);
		dst->e_shentsize = tomh(src, E1_shentsize_$2);
		dst->e_phnum = tomh(src, E1_phnum_$2);
		dst->e_phentsize = tomh(src, E1_phentsize_$2);
		dst->e_ehsize = tomh(src, E1_ehsize_$2);
		dst->e_flags = tomw(src, E1_flags_$2);
		dst->e_shoff = tomo(src, E1_shoff_$2);
		dst->e_phoff = tomo(src, E1_phoff_$2);
		dst->e_entry = toma(src, E1_entry_$2);
		dst->e_version = tomw(src, E1_version_$2);
		dst->e_machine = tomh(src, E1_machine_$2);
		dst->e_type = tomh(src, E1_type_$2);
		if (dst->e_ident != &src[E1_ident])
			(void)memcpy(dst->e_ident, &src[E1_ident], E1_Nident);
	}
}')

ehdr_11_tom(ehdr_2L11_tom,L)
ehdr_11_tom(ehdr_2M11_tom,M)


define(half_tom, `
static void
$1(Elf32_Half *dst, unsigned char *src, size_t cnt)
{
	register Elf32_Half	*end = dst;

	dst += cnt;
	src += cnt * H_sizeof;
	while (dst-- > end)
	{
		src -= H_sizeof;
		*dst = tomh(src, H_$2);
	}
}')

half_tom(half_2L_tom,L)
half_tom(half_2M_tom,M)


define(off_tom, `
static void
$1(Elf32_Off *dst, unsigned char *src, size_t cnt)
{
	register Elf32_Off	*end = dst;

	dst += cnt;
	src += cnt * O_sizeof;
	while (dst-- > end)
	{
		src -= O_sizeof;
		*dst = tomo(src, O_$2);
	}
}')

off_tom(off_2L_tom,L)
off_tom(off_2M_tom,M)

define(phdr_11_tom, `
static void
$1(Elf32_Phdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Phdr		*end = dst;

	dst += cnt;
	src += cnt * P1_sizeof;
	while (dst-- > end)
	{
		src -= P1_sizeof;
		dst->p_align = tomw(src, P1_align_$2);
		dst->p_flags = tomw(src, P1_flags_$2);
		dst->p_memsz = tomw(src, P1_memsz_$2);
		dst->p_filesz = tomw(src, P1_filesz_$2);
		dst->p_paddr = toma(src, P1_paddr_$2);
		dst->p_vaddr = toma(src, P1_vaddr_$2);
		dst->p_offset = tomo(src, P1_offset_$2);
		dst->p_type = tomw(src, P1_type_$2);
	}
}')

phdr_11_tom(phdr_2L11_tom,L)
phdr_11_tom(phdr_2M11_tom,M)


define(rel_11_tom, `
static void
$1(Elf32_Rel *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rel		*end = dst;

	dst += cnt;
	src += cnt * R1_sizeof;
	while (dst-- > end)
	{
		src -= R1_sizeof;
		dst->r_info = tomw(src, R1_info_$2);
		dst->r_offset = toma(src, R1_offset_$2);
	}
}')

rel_11_tom(rel_2L11_tom,L)
rel_11_tom(rel_2M11_tom,M)


define(rela_11_tom, `
static void
$1(Elf32_Rela *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rela		*end = dst;

	dst += cnt;
	src += cnt * RA1_sizeof;
	while (dst-- > end)
	{
		src -= RA1_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1	/* 32-bit 2s comp */
		&& ~(~(Elf32_Word)0 >> 1) == HI32)
		{
			dst->r_addend = tomw(src, RA1_addend_$2);
		}
		else					/* other */
		{
			union { Elf32_Word w; Elf32_Sword sw; } u;

			if ((u.w = tomw(src, RA1_addend_$2)) & HI32)
			{
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			dst->r_addend = u.sw;
		}
		dst->r_info = tomw(src, RA1_info_$2);
		dst->r_offset = toma(src, RA1_offset_$2);
	}
}')

rela_11_tom(rela_2L11_tom,L)
rela_11_tom(rela_2M11_tom,M)


define(shdr_11_tom, `
static void
$1(Elf32_Shdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Shdr		*end = dst;

	dst += cnt;
	src += cnt * SH1_sizeof;
	while (dst-- > end)
	{
		src -= SH1_sizeof;
		dst->sh_entsize = tomw(src, SH1_entsize_$2);
		dst->sh_addralign = tomw(src, SH1_addralign_$2);
		dst->sh_info = tomw(src, SH1_info_$2);
		dst->sh_link = tomw(src, SH1_link_$2);
		dst->sh_size = tomw(src, SH1_size_$2);
		dst->sh_offset = tomo(src, SH1_offset_$2);
		dst->sh_addr = toma(src, SH1_addr_$2);
		dst->sh_flags = tomw(src, SH1_flags_$2);
		dst->sh_type = tomw(src, SH1_type_$2);
		dst->sh_name = tomw(src, SH1_name_$2);
	}
}')

shdr_11_tom(shdr_2L11_tom,L)
shdr_11_tom(shdr_2M11_tom,M)


define(sword_tom, `
static void
$1(Elf32_Sword *dst, unsigned char *src, size_t cnt)
{
	register Elf32_Sword	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end)
	{
		src -= W_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1	/* 32-bit 2s comp */
		&& ~(~(Elf32_Word)0 >> 1) == HI32)
		{
			*dst = tomw(src, W_$2);
		}
		else					/* other */
		{
			union { Elf32_Word w; Elf32_Sword sw; } u;

			if ((u.w = tomw(src, W_$2)) & HI32)
			{
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			*dst = u.sw;
		}
	}
}')

sword_tom(sword_2L_tom,L)
sword_tom(sword_2M_tom,M)


define(sym_11_tom, `
static void
$1(Elf32_Sym *dst, unsigned char *src, size_t cnt)
{
	Elf32_Sym		*end = dst;

	dst += cnt;
	src += cnt * ST1_sizeof;
	while (dst-- > end)
	{
		src -= ST1_sizeof;
		dst->st_shndx = tomh(src, ST1_shndx_$2);
		dst->st_other = tomb(src, ST1_other_$2);
		dst->st_info = tomb(src, ST1_info_$2);
		dst->st_size = tomw(src, ST1_size_$2);
		dst->st_value = toma(src, ST1_value_$2);
		dst->st_name = tomw(src, ST1_name_$2);
	}
}')

sym_11_tom(sym_2L11_tom,L)
sym_11_tom(sym_2M11_tom,M)


define(word_tom, `
static void
$1(Elf32_Word *dst, unsigned char *src, size_t cnt)
{
	register Elf32_Word	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end)
	{
		src -= W_sizeof;
		*dst = tomw(src, W_$2);
	}
}')

word_tom(word_2L_tom,L)
word_tom(word_2M_tom,M)


define(verdef_11_tom, `
static void
$1(Elf32_Verdef *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	register Elf32_Verdef		*end = (Elf32_Verdef *)((char *)dst +
					    cnt);

	while (dst < end)
	{
		Elf32_Verdaux		*vaux;
		unsigned char	*src_vaux;
		Elf32_Half	i;

		dst->vd_version = tomh(src, VD1_version_$2);
		dst->vd_flags = tomh(src, VD1_flags_$2);
		dst->vd_ndx = tomh(src, VD1_ndx_$2);
		dst->vd_cnt = tomh(src, VD1_cnt_$2);
		dst->vd_hash = tomw(src, VD1_hash_$2);
		dst->vd_aux = tomw(src, VD1_aux_$2);
		dst->vd_next = tomw(src, VD1_next_$2);

		src_vaux = src + dst->vd_aux;
		/* LINTED */
		vaux = (Elf32_Verdaux*)((char *)dst + dst->vd_aux);
		for (i = 0; i < dst->vd_cnt; i++) {
			vaux->vda_name = toma(src_vaux, VDA1_name_$2);
			vaux->vda_next = toma(src_vaux, VDA1_next_$2);
			src_vaux += vaux->vda_next;
			/* LINTED */
			vaux = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
		}
		src += dst->vd_next;
		/* LINTED */
		dst = (Elf32_Verdef *)(dst->vd_next ?
		    (char *)dst + dst->vd_next : (char *)end);
	}
}')

verdef_11_tom(verdef_2L11_tom,L)
verdef_11_tom(verdef_2M11_tom,M)

define(verneed_11_tom, `
static void
$1(Elf32_Verneed *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	register Elf32_Verneed *	end = (Elf32_Verneed *)((char *)dst +
					    cnt);

	while (dst < end) {
		Elf32_Vernaux *	vaux;
		unsigned char *	src_vaux;
		Elf32_Half	i;
		dst->vn_version = tomh(src, VN1_version_$2);
		dst->vn_cnt = tomh(src, VN1_cnt_$2);
		dst->vn_file = toma(src, VN1_file_$2);
		dst->vn_aux = tomw(src, VN1_aux_$2);
		dst->vn_next = tomw(src, VN1_next_$2);

		src_vaux = src + dst->vn_aux;
		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)dst + dst->vn_aux);
		for (i = 0; i < dst->vn_cnt; i++) {
			vaux->vna_hash = tomw(src_vaux, VNA1_hash_$2);
			vaux->vna_flags = tomh(src_vaux, VNA1_flags_$2);
			vaux->vna_other = tomh(src_vaux, VNA1_other_$2);
			vaux->vna_name = toma(src_vaux, VNA1_name_$2);
			vaux->vna_next = tomw(src_vaux, VNA1_next_$2);
			src_vaux += vaux->vna_next;
			/* LINTED */
			vaux = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
		}
		src += dst->vn_next;
		/* LINTED */
		dst = (Elf32_Verneed *)(dst->vn_next ?
		    (char *)dst + dst->vn_next : (char *)end);
	}
}')

verneed_11_tom(verneed_2L11_tom,L)
verneed_11_tom(verneed_2M11_tom,M)
