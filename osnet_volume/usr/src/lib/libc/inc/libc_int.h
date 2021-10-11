/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LIBC_INT_H
#define	_LIBC_INT_H

#pragma ident	"@(#)libc_int.h	1.1 99/10/07"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Libc/rtld Runtime Interface
 */
#define	CI_NULL		0	/* (void) last entry */
#define	CI_VERSION	1	/* current version of ri_interface */
#define	CI_ATEXIT	2	/* _preexec_exit_handlers() address */

#define	CI_MAX		3

#define	CI_V_NONE	0	/* ci_version versions */
#define	CI_V_CURRENT	1	/* current version of libc interface */
#define	CI_V_NUM	2

/*
 * Libc to ld.so.1 interface communication structure.
 */
typedef struct {
	int	ci_tag;
	union {
		int (*	ci_func)();
		long	ci_val;
	} ci_un;
} Lc_interface;

/*
 * Address range returned via CI_ATEXIT.  Note, the address range array passed
 * back from ld.so.1 is maintained by ld.so.1 and should not be freed by libc.
 */
typedef struct {
	void *	lb;		/* lower bound */
	void *	ub;		/* upper bound */
} Lc_addr_range_t;


#ifdef	__cplusplus
}
#endif

#endif /* _LIBC_INT_H */
