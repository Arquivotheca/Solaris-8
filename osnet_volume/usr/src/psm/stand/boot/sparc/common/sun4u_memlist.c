/*
 * Copyright (c) 1992-1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)sun4u_memlist.c	1.13	97/06/30 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/openprom.h>	/* only for struct prom_memlist */
#include <sys/bootconf.h>
#include <sys/salib.h>

/*
 * This file defines the interface from the prom and platform-dependent
 * form of the memory lists, to boot's more generic form of the memory
 * list.  For sun4u, the memory list properties are {hi, lo, size_hi, size_lo},
 * which is similar to boot's format, except boot's format is a linked
 * list, and the prom's is an array of these structures. Note that the
 * native property on sparc machines is identical to the property encoded
 * format, so no property decoding is required.
 *
 * Note that the format of the memory lists is really 4 encoded integers,
 * but the encoding is the same as that given in the following structure
 * on SPARC systems ...
 */

struct sun4u_prom_memlist {
	u_longlong_t	addr;
	u_longlong_t	size;
};

struct sun4u_prom_memlist scratch_memlist[200];

#define	NIL	((u_int)0)

struct memlist *fill_memlists(char *name, char *prop, struct memlist *);
extern struct memlist *pfreelistp, *vfreelistp, *pinstalledp;

extern caddr_t getlink(u_int n);
static struct memlist *reg_to_list(struct sun4u_prom_memlist *a, size_t size,
					struct memlist *old);
static void sort_reglist(struct sun4u_prom_memlist *ar, size_t size);
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
	static dnode_t pmem = 0;
	static dnode_t pmmu = 0;
	dnode_t node;
	size_t links;
	struct memlist *al;
	struct sun4u_prom_memlist *pm = scratch_memlist;

	if (pmem == (dnode_t)0)  {

		/*
		 * Figure out the interesting phandles, one time
		 * only.
		 */

		ihandle_t ih;

		if ((ih = prom_mmu_ihandle()) == (ihandle_t)-1)
			prom_panic("Can't get mmu ihandle");
		pmmu = prom_getphandle(ih);

		if ((ih = prom_memory_ihandle()) == (ihandle_t)-1)
			prom_panic("Can't get memory ihandle");
		pmem = prom_getphandle(ih);
	}

	if (strcmp(name, "memory") == 0)
		node = pmem;
	else
		node = pmmu;

	/*
	 * Read memory node and calculate the number of entries
	 */
	if ((links = prom_getproplen(node, prop)) == -1)
		prom_panic("Cannot get list.\n");
	if (links > sizeof (scratch_memlist)) {
		prom_printf("%s list <%s> exceeds boot capabilities\n",
			name, prop);
		prom_panic("fill_memlists - memlist size");
	}
	links = links / sizeof (struct sun4u_prom_memlist);


	(void) prom_getprop(node, prop, (caddr_t)pm);
	sort_reglist(pm, links);
	al = reg_to_list(pm, links, old);
	return (al);
}

/*
 *  Simple selection sort routine.
 *  Sorts platform dependent memory lists into ascending order
 */

static void
sort_reglist(struct sun4u_prom_memlist *ar, size_t n)
{
	int i, j, min;
	struct sun4u_prom_memlist temp;

	for (i = 0; i < n; i++) {
		min = i;

		for (j = i+1; j < n; j++)  {
			if (ar[j].addr < ar[min].addr)
				min = j;
		}

		if (i != min)  {
			/* Swap ar[i] and ar[min] */
			temp = ar[min];
			ar[min] = ar[i];
			ar[i] = temp;
		}
	}
}

/*
 *  This routine will convert our platform dependent memory list into
 *  struct memlists's.  And it will also coalesce adjacent  nodes if
 *  possible.
 */
static struct memlist *
reg_to_list(struct sun4u_prom_memlist *ar, size_t n, struct memlist *old)
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
		start1 = ar[i].addr;
		start2 = ar[i+1].addr;
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

		if ((uintptr_t)lo <= n->address) {
			/*
			 * This node is completely above lo.
			 * Just check for size.
			 */
			if (n->size >= size)
				return (n);
		} else if ((uintptr_t)lo < (n->address + n->size)) {
			/*
			 * lo falls within this node.
			 * Check if the end of needed size is within node.
			 */
			if (((uintptr_t)lo + size) <= (n->address + n->size))
				return (n);
		}
		/* The node is completely below lo. Skip to next one */
	}
	return (n);
}

/*
 *	Once we know if and my what node the request can be
 *	satisfied, we use this routine to stick it in.
 */
int
insert_node(struct memlist **list, struct memlist *request)
{
	struct memlist *node;
	struct memlist *np;
	uintptr_t request_eaddr = request->address + request->size;

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
