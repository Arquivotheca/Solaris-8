/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LOADAVG_H
#define	_SYS_LOADAVG_H

#pragma ident	"@(#)loadavg.h	1.1	97/12/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	LOADAVG_1MIN	0
#define	LOADAVG_5MIN	1
#define	LOADAVG_15MIN	2

#define	LOADAVG_NSTATS	3

#ifdef _KERNEL

extern int getloadavg(int *, int);

#else	/* _KERNEL */

/*
 * This is the user API
 */
extern int getloadavg(double [], int);

/*
 * This is the system call that implements it.
 * Do not invoke this directly.
 */
extern int __getloadavg(int *, int);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_LOADAVG_H */
