/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CONDVAR_IMPL_H
#define	_SYS_CONDVAR_IMPL_H

#pragma ident	"@(#)condvar_impl.h	1.2	97/04/04 SMI"

/*
 * Implementation-private definitions for condition variables
 */

#ifndef	_ASM
#include <sys/types.h>
#include <sys/thread.h>
#endif	/* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * Condtion variables.
 */

typedef struct _condvar_impl {
	ushort_t	cv_waiters;
} condvar_impl_t;

#define	CV_HAS_WAITERS(cvp)	(((condvar_impl_t *)(cvp))->cv_waiters != 0)

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONDVAR_IMPL_H */
