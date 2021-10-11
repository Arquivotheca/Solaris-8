/*
 * Copyright (c) 1991-1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module_ross.c	1.10	94/06/15 SMI"
	/* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/module.h>

/*
 * Support for modules based on the Cypress CY604/CY605
 * memory management unit.
 */

#ifdef	VAC
extern void	ross_cache_init();
extern void	ross_vac_pageflush();
extern void	ross_vac_flush();
extern void	ross_cache_on();
#endif	/* VAC */

#define	VERSION_MASK	0x0F000000
#define	HYP_VERSION	0x07000000

int
ross_module_identify(u_int mcr)
{
	if (((mcr >> 24) & 0xf0) == 0x10 &&
	    ((mcr & VERSION_MASK) != HYP_VERSION))
		return (1);

	return (0);
}

/*ARGSUSED*/
void
ross_module_setup(mcr)
	int	mcr;
{

#ifdef	VAC
	extern void (*v_cache_on)();
	extern int vac;

	vac = 1;
	v_cache_init = ross_cache_init;
	v_vac_pageflush = ross_vac_pageflush;
	v_vac_flush = ross_vac_flush;
	v_cache_on = ross_cache_on;
#endif	/* VAC */
}
