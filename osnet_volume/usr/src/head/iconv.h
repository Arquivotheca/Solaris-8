/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef	_ICONV_H
#define	_ICONV_H

#pragma ident	"@(#)iconv.h	1.2	94/01/21 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct _iconv_info *iconv_t;

#if defined(__STDC__)
extern iconv_t	iconv_open(const char *, const char *);
extern size_t	iconv(iconv_t, const char **, size_t *, char **, size_t *);
extern int	iconv_close(iconv_t);
#else
extern iconv_t	iconv_open();
extern size_t	iconv();
extern int	iconv_close();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ICONV_H */
