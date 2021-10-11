/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_stdin.c	1.4	95/02/15 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Return ihandle of stdin
 */
ihandle_t
prom_stdin_ihandle(void)
{
	static ihandle_t istdin;
	static char *name = "stdin";

	if (istdin)
		return (istdin);

	if (prom_getproplen(prom_chosennode(), name) !=
	    sizeof (ihandle_t))  {
#if defined(PROMIF_DEBUG) || defined(lint)
		prom_fatal_error("No stdout ihandle?");
#endif
		return (istdin = (ihandle_t)-1);
	}
	(void) prom_getprop(prom_chosennode(), name,
	    (caddr_t)(&istdin));
	istdin = prom_decode_int(istdin);
	return (istdin);
}

/*
 * Return phandle of stdin
 */
dnode_t
prom_stdin_node(void)
{
	static phandle_t pstdin;
	ihandle_t istdin;

	if (pstdin)
		return (pstdin);

	if ((istdin = prom_stdin_ihandle()) == (ihandle_t)-1)
		return (pstdin = (dnode_t)OBP_BADNODE);

	return (pstdin = prom_getphandle(istdin));
}
