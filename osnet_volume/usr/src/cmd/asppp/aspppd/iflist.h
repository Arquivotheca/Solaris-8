#ident	"@(#)iflist.h	1.2	97/01/02 SMI"

/* Copyright (c) 1995 by Sun Microsystems, Inc. */

#ifndef _IFLIST_H
#define	_IFLIST_H

struct iflist {
	char *name;
	struct iflist *next;
	u_long dst_addr;
	boolean_t used;
};

extern struct iflist	*iflist;

void add_interface(char *);
void register_interfaces(void);
void mark_interface_used(char *);
void mark_interface_free(char *);
int find_interface(u_long);

#endif	/* _IFLIST_H */
