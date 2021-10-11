/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)stime.s	1.6	92/07/14 SMI"	/* SVr4.0 1.6	*/

/*  C library -- stime						*/
/* int stime (const time_t *tp);				*/

	.file	"stime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(stime,function)

#include "SYS.h"

	ENTRY(stime)
	ld	[%o0], %o0
	SYSTRAP(stime)
	SYSCERROR
	RETC

	SET_SIZE(stime)
