/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_WIDE_H
#define	_STREAMS_WIDE_H

#pragma ident	"@(#)streams_wide.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fields.h"
#include "streams_stdio.h"
#include "types.h"
#include "utility.h"

extern ssize_t stream_wide_fetch_overwrite(stream_t *);
extern void stream_wide_put_line_unique(stream_t *, line_rec_t *);

extern const stream_ops_t stream_wide_ops;

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_WIDE_H */
