/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)order.c	1.25	99/05/27 SMI"

/*
 * Processing of SHF_ORDERED sections.
 */
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>
#include	<link.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"

/*
 * Part 1, Input processing.
 */
/*
 * Get the head section number
 */
static Word
is_shinfo_ok(Ifl_desc *ifl, Is_desc *isp, Word limit)
{
	Word info = isp->is_shdr->sh_info;

	if ((info != SHN_BEFORE) && (info != SHN_AFTER)) {
		/*
		 * Range Check
		 */
		if ((info == 0) || (info >= limit)) {
			return (DBG_ORDER_LINK_OUTRANGE);
		}

		/*
		 * The section pointed by sh_info should not be an
		 * ordered section.
		 */
		if (ifl->ifl_isdesc[info]->is_shdr->sh_flags &
		    SHF_ORDERED) {
			return (DBG_ORDER_INFO_ORDER);
		}
	}
	return (0);
}

static Word
get_head_ndx(Ifl_desc *ifl, Word ndx, Word limit)
{
	Word t1_link = ndx, t2_link, ret_link;
	Is_desc *isp, *isp1, *isp2;
	int error = 0;

	/*
	 * Check the sh_info of myself.
	 */
	isp = ifl->ifl_isdesc[ndx];
	if ((error = is_shinfo_ok(ifl, isp, limit)) != 0) {
		DBG_CALL(Dbg_sec_order_error(ifl, ndx, error));
		return (0);
	}

	isp1 = ifl->ifl_isdesc[t1_link];
	ret_link = t2_link = isp1->is_shdr->sh_link;

	do {
		/*
		 * Check the validitiy of the link
		 */
		if (t2_link == 0 || t2_link >= limit) {
			error = DBG_ORDER_LINK_OUTRANGE;
			break;
		}
		isp2 = ifl->ifl_isdesc[t2_link];

		/*
		 * Pointing to a bad ordered section ?
		 */
		if ((isp2->is_flags & FLG_IS_ORDERED) == 0) {
			error = DBG_ORDER_LINK_ERROR;
			break;
		}

		/*
		 * Check sh_flag
		 */
		if (isp1->is_shdr->sh_flags != isp2->is_shdr->sh_flags) {
			error = DBG_ORDER_FLAGS;
			break;
		}

		/*
		 * Check the validity of sh_info field.
		 */
		if ((error = is_shinfo_ok(ifl, isp2, limit)) != 0) {
			break;
		}

		/*
		 * Can I break ?
		 */
		if (t1_link == t2_link)
			break;

		/*
		 * Get the next link
		 */
		t1_link = t2_link;
		isp1 = ifl->ifl_isdesc[t1_link];
		ret_link = t2_link = isp1->is_shdr->sh_link;

		/*
		 * Cyclic ?
		 */
		if (t2_link == ndx) {
			error = DBG_ORDER_CYCLIC;
			break;
		}
	/* CONSTANTCONDITION */
	} while (1);

	if (error != 0) {
		ret_link = 0;
		DBG_CALL(Dbg_sec_order_error(ifl, ndx, error));
	}
	return (ret_link);
}

/*
 * Called from process_elf().
 * This routine does the input processing of the ordered sections.
 */
uintptr_t
process_ordered(Ifl_desc *ifl, Ofl_desc *ofl, Word ndx, Word limit)
{
	Is_desc *	isp2, * isp = ifl->ifl_isdesc[ndx];
	Os_desc *	osp2, * osp;
	Word		head_ndx;
	Sort_desc *	st;
	Listnode *	lnp;

	/*
	 * I might have been checked and marked error already.
	 */
	if ((isp->is_flags & FLG_IS_ORDERED) == 0)
		return (0);

	/*
	 * Get the head section
	 */
	if ((head_ndx = get_head_ndx(ifl, ndx, limit)) == 0) {
		isp->is_flags &= ~FLG_IS_ORDERED;
		if (isp->is_osdesc == NULL)
			return ((uintptr_t)place_section(ofl, isp,
			    isp->is_key, 0));
		return ((uintptr_t) isp->is_osdesc);
	}

	/*
	 * Got the head ok.
	 */
	if ((osp = isp->is_osdesc) == NULL) {
		if ((osp = place_section(ofl, isp, 0, head_ndx)) == 0)
			return ((uintptr_t) S_ERROR);
	}

	osp2 = NULL;
	for (LIST_TRAVERSE(&ofl->ofl_ordered, lnp, osp2))
		if (osp2 == osp)
			break;

	if (osp != osp2)
		if (list_appendc(&(ofl->ofl_ordered), osp) == 0)
			return ((uintptr_t)S_ERROR);

	/*
	 * Here, osp is valid
	 */
	if ((st = osp->os_sort) == 0) {
		st = osp->os_sort = (Sort_desc *)
			libld_calloc(1, sizeof (Sort_desc));
		if (osp->os_sort == NULL)
			return (S_ERROR);
	}
	if (isp->is_shdr->sh_info == SHN_BEFORE) {
		st->st_beforecnt++;
	} else if (isp->is_shdr->sh_info == SHN_AFTER) {
		st->st_aftercnt++;
	} else {
		st->st_ordercnt++;
		isp2 = ifl->ifl_isdesc[isp->is_shdr->sh_info];
		osp2 = isp2->is_osdesc;
		osp2->os_flags |= FLG_OS_ORDER_KEY;
		osp2->os_sgdesc->sg_flags |= FLG_SG_KEY;
		isp2->is_flags |= FLG_IS_KEY;
	}

	return ((uintptr_t) osp);
}

