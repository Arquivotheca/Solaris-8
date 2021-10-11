/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)insque.c	1.8	96/12/04 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

/*
 * insque() and remque() insert or remove an element from a queue.
 * The queue is built from a doubly linked list whose elements are
 * defined by a structure where the first member of the structure
 * points to the next element in the queue and the second member
 * of the structure points to the previous element in the queue.
 */

#pragma weak insque = _insque
#pragma weak remque = _remque

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>
#include <search.h>

void
insque(void *elem, void *pred)
{
	if (pred == NULL) {    /* This is the first element being inserted. */
		((struct qelem *)elem)->q_forw = NULL;
		((struct qelem *)elem)->q_back = NULL;
	} else if (((struct qelem *)pred)->q_forw == NULL) {
					/* The element is inserted at */
					/* the end of the queue. */
		((struct qelem *)elem)->q_forw = NULL;
		((struct qelem *)elem)->q_back = pred;
		((struct qelem *)pred)->q_forw = elem;
	} else {		/* The element is inserted in the middle of */
				/* the queue. */
		((struct qelem *)elem)->q_forw = ((struct qelem *)pred)->q_forw;
		((struct qelem *)elem)->q_back = pred;
		((struct qelem *)pred)->q_forw->q_back = elem;
		((struct qelem *)pred)->q_forw = elem;
	}
}

void
remque(void *elem)
{
	if (((struct qelem *)elem)->q_back == NULL) {
					/* The first element is removed. */
		if (((struct qelem *)elem)->q_forw == NULL)
					/* The only element is removed. */
			return;
		((struct qelem *)elem)->q_forw->q_back = NULL;
	} else if (((struct qelem *)elem)->q_forw == NULL) {
					/* The last element is removed */
		((struct qelem *)elem)->q_back->q_forw = NULL;
	} else {	/* The middle element is removed. */
		((struct qelem *)elem)->q_back->q_forw =
				((struct qelem *)elem)->q_forw;
		((struct qelem *)elem)->q_forw->q_back =
				((struct qelem *)elem)->q_back;
	}
}
