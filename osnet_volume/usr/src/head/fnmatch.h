/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * Copyright 1985, 1994 by Mortice Kern Systems Inc.  All rights reserved.
 */

#ifndef	_FNMATCH_H
#define	_FNMATCH_H

#pragma ident	"@(#)fnmatch.h	1.3	94/10/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	FNM_PATHNAME	0x01	/* Slash in str only matches slash in pattern */
#define	FNM_NOESCAPE	0x02	/* Disable '\'-quoting of metacharacters */
#define	FNM_PERIOD	0x04	/* Leading period in string must be exactly */
				/* matched by period in pattern	*/
#define	FNM_IGNORECASE	0x08	/* Ignore case when making comparisons */

#define	FNM_NOMATCH	1	/* string doesnt match the specified pattern */
#define	FNM_ERROR	2	/* error occured */
#define	FNM_NOSYS	3	/* Function (XPG4) not supported */

#if defined(__STDC__)
extern int fnmatch(const char *, const char *, int);
#else
extern int fnmatch();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _FNMATCH_H */
