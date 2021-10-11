/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PCSAMPLE_H
#define	_PCSAMPLE_H

#pragma ident	"@(#)pcsample.h	1.1	98/01/29 SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PC sampling profiling
 */
#ifdef __STDC__
long pcsample(uintptr_t [], long);
#else
long pcsample();
#endif /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif	/* _PCSAMPLE_H */
