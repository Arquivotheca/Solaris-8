/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_inpath.c	1.11	95/08/15 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

static char *stdinpath;
static char buffer[OBP_MAXPATHLEN];

static char *obp_v0_stdinpaths[] = {
	"/zs@1,f0000000:a",		/* INKEYB:   kbd  */
	"/zs@1,f1000000:a",		/* INUARTA:  ttya */
	"/zs@1,f1000000:b",		/* INUARTB:  ttyb */
	(char *)0,			/* INUARTC:  ttyc */
	(char *)0			/* INUARTD:  ttyc */
};

char *
prom_stdinpath(void)
{
	register int	index;

	if (stdinpath != (char *) 0)		/* Got it already? */
		return (stdinpath);

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
		index = (OBP_V0_INSOURCE);
		return (stdinpath = obp_v0_stdinpaths[index]);

	default:
		if (prom_getprop(prom_rootnode(), OBP_STDINPATH,
		    buffer) != -1)  {
			prom_pathname(buffer);	/* canonical processing */
			return (stdinpath = buffer);
		}
		/*
		 * For OBP_V2, we can still use INSOURCE, if we have to.
		 * (Later OBP_V2 PROMS have the root node property;
		 * earlier ones do not.)
		 */

		if (obp_romvec_version == OBP_V2_ROMVEC_VERSION)  {
			index = (OBP_V0_INSOURCE);
			return (stdinpath = obp_v0_stdinpaths[index]);
		}
		return ((char *)0);
	}
}
