/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ZSMACH_H
#define	_SYS_ZSMACH_H

#pragma ident	"@(#)zsmach.h	1.5	96/05/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * sun4u platform dependent software definitions for zs driver.
 */
#define	ZSDELAY()
#define	ZSFLUSH()	(void) zs->zs_addr->zscc_control
#define	ZSNEXTPOLL(zs, zscurr)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZSMACH_H */
