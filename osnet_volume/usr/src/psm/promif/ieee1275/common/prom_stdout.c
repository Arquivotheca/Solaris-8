/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_stdout.c	1.3	95/02/15 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Return ihandle of stdout
 */
ihandle_t
prom_stdout_ihandle(void)
{
	static ihandle_t istdout;
	static char *name = "stdout";

	if (istdout)
		return (istdout);

	if (prom_getproplen(prom_chosennode(), name) !=
	    sizeof (ihandle_t))  {
		return (istdout = (ihandle_t)-1);
	}
	(void) prom_getprop(prom_chosennode(), name,
	    (caddr_t)(&istdout));
	istdout = prom_decode_int(istdout);
	return (istdout);

}

/*
 * Return phandle of stdout
 */
dnode_t
prom_stdout_node(void)
{
	static phandle_t pstdout;
	ihandle_t istdout;

	if (pstdout)
		return (pstdout);

	if ((istdout = prom_stdout_ihandle()) == (ihandle_t)-1)
		return (pstdout = (dnode_t)OBP_BADNODE);

	return (pstdout = prom_getphandle(istdout));
}
