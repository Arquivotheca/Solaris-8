/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_getunum.c	1.6	96/09/24 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This service converts the given physical address into a text string,
 * representing the name of the field-replacable part for the given
 * physical address. In other words, it tells the kernel which chip got
 * the (un)correctable ECC error, which info is hopefully relayed to the user!
 */
int
prom_get_unum(int syn_code, unsigned long long physaddr, char *buf,
		u_int buflen, int *ustrlen)
{
	cell_t ci[12];
	int rv;
	ihandle_t imemory = prom_memory_ihandle();

	*ustrlen = -1;
	if ((imemory == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");		/* Service name */
	ci[1] = (cell_t)7;				/* #argument cells */
	ci[2] = (cell_t)2;				/* #result cells */
	ci[3] = p1275_ptr2cell("SUNW,get-unumber");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(imemory);		/* Arg2: mem. ihandle */
	ci[5] = p1275_uint2cell(buflen);		/* Arg3: buflen */
	ci[6] = p1275_ptr2cell(buf);			/* Arg4: buf */
	ci[7] = p1275_ull2cell_high(physaddr);		/* Arg5: physhi */
	ci[8] = p1275_ull2cell_low(physaddr);		/* Arg6: physlo */
	ci[9] = p1275_int2cell(syn_code);		/* Arg7: bit # */
	ci[10] = (cell_t)-1;				/* ret1: catch result */
	ci[11] = (cell_t)-1;				/* ret2: length */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[10]) != 0)	/* Res1: catch result */
		return (-1);			/* "SUNW,get-unumber" failed */
	*ustrlen = p1275_cell2uint(ci[11]);	/* Res2: unum str length */
	return (0);
}
