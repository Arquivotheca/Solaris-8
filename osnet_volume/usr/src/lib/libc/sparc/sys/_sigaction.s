/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)_sigaction.s	1.5	92/07/14 SMI"	/* SVr4.0 1.4	*/

/* C library -- sigaction					*/
/* int sigaction (int sig, struct sigaction *act);		*/

	.file	"_sigaction.s"

#include "SYS.h"

	ENTRY(__sigaction)
	SYSTRAP(sigaction)
	SYSCERROR
	RET

	SET_SIZE(__sigaction)
