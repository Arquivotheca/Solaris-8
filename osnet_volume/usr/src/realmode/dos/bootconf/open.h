/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * open.h -- public definitions for open module
 */

#ifndef	_OPEN_H
#define	_OPEN_H

#ident "@(#)open.h   1.6   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_open();
int fn_open(char *namebuf, unsigned len, const char *newdirname,
    const char *name, const char *newsuffix, int flags);

#ifdef	__cplusplus
}
#endif

#endif	/* _OPEN_H */
