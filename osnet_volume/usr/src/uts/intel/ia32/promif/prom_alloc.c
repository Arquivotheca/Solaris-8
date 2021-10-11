/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_alloc.c	1.12	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

#ifdef KADB
#undef _KERNEL
#include <sys/bootsvcs.h>
#undef printf
#endif

#ifdef I386BOOT
#include <sys/bootsvcs.h>
#endif

#if !defined(KADB) && !defined(I386BOOT)
#include <sys/cmn_err.h>
#include <sys/bootconf.h>
extern struct bootops *bootops;
#endif

/*
 * prom_alloc
 *
 * allocate chunks of memory; takes two arguments; size, and
 * the desired virtual address, which is an optional argument.
 * If the requested address is 0, the current segment is extended.
 * Otherwise a new data segment is allocated.
 */

/*ARGSUSED2*/
caddr_t
prom_alloc(caddr_t virthint, uint_t size, int align)
{
#ifdef KADB
	return ((caddr_t)malloc(virthint, size, 0));
#endif

#ifdef I386BOOT
	return ((caddr_t)map_mem((uint_t)virthint, size, align));
#endif

#if !defined(KADB) && !defined(I386BOOT)
	return (caddr_t)BOP_ALLOC(bootops, virthint, size, BO_NO_ALIGN);
#endif
}

/*
 * prom_free
 *
 * currently a no-op.
 */

/*ARGSUSED*/
void
prom_free(caddr_t virt, uint_t size)
{
#ifdef I386BOOT
	prom_printf("prom_free on AT386?\n");
#else
	printf("prom_free(%x,%x) on AT386?\n", (int)virt, size);
#endif
}
