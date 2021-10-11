/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module_swift.c 1.3	94/01/15 SMI"

#include <sys/param.h>
#include <sys/module.h>

/*
 * Support for modules based on the Sun/Fujitsu Swift CPU.
 */

extern void	swift_cache_init();
extern void	swift_turn_cache_on();

extern void	swift_vac_pageflush();
extern void	swift_vac_flush();

int
swift_module_identify(u_int mcr)
{
	u_int psr = getpsr();

	if (((psr >> 24) & 0xff) == 0x04)
		return (1);

	return (0);
}

/*ARGSUSED*/
void
swift_module_setup(mcr)
	int	mcr;
{
	extern void (*v_cache_on)();
	extern int vac;

	vac = 1;
	v_cache_init = swift_cache_init;
	v_cache_on = swift_turn_cache_on;

	v_vac_pageflush = swift_vac_pageflush;
	v_vac_flush = swift_vac_flush;
}
