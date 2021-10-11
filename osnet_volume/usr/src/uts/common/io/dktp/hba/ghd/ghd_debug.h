/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GHD_DEBUG_H
#define	_GHD_DEBUG_H

#pragma	ident	"@(#)ghd_debug.h	1.5	99/03/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/varargs.h>

void		ghd_err(char *fmt, ...);
extern	ulong_t	ghd_debug_flags;

#define	GDBG_FLAG_ERROR		0x0001
#define	GDBG_FLAG_INTR		0x0002
#define	GDBG_FLAG_PEND_INTR	0x0004
#define	GDBG_FLAG_START		0x0008
#define	GDBG_FLAG_WARN		0x0010
#define	GDBG_FLAG_DMA		0x0020
#define	GDBG_FLAG_PKT		0x0040
#define	GDBG_FLAG_INIT		0x0080
#define	GDBG_FLAG_WAITQ		0x0100

/*
 * Use prom_printf() or vcmn_err()
 */
#ifdef GHD_DEBUG_PROM_PRINTF
#define	GDBG_PRF(fmt)	prom_printf fmt
void	prom_printf(char *fmt, ...);
#else
#define	GDBG_PRF(fmt)	ghd_err fmt
#endif



#ifdef	GHD_DEBUG

#define	GDBG_FLAG_CHK(flag, fmt) if (ghd_debug_flags & (flag)) GDBG_PRF(fmt)

#else	/* !GHD_DEBUG */

#define	GDBG_FLAG_CHK(flag, fmt)

#endif	/* !GHD_DEBUG */

/*
 * Always print "real" error messages on non-debugging kernels
 */

#ifdef	GHD_DEBUG
#define	GDBG_ERROR(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_ERROR, fmt)
#else
#define	GDBG_ERROR(fmt)	ghd_err fmt
#endif

/*
 * Debugging printf macros
 */

#define	GDBG_INTR(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_INTR, fmt)
#define	GDBG_PEND_INTR(fmt)	GDBG_FLAG_CHK(GDBG_FLAG_PEND_INTR, fmt)
#define	GDBG_START(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_START, fmt)
#define	GDBG_WARN(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_WARN, fmt)
#define	GDBG_DMA(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_DMA, fmt)
#define	GDBG_PKT(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_PKT, fmt)
#define	GDBG_INIT(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_INIT, fmt)
#define	GDBG_WAITQ(fmt)		GDBG_FLAG_CHK(GDBG_FLAG_WAITQ, fmt)



#ifdef	__cplusplus
}
#endif

#endif  /* _GHD_DEBUG_H */
