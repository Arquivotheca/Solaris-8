/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers:
 */

#ident	"<@(#)qsort.c	1.4	95/08/14	SMI>"
#include <stdlib.h>

static void
swap(char far *p, char far *q, int len)
{
	/*
	 *  Qsort buffer swap:
	 *
	 *  Swaps the contents of the "len"-byte buffer at "*p" with the
	 *  buffer at "*q".
	 */

	while (len-- > 0) {
		/*
		 *  It's hard to get simpler than this ...
		 */

		register int tmp = *p;
		*p++ = *q; *q++ = tmp;
	}
}

void
qsort(void far *a, size_t c, size_t l, int (*cmp)(void far *, void far *))
{
	/*
	 *  Quicksort routine:
	 *
	 *  I thought about using the qsort routine from Solaris libc, but it's
	 *  really much more complicated than we need here.  Notice that I do
	 *  none of the traditional optimizing (e.g, switching to insertion
	 *  sort when the "c"ount gets small enough, etc).  The conditiions
	 *  under which realmode drivers actually call qsort limit the value
	 *  of these optimizations, and code space is more of a problem for
	 *  realmode drivers than is running time (within reason, of course).
	 */

	int rcnt;
	char far *lp = a;
	char far *mp = &lp[(c >> 1) * l];
	char far *rp = &lp[(c  - 1) * l];

	switch (c) {
		/*
		 *  Partition the sort space into three parts:  "lp" points to
		 *  unsorted stuff on the left, "rp" just beyond unsorted stuff
		 *  on the right.  "mp" points to an entry somewhere in the
		 *  middle of the current partition that seperates the left and
		 *  right subpartitions.
		 */

		default: {
			/*
			 *  We have two full partitions.  Make sure the first
			 *  entry on the left is less than the last entry on
			 *  the right.
			 */

			if ((*cmp)(lp, rp) > 0) swap(lp, rp, l);
			if ((*cmp)(mp, rp) > 0) swap(mp, rp, l);
			/*FALLTHROUGH*/
		}

		case 2: {
			/*
			 *  We have half a partition.  Make sure the first
			 *  entry on the left is less than the midpoint.
			 */

			if ((*cmp)(lp, mp) > 0) swap(lp, mp, l);
			/*FALLTHROUGH*/
		}

		if (c < 4) {
			/*
			 *  Arrays of 3 or fewer entries are sorted as a side
			 *  effect of the partitioning process!
			 */

			case 1: case 0:
			return;
		}
	}

	for ((rp -= l, lp += l); lp < mp; lp += l) {
		/*
		 *  Compare entries in the left half of the partition with
		 *  those on the right, swapping any that appear to be out
		 *  of place.
		 */

		if ((*cmp)(lp, mp) > 0) {
			/*
			 *  Next entry on the left side belongs on the right.
			 *  Advance the mid pointer until it points to an el-
			 *  ement on the right side that belongs on the left.
			 */

			while ((rp > mp) && ((*cmp)(mp, rp) <= 0)) rp -= l;

			if (rp > mp) {
				/*
				 *  If the search above was successful, we can
				 *  exchange the bogus left and right side el-
				 *  ements and balance everything out.
				 */

				swap(lp, rp, l);
				rp -= l;

			} else for (;;) {
				/*
				 *  If there are no elements on the right side
				 *  that belong on the left, move the mid ptr
				 *  to the left until we find one!
				 */

				mp -= l;
				swap(mp, rp, l);

				if ((lp == mp) || ((*cmp)(mp, rp) > 0)) {
					/*
					 *  The midpoint has moved to the left
					 *  by at least one position, and the
					 *  element to its right either:
					 *
					 *    [a] Belongs on the left: swap it
					 *        with "*lp"
					 *
					 *    [b] Is the bogus left-side el-
					 *	  ement, and is now on the right
					 *	  (because the midpoint moved).
					 */

					if (lp != mp) swap(lp, rp, l);
					rp = mp;
					break;
				}

				rp = mp;
			}
		}
	}

	for (; rp > mp; rp -= l) {
		/*
		 *  Now check the untested elements on the right side (those
		 *  between "mp" and "rp") to see if they sort greater than
		 *  the midpoint.
		 */

		if ((*cmp)(mp, rp) > 0) {
			/*
			 *  Another entry out of place ...
			 */

			do {
				/*
				 *  .. move the midpoint to the right until
				 *  either:
				 *
				 *    [a]  We've included a left-side element
				 *	   that doesn't belong there.
				 *
				 *    [b]  The bogus right-side element ends up
				 *	    on the left.
				 */

				lp = mp; mp += l;
				swap(lp, mp, l);

			} while ((mp < rp) && ((*cmp)(lp, mp) <= 0));

			if (mp != rp) {
				/*
				 *  Condition [a] cause the above loop to exit.
				 *  Swap the bogus elements on left and right
				 *  side of the partition.
				 */

				swap(lp, rp, l);
			}
		}
	}

	/*
	 *  Now recursively sort the left and right sub-partitions.  The element
	 *  at the mid point is already in its proper position.
	 */

	rcnt = (mp - (char far *)a)/l;
	qsort(a, rcnt, l, cmp);
	qsort(mp+l, c - (rcnt+1), l, cmp);
}
