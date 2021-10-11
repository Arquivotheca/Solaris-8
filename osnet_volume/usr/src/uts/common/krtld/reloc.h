/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RELOC_DOT_H
#define	_RELOC_DOT_H

#pragma ident	"@(#)reloc.h	1.21	99/06/29 SMI"

#if defined(_KERNEL)
#include <sys/machelf.h>
#include <sys/bootconf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#else
#include "machdep.h"
#endif /* _KERNEL */

#include <relmach.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Global include file for relocation common code.
 *
 * Flags for reloc_entry->re_flags
 */
#define	FLG_RE_NOTREL		0x00000000
#define	FLG_RE_GOTADD		0x00000001	/* create a GOT entry */
#define	FLG_RE_PCREL		0x00000002
#define	FLG_RE_GOTPCREL		0x00000004	/* GOT - P */
#define	FLG_RE_PLTREL		0x00000008
#define	FLG_RE_VERIFY		0x00000010	/* verify value fits */
#define	FLG_RE_UNALIGN		0x00000020	/* offset is not aligned */
#define	FLG_RE_WDISP16		0x00000040	/* funky sparc DISP16 rel */
#define	FLG_RE_SIGN		0x00000080	/* value is signed */
#define	FLG_RE_ADDRELATIVE	0x00000100	/* RELATIVE relocation */
						/* required for non-fixed */
						/* objects */
#define	FLG_RE_EXTOFFSET	0x00000200	/* extra offset required */
#define	FLG_RE_REGISTER		0x00000400	/* relocation initializes */
						/*    a REGISTER */
						/* by OLO10 */
#define	FLG_RE_MSB		0x00000800	/* merced MSB data field */
#define	FLG_RE_LSB		0x00001000	/* merced LSB data field */
#define	FLG_RE_ADDFIELD		0x00002000	/* add contents of field at */
						/* r_offset to value */
#define	FLG_RE_NOTSUP		0x00004000	/* relocation not supported */
#define	FLG_RE_FRMOFF		0x00008000	/* offset contains islot */
						/*  value (IA64) */
#define	FLG_RE_GOTREL		0x00010000	/* GOT based */
#define	FLG_RE_SEGREL		0x00020000	/* Segment relative */
#define	FLG_RE_FPTR		0x00040000	/* FPTR required */
#define	FLG_RE_SECREL		0x00080000	/* Section relative */

/*
 * Macros for testing relocation table flags
 */
extern	const Rel_entry		reloc_table[];

#define	IS_PLT(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_PLTREL) != 0)
#define	IS_GOT_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_GOTADD) != 0)
#define	IS_GOT_PC(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_GOTPCREL) != 0)
#define	IS_GOT_BASED(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_GOTREL) != 0)
#define	IS_PC_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_PCREL) != 0)
#define	IS_ADD_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_ADDRELATIVE) != 0)
#define	IS_REGISTER(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_REGISTER) != 0)
#define	IS_FORMOFF(X)		((reloc_table[(X)].re_flags &\
					FLG_RE_FRMOFF) != 0)
#define	IS_NOTSUP(X)		((reloc_table[(X)].re_flags &\
					FLG_RE_NOTSUP) != 0)
#define	IS_SEG_RELATIVE(X)	((reloc_table[(X)].re_flags &\
					FLG_RE_SEGREL) != 0)
#define	IS_FPTR(X)		((reloc_table[(X)].re_flags &\
					FLG_RE_FPTR) != 0)
#define	IS_EXTOFFSET(X)		((reloc_table[(X)].re_flags &\
					FLG_RE_EXTOFFSET) != 0)
#define	IS_SEC_RELATIVE(X)	((reloc_table[(X)].re_flags &\
					FLG_RE_SECREL) != 0)

/*
 * Functions.
 */
#if defined(i386) || defined(__ia64)
extern	int	do_reloc(unsigned char, unsigned char *, Xword *,
			const char *, const char *);
#else /* sparc */
extern	int	do_reloc(unsigned char, unsigned char *, Xword *,
			const char *, const char *);
#endif /* i386 */

#if defined(_KERNEL)
/*
 * These are macro's that are only needed for krtld.  Many of these
 * are already defined in the sgs/include files referenced by
 * ld and rtld
 */

#define	S_MASK(n)	((1l << (n)) - 1l)
#define	S_INRANGE(v, n)	(((-(1l << (n)) - 1l) < (v)) && ((v) < (1l << (n))))

/*
 * This converts the sgs eprintf() routine into the _printf()
 * as used by krtld.
 */
#define	eprintf		_kobj_printf
#define	ERR_FATAL	ops

/*
 * Message strings used by doreloc()
 */
#define	MSG_ORIG(x)		x
#define	MSG_INTL(x)		x

#define	MSG_STR_UNKNOWN		"(unknown)"
#define	MSG_REL_UNSUPSZ		"relocation error: %s: file %s: symbol %s: " \
				"offset size (%d bytes) is not supported"
#define	MSG_REL_ERR_STR		"relocation error: %s:"
#define	MSG_REL_ERR_WITH_FILE	"relocation error: file %s: "
#define	MSG_REL_ERR_FILE	" file %s: "
#define	MSG_REL_ERR_SYM		" symbol %s: "
#define	MSG_REL_ERR_VALUE	" value 0x%llx"
#define	MSG_REL_ERR_OFF		" offset 0x%llx\n"
#define	MSG_REL_UNIMPL		" unimplemented relocation type: %d"
#define	MSG_REL_NONALIGN	" offset 0x%llx is non-aligned\n"
#define	MSG_REL_UNNOBITS	" unsupported number of bits: %d"
#define	MSG_REL_NOFIT		" value 0x%llx does not fit\n"
#define	MSG_REL_LOOSEBITS	" looses %d bits at"

