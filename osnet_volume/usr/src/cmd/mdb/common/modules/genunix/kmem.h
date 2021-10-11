/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_KMEM_H
#define	_KMEM_H

#pragma ident	"@(#)kmem.h	1.2	99/11/19 SMI"

#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int kmem_cache_walk_init(mdb_walk_state_t *);
extern int kmem_cache_walk_step(mdb_walk_state_t *);
extern void kmem_cache_walk_fini(mdb_walk_state_t *);

extern int kmem_cpu_cache_walk_init(mdb_walk_state_t *);
extern int kmem_cpu_cache_walk_step(mdb_walk_state_t *);

extern int kmem_slab_walk_init(mdb_walk_state_t *);
extern int kmem_slab_walk_step(mdb_walk_state_t *);

extern int kmem_walk_init(mdb_walk_state_t *);
extern int kmem_walk_step(mdb_walk_state_t *);
extern void kmem_walk_fini(mdb_walk_state_t *);

extern int bufctl_walk_init(mdb_walk_state_t *);
extern int bufctl_walk_step(mdb_walk_state_t *);

extern int freemem_walk_init(mdb_walk_state_t *);
extern int freectl_walk_init(mdb_walk_state_t *);

extern int kmem_log_walk_init(mdb_walk_state_t *);
extern int kmem_log_walk_step(mdb_walk_state_t *);
extern void kmem_log_walk_fini(mdb_walk_state_t *);

extern int allocdby_walk_init(mdb_walk_state_t *);
extern int allocdby_walk_step(mdb_walk_state_t *);
extern void allocdby_walk_fini(mdb_walk_state_t *);

extern int freedby_walk_init(mdb_walk_state_t *);
extern int freedby_walk_step(mdb_walk_state_t *);
extern void freedby_walk_fini(mdb_walk_state_t *);

extern int vmem_walk_init(mdb_walk_state_t *);
extern int vmem_walk_step(mdb_walk_state_t *);
extern void vmem_walk_fini(mdb_walk_state_t *);

extern int vmem_postfix_walk_step(mdb_walk_state_t *);

extern int vmem_seg_walk_init(mdb_walk_state_t *);
extern int vmem_seg_walk_step(mdb_walk_state_t *);
extern void vmem_seg_walk_fini(mdb_walk_state_t *);

extern int vmem_span_walk_init(mdb_walk_state_t *);
extern int vmem_alloc_walk_init(mdb_walk_state_t *);
extern int vmem_free_walk_init(mdb_walk_state_t *);

extern int kmem_cache(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int allocdby(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int freedby(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int whatis(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmem_log(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmem_debug(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int bufctl(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmem_verify(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmem_verify_alloc(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmem_verify_free(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int vmem(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int vmem_seg(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmalog(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int kmausers(uintptr_t, uint_t, int, const mdb_arg_t *);

extern int kmem_init_walkers(uintptr_t, const kmem_cache_t *, void *);

extern int kmem_content_maxsave;

#ifdef	__cplusplus
}
#endif

#endif	/* _KMEM_H */
