/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_init.c	1.24	98/06/16 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int	obp_romvec_version = -1;  /* -1 rsrvd for non-obp sunromvec */
int	prom_aligned_allocator = 0; /* Not needed for 1275 */
void	*p1275cif;		/* 1275 Client interface handler */

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
 *
 */
void
prom_init(char *pgmname, void *p1275cookie)
{
	/*
	 * Allow implementation to validate input argument.
	 */
	p1275cif = p1275_cif_init(p1275cookie);

	if ((p1275cif == NULL)) {
		prom_fatal_error("promif: No interface!");
		/*NOTREACHED*/
	}

	/*
	 * Initialize the "clientname" string with the string we've
	 * been handed by the standalone
	 */
	(void) prom_strncpy(promif_clntname, pgmname, PROMIF_CLNTNAMELEN - 1);
	promif_clntname[PROMIF_CLNTNAMELEN - 1] = '\0';

	obp_romvec_version = OBP_PSEUDO_ROMVEC_VERSION;

	/*
	 * Add default pre- and post-prom handlers
	 * (We add this null handler to avoid the numerous tests
	 * that would otherwise have to be included around every call)
	 */
	(void) prom_set_preprom(default_prepost_prom);
	(void) prom_set_postprom(default_prepost_prom);
}

/*
 * Fatal promif internal error, not an external interface
 */

/*ARGSUSED*/
void
prom_fatal_error(const char *errormsg)
{

	volatile int	zero = 0;
	volatile int	i = 1;

	/*
	 * No prom interface, try to cause a trap by
	 * dividing by zero, leaving the message in %i0.
	 */

	i = i / zero;
	/*NOTREACHED*/
}
