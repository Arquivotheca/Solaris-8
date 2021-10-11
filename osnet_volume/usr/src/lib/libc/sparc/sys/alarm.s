/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)alarm.s	1.7	96/03/08 SMI"	/* SVr4.0 1.7	*/

/* C library -- alarm						*/
/* unsigned alarm(unsigned seconds)				*/

	.file	"alarm.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_alarm)
	SYSTRAP(alarm)

	RET

	SET_SIZE(_libc_alarm)
