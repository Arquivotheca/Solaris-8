/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEMLIST_PLAT_H
#define	_SYS_MEMLIST_PLAT_H

#pragma ident	"@(#)memlist_plat.h	1.3	98/06/03 SMI"

/*
 * Boot time configuration information objects
 */

#include <sys/types.h>
#include <sys/memlist.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int check_boot_version(int);
extern int check_memexp(struct memlist *, uint_t);
extern void copy_memlist(struct memlist *, struct memlist **);
extern int copy_physavail(struct memlist *, struct memlist **,
    uint_t, uint_t);
extern void size_physavail(struct memlist *, pgcnt_t *, int *);
extern void installed_top_size(struct memlist *, int *, int *);
extern void fix_prom_pages(struct memlist *, struct memlist *);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MEMLIST_PLAT_H */
