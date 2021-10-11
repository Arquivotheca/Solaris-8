/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machstate.c	1.4	99/08/16 SMI"

/*
 * Machine state management functions for kadb on i86pc
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/consdev.h>
#include <sys/debug/debug.h>
#include <sys/systm.h>

void callb_set_polled_callbacks(cons_polledio_t *polled);

struct debugvec dvec = {
	DEBUGVEC_VERSION_2,
	callb_set_polled_callbacks,	/* dv_set_polled_callbacks */
};

/*
 * Kernel callbacks for polled I/O
 */
cons_polledio_t	polled_io;

/*
 * Called to notify kadb that the kernel has taken over the console and
 * the arg points to the polled input/output functions
 */
void
callb_set_polled_callbacks(cons_polledio_t *polled)
{
	/*
	 * If the argument is NULL, then just return
	 */
	if (polled == NULL)
		return;

	bcopy(polled, &polled_io, sizeof (cons_polledio_t));
}
