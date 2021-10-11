/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

/* C library -- getgid						*/
/* int getitimer (int, struct itimerval *value);		*/

	.file	"getitimer.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getitimer,function)

#include "SYS.h"

	SYSCALL(getitimer)
	RET

	SET_SIZE(getitimer)
