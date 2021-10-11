/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved
 */
#pragma ident	"@(#)ldentry.c	1.13	98/08/28 SMI"

#include	<stdio.h>
#include	<string.h>
#include	"msg.h"
#include	"_ld.h"


/*
 * Print a virtual address map of input and output sections together with
 * multiple symbol definitions (if they exist).
 */
static Boolean	symbol_title = TRUE;

static void
sym_muldef_title()
{
	(void) printf(MSG_INTL(MSG_ENT_MUL_FMT_TIL_0),
		MSG_INTL(MSG_ENT_MUL_TIL_0));
	(void) printf(MSG_INTL(MSG_ENT_MUL_FMT_TIL_1),
		MSG_INTL(MSG_ENT_MUL_ITM_SYM),
		MSG_INTL(MSG_ENT_MUL_ITM_DEF_0),
		MSG_INTL(MSG_ENT_MUL_ITM_DEF_1));
	symbol_title = FALSE;
}

void
ldmap_out(Ofl_desc * ofl)
{
	Listnode *	lnp1, * lnp2, * lnp3;
	Os_desc *	osp;
	Sg_desc *	sgp;
	Is_desc *	isp;
	Xword		bkt;
	Sym_desc *	sdp;

	(void) printf(MSG_INTL(MSG_ENT_MAP_FMT_TIL_1),
		MSG_INTL(MSG_ENT_MAP_TITLE_1));
	if (ofl->ofl_flags & FLG_OF_RELOBJ)
		(void) printf(MSG_INTL(MSG_ENT_MAP_FMT_TIL_2),
			MSG_INTL(MSG_ENT_ITM_OUTPUT),
			MSG_INTL(MSG_ENT_ITM_INPUT),
			MSG_INTL(MSG_ENT_ITM_NEW),
			MSG_INTL(MSG_ENT_ITM_SECTION),
			MSG_INTL(MSG_ENT_ITM_SECTION),
			MSG_INTL(MSG_ENT_ITM_DISPMNT),
			MSG_INTL(MSG_ENT_ITM_SIZE));
	else
		(void) printf(MSG_INTL(MSG_ENT_MAP_FMT_TIL_3),
			MSG_INTL(MSG_ENT_ITM_OUTPUT),
			MSG_INTL(MSG_ENT_ITM_INPUT),
			MSG_INTL(MSG_ENT_ITM_VIRTUAL),
			MSG_INTL(MSG_ENT_ITM_SECTION),
			MSG_INTL(MSG_ENT_ITM_SECTION),
			MSG_INTL(MSG_ENT_ITM_ADDRESS),
			MSG_INTL(MSG_ENT_ITM_SIZE));

	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		if (sgp->sg_phdr.p_type != PT_LOAD)
			continue;
		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			(void) printf(MSG_INTL(MSG_ENT_MAP_ENTRY_1),
				osp->os_name, EC_ADDR(osp->os_shdr->sh_addr),
				EC_XWORD(osp->os_shdr->sh_size));
			for (LIST_TRAVERSE(&(osp->os_isdescs), lnp3, isp)) {
			    Addr	addr;

			    addr = (Addr)_elf_getxoff(isp->is_indata);
			    if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
				addr += isp->is_osdesc->os_shdr->sh_addr;
			    (void) printf(MSG_INTL(MSG_ENT_MAP_ENTRY_2),
				isp->is_name, EC_ADDR(addr),
				EC_XWORD(isp->is_shdr->sh_size),
				((isp->is_file != NULL) ?
				(char *)(isp->is_file->ifl_name) :
				MSG_INTL(MSG_STR_NULL)));
			}
		}
	}

	if (ofl->ofl_flags & FLG_OF_RELOBJ)
		return;

	/*
	 * Check for any multiply referenced symbols (ie. symbols that have
	 * been overridden from a shared library).
	 */
	for (bkt = 0; bkt < ofl->ofl_symbktcnt; bkt++) {
		Sym_cache *	scp;

		for (scp = ofl->ofl_symbkt[bkt]; scp; scp = scp->sc_next) {
			/*LINTED*/
			for (sdp = (Sym_desc *)(scp + 1);
			    sdp < scp->sc_free; sdp++) {
				const char *	name, * ducp, * adcp;
				List *		dfiles;

				name = sdp->sd_name;
				dfiles = &sdp->sd_aux->sa_dfiles;

				/*
				 * Files that define a symbol are saved on the
				 * `sa_dfiles' list, if the head and tail of
				 * this list differ there must have been more
				 * than one symbol definition.  Ignore symbols
				 * that aren't needed, and any special symbols
				 * that the link editor may produce (symbols of
				 * type ABS and COMMON are not recorded in the
				 * first place, however functions like _init()
				 * and _fini() commonly have multiple
				 * occurrances).
				 */
				if ((sdp->sd_ref == REF_DYN_SEEN) ||
				    (dfiles->head == dfiles->tail) ||
				    (sdp->sd_aux && sdp->sd_aux->sa_symspec) ||
				    (strcmp(MSG_ORIG(MSG_SYM_FINI_U),
				    name) == 0) ||
				    (strcmp(MSG_ORIG(MSG_SYM_INIT_U),
				    name) == 0) ||
				    (strcmp(MSG_ORIG(MSG_SYM_LIBVER_U),
				    name) == 0))
					continue;

				if (symbol_title)
					sym_muldef_title();

				ducp = sdp->sd_file->ifl_name;
				(void) printf(MSG_INTL(MSG_ENT_MUL_ENTRY_1),
				    name, ducp);
				for (LIST_TRAVERSE(dfiles, lnp2, adcp)) {
					/*
					 * Ignore the referenced symbol.
					 */
					if (strcmp(adcp, ducp) != 0)
					    (void) printf(
						MSG_INTL(MSG_ENT_MUL_ENTRY_2),
						adcp);
				}
			}
		}
	}
}

/*
 * Traverse the entrance criteria list searching for those sections that haven't
 * been met and print error message.  (only in the case of reordering)
 */
void
ent_check(Ofl_desc * ofl)
{
	Listnode *	lnp;
	Ent_desc *	enp;

	/*
	 *  Try to give as much information to the user about the specific
	 *  line in the mapfile.  If the line contains a file name then
	 *  output the filename too.  Hence we have two warning lines -
	 *  one for criterias where a filename is used and the other
	 *  for those without a filename.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_ents, lnp, enp)) {
		if ((enp->ec_segment->sg_flags & FLG_SG_ORDER) &&
		    !(enp->ec_flags & FLG_EC_USED) && enp->ec_ndx) {
			Listnode *	_lnp = enp->ec_files.head;

			if ((_lnp != NULL) && (_lnp->data != NULL) &&
			    (char *)(_lnp->data) != NULL) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_ENT_NOSEC_1),
				    enp->ec_segment->sg_name, enp->ec_name,
				    (const char *)(_lnp->data));
			} else {
				eprintf(ERR_WARNING, MSG_INTL(MSG_ENT_NOSEC_2),
				    enp->ec_segment->sg_name, enp->ec_name);
			}
		}
	}
}
