/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_outpath.c	1.24	94/11/16 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

static char *stdoutpath;
static char buffer[OBP_MAXPATHLEN];

char *
prom_stdoutpath(void)
{
	ihandle_t	istdout;

	if (stdoutpath != (char *)0)
		return (stdoutpath);

	istdout = prom_stdout_ihandle();

	if (istdout != (ihandle_t)-1)
		if (prom_ihandle_to_path(istdout, buffer,
		    OBP_MAXPATHLEN - 1) > 0)
			return (stdoutpath = buffer);
	return ((char *)0);
}
