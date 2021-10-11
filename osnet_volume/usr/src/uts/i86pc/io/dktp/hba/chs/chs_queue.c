/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_queue.c	1.3	99/01/11 SMI"

#include <sys/types.h>
#include <sys/param.h>
#if defined(CHS_DEBUG) && defined(DEBUG)
#include <sys/ksynch.h>
#endif

#include "chs_queue.h"



void
QueueAdd(
	Que_t	*qp,
	Qel_t	*qelp,
	void	*datap
)
{
	if (!qp->qh_tailpp) {
		/* first time, initialize the queue header */
		qp->qh_headp = NULL;
		qp->qh_tailpp = &qp->qh_headp;
	}

	/* init the queue element */
	qelp->qe_linkp = NULL;
	qelp->qe_datap = datap;

	/* add it to the tailend */
	*(qp->qh_tailpp) = qelp;
	qp->qh_tailpp = &qelp->qe_linkp;

#ifdef CHS_DEBUG
	qp->qh_add++;
#endif

}



void *
QueueRemove(Que_t *qp)
{
	Qel_t	*qelp;

	/* pop one off the done queue */
	if ((qelp = qp->qh_headp) == NULL) {
		return (NULL);
	}

	/* if the queue is now empty fix the tail pointer */
	if ((qp->qh_headp = qelp->qe_linkp) == NULL)
		qp->qh_tailpp = &qp->qh_headp;
	qelp->qe_linkp = NULL;

#ifdef CHS_DEBUG
	qp->qh_rm++;
#endif

	return (qelp->qe_datap);
}
