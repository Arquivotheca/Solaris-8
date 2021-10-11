/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hrestime.s	1.8	97/11/06 SMI"

#include <sys/asm_linkage.h>
#include "SYS.h"

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
