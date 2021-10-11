/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dumprcv.c	1.5	92/07/14 SMI"        /* SVr4.0 1.6 */

#include "mail.h"
/*
 * Put out H_RECEIVED lines if necessary, or
 * suppress their printing from the calling routine.
 */
void dumprcv (type, htype, didrcvlines, suppress, f)
register int	type;
register int	htype;
register int	*didrcvlines;
register int	*suppress;
register FILE	*f;
{
	int		rcvspot;	/* Place to put H_RECEIVED lines */
	struct hdrs	*hptr;
	char		*pn = "dumprcv";

	Dout(pn, 15, "type=%d, htype=%d/%s, *didrcvlines=%d, *suppress=%d\n", type, htype, htype >= 0 ? header[htype].tag : "None", *didrcvlines, *suppress);

	rcvspot = pckrcvspot();
	if (rcvspot == -1) {
		Dout(pn, 15, "\trcvspot==-1\n");
		return;
	}

	if (htype == H_RECEIVED) {
		*suppress = TRUE;
	}

	if (*didrcvlines == TRUE) {
		Dout(pn, 15, "\tdidrcvlines == TRUE\n");
		return;
	}
	if ((htype >= 0) && (rcvspot != htype)) {
		Dout(pn, 15, "\thtype < 0 || rcvspot != htype, *suppress=%d\n", *suppress);
		return;
	}

	*didrcvlines = TRUE;
	for (hptr = hdrlines[H_RECEIVED].head; 
	     hptr != (struct hdrs *)NULL; 
	     hptr = hptr->next) {
		printhdr(type, H_RECEIVED, hptr, f);
	}
	Dout(pn, 15, "\t*didrcvlines=%d, *suppress=%d\n", *didrcvlines, *suppress);
}
