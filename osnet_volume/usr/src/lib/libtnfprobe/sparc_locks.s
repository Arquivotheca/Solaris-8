#pragma ident  "@(#)sparc_locks.s 1.2 94/07/11 SMI"
/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#include <sys/asm_linkage.h>

	.file		__FILE__
/*
 * int tnfw_b_get_lock(tnf_byte_lock_t *);
 */
	ENTRY(tnfw_b_get_lock)
	ldstub	[%o0], %o1
	jmpl	%o7+8, %g0
	mov	%o1, %o0
	SET_SIZE(tnfw_b_get_lock)

/*
 * void tnfw_b_clear_lock(tnf_byte_lock_t *);
 */
	ENTRY(tnfw_b_clear_lock)
	jmpl	%o7+8, %g0
	stb	%g0, [%o0]
	SET_SIZE(tnfw_b_clear_lock)

/*
 * u_long tnfw_b_atomic_swap(u_long *, u_long);
 */
	ENTRY(tnfw_b_atomic_swap)
	swap	[%o0], %o1
	jmpl	%o7+8, %g0
	mov	%o1, %o0
	SET_SIZE(tnfw_b_atomic_swap)
