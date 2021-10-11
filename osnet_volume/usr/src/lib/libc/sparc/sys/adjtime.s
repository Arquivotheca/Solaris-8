/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)adjtime.s	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C Library -- adjtime						*/
/* int adjtime(struct timeval *delta, struct timeval *olddelta)	*/

	.file	"adjtime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(adjtime,function)

#include "SYS.h"

	SYSCALL(adjtime)
	RETC

	SET_SIZE(adjtime)
