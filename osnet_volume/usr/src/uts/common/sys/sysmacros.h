/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SYSMACROS_H
#define	_SYS_SYSMACROS_H

#pragma ident	"@(#)sysmacros.h	1.37	99/04/14 SMI"

#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Some macros for units conversion
 */
/*
 * Disk blocks (sectors) and bytes.
 */
#define	dtob(DD)	((DD) << DEV_BSHIFT)
#define	btod(BB)	(((BB) + DEV_BSIZE - 1) >> DEV_BSHIFT)
#define	btodt(BB)	((BB) >> DEV_BSHIFT)
#define	lbtod(BB)	(((offset_t)(BB) + DEV_BSIZE - 1) >> DEV_BSHIFT)

/* common macros */
#ifndef MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define	MAX(a, b)	((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define	ABS(a)		((a) < 0 ? -(a) : (a))
#endif

#ifdef _KERNEL

/*
 * Convert a single byte to/from binary-coded decimal (BCD).
 */
extern unsigned char byte_to_bcd[256];
extern unsigned char bcd_to_byte[256];

#define	BYTE_TO_BCD(x)	byte_to_bcd[(x) & 0xff]
#define	BCD_TO_BYTE(x)	bcd_to_byte[(x) & 0xff]

#endif	/* _KERNEL */

/*
 * WARNING: The device number macros defined here should not be used by device
 * drivers or user software. Device drivers should use the device functions
 * defined in the DDI/DKI interface (see also ddi.h). Application software
 * should make use of the library routines available in makedev(3). A set of
 * new device macros are provided to operate on the expanded device number
 * format supported in SVR4. Macro versions of the DDI device functions are
 * provided for use by kernel proper routines only. Macro routines bmajor(),
 * major(), minor(), emajor(), eminor(), and makedev() will be removed or
 * their definitions changed at the next major release following SVR4.
 */

#define	O_BITSMAJOR	7	/* # of SVR3 major device bits */
#define	O_BITSMINOR	8	/* # of SVR3 minor device bits */
#define	O_MAXMAJ	0x7f	/* SVR3 max major value */
#define	O_MAXMIN	0xff	/* SVR3 max major value */


#define	L_BITSMAJOR32	14	/* # of SVR4 major device bits */
#define	L_BITSMINOR32	18	/* # of SVR4 minor device bits */
#define	L_MAXMAJ32	0x3fff	/* SVR4 max major value */
#define	L_MAXMIN32	0x3ffff	/* MAX minor for 3b2 software drivers. */
				/* For 3b2 hardware devices the minor is */
				/* restricted to 256 (0-255) */

#ifdef _LP64
#define	L_BITSMAJOR	32	/* # of major device bits in 64-bit Solaris */
#define	L_BITSMINOR	32	/* # of minor device bits in 64-bit Solaris */
#define	L_MAXMAJ	0xfffffffful	/* max major value */
#define	L_MAXMIN	0xfffffffful	/* max minor value */
#else
#define	L_BITSMAJOR	L_BITSMAJOR32
#define	L_BITSMINOR	L_BITSMINOR32
#define	L_MAXMAJ	L_MAXMAJ32
#define	L_MAXMIN	L_MAXMIN32
#endif

#ifdef _KERNEL

/* major part of a device internal to the kernel */

#define	major(x)	(major_t)((((unsigned)(x)) >> O_BITSMINOR) & O_MAXMAJ)
#define	bmajor(x)	(major_t)((((unsigned)(x)) >> O_BITSMINOR) & O_MAXMAJ)

/* get internal major part of expanded device number */

#define	getmajor(x)	(major_t)((((dev_t)(x)) >> L_BITSMINOR) & L_MAXMAJ)

/* minor part of a device internal to the kernel */

#define	minor(x)	(minor_t)((x) & O_MAXMIN)

/* get internal minor part of expanded device number */

#define	getminor(x)	(minor_t)((x) & L_MAXMIN)

#else

/* major part of a device external from the kernel (same as emajor below) */

#define	major(x)	(major_t)((((unsigned)(x)) >> O_BITSMINOR) & O_MAXMAJ)

/* minor part of a device external from the kernel  (same as eminor below) */

#define	minor(x)	(minor_t)((x) & O_MAXMIN)

#endif	/* _KERNEL */

/* create old device number */

#define	makedev(x, y) (unsigned short)(((x) << O_BITSMINOR) | ((y) & O_MAXMIN))

/* make an new device number */

