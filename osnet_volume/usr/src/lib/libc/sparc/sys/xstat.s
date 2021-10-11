/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)xstat.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- xstat						*/
/* int xstat(int version, const char *path, struct stat *buf)	*/

	.file	"xstat.s"

#include "SYS.h"

	ENTRY(_xstat)
	SYSTRAP(xstat)
	SYSCERROR
	RETC

	SET_SIZE(_xstat)
