/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sysconfig.s	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- _sysconfig					*/
/* int _sysconfig(name)						*/

	.file	"sysconfig.s"

#include "SYS.h"

	ENTRY(_sysconfig)
	SYSTRAP(sysconfig)
	SYSCERROR
	RET

	SET_SIZE(_sysconfig)
