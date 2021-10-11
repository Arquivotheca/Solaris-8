/*
 * Copyright (c) 1992-1994 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident "@(#)sun4x_memlist.c	1.12	99/10/04 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/openprom.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

/*
 * This file contains the memlist/prom interfaces for sun4x machines.
 * There's a similar file for sun4u (and newer machines.)
 */

#define	NIL	((u_int)0)

#ifdef DEBUG
static int memlistdebug = 1;
#else DEBUG
static	int memlistdebug = 0;
#endif DEBUG
#define	dprintf		if (memlistdebug) printf

struct memlist *fill_memlists(char *name, char *prop, struct memlist *old);
extern struct memlist *pfreelistp, *vfreelistp, *pinstalledp;

extern caddr_t getlink(u_int n);
static struct memlist *reg_to_list(struct avreg *a, int size,
					struct memlist *old);
static void sort_reglist(struct avreg *ar, int size);
static struct prom_memlist *prom_memory_available(void);
static struct prom_memlist *prom_memory_physical(void);
static struct prom_memlist *prom_memory_virtual(void);
extern void kmem_init(void);
extern void add_to_freelist(struct memlist *);
extern struct memlist *get_memlist_struct();

void
init_memlists()
{
	/* this list is a map of pmem actually installed */
	pinstalledp = fill_memlists("memory", "reg", pinstalledp);

	vfreelistp = fill_memlists("virtual-memory", "available", vfreelistp);
	pfreelistp = fill_memlists("memory", "available", pfreelistp);

	kmem_init();
}

struct memlist *
fill_memlists(char *name, char *prop, struct memlist *old)
{
	dnode_t	node;
	struct memlist *al;
	struct avreg *ar;
	int len = 0;
	int links = 0;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;

	stk = prom_stack_init(sp, sizeof (sp));
	node = prom_findnode_byname(prom_rootnode(), name, stk);
	prom_stack_fini(stk);

	if (node == OBP_NONODE) {
		if (prom_getversion() > 0) {
			prom_panic("Cannot find node.\n");

		} else if (strcmp(prop, "available") == 0 &&
		    strcmp(name, "memory") == 0) {

			struct prom_memlist *list, *dbug, *tmp;
			struct avreg *ap;

			dbug = list = prom_memory_available();

			if (memlistdebug) {
				printf("physmem-avail:\n");
				do {
					printf("start = 0x%x, size = 0x%x, "
					    "list = 0x%x\n", dbug->address,
					    dbug->size, dbug);
				} while ((dbug = dbug->next) != NIL);
			}

			for (tmp = list; tmp; tmp = tmp->next)
				links++;
			ar = ap = (struct avreg *)getlink(links *
			    sizeof (struct avreg));
			do {
				ap->type = 0;
				ap->start = list->address;
				ap->size = list->size;
				ap++;
			} while ((list = list->next) != NIL);

		} else if (strcmp(prop, "available") == 0 &&
				strcmp(name, "virtual-memory") == 0) {

			struct prom_memlist *list, *dbug, *tmp;
			struct avreg *ap;

			dbug = list = prom_memory_virtual();

			if (memlistdebug) {
				printf("virtmem-avail: \n");
				do {
					printf("start = 0x%x, size = 0x%x, "
					    "list = 0x%x\n", dbug->address,
					    dbug->size, dbug);
				} while ((dbug = dbug->next) != NIL);
			}

			for (tmp = list; tmp; tmp = tmp->next)
				links++;
			ar = ap = (struct avreg *)getlink(links *
			    sizeof (struct avreg));
			do {
				ap->type = 0;
				ap->start = list->address;
				ap->size = list->size;
				ap++;
			} while ((list = list->next) != NIL);

		} else if (strcmp(prop, "reg") == 0 &&
				strcmp(name, "memory") == 0) {
			struct prom_memlist *list, *dbug, *tmp;
			struct avreg *ap;

			dbug = list = prom_memory_physical();

			if (memlistdebug) {

				printf("physmem-installed: \n");
				do {
					printf("start = 0x%x, size = 0x%x, "
					    "list = 0x%x\n", dbug->address,
					    dbug->size, dbug);
				} while ((dbug = dbug->next) != NIL);
			}

			for (tmp = list; tmp; tmp = tmp->next)
				links++;
			ar = ap = (struct avreg *)getlink(links *
			    sizeof (struct avreg));
			do {
				ap->type = 0;
				ap->start = list->address;
				ap->size = list->size;
				ap++;
			} while ((list = list->next) != NIL);
		}
	} else {
		/*
		 * XXX  I know, I know.
		 * But this will get fixed when boot uses arrays, not lists.
		 */
		u_int buf[200/sizeof (u_int)];	/* u_int aligned buffer */

		/* Read memory node, if it is there */
		if ((len = prom_getproplen(node, prop)) ==
		    (int)OBP_BADNODE)
			prom_panic("Cannot get list.\n");

		/* Oh, malloc, where art thou? */
		ar = (struct avreg *)buf;
		(void) prom_getprop(node, prop, (caddr_t)ar);

		/* calculate the number of entries we will have */
		links = len / sizeof (struct avreg);
	}

	sort_reglist(ar, links);

	al = reg_to_list(ar, links, old);

	return (al);
}

