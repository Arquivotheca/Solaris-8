
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DISK_GENERIC_H
#define	_DISK_GENERIC_H

#pragma ident	"@(#)disk_generic.h	1.3	98/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
/*
 *	Local prototypes for ANSI C compilers
 */
int	generic_ck_format(void);
int	generic_rdwr(int dir, int fd, daddr_t blkno, int secnt,
			caddr_t bufaddr, int flags, int *xfercntp);

#else

int	generic_ck_format();
int	generic_rdwr();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _DISK_GENERIC_H */
