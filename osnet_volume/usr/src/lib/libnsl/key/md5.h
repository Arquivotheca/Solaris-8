/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/*
 * MD5.H - header file for MD5C.C
 */

#ifndef _MD5_H
#define	_MD5_H

#pragma ident	"@(#)md5.h	1.1	97/11/19 SMI"

#include <sys/types.h>		/* for uint_* */

#ifdef	__cplusplus
extern "C" {
#endif

/* MD5 context. */
typedef struct	{
	uint32_t state[4];	/* state (ABCD) */
	uint32_t count[2];	/* number of bits, modulo 2^64 (lsb first) */
	union	{
		uint8_t		buf8[64];	/* undigested input */
		uint32_t	buf32[16]; 	/* realigned input */
	} buf_un;
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, uint8_t *, uint32_t);
void MD5Final(uint8_t [16], MD5_CTX *);

#ifdef	__cplusplus
}
#endif

#endif /* _MD5_H */
