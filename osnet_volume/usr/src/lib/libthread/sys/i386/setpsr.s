/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


.ident	"@(#)setpsr.s	1.2 94/11/03	SMI" /* SVr4.0 1.9 */

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include "i386/SYS.h"

	.file "setpsr.s"
/*
 * int
 * getpsr ()
 */

	ta	ST_GETPSR
	nop

/*
 * setpsr (psr)
 */
	ta	ST_SETPSR
	nop
