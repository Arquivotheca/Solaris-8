/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)okprinter.c	1.6	97/05/14 SMI"	/* SVr4.0 1.19	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "sys/types.h"
#include "stdlib.h"
#include <unistd.h>

#include "lp.h"
#include "printers.h"

unsigned long		badprinter	= 0;

static int		okinterface ( char * , PRINTER * );

/**
 ** okprinter() - SEE IF PRINTER STRUCTURE IS SOUND
 **/

int
okprinter(char *name, PRINTER *prbufp, int isput)
{
	badprinter = 0;

	/*
	 * A printer can't be remote and have device, interface,
	 * fault recovery, or alerts.
	 */
	if (
		prbufp->remote
	     && (
			prbufp->device
		     || prbufp->interface
		     || (
				prbufp->fault_alert.shcmd
			     && !STREQU(NAME_NONE, prbufp->fault_alert.shcmd)
			)
#if	defined(CAN_DO_MODULES)
# if	defined(FIXED)
/*
 * This needs some work...getprinter() initializes this to "default"
 */
		     || (
				!emptylist(prbufp->modules)
			     && !STREQU(NAME_NONE, prbufp->modules[0])
			)
# endif
#endif
		)
	)
		badprinter |= BAD_REMOTE;

	/*
	 * A local printer must have an interface program. This is
	 * for historical purposes (it let's someone know where the
	 * interface program came from) AND is used by "putprinter()"
	 * to copy the interface program. We must be able to read it.
	 */
	if (!prbufp->remote && isput && !okinterface(name, prbufp))
		badprinter |= BAD_INTERFACE;

	/*
	 * A local printer must have device or dial info.
	 */
	if (!prbufp->remote && !prbufp->device && !prbufp->dial_info)
		badprinter |= BAD_DEVDIAL;

	/*
	 * Fault recovery must be one of three kinds
	 * (or default).
	 */
	if (
		prbufp->fault_rec
	     && !STREQU(prbufp->fault_rec, NAME_CONTINUE)
	     && !STREQU(prbufp->fault_rec, NAME_BEGINNING)
	     && !STREQU(prbufp->fault_rec, NAME_WAIT)
	)
		badprinter |= BAD_FAULT;

	/*
	 * Alert command can't be reserved word.
	 */
	if (
	     	prbufp->fault_alert.shcmd
	     && (
		STREQU(prbufp->fault_alert.shcmd, NAME_QUIET)
	     || STREQU(prbufp->fault_alert.shcmd, NAME_LIST)
		)
	)
		badprinter |= BAD_ALERT;

	return ((badprinter & ~ignprinter)? 0 : 1);
}

/**
 ** okinterface() - CHECK THAT THE INTERFACE PROGRAM IS OKAY
 **/

static int
canread(char *path)
{
	return ((access(path, R_OK) < 0) ? 0 : 1);
}

static int
okinterface(char *name, PRINTER *prbufp)
{
	int			ret;

	register char		*path;


	if (prbufp->interface)
		ret = canread(prbufp->interface);

	else
		if (!(path = makepath(Lp_A_Interfaces, name, (char *)0)))
			ret = 0;
		else {
			ret = canread(path);
			Free (path);
		}

	return (ret);
}
