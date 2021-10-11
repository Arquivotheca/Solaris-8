/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ATOMIC_H
#define	_SYS_ATOMIC_H

#pragma ident	"@(#)atomic.h	1.7	99/08/15 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Add delta to target
 */
extern void atomic_add_16(uint16_t *target, int16_t delta);
extern void atomic_add_32(uint32_t *target, int32_t delta);
extern void atomic_add_long(ulong_t *target, long delta);
extern void atomic_add_64(uint64_t *target, int64_t delta);

/*
 * logical OR bits with target
 */
extern void atomic_or_uint(uint_t *target, uint_t bits);
extern void atomic_or_32(uint32_t *target, uint32_t bits);

/*
 * logical AND bits with target
 */
extern void atomic_and_uint(uint_t *target, uint_t bits);
extern void atomic_and_32(uint32_t *target, uint32_t bits);

/*
 * As above, but return the new value.  Note that these _nv() variants are
 * substantially more expensive on some platforms than the no-return-value
 * versions above, so don't use them unless you really need to know the
 * new value *atomically* (e.g. when decrementing a reference count and
 * checking whether it went to zero).
 */
extern uint16_t atomic_add_16_nv(uint16_t *target, int16_t delta);
extern uint32_t atomic_add_32_nv(uint32_t *target, int32_t delta);
extern ulong_t atomic_add_long_nv(ulong_t *target, long delta);
extern uint64_t atomic_add_64_nv(uint64_t *target, int64_t delta);

/*
 * If target == cmp, set target = newval; return old value
 */
extern uint32_t cas32(uint32_t *target, uint32_t cmp, uint32_t newval);
extern ulong_t caslong(ulong_t *target, ulong_t cmp, ulong_t newval);
extern uint64_t cas64(uint64_t *target, uint64_t cmp, uint64_t newval);
extern void *casptr(void *target, void *cmp, void *newval);

/*
 * Generic memory barrier used during lock entry, placed after the
 * memory operation that acquires the lock to guarantee that the lock
 * protects its data.  No stores from after the memory barrier will
 * reach visibility, and no loads from after the barrier will be
 * resolved, before the lock acquisition reaches global visibility.
 */
extern void membar_enter(void);

/*
 * Generic memory barrier used during lock exit, placed before the
 * memory operation that releases the lock to guarantee that the lock
 * protects its data.  All loads and stores issued before the barrier
 * will be resolved before the subsequent lock update reaches visibility.
 */
extern void membar_exit(void);

/*
 * Arrange that all stores issued before this point in the code reach
 * global visibility before any stores that follow; useful in producer
 * modules that update a data item, then set a flag that it is available.
 * The memory barrier guarantees that the available flag is not visible
 * earlier than the updated data, i.e. it imposes store ordering.
 */
extern void membar_producer(void);

/*
 * Arrange that all loads issued before this point in the code are
 * completed before any subsequent loads; useful in consumer modules
 * that check to see if data is available and read the data.
 * The memory barrier guarantees that the data is not sampled until
 * after the available flag has been seen, i.e. it imposes load ordering.
 */
extern void membar_consumer(void);

#if defined(_LP64) || defined(_ILP32)
#define	atomic_add_ip		atomic_add_long
#define	atomic_add_ip_nv	atomic_add_long_nv
#define	casip			caslong
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ATOMIC_H */
