/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)got.c	1.7	99/05/04 SMI"

#include	<stdlib.h>
#include	"_debug.h"
#include	"msg.h"
#include	"libld.h"


static int
comparegotsym(Gottable *g1, Gottable *g2)
{

	if (g1->gt_gotndx > g2->gt_gotndx)
		return (1);
	if (g1->gt_gotndx < g2->gt_gotndx)
		return (-1);
	if (g1->gt_addend > g2->gt_addend)
		return (1);
	if (g1->gt_addend < g2->gt_addend)
		return (-1);
	return (0);
}

void
Dbg_got_display(Gottable *gottable, Ofl_desc *ofl)
{
	int	gotndx;
	if (DBG_NOTCLASS(DBG_GOT))
		return;

	if (ofl->ofl_gotcnt == M_GOT_XNumber)
		return;


	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_GOT_TITLE), (int)ofl->ofl_gotcnt);

	if (DBG_NOTDETAIL())
		return;

	qsort((char *)gottable, ofl->ofl_gotcnt, sizeof (Gottable),
		(int(*)(const void *, const void *))comparegotsym);


	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_ORIG(MSG_GOT_COLUMNS));
	for (gotndx = 0; gotndx < ofl->ofl_gotcnt; gotndx++, gottable++) {
		Sym_desc	*sdp;
		if ((sdp = gottable->gt_sym) != 0) {
			const char	*refstr;

			if (sdp->sd_flags & FLG_SY_SMGOT)
				refstr = MSG_ORIG(MSG_GOT_SMALL_PIC);
			else
				refstr = MSG_ORIG(MSG_GOT_PIC);


			if ((sdp->sd_sym->st_shndx == SHN_UNDEF) ||
			    (sdp->sd_file == 0)) {
				dbg_print(MSG_ORIG(MSG_GOT_FORMAT1),
					gottable->gt_gotndx, refstr,
					gottable->gt_addend,
					sdp->sd_name ? sdp->sd_name :
					MSG_INTL(MSG_STR_UNKNOWN));
			} else {
				dbg_print(MSG_ORIG(MSG_GOT_FORMAT2),
					gottable->gt_gotndx, refstr,
					gottable->gt_addend,
					sdp->sd_file->ifl_name,
					sdp->sd_name ? sdp->sd_name :
					MSG_INTL(MSG_STR_UNKNOWN));
			}
		}
	}
}


#if	!defined(_ELF64)
void
Gelf_got_title(GElf_Ehdr *ehdr, GElf_Word entries)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_GOT_TITLE), entries);

	if ((int)ehdr->e_ident[EI_CLASS] == ELFCLASS64)
		dbg_print(MSG_ORIG(MSG_GOT_ECOLUMNS_64));
	else
		dbg_print(MSG_ORIG(MSG_GOT_ECOLUMNS));
}

void
Gelf_got_entry(GElf_Ehdr *ehdr, int ndx, GElf_Addr addr, GElf_Xword value,
	GElf_Word rshtype, void *rel, const char *sname)
{
	GElf_Word	rtype;
	GElf_Sxword	addend;
	const char	*rstring;

	if (rel) {
		if (rshtype == SHT_RELA) {
			/* LINTED */
			rtype = (GElf_Word)GELF_R_TYPE(
			    ((GElf_Rela *)rel)->r_info);
			addend = ((GElf_Rela *)rel)->r_addend;
		} else {
			/* LINTED */
			rtype = (GElf_Word)GELF_R_TYPE(
			    ((GElf_Rel *)rel)->r_info);
			addend = 0;
		}
		rstring = conv_reloc_type_str(ehdr->e_machine, rtype);
	} else {
		addend = 0;
		rstring = MSG_ORIG(MSG_STR_EMPTY);
	}

	if ((int)ehdr->e_ident[EI_CLASS] == ELFCLASS64)
		dbg_print(MSG_ORIG(MSG_GOT_EFORMAT_64), ndx,
		    EC_ADDR(addr), EC_XWORD(value), rstring, EC_SXWORD(addend),
		    sname ? sname : MSG_ORIG(MSG_STR_EMPTY));
	else
		dbg_print(MSG_ORIG(MSG_GOT_EFORMAT), ndx, EC_ADDR(addr),
		    EC_XWORD(value), rstring, EC_SXWORD(addend),
		    sname ? sname : MSG_ORIG(MSG_STR_EMPTY));
}
#endif	/* !defined(_ELF64) */
