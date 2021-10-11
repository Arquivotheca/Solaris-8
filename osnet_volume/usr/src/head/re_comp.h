/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RE_COMP_H
#define	_RE_COMP_H

#pragma ident	"@(#)re_comp.h	1.1	95/02/28 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

extern char *re_comp(const char *string);
extern int re_exec(const char *string);

#else

extern char *re_comp();
extern int re_exec();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _RE_COMP_H */
