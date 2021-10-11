/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)waitid.s	1.8	98/02/27 SMI"	/* SVr4.0 1.1	*/

/* C library -- waitid						*/
/* int waitid(idtype_t idtype, id_t id, siginfo_t *infop,
	int options)						*/

	.file	"waitid.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_waitid;
	.type   _libc_waitid, #function
	_libc_waitid = waitid

	SYSREENTRY(waitid)
	SYSTRAP(waitsys)
	SYSRESTART(.restart_waitid)
	RET

	SET_SIZE(waitid)
