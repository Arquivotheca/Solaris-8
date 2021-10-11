/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)doreloc.c	1.4	99/07/05 SMI"

#if	defined(_KERNEL)
#include	<sys/types.h>
#include	"reloc.h"
#include	<sys/elf_ia64.h>
#include	<sys/cmn_err.h>
#else
#include	"sgs.h"
#include	"machdep.h"
#include	"libld.h"
#include	"reloc.h"
#include	"conv.h"
#include	"msg.h"
#include	<sys/elf_ia64.h>
#endif


/*
 * This table represents the current relcoations that do_reloc() is able to
 * to process.  The relocations below that are marked SPECIAL are relocations
 * that take special processing and shouldn't actually ever be passed to
 * do_reloc().
 */

const Rel_entry reloc_table[R_IA_64_NUM] = {
/* 0x00 NONE */		{FLG_RE_NOTSUP, 0, 0},
/* 0x01 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x02 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x03 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x04 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x05 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x06 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x07 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x08 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x09 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0a null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0b null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0c null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0d null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0e null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x0f null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x20 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x10 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x11 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x12 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x13 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x14 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x15 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x16 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x17 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x18 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x19 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1a null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1b null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1c null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1d null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1e null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x1f null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x21 IMM14 */	{FLG_RE_NOTSUP, 4, 0},
/* 0x22 IMM22 */	{FLG_RE_NOTREL | FLG_RE_FRMOFF | FLG_RE_VERIFY, 0, 22},
/* 0x23 IMM64 */	{FLG_RE_NOTREL | FLG_RE_FRMOFF, 0, 0},
/* 0x24 DIR32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x25 DIR32LSB */	{FLG_RE_LSB, 4, 0},
/* 0x26 DIR64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x27 DIR64LSB */	{FLG_RE_LSB, 8, 0},
/* 0x28 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x29 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x2a GPREL22 */	{FLG_RE_GOTREL | FLG_RE_FRMOFF | FLG_RE_VERIFY |
				FLG_RE_SIGN, 0, 22},
/* 0x2b GPREL64I */	{FLG_RE_GOTREL, 0, 0},
/* 0x2c null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x2d null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x2e GPREL64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x2f GPREL64LSB */	{FLG_RE_GOTREL, 8, 0},
/* 0x30 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x31 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x32 LTOFF22 */	{FLG_RE_GOTADD | FLG_RE_FRMOFF | FLG_RE_VERIFY |
				FLG_RE_SIGN, 0, 22},
/* 0x33 LTOFF64I */	{FLG_RE_NOTSUP, 0, 0},
/* 0x34 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x35 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x36 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x37 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x38 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x39 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x3a PLTOFF22 */	{FLG_RE_NOTSUP, 0, 0},
/* 0x3b PLTOFF64I */	{FLG_RE_NOTSUP, 0, 0},
/* 0x3c null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x3d null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x3e PLTOFF64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x3f PLTOFF64LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x40 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x41 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x42 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x43 FPTR64I */	{FLG_RE_NOTSUP, 0, 0},
/* 0x44 FPTR32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x45 FPTR32LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x46 FPTR64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x47 FPTR64LSB */	{FLG_RE_FPTR, 8, 0},
/* 0x48 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x49 PCREL21B */	{FLG_RE_PLTREL |FLG_RE_PCREL | FLG_RE_FRMOFF |
			FLG_RE_VERIFY | FLG_RE_SIGN, 0, 25},
/* 0x4a PCREL21M */	{FLG_RE_NOTSUP, 0, 0},
/* 0x4b PCREL21F */	{FLG_RE_NOTSUP, 0, 0},
/* 0x4c PCREL32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x4d PCREL32LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x4e PCREL64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x4f PCREL64LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x50 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x51 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x52 LTOFF_FPTR22 */	{FLG_RE_FPTR | FLG_RE_GOTADD | FLG_RE_FRMOFF |
			FLG_RE_VERIFY | FLG_RE_SIGN, 0, 22},
/* 0x53 LTOFF_FPTR64I */ {FLG_RE_NOTSUP, 0, 0},
/* 0x54 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x55 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x56 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x57 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x58 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x59 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x5a null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x5b null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x5c SEGREL32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x5d SEGREL32LSB */	{FLG_RE_SEGREL, 4, 0},
/* 0x5e SEGREL64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x5f SEGREL64LSB */	{FLG_RE_SEGREL, 8, 0},
/* 0x60 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x61 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x62 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x63 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x64 SECREL32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x65 SECREL32LSB */	{FLG_RE_SECREL, 4, 0},
/* 0x66 SECREL64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x67 SECREL64LSB */	{FLG_RE_SECREL, 8, 0},
/* 0x68 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x69 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x6a null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x6b null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x6c REL32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x6d REL32LSB */	{FLG_RE_NOTREL, 4, 0},
/* 0x6e REL64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x6f REL64LSB */	{FLG_RE_NOTREL, 8, 0},
/* 0x70 LTV32MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x71 LTV32LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x72 LTV64MSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x73 LTV64LSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x74 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x75 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x76 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x77 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x78 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x79 null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7a null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7b null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7c null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7d null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7e null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x7f null */		{FLG_RE_NOTSUP, 0, 0},
/* 0x80 IPLTMSB */	{FLG_RE_NOTSUP, 0, 0},
/* 0x81 IPLTLSB */	{0, 0, 0}			/* SPECIAL */
};

