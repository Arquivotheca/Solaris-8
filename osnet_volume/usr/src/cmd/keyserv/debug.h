/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _KEYSERV_DEBUG_H
#define	_KEYSERV_DEBUG_H

#pragma ident	"@(#)debug.h	1.1	97/11/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
#define	DEBUG
*/

#ifdef DEBUG
typedef enum {
	KEYSERV_DEBUG0 = 1,
	KEYSERV_DEBUG1,
	KEYSERV_DEBUG,
	KEYSERV_INFO,
	KEYSERV_PANIC
} debug_level;

extern int debugging;

#define	debug(x, y) (test_debug(x, __FILE__, __LINE__) && real_debug ## y)
#else
#define	debug(x, y)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _KEYSERV_DEBUG_H */
