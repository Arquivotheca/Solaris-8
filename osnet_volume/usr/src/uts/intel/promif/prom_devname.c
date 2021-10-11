/*
 * Copyright (c) 1991,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_devname.c	1.8	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_devname_from_pathname(register char *pathname, register char *buffer)
{
	register char *p;

	if ((pathname == (char *)0) || (*pathname == (char)0))
		return (-1);

	p = prom_strrchr(pathname, '/');
	if (p == 0)
		return (-1);

	p++;
	while (*p != 0)  {
		*buffer++ = *p++;
		/*
		 * XXX Compiled differently for kadb?
		 */
#ifdef KADB
		if ((*p == '@') || (*p == '/'))
#else
		if ((*p == '@') || (*p == ':'))
#endif
			break;
	}
	*buffer = (char)0;

	return (0);
}