/*
 * NAME			VALUE	FIELD			CALCULATION
 *
 * R_IA_64_NONE		0x00	none			none
 *	...whole...
 * R_IA_64_IMM14	0x21	instr. - imm14		S + A
 * R_IA_64_IMM22	0x22	instr. - imm22		S + A
 * R_IA_64_IMM64	0x23	instr. - imm64		S + A
 * R_IA_64_DIR32MSB	0x24	Word32 MSB		S + A
 * R_IA_64_DIR32LSB	0x25	Word32 LSB		S + A
 * R_IA_64_DIR64MSB	0x26	Word64 MSB		S + A
 * R_IA_64_DIR64LSB	0x27	Word64 LSB		S + A
 * 	...whole...
 * R_IA_64_GPREL22	0x2a	instr. - imm22		@gprel(S + A)
 * R_IA_64_GPREL64I	0x2b	instr. - imm64		@gprel(S + A)
 * 	...whole...
 * R_IA_64_GPREL64MSB	0x2e	Word64 MSB		@gprel(S + A)
 * R_IA_64_GPREL64LSB	0x2f	Word64 LSB		@gprel(S + A)
 * 	...whole...
 * R_IA_64_LTOFF22	0x32	instr. - imm22		@ltoff(S + A)
 * R_IA_64_LTOFF64I	0x33	instr. - imm64		@ltoff(S + A)
 * 	...whole...
 * R_IA_64_PLTOFF22	0x3a	instr. - imm22		@pltoff(S + A)
 * R_IA_64_PLTOFF64I	0x3b	instr. - imm64		@pltoff(S + A)
 * 	...whole...
 * R_IA_64_PLTOFF64MSB	0x3e	Word64 MSB		@pltoff(S + A)
 * R_IA_64_PLTOFF64LSB	0x3f	Word64 LSB		@pltoff(S + A)
 * 	...whole...
 * R_IA_64_FPTR64I	0x43	instr. - imm64		@fptr(S + A)
 * R_IA_64_FPTR32MSB	0x44	Word32 MSB		@fptr(S + A)
 * R_IA_64_FPTR32LSB	0x45	Word32 LSB		@fptr(S + A)
 * R_IA_64_FPTR64MSB	0x46	Word64 MSB		@fptr(S + A)
 * R_IA_64_FPTR64LSB	0x47	Word64 LSB		@fptr(S + A)
 * 	...whole...
 * R_IA_64_PCREL21B	0x49	instr. - imm21 form1	S + A - P
 * R_IA_64_PCREL21M	0x4a	instr. - imm21 form2	S + A - P
 * R_IA_64_PCREL21F	0x4b	instr. - imm21 form3	S + A - P
 * R_IA_64_PCREL32MSB	0x4c	Word32 MSB		S + A - P
 * R_IA_64_PCREL32LSB	0x4d	Word32 LSB		S + A - P
 * R_IA_64_PCREL64MSB	0x4e	Word64 MSB		S + A - P
 * R_IA_64_PCREL64LSB	0x4f	Word64 LSB		S + A - P
 * 	...whole...
 * R_IA_64_LTOFF_FPTR22	0x52	instr. - imm22		@ltoff(@fptr(S + A))
 * R_IA_64_LTOFF_FPTR64I 0x53	instr. - imm64		@ltoff(@fptr(S + A))
 * 	...whole...
 * R_IA_64_SEGREL32MSB	0x5c	Word32 MSB		@segrel(S + A)
 * R_IA_64_SEGREL32LSB	0x5d	Word32 LSB		@segrel(S + A)
 * R_IA_64_SEGREL64MSB	0x5e	Word64 MSB		@segrel(S + A)
 * R_IA_64_SEGREL64LSB	0x5f	Word64 LSB		@segrel(S + A)
 * 	...whole...
 * R_IA_64_SECREL32MSB	0x64	Word32 MSB		@secrel(S + A)
 * R_IA_64_SECREL32LSB	0x65	Word32 LSB		@secrel(S + A)
 * R_IA_64_SECREL64MSB	0x66	Word64 MSB		@secrel(S + A)
 * R_IA_64_SECREL64LSB	0x67	Word64 LSB		@secrel(S + A)
 * 	...whole...
 * R_IA_64_REL32MSB	0x6c	Word32 MSB		BD + C
 * R_IA_64_REL32LSB	0x6d	Word32 LSB		BD + C
 * R_IA_64_REL64MSB	0x6e	Word64 MSB		BD + C
 * R_IA_64_REL64LSB	0x6f	Word64 LSB		BD + C
 * R_IA_64_LTV32MSB	0x70	Word32 MSB		S + A
 * R_IA_64_LTV32LSB	0x71	Word32 LSB		S + A
 * R_IA_64_LTV64MSB	0x72	Word64 MSB		S + A
 * R_IA_64_LTV64LSB	0x73	Word64 LSB		S + A
 * 	...whole...
 * R_IA_64_IPLTMSB	0x80	func. descriptor MSB
 * R_IA_64_IPLTLSB	0x81	func. descriptor LSB
 *
 *	This is from sectino 4.3.1 from the Intel IA-64 PS ABI,
 * ref-no SC-2253.
 *
 * The above calculations use the following descriptions:
 *
 * A
 *	This means the addend used to compute the value of the relocatable
 *	field.
 *
 * BD
 *	This means the base address diffeence, a constant that must be
 *	applied to a virtual address.  This constant represents the
 *	difference between the run-time virtual address and the link-time
 *	virtual address of a particular segment.  The segment is implied
 *	by the value of the link-time virtual address.
 *
 * C
 *	This means the contents of the relocatable field.  The size
 *	and byte order of the data are determined by the
 *	relocation type.
 *
 * P
 *	This means the place (section offset or address) of the storage
 *	unit being relocated (computed using r_offset).  If the
 *	relocation applies to an instruction, this is the address of the
 *	bundle containing the instruction.
 *
 * S
 *	This means the value of the symbol whose index resides in the
 *	relocation entry.
 *
 * @gprel (expr)
 *	Computes a gp-relative displacement - the difference between the
 *	effective address and teh value of the global pointer (gp) for the
 *	current module.
 *
 * @ltoff (expr)
 *	Requests the creation of a global offset table (GOT) entry that will
 *	hold the full value of the effective address and computes the
 *	gp-relative displacement to that GOT entry.
 *
 * @pltoff (expr)
 *	Requests the creation of a local function discriptor entry for the
 *	given symbol and computes the gp-relative displacement to
 *	that function descriptor entry.
 *
 * @segrel (expr)
 *	Computes a segment-relative displacement - the difference between
 *	the effective address and the address of the beginning of the
 *	segment containing the relocatable object.  This relocation
 *	type is designed for data structures that reside in read-only
 *	segments, but need to contain pointers.  The relocatable
 *	object and effective address must be contained within the same
 *	segment.  Applications using these pointers must be aware that
 *	they are segment-relative and must adjust their values at run-time
 *	using the load address of the containing segment.  No output
 *	relocations will be generated for @segrel relocations.
 *
 * @secrel (expr)
 *	Computes a section-relative displacement - the difference between
 *	the effective address and the address of the beginning of the
 *	(output) section that contains the effective address.  This
 *	relocations type is designed for references for one non-allocatble
 *	section to another.  Applications using these values must
 *	be aware that they are section-relative and must adjust their values
 *	at run-time, using the adjusted address of the target section.
 *	No output relocations will be generated for @secrel relocations.
 *
 * @fptr (symbol)
 *	Evaluates to the address of the "official" function descriptor for
 *	the given symbol.
 *
 * The MSB and LSB suffixes on the relcoations indicate whether the target
 * field is stored most significant byte first (big-endian) or least
 * significant byte first (little-endian), respectivly.
 */


