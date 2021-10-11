/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_init.c	1.22	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int	obp_romvec_version = -1;  /* -1 rsrvd for non-obp sunromvec */
int	prom_aligned_allocator = 0; /* shall we use mmu-aligned alloc's? */

#ifdef PROMIF_DEBUG
int promif_debug = 0;
#endif /* PROMIF_DEBUG */

/*
 * This is the string we use to print out "panic" level messages,
 * so that it's easier to identify who's doing the complaining.
 */
#define	PROMIF_CLNTNAMELEN	16
char	promif_clntname[PROMIF_CLNTNAMELEN];

/*
 * This 'do-nothing' function is called immediately before and immediately
 * after entry to the PROM.  Some standalones (e.g. the kernel)
 * may replace this routine with their own.
 */
static void
default_prepost_prom(void)
{}

/*
 * Every standalone that wants to use this library must call
 * prom_init() before any of the other routines can be called.
 * The only state it creates is the obp_romvec_version variable,
 * and the prom_aligned_allocator variable (plus the default pre-
 * and post-prom handlers, and the clientname string)
 */
/*ARGSUSED*/
void
prom_init(char *pgmname, void *cookie)
{

	romp = cookie;

	/*
	 * Initialize the "clientname" string with the string we've
	 * been handed by the standalone
	 */
	(void) prom_strncpy(promif_clntname, pgmname, PROMIF_CLNTNAMELEN - 1);
	promif_clntname[PROMIF_CLNTNAMELEN - 1] = '\0';

#ifdef PROMIF_DEBUG
	/*
	 * Handy for bringup.
	 */
	if (!romp) {
		register u_int	dummy;

		/*
		 * We could panic here, but how do we print anything
		 * out when we can't work out what version of the PROM
		 * we have?  Force an alignment trap first, then try
		 * printing something ..
		 */
		dummy = *(int *)1;
#ifdef lint
		dummy = dummy;
#endif
		prom_panic("nil romp");
		/*NOTREACHED*/
	}
#endif /* PROMIF_DEBUG */

	/*
	 *  This is an imperfect heuristic, but it will suffice.
	 *  There is an OBP_MAGIC number in the op_magic spot.
	 */
	if (romp->obp.op_magic == OBP_MAGIC)
		obp_romvec_version = romp->obp.op_romvec_version;

	/*
	 * Add default pre- and post-prom handlers
	 * (We add this null handler to avoid the numerous tests
	 * that would otherwise have to be included around every call)
	 */
	(void) prom_set_preprom(default_prepost_prom);
	(void) prom_set_postprom(default_prepost_prom);

	/*
	 * Now do version-specific initialization
	 */
	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		PROMIF_DPRINTF(("%s: OBP V%d romvec\n", pgmname,
		    obp_romvec_version));
		break;

	case OBP_V3_ROMVEC_VERSION:
	{
		dnode_t node;

		/*
		 * Test for the presence of the aligned allocator vector
		 * If the boolean property 'aligned-allocator' exists
		 * in the /openprom node, then we have the aligned allocator.
		 */
		node = prom_finddevice("/openprom");
		if (prom_getproplen(node, "aligned-allocator") == 0)
			prom_aligned_allocator = 1;

		PROMIF_DPRINTF(("%s: OBP V%d romvec\n", pgmname,
		    obp_romvec_version));
		break;
	}

	default:
		prom_printf("%s: unknown romvec version %d\n", pgmname,
		    obp_romvec_version);
		break;
	}
}
