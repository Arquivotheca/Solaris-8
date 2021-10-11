/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

#pragma ident	"@(#)setitimer.s	1.5	96/03/08 SMI"

/* C library -- setitimer					*/
/* int setitimer (int, const struct itimerval *, struct itimerval *);	*/

	.file	"setitimer.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_setitimer);
	SYSTRAP(setitimer);
	SYSCERROR
	RET

	SET_SIZE(_libc_setitimer)
