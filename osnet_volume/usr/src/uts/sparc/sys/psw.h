/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PSW_H
#define	_SYS_PSW_H

#pragma ident	"@(#)psw.h	1.17	94/11/19 SMI" /* from SunOS psl.h 1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file only exists for v7 backwards compatibility.
 * Kernel code shoulf not include it.
 */
#ifdef _KERNEL
#error Kernel include of psw.h
#else

#include <v7/sys/psr.h>

/*
 * The following defines are obsolete; they exist only for existing
 * application compatibility.
 */
#define	SR_SMODE	PSR_PS

/*
 * Macros to decode psr.
 *
 * XXX - note that AT&T's usage of BASEPRI() is reversed from ours
 * (i.e. (!BASEPRI(ps)) means that you *are* at the base priority).
 */
#define	BASEPRI(ps)	(((ps) & PSR_PIL) == 0)

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSW_H */
