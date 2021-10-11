/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 * Copyright (c) 1989 by Nihon Sun Microsystems K.K.
 */

#ifndef	_GETWIDTH_H
#define	_GETWIDTH_H

#pragma ident	"@(#)getwidth.h	1.4	92/07/14 SMI"

#include <euc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
extern void getwidth(eucwidth_t *);
#else	/* __STDC__ */
extern void getwidth();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _GETWIDTH_H */
