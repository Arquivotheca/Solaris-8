/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * version.c -- output the version property
 */

#ident	"<@(#)version.c	1.4	97/03/21 SMI>"

#include <stdio.h>

#include "types.h"
#include "bop.h"
#include "version.h"

void
set_prop_version(void)
{
	char buf[80];

	out_bop("dev /chosen\n");
	(void) sprintf(buf, "setprop devconf-version \"DevConf %d.%d\"\n",
	    MAJOR_VERSION, MINOR_VERSION);
	out_bop(buf);
}
