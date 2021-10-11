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
 *  publication.
 *
 *	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_SOFTDES_H
#define	_SYS_SOFTDES_H

#pragma ident	"@(#)softdes.h	1.9	98/01/06 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * softdes.h,  Data types and definition for software DES
 */

/*
 * A chunk is an area of storage used in three different ways
 * - As a 64 bit quantity (in high endian notation)
 * - As a 48 bit quantity (6 low order bits per byte)
 * - As a 32 bit quantity (first 4 bytes)
 */
typedef union {
	struct {
/*
 * This (and the one farther down) looks awfully backwards???
 */
#ifdef _LONG_LONG_LTOH
		uint32_t	_long1;
		uint32_t	_long0;
#else
		uint32_t	_long0;
		uint32_t	_long1;
#endif
	} _longs;

#define	long0	_longs._long0
#define	long1	_longs._long1
	struct {
#ifdef _LONG_LONG_LTOH
		uchar_t	_byte7;
		uchar_t	_byte6;
		uchar_t	_byte5;
		uchar_t	_byte4;
		uchar_t	_byte3;
		uchar_t	_byte2;
		uchar_t	_byte1;
		uchar_t	_byte0;
#else
		uchar_t	_byte0;
		uchar_t	_byte1;
		uchar_t	_byte2;
		uchar_t	_byte3;
		uchar_t	_byte4;
		uchar_t	_byte5;
		uchar_t	_byte6;
		uchar_t	_byte7;
#endif
	} _bytes;
#define	byte0	_bytes._byte0
#define	byte1	_bytes._byte1
#define	byte2	_bytes._byte2
#define	byte3	_bytes._byte3
#define	byte4	_bytes._byte4
#define	byte5	_bytes._byte5
#define	byte6	_bytes._byte6
#define	byte7	_bytes._byte7
} chunk_t;

/*
 * Intermediate key storage
 * Created by des_setkey, used by des_encrypt and des_decrypt
 * 16 48 bit values
 */
struct deskeydata {
	chunk_t	keyval[16];
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOFTDES_H */
