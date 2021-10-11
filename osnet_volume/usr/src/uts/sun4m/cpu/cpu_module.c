/*
 * Copyright (c) 1987-1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)cpu_module.c	1.1	96/05/17 SMI"

#include <sys/types.h>
#include <sys/cpu_module.h>
#include <sys/machparam.h>
#include <sys/module.h>

/*
 * This is a dummy file that provides the default cpu module
 * that is linked to unix.
 *
 * It can also be viewed as a prototype of what is necessary to build
 * any new cpu specific module interface.
 *
 * At early boot time, the module_setup() routine walks through the
 * array "module_info", calling cpu specific identify routines to
 * determine the hardware currently installed.
 *
 * The cpu specific code must also provide a machine setup routine
 * to install function address pointers to any routines which
 * need special handling.
 *
 * See the file module.c for more comments explaining the
 * module_identify() function, and for a list of all vectored
 * functions which can be reassigned.
 *
 * Typically, the cpu setup function will set the appropriate
 * flags, such as CACHE_VAC or CACHE_IOCOHERENT, in the cache
 * variable, and install pointers to functions to initialize
 * the cache, flush specific cache lines, or to write mmu pte
 * entries.
 *
 * The individual loadable cpu modules are built in separate directories,
 * and given names that match the OBP cpu property name.  At boot time,
 * the kernel linker will use the same property name.  For old
 * legacy code, a "default" cpu module is also built that contains
 * all the previously supported module types.
 */


/*
 * Initialize module_info[] array with new cpu identify and setup
 * function addresses.
 */

extern int	newcpu_module_identify(u_int);
extern void	newcpu_module_setup(u_int);

struct module_linkage module_info[] = {
	{ newcpu_module_identify, newcpu_module_setup },
};

int	module_info_size = sizeof (module_info) / sizeof (module_info[0]);

/*
 * The typical module_identify routine just looks at the psr or mcr
 * to regognize hardware vendor code.
 */

#define	NEWCPU_VERSION	0x00		/* Actual hardware version id code */

int
newcpu_module_identify(u_int mcr)
{
	return (((mcr >> 24) & 0xff) == NEWCPU_VERSION);
}

void
newcpu_module_setup(u_int mcr)
{
	/*
	 * Typical code:
	 *
	 * cache |= (CACHE_PAC | CACHE_PTAG);
	 *
	 * v_cache_init = newcpu_cache_init;
	 * v_turn_cache_on = newcpu_turn_cache_on;
	 * v_pac_flushall = newcpu_pac_flushall;
	 * v_mmu_writepte = newcpu_mmu_writepte;
	 * v_mp_mmu_writepte = newcpu_mp_mmu_writepte;
	 * v_window_overflow = newcpu_window_overflow;
	 * v_window_underflow = newcpu_window_underflow;
	 *
	 * Virtual cache processors may need other routines
	 * user, context, region, or full cache/tlb flushes.
	 */
}
