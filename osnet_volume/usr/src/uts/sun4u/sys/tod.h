/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TOD_H
#define	_SYS_TOD_H

#pragma ident	"@(#)tod.h	1.8	97/11/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	TOD_READ			0x40
#define	TOD_WRITE			0x80
#define	TOD_SUSPENDED			0x0

struct tod_softc {
	dev_info_t *dip;
	kmutex_t mutex;
	int cpr_stage;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TOD_H */
