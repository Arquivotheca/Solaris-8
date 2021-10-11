/*
 * Copyright (c) 1991-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_macaddr.c	1.9	98/01/24 SMI"

/*
 * Return our machine address in the single argument. For the OBP cases,
 * simply call the appropriate mac_addr call.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/idprom.h>

int
prom_getmacaddr(ihandle_t hd, caddr_t ea)
{
	idprom_t idprom;

	switch (obp_romvec_version) {

	/*
	 *  If any of the methods below fail, we just resort to reading
	 *  the idprom.
	 */

	case OBP_V0_ROMVEC_VERSION:
		promif_preprom();
		OBP_V0_MAC_ADDRESS(hd, ea);  /* no error return defined here */
		promif_postprom();
		return (0);

	default: {
			dnode_t macnodeid;
			macnodeid = prom_getphandle(hd);
			if (macnodeid == OBP_BADNODE)
				break;
			if (prom_getproplen(macnodeid, OBP_MAC_ADDR) == -1)
				break;
			(void) prom_getprop(macnodeid, OBP_MAC_ADDR, ea);
			return (0);
		}
	}

	/*
	 * Extract it from the root node idprom property
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
