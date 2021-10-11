/*
 * Copyright (c) 1991-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_init.c	1.15	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/bootconf.h>
#include <sys/obpdefs.h>
#include <sys/kmem.h>

int	promif_debug = 0;	/* debug */
int	emul_1275 = 0;

/*
 *  Every standalone that wants to use this library must call
 *  prom_init() before any of the other routines can be called.
 *  Copy PROM device tree into memory.
 */
/*ARGSUSED*/
void
prom_init(char *pgmname, void *cookie)
{
#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)
#ifdef __i386
	/*
	 * Look for the 1275 property 'bootpath' here. If it exists
	 * and has a non-NULL value we need to assimilate the device
	 * tree bootconf has constructed.  Otherwise we do things the
	 * old way.
	 */
	emul_1275 = (BOP_GETPROPLEN(bootops, "bootpath") > 1);
#else
	/*
	 * The ia64 secondary boot will always emulate 1275
	 */
	emul_1275 = 1;
#endif	/* __i386 */
#endif
}

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)

void
prom_setup()
{
	if (!prom_is_p1275())
		return;

	promif_create_device_tree();
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
	 * No prom interface, try to cause a trap by dividing by zero.
	 */

	i = i / zero;
	/*NOTREACHED*/
}
#endif /* !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT) */
