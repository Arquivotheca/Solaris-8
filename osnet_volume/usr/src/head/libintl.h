/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* Libintl is a library of advanced internationalization functions. */

#ifndef	_LIBINTL_H
#define	_LIBINTL_H

#pragma ident	"@(#)libintl.h	1.12	97/08/20 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _WCHAR_T
#define	_WCHAR_T
#if defined(_LP64)
typedef int	wchar_t;
#else
typedef long	wchar_t;
#endif
#endif	/* !_WCHAR_T */

#define	TEXTDOMAINMAX	256

#ifdef __STDC__
extern char *dcgettext(const char *, const char *, const int);
extern char *dgettext(const char *, const char *);
extern char *gettext(const char *);
extern char *textdomain(const char *);
extern char *bindtextdomain(const char *, const char *);

/* Word handling functions --- requires dynamic linking */
/* Warning: these are experimental and subject to change. */
extern int wdinit(void);
extern int wdchkind(wchar_t);
extern int wdbindf(wchar_t, wchar_t, int);
extern wchar_t *wddelim(wchar_t, wchar_t, int);
extern wchar_t mcfiller(void);
extern int mcwrap(void);

#else
extern char *dcgettext();
extern char *dgettext();
extern char *gettext();
extern char *textdomain();
extern char *bindtextdomain();

/* Word handling functions --- requires dynamic linking */
/* Warning: these are experimental and subject to change. */
extern int wdinit();
extern int wdchkind();
extern int wdbindf();
extern wchar_t *wddelim();
extern wchar_t mcfiller();
extern int mcwrap();

#endif

#ifdef	__cplusplus
}
#endif

#endif /* _LIBINTL_H */
