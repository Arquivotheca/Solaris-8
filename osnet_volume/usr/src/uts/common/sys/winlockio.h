/*
 * Copyright (c) 1986,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_WINLOCKIO_H
#define	_SYS_WINLOCKIO_H

#pragma ident	"@(#)winlockio.h	1.6	98/01/06 SMI"

#include <sys/types.h>
#include <sys/ioccom.h>

#ifdef	__cplusplus
extern "C" {
#endif

	/*
	 * Structure for allocating lock contexts.  The identification
	 * should be provided as the offset for mmap(2).  The offset is
	 * the byte-offset relative to the start of the page returned
	 * by mmap(2).
	 */

struct	winlockalloc {
	uint_t	sy_key;		/* user-provided key, if any */
	uint_t	sy_ident;	/* system-provided identification */
};

struct	winlocktimeout {
	uint_t	sy_ident;
	uint_t	sy_timeout;
	int	sy_flags;
};

#define	WIOC	('L'<<8)


#define	WINLOCKALLOC		(WIOC|0)
#define	WINLOCKFREE		(WIOC|1)
#define	WINLOCKSETTIMEOUT	(WIOC|2)
#define	WINLOCKGETTIMEOUT	(WIOC|3)
#define	WINLOCKDUMP		(WIOC|4)


/* flag bits */
#define	SY_NOTIMEOUT	0x1	/* This client never times out */


#ifndef	GRABPAGEALLOC
#include <sys/fbio.h>
#endif

#ifdef	_KERNEL

#define	UFLAGS		0x00ff	/* user's flags */
#define	KFLAGS		0xff00	/* kernel's flags */
#define	LOCKMAP		0x0100	/* proc has a lock mapping */
#define	UNLOCKMAP	0x0200	/* proc has an unlock mapping */
#define	TRASHPAGE	0x0400	/* proc has unlock mapping to trashpage */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_WINLOCKIO_H */
