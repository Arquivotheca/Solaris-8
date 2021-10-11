/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)_range.h	1.7	97/03/28 SMI"	/* SVr4.0 1.1.2.1 */

#define	valid_range(c1, c2) \
	(((c1) & WCHAR_CSMASK) == ((c2) & WCHAR_CSMASK) && \
	((c1) > 0xff || !iscntrl((int)c1)) && ((c2) > 0xff || \
	!iscntrl((int)c2)))
