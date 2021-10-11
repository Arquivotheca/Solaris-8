/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_KGSSAPI_DEFS_H
#define	_KGSSAPI_DEFS_H

#pragma ident	"@(#)kgssapi_defs.h	1.8	98/06/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern kmutex_t gssrpcb_lock;
extern kmutex_t	__kgss_mech_lock;

/*
 * GSSLOG debugging for kgss module.
 */
#if defined(DEBUG) && !defined(GSSDEBUG)
#define	GSSDEBUG
#endif

#ifdef	GSSDEBUG
extern uint_t gss_log;
#include <sys/cmn_err.h>

#define	GSSLOG(A, B, C) \
	((void)((gss_log & (A)) && (printf((B), (C)), TRUE)))
#define	GSSLOG0(A, B)   \
	((void)((gss_log & (A)) && (printf(B), TRUE)))
#else
#define	GSSLOG(A, B, C)
#define	GSSLOG0(A, B)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _KGSSAPI_DEFS_H */
