/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)modctl.s	1.3	92/07/14 SMI"	/* SVr4.0 1.0	*/

/* C library -- modctl						*/
/* int modctl(int opcode, char *arg)				*/

	.file	"modctl.s"

#include "SYS.h"

	ENTRY(modctl)
	SYSTRAP(modctl)
	SYSCERROR
	RET

	SET_SIZE(modctl)
