/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RELMACH_H
#define	_RELMACH_H

#pragma ident	"@(#)relmach.h	1.1	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Architecture specific flags presented in a architecture nutral format
 */
#define	SHF_NEUT_SHORT	SHF_IA_64_SHORT


/*
 * Note: the IA64 SLOT calculations are being placed in macros
 * because these routines need to be used in libld, ld.so.1, and
 * krtld.  Also - since both krtld and ld.so.1 need these macros
 * as they are boot strapping themselves (which means they can't
 * call doreloc()) we are placing them in macros for inlining
 * in each of these files.
 */

/*
 * Macros to convert values into proper bit fields for IA64 relocations
 */

#define	SLOTIMM22(uvalue) \
	{\
		Elf64_Xword	tvalue; \
		tvalue = uvalue; \
		uvalue = (0x200000 & tvalue) << 15;	/* i1 */ \
		uvalue |= (0x1f0000 & tvalue) << 6;	/* i5 */ \
		uvalue |= (0x00ff80 & tvalue) << 20;    /* i9 */ \
		uvalue |= (0x00007f & tvalue) << 13;    /* i7 */ \
	}

#define	SLOTFORM1(uvalue) \
	{ \
		Elf64_Xword	tvalue; \
		tvalue = uvalue >> 4; \
		uvalue = ( \
			((tvalue & 0x00000000000fffff) << 13) | /* i20 */ \
			((tvalue & 0x0800000000000000) >> 23)); /* i1 */ \
	}

#define	FILLSLOT(islot, uvalue, off) \
	switch (islot) { \
	case 0: \
		/* slot 0 */ \
		*((Elf64_Xword *)off) |= (uvalue << 5); \
		break; \
	case 1: \
		/* slot 1 */ \
		*((Elf64_Xword *)off) |= ((uvalue & 0x3ffff) << 46); \
		*((Elf64_Xword *)off + 1) |= (uvalue >> 18); \
		break; \
	case 2: \
		/* slot 2 */ \
		*((Elf64_Xword *)off + 1) |= (uvalue << 23); \
		break; \
	case 3: \
		/* error */ \
		; \
		}

/*
 * Since R_IA_64_IMM64 spans two slots we have a single macro which
 * both calculates the uvalue and fills in the slots.
 *
 * Note: By defenition a instr.-imm64 is always set in slot 1 & 2
 */
#define	SLOTFILLIMM64(uvalue, off) \
	{ \
		Elf64_Xword	tvalue; \
		/* \
		 * update slot 1 \
		 */ \
		tvalue = (uvalue & 0x7fffffffffc00000) >> 22; /* i41 */ \
		*((Elf64_Xword *)off) |= ((tvalue & 0x3ffff) << 46); \
		*((Elf64_Xword *)off + 1) |= (tvalue >> 18); \
		/* \
		 * update slot 2 \
		 */ \
		tvalue =  (uvalue & 0x8000000000000000) >> 27; /* i1a */ \
		tvalue |= (uvalue & 0x0000000000200000); /* i1b */ \
		tvalue |= (uvalue & 0x00000000001f0000) << 6;  /* i5 */ \
		tvalue |= (uvalue & 0x000000000000ff80) << 20; /* i9 */ \
		tvalue |= (uvalue & 0x000000000000007f) << 13; /* i7 */ \
		*((Elf64_Xword *)off + 1) |= (tvalue << 23); \
	}


#ifdef	__cplusplus
}
#endif

#endif /* _RELMACH_H */
