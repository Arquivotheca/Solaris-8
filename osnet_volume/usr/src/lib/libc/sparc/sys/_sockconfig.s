/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_sockconfig.s	1.2	96/06/13 SMI"

/* C library -- _sockconfig					*/
/*
 * int _sockconfig (int domain, int type, int protocol,
 *			dev_t dev, int version);
 */

	.file	"_sockconfig.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_sockconfig,sockconfig)
	RET

	SET_SIZE(_sockconfig)
