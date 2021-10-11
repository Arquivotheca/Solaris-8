/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)link.c	1.5	97/09/17 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

static void
link_col_major(MENU *m)
{
	ITEM *i;
	int n;
	short c, r;
	int left, up;

	r = 0;
	c = 0;
	for (i = IthItem(m, 0), n = 0; i; i = IthItem(m, ++n)) {
		X(i) = c;
		Y(i) = r;
		Left(i) = c ? IthItem(m, n-Rows(m)) : (ITEM *) NULL;
		if (n + Rows(m) >= Nitems(m)) {
			Right(i) = (ITEM *) NULL;
		} else {
			Right(i) = IthItem(m, n + Rows(m));
		}
		Up(i) = r ? IthItem(m, n-1) : (ITEM *) NULL;
		Down(i) = (r == Rows(m)-1) ? (ITEM *)0 : IthItem(m, n+1);
		if (++r == Rows(m)) {
			r = 0;
			c += 1;
		}
	}
	if (r) {
		Down(IthItem(m, n-1)) = IthItem(m, n - Rows(m));
	}

	if (Cyclic(m)) {
		/* Set up left and right links at edge of menu */

		r = Rows(m) * (Nitems(m)/Rows(m));
		for (n = 0; n < Rows(m); n++) {
			left = n + r;
			if (left >= Nitems(m)) {
				left -= Rows(m);
			}
			Left(IthItem(m, n)) = IthItem(m, left);
			Right(IthItem(m, left)) = IthItem(m, n);
		}

		/* Setup up and down links at edge of menu */

		for (n = 0; n < Nitems(m); n += Rows(m)) {
			up = n + Rows(m) - 1;
			if (up >= Nitems(m)) {
				Up(IthItem(m, n)) = IthItem(m, n-1);
			} else {
				Up(IthItem(m, n)) = IthItem(m, up);
				Down(IthItem(m, up)) = IthItem(m, n);
			}
		}
	}
}

static void
link_row_major(MENU *m)
{
	int n;
	short c, r;
	ITEM *i;
	int left, up;

	r = 0;
	c = 0;
	for (i = IthItem(m, 0), n = 0; i; i = IthItem(m, ++n)) {
		X(i) = c;
		Y(i) = r;
		Left(i) = c ? IthItem(m, n-1) : (ITEM *) NULL;
		Right(i) = (c == Cols(m)-1 || n == Nitems(m)-1) ? (ITEM *)0 :
				IthItem(m, n+1);
		Up(i) = r ? IthItem(m, n-Cols(m)) : (ITEM *) NULL;

		if (n+Cols(m) < Nitems(m)) {
			Down(i) = IthItem(m, n + Cols(m));
		} else {
			if (r == Rows(m)-1) {
				/* Down is undefined if this is a complete */
				/* last column */
				Down(i) = (ITEM *) NULL;
			} else {
				/* Down is set to last item if the last */
				/* column isn't full */
				Down(i) = IthItem(m, Nitems(m)-1);
			}
		}
		if (++c == Cols(m)) {
			c = 0;
			r += 1;
		}
	}

	if (Cyclic(m)) {

		/* Setup left and right links at edge of menu */

		for (n = 0; n < Nitems(m); n += Cols(m)) {
			left = n + Cols(m) - 1;
			if (left >= Nitems(m)) {
				left = Nitems(m) - 1;
			}
			Left(IthItem(m, n)) = IthItem(m, left);
			Right(IthItem(m, left)) = IthItem(m, n);
		}

		/* Setup up and down links at edge of menu */

		r = (Rows(m) - 1) * Cols(m);
		for (n = 0; n < Cols(m); n++) {
			up = n + r;

			/* If there is an incomplete line below this one */
			/* the point to the last item in the menu. */

			if (up >= Nitems(m)) {
				Up(IthItem(m, n)) = IthItem(m, Nitems(m)-1);
			} else {
				Up(IthItem(m, n)) = IthItem(m, up);
				Down(IthItem(m, up)) = IthItem(m, n);
			}
		}
	}
}

void
_link_items(MENU *m)
{
	if (Items(m) && IthItem(m, 0)) {
		ResetLink(m);
		if (RowMajor(m)) {
			link_row_major(m);
		} else {
			link_col_major(m);
		}
	}
}
