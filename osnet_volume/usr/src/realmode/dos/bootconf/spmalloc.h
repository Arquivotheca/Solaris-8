/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * spmalloc.h -- public definitions for spmalloc module
 */

#ifndef _SPMALLOC_H
#define	_SPMALLOC_H

#ident	"@(#)spmalloc.h	1.1	99/06/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void *spcl_malloc(unsigned short);
extern void *spcl_calloc(unsigned short, unsigned short);
extern void spcl_free(void *);
extern void *spcl_realloc(void *, unsigned short);

#ifdef	__cplusplus
}
#endif

#endif /* _SPMALLOC_H */
