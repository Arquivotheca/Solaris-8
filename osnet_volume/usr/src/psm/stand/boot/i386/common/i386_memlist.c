/*
 * Copyright (c) 1992-1996,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i386_memlist.c	1.4	99/05/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/machparam.h>
#include <sys/promif.h>
#include <sys/pte.h>
#include <sys/bootconf.h>

#define	NIL	0


/*
 *	Function prototypes
 */
extern int 		insert_node(struct memlist **list,
				struct memlist *newnode);
extern caddr_t 		getlink(uint_t n);


/*
 *	This does a linear search thru the list until it finds
 *	a node that matches or engulfs the request addr and size.
 *	That node is returned.
 */
struct memlist *
search_list(struct memlist *list, struct memlist *request)
{
	while (list != (struct memlist *)NIL) {

		if ((list->address == request->address) &&
			(list->size >= request->size))
				break;

		if ((list->address < request->address) &&
			((list->address + list->size) >=
			(request->address + request->size)))
				break;

		list = list->next;
	}
	return	(list);
}


/*
 *	Once we know if and my what node the request can be
 *	satisfied, we use this routine to stick it in.
 *	There is NO ERROR RETURN from this routine.
 *	The caller MUST call search_list() before this
 *	to assure success.
 */
int
insert_node(struct memlist **list, struct memlist *request)
{
	struct memlist *node;
	struct memlist *np;
	uint_t request_eaddr = request->address + request->size;

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

#ifdef notdef
void
coalesce_list(struct memlist *list)
{
	while (list && list->next) {
		while (list->address + list->size ==
				list->next->address) {

			list->size += list->next->size;
			if (list->next->next) {
				list->next->next->prev = list;
				list->next = list->next->next;
			} else {
				list->next = NIL;
				return;
			}
		}
		list = list->next;
	}
}
#endif
