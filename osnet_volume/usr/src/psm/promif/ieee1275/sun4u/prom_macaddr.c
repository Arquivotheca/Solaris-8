/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_macaddr.c	1.12	96/03/17 SMI"

/*
 * Return our machine address in the single argument.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/idprom.h>

/*ARGSUSED*/
int
prom_getmacaddr(ihandle_t hd, caddr_t ea)
{
	idprom_t idprom;
	dnode_t macnodeid;

	/*
	 * Look for the 'mac-address' property in the device node
	 * associated with the ihandle 'hd'. This handles the
	 * cases when we booted from the network device, using either
	 * the platform mac address or a local mac address. This
	 * code will always return whichever mac-address was used by the
	 * firmware (local or platform, depending on nvram settings).
	 */
	macnodeid = prom_getphandle(hd);
	if (macnodeid != OBP_BADNODE) {
		if (prom_getproplen(macnodeid, OBP_MAC_ADDR) != -1) {
			(void) prom_getprop(macnodeid, OBP_MAC_ADDR, ea);
			return (0);
		}
	}

	/*
	 * The code above, should have taken care of the case
	 * when we booted from the device ... otherwise, as a fallback
	 * case, return the system mac address from the idprom.
	 * This code (idprom) is SMCC (and compatibles) platform-centric.
	 * This code always returns the platform mac address.
	 */
	if (prom_getidprom((caddr_t) &idprom, sizeof (idprom)) == 0) {
		register char *f = (char *) idprom.id_ether;
		register char *t = ea;
		int i;

		for (i = 0; i < sizeof (idprom.id_ether); ++i)
			*t++ = *f++;

		return (0);
	} else
		return (-1); /* our world must be starting to explode */
}
