/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

#pragma ident	"@(#)utils.h	1.1	98/12/14 SMI"

#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void warn(const char *, ...);
extern void die(const char *, ...);
extern char *strcats(char *, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILS_H */
