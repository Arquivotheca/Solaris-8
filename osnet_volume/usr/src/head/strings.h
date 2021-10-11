/*
 * Copyright (c) 1995, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STRINGS_H
#define	_STRINGS_H

#pragma ident	"@(#)strings.h	1.3	96/03/12 SMI"

#include <sys/types.h>
#include <sys/feature_tests.h>

#if !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
#include <string.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

extern int bcmp(const void *, const void *, size_t);
extern void bcopy(const void *, void *, size_t);
extern void bzero(void *, size_t);

extern char *index(const char *, int);
extern char *rindex(const char *, int);

/*
 * X/Open System Interfaces and Headers, Issue 4, Version 2, defines
 * both <string.h> and <strings.h>.  The namespace requirements
 * do not permit the visibility of anything other than what is
 * specifically defined for each of these headers.  As a result,
 * inclusion of <string.h> would result in declarations not allowed
 * in <strings.h>, and making the following prototypes visible for
 * anything other than X/Open UNIX Extension would result in
 * conflicts with what is now in <string.h>.
 */
#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
extern int ffs(int);
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);
#endif	/* defined(_XPG4_2) && !defined(__EXTENSIONS__) */

#else

extern int bcmp();
extern void bcopy();
extern void bzero();

extern char *index();
extern char *rindex();

#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
extern int ffs();
extern int strcasecmp();
extern int strncasecmp();
#endif /* defined(_XPG4_2) && !defined(__EXTENSIONS__) */

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _STRINGS_H */
