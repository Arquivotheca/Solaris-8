/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_prop.c	1.10	95/08/15 SMI"

/*
 * Stuff for mucking about with properties
 *
 * XXX: There is no distinction between intefer and non-integer properties
 * XXX: and no functions included for decoding properties.  As is, this
 * XXX: file is suitable for a big-endian machine, since properties are
 * XXX: encoded using an XDR-like property encoding mechanism, which is
 * XXX: big-endian native ordering.  To fix this, you need to add type-
 * XXX: sensitive decoding mechanisms and have the consumer of the data
 * XXX: decode the data, since only the consumer can claim to know the
 * XXX: the type of the data. (It can't be done automatically.)
 */

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_getproplen(dnode_t nodeid, caddr_t name)
{
	cell_t ci[6];

	ci[0] = p1275_ptr2cell("getproplen");	/* Service name */
	ci[1] = (cell_t)2;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #return cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid);	/* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: Property name */
	ci[5] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[5]));		/* Res1: Property length */
}


int
prom_getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	int len, rv;
	cell_t ci[8];

	/*
	 * This function assumes the buffer is large enough to
	 * hold the result, so in 1275 mode, we pass in the length
	 * of the property as the length of the buffer, since we
	 * have no way of knowing the size of the buffer. Pre-1275
	 * OpenBoot(tm) PROMs did not have a bounded getprop.
	 *
	 * Note that we ignore the "length" result of the service.
	 */

	if ((len = prom_getproplen(nodeid, name)) <= 0)
		return (len);

	ci[0] = p1275_ptr2cell("getprop");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid);	/* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: property name */
	ci[5] = p1275_ptr2cell(value);		/* Arg3: buffer address */
	ci[6] = len;				/* Arg4: buf len (assumed) */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (-1);
	return (len);				/* Return known length */
}

int
prom_bounded_getprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	cell_t ci[8];

	ci[0] = p1275_ptr2cell("getprop");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid); /* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: property name */
	ci[5] = p1275_ptr2cell(value);		/* Arg3: buffer address */
	ci[6] = p1275_int2cell(len);		/* Arg4: buffer length */
	ci[7] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[7]));		/* Res1: Returned length */
}

caddr_t
prom_nextprop(dnode_t nodeid, caddr_t previous, caddr_t next)
{
	cell_t ci[7];

	(void) prom_strcpy(next, "");	/* Prime result, in case call fails */

	ci[0] = p1275_ptr2cell("nextprop");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid); /* Arg1: phandle */
	ci[4] = p1275_ptr2cell(previous);	/* Arg2: addr of prev name */
	ci[5] = p1275_ptr2cell(next);		/* Arg3: addr of 32 byte buf */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (next);
}

int
prom_setprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	cell_t ci[8];

	ci[0] = p1275_ptr2cell("setprop");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid);	/* Arg1: phandle */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: property name */
	ci[5] = p1275_ptr2cell(value);		/* Arg3: New value ptr */
	ci[6] = p1275_int2cell(len);		/* Arg4: New value len */
	ci[7] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[7]));		/* Res1: Actual new size */
}

/*
 * prom_decode_composite_string:
 *
 * Returns successive strings in a composite string property.
 * A composite string property is a buffer containing one or more
 * NULL terminated strings contained within the length of the buffer.
 *
 * Always call with the base address and length of the property buffer.
 * On the first call, call with prev == 0, call successively
 * with prev == to the last value returned from this function
 * until the routine returns zero which means no more string values.
 */
char *
prom_decode_composite_string(void *buf, size_t buflen, char *prev)
{
	if ((buf == 0) || (buflen == 0) || ((int)buflen == -1))
		return ((char *)0);

	if (prev == 0)
		return ((char *)buf);

	prev += prom_strlen(prev) + 1;
	if (prev >= ((char *)buf + buflen))
		return ((char *)0);
	return (prev);
}
