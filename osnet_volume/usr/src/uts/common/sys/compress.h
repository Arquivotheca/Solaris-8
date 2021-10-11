/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_COMPRESS_H
#define	_SYS_COMPRESS_H

#pragma ident	"@(#)compress.h	1.1	98/06/03 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern size_t compress(void *, void *, size_t);
extern size_t decompress(void *, void *, size_t, size_t);
extern uint32_t checksum32(void *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_COMPRESS_H */
