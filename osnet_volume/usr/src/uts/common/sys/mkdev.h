/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MKDEV_H
#define	_SYS_MKDEV_H

#pragma ident	"@(#)mkdev.h	1.17	97/10/22 SMI"	/* SVr4.0 1.6	*/

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SVR3/Pre-EFT device number constants.
 */
#define	ONBITSMAJOR	7	/* # of SVR3 major device bits */
#define	ONBITSMINOR	8	/* # of SVR3 minor device bits */
#define	OMAXMAJ		0x7f	/* SVR3 max major value */
#define	OMAXMIN		0xff	/* SVR3 max major value */

/*
 * 32-bit Solaris device major/minor sizes.
 */
#define	NBITSMAJOR32	14
#define	NBITSMINOR32	18
#define	MAXMAJ32	0x3ffful	/* SVR4 max major value */
#define	MAXMIN32	0x3fffful	/* SVR4 max minor value */

#ifdef _LP64

#define	NBITSMAJOR64	32	/* # of major device bits in 64-bit Solaris */
#define	NBITSMINOR64	32	/* # of minor device bits in 64-bit Solaris */
#define	MAXMAJ64	0xfffffffful	/* max major value */
#define	MAXMIN64	0xfffffffful	/* max minor value */

#define	NBITSMAJOR	NBITSMAJOR64
#define	NBITSMINOR	NBITSMINOR64
#define	MAXMAJ		MAXMAJ64
#define	MAXMIN		MAXMIN64

#else /* !_LP64 */

#define	NBITSMAJOR	NBITSMAJOR32
#define	NBITSMINOR	NBITSMINOR32
#define	MAXMAJ		MAXMAJ32
#define	MAXMIN		MAXMIN32

#endif /* !_LP64 */

#if !defined(_KERNEL)

/*
 * Undefine sysmacros.h device macros.
 */
#undef makedev
#undef major
#undef minor

#if defined(__STDC__)

extern dev_t makedev(const major_t, const minor_t);
extern major_t major(const dev_t);
extern minor_t minor(const dev_t);
extern dev_t __makedev(const int, const major_t, const minor_t);
extern major_t __major(const int, const dev_t);
extern minor_t __minor(const int, const dev_t);

#else

extern dev_t makedev();
extern major_t major();
extern minor_t minor();
extern dev_t __makedev();
extern major_t __major();
extern minor_t __minor();

#endif	/* defined(__STDC__) */

#define	OLDDEV 0	/* old device format */
#define	NEWDEV 1	/* new device format */

#define	makedev(maj, min)	(__makedev(NEWDEV, maj, min))
#define	major(dev)		(__major(NEWDEV, dev))
#define	minor(dev)		(__minor(NEWDEV, dev))

#endif	/* !defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MKDEV_H */
