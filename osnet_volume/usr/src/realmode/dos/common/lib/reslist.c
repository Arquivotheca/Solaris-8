/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  ISA Bus Resource Management:
 *
 *    This file contains code to implements the bus resource list functions.
 *    There are two such functions, one for unit resources and one for
 *    aggregate resources.
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)reslist.c	1.4	95/06/09 SMI\n"
#include <resmgmt.h>

int
LstUnit(char *list, int unit, int lim)
{
	/*
	 *  List unit resource reservations:
	 *
	 *  This routine will return the next unit reservation in the
	 *  specified "list" immediately after the given "unit".  To get
	 *  the first reservation, specify "unit" as -1.
	 *
	 *  Returns -1 if there are no more reservations in the list.
	 */

	while ((++unit < lim) && !list[unit]);
	return ((unit >= lim) ? -1 : unit);
}

int LstRange(struct _range_ _far **head, unsigned long _far *ap,
						    unsigned long _far *lp)
{
	/*
	 *  List aggregate unit reservations:
	 *
	 *  This routine will find the aggregate resource reservation in the
	 *  list specified by "head" that immediately follows the reservation
	 *  based at "*ap".  The base and size of the next reservation are
	 *  stored at "*ap" and "*lp", respectively.  To get the first reser-
	 *  vation, pass -1 at "*ap".
	 *
	 *  Returns 0 when we deliver a reservation, -1 if there are none
	 *  left in the list.
	 *
	 *  NOTE: The far pointer at "head[1]" points to the last _range_ entry
	 *        returned by this routine.  If it hasn't been invalidated (e.g,
	 *        by a deleted reservation), we can start our search from there.
	 */

	struct _range_ _far *cp = head[0];

	if (*ap != (unsigned long)-1) {
		/*
		 *  If we're not starting from the beginning, step thru the
		 *  range list until we find the entry immediately behind the
		 *  one identified by the caller.
		 */

		if (head[1] && (head[1]->base <= *ap)) cp = head[1];
		while (cp && (cp->base <= *ap)) cp = cp->next;
	}

	return ((head[1] = cp) ? (*ap = cp->base, *lp = cp->size, 0) : -1);
}
