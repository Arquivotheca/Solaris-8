/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEMLIST_PLAT_H
#define	_SYS_MEMLIST_PLAT_H

#pragma ident	"@(#)memlist_plat.h	1.4	98/08/21 SMI"

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
extern void copy_memlist(u_longlong_t *, size_t, struct memlist **);
extern int copy_physavail(u_longlong_t *, size_t, struct memlist **,
    uint_t, uint_t);
extern void size_physavail(u_longlong_t *physavail, size_t size,
    pgcnt_t *npages, int *memblocks);
extern void installed_top_size_memlist_array(u_longlong_t *, size_t, pfn_t *,
    pgcnt_t *);
extern void installed_top_size(struct memlist *, pfn_t *, pgcnt_t *);
extern void fix_prom_pages(struct memlist *, struct memlist *);
extern void copy_boot_memlists(u_longlong_t **physinstalled,
    size_t *physinstalled_len, u_longlong_t **physavail, size_t *physavail_len,
    u_longlong_t **virtavail, size_t *virtavail_len);
extern void phys_install_has_changed(void);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MEMLIST_PLAT_H */
