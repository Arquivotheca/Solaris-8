/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MINFREE_H
#define	_MINFREE_H

#pragma ident	"@(#)minfree.h	1.1	98/05/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int minfree_read(const char *, unsigned long long *);
extern int minfree_write(const char *, unsigned long long);
extern int minfree_compute(const char *, char *, unsigned long long *);

#ifdef	__cplusplus
}
#endif

#endif	/* _MINFREE_H */
