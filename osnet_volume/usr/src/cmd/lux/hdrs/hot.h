/*
 * Copyright 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * PHOTON CONFIGURATION MANAGER
 * Common definitions
 */

/*
 * I18N message number ranges
 *  This file: 13000 - 13499
 *  Shared common messages: 1 - 1999
 */

#ifndef	_HOT_H
#define	_HOT_H

#pragma ident	"@(#)hot.h	1.5	99/05/07 SMI"


/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* Primary commands */
#define	INSERT_DEVICE	106	/* Hot plug */
#define	REMOVE_DEVICE	114	/* hot plug */

/* Device hotplugging */
#define	REPLACE_DEVICE	150
#define	DEV_ONLINE	155
#define	DEV_OFFLINE	156
#define	DEV_GETSTATE	157
#define	DEV_RESET	158
#define	BUS_QUIESCE	160
#define	BUS_UNQUIESCE	161
#define	BUS_GETSTATE	162
#define	BUS_RESET	163
#define	BUS_RESETALL	164

#define	SKIP		111
#define	QUIT		222

#define	NODE_CREATION_TIME	60	/* # seconds */
/*
 * Version 0.16 of the SES firmware powers up disks in front/back pairs.
 * However, the first disk inserted is usually spun up by itself, so
 * we need to calculate a timeout for 22/2 + 1 = 12 disks.
 *
 * Measured times are about 40s/disk for a total of 40*12=8 min total
 * The timeout assumes 10s/iteration or 4*12*10=8 min
 */
#define	PHOTON_SPINUP_TIMEOUT	(4*12)
#define	PHOTON_SPINUP_DELAY	10


#define		TARGET_ID(box_id, f_r, slot)    \
		((box_id | ((f_r == 'f' ? 0 : 1) << 4)) | (slot + 2))

#define		NEWER(time1, time2) 	(time1.tv_sec > time2.tv_sec)

extern	int	Options;


#ifdef	__cplusplus
}
#endif

#endif	/* _HOT_H */