/*
 *  These routines are for use by /boot only.  The kernel
 *  is not to call them because they return a foreign memory
 *  list data type.  If it were to try to copy them, the kernel
 *  would have to dynamically allocate space for each node.
 *  And these promif functions are specifically prohibited from
 *  such activity.
 */

/*
 * This routine will return a list of the physical
 * memory available to standalone programs for
 * further allocation.
 */
static struct prom_memlist *
prom_memory_available(void)
{
	switch (prom_getversion()) {

	case OBP_V0_ROMVEC_VERSION:
		return ((struct prom_memlist *)OBP_V0_AVAILMEMORY);

	default:
		return ((struct prom_memlist *)0);
	}
}

/*
 * This is a description of the number of memory cells actually installed
 * in the system and where they reside.
 */
static struct prom_memlist *
prom_memory_physical(void)
{
	switch (prom_getversion()) {

	case OBP_V0_ROMVEC_VERSION:
		return ((struct prom_memlist *)OBP_V0_PHYSMEMORY);

	default:
		return ((struct prom_memlist *)0);
	}
}

/*
 * This returns a list of virtual memory available
 * for further allocation by the standalone.
 */
static struct prom_memlist *
prom_memory_virtual(void)
{
	switch (prom_getversion()) {

	case OBP_V0_ROMVEC_VERSION:
		return ((struct prom_memlist *)OBP_V0_VIRTMEMORY);

	default:
		return ((struct prom_memlist *)0);
	}
}

/*
 *  Simple selection sort routine.
 *  Sorts avreg's into ascending order
 */

static void
sort_reglist(struct avreg *ar, int n)
{
	int i, j, min;
	struct avreg temp;
	u_longlong_t start1, start2;

	for (i = 0; i < n; i++) {
		min = i;

		for (j = i+1; j < n; j++) {
			start1 = (((u_longlong_t)ar[j].type) << 32) +
			    ar[j].start;
			start2 = (((u_longlong_t)ar[min].type) << 32) +
			    ar[min].start;
			if (start1 < start2)
				min = j;
		}

		/* Swap ar[i] and ar[min] */
		temp = ar[min];
		ar[min] = ar[i];
		ar[i] = temp;
	}
}

/*
 *  This routine will convert our struct avreg's into
 *  struct memlists's.  And it will also coalesce adjacent
 *  nodes if possible.
 */
