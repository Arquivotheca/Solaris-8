/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)prom_env.c	1.3	97/02/12 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * These routines are called immediately before,
 * and immediately after entering the PROM
 */
void (*promif_preprom)(void);
void (*promif_postprom)(void);

/*
 * These two routines are used by prom_setprop() in sun4d.
 * See #4011031; Needed for sun4d prom/kernel demap problem.
 * Other architectures don't need to use these functions; they
 * are setup at prom_init time with null-effect defaults.
 *
 * These functions are used as a monitor to prevent the kernel and
 * OBP from doing demaps at the same time.
 */
void (*promif_setprop_preprom)(void);
void (*promif_setprop_postprom)(void);

/*
 * And these routines are functional interfaces
 * used to set the above (promif-private) globals
 */
void (*
prom_set_preprom(void (*preprom)(void)))(void)
{
	void (*fn)(void);

	fn = promif_preprom;
	promif_preprom = preprom;
	promif_setprop_preprom = preprom;
	return (fn);
}

void (*
prom_set_postprom(void (*postprom)(void)))(void)
{
	void (*fn)(void);

	fn = promif_postprom;
	promif_postprom = postprom;
	promif_setprop_postprom = postprom;
	return (fn);
}

void (*
prom_set_prop_preprom(void (*preprom)(void)))(void)
{
	void (*fn)(void);

	fn = promif_setprop_preprom;
	promif_setprop_preprom = preprom;
	return (fn);
}

void (*
prom_set_prop_postprom(void (*postprom)(void)))(void)
{
	void (*fn)(void);

	fn = promif_setprop_postprom;
	promif_setprop_postprom = postprom;
	return (fn);
}
