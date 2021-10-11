/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INTERNAL_SORT_H
#define	_INTERNAL_SORT_H

#pragma ident	"@(#)internal.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/mman.h>

#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "streams.h"
#include "types.h"
#include "utility.h"

extern void internal_sort(sort_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _INTERNAL_SORT_H */
