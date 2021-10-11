/*
 * Copyright (c) 1994,1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_prop.c	1.7	99/06/06 SMI"

/*
 * Stuff for mucking about with properties
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)

int
prom_getproplen(dnode_t nodeid, caddr_t name)
{
	return (promif_getproplen(nodeid, name));
}

int
prom_getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	return (promif_getprop(nodeid, name, value));
}

caddr_t
prom_nextprop(dnode_t nodeid, caddr_t previous, caddr_t next)
{
	return (promif_nextprop(nodeid, previous, next));
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

int
prom_bounded_getprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	return (promif_bounded_getprop(nodeid, name, value, len));
}
#endif
