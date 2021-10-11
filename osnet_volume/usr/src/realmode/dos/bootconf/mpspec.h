/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * mpspec.h -- public definitions for MP Spec. related routines
 */

#ifndef	_MPSPEC_H
#define	_MPSPEC_H

#ident "@(#)mpspec.h   1.1   99/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
int mps_find_bus_res(int bus, int type, struct range **res);
u_char far *find_sig(u_char far *cp, long len, char *sig);

#ifdef	__cplusplus
}
#endif

#endif	/* _MPSPEC_H */
