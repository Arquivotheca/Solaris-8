/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)X500Trace.cc	1.1	96/03/31 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>	// vfprintf()
#include "X500Trace.hh"


/*
 * X.500 trace messages
 */


/*
 * Display the supplied message on stderr
 */
void
X500Trace::x500_trace(
#ifdef DEBUG
	char	*format,
	...
#else
	char *,
	...
#endif
) const
{
#ifdef DEBUG
	va_list	ap;
	va_start(ap, /* */);

	char		time_string[32];
	const time_t	time_value = time((time_t *)0);

	cftime(time_string, "%c", &time_value);
	(void) fprintf(stderr, "%s (fns x500) ", time_string);
	(void) vfprintf(stderr, format, ap);
	va_end(ap);
#endif
}
