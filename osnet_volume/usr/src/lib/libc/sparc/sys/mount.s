/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)mount.s	1.5	92/07/14 SMI"	/* SVr4.0 1.6	*/

/* C library -- mount						*/
/* int mount(const char *spec, const char *dir, int mflag,
	int fstype, const char *dataptr, int datalen);		*/

	.file	"mount.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mount,function)

#include "SYS.h"

	SYSCALL(mount)
	RETC

	SET_SIZE(mount)