/*
 * Part 2, Sorting processing
 */

/*
 * Traverse all of the segments looking for section ordering information
 * that wasn't used.  If found give a warning message to the user.
 * Also, check if there are any SHF_ORDERED key section.
 * If there are any, set up sort key values.
 */
void
sec_validate(Ofl_desc * ofl)
{
	Listnode *	lnp1, *lnp2, *lnp3;
	Sg_desc *	sgp;
	Sec_order *	scop;
	Os_desc *	osp;
	Is_desc *	isp;
	int 		key = 1;

	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
	    for (LIST_TRAVERSE(&sgp->sg_secorder, lnp2, scop))
		if (!(scop->sco_flags & FLG_SGO_USED))
			eprintf(ERR_WARNING, MSG_INTL(MSG_MAP_SECORDER),
			    sgp->sg_name, scop->sco_secname);
	    if ((sgp->sg_flags & FLG_SG_KEY) == 0)
		continue;
	    for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
		if ((osp->os_flags & FLG_OS_ORDER_KEY) == 0)
			continue;
		for (LIST_TRAVERSE(&(osp->os_isdescs), lnp3, isp)) {
		    if (isp->is_flags & FLG_IS_KEY) {
			isp->is_key = key++;
		    }
		}
	    }
	}
}

static int
setup_sortbuf(Os_desc *osp)
{
	Sort_desc *st = osp->os_sort;
	Word num, num_after = 0, num_before = 0, num_order = 0;
	Listnode *lnp1;
	Is_desc *isp;

	if ((st == NULL) ||
	    ((num = st->st_ordercnt + st->st_beforecnt + st->st_aftercnt) == 0))
		/* LINTED */
		return ((int)S_ERROR);

	/*
	 * Get memory
	 */
	st->st_list = (Listnode **)libld_calloc(num, sizeof (Listnode *));
	if (st->st_list == NULL)
		/* LINTED */
		return ((int)S_ERROR);
	if (st->st_beforecnt != 0) {
		if ((st->st_before = (Is_desc **)
		    libld_calloc(st->st_beforecnt, sizeof (Is_desc *))) == NULL)
			/* LINTED */
			return ((int)S_ERROR);
	}
	if (st->st_ordercnt != 0) {
		if ((st->st_order = (Is_desc **)
		    libld_calloc(st->st_ordercnt, sizeof (Is_desc *))) == NULL)
			/* LINTED */
			return ((int)S_ERROR);
	}
	if (st->st_aftercnt != 0) {
		if ((st->st_after = (Is_desc **)
		    libld_calloc(st->st_aftercnt, sizeof (Is_desc *))) == NULL)
			/* LINTED */
			return ((int)S_ERROR);
	}

	/*
	 * Set info.
	 */
	num = 0;
	for (LIST_TRAVERSE(&(osp->os_isdescs), lnp1, isp)) {
		if ((isp->is_flags & FLG_IS_ORDERED) == 0)
			continue;
		st->st_list[num++] = lnp1;
		if (isp->is_shdr->sh_info == SHN_BEFORE)
			st->st_before[num_before++] = isp;
		else if (isp->is_shdr->sh_info == SHN_AFTER)
			st->st_after[num_after++] = isp;
		else
			st->st_order[num_order++] = isp;
	}
	return (0);
}

static int
comp(const void *ss1, const void *ss2)
{
	Is_desc **s1 = (Is_desc **)ss1, **s2 = (Is_desc **)ss2;
	Is_desc *i1, *i2;

	i1 = (*s1)->is_file->ifl_isdesc[(*s1)->is_shdr->sh_info];
	i2 = (*s2)->is_file->ifl_isdesc[(*s2)->is_shdr->sh_info];

	if (i1->is_key > i2->is_key)
		return (1);
	if (i1->is_key < i2->is_key)
		return (-1);
	return (0);
}

void
sort_ordered(Ofl_desc *ofl)
{
	Listnode *lnp1;
	Os_desc *osp;

	DBG_CALL(Dbg_sec_order_list(ofl, 0));

	/*
	 * Sort Sections
	 */
	for (LIST_TRAVERSE(&ofl->ofl_ordered, lnp1, osp)) {
		int i, idx = 0;
		Sort_desc *st = osp->os_sort;

		if (setup_sortbuf(osp) == S_ERROR)
			continue;

		/*
		 * Sorting.
		 * First Sort the ordered sections.
		 */
		if (st->st_ordercnt != 0)
			qsort((char *) st->st_order, st->st_ordercnt,
			sizeof (Is_desc *), comp);
		/*
		 * Update link, First SHF_BEFORE
		 */
		for (i = 0; i < st->st_beforecnt; i++)
			st->st_list[idx++]->data = (void *)st->st_before[i];
		/*
		 * Update link, Then sh_info
		 */
		for (i = 0; i < st->st_ordercnt; i++)
			st->st_list[idx++]->data = (void *)st->st_order[i];
		/*
		 * Update link, Then SHF_AFTER
		 */
		for (i = 0; i < st->st_aftercnt; i++)
			st->st_list[idx++]->data = (void *)st->st_after[i];
	}
	DBG_CALL(Dbg_sec_order_list(ofl, 1));
}
