/*
 *  Copyright (c) 1995, by Sun Microsystems, Inc.
 *
 *  Promlib extensions for kadb:
 *
 *    The following (prom-like) routines have special behavior when called
 *    from KADB.  For the most part, it's simply a matter of vectoring off
 *    to the appropriate standalone subroutine ...
 */

#pragma ident	"@(#)prom.c	1.10	99/08/19 SMI"

#include <sys/promif.h>
#undef _KERNEL
#include <sys/fcntl.h>
#include <sys/bootsvcs.h>

void
prom_panic(char *s)
{
	for (;;) {
		prom_printf("PANIC: %s\n", s);
		prom_getchar();
	}
}

void
prom_exit_to_mon(void)
{
	prom_printf("rebooting...\n");
	pc_reset();
}

char *
prom_alloc(caddr_t virt, unsigned size, int align)
{
	return (malloc(virt, size, align));
}

unsigned int
prom_vlimit(unsigned int utop)
{
	return (vlimit(utop));
}
