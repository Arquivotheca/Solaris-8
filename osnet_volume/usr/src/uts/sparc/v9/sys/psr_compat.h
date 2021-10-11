/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PSR_COMPAT_H
#define	_SYS_PSR_COMPAT_H

#pragma ident	"@(#)psr_compat.h	1.4	94/11/16 SMI"
/* from SunOS psl.h 1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <v7/sys/psr.h>		/* the real thing */

/*
 * Handy defines for converting between pstate or tstate and psr.
 */
#define	PSR_ICC_SHIFT		20
#define	PSR_IMPLVER_SHIFT	24
#define	PSR_TSTATE_CC_SHIFT	12
#define	PSR_PSTATE_EF_SHIFT	8
#define	PSR_FPRS_FEF_SHIFT	10

/*
 * PSR VER|IMPL value assigned by Sparc International for V8 compatibility.
 */
#ifdef _ASM
#define	V9_IMPLVER	0xFE
#else
#define	V9_IMPLVER	0xFEU
#endif

#define	V9_PSR_IMPLVER	(V9_IMPLVER << PSR_IMPLVER_SHIFT)



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSR_COMPAT_H */
