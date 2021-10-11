/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DADA_DADA_CTL_H
#define	_SYS_DADA_DADA_CTL_H

#pragma ident	"@(#)dada_ctl.h	1.4	97/03/24 SMI"

#include <sys/dada/dada_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DCD control information
 *
 * Defines for stating level of reset
 */

#define	RESET_ALL	0	/* Reset all the target on the bus. */
#define	RESET_TARGET	1	/* Reset a specific target */

/*
 * Defines for reset_notify flag, to regsiter or cancel
 * the notification of external and internal bus resets
 */

#define	DCD_RESET_NOTIFY	0x01	/* register the reset notification */
#define	DCD_RESET_CANCEL	0x02	/* Cancel the reset notification */


/*
 * Define for the dcd_get_addr/ dcd_get_name first argument.
 */
#define	DCD_GET_INITIATOR_ID	((struct dcd_device *)NULL)
				/* return initiator-id */

/*
 * Define for dcd_get_name string length.
 * This is needed ebcause MAXNAMELEN is not part of DDI.
 */

#define	DCD_MAXNAMELEN		MAXNAMELEN

#ifdef _KERNEL

/*
 * kernel function decalarations
 */

/*
 * Abort and reset functions
 */

#ifdef	__STDC__
extern	int dcd_abort(struct dcd_address *ap, struct dcd_pkt *pkt);
extern  int dcd_reset(struct dcd_address *ap, int level);
#else /* __STDC__ */
extern 	int dcd_abort(), dcd_reset();
#endif

/*
 * Other functions
 */

#ifdef	__STDC__
extern int	dcd_get_bus_addr(struct dcd_device *devp, char *name, int len);
extern int	dcd_get_name(struct dcd_device *devp, char *name, int len);
#else	/* __STDC__ */
extern int 	dcd_get_bus_addr();
extern int	dcd_get_name();
#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DADA_DADA_CTL_H */
