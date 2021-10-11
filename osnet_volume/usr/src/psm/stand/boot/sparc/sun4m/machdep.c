/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ident	"@(#)machdep.c	1.29	99/10/04 SMI" /* From SunOS 4.1.1 */

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/promif.h>
#include <sys/salib.h>

int pagesize = PAGESIZE;
int vac = 1;
union sunromvec *romp = (union sunromvec *)0xffe81000;

void
fiximp(void)
{
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	char name[OBP_MAXDRVNAME];
	static char cpubuf[3 * OBP_MAXDRVNAME];
	extern char *cpulist;
	extern int use_align;

#ifndef	ALL_BROKEN_PROMS_REALLY_GONE
	{
		auto int rmap;
		prom_interpret("h# f800.0000 rmap@ swap ! ",
			(int)&rmap, 0, 0, 0, 0);
		/*
		 * If this region is mapped, it means you have a
		 * preFCS PROM in your 4/6xx machine.  You should
		 * get it updated (or we should put back the workaround
		 * from fiximp_sun4m.c)
		 */
		if (rmap)
			prom_printf("Warning: f8 region already mapped!\n");
	}
#endif

	/*
	 * Get properties for aligned memory requests and cpu module
	 */
	use_align = 0;
	if (prom_is_openprom()) {
		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_byname(prom_rootnode(), "openprom", stk);
		if (node != OBP_NONODE && node != OBP_BADNODE) {
			if (prom_getproplen(node, "aligned-allocator") == 0)
				use_align = 1;
		}
		node = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
		if (node != OBP_NONODE && node != OBP_BADNODE) {
			if (prom_getprop(node, OBP_NAME, name) > 0) {
				(void) strcpy(cpubuf, name);
				(void) strcat(cpubuf, ":");
			}
			if (prom_getprop(node, OBP_COMPATIBLE, name) > 0) {
				(void) strcpy(cpubuf, name);
				(void) strcat(cpubuf, ":");
			}
		}
		prom_stack_fini(stk);
	}

	/* Legacy code can use cpu default module */
	(void) strcat(cpubuf, "default");
	cpulist = cpubuf;
}


extern u_int	getmcr(void);
extern int	ross_module_identify(u_int);
extern int	ross625_module_identify(u_int);
extern void	ross625_module_ftd();

void
setup_aux(void)
{
	u_int mcr;
	extern int icache_flush;

	/*
	 * Read the module control register.
	 */
	mcr = getmcr();

	/*
	 * Set the value of icache_flush.  This value is
	 * passed to the kernel linker so that it knows
	 * whether or not to iflush when relocating text.
	 * Because of a bug in the Ross605, the iflush
	 * instruction causes an illegal instruction
	 * trap therefore we don't iflush in that case.
	 */
	if (ross_module_identify(mcr))
		icache_flush = 0;
	else
		icache_flush = 1;

	/*
	 * On modules which allow FLUSH instructions
	 * to cause a T_UNIMP_FLUSH trap, make sure
	 * the trap is disabled since the kernel linker will
	 * be iflush'ing prior to the kernel taking over
	 * the trap table.
	 */
	if (ross625_module_identify(mcr))
		ross625_module_ftd();
}
