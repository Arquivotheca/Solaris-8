/*
 * Copyright (c) 1991-1994,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IOCACHE_H
#define	_SYS_IOCACHE_H

#pragma ident	"@(#)iocache.h	1.23	98/01/20 SMI"

#ifndef _ASM
#include <sys/sysiosbus.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	OFF_STR_BUF_CTRL_REG	0x2800
#define	STR_BUF_CTRL_REG_SIZE	(NATURAL_REG_SIZE)
#define	OFF_STR_BUF_FLUSH_REG	0x2808
#define	STR_BUF_FLUSH_REG_SIZE	(NATURAL_REG_SIZE)
#define	OFF_STR_BUF_SYNC_REG	0x2810
#define	STR_BUF_SYNC_REG_SIZE	(NATURAL_REG_SIZE)
#define	STR_BUF_PAGE_TAG_DIAG	0x5800

#define	STREAM_BUF_DISABLE	0x0ull
#define	STREAM_BUF_ENABLE	0x1ull
#define	STREAM_BUF_DIAG_ENABLE	0x2ull
#define	IOCACHE_LINE_SIZE_MASK	0x3f		/* 64 byte line size */
#define	STREAM_BUF_OFF		1		/* All stream bufs off */
#define	STREAM_BUF_TIMEOUT	2		/* Streaming buf timed out */
#define	STREAM_CACHE_LINES	16

#define	STR_PG_VALID		0x2ull
#define	STR_PG_SHIFT		11
#define	STR_PG_MASK		0x3ffffull

#if defined(_KERNEL) && !defined(_ASM)

extern int stream_buf_init(struct sbus_soft_state *, caddr_t);
extern int stream_buf_resume_init(struct sbus_soft_state *);
extern void sync_stream_buf(struct sbus_soft_state *, uint_t, uint_t, int *,
    uint64_t);

#endif /* _KERNEL && !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IOCACHE_H */
