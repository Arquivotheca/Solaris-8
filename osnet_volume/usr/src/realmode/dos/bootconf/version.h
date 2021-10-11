/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * version.h -- public definitions for version routines
 */

#ifndef	_VERSION_H
#define	_VERSION_H

#ident "@(#)version.h   1.7   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */

void set_prop_version(void);

/*
 * These defines should be updated whenever the bootconf interface changes.
 * This will be the version that is returned to /dev/openprom in response
 * to a request for the openboot version.
 */
#define	MAJOR_VERSION 1
#define	MINOR_VERSION 0

#ifdef	__cplusplus
}
#endif

#endif	/* _VERSION_H */
