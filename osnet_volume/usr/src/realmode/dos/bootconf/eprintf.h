/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * eprintf.h -- public definitions for eprintf routines
 */

#ifndef	_EPRINTF_H
#define	_EPRINTF_H

#ident "@(#)eprintf.h   1.4   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public eprintf function prototypes
 */
int eprintf(int (*write_func)(void *arg, char *ptr, int len),
    void *arg, const char *fmt, va_list ap);

#ifdef	__cplusplus
}
#endif

#endif	/* _EPRINTF_H */
