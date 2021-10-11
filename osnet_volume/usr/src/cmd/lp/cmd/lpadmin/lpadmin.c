/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lpadmin.c	1.6	92/07/14 SMI"	/* SVr4.0 1.12	*/

#include "stdio.h"
#include "ctype.h"
#include "errno.h"
#include "sys/types.h"
#include "sys/utsname.h"
#include "string.h"

#include "lp.h"
#include "msgs.h"
#include "access.h"
#include "class.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPADMIN
#include "oam.h"

#include "lpadmin.h"

#include <locale.h>

extern void		chkopts(),
			chkopts2(),
			chkopts3(),
			exit();

int			scheduler_active = 0;

char			*label = 0;

PRINTER			*printer_pointer = 0;

static CLASS		*class_pointer = 0;

PWHEEL			*pwheel_pointer	= 0;
char			*Local_System = 0;

/**
 ** main()
 **/

int			main (argc, argv)
	int			argc;
	char			*argv[];
{
	struct utsname	un;

	(void) setlocale (LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (!is_user_admin()) {
		LP_ERRMSG (ERROR, E_LP_NOTADM);
		done (1);
		/*NOTREACHED*/
	}

	uname(&un);
	Local_System = strdup(un.nodename);
    
	options (argc, argv);	/* process command line options */

	chkopts ();		/* check for legality of options */

	startup ();		/* open path to Spooler */

	if (d)
		newdflt (d);	/* -d */

	else if (x) {

		/* allow "any" or "all" to do all destinations */
		if (STREQU(NAME_ALL, x) || STREQU(NAME_ANY, x)) {

			/*
			 * Just deleting all the printers should get
			 * rid of all the classes, too, but removing
			 * the classes first should make things go a bit
			 * faster.
			 */

			while ((class_pointer = getclass(NAME_ALL)))
				rmdest (1, class_pointer->name);

			if (errno != ENOENT) {
				LP_ERRMSG1 (
					ERROR,
					E_ADM_GETCLASSES,
					PERROR
				);
				done (1);
				/*NOTREACHED*/
			}

			while ((printer_pointer = getprinter(NAME_ALL)))
				rmdest (0, printer_pointer->name);

			if (errno != ENOENT) {
				LP_ERRMSG1 (
					ERROR,
					E_ADM_GETPRINTERS,
					PERROR
				);
				done (1);
				/*NOTREACHED*/
			}

		} else 
			rmdest (isclass(x), x);

	} else if (!p && S) {
		if (STREQU(*S, NAME_ALL) || STREQU(*S, NAME_ANY)) {
			while ((pwheel_pointer = getpwheel(NAME_ALL))) {
				*S = pwheel_pointer->name;
				chkopts3 (0);
				label = *S;
				do_pwheel ();
			}
		} else {
			label = 0;
			do_pwheel ();
		}

#if	defined(J_OPTION)
	} else if (j) {
		do_fault ();	/* -j */
#endif

	} else {
		/* allow "any" or "all" to do all printers */
		if (STREQU(NAME_ALL, p) || STREQU(NAME_ANY, p)) {
			int called=0;
			while ((printer_pointer = getprinter(NAME_ALL)) != NULL) {
				/*
				 * "chkopts2()" will clobber "s".
				 */
				char *		save_s = s;

				called++;
				p = printer_pointer->name;
				chkopts2 (0);

				if (s)
					if (
						A || a || e || F || H
					     || h || i || l || m || M
					     || o || U || v
					     || Q != -1 || W != -1
					)
						LP_ERRMSG1 (
							WARNING,
							E_ADM_SIGNORE,
							p
						);
				label = p;
				do_printer ();

				s = save_s;
			}
			if (called == 0 )
				LP_ERRMSG (ERROR, E_ADM_PLONELY);

			if (errno != ENOENT) {
				LP_ERRMSG2 (
					ERROR,
					E_LP_GETPRINTER,
					NAME_ALL,
					PERROR
				);
				done (1);
				/*NOTREACHED*/
			}
		} else {
			label = 0;
			do_printer ();	/* -p etc. */
		}
	}
	done (0);
	/*NOTREACHED*/
}

/**
 ** putp() - FAKE ROUTINES TO AVOID REAL ONES
 **/

int			putp ()
{
	return (0);
}
