/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _ULIMIT_H
#define	_ULIMIT_H

#pragma ident	"@(#)ulimit.h	1.5	92/07/14 SMI"	/* SVr4.0 1.4	*/

#include <sys/ulimit.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern long ulimit(int, ...);
#else
extern long ulimit();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ULIMIT_H */
