/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1998 by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SELECT_H
#define	_SYS_SELECT_H

#pragma ident	"@(#)select.h	1.16	98/04/27 SMI"	/* SVr4.0 1.2 */

#include <sys/feature_tests.h>

#ifndef _KERNEL
#include <sys/time.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Select uses bit masks of file descriptors in longs.
 * These macros manipulate such bit fields.
 * FD_SETSIZE may be defined by the user, but the default here
 * should be >= NOFILE (param.h).
 */
#ifndef	FD_SETSIZE
#ifdef _LP64
#define	FD_SETSIZE	65536
#else
#define	FD_SETSIZE	1024
#endif	/* _LP64 */
#elif FD_SETSIZE > 1024 && !defined(_LP64)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	select	select_large_fdset
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	select	select_large_fdset
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* FD_SETSIZE */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
typedef	long	fd_mask;
#endif
typedef	long	fds_mask;

/*
 *  The value of _NBBY needs to be consistant with the value
 *  of NBBY in <sys/param.h>.
 */
#define	_NBBY 8
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef NBBY		/* number of bits per byte */
#define	NBBY _NBBY
#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NFDBITS		(sizeof (fd_mask) * NBBY)	/* bits per mask */
#endif
#define	FD_NFDBITS	(sizeof (fds_mask) * _NBBY)	/* bits per mask */

#define	__howmany(__x, __y)	(((__x)+((__y)-1))/(__y))
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef	howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
typedef	struct fd_set {
#else
typedef	struct __fd_set {
#endif
	long	fds_bits[__howmany(FD_SETSIZE, FD_NFDBITS)];
} fd_set;

#define	FD_SET(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] |= \
				    (1ul << ((__n) % FD_NFDBITS)))

#define	FD_CLR(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] &= \
				    ~(1ul << ((__n) % FD_NFDBITS)))

#define	FD_ISSET(__n, __p)	(((__p)->fds_bits[(__n)/FD_NFDBITS] & \
				    (1ul << ((__n) % FD_NFDBITS))) != 0l)

#ifdef _KERNEL
#define	FD_ZERO(p)	bzero((p), sizeof (*(p)))
#else
#define	FD_ZERO(__p)	memset((void *)(__p), 0, sizeof (*(__p)))
#endif /* _KERNEL */

#ifndef	_KERNEL
#ifdef	__STDC__
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#else
extern int select();
#endif	/* __STDC__ */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SELECT_H */
