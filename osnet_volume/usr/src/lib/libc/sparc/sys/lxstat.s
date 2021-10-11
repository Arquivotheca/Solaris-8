/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lxstat.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- lxstat						*/
/* int _lxstat(int version, const char *path, struct lstat *buf)*/

	.file	"lxstat.s"

#include "SYS.h"

	ENTRY(_lxstat)
	SYSTRAP(lxstat)
	SYSCERROR
	RETC

	SET_SIZE(_lxstat)
