/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEM_CAGE_H
#define	_SYS_MEM_CAGE_H

#pragma ident	"@(#)mem_cage.h	1.12	98/12/03 SMI"

#include <sys/types.h>
#include <sys/memlist.h>

/*
 * Memory caging interfaces.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern int kcage_on;
extern pgcnt_t kcage_freemem;
extern pgcnt_t kcage_needfree;
extern pgcnt_t kcage_lotsfree;
extern pgcnt_t kcage_desfree;
extern pgcnt_t kcage_minfree;
extern pgcnt_t kcage_throttlefree;

extern void kcage_freemem_add(ssize_t);
extern void kcage_freemem_sub(ssize_t);
extern void kcage_create_throttle(pgcnt_t, int);

/* Third arg controls direction of growth: 0: increasing pfns, 1: decreasing. */
extern int kcage_range_trylock(void);
extern void kcage_range_lock(void);
extern void kcage_range_unlock(void);
extern int kcage_range_islocked(void);
extern int kcage_range_init(struct memlist *, int);
extern int kcage_range_add(pfn_t, pgcnt_t, int);
extern int kcage_range_delete(pfn_t, pgcnt_t);

extern void kcage_init(pgcnt_t);
extern void kcage_recalc_thresholds(void);

/* Called from vm_pageout.c */
extern void kcage_cageout_init(void);
extern void kcage_cageout_wakeup(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEM_CAGE_H */