extern const char *conv_reloc_SPARC_type_str(Word rtype);
extern const char *conv_reloc_386_type_str(Word rtype);
extern const char *conv_reloc_ia64_type_str(Word rtype);

/*
 * Note:  Related to bug 4128755, dlerror() only keeps track
 * of a single error string, and therefore must have errors
 * reported through a single eprintf() call.  The kernel's
 * printf is somewhat more limited, and must receive messages
 * with only one arguement to the format string.  The following
 * macros are to straighted all this out because krtld and
 * rtld share do_reloc().
 */
#define	REL_ERR_UNIMPL(file, sym, rtype) \
	eprintf(ERR_FATAL, MSG_REL_ERR_WITH_FILE, (file)); \
	eprintf(ERR_FATAL, MSG_REL_ERR_SYM, \
	    ((sym) ? (sym) : MSG_STR_UNKNOWN)); \
	eprintf(ERR_FATAL,  MSG_REL_UNIMPL, \
	    (int)(rtype))

#define	REL_ERR_NONALIGN(file, sym, rtype, off) \
	eprintf(ERR_FATAL, MSG_REL_ERR_STR, \
	    conv_reloc_SPARC_type_str((rtype))); \
	eprintf(ERR_FATAL, MSG_REL_ERR_FILE, (file)); \
	eprintf(ERR_FATAL, MSG_REL_ERR_SYM, \
	    ((sym) ? (sym) : MSG_STR_UNKNOWN)); \
	eprintf(ERR_FATAL, MSG_REL_NONALIGN, \
	    EC_OFF((off)))

#define	REL_ERR_UNNOBITS(file, sym, rtype, nbits) \
	eprintf(ERR_FATAL, MSG_REL_ERR_STR, \
	    conv_reloc_SPARC_type_str((rtype))); \
	eprintf(ERR_FATAL, MSG_REL_ERR_FILE, (file)); \
	eprintf(ERR_FATAL, MSG_REL_ERR_SYM, \
	    ((sym) ? (sym) : MSG_STR_UNKNOWN)); \
	eprintf(ERR_FATAL, MSG_REL_UNNOBITS, (nbits))

#define	REL_ERR_LOOSEBITS(file, sym, rtype, uvalue, nbits, off) \
	eprintf(ERR_FATAL,  MSG_REL_ERR_STR, \
	    conv_reloc_SPARC_type_str((rtype))); \
	eprintf(ERR_FATAL,  MSG_REL_ERR_FILE, (file)); \
	eprintf(ERR_FATAL,  MSG_REL_ERR_SYM, \
	    ((sym) ? (sym) : MSG_STR_UNKNOWN)); \
	eprintf(ERR_FATAL,  MSG_REL_ERR_VALUE, EC_XWORD((uvalue))); \
	eprintf(ERR_FATAL,  MSG_REL_LOOSEBITS, (nbits)); \
	eprintf(ERR_FATAL,  MSG_REL_ERR_OFF, EC_ADDR((off)))

#define	REL_ERR_NOFIT(file, sym, rtype, uvalue) \
	eprintf(ERR_FATAL, MSG_REL_ERR_STR, \
	    conv_reloc_SPARC_type_str((rtype))); \
	eprintf(ERR_FATAL, MSG_REL_ERR_FILE, (file)); \
	eprintf(ERR_FATAL, MSG_REL_ERR_SYM, \
	    ((sym) ? (sym) : MSG_STR_UNKNOWN)); \
	eprintf(ERR_FATAL, MSG_REL_NOFIT, EC_XWORD((uvalue)))


#else	/* !_KERNEL */

#define	REL_ERR_UNIMPL(file, sym, rtype) \
	(eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNIMPL), \
		(file), ((sym) ? (sym) : MSG_INTL(MSG_STR_UNKNOWN)), \
		(int)(rtype)))

#define	REL_ERR_NONALIGN(file, sym, rtype, off) \
	(eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NONALIGN), \
		conv_reloc_SPARC_type_str(rtype), (file), \
		((sym) ? (sym) : MSG_INTL(MSG_STR_UNKNOWN)), \
		EC_OFF((off))))

#define	REL_ERR_UNNOBITS(file, sym, rtype, nbits) \
	(eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNNOBITS), \
	    conv_reloc_SPARC_type_str(rtype), (file), \
	    ((sym) ? (sym) : MSG_INTL(MSG_STR_UNKNOWN)), (nbits)))

#define	REL_ERR_LOOSEBITS(file, sym, rtype, uvalue, nbits, off) \
	(eprintf(ERR_FATAL,  MSG_INTL(MSG_REL_LOOSEBITS), \
	    conv_reloc_SPARC_type_str((rtype)), (file), \
	    ((sym) ? (sym) : MSG_INTL(MSG_STR_UNKNOWN)), \
	    EC_XWORD((uvalue)), (nbits), EC_ADDR((off))))

#define	REL_ERR_NOFIT(file, sym, rtype, uvalue) \
	(eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOFIT), \
	    conv_reloc_SPARC_type_str((rtype)), (file), \
	    ((sym) ? (sym) : MSG_INTL(MSG_STR_UNKNOWN)), \
	    EC_XWORD((uvalue))))

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _RELOC_DOT_H */
