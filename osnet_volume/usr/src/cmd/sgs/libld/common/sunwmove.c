/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sunwmove.c	1.24	98/08/28 SMI"

#include	<string.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"

/*
 *
 */
static uintptr_t
make_mvsections(Ofl_desc *ofl)
{
	Listnode *	lnp1;
	Psym_info *	psym;
	Word 		mv_nums = 0;
	Xword		align_sunwbss = 0;	/* Alignment for .sunwbss */
	Xword		align_sunwdata1 = 0;	/*   for .sunwdata1 */
	size_t		size_sunwbss = 0;	/* Size of .sunwbss */
	size_t		size_sunwdata1 = 0;	/* Size of .sunwdata1 */

	/*
	 * Compute the size of the output move section
	 */
	for (LIST_TRAVERSE(&ofl->ofl_parsym, lnp1, psym)) {
		Sym_desc *	symd;
		Sym *		sym;
		Xword		align_val;

		symd = psym->psym_symd;
		sym = symd->sd_sym;
		if (sym->st_shndx == SHN_COMMON)
			align_val = sym->st_value;
		else
			align_val = 8;
		if (symd->sd_flags & FLG_SY_PAREXPN) {
			/*
			 * This global symbol goes to .sunwdata1
			 */
			size_sunwdata1 = (size_t)
				S_ROUND(size_sunwdata1, sym->st_value) +
				sym->st_size;
			if (align_val > align_sunwdata1)
				align_sunwdata1 = align_val;

		} else {
			if ((ofl->ofl_flags & FLG_OF_SHAROBJ) &&
			    (ELF_ST_BIND(sym->st_info) != STB_LOCAL)) {
				/*
				 * If output file is non-executable
				 * shared object * this symbol goes
				 * to .sunwbss
				 */
				size_sunwbss = (size_t)
					S_ROUND(size_sunwbss, sym->st_value) +
					sym->st_size;
				if (align_val > align_sunwbss)
					align_sunwbss = align_val;
			}
			mv_nums += psym->psym_num;
		}
	}

	if (mv_nums != 0) {
		/* LINTED */
		DBG_CALL(Dbg_move_sections(
			MSG_ORIG(MSG_SCN_SUNWMOVE),
			mv_nums*sizeof (Move)));
		if (make_sunwmove(ofl, mv_nums) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Generate the .sunwbss section now
	 * that we know its size and alignment.
	 */
	if (size_sunwbss) {
		/* LINTED */
		DBG_CALL(Dbg_move_sections(
			MSG_ORIG(MSG_SCN_SUNWBSS),
			size_sunwbss));
		if (make_sunwbss(ofl, size_sunwbss,
		    align_sunwbss) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Add empty area for partially initialized symbols.
	 *
	 * The .SUNWDATA1 is to be created when '-z option' is in effect or
	 * there are any partially init. symbol which are to be expanded.
	 */
	if (size_sunwdata1) {
		/* LINTED */
		DBG_CALL(Dbg_move_sections(
			MSG_ORIG(MSG_SCN_SUNWDATA1),
			size_sunwdata1));
		if (make_sunwdata(ofl, size_sunwdata1,
		    align_sunwdata1) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}

/*
 * This function checkes of two given Move entries
 * overlap or not.
 */
static void
overlapping(Psym_info *psymp, Mv_itm *itm, Mv_itm *mvp)
{
	Mv_itm *	small = itm;
	Mv_itm *	large = mvp;

	if (itm->mv_start == mvp->mv_start) {
		psymp->psym_flag |= FLG_PSYM_OVERLAP;
		return;
	}

	if (itm->mv_start > mvp->mv_start) {
		small = mvp;
		large = itm;
	}

	if (small->mv_start + small->mv_length > large->mv_start)
		psymp->psym_flag |= FLG_PSYM_OVERLAP;
}

/*
 * This function insert the Move_itm into the move list held by
 * psymp.
 */
static uintptr_t
insert_mvitm(Psym_info *psymp, Mv_itm *itm)
{
	Listnode *	lnpc;
	Listnode *	lnpp;
	Listnode *	new;
	Mv_itm *	mvp;

	/*
	 * If there is error on this symbol already,
	 * don't go any further.
	 */
	if ((psymp->psym_flag & FLG_PSYM_OVERLAP) != 0)
		return (1);

	if ((new = (Listnode *)
	    libld_calloc(sizeof (Listnode), 1)) == NULL)
		return (S_ERROR);
	new->data = (void *) itm;
	lnpp = lnpc = psymp->psym_mvs.head;

	/*
	 * If this is the first, just update the
	 * head and tail.
	 */
	if (lnpc == (Listnode *) NULL) {
		psymp->psym_mvs.tail =
			psymp->psym_mvs.head = new;
		return (1);
	}

	for (LIST_TRAVERSE(&psymp->psym_mvs, lnpc, mvp)) {
		/*
		 * Check overlapping
		 * If there is no overlapping so far,
		 * check overlapping.
		 */
		overlapping(psymp, itm, mvp);
		if ((psymp->psym_flag & FLG_PSYM_OVERLAP) != 0) {
			free(new);
			return (1);
		}

		/*
		 * If passed, insert
		 */
		if (mvp->mv_start > itm->mv_start) {
			new->next = lnpc;
			if (lnpc == psymp->psym_mvs.head) {
				psymp->psym_mvs.head = new;
			} else
				lnpp->next = new;
			return (1);
		}

		/*
		 * If lnpc is the end, add
		 */
		if (lnpc->next == NULL) {
			new->next = lnpc->next;
			lnpc->next = new;
			psymp->psym_mvs.tail = new;
			return (1);
		}

		/*
		 * Go next
		 */
		lnpp = lnpc;
	}
	return (1);
}

/*
 * Install the mv entry into the Psym_info
 *
 * Count coverage size
 *	If the coverage size meets the symbol size,
 *	mark that the symbol should be expanded.
 *	psymp->psym_symd->sd_flags |= FLG_SY_PAREXPN;
 *
 * Check overlapping
 *	If overlapping occurs, mark it at psymp->psym_flags
 */
static uintptr_t
install_mv(Psym_info *psymp, Move *mv, Is_desc *isp)
{
	Mv_itm *	mvitmp;
	int 		cnt = mv->m_repeat;
	int 		i;

	if ((mvitmp = (Mv_itm *)
	    libld_calloc(sizeof (Mv_itm), cnt)) ==
	    (Mv_itm *) NULL)
		return (S_ERROR);

	mvitmp->mv_flag |= FLG_MV_OUTSECT;
	psymp->psym_num += 1;
	for (i = 0; i < cnt; i++) {
		/* LINTED */
		mvitmp->mv_length = ELF_M_SIZE(mv->m_info);
		mvitmp->mv_start = mv->m_poffset +
			i * ((mv->m_stride + 1) * mvitmp->mv_length);
		mvitmp->mv_ientry = mv;
		mvitmp->mv_isp = isp;		/* Mark input section */

		/*
		 * Insert the item
		 */
		if (insert_mvitm(psymp, mvitmp) == S_ERROR)
			return (S_ERROR);
		mvitmp++;
	}
	return (1);
}

/*
 * Insert the given psym_info
 */
static uintptr_t
insert_psym(Ofl_desc *ofl, Psym_info *p1)
{
	Listnode *	lnpc;
	Listnode *	lnpp;
	Listnode *	new;
	Psym_info *	p2;
	int		g1 = 0;

	if ((new = (Listnode *)
	    libld_calloc(sizeof (Listnode), 1)) == NULL)
		return (S_ERROR);
	new->data = (void *) p1;
	lnpp = lnpc = ofl->ofl_parsym.head;
	if (ELF_ST_BIND(p1->psym_symd->sd_sym->st_info) != STB_LOCAL)
		g1 = 1;

	/*
	 * If this is the first, just update the
	 * head and tail.
	 */
	if (lnpc == (Listnode *) NULL) {
		ofl->ofl_parsym.tail =
			ofl->ofl_parsym.head = new;
		return (1);
	}

	for (LIST_TRAVERSE(&ofl->ofl_parsym, lnpc, p2)) {
		int cmp1;
		int g2;
		int cmp;

		if (ELF_ST_BIND(p2->psym_symd->sd_sym->st_info) != STB_LOCAL)
			g2 = 1;
		else
			g2 = 0;

		cmp1 = strcmp(p1->psym_symd->sd_name,
			p2->psym_symd->sd_name);

		/*
		 * Compute position
		 */
		if (g1 == g2)
			cmp = cmp1;
		else if (g1 == 0) {
			/*
			 * p1 is a local symbol.
			 * p2 is a global, so p1 passed.
			 */
			cmp = -1;
		} else {
			/*
			 * p1 is global
			 * p2 is still local.
			 * so try the next one.
			 *
			 * If lnpc is the end, add
			 */
			if (lnpc->next == NULL) {
				new->next = lnpc->next;
				lnpc->next = new;
				ofl->ofl_parsym.tail = new;
				break;
			}
			lnpp = lnpc;
			continue;
		}

		/*
		 * If same, just add after
		 */
		if (cmp == 0) {
			new->next = lnpc->next;
			if (lnpc == ofl->ofl_parsym.tail)
				ofl->ofl_parsym.tail = new;
			lnpc->next = new;
			break;
		}

		/*
		 * If passed, insert
		 */
		if (cmp < 0) {
			new->next = lnpc;
			if (lnpc == ofl->ofl_parsym.head) {
				ofl->ofl_parsym.head = new;
			} else
				lnpp->next = new;
			break;
		}

		/*
		 * If lnpc is the end, add
		 */
		if (lnpc->next == NULL) {
			new->next = lnpc->next;
			lnpc->next = new;
			ofl->ofl_parsym.tail = new;
			break;
		}

		/*
		 * Go next
		 */
		lnpp = lnpc;
	}
	return (1);
}

/*
 * Mark the symbols
 *
 * Check only the symbols which came from the relocatable
 * files.If partially initialized symbols come from
 * shared objects, they can be ignored here because
 * they are already processed when the shared object is
 * created.
 *
 */
uintptr_t
sunwmove_preprocess(Ofl_desc *ofl)
{
	Listnode *	lnp;
	Is_desc *	isp;
	Sym_desc *	sdp;
	Move *		mv;
	Psym_info *	psym;
	int 		i;
	int		errcnt;

	errcnt = 0;
	for (LIST_TRAVERSE(&ofl->ofl_ismove, lnp, isp)) {
		Ifl_desc *	ifile = isp->is_file;
		Xword		num;

		DBG_CALL(Dbg_move_input1(ifile->ifl_name));
		DBG_CALL(Dbg_move_title(1));
		mv = (Move *) isp->is_indata->d_buf;
		num = isp->is_shdr->sh_size/isp->is_shdr->sh_entsize;
		for (i = 0; i < num; i++) {
			sdp = isp->is_file->ifl_oldndx
			    [ELF_M_SYM(mv->m_info)];
			DBG_CALL(Dbg_move_mventry(0, mv, sdp));
			/*
			 * Check if this entry has a valid size of not
			 */
			/* LINTED */
			switch (ELF_M_SIZE(mv->m_info)) {
			case 1: case 2: case 4: case 8:
				break;
			default:
				eprintf(ERR_WARNING,
					MSG_INTL(MSG_PSYM_WARN3));
				mv++;
				continue;
			}

			if (sdp->sd_psyminfo == (Psym_info *)NULL) {
				/*
				 * Mark the symbol as partial
				 * and intall the symbol in
				 * partial symbol list.
				 */
				if ((psym = (Psym_info *)
				    libld_calloc(sizeof (Psym_info), 1)) ==
				    (Psym_info *)NULL)
					return (S_ERROR);
				psym->psym_symd = sdp;
				sdp->sd_psyminfo = psym;
				if (insert_psym(ofl, psym) == 0)
					return (S_ERROR);
				/*
				 * Mark the input section which the partially
				 * initialized * symbol is defined.
				 * This is needed when the symbol
				 * the relocation entry uses symbol information
				 * not from the symbol entry.
				 *
				 * For executable, the following is
				 * needed only for expanded symbol. However,
				 * for shared object * any partially non
				 * expanded symbols are moved * from
				 * .bss/COMMON to .sunwbss. So the following are
				 * needed.
				 */
				if ((sdp->sd_sym->st_shndx != SHN_UNDEF) &&
				    (sdp->sd_sym->st_shndx < SHN_LOPROC)) {
					Is_desc *	isym;

					isym = ifile->ifl_isdesc[
						sdp->sd_sym->st_shndx];
					isym->is_flags |= FLG_IS_RELUPD;
					if (sdp->sd_osym == (Sym *) 0) {
						if ((sdp->sd_osym = (Sym *)
						    libld_calloc(
						    sizeof (Sym), 1)) == NULL)
							return (S_ERROR);
						*(sdp->sd_osym) =
							*(sdp->sd_sym);
					}
				}
			} else
				psym = sdp->sd_psyminfo;

			if (install_mv(psym, mv, isp) == S_ERROR)
				return (S_ERROR);

			/*
			 * If this symbol is marked to be
			 * expanded, go to the next moveentry.
			 */
			if (sdp->sd_flags & FLG_SY_PAREXPN) {
				mv++;
				continue;
			}

			/*
			 * Decide whether this partial symbol is to be expanded
			 * or not.
			 *
			 * The symbol will be expanded if:
			 *	a) '-z nopartial' is specified
			 *	b) move entries covered entire symbol
			 */
			if (((ofl->ofl_flags & FLG_OF_STATIC) != 0) &&
			    ((ofl->ofl_flags & FLG_OF_EXEC) != 0)) {
				if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
				    STT_SECTION) {
					errcnt++;
					eprintf(ERR_FATAL,
					MSG_INTL(MSG_PSYM_FATAL1));
				} else {
					sdp->sd_flags |= FLG_SY_PAREXPN;
				}
			} else if ((ofl->ofl_flags1 & FLG_OF1_NOPARTI) != 0) {
				if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
				    STT_SECTION)
					eprintf(ERR_WARNING,
					MSG_INTL(MSG_PSYM_WARN1));
				else {
					sdp->sd_flags |= FLG_SY_PAREXPN;
				}
			} else if (
			    (((sizeof (Move)) * psym->psym_num) >
			    psym->psym_symd->sd_sym->st_size) &&
			    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_OBJECT)) {
				sdp->sd_flags |= FLG_SY_PAREXPN;
			}
			mv++;
		}
	}

	/*
	 * Check if there were any overlapping
	 */
	for (LIST_TRAVERSE(&ofl->ofl_parsym, lnp, psym)) {
		if ((psym->psym_flag & FLG_PSYM_OVERLAP) != 0) {
			errcnt++;
			eprintf(ERR_FATAL,
				MSG_INTL(MSG_PSYM_OVERLAP),
				psym->psym_symd->sd_name,
				psym->psym_symd->sd_file->ifl_name);
		}
	}

	DBG_CALL(Dbg_move_psyminfo(ofl));
	if (errcnt != 0)
		return (S_ERROR);
	if (make_mvsections(ofl) == S_ERROR)
		return (S_ERROR);

	return (1);
}
