/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#ifndef _SYS_SPL_H
#define	_SYS_SPL_H

#pragma ident	"@(#)spl.h	1.3	98/11/17 SMI" /* from SunOS psl.h 1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	__sparcv9cpu
/*
 * Convert a hardware interrupt priority level (0-15) into a psr for splx.
 * Also are macros to convert back.
 */
#define	ipltospl(n)	((n) << 8)
#define	spltoipl(n)	(((n) >> 8) & 0xf)
#else
/*
 * v9 spl and ipl are identical since pil is a separate register.
 */
#define	ipltospl(n)	(n)
#define	spltoipl(n)	(n)
#endif

/*
 * Hardware spl levels
 * XXX - This is a hack for softcall to block all i/o interrupts.
 * XXX - SPL5 and SPL3 are hacks for the latest zs code.
 * it should be replace by the appropriate interrupt class info.
 */
#define	SPL8    15
#define	SPL7    13
#define	SPL5    12
#define	SPLTTY  SPL5
#define	SPL3    6

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPL_H */
