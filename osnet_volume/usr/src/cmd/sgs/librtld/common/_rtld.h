/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__RTLD_H
#define	__RTLD_H

#pragma ident	"@(#)_rtld.h	1.15	99/09/14 SMI"

#include <libelf.h>
#include <rtld.h>
#include <machdep.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Define a cache structure that is used to retain all elf section information.
 */
typedef struct cache {
	int		c_flags;
	Elf_Scn *	c_scn;
	Shdr *		c_shdr;
	Elf_Data *	c_data;
	char *		c_name;
	void *		c_info;
} Cache;

#define	FLG_C_EXCLUDE	1		/* exclude section from output image */
#define	FLG_C_HEAP	2		/* heap section (addition) */
#define	FLG_C_RELOC	3		/* relocation section requiring */
					/*	modification */
#define	FLG_C_SHSTR	4		/* section header string table */
#define	FLG_C_END	5		/* null terminating section marker */


/*
 * Define a structure for maintaining relocation information.  During the first
 * pass through an input files relocation records, counts are maintained of the
 * number of relocations to recreate in the output image.  The intent is to
 * localize any remaining relocation records.  The relocation structure
 * indicates the actions that must be performed on the output images relocation
 * information.
 */
typedef struct reloc {
	int		r_flags;
	Addr		r_reloc;	/* relocation records address (input) */
	Addr		r_value;	/* value to apply to relocation */
	Word		r_size;		/* size (used for copy relocations) */
	const char *	r_name;		/* relocation symbol */
} Reloc;

#define	FLG_R_UNDO	0x01		/* undo any relocation offset value */
#define	FLG_R_CLR	0x02		/* clear the relocation record */
#define	FLG_R_INC	0x04		/* increment the relocation offset */
					/*	(uses new fixed addr) */
#define	FLG_R_APPLY	0x08		/* apply the relocation */


/*
 * Define any local prototypes.
 */
extern	void	apply_reloc(void *, Reloc *, const char *, unsigned char *,
		    Rt_map * lmp);
extern	void	clear_reloc(void *);
extern	int	count_reloc(Cache *, Cache *, Rt_map *, int, Addr, Xword *,
		    Xword *, Xword *);
extern	void	inc_reloc(void *, void *, Reloc *, unsigned char *,
		    unsigned char *);
extern	void	undo_reloc(void *, unsigned char *, unsigned char *,
		    Reloc *);
extern	int	update_dynamic(Cache *, Cache *, Rt_map *, int, Addr, Off,
		    const char *, Xword, Xword, Xword, Xword, Xword);
extern	void	update_reloc(Cache *, Cache *, Cache *, const char *,
		    Rt_map *, Rel **, Rel **, Rel **);
extern	void	update_sym(Cache *, Cache *, Addr, Half, Addr);

#ifdef	__cplusplus
}
#endif

#endif /* __RTLD_H */
