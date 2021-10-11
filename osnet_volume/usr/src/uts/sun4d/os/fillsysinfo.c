/*
 * Copyright (c) 1987-1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)fillsysinfo.c	1.59	95/01/25 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/cmn_err.h>

/*
 * The OpenBoot Standalone Interface supplies the kernel with
 * implementation dependent parameters through the devinfo/property mechanism
 */

int debug_fillsysinfo = 0;
#define	VPRINTF		if (debug_fillsysinfo) printf

/*
 * property names
 */
char psdevid[]		= "device-id";
char psname[]		= "name";
char psdevtype[]	= "device_type";	/* yes, underscore. */
char psrange[]		= "range";
char psreg[]		= "reg";

/*
 * sun4d device-id's
 */
int
get_deviceid(int nodeid, int parent)
{
	int device_id;

	if (prom_getprop((dnode_t)nodeid, psdevid, (caddr_t)&device_id) == -1) {
		if (prom_getprop((dnode_t)parent, psdevid,
		    (caddr_t)&device_id) == -1) {
			VPRINTF("get_deviceid: BOGUS\n");
			return (0);	/* BOGUS */
		}
	}

	VPRINTF("get_deviceid: nodeid(0x%x) device_id=0x%x\n",
		nodeid, device_id);
	return (device_id);
}
