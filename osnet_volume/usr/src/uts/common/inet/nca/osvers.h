/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_OSVERS_H
#define	_OSVERS_H

#pragma ident	"@(#)osvers.h	1.1	99/08/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#define	SunOS_5_5	0x05005000
#define	SunOS_5_5_1	0x05005001
#define	SunOS_5_6	0x05006000
#define	SunOS_5_7	0x05007000
#define	SunOS_5_8	0x05008000

#ifndef	SunOS

#define	SunOS		SunOS_5_7

#endif	/* SunOS */


#ifdef	__cplusplus
}
#endif

#endif	/* _OSVERS_H */
