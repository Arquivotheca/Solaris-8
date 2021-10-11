/*
 * Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TODIO_H
#define	_SYS_TODIO_H

#pragma ident	"@(#)todio.h	1.3	96/04/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	TOD_IOC			('t' << 8)
#define	TOD_GET_DATE		(TOD_IOC | 0)
#define	TOD_SET_ALARM		(TOD_IOC | 1)
#define	TOD_CLEAR_ALARM		(TOD_IOC | 2)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TODIO_H */
