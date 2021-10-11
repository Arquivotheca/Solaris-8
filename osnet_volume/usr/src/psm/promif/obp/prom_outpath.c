/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_outpath.c	1.27	99/10/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

static char *stdoutpath;
static char buffer[OBP_MAXPATHLEN];

/*
 * Note that this function generates the type of pathnames
 * that the kernel wants to see. (normalized, relative addresses).
 */

static char *obp_v0_stdoutpaths[] = {
	(char *)0,			/* OUTSCREEN: (Figure it out) */
	"/zs@1,f1000000:a",		/* OUTUARTA:  ttya */
	"/zs@1,f1000000:b",		/* OUTUARTB:  ttyb */
	(char *)0,			/* OUTUARTC:  ttyc */
	(char *)0			/* OUTUARTD:  ttyc */
};

char *
prom_stdoutpath(void)
{
	register int	index;
	dnode_t fbid;

	if (stdoutpath != (char *)0)
		return (stdoutpath);

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
		break;

	default:
		if (prom_getprop(prom_rootnode(), OBP_STDOUTPATH,
		    buffer) != -1)  {
			prom_pathname(buffer);	/* cannonical processing */
			return (stdoutpath = buffer);
		}

		/*
		 * For OBP_V2, we can still use OUTSINK, if we have to.
		 * (Later OBP_V2 PROMS have the root node property;
		 * earlier ones do not.)
		 */

		if (obp_romvec_version != OBP_V2_ROMVEC_VERSION)  {
			prom_printf("prom_stdoutpath: Missing property!\n");
			return ((char *)0);	/* XXX: Can't figure it out? */
		}
		break;
	}

	/*
	 * OBP_V0 and early OBP_V2 case...
	 */

	index = (OBP_V0_OUTSINK);
	if (index != OUTSCREEN)  {
		return (stdoutpath = obp_v0_stdoutpaths[index]);
	}

	/*
	 * Now we are faced with the really hard case, for a promif function.
	 * We can get the nodeid of the framebuffer device from the "fb"
	 * root property, then we have to cons a pathname for this device,
	 * which is the difficult part, because we have to follow the parent
	 * pointers without having a function that gives me the parent of
	 * a nodeid in the prom.  We also assume that we can use the "reg"
	 * paradigm for creating the pathname.
	 */

	if (prom_getprop(prom_rootnode(), "fb", (char *)(&fbid)) == -1)  {
		prom_printf("prom_stdoutpath: Unknown frame buffer device!\n");
		return ((char *)0);
	}

	*buffer = '\0';
	prom_dnode_to_pathname(fbid, buffer);
	return (stdoutpath = buffer);
}

/*
 * XXX: The following function can handle devices
 * XXX: with at most NREGSPECS sets of prom-defined registers.
 */

#define	NREGSPECS	16
static struct prom_reg regs[NREGSPECS];
static char namebuf[OBP_MAXPATHLEN];

void
prom_dnode_to_pathname(register dnode_t nodeid, register char *outbuf)
{
	dnode_t parent;
	register char *p;
	int regsize;

	if ((parent = prom_parentnode(nodeid)) != OBP_NONODE)
		prom_dnode_to_pathname(parent, outbuf);

	if (nodeid == prom_rootnode())  {
		(void) prom_strcat(outbuf, "/");
		return;		/* Root node's name is just "/" */
	}

	if (prom_getprop(nodeid, OBP_NAME, namebuf) == -1)
		return;		/* XXX */

	if (prom_strlen(outbuf) != 1)	/* Avoid two leading slashes */
		(void) prom_strcat(outbuf, "/");
	(void) prom_strcat(outbuf, namebuf);

	if ((regsize = prom_getproplen(nodeid, OBP_REG)) == -1)
		return;
	if (regsize == 0)
		return;
	if (regsize > sizeof (regs))  {
		prom_printf("prom_stdoutpath: too many regs in %s\n", namebuf);
		return;
	}
	(void) prom_getprop(nodeid, OBP_REG, (caddr_t)&regs);
	for (p = outbuf; *p; ++p)
		;

	(void) prom_sprintf(p, "@%x,%x", regs[0].hi, regs[0].lo);
}
