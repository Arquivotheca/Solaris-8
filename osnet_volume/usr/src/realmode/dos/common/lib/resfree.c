/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  More ISA Bus Resource Management:
 *
 *    This file contains the "RelRange" routine that was left out of
 *    "resmgmt.c" to prevent library bloat.
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)resfree.c	1.3	95/06/09 SMI\n"
#include <resmgmt.h>

extern void _far *_fmalloc(unsigned); /* Rather than deal with include paths */
extern void _ffree(void _far *);

void
RelRange(struct _range_ _far **head, unsigned long a, unsigned long l args_)
{
	/*
	 *  Cancel an aggregate reservation:
	 *
	 *  This routine is the inverse of the "ResRange" routine defined in
	 *  "resmgmt.c".  It cancels the aggregate reservation defined by the
	 *  starting "a"ddress/"l"ength parameters and updates the reservation
	 *  list at "head" accordingly.
	 */

	long resid = -1;
	struct _range_ _far *cp;
	struct _range_ _far *pp = (struct _range_ _far *)head;
	while ((cp = pp->next) && (a >= (cp->base + cp->size))) pp = cp;

	if (cp && (a >= cp->base)) {
		/*
		 *  The "cp" register points to the range entry that describes
		 *  the reservation we're trying to cancel.  Compute the right
		 *  hand residual length.  If this is negative, we have a
		 *  problem!
		 */

		resid = (long)(cp->base+cp->size - (a+l));

		if (a == cp->base) {
			/*
			 *  Cancel from beginning of the current reservation ..
			 */

			if (resid > 0) {
				/*
				 *  ... if we have a positive residual, all we
				 *  need do is adjust the base and size of the
				 *  current list entry.
				 */

				cp->size = (unsigned long)resid;
				cp->base += l;

			} else {
				/*
				 *  ... Otherwise we're cancelling everything
				 *  in this entry so we free up the entry itself
				 *   while we're at it.
				 */

				pp->next = cp->next;
				_ffree(cp);
			}

		} else {
			/*
			 *  The current reservation entry has to remain in the
			 *  list to record the stuff to the left of the request
			 *  that isn't being cancelled.  Adjust entry size to
			 *  reflect this.
			 */

			cp->size = (a - cp->base);

			if (resid > 0) {
				/*
				 *  If we have a right-side residual, we'll
				 *  need another list entry to record it.
				 */

				if (pp = (struct _range_ _far *)
						    _fmalloc(sizeof (*pp))) {
					/*
					 *  The new entry address is now in the
					 *  "pp" register.  Set it's base and
					 *  size fields and link it behind the
					 *  current entry.
					 */

					pp->next = cp->next;
					pp->size = resid;
					pp->base = (a+l);
					cp->next = pp;

				} else {
					/*
					 *  No memory left for another list
					 *  entry.  This is a fatal error since
					 *  we just lost track of the right
					 *  side residual reservation!
					 */


					printf("\n*** Lost reservation due to "
						    "lack of memory ***\n");
					exit(100);
				}
			}
		}
	}

#if 	/* IF */ defined(DEBUG) /* THEN */
		/*
		 *  We do add some extra sanity checks when debugging ...
		 */

		if (resid < 0) {
			/*
			 *  A negative right-side residual means that the all
			 *  or part of the reservation we're trying to cancel
			 *  didn't exist!
			 */

			printf("\n*** Attempt to release unreserved %s, "
			    "%s line %d ***\n",
			    ((head == MemResList) ? "memory" : "I/O port"),
			    file,
			    line);
		}
	/* FI */
#endif
	head[1] = 0;
}
