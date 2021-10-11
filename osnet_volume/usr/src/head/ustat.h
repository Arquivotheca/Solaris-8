/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _USTAT_H
#define	_USTAT_H

#pragma ident	"@(#)ustat.h	1.6	92/07/14 SMI"	/* SVr4.0 1.3.1.6 */

#include <sys/types.h>
#include <sys/ustat.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern int ustat(dev_t, struct ustat *);
#else
extern int ustat();
#endif	/* end defined(_STDC) */

#ifdef	__cplusplus
}
#endif

#endif	/* _USTAT_H */
