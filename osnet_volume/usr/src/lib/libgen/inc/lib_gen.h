/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This is where all the interfaces that are internal to libgen
 * which do not have a better home live
 */

#ifndef _LIB_GEN_H
#define	_LIB_GEN_H

#pragma ident	"@(#)lib_gen.h	1.1	98/03/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int __p2open(const char *, int [2]);
extern int __p2close(int [2]);

#ifdef	__cplusplus
}
#endif

#endif /* _LIB_GEN_H */
