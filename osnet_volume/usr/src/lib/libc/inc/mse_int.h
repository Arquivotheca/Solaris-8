/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MSE_INT_H
#define	_MSE_INT_H

#pragma ident	"@(#)mse_int.h	1.1	97/12/06 SMI"

#include <stddef.h>
#include <time.h>

extern size_t wcsftime(wchar_t *, size_t, const char *, const struct tm *);
extern size_t __wcsftime_xpg5(wchar_t *, size_t, const wchar_t *,
	const struct tm *);

extern wchar_t *wcstok(wchar_t *, const wchar_t *);
extern wchar_t *__wcstok_xpg5(wchar_t *, const wchar_t *, wchar_t **);

#endif	/* _MSE_INT_H */
