/*
 * Copyright (c) 1992,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_macaddr.c	1.8	99/05/04 SMI"

/*
 * Return our machine address in the single argument.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootlink.h>

typedef unsigned char ether_addr_t[6];

extern struct int_pb ic;

/*
 * Pass the macaddr in the parameter area pointed to by ea,
 * and return success of fail.
 */
int
prom_getmacaddr(int hd, ether_addr_t *ea)
{

	return (i86_get_macaddr(hd, ea));
}

#define	GLUE_INT 	0xfb
#define	GET_MACADDR	0x05
int
i86_get_macaddr(int hd, unsigned char *ea)
{
	/*
	 * XXX - we should be able to find macaddr as a property in the
	 * tree somewhere now.
	 */

	ic.intval = GLUE_INT;
	ic.ax = GET_MACADDR << 8 + 0;
	(void) doint();
	/* the address is passed in bx:cx:dx */
	ea[0] = (unsigned char)(ic.bx / 256);
	ea[1] = (unsigned char)(ic.bx % 256);
	ea[2] = (unsigned char)(ic.cx / 256);
	ea[3] = (unsigned char)(ic.cx % 256);
	ea[4] = (unsigned char)(ic.dx / 256);
	ea[5] = (unsigned char)(ic.dx % 256);
	return (0);
}
