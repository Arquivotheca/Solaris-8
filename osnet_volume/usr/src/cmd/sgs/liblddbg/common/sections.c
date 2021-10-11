/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sections.c	1.17	99/06/23 SMI"

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Error message string table.
 */
static const Msg order_errors[] = {
	MSG_ORD_ERR_INFORANGE,		/* MSG_INTL(MSG_ORD_ERR_INFORANGE) */
	MSG_ORD_ERR_ORDER,		/* MSG_INTL(MSG_ORD_ERR_ORDER) */
	MSG_ORD_ERR_LINKRANGE,		/* MSG_INTL(MSG_ORD_ERR_LINKRANGE) */
	MSG_ORD_ERR_FLAGS,		/* MSG_INTL(MSG_ORD_ERR_FLAGS) */
	MSG_ORD_ERR_CYCLIC,		/* MSG_INTL(MSG_ORD_ERR_CYCLIC) */
	MSG_ORD_ERR_LINKINV		/* MSG_INTL(MSG_ORD_ERR_LINKINV) */
};

void
Dbg_sec_in(Is_desc * isp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (isp->is_file != NULL)
		str = isp->is_file->ifl_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_INPUT), isp->is_name, str);
}

void
Dbg_sec_added(Os_desc * osp, Sg_desc * sgp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (sgp->sg_name && *sgp->sg_name)
		str = sgp->sg_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_ADDED), osp->os_name, str);
}

void
Dbg_sec_created(Os_desc * osp, Sg_desc * sgp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (sgp->sg_name && *sgp->sg_name)
		str = sgp->sg_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_CREATED), osp->os_name, str);
}

void
Dbg_sec_discarded(Is_desc * isp, Is_desc * disp)
{
	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	dbg_print(MSG_INTL(MSG_SEC_DISCARDED), isp->is_name,
	    isp->is_file->ifl_name, disp->is_name,
	    disp->is_file->ifl_name);
}

void
Dbg_sec_order_list(Ofl_desc * ofl, int flag)
{
	Os_desc *	osp;
	Is_desc *	isp1;
	Listnode *	lnp1, * lnp2;
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * If the flag == 0, then the routine is called before sorting.
	 */
	if (flag == 0)
		str = MSG_INTL(MSG_ORD_SORT_BEFORE);
	else
		str = MSG_INTL(MSG_ORD_SORT_AFTER);

	for (LIST_TRAVERSE(&ofl->ofl_ordered, lnp1, osp)) {
		Sort_desc *	sort = osp->os_sort;

		dbg_print(str, osp->os_name);
		dbg_print(MSG_INTL(MSG_ORD_HDR_1),
		    (int)sort->st_beforecnt, (int)sort->st_aftercnt,
		    (int)sort->st_ordercnt);

		for (LIST_TRAVERSE(&osp->os_isdescs, lnp2, isp1)) {
			if ((isp1->is_flags & FLG_IS_ORDERED) == 0)
				dbg_print(MSG_INTL(MSG_ORD_TITLE_0),
				    isp1->is_name, isp1->is_file->ifl_name);
			else if (isp1->is_shdr->sh_info == SHN_BEFORE)
				dbg_print(MSG_INTL(MSG_ORD_TITLE_1),
				    isp1->is_name, isp1->is_file->ifl_name);
			else if (isp1->is_shdr->sh_info == SHN_AFTER)
				dbg_print(MSG_INTL(MSG_ORD_TITLE_2),
				    isp1->is_name, isp1->is_file->ifl_name);
			else {
			    Ifl_desc *	ifl = isp1->is_file;
			    Is_desc *	isp2;

			    isp2 = ifl->ifl_isdesc[isp1->is_shdr->sh_info];
			    dbg_print(MSG_INTL(MSG_ORD_TITLE_3),
				isp1->is_name, ifl->ifl_name,
				isp2->is_name, isp2->is_key);
			}
		}
	}
}

void
Dbg_sec_order_error(Ifl_desc * ifl, Word ndx, int error)
{
	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (error == 0)
		return;

	dbg_print(MSG_INTL(MSG_ORD_ERR_TITLE),
		ifl->ifl_isdesc[ndx]->is_name, ifl->ifl_name);

	if (error)
		dbg_print(MSG_INTL(order_errors[error - 1]));
}
