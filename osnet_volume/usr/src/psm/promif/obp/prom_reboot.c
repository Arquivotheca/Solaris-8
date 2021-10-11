/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_reboot.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * There should be no return from this function
 */
void
prom_reboot(char *bootstr)
{
	promif_preprom();
	OBP_BOOT(bootstr);
	promif_postprom();
	/* NOTREACHED */
}
