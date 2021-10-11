/*
 * Copyright (c) 1992-1993,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SPL_H
#define	_SYS_SPL_H

#pragma ident	"@(#)spl.h	1.3	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convert system interrupt priorities (0-7) into a psr for splx.
 * In general, the processor priority (0-15) should be 2 times
 * the system pririty.
 */
#define	pritospl(n)	((n) << 1)

/*
 * on x86 platform these are identity functions
 */
#define	ipltospl(n)	(n)
#define	spltoipl(n)	(n)
#define	spltopri(n)	(n)

/*
 * Hardware spl levels
 * it should be replace by the appropriate interrupt class info.
 */
#define	SPL8    15
#define	SPL7    13

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPL_H */
