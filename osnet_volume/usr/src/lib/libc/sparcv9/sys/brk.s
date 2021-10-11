/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

	.ident	"@(#)brk.s	1.1	97/07/04 SMI"
	.file	"brk.s"

#include <sys/asm_linkage.h>

#include "SYS.h"
#ifdef PIC
#include "PIC.h"
#endif

#ifndef	DSHLIB
	.section	".data"
	.global end
	.global _nd
_nd:
	.xword	end
#endif	/* DSHLIB */

/*
 * _brk_unlocked() simply traps into the kernel to set the brk.  It
 * returns 0 if the break was successfully set, or -1 otherwise.
 * It doesn't enforce any alignment and it doesn't perform any locking.
 * _brk_unlocked() is only called from brk() and _sbrk_unlocked().
 */
	ENTRY_NP(_brk_unlocked)
	mov	%o0, %o2		! stash away new break
	SYSTRAP(brk)
	SYSCERROR
#ifdef PIC
	PIC_SETUP(o5)
	sethi	%hi(_nd), %g1
	or	%g1, %lo(_nd), %g1
	ldn	[%o5 + %g1], %g1
	stn	%o2, [%g1]		! write new break
#else
	setnhi	_nd, %o5, %g1
	stn	%o2, [%g1 + %lo(_nd)]	! write new break
#endif
	RETC

	SET_SIZE(_brk_unlocked)