#define	makedevice(x, y) (dev_t)(((dev_t)(x) << L_BITSMINOR) | ((y) & L_MAXMIN))


/*
 * emajor() allows kernel/driver code to print external major numbers
 * eminor() allows kernel/driver code to print external minor numbers
 */

#define	emajor(x) \
	(major_t)(((unsigned int)(x) >> O_BITSMINOR) > O_MAXMAJ) ? \
	    NODEV : (((unsigned int)(x) >> O_BITSMINOR) & O_MAXMAJ)

#define	eminor(x) \
	(minor_t)((x) & O_MAXMIN)

/*
 * get external major and minor device
 * components from expanded device number
 */
#define	getemajor(x)	(major_t)((((dev_t)(x) >> L_BITSMINOR) > L_MAXMAJ) ? \
			    NODEV : (((dev_t)(x) >> L_BITSMINOR) & L_MAXMAJ))
#define	geteminor(x)	(minor_t)((x) & L_MAXMIN)

/*
 * These are versions of the kernel routines for compressing and
 * expanding long device numbers that don't return errors.
 */
#if (L_BITSMAJOR32 == L_BITSMAJOR) && (L_BITSMINOR32 == L_BITSMINOR)

#define	DEVCMPL(x)	(x)
#define	DEVEXPL(x)	(x)

#else

#define	DEVCMPL(x)	\
	(dev32_t)((((x) >> L_BITSMINOR) > L_MAXMAJ32 || \
	    ((x) & L_MAXMIN) > L_MAXMIN32) ? NODEV32 : \
	    ((((x) >> L_BITSMINOR) << L_BITSMINOR32) | ((x) & L_MAXMIN32)))

#define	DEVEXPL(x)	\
	(((x) == NODEV32) ? NODEV : \
	makedevice(((x) >> L_BITSMINOR32) & L_MAXMAJ32, (x) & L_MAXMIN32))

#endif /* L_BITSMAJOR32 ... */

/* convert to old (SVR3.2) dev format */

#define	cmpdev(x) \
	(o_dev_t)((((x) >> L_BITSMINOR) > O_MAXMAJ || \
	    ((x) & L_MAXMIN) > O_MAXMIN) ? NODEV : \
	    ((((x) >> L_BITSMINOR) << O_BITSMINOR) | ((x) & O_MAXMIN)))

/* convert to new (SVR4) dev format */

#define	expdev(x) \
	(dev_t)(((dev_t)(((x) >> O_BITSMINOR) & O_MAXMAJ) << L_BITSMINOR) | \
	    ((x) & O_MAXMIN))

#define	SALIGN(p)	\
	(char *)(((intptr_t)p + (sizeof (short) - 1)) & ~(sizeof (short) - 1))
#define	IALIGN(p)	\
	(char *)(((intptr_t)p + (sizeof (int) - 1)) & ~(sizeof (int) - 1))
#define	LALIGN(p)	\
	(char *)(((intptr_t)p + (sizeof (long) - 1)) & ~(sizeof (long) - 1))

#define	SNEXT(p)		(char *)((short *)p + 1)
#define	INEXT(p)		(char *)((int *)p + 1)
#define	LNEXT(p)		(char *)((long *)p + 1)

/*
 * Macro for checking power of 2 address alignment.
 */
#define	IS_P2ALIGNED(v, a) ((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)

/*
 * Macros for counting and rounding.
 */
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/*
 * Macros for various sorts of alignment and rounding when the alignment
 * is known to be a power of 2.
 */
#define	P2ALIGN(x, align)		((x) & -(align))
#define	P2PHASE(x, align)		((x) & ((align) - 1))
#define	P2NPHASE(x, align)		(-(x) & ((align) - 1))
#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))
#define	P2END(x, align)			(-(~(x) & -(align)))
#define	P2PHASEUP(x, align, phase)	((phase) - (((phase) - (x)) & -(align)))
#define	P2CROSS(x, y, align)		(((x) ^ (y)) > (align) - 1)

/*
 * Macros to atomically increment/decrement a variable.  mutex and var
 * must be pointers.
 */
#define	INCR_COUNT(var, mutex) mutex_enter(mutex), (*(var))++, mutex_exit(mutex)
#define	DECR_COUNT(var, mutex) mutex_enter(mutex), (*(var))--, mutex_exit(mutex)

#if defined(_KERNEL) && !defined(_KMEMUSER) && !defined(offsetof)

/* avoid any possibility of clashing with <stddef.h> version */

#define	offsetof(s, m)	((size_t)(&(((s *)0)->m)))
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSMACROS_H */
