/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * hrt.h -- public definitions for HRT related routines
 */

#ifndef	_HRT_H
#define	_HRT_H

#ident "@(#)hrt.h   1.1   99/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
int hrt_find_bus_res(int bus, int type, struct range **res);
int hrt_find_bus_range(int bus);

#ifdef	__cplusplus
}
#endif

#endif	/* _HRT_H */
