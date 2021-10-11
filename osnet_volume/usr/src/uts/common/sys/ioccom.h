/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_IOCCOM_H
#define	_SYS_IOCCOM_H

#pragma ident	"@(#)ioccom.h	1.14	97/10/22 SMI"	/* SVr4.0 1.4	*/

/*	ioccom.h 1.3 88/02/08 SMI; from UCB ioctl.h 7.1 6/4/86	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 255 bytes.
 */
#define	IOCPARM_MASK	0xff		/* parameters must be < 256 bytes */
#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)

/*
 * The 0x20000000 is so we can distinguish new ioctl's from old.
 */
#define	_IO(x, y)	(IOC_VOID|(x<<8)|y)
#define	_IOR(x, y, t) 							\
	    ((int)((uint32_t)						\
	    (IOC_OUT|(((sizeof (t))&IOCPARM_MASK)<<16)|(x<<8)|y)))

#define	_IORN(x, y, t)	((int)((uint32_t)(IOC_OUT|(((t)&IOCPARM_MASK)<<16)| \
	    (x<<8)|y)))

#define	_IOW(x, y, t)							\
	    ((int)((uint32_t)(IOC_IN|(((sizeof (t))&IOCPARM_MASK)<<16)|	\
	    (x<<8)|y)))

#define	_IOWN(x, y, t)	((int32_t)(uint32_t)(IOC_IN|(((t)&IOCPARM_MASK)<<16)| \
	    (x<<8)|y))

#define	_IOWR(x, y, t)							\
	    ((int)((uint32_t)(IOC_INOUT|(((sizeof (t))&IOCPARM_MASK)<<16)| \
	    (x<<8)|y)))

#define	_IOWRN(x, y, t)							\
	    ((int)((uint32_t)(IOC_INOUT|(((t)&IOCPARM_MASK)<<16)| \
	    (x<<8)|y)))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IOCCOM_H */
