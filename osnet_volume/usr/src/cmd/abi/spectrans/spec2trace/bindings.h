/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _BINDINGS_H
#define	_BINDINGS_H

#pragma ident	"@(#)bindings.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ANTONYMS YES

extern int bindings_exist(void);
extern int need_exception_binding(void);
extern int need_bindings(char *);
extern int generate_bindings(char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _BINDINGS_H */
