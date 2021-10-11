/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_outpath.c	1.7	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>

static char *stdoutpath;
static char path_name[OBP_MAXPATHLEN];

char *
prom_stdoutpath(void)
{
	dnode_t options;
	static char output_device[] = "output-device";

	if (stdoutpath != NULL)
		return (stdoutpath);

	options = prom_optionsnode();
	if ((options == OBP_NONODE) || (options == OBP_BADNODE))
		return (NULL);

	path_name[0] = (char)0;
	if (prom_getprop(options, output_device, path_name) <= 0)
		return (NULL);

	if (path_name[0] != '/')
		if (prom_getprop(prom_alias_node(), path_name, path_name) <= 0)
			return (NULL);

	stdoutpath = path_name;
	return (stdoutpath);
}
