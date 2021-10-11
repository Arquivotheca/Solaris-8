/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_version.c	1.7	94/11/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_getversion(void)
{
	/*
	 * Returns romvec version number, or a large number, if IEEE 1275.
	 */
	return (obp_romvec_version);
}

int
prom_is_openprom(void)
{
	/*
	 * Returns true if OpenBoot(tm) or IEEE 1275 interface exists.
	 */
	return (1);
}

int
prom_is_p1275(void)
{
	/*
	 * Returns true, if IEEE 1275 interface exists.
	 */
	return (0);
}

/*
 * Certain standalones need to get the revision of a certain
 * version of the firmware to be able to figure out how to
 * correct for certain errors in certain versions of the PROM.
 *
 * The caller has to know what to do with the cookie returned
 * from prom_mon_id(). c.f. uts/sun4c/io/autoconf.c:impl_fix_props().
 */
void *
prom_mon_id(void)
{
	return ((void *)romp->obp.op_mon_id);
}
