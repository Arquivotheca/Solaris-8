/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_H
#define	_STREAMS_H

#pragma ident	"@(#)streams.h	1.2	99/01/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fields.h"
#include "types.h"
#include "streams_array.h"
#include "streams_common.h"
#include "streams_mmap.h"
#include "streams_stdio.h"
#include "streams_wide.h"
#include "utility.h"

#define	ST_MEM_FILLED	0x0	/* no memory left; proceed to internal sort */
#define	ST_MEM_AVAIL	0x1	/* memory left for sort; take add'l input */

#define	ST_NOCACHE	0x0	/* write sorted array to temporary file */
#define	ST_CACHE	0x1	/* keep sorted array in memory */
#define	ST_OPEN		0x2	/* create open temporary file */
#define	ST_WIDE		0x4	/* write multibyte chars to temporary file */

extern void stream_add_file_to_chain(stream_t **, char *);
extern void stream_clear(stream_t *);
extern void stream_close_all_previous(stream_t *);
extern ssize_t stream_count_chain(stream_t *);
extern void stream_dump(stream_t *, stream_t *, flag_t);
extern int stream_eos(stream_t *);
extern int stream_insert(sort_t *, stream_t *, stream_t *);
extern stream_t *stream_new(int);
extern int stream_open_for_read(sort_t *, stream_t *);
extern void stream_push_to_chain(stream_t **, stream_t *);
extern stream_t *stream_push_to_temporary(char *, stream_t **, stream_t *, int);
extern void stream_set_size(stream_t *, size_t);
extern void stream_stat_chain(stream_t *);
extern void stream_swap_buffer(stream_t *, char **, size_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_H */
