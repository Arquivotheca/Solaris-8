/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fxstat.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- _fxstat						*/
/* int _fxstat(int version, int fildes, struct stat *buf)	*/

	.file	"fxstat.s"

#include "SYS.h"

	ENTRY(_fxstat)
	SYSTRAP(fxstat)
	SYSCERROR
	RETC

	SET_SIZE(_fxstat)
