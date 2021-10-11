/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_WCHAR_IMPL_H
#define	_WCHAR_IMPL_H

#pragma ident	"@(#)wchar_impl.h	1.3	99/07/26 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_MBSTATET_H
#define	_MBSTATET_H
typedef struct __mbstate_t {
#if defined(_LP64)
	long	__filler[4];
#else
	int	__filler[6];
#endif
} __mbstate_t;
#endif	/* _MBSTATET_H */

#ifdef	__cplusplus
}
#endif

#endif	/* _WCHAR_IMPL_H */
