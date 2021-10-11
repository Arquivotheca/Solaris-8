#ident	"@(#)misc.c	1.16	99/08/27	SMI"

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <varargs.h>
#include <syslog.h>
#include <locale.h>

/*
 * Error message function. If debugging off, then logging goes to
 * syslog.
 *
 * Must be MT SAFE - called by various threads as well as the main thread.
 */

extern int debug;
extern int errno;

/* VARARGS */
void
dhcpmsg(va_alist)
	va_dcl
{
	va_list		ap;
	int		errlevel;
	char		*fmtp, buff[BUFSIZ], errbuf[BUFSIZ];

	va_start(ap);

	errlevel = (int) va_arg(ap, int);
	fmtp = (char *) va_arg(ap, char *);

	if (debug)  {
		if (errlevel != LOG_ERR)
			*errbuf = '\0';
		else
			(void) sprintf(errbuf, "(%s)", strerror(errno));
		(void) sprintf(buff, "%s %s", errbuf, gettext(fmtp));
		(void) vfprintf(stderr, buff, ap);
	} else
		(void) vsyslog(errlevel, gettext(fmtp), ap);

	va_end(ap);
}
