/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _ATA_DEBUG_H
#define	_ATA_DEBUG_H

#pragma ident	"@(#)ata_debug.h	1.1	97/09/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * debugging options
 */

/*
 * Always print "real" error messages on non-debugging kernels
 */

#ifdef	ATA_DEBUG
#define	ADBG_ERROR(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_ERROR, fmt)
#else
#define	ADBG_ERROR(fmt)	ghd_err fmt
#endif

/*
 * ... everything else is conditional on the ATA_DEBUG preprocessor symbol
 */

#define	ADBG_WARN(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_WARN, fmt)
#define	ADBG_TRACE(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_TRACE, fmt)
#define	ADBG_INIT(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_INIT, fmt)
#define	ADBG_TRANSPORT(fmt)	ADBG_FLAG_CHK(ADBG_FLAG_TRANSPORT, fmt)
#define	ADBG_DMA(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_DMA, fmt)
#define	ADBG_ARQ(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_ARQ, fmt)




extern int ata_debug;

#define	ADBG_FLAG_ERROR		0x0001
#define	ADBG_FLAG_WARN		0x0002
#define	ADBG_FLAG_TRACE		0x0004
#define	ADBG_FLAG_INIT		0x0008
#define	ADBG_FLAG_TRANSPORT	0x0010
#define	ADBG_FLAG_DMA		0x0020
#define	ADBG_FLAG_ARQ		0x0040



#ifdef	ATA_DEBUG
#define	ADBG_FLAG_CHK(flag, fmt) if (ata_debug & (flag)) GDBG_PRF(fmt)
#else
#define	ADBG_FLAG_CHK(flag, fmt)
#endif



#ifdef	__cplusplus
}
#endif

#endif /* _ATA_DEBUG_H */
