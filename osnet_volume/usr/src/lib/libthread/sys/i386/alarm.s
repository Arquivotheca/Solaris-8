/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)alarm.s	1.4 95/03/15	SMI" /* SVr4.0 1.9 */

#include <sys/asm_linkage.h>
#include "i386/SYS.h"

	.file "alarm.s"
/*
 * void
 * __alarm(t)
 */

	fwdef(_alarm)
	SYSTRAP(alarm)
	ret

/*
 * void
 * __lwp_alarm(t)
 */

	fwdef(_lwp_alarm)
	SYSTRAP(lwp_alarm)
	ret

/*
 * __setitimer(which, v, ov)
 *	int which;
 *	struct itimerval *v, *ov;
 */
	fwdef(_setitimer)
	SYSTRAP(setitimer)
	ret
