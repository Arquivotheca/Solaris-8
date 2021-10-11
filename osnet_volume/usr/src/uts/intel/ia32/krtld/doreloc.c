/*
 * Copyright (c) 1996-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)doreloc.c	1.12	99/05/04 SMI"

#if	defined(_KERNEL)
#include	<sys/types.h>
#include	"reloc.h"
#else
#include	"sgs.h"
#include	"machdep.h"
#include	"libld.h"
#include	"reloc.h"
#include	"conv.h"
#include	"msg.h"
#endif

/*
 * This table represents the current relocations that do_reloc()
 * is able to process.  The relocations below that are marked
 * 'SPECIAL' in the comments are relocations that take special
 * processing and shouldn't actually ever be passed to do_reloc().
 */
const Rel_entry	reloc_table[R_386_NUM] = {
/* R_386_NONE */	{0, 0},
/* R_386_32 */		{FLG_RE_NOTREL, 4},
/* R_386_PC32 */	{FLG_RE_PCREL, 4},
/* R_386_GOT32 */	{FLG_RE_GOTADD, 4},
/* R_386_PLT32 */	{FLG_RE_PLTREL | FLG_RE_PCREL, 4},
/* R_386_COPY */	{0, 0},					/* SPECIAL */
/* R_386_GLOB_DAT */	{FLG_RE_NOTREL, 4},
/* R_386_JMP_SLOT */	{FLG_RE_NOTREL, 4},
/* R_386_RELATIVE */	{FLG_RE_NOTREL, 4},
/* R_386_GOTOFF */	{FLG_RE_PCREL | FLG_RE_GOTREL, 4},
/* R_386_GOTPC */	{FLG_RE_PCREL | FLG_RE_GOTPCREL, 4},
/* R_386_32PLT */	{FLG_RE_PLTREL, 4},
};


/*
 * Write a single relocated value to its reference location.
 * We assume we wish to add the relocatoin amount, value, to the
 * value of the address already present at the offset.
 *
 * NAME			VALUE	FIELD		CALCULATION
 *
 * R_386_NONE		 0	none		none
 * R_386_32		 1	word32		S + A
 * R_386_PC32		 2	word32		S + A - P
 * R_386_GOT32		 3	word32		G + A - P
 * R_386_PLT32		 4	word32		L + A - P
 * R_386_COPY		 5	none		none
 * R_386_GLOB_DAT	 6	word32		S
 * R_386_JMP_SLOT	 7	word32		S
 * R_386_RELATIVE	 8	word32		B + A
 * R_386_GOTOFF		 9	word32		S + A - GOT
 * R_386_GOTPC		10	word32		GOT + A - P
 * R_386_32PLT		11	word32		L + A
 *
 * Relocatoins 0-10 are from Figure 4-4: Relocations Types from the
 * intel ABI.  Relocation 11 (R_386_32PLT) is from the C++ intel abi
 * and is in the process of being registered with intel ABI (1/13/94).
 *
 * Relocation calculations:
 *
 * CALCULATION uses the following notation:
 *	A	the addend used
 *	B	the base address of the shared object in memory
 *	G	the offset into the global offset table
 *	GOT	the address of teh global offset table
 *	L	the procedure linkage entry
 *	P	the place of the storage unit being relocated
 *	S	the value of the symbol
 *
 * The calculations in the CALCULATION column are assumed to have
 * been performed before calling this function except for the addition of
 * the addresses in the instructions.
 */
int
do_reloc(unsigned char rtype, unsigned char *off, Xword *value,
	const char *sym, const char *file)
{
	const Rel_entry *rep;

	rep = &reloc_table[rtype];
	/*
	 * Currenty 386 *only* relocates against full words
	 */
	if (rep->re_fsize != 4) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNSUPSZ),
		    conv_reloc_386_type_str(rtype), file,
		    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)),
		    EC_WORD(rep->re_fsize));
		return (0);
	}
	/* LINTED */
	*((Xword *)off) += *value;
	return (1);
}
