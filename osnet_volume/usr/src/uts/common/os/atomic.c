/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)atomic.c	1.6	99/08/15 SMI"

#include <sys/atomic.h>
#include <sys/spl.h>
#include <sys/machlock.h>

/*
 * Standard implementations of various atomic primitives
 * for old platforms that can't do them in hardware.
 *
 * Platforms that implement *all* of these routines
 * should not link with this file (it's all dead code).
 *
 * Platforms that implement some subset of these routines
 * should link with this file to get the rest of them.
 * The emulated versions are all defined with #pragma weak
 * so that the platform's implementation wins if it exists.
 */
#define	ATOMIC_HASH_SIZE	256
#define	ATOMIC_HASH_MASK	(ATOMIC_HASH_SIZE - 1)
#define	ATOMIC_HASH_SHIFT	6
#define	ATOMIC_HASH(addr)	\
	(((uintptr_t)(addr) >> ATOMIC_HASH_SHIFT) & ATOMIC_HASH_MASK)
#define	ATOMIC_LOCK(addr)	&atomic_lock[ATOMIC_HASH(addr)]
static lock_t atomic_lock[ATOMIC_HASH_SIZE];

#pragma weak atomic_add_16 = emul_atomic_add_16
#pragma weak atomic_add_32 = emul_atomic_add_32
#pragma weak atomic_add_64 = emul_atomic_add_64
#ifdef _LP64
#pragma weak atomic_add_long = emul_atomic_add_64
#else
#pragma weak atomic_add_long = emul_atomic_add_32
#endif

#pragma weak atomic_or_uint = emul_atomic_or_uint
#pragma weak atomic_or_32 = emul_atomic_or_uint
#pragma weak atomic_and_uint = emul_atomic_and_uint
#pragma weak atomic_and_32 = emul_atomic_and_uint

#pragma weak atomic_add_16_nv = emul_atomic_add_16_nv
#pragma weak atomic_add_32_nv = emul_atomic_add_32_nv
#pragma weak atomic_add_64_nv = emul_atomic_add_64_nv
#ifdef _LP64
#pragma weak atomic_add_long_nv = emul_atomic_add_64_nv
#else
#pragma weak atomic_add_long_nv = emul_atomic_add_32_nv
#endif

#pragma weak cas32 = emul_cas32
#pragma weak cas64 = emul_cas64
#ifdef _LP64
#pragma weak casptr = emul_cas64
#pragma weak caslong = emul_cas64
#else
#pragma weak casptr = emul_cas32
#pragma weak caslong = emul_cas32
#endif

void
emul_atomic_add_16(uint16_t *target, int16_t delta)
{
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	*target += delta;
	lock_clear_splx(lp, s);
}

void
emul_atomic_add_32(uint32_t *target, int32_t delta)
{
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	*target += delta;
	lock_clear_splx(lp, s);
}

void
emul_atomic_add_64(uint64_t *target, int64_t delta)
{
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	*target += delta;
	lock_clear_splx(lp, s);
}

void
emul_atomic_or_uint(uint_t *target, uint_t bits)
{
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	*target |= bits;
	lock_clear_splx(lp, s);
}

void
emul_atomic_and_uint(uint_t *target, uint_t bits)
{
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	*target &= bits;
	lock_clear_splx(lp, s);
}

uint16_t
emul_atomic_add_16_nv(uint16_t *target, int16_t delta)
{
	uint16_t new;
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	new = (*target += delta);
	lock_clear_splx(lp, s);
	return (new);
}

uint32_t
emul_atomic_add_32_nv(uint32_t *target, int32_t delta)
{
	uint32_t new;
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	new = (*target += delta);
	lock_clear_splx(lp, s);
	return (new);
}

uint64_t
emul_atomic_add_64_nv(uint64_t *target, int64_t delta)
{
	uint64_t new;
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	new = (*target += delta);
	lock_clear_splx(lp, s);
	return (new);
}

uint32_t
emul_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
{
	uint32_t old;
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	if ((old = *target) == cmp)
		*target = new;
	lock_clear_splx(lp, s);
	return (old);
}

uint64_t
emul_cas64(uint64_t *target, uint64_t cmp, uint64_t new)
{
	uint64_t old;
	lock_t *lp = ATOMIC_LOCK(target);
	ushort_t s;

	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &s);
	if ((old = *target) == cmp)
		*target = new;
	lock_clear_splx(lp, s);
	return (old);
}
