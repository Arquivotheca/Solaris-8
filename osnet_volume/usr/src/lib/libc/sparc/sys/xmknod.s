/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)xmknod.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- xmknod						*/
/* int xmknod(int version, const char *path, mode_t mode,
	dev_t  dev)						*/

	.file	"xmknod.s"

#include "SYS.h"

	ENTRY(_xmknod)
	SYSTRAP(xmknod)
	SYSCERROR
	RETC

	SET_SIZE(_xmknod)
