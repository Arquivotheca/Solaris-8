/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock.s	1.4	98/04/14 SMI"

#include <sys/asm_linkage.h>

/*
 * lock_try(lp)
 *	- returns non-zero on success.
 */
	ENTRY(_lock_try)
	ldstub	[%o0], %o1		! try to set lock, get value in %o1
	membar	#LoadLoad
	retl
	  xor	%o1, 0xff, %o0		! delay - return non-zero if success
	SET_SIZE(_lock_try)

/*
 * lock_clear(lp)
 *	- clear lock.  Waiter resident in lock word. 
 */
	ENTRY(_lock_clear)
	membar	#LoadStore|#StoreStore
	retl
	  clrb	[%o0]
	SET_SIZE(_lock_clear)
