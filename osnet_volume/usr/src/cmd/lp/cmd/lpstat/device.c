/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)device.c	1.6	92/07/14 SMI"	/* SVr4.0 1.9	*/

#include <locale.h>
#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


#if	defined(__STDC__)
static void		putdline ( PRINTER * );
#else
static void		putdline();
#endif

/**
 ** do_device()
 **/

void
#if	defined(__STDC__)
do_device (
	char **			list
)
#else
do_device (list)
	char			**list;
#endif
{
	register PRINTER	*pp;


	while (*list) {
		if (STREQU(NAME_ALL, *list))
			while ((pp = getprinter(NAME_ALL)))
				putdline (pp);

		else if ((pp = getprinter(*list)))
			putdline (pp);

		else {
			LP_ERRMSG1 (ERROR, E_LP_NOPRINTER, *list);
			exit_rc = 1;
		}

		list++;
	}
	return;
}

/**
 ** putdline()
 **/

static void
#if	defined(__STDC__)
putdline (
	PRINTER *		pp
)
#else
putdline (pp)
	register PRINTER	*pp;
#endif
{
	if (!pp->device && !pp->dial_info && !pp->remote) {
		LP_ERRMSG1 (ERROR, E_LP_PGONE, pp->name);

	} else if (pp->remote) {
		char *			cp = strchr(pp->remote, BANG_C);


		if (cp)
			*cp++ = 0;
		(void)printf (gettext("system for %s: %s"), pp->name, pp->remote);
		if (cp)
			(void)printf (gettext(" (as printer %s)"), cp);
		(void)printf ("\n");

	} else if (pp->dial_info) {
		(void)printf (gettext("dial token for %s: %s"), pp->name, pp->dial_info);
		if (pp->device)
			(void)printf (gettext(" (on port %s)"), pp->device);
		(void)printf ("\n");

	} else {
		(void)printf (gettext("device for %s: %s"), pp->name, pp->device);
#if	defined(CAN_DO_MODULES)
		if (verbosity & V_MODULES)
			if (
				emptylist(pp->modules)
			     || STREQU(NAME_NONE, pp->modules[0])
			)
				(void)printf (gettext(" (no modules)"));
			else if (STREQU(NAME_KEEP, pp->modules[0]))
				(void)printf (gettext(" (keep startup modules)"));
			else if (STREQU(NAME_DEFAULT, pp->modules[0]))
				(void)printf (gettext(" %s (default)"), DEFMODULES);
			else {
				(void)printf (" ");
				printlist_setup ("", 0, ",", "");
				printlist (stdout, pp->modules);
				printlist_unsetup ();
			}
#endif
		(void)printf ("\n");
	}

	return;
}
