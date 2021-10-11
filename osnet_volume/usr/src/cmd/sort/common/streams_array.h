/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SORT_STREAMS_ARRAY_H
#define	_SORT_STREAMS_ARRAY_H

#pragma ident	"@(#)streams_array.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fields.h"
#include "types.h"
#include "utility.h"

extern const stream_ops_t stream_array_ops;

#ifdef	__cplusplus
}
#endif

#endif	/* _SORT_STREAMS_ARRAY_H */
