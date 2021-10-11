/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_COMMON_H
#define	_STREAMS_COMMON_H

#pragma ident	"@(#)streams_common.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "types.h"

#define	STDIO_VBUF_SIZE	(16 * KILOBYTE)

extern void stream_set(stream_t *, flag_t);
extern void stream_unset(stream_t *, flag_t);

extern void stream_unlink_temporaries(stream_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_COMMON_H */
