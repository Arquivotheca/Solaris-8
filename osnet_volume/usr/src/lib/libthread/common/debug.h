/*	Copyright (c) 1993, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _DEBUG_H
#define	_DEBUG_H

#pragma ident	"@(#)debug.h	1.13	95/03/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef DEBUG
extern int dbg;

#define	ASSERT(EX)\
	((void)((dbg == 0 || (EX)) || _assfail(#EX, __FILE__, __LINE__)))
#else /* DEBUG */
#define	ASSERT(EX)
#endif /* DEBUG */

#ifdef	__cplusplus
}
#endif

#endif /* _DEBUG_H */
