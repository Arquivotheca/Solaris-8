/*
 *	Copyright (c) 1991-1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module.c	1.5	93/03/26 SMI" /* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/cpu.h>

/* Generic pointer to specific routine related to modules */

/* FIXME: should be replaced by cpuinfo */
/* note: autoconf also uses this, a little. */

/*
 * Module_setup is called from locore.s, very early after.  It will look
 * at the implementation and version number of the module, and decide
 * which routines to use for modules specific tasks.
 */

/*
 * If we don't have a prom (some SAS runs are done
 * like this, before fakeproms are made available
 * for the platform being simulated) we need to figure
 * out what our "module_type" is.
 */
extern int	module_type;

int	mxcc_present = 1;	/* yes, force vik to vik_m */

#define	ROSS604f	(ROSS604+1)
#define	ROSS605b	(ROSS605-1)

extern int	srmmu_mmu_getctp();
extern void	srmmu_mmu_flushall();
extern void	srmmu_mmu_flushpage();

int		(*v_mmu_getctp)() = srmmu_mmu_getctp;
void		(*v_mmu_flushall)() = srmmu_mmu_flushall;
void		(*v_mmu_flushpage)() = srmmu_mmu_flushpage;

#ifdef	VAC
extern void	vac_noop();

void	      (*v_cache_init)() = vac_noop;
void	      (*v_vac_pageflush)() = vac_noop;
void	      (*v_vac_flush)() = vac_noop;
void	      (*v_cache_on)() = vac_noop;
#endif	/* VAC */

void
module_setup(mcr)
	int	mcr;
{
	extern int module_info_size;
	int	i = module_info_size;
	struct module_linkage *p = module_info;

#ifdef NOPROM
	register int    mt = mcr >> 24;

	if (mt == ROSS604f)
		mt = ROSS604;

	if (mt == ROSS605b)
		mt = ROSS605;

	if (mt == VIKING)
		if (mxcc_present)
			mt = VIKING_M;

	module_type = mt;
#endif

	while (i-- > 0) {
		if ((*p->identify_func)(mcr))
			(*p->setup_func)(mcr);
		++p;
	}
}
