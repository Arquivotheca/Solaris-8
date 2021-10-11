/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)hrestime.s	1.2 94/11/03	SMI" /* SVr4.0 1.9 */

#include <sys/asm_linkage.h>
#include "i386/SYS.h"

	.file	"hrestime.s"

#define	ST_HRESTIME	0x24

/*
 * hrestime(tval)
 *	timestruc_t *tval;
 */
	ENTRY(hrestime);
	ta	ST_HRESTIME
	nop
	st	%g2, [%o0]	! secs
	st	%g3, [%o0+4]	! nsecs
	retl
	nop
	SET_SIZE(hrestime)
