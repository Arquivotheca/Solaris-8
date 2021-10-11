/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)setsid.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- setsid, setpgid, getsid, getpgid		*/
/* sid_t getsid(void)						*/
/* int setsid(void)						*/
/* pid_t getpgid(pid_t pid)					*/
/* int setpgid(pid_t pid, pgid_t pgid)				*/

	.file	"setsid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getsid,function)
	ANSI_PRAGMA_WEAK(setsid,function)
	ANSI_PRAGMA_WEAK(getpgid,function)
	ANSI_PRAGMA_WEAK(setpgid,function)

#include "SYS.h"

/*	getsid()	: syscall(SYS_pgrpsys,2,pid)
 *	setsid()	: syscall(SYS_pgrpsys,3)
 *	getpgid()	: syscall(SYS_pgrpsys,4,pid)
 *	setpgid()	: syscall(SYS_pgrpsys,5,pid,pgid)
 */

#define	SUBSYS_getsid	2
#define	SUBSYS_setsid	3
#define	SUBSYS_getpgid	4
#define	SUBSYS_setpgid	5

	ENTRY(getsid)
	mov	%o0, %o1
	ba	.pgrp
	mov	SUBSYS_getsid, %o0

	ENTRY(setsid)
	ba	.pgrp
	mov	SUBSYS_setsid, %o0

	ENTRY(getpgid)
	mov	%o0, %o1
	ba	.pgrp
	mov	SUBSYS_getpgid, %o0

	ENTRY(setpgid)
	mov	%o1, %o2
	mov	%o0, %o1
	ba	.pgrp
	mov	SUBSYS_setpgid, %o0

.pgrp:
	SYSTRAP(pgrpsys)
	SYSCERROR
	RET

	SET_SIZE(getsid)
	SET_SIZE(setsid)
	SET_SIZE(getpgid)
	SET_SIZE(setpgid)
