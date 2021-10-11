/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MERGE_H
#define	_MERGE_H

#pragma ident	"@(#)merge.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <fcntl.h>

#include "fields.h"
#include "initialize.h"
#include "streams.h"
#include "types.h"
#include "utility.h"

extern void merge(sort_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _MERGE_H */
