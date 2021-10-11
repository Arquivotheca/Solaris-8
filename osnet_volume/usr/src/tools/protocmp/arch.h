/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)arch.h	1.1	99/01/11 SMI"

#if defined(i386)
#define	PROTO_EXT "_i386"
#elif defined(sparc)
#define	PROTO_EXT "_sparc"
#elif defined(__ppc)
#define	PROTO_EXT "_ppc"
#else
#error "Unknown instruction set"
#endif


extern int assign_arch(const char *);
