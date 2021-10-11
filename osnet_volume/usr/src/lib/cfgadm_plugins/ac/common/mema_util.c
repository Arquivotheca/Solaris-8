/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mema_util.c	1.3	98/05/18 SMI"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <sys/param.h>
#include <config_admin.h>
#include "mema_util.h"

/*
 * The libmemadm routines can return arbitrary error strings.  As the
 * calling program does not know how long these errors might be,
 * the library routines must allocate the required space and the
 * calling program must deallocate it.
 *
 * This routine povides a printf-like interface for creating the
 * error strings.
 */

#define	FMT_STR_SLOP		(16)

void
__fmt_errstring(
	char **errstring,
	size_t extra_length_hint,
	const char *fmt,
	...)
{
	char *ebuf;
	size_t elen;
	va_list ap;

	/*
	 * If no errors required or error already set, return.
	 */
	if ((errstring == NULL) || (*errstring != NULL))
		return;

	elen = strlen(fmt) + extra_length_hint + FMT_STR_SLOP;

	if ((ebuf = (char *)malloc(elen + 1)) == NULL)
		return;

	va_start(ap, /* null */);
	(void) vsprintf(ebuf, fmt, ap);
	va_end(ap);

	if (strlen(ebuf) > elen)
		abort();

	*errstring = ebuf;
}
