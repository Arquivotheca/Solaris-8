/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)exit.s	1.8	92/07/14 SMI"	/* SVr4.0 1.4	*/

/* C library -- _exit						*/
/* void _exit(int status)
 *
 * code is return in r0 to system
 * Same as plain exit, for user who want to define their own exit.
 */

	.file	"exit.s"

#include "SYS.h"

	ENTRY(_exit)
	SYSTRAP(exit)

	SET_SIZE(_exit)
