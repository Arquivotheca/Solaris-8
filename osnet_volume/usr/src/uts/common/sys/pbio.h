/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PBIO_H
#define	_SYS_PBIO_H

#pragma ident	"@(#)pbio.h	1.1	99/07/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Supported ioctls
 */
#define	PBIOC			('p' << 8)
#define	PB_BEGIN_MONITOR	(PBIOC | 1)
#define	PB_END_MONITOR		(PBIOC | 2)
#define	PB_CREATE_BUTTON_EVENT	(PBIOC | 3)	/* used by test suite */
#define	PB_GET_EVENTS		(PBIOC | 4)

/*
 * Supported events
 */
#define	PB_BUTTON_PRESS		0x1

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PBIO_H */
