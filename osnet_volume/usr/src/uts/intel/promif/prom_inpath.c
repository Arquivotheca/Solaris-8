/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_inpath.c	1.7	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>

static char *stdinpath;
static char path_name[OBP_MAXPATHLEN];

char *
prom_stdinpath(void)
{
	dnode_t options;
	static char input_device[] = "input-device";

	if (stdinpath != NULL)
		return (stdinpath);

	options = prom_optionsnode();
	if ((options == OBP_NONODE) || (options == OBP_BADNODE))
		return (NULL);

	path_name[0] = (char)0;
	if (prom_getprop(options, input_device, path_name) <= 0)
		return (NULL);

	if (path_name[0] != '/')
		if (prom_getprop(prom_alias_node(), path_name, path_name) <= 0)
			return (NULL);

	stdinpath = path_name;
	return (stdinpath);
}
