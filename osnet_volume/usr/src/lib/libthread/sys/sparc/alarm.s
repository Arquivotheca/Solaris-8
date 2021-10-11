/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)alarm.s	1.9	97/11/06 SMI"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.file "alarm.s"
/*
 * unsigned
 * _alarm(unsigned t)
 */

	ENTRY(__alarm);
	SYSTRAP(alarm)
	RET
	SET_SIZE(__alarm)

/*
 * unsigned
 * _lwp_alarm(unsigned t)
 */

	ENTRY(__lwp_alarm);
	SYSTRAP(lwp_alarm)
	RET
	SET_SIZE(__lwp_alarm)

/*
 * setitimer(which, v, ov)
 *	int which;
 *	struct itimerval *v, *ov;
 */
	ENTRY(__setitimer)
	SYSTRAP(setitimer)
	RET
	SET_SIZE(__setitimer)
