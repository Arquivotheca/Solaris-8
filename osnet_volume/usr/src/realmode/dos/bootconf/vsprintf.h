/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * vsprintf.h -- public definitions for vsprintf routines
 */

#ifndef	_VSPRINTF_H
#define	_VSPRINTF_H

#ident "@(#)vsprintf.h   1.4   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public vsprintf function prototypes
 */
int
vsnprintf(char *buffer, int len, const char *fmt, va_list ap);

#ifdef	__cplusplus
}
#endif

#endif	/* _VSPRINTF_H */
