/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)setpgrp.s	1.6	92/07/14 SMI"	/* SVr4.0 1.7	*/

/* C library -- setpgrp, getpgrp				*/
/* pid_t setpgrp (void);					*/
/* pid_t getpgrp (void);					*/

	.file	"setpgrp.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setpgrp,function)
	ANSI_PRAGMA_WEAK(getpgrp,function)

#include "SYS.h"

/*	getpgrp()	: syscall(SYS_pgrpsys,0)
 *	setpgrp()	: syscall(SYS_pgrpsys,1)
 */

#define	SUBSYS_getpgrp	0
#define	SUBSYS_setpgrp	1

	ENTRY(setpgrp)
	b	.pgrp
	mov	SUBSYS_setpgrp, %o0

	ENTRY(getpgrp)
	mov	SUBSYS_getpgrp, %o0

.pgrp:
	SYSTRAP(pgrpsys)
	SYSCERROR
	RET

	SET_SIZE(setpgrp)
	SET_SIZE(getpgrp)
