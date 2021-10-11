
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_PARAM_H
#define	_PARAM_H

#pragma ident	"@(#)param.h	1.6	99/11/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains declarations of miscellaneous parameters.
 */
#ifndef	SECSIZE
#define	SECSIZE		DEV_BSIZE
#endif

#define	MAX_CYLS	(32 * 1024 - 1)		/* max legal cylinder count */
#define	MAX_HEADS	(64)			/* max legal head count */
#define	MAX_SECTS	(256)			/* max legal sector count */

#define	MIN_RPM		2000			/* min legal rpm */
#define	AVG_RPM		3600			/* default rpm */
#define	MAX_RPM		76000			/* max legal rpm */

#define	MIN_BPS		512			/* min legal bytes/sector */
#define	AVG_BPS		600			/* default bytes/sector */
#define	MAX_BPS		1000			/* max legal bytes/sector */

#define	INFINITY	0x7fffffff		/* a big number */

#ifdef	__cplusplus
}
#endif

#endif	/* _PARAM_H */
