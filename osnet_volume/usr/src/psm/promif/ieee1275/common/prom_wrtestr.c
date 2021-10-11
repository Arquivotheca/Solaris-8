/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_wrtestr.c	1.10	97/06/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Write string to PROM's notion of stdout.
 */
void
prom_writestr(const char *buf, size_t len)
{
	size_t written = 0;
	ihandle_t istdin;
	size_t i;

	istdin = prom_stdout_ihandle();
	while (written < len)  {
		if ((i = prom_write(istdin, (char *)buf,
		    len - written, 0, BYTE)) == -1)
			continue;
		written += i;
	}
}
