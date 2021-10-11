/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_STDIO_H
#define	_STREAMS_STDIO_H

#pragma ident	"@(#)streams_stdio.h	1.1	98/12/14 SMI"

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

extern int stream_stdio_open_for_write(stream_t *str);
extern int stream_stdio_is_closable(stream_t *str);
extern int stream_stdio_close(stream_t *str);
extern int stream_stdio_unlink(stream_t *str);
extern int stream_stdio_free(stream_t *str);

extern ssize_t stream_stdio_fetch_overwrite(stream_t *);
extern void stream_stdio_put_line_unique(stream_t *, line_rec_t *);
extern void stream_stdio_flush(stream_t *);

extern const stream_ops_t stream_stdio_ops;

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_STDIO_H */
