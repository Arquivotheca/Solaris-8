/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_gettime.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This prom entry point cannot be used once the kernel is up
 * and running (after stop_mon_clock is called) since the kernel
 * takes over level 14 interrupt handling which the PROM depends
 * on to update the time.
 */
u_int
prom_gettime(void)
{
	return (OBP_MILLISECONDS);
}
