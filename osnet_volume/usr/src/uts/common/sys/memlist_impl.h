/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEMLIST_IMPL_H
#define	_SYS_MEMLIST_IMPL_H

#pragma ident	"@(#)memlist_impl.h	1.3	98/08/12 SMI"

/*
 * Common memlist routines.
 */

#include <sys/memlist.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct memlist *memlist_get_one(void);
extern void memlist_free_one(struct memlist *);
extern void memlist_free_list(struct memlist *);
extern void memlist_free_block(caddr_t base, size_t bytes);
extern void memlist_insert(struct memlist *new, struct memlist **);
extern void memlist_del(struct memlist *, struct memlist **);
extern struct memlist *memlist_find(struct memlist *, uint64_t address);

#define	MEML_SPANOP_OK		0
#define	MEML_SPANOP_ESPAN	1
#define	MEML_SPANOP_EALLOC	2

extern int memlist_add_span(uint64_t address, uint64_t bytes,
	struct memlist **);
extern int memlist_delete_span(uint64_t address, uint64_t bytes,
	struct memlist **);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MEMLIST_IMPL_H */
