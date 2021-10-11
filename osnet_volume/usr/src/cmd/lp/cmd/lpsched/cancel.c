/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cancel.c	1.13	96/04/10 SMI"	/* SVr4.0 1.2.1.4	*/

#include "lpsched.h"

static char cerrbuf[160] = "";	/* fix for bugid 1100252	*/


/**
 ** cancel() - CANCEL A REQUEST
 **/

int
cancel (RSTATUS *prs, int spool)
{
	if (prs->request->outcome & RS_DONE)
		return (0);

	prs->request->outcome |= RS_CANCELLED;

	if (spool || (prs->request->actions & ACT_NOTIFY))
		prs->request->outcome |= RS_NOTIFY;

	if (prs->request->outcome & RS_PRINTING) {
		terminate(prs->printer->exec);
	}
	else if (prs->request->outcome & RS_FILTERING) {
		terminate (prs->exec);
	}
	else if (prs->request->outcome | RS_NOTIFY) {
		/* start fix for bugid 1100252	*/
		if (prs->printer->status & PS_REMOTE) {
			sprintf(cerrbuf,
				"Remote status=%d, canceled by remote system\n",
				prs->reason);
		}
		notify (prs, cerrbuf, 0, 0, 0);
		cerrbuf[0] = (char) NULL;
		/* end fix for bugid 1100252	*/
	}
	check_request (prs);

	return (1);
}
