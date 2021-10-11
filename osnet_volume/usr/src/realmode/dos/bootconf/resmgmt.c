/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  resmgmt.c -- bus resource management:
 */

#ident "@(#)resmgmt.c   1.27   97/12/10 SMI"

#include "types.h"

#include "menu.h"
#include "boot.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "resmgmt.h"
#include "tree.h"
#include "ur.h"

static void valid_resource(unsigned t, unsigned long a,
    unsigned long l);

Board *
Query_resmgmt(Board *bp, u_int t, u_long a, u_long l)
{
	u_int share = t & RESF_SHARE;
	u_int usurp = t & RESF_USURP;

	/*
	 * Knock off all flags except those we are interested in
	 */
	t &= RESF_TYPE | RESF_ALT;

	/*
	 *  Check for resource conflict:
	 *
	 *  This routine searches the board list rooted at "bp" for a device
	 *  using resources of type "t" that conflict with the target resource
	 *  defined by the given "a"ddress and "l"ength.  It returns a pointer
	 *  to the conflicting function record, or NULL if no device/functions
	 *  in the board list conflicts with the target resource.
	 */

	valid_resource(t, a, l);

	while (bp != 0) {
		/*
		 *  Check each record in the board list until (and if) we
		 *  find a resource conflict.
		 */

		int rc;
		Resource *rp = resource_list(bp);

		for (rc = resource_count(bp); rc--; rp++) {
			/*
			 * Scan thru the resource list looking for
			 * records of the current "t"ype that overlap
			 * the target range.  But watch for
			 * shared resources, they never conflict.
			 */

			if (share && (rp->flags & RESF_SHARE)) {
				continue;
			}
			if ((t == (rp->flags & (RESF_TYPE | RESF_ALT))) &&
			    ((a >= rp->base &&
				    (a < (rp->base + rp->length))) ||
			    (rp->base >= a) &&
				    (rp->base < (a+l)))) {
				/*
				 * Not a conflict if we are usurping a weak
				 * resource
				 */
				if (usurp && (rp->flags & RESF_WEAK))
					continue;
				else
					return (bp);
			}
		}
		bp = bp->link;
	}
	return (0);
}

static void
valid_resource(unsigned t, unsigned long a, unsigned long l)
{
	switch (t & RESF_TYPE) {

	case RESF_Port:
		if ((a + l - 1) > MAX_PORT_ADDR) {
			fatal("bad port 0x%lx len 0x%lx\n", a, l);
		}
		break;
	case RESF_Irq:
		if ((a + l - 1) >= NUM_IRQ) {
			fatal("bad irq 0x%lx len 0x%lx\n", a, l);
		}
		break;
	case RESF_Dma:
		if ((a + l - 1) >= NUM_DMA) {
			fatal("bad dma 0x%lx len 0x%lx\n", a, l);
		}
		break;
	case RESF_Mem:
		break;
	default:
		fatal("bad resource type 0x%x", t);
	}
}

/*
 * For each resource of the target board check for conflict
 * Return the resource (and the conflicting Board if requested)
 * if a conflict is found, else NULL.
 */
Resource *
board_conflict_resmgmt(Board *bp, short test_weak, Board **cbp)
{
	Board *confbp;
	Resource *rp;
	int rc;

	for (rp = resource_list(bp), rc = resource_count(bp); rc--; rp++) {
		/*
		 * Check each resource of each function.
		 */
		if (rp->flags & RESF_ALT) {
			continue;
		}
		if (test_weak && (!(rp->flags & RESF_WEAK))) {
			continue;
		}
		if (confbp = Query_resmgmt(Head_board,
		    rp->flags, rp->base, rp->length)) {
			if (cbp) {
				*cbp = confbp;
			}
			return (rp);
		}
	}
	return (NULL);
}

/*
 * Check all boards, mark boards that gave up weak resources as disabled.
 */
void
check_weak()
{
	Board *bp;
	Resource *rp;
	int rc;

	for (bp = Head_board; bp; bp = bp->link) {
		rp = resource_list(bp);
		rc = resource_count(bp);
		for (; rc; rp++, rc--) {
			if (rp->flags & RESF_WEAK)
				bp->flags &= ~BRDF_DISAB;
		}
		if (weak_binding_tree(bp))
			bp->flags |= BRDF_DISAB;
	}
}
