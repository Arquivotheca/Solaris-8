/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * mount.h -- public definitions for mount routines
 */

#ifndef	_MOUNT_H
#define	_MOUNT_H

#ident "@(#)mount.h   1.7   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "boot.h"

int try_mount(bef_dev *bdp, char *mnt_dev_desc);

#ifdef	__cplusplus
}
#endif

#endif	/* _MOUNT_H */
