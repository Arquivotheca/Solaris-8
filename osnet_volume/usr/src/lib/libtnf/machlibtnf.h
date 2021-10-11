/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _MACHLIBTNF_H
#define	_MACHLIBTNF_H

#pragma ident	"@(#)machlibtnf.h	1.6	94/09/21 SMI"

#include <sys/isa_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if	defined(_BIG_ENDIAN)	|| defined(__sparc)	|| defined(sparc)

tnf_uint32_t	_tnf_swap32(tnf_uint32_t);
tnf_uint16_t	_tnf_swap16(tnf_uint16_t);

#elif 	defined(_LITTLE_ENDIAN) || defined(__i386)	|| defined(i386)

#include <sys/byteorder.h>

#define	_tnf_swap32(x)	ntohl(x)
#define	_tnf_swap16(x)	ntohs(x)

#else

#error Unknown endian

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MACHLIBTNF_H */
