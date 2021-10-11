/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UFN_H
#define	_UFN_H

#pragma ident	"@(#)ufn.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef NEEDPROTOS
typedef int (*cancelptype)(void *cancelparm);
#else /* NEEDPROTOS */
typedef int (*cancelptype)();
#endif /* NEEDPROTOS */

#ifdef	__cplusplus
}
#endif

#endif	/* _UFN_H */
