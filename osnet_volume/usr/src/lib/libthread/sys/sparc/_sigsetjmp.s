/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_sigsetjmp.s	1.4	98/08/07 SMI"

#include <sys/asm_linkage.h>
#include "assym.h"

	.file "_sigsetjmp.s"
/*
 * void
 * _sigsetjmp(sigjmp_buf env, int savemask)
 */

	.weak sigsetjmp;
	.type sigsetjmp, #function;
	sigsetjmp = _sigsetjmp;

	.weak _ti_sigsetjmp;
	.type _ti_sigsetjmp, #function;
	_ti_sigsetjmp = _sigsetjmp;

	ENTRY(_sigsetjmp)
	stn	%sp, [%o0 + SJS_SP]	! save caller's sp into env->sjs_sp
	add	%o7, 8, %o2		! calculate caller's return pc
	stn	%o2, [%o0 + SJS_PC]	! save caller's pc into env->sjs_pc

	! The sparc ABI requires the 3 registers: %g2-%g4 to be reserved
	! for the application. Hence, they should be preserved across
	! an application's call to sigsetjmp()/siglongjmp().  Likewise, 
	! the sparc V9 ABI requires the 2 registers: %g2, %g3 to be reserved
	! for the application and therefore, they too should be preserved 
	! across an application's call to sigsetjmp()/siglongjmp().

	stn	%g2, [%o0 + SJS_G2]	! save caller's %g2 into env->sjs_g2
	stn	%g3, [%o0 + SJS_G3]	! save caller's %g3 into env->sjs_g3
#ifndef __sparcv9
	st	%g4, [%o0 + SJS_G4]	! save caller's %g4 into env->sjs_g4
#endif /* __sparcv9 */

	call	__csigsetjmp
	sub	%o2, 8, %o7		! __csigsetjmp returns to caller
	SET_SIZE(_sigsetjmp)
