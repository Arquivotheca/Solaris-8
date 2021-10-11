/*
 * Copyright (c) 1996-1997, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)bootops.c	1.1	97/06/30 SMI"

/*
 * Definitions of interfaces that provide services from the secondary
 * boot program to its clients (primarily unix, krtld, kadb and their
 * successors.) This interface replaces the bootops (BOP) implementation
 * as the interface to be called by boot clients.
 *
 * Not used on x86 yet, but needed since standalones share link directives
 * and thus libboot.a needs to be built.
 *
 */

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/bootconf.h>

u_int
bop_getversion(struct bootops *bop)
{
	return (bop->bsys_version);
}

int
bop_open(struct bootops *bop, char *name, int flags)
{
	return ((bop->bsys_open)(bop, name, flags));
}

int
bop_read(struct bootops *bop, int fd, caddr_t buf, size_t size)
{
	return ((bop->bsys_read)(bop, fd, buf, size));
}

int
bop_seek(struct bootops *bop, int fd, off_t hi, off_t lo)
{
	return ((bop->bsys_seek)(bop, fd, hi, lo));
}

int
bop_close(struct bootops *bop, int fd)
{
	return ((bop->bsys_close)(bop, fd));
}

caddr_t
bop_alloc(struct bootops *bop, caddr_t virthint, size_t size, int align)
{
	return ((bop->bsys_alloc)(bop, virthint, size, align));
}

void
bop_free(struct bootops *bop, caddr_t virt, size_t size)
{
	(bop->bsys_free)(bop, virt, size);
}

caddr_t
bop_map(struct bootops *bop, caddr_t virt, int space,
	caddr_t phys, size_t size)
{
	return ((bop->bsys_map)(bop, virt, space, phys, size));
}

void
bop_unmap(struct bootops *bop, caddr_t virt, size_t size)
{
	(bop->bsys_unmap)(bop, virt, size);
}

void
bop_quiesce_io(struct bootops *bop)
{
	(bop->bsys_quiesce_io)(bop);
}

int
bop_getproplen(struct bootops *bop, char *name)
{
	return ((bop->bsys_getproplen)(bop, name));
}

int
bop_getprop(struct bootops *bop, char *name, void *value)
{
	return ((bop->bsys_getprop)(bop, name, value));
}

char *
bop_nextprop(struct bootops *bop, char *prevprop)
{
	return ((bop->bsys_nextprop)(bop, prevprop));
}

void
bop_puts(struct bootops *bop, char *string)
{
	(bop->bsys_printf)(bop, string);

}

void
bop_putsarg(struct bootops *bop, char *string, u_longlong_t arg)
{
	(bop->bsys_printf)(bop, string, arg);
}
