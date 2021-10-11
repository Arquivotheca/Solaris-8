/*
 * Copyright (c) 1985-1991, by Sun Microsystems, Inc.
 */

#ident "@(#)sm_io.c	1.5	94/06/24 SMI"

/*
 * Device interface code for standalone I/O system.
 *
 * Most simply indirect thru the table to call the "right" routine.
 */
#ifdef sparc
#include <sys/machparam.h>
#include <sys/saio.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#else
#include <sys/types.h>
#endif
#include <sys/bootconf.h>


#ifdef ALLOC_DEBUG
static int debug = 1;
#else
static int debug = 0;
#endif

#define	dprintf		if (debug) printf


#ifdef sparc
extern int devread(struct saioreq *s);
extern int devopen(struct saioreq *s);
extern int devclose(struct saioreq *s);
#endif
extern caddr_t devalloc(int devtype, char *physaddr,
	unsigned bytes);
extern void map_pages(caddr_t v, u_int sz);

extern int pagesize;

#ifdef sparc
int
devread(struct saioreq *sip)
{
	return ((*sip->si_boottab->b_strategy)(sip, READ));
}

int
devopen(struct saioreq *sip)
{
}


devclose(struct saioreq *sip)
{
	return ((sip->si_boottab->b_close)(sip));
}
#endif

caddr_t
devalloc(int devtype, char *physaddr, unsigned bytes)
{
	caddr_t		addr;
	u_long		offset;
	register caddr_t  raddr;
	register unsigned pages;

	if (!bytes)
		return ((caddr_t)0);

	offset = (u_int)physaddr & (pagesize-1);
	pages = bytes + offset;
	dprintf("pages = %x pgsize = %x\n", pages, pagesize);

	if (!addr)
		return ((caddr_t)0);

	return (addr + ((int)(physaddr) & (pagesize-1)));
}

void
map_pages(caddr_t vaddr, u_int bytes)
{
	bzero(vaddr, bytes);
}
