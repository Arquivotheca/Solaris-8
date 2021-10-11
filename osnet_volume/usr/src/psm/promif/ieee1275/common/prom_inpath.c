/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_inpath.c	1.13	95/01/19 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

static char *stdinpath;
static char buffer[OBP_MAXPATHLEN];

char *
prom_stdinpath(void)
{
	ihandle_t	istdin;

	if (stdinpath != (char *) 0)		/* Got it already? */
		return (stdinpath);

	istdin = prom_stdin_ihandle();

	if (istdin != (ihandle_t)-1)
		if (prom_ihandle_to_path(istdin, buffer,
		    OBP_MAXPATHLEN - 1) > 0)
			return (stdinpath = buffer);
	return ((char *)0);
}
