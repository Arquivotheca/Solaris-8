/*
 * Copyright (c) 1991, Sun Microsystems,  Inc.
 */

#ifndef _SYS_XC_LEVELS_H
#define	_SYS_XC_LEVELS_H

#pragma ident	"@(#)xc_levels.h	1.11	92/07/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Index for xc_mboxes at each level */
#define	X_CALL_LOPRI	0
#define	X_CALL_MEDPRI	1
#define	X_CALL_HIPRI	2
#define	X_CALL_LEVELS	(X_CALL_HIPRI - X_CALL_LOPRI + 1)


/* PIL associated with each x-call level */
#define	XC_LO_PIL	1	/* low priority x-calls */
#define	XC_MED_PIL	13	/* medium priority x-calls */
#define	XC_HI_PIL	15	/* high priority x-calls */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XC_LEVELS_H */
