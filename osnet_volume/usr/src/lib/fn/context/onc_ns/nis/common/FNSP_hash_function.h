/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)FNSP_hash_function.h	1.1 96/08/15 SMI"

#ifndef SHA_H
#define SHA_H

#include <sys/types.h>

#define SHA_BLOCKBYTES	64
#define SHA_BLOCKWORDS	16

#define SHA_HASHBYTES	20
#define SHA_HASHWORDS	5

typedef unsigned int 	word32;
typedef unsigned char   byte;

struct SHAContext {
	word32 key[SHA_BLOCKWORDS];
	word32 iv[SHA_HASHWORDS];
#if HAVE64
	word64 bytes;
#else
	word32 bytesHi, bytesLo;
#endif
};

#endif /* !SHA_H */
