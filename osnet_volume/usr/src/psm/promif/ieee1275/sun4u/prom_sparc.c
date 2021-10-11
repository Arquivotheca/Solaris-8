/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_sparc.c	1.5	95/01/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * P1275 Client Interface Functions defined for SPARC.
 * 32 bit client, 32 bit PROM.
 * This file belongs in a platform dependent area.
 */

/*
 * This function returns NULL or a a verified client interface structure
 * pointer to the caller.
 */

int (*cif_handler)(void *);

void *
p1275_sparc_cif_init(void *cookie)
{
	cif_handler = (int (*)(void *))cookie;
	return ((void *)cookie);
}


/*
 * NB: This code is appropriate for 32 bit client programs calling the
 * 64 bit cell-sized client interface handler.  On SPARC V9 machines,
 * the client program must manage the conversion of the 32 bit stack
 * to a 64 bit stack itself. Thus, the client program must provide
 * this function. (client_handler).
 */

int
p1275_sparc_cif_handler(void *p)
{
	int rv;

	if (cif_handler == NULL)
		return (-1);

	rv = client_handler((void *)cif_handler, p);
	return (rv);
}
