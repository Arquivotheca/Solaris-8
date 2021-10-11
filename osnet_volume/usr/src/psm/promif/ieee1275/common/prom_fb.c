/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_fb.c	1.12	95/03/02 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_stdout_is_framebuffer(void)
{
	static int remember = -1;

	if (remember == -1)
		remember = prom_devicetype((dnode_t) prom_stdout_node(),
			OBP_DISPLAY);
	return (remember);
}
