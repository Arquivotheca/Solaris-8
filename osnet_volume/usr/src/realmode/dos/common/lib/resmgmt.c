/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  ISA Bus Resource Management:
 *
 *    This file contains the code that implements aggregate bus resource
 *    reservations (see <resmgmt.h>).  It also contains the tables used to
 *    implement unit resource reservations, but the unit reservation operations
 *    themselves are implemented as macros in <resmgmt.h>.
 *
 *    The RelRange() routine resides in its own source file ("resfree.c")
 *    to avoid library bloat for those users of the resource management
 *    facility that don't need to delete reservations.
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)resmgmt.c	1.7	95/06/09 SMI\n"
#include <resmgmt.h>

#define	PORT_LIMIT 0xFFFF		  /* Limit of I/O address space	    */
#define	MEM_LIMIT  0x3FFFFFFF		  /* Limit of memory space	    */

char IrqResList[MAX_IRQ_RES_];		  /* One byte per reserved IRQ	    */
char DmaResList[MAX_DMA_RES_];		  /* One byte per DMA reservation   */
char SlotResList[MAX_SLOT_RES_];	  /* One byte per slot reservation  */

struct _range_ _far *MemResList[2];	  /* Range list header for memory   */
struct _range_ _far *IopResList[2];	  /* Range list header for ports    */

extern void _far *_fmalloc(unsigned);	  /* Rather than deal with include  */
extern void _ffree(void _far *);	  /* .. paths!			    */

void
ResRange(struct _range_ _far **head, unsigned long a, unsigned long l,
								    int s args_)
{
	/*
	 *  Aggregate resource reservation:
	 *
	 *  This routine may be used to reserve aggregate bus resources.  It
	 *  uses the classic linked-list method to keep track of the memory/
	 *  port ranges that have been reserved.
	 *
	 *  NOTE: For memory reservations, a length of zero means "from here
	 *        to the end".
	 */

	struct _range_ _far *cp, _far *pp = (struct _range_ _far *)head;
	int mt = (head == MemResList);

	if (mt && (l == 0)) l = (0x4000000 - a);
	while ((cp = pp->next) && (a > cp->base)) pp = cp;

#if	/* IF */ defined(DEBUG) /* THEN */
		/*
		 *  If we're debugging, we'll want to know about bogus
		 *  reservations:
		 */

		if ((l == 0) || ((a+l-1) > (mt ? MEM_LIMIT : PORT_LIMIT))) {
			/*
			 *  Length should be non-zero by now, and the address
			 *  should be within range.
			 */

			printf("\n*** Bad %s reservation, %s line %d ***\n",
				    (mt ? "memory" : "I/O port"), file, line);
			return;
		}

		if (s != 0) {
			/*
			 *  The "s"hared flag is provided for completeness,
			 *  but we don't support it.  Support for this option
			 *  could be added if the need arises.
			 */

			printf("\n*** Can't share %s reservation, "
			    "%s line %d ***\n",
			    (mt ? "memory" : "I/O port"), file, line);
		}
	/* FI */
#endif

	if ((pp != (struct _range_ _far *)head) && (a == (pp->base+pp->size))) {
		/*
		 *  Requested memory fits immediately behind the previous
		 *  reservation.  Bump its size accordingly and clear caller's
		 *  "l"engh argument so we'll know not to allocate a new range
		 *  struct.
		 */

		pp->size += l;
		a += l;
		l = 0;
	}

	if (cp != 0) {
		/*
		 *  The requested reservation does not get appended to the end
		 *  of the current reservation list.  Let's see if we can get
		 *  rid of a list entry!
		 */

		if ((a+l) > cp->base) {
			/*
			 *  This reservation conflicts with the one at "cp".
			 *  Remove the overlapping portion from the request
			 *  length so that we don't screw up the sequincing
			 *  of the reservation list.
			 */

#if 			/* IF */ defined(DEBUG) /* THEN */
				/*
				 *  ... Of course, if we're debugging we should
				 *   tell the programmer what we're doing first
				 */

				printf("\n*** Conflicting reservation, "
				    "%s line %d ***\n", file, line);
			/* FI */
#endif
			l = (cp->base - a);
		}

		if ((a+l) == cp->base) {
			/*
			 *  Requested memory fits immediately in front of
			 *  the current reservation ...
			 */

			if (l != 0) {
				/*
				 *  Adjust current entry's size and base to
				 *  include the memory we just reserved and
				 *  clear caller's "l"ength arg to prevent
				 *  allocation of a new range struct.
				 */

				cp->size += l;
				cp->base = a;
				l = 0;

			} else {
				/*
				 *  We just filled the hole between two res-
				 *  ervations!  Adjust the previous range entry
				 *  size to include the current one and free
				 *  the current entry as we don't need it any
				 *  more.
				 */

				pp->size += (cp->size + l);
				pp->next = cp->next;
				_ffree(cp);
			}
		}
	}

	if (l != 0) {
		/*
		 *  We were unable to use an existing range struct to record
		 *  this reservation, so we'll just have to buy a new one.
		 */

		if (cp = (struct _range_ _far *)_fmalloc(sizeof (*cp))) {
			/*
			 *  Link the new reservation between the previous and
			 *  current range structures.
			 */

			cp->base = a; cp->size = l;
			cp->next = pp->next;
			pp->next = cp;

		} else {
			/*
			 *  Malloc failure!  This should be very rare since
			 *  we're using "far" memory for all dynamically
			 *  allocated stuff.  This is considered a fatal error,
			 *  so we blow up!
			 */

			printf("\n*** Can't get memory "
					    "for range reservation ***\n");
			exit(100);
		}
	}

	head[1] = 0;	// Mark the list as having been modified!
}

int
QryRange(struct _range_ _far **head, unsigned long a, unsigned long l)
{
	/*
	 *  Test for reservation:
	 *
	 *  This routine returns a non-zero value if there's an aggregate
	 *  resource reservation at the given "a"ddress/"l"ength.
	 */

	struct _range_ _far *cp, _far *pp = (struct _range_ _far *)head;

	if ((head == MemResList) && (l == 0)) l = (0x4000000 - a);
	while ((cp = pp->next) && (a >= (cp->base + cp->size))) pp = cp;

	return (cp && ((a+l) > cp->base));
}
