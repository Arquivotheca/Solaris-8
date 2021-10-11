/*
 * Copyright (c) 1985,1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DLTEST_H
#define	_DLTEST_H

#pragma ident	"@(#)dltest.h	1.3	98/02/12 SMI"

/*
 * Common DLPI Test Suite header file
 *
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum control/data buffer size (in long's !!) for getmsg().
 */
#define		MAXDLBUF	8192

/*
 * Maximum number of seconds we'll wait for any
 * particular DLPI acknowledgment from the provider
 * after issuing a request.
 */
#define		MAXWAIT		15

/*
 * Maximum address buffer length.
 */
#define		MAXDLADDR	1024


/*
 * Handy macro.
 */
#define		OFFADDR(s, n)	(uchar_t *)((char *)(s) + (int)(n))

#ifdef __cplusplus
}
#endif

#endif /* _DLTEST_H */
