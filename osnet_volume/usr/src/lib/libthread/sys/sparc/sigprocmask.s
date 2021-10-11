/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigprocmask.s	1.4	97/11/06 SMI"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.file "sigprocmask.s"
/*
 * void
 * __sigprocmask(how, set, oset)
 */

	ENTRY(__sigprocmask);
	SYSTRAP(sigprocmask)
	RET
	SET_SIZE(__sigprocmask)
