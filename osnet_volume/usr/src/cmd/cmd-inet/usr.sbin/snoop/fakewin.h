/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef _FAKEWIN_H
#define	_FAKEWIN_H

#pragma ident	"@(#)fakewin.h	1.4	94/09/28 SMI"

/*
 * This file defines appropriate macros so that
 * we can use the same codebase for Unix, DOS, and Windows.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _WINDOWS
#include <tklib.h>

#define	malloc		_fmalloc
#define	calloc		_fcalloc
#define	free		_ffree
#define	strdup		_fstrdup
#define	strcpy		_fstrcpy
#define	strcmp		_fstrcmp
#define	strchr		_fstrchr
#define	sprintf		wsprintf
#define	vsprintf	wvsprintf
#define	memcpy		_fmemcpy
#define	strlen		_fstrlen
#else
#define	LPSTR	char *
#endif

#if !defined(_WINDOWS) && !defined(_MSDOS)
#define	_TKFAR
#endif

#ifndef	_WINDOWS
#define	_TKPASCAL
#define	__export
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_FAKEWIN_H */
