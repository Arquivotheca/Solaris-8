/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_DES_H
#define	_SYS_DES_H

#pragma ident	"@(#)des.h	1.12	98/01/06 SMI"	/* SVr4.0 1.3	*/

/*
 * Generic DES driver interface
 * Keep this file hardware independent!
 */

#include <sys/ioccom.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DES_MAXLEN 	65536	/* maximum # of bytes to encrypt  */
#define	DES_QUICKLEN	16	/* maximum # of bytes to encrypt quickly */

enum desdir { ENCRYPT, DECRYPT };
enum desmode { CBC, ECB };

/*
 * parameters to ioctl call
 */
struct desparams {
	uchar_t des_key[8];	/* key (with low bit parity) */
	enum desdir des_dir;	/* direction */
	enum desmode des_mode;	/* mode */
	uchar_t des_ivec[8];	/* input vector */
	unsigned int des_len;	/* number of bytes to crypt */
	union {
		uchar_t UDES_data[DES_QUICKLEN];
		uchar_t *UDES_buf;
	} UDES;
#define	des_data	UDES.UDES_data	/* direct data here if quick */
#define	des_buf		UDES.UDES_buf	/* otherwise, pointer to data */
};

/*
 * Encrypt an arbitrary sized buffer
 */
#define	DESIOCBLOCK	_IOWR('d', 6, struct desparams)

/*
 * Encrypt of small amount of data, quickly
 */
#define	DESIOCQUICK	_IOWR('d', 7, struct desparams)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DES_H */
