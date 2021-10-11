/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fdsync.s 93/03/10	1.3 SMI"	

/* Underlying function for C library(fsync) and POSIX(fdatasync)*/
/* int fdsync(int fildes,int flag)				*/

	.file	"fdsync.s"


#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(__fdsync)
	SYSTRAP(fdsync)
	SYSCERROR
	RETC

	SET_SIZE(__fdsync)
