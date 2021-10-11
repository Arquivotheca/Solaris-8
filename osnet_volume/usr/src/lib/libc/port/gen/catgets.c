/*
 * catgets.c
 *
 * Copyright (c) 1990, 1991, 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)catgets.c	1.11	96/10/15 SMI"

/* LINTLIBRARY */

#pragma weak catgets = _catgets

#include "synonyms.h"
#include <sys/types.h>
#include <nl_types.h>

char *
_catgets(nl_catd catd_st, int set_id, int msg_id, const char *def_str)
{
	int			hi, lo, mid;
	struct	_cat_hdr 	*p;
	struct	_cat_set_hdr	*q;
	struct	_cat_msg_hdr	*r;
	void			*catd;

	if (catd_st == NULL ||
	    catd_st == (nl_catd)-1 ||
	    catd_st->__content == NULL ||
	    catd_st->__size == 0)
		return ((char *)def_str);

	catd = catd_st->__content;
	p = (struct _cat_hdr *) catd_st->__content;
	hi = p->__nsets - 1;
	lo = 0;
	/*
	 * Two while loops will perform binary search.
	 * Outer loop searches the set and inner loop searches
	 * message id
	 */
	while (hi >= lo) {
		mid = (hi + lo) / 2;
		q = (struct _cat_set_hdr *)
			((char *) catd
			+ _CAT_HDR_SIZE
			+ _CAT_SET_HDR_SIZE * mid);
		if (q->__set_no == set_id) {
			lo = q->__first_msg_hdr;
			hi = lo + q->__nmsgs - 1;
			while (hi >= lo) {
				mid = (hi + lo) / 2;
				r = (struct _cat_msg_hdr *) (
					(char *) catd
					+ _CAT_HDR_SIZE
					+ p->__msg_hdr_offset
					+ _CAT_MSG_HDR_SIZE * mid);
				if (r->__msg_no == msg_id) {
					return ((char *) catd
						+ _CAT_HDR_SIZE
						+ p->__msg_text_offset
						+ r->__msg_offset);
				} else if (r->__msg_no < msg_id)
					lo = mid + 1;
				else
					hi = mid - 1;
			} /* while */

			/* In case set number not found */
			return ((char *)def_str);
		} else if (q->__set_no < set_id)
			lo = mid + 1;
		else
			hi = mid - 1;
	} /* while */

	/* In case msg_id not found. */
	return ((char *)def_str);
}
