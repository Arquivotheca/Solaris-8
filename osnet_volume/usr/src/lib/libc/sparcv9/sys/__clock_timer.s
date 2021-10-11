/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__clock_timer.s	1.5	97/12/23 SMI"

#include <sys/asm_linkage.h>
#include <sys/time_impl.h>
#include "SYS.h"

	.file "__clock_timer.s"


/*
 * int
 * __clock_getres(clockid_t clock_id, struct timespec *res)
 */

	ENTRY(__clock_getres)
	SYSTRAP(clock_getres)
	SYSCERROR
	RET
	SET_SIZE(__clock_getres)

/*
 * int
 * __clock_gettime(clockid_t clock_id, timespec_t *tp)
 */

	ENTRY(__clock_gettime)
	cmp	%o0, __CLOCK_REALTIME0		! if (clock_id)
	beq	2f
	cmp	%o0, __CLOCK_REALTIME3
	bne	1f				! equal to CLOCK_REALTIME
	.empty					! optimize for CLOCK_REALTIME
2:
	mov	%o1, %o5			! %o1 = address of timespec
	ta	ST_GETHRESTIME			! call get_hrestime
	stn	%o0, [%o5]			! *tp = hrest
	stn	%o1, [%o5 + CLONGSIZE]
	retl
	clr	%o0
1:
	SYSTRAP(clock_gettime)
	SYSCERROR
	RET
	SET_SIZE(__clock_gettime)

/*
 * int
 * __clock_settime(clockid_t clock_id, timespec_t *tp)
 */

	ENTRY(__clock_settime)
	SYSTRAP(clock_settime)
	SYSCERROR
	RET
	SET_SIZE(__clock_settime)

/*
 * int
 * __timer_create(clockid_t clock_id, struct sigevent *evp, timer_t *timerid)
 */

	ENTRY(__timer_create);
	SYSTRAP(timer_create);
	SYSCERROR
	RET
	SET_SIZE(__timer_create)

/*
 * int
 * __timer_delete(timer_t timerid)
 */

	ENTRY(__timer_delete);
	SYSTRAP(timer_delete);
	SYSCERROR
	RET
	SET_SIZE(__timer_delete)

/*
 * int
 * __timer_getoverrun(timer_t timerid)
 */

	ENTRY(__timer_getoverrun);
	SYSTRAP(timer_getoverrun);
	SYSCERROR
	RET
	SET_SIZE(__timer_getoverrun)

/*
 * int
 * __timer_gettime(timer_t timerid, struct itimerspec *value)
 */

	ENTRY(__timer_gettime);
	SYSTRAP(timer_gettime);
	SYSCERROR
	RET
	SET_SIZE(__timer_gettime)

/*
 * int
 * __timer_settime(timer_t timerid, int flags,
 *	const struct itimerspec *value, struct itimerspec *ovalue)
 */

	ENTRY(__timer_settime);
	SYSTRAP(timer_settime);
	SYSCERROR
	RET
	SET_SIZE(__timer_settime)

/*
 * int
 * _libc_nanosleep(const struct timespec *rqtp,	struct timespec *rqtp)
 */

	ENTRY(_libc_nanosleep);
	SYSTRAP(nanosleep);
	SYSCERROR
	RET
	SET_SIZE(_libc_nanosleep)
