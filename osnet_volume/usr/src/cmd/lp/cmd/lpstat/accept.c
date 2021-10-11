/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)accept.c	1.7	93/08/11 SMI"	/* SVr4.0 1.6	*/

#include <locale.h>
#include "stdio.h"

#include "lp.h"
#include "class.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"

/**
 ** do_accept()
 **/

void
#if	defined(__STDC__)
do_accept (
	char **			list
)
#else
do_accept (list)
	char			**list;
#endif
{
	while (*list) {
		if (STREQU(*list, NAME_ALL)) {
			send_message (S_INQUIRE_CLASS, "");
			(void)output (R_INQUIRE_CLASS);
			send_message (S_INQUIRE_PRINTER_STATUS, "");
			(void)output (R_INQUIRE_PRINTER_STATUS);

		} else if (isclass(*list)) {
			send_message (S_INQUIRE_CLASS, *list);
			(void)output (R_INQUIRE_CLASS);

		} else {
			send_message (S_INQUIRE_PRINTER_STATUS, *list);
			switch (output(R_INQUIRE_PRINTER_STATUS)) {
			case MNODEST:
				LP_ERRMSG1 (ERROR, E_LP_BADDEST, *list);
				exit_rc = 1;
				break;
			}

		}
		list++;
	}
	return;
}

/**
 ** putqline()
 **/

void
putqline(char *dest, int rejecting, time_t date, char *reject_reason)
{
	char	reject_date[SZ_DATE_BUFF];

	cftime(reject_date, NULL, &date);

	if (!rejecting)
		(void) printf(gettext("%s accepting requests since %s\n"),
			dest, reject_date);
	else
		(void) printf(
			gettext("%s not accepting requests since %s -\n\t%s\n"),
			dest, reject_date, reject_reason);
	return;
}
