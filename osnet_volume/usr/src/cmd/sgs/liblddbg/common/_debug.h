/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_DEBUG_DOT_H
#define	_DEBUG_DOT_H

#pragma ident	"@(#)_debug.h	1.22	99/06/01 SMI"

#include	"debug.h"
#include	"conv.h"

#ifdef	__cplusplus
extern "C" {
#endif

extern	int	_Dbg_mask;


/*
 * Debugging is enabled by various tokens (see debug.c) that result in an
 * internal bit mask (_Dbg_mask) being initialized.  Each debugging function is
 * appropriate for one or more of the classes specified by the bit mask.  Each
 * debugging function validates whether it is appropriate for the present
 * classes before printing anything.
 */
#define	DBG_NOTCLASS(c)	!(_Dbg_mask & DBG_OMASK & (c))
#define	DBG_NOTDETAIL()	!(_Dbg_mask & DBG_DETAIL)

#define	DBG_ALL		0x7fffff
#define	DBG_OMASK	0x0fffff
#define	DBG_AMASK	0x700000

#define	DBG_DETAIL	0x100000

#define	DBG_ARGS	0x000001
#define	DBG_BASIC	0x000002
#define	DBG_BINDINGS	0x000004
#define	DBG_ENTRY	0x000008
#define	DBG_FILES	0x000010
#define	DBG_HELP	0x000020
#define	DBG_LIBS	0x000040
#define	DBG_MAP		0x000080
#define	DBG_RELOC	0x000100
#define	DBG_SECTIONS	0x000200
#define	DBG_SEGMENTS	0x000400
#define	DBG_SYMBOLS	0x000800
#define	DBG_SUPPORT	0x001000
#define	DBG_VERSIONS	0x002000
#define	DBG_AUDITING	0x004000
#define	DBG_GOT		0x008000
#define	DBG_MOVE	0x010000

typedef struct options {
	const char	*o_name;	/* command line argument name */
	int		o_mask;		/* associated bit mask for this name */
} DBG_options, *DBG_opts;


/*
 * Internal debugging routines.
 */
#ifdef _ELF64
#define	_Dbg_seg_desc_entry	_Dbg_seg_desc_entry64
#endif
extern	void		_Dbg_elf_data_in(Os_desc *, Is_desc *);
extern	void		_Dbg_elf_data_out(Os_desc *);
extern	void		_Dbg_ent_entry(Half, Ent_desc * enp);
extern	void		_Dbg_seg_desc_entry(Half, int, Sg_desc *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DEBUG_DOT_H */
