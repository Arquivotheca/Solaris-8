/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Power management chip
 */

#ifndef	_SYS_PMCIO_H
#define	_SYS_PMCIO_H

#pragma ident	"@(#)pmcio.h	1.8	96/04/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ioctls available thru the pm driver ...
 */

typedef enum {
	PMC_GET_KBD = 1,	/* Return the connection status of keyboard */
	PMC_GET_ENET,		/* Return the connection status of ethernet */
	PMC_GET_ISDN,		/* Return the connection status of NT & TE */
	PMC_GET_A2D,		/* Return the result of a2d converter */
	PMC_POWER_OFF		/* Turn all power off (superuser) */
} pmc_ioctls;

/* Connection status return values */
#define	PMC_KBD_STAT		0x04    /* bit2 */
#define	PMC_ENET_STAT		0x08    /* bit3 */
#define	PMC_ISDN_ST0		0x04    /* bit2 */
#define	PMC_ISDN_ST1		0x08    /* bit3 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PMCIO_H */
