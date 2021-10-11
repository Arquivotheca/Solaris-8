/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)done.c	1.8	95/10/05 SMI" 	/* SVr4.0 2.	*/

#include "mail.h"
#include <sysexits.h>

/*
 * remap the bin/mail exit code to sendmail recognizable
 * exit code when in deliver mode.
 */
static int
maperrno(err)
int err;
{
	int rc;

	switch (sav_errno) {
	case 0:
		rc = EX_OK;
		break;
	case EPERM:
	case EACCES:
	case ENOSPC:
        case EDQUOT:
		rc = EX_CANTCREAT;
		break;
	case EAGAIN:
		rc = EX_TEMPFAIL;
		break;
	case ENOENT:
	case EISDIR:
	case ENOTDIR:
		rc = EX_OSFILE;
		break;
	default:
		rc = EX_IOERR;
		break;
	}
	return(rc);
}

/* Fix for bug 1207994 */
void
sig_done(needtmp)
int 	needtmp;
{
	static char pn[] = "sig_done";
	Dout(pn, 0, "got signal %d\n", needtmp);
	done(0);
}


void done(needtmp)
int	needtmp;
{
	static char pn[] = "done";
	unlock();
	if (!maxerr) {
		maxerr = error;
		Dout(pn, 0, "maxerr set to %d\n", maxerr);
		if ((debug > 0) && (keepdbgfile == 0)) {
			unlink (dbgfname);
		}
	}
	if (maxerr && sending)
		mkdead();
	if (tmpf)
		fclose(tmpf);
	if (!needtmp && lettmp) {
		Dout(pn, 0, "temp file removed\n");
		unlink(lettmp);
	}

	if (deliverflag) {
		/* bug fix for 1104684 */
		exit(maperrno(sav_errno));
	}
	exit(maxerr);
	/* NOTREACHED */
}
