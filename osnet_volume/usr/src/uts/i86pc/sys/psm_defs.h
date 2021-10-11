/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PSM_DEFS_H
#define	_SYS_PSM_DEFS_H

#pragma ident	"@(#)psm_defs.h	1.3	98/01/06 SMI"

/*
 * Platform Specific Module Definitions
 */

#include <sys/pic.h>
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
typedef	void *opaque_t;
#else	/* __STDC__ */
typedef	char *opaque_t;
#endif	/* __STDC__ */

/*
 *	External Kernel Interface
 */

extern void picsetup(void);	/* isp initialization 			*/
extern u_longlong_t mul32(ulong_t a, ulong_t b);
				/* u_long_long = ulong_t x ulong_t */

/*
 *	External Kernel Reference Data
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_DEFS_H */
