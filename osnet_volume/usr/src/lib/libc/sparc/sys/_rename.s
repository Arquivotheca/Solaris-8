/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)_rename.s	1.5	92/07/14 SMI"	/* SVr4.0 1.3	*/

/* C library -- _rename						*/
/* _rename() is the system call version of rename()		*/

	.file	"_rename.s"

#include "SYS.h"

	ENTRY(_rename)
	SYSTRAP(rename)
	SYSCERROR
	RETC

	SET_SIZE(_rename)
