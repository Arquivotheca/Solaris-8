/*
 * Copyright (c) 1994-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_PLAT_H
#define	_SYS_PROM_PLAT_H

#pragma ident	"@(#)prom_plat.h	1.2	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains external platform-specific promif interface definitions
 * for i386 platforms.  Mostly, this should be empty.
 */

extern	int		prom_getmacaddr(int hd, caddr_t ea);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_PLAT_H */
