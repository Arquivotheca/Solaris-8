/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1998,1999, by Sun Microsystems, Inc.	*/
/*	All rights reserved.					*/

.ident	"@(#)_rpcsys.s	1.1	99/07/28 SMI"

/* rpcsys						*/
/* int rpcsys(int opcode, int arg)				*/

	.file	"_rpcsys.s"

#include "SYS.h"

	ENTRY(_rpcsys)
	SYSTRAP(rpcsys)
	SYSCERROR
	RETC

	SET_SIZE(_rpcsys)
