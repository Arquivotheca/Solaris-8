/*
 * Copyright (c) 1992-1996,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memlist.c	1.19	99/05/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/machparam.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

#define	NIL	0
#define	roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

extern caddr_t memlistpage;

/* Always pts to the next free link in the headtable */
/* i.e. it is always memlistpage+tableoffset */
caddr_t tablep = (caddr_t)0;
static int table_freespace;

/*
 *	Function prototypes
 */
extern caddr_t 		getlink(uint_t n);
extern void 		reset_alloc(void);

void
print_memlist(struct memlist *av)
{
	struct memlist *p = av;

	while (p != NIL) {
		printf("addr = 0x%x:0x%x, size = 0x%x:0x%x\n",
		    (uint_t)(p->address >> 32), (uint_t)p->address,
		    (uint_t)(p->size >> 32), (uint_t)p->size);
		p = p->next;
	}

}

/* allocate room for n bytes, return 8-byte aligned address */
caddr_t
getlink(uint_t n)
{
	caddr_t p;
	extern int pagesize;

	if (memlistpage == (caddr_t)0) {
		reset_alloc();
	}

	if (tablep == (caddr_t)0) {

		/*
		 * Took the following 2 lines out of above test for
		 * memlistpage == null so we can initialize table_freespace
		 */

		table_freespace = pagesize - sizeof (struct bsys_mem);
		tablep = memlistpage + sizeof (struct bsys_mem);
		tablep = (caddr_t)roundup((uintptr_t)tablep, 8);
	}

	if (n == 0)
		return ((caddr_t)0);

	n = roundup(n, 8);
	p = (caddr_t)tablep;

	table_freespace -= n;
	tablep += n;
	if (table_freespace <= 0) {
		char buf[80];

		(void) sprintf(buf,
		    "Boot getlink(): no memlist space (need %d)\n", n);
		prom_panic(buf);
	}

	return (p);
}


/*
 * This is the number of memlist structures allocated in one shot. kept
 * to small number to reduce wastage of memory, it should not be too small
 * to slow down boot.
 */
#define		ALLOC_SZ	5
static struct memlist *free_memlist_ptr = NULL;

/*
 * Free memory lists are maintained as simple single linked lists.
 * get_memlist_struct returns a memlist structure without initializing
 * any of the fields.  It is caller's responsibility to do that.
 */

struct memlist *
get_memlist_struct()
{
	struct memlist *ptr;
	register int i;

	if (free_memlist_ptr == NULL) {
		ptr = free_memlist_ptr =
			(struct memlist *)
			    getlink(ALLOC_SZ * sizeof (struct memlist));
		bzero((char *)free_memlist_ptr,
			(ALLOC_SZ * sizeof (struct memlist)));
		for (i = 0; i < ALLOC_SZ; i++)
			ptr[i].next = &ptr[i+1];
		ptr[i-1].next = NULL;
	}
	ptr = free_memlist_ptr;
	free_memlist_ptr = ptr->next;
	return (ptr);
}

/*
 * Return memlist structure to free list.
 */
void
add_to_freelist(struct memlist *ptr)
{
	struct memlist *tmp;

	if (free_memlist_ptr == NULL) {
		free_memlist_ptr = ptr;
	} else {
		for (tmp = free_memlist_ptr; tmp->next; tmp = tmp->next)
			;
		tmp->next = ptr;
	}
}
