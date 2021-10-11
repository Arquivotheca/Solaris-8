/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_test.c	1.7	95/07/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Test for existance of a specific P1275 client interface service
 */
int
prom_test(char *service)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("test");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(service);	/* Arg1: requested svc name */
	ci[4] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[4]));		/* Res1: missing flag */
}

int
prom_test_method(char *method, dnode_t node)
{
	cell_t ci[6];
	int rv;
	char buf[80];

	if (prom_test("test-method") == 0) {
		ci[0] = p1275_ptr2cell("test-method");	/* service */
		ci[1] = (cell_t)2;			/* #argument cells */
		ci[2] = (cell_t)1;			/* #result cells */
		ci[3] = p1275_dnode2cell(node);
		ci[4] = p1275_ptr2cell(method);
		ci[5] = (cell_t)-1;

		promif_preprom();
		(void) p1275_cif_handler(&ci);
		promif_postprom();
		rv = p1275_cell2int(ci[5]);
	} else {
		(void) prom_sprintf(buf,
		    "\" %s\" h# %x find-method invert h# %x l!",
		    method, node, &rv);
		prom_interpret(buf, 0, 0, 0, 0, 0);
	}
	return (rv);
}