int
do_reloc(unsigned char rtype, unsigned char *off, Xword *value,
	const char *sym, const char *file)
{
	Xword			uvalue = 0;
	const Rel_entry *	rep;
	unsigned int		re_flags;
	uint_t			islot;

	rep = &reloc_table[rtype];
	re_flags = rep->re_flags;
	uvalue = *value;

	if ((re_flags & FLG_RE_VERIFY) && rep->re_sigbits) {
		if (((re_flags & FLG_RE_SIGN) &&
		    (S_INRANGE((Sxword)uvalue, rep->re_sigbits - 1) == 0)) ||
		    (!(re_flags & FLG_RE_SIGN) &&
		    ((S_MASK(rep->re_sigbits) & uvalue) != uvalue))) {
#if	defined(_KERNEL)
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_ERR_STR),
			    conv_reloc_ia64_type_str(rtype));
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_ERR_FILE), file);
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_ERR_SYM),
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)));
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOFIT),
			    EC_XWORD(uvalue));
#else
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOFIT),
			    conv_reloc_ia64_type_str(rtype), file,
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)),
			    EC_XWORD(uvalue));
#endif
			return (0);
		}
	}
	switch (rep->re_fsize) {
	case 0:
		/*
		 * Field size '0' signifies one of the 'instruction'
		 * relocations.  Which slot is being filled in is
		 * identified by the lower 2 bits of the offset.
		 */
		/* LINTED */
		islot = (uint_t)((uintptr_t)off & 0x3);

		/*
		 * We then mask out the lower three bits since they
		 * are not part of the offset.
		 */
		off = (unsigned char *)((uintptr_t)off & (~0x3));

		/*
		 * Some verification that the bits fit should be
		 * placed in here somewhere....
		 */
		switch (rtype) {
		case R_IA_64_IMM22:
		case R_IA_64_GPREL22:
		case R_IA_64_LTOFF22:
		case R_IA_64_PLTOFF22:
		case R_IA_64_LTOFF_FPTR22:
			/* instr.-imm22 */
			SLOTIMM22(uvalue);
			break;
		case R_IA_64_PCREL21B:
			/* instr.-imm FORM1 */
			SLOTFORM1(uvalue);
			break;
		case R_IA_64_IMM64:
			/* instr.-imm64 */
			if (islot != 1) {
				/*
				 * xxx ia64 NOTE:
				 * convert to DBG print when placed
				 * into krtld
				 */
				/* LINTED */
				printf("krtld: error - intr.-imm64 only valid "
				    "against slot1:\n"
				    "krtld:\toffset: %p slot: %d\n",
				    off, islot);
				return (0);
			}
			SLOTFILLIMM64(uvalue, off);
			return (1);
		default:
			printf("Fatal - unexecptected FORM reloc: %d\n",
				rtype);
			/* error */
			;
		}
		FILLSLOT(islot, uvalue, off);

		break;
	case	4:
		/* LINTED */
		*((Word *)off) += (Word)uvalue;
		break;
	case	8:
		*((Xword *)off) += uvalue;
		break;
	}
	return (1);
}