static struct memlist *
reg_to_list(struct avreg *ar, int n, struct memlist *old)
{
	struct memlist *ptr, *head, *last;
	int i;
	u_longlong_t size = 0;
	u_longlong_t addr = 0;
	u_longlong_t start1, start2;
	int flag = 0;

	if (n == 0)
		return ((struct memlist *)0);

	/*
	 * if there was a memory list allocated before, free it first.
	 */
	if (old)
		(void) add_to_freelist(old);

	head = NULL;
	last = NULL;

	for (i = 0; i < n; i++) {
		start1 = (((u_longlong_t)ar[i].type) << 32) +
		    ar[i].start;
		start2 = (((u_longlong_t)ar[i+1].type) << 32) +
		    ar[i+1].start;
		if (i < n-1 && (start1 + ar[i].size == start2)) {
			size += ar[i].size;
			if (!flag) {
				addr = start1;
				flag++;
			}
			continue;
		} else if (flag) {
			/*
			 * catch the last one on the way out of
			 * this iteration
			 */
			size += ar[i].size;
		}

		ptr = (struct memlist *)get_memlist_struct();
		if (!head)
			head = ptr;
		if (last)
			last->next = ptr;
		ptr->address = flag ? addr : start1;
		ptr->size = size ? size : ar[i].size;
		ptr->prev = last;
		last = ptr;

		size = 0;
		flag = 0;
		addr = 0;
	}

	last->next = NULL;
	return (head);
}

/*
 *	This does a linear search thru the list until it finds a node
 *	1). whose startaddr lies above lo and whose size is at least
 *		the size of the requested size, OR
 *	2). lo is between startaddr and startaddr+endaddr, but there is
 *		enough room there so that lo+size is within node's range.
 *	If node's range is completely below lo, it is no good.
 *	When such node is found, that node is returned.
 *	If no node is found, NIL is returned.
 */
struct memlist *
get_min_node(struct memlist *list, u_longlong_t lo, u_int size)
{
	struct memlist *n;

	for (n = list; n != NIL; n = n->next) {

		if ((u_int) lo <= n->address) {
			/*
			 * This node is completely above lo.
			 * Just check for size.
			 */
			if (n->size >= size)
				return (n);
		} else if ((u_int)lo < (n->address + n->size)) {
			/*
			 * lo falls within this node.
			 * Check if the end of needed size is within node.
			 */
			if (((u_int)lo + size) <= (n->address + n->size))
				return (n);
		}
		/* The node is completely below lo. Skip to next one */
	}
	return (n);
}

/*
 *	Once we know if and what node the request can be
 *	satisfied, we use this routine to stick it in.
 */
int
insert_node(struct memlist **list, struct memlist *request)
{
	struct memlist *node;
	struct memlist *np;
	u_int request_eaddr = request->address + request->size;

	/* Search for the memlist that engulfs this request */
	for (node = (*list); node != NIL; node = node->next) {
		if ((node->address <= request->address) &&
		    ((node->address + node->size) >= request_eaddr))
			break;
	}

	if (node == NIL)
		return (0);

	if (request->address == node->address) {
		/* See if we completely replace an existing node */
		/* Then delete the node from the list */

		if (request->size == node->size) {
			if (node->next != NIL) node->next->prev = node->prev;
			if (node->prev != NIL) node->prev->next = node->next;
			else *list = node->next;
			return (1);
		/* else our request is replacing the front half */
		} else {
			node->address = request->address + request->size;
			node->size -= request->size;
			return (1);
		}
	} else if ((request->address + request->size) ==
			(node->address + node->size)) {
		node->size -= request->size;
		return (1);
	}

	/* else we have to split a node */

	np = (struct memlist *)getlink(sizeof (struct memlist));

	/* setup the new data link and */
	/* point the new link at the larger node */
	np->address = request->address + request->size;
	np->size = (node->address + node->size) -
			(request->address + request->size);
	node->size = node->size - request->size - np->size;

	/* Insert the node in the chain after node */
	np->prev = node;
	np->next = node->next;

	if (node->next != NIL) node->next->prev = np;
	node->next = np;
	return (1);
}
