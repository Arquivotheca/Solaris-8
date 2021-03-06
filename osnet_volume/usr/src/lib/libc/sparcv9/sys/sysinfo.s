/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sysinfo.s	1.1	96/12/04 SMI"	/* SVr4.0 1.2	*/

/* C library -- sysinfo						*/
/* int sysinfo(cmd, buf, count)					*/

	.file	"sysinfo.s"

#if	!defined(ABI) && !defined(DSHLIB)

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sysinfo,function)

#endif	/* !defined(ABI) && !defined(DSHLIB) */

#include "SYS.h"

	ENTRY(sysinfo)
	SYSTRAP(systeminfo)
	SYSCERROR
	RET

	SET_SIZE(sysinfo)
