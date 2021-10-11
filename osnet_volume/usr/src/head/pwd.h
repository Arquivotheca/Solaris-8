/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PWD_H
#define	_PWD_H

#pragma ident	"@(#)pwd.h	1.19	96/03/12 SMI"	/* SVr4.0 1.3.1.9 */

#include <sys/feature_tests.h>

#include <sys/types.h>

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
#include <stdio.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

struct passwd {
	char	*pw_name;
	char	*pw_passwd;
	uid_t	pw_uid;
	gid_t	pw_gid;
	char	*pw_age;
	char	*pw_comment;
	char	*pw_gecos;
	char	*pw_dir;
	char	*pw_shell;
};

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
struct comment {
	char	*c_dept;
	char	*c_name;
	char	*c_acct;
	char	*c_bin;
};
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#if defined(__STDC__)

extern struct passwd *getpwuid(uid_t);		/* MT-unsafe */
extern struct passwd *getpwnam(const char *);	/* MT-unsafe */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern struct passwd *getpwent_r(struct passwd *, char *, int);
extern struct passwd *fgetpwent_r(FILE *, struct passwd *, char *, int);
extern struct passwd *fgetpwent(FILE *);	/* MT-unsafe */
extern int putpwent(const struct passwd *, FILE *);
#endif /* defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) ... */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2)
extern void endpwent(void);
extern struct passwd *getpwent(void);		/* MT-unsafe */
extern void setpwent(void);
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#else  /* (__STDC__) */

extern struct passwd *getpwuid();		/* MT-unsafe */
extern struct passwd *getpwnam();		/* MT-unsafe */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern struct passwd *getpwent_r();
extern struct passwd *fgetpwent_r();

extern struct passwd *fgetpwent();		/* MT-unsafe */
extern int putpwent();
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2)
extern void endpwent();
extern struct passwd *getpwent();		/* MT-unsafe */
extern void setpwent();
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#endif /* (__STDC__) */

/*
 * getpwuid_r() & getpwnam_r() prototypes are defined here.
 */

/*
 * Previous releases of Solaris, starting at 2.3, provided definitions of
 * various functions as specified in POSIX.1c, Draft 6.  For some of these
 * functions, the final POSIX 1003.1c standard had a different number of
 * arguments and return values.
 *
 * The following segment of this header provides support for the standard
 * interfaces while supporting applications written under earlier
 * releases.  The application defines appropriate values of the feature
 * test macros _POSIX_C_SOURCE and _POSIX_PTHREAD_SEMANTICS to indicate
 * whether it was written to expect the Draft 6 or standard versions of
 * these interfaces, before including this header.  This header then
 * provides a mapping from the source version of the interface to an
 * appropriate binary interface.  Such mappings permit an application
 * to be built from libraries and objects which have mixed expectations
 * of the definitions of these functions.
 *
 * For applications using the Draft 6 definitions, the binary symbol is
 * the same as the source symbol, and no explicit mapping is needed.  For
 * the standard interface, the function func() is mapped to the binary
 * symbol _posix_func().  The preferred mechanism for the remapping is a
 * compiler #pragma.  If the compiler does not provide such a #pragma, the
 * header file defines a static function func() which calls the
 * _posix_func() version; this is required if the application needs to
 * take the address of func().
 *
 * NOTE: Support for the Draft 6 definitions is provided for compatibility
 * only.  New applications/libraries should use the standard definitions.
 */

#if	defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#if	defined(__STDC__)

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int getpwuid_r(uid_t, struct passwd *, char *, int, struct passwd **);
extern int getpwnam_r(const char *, struct passwd *, char *,
							int, struct passwd **);
#pragma redefine_extname getpwuid_r __posix_getpwuid_r
#pragma redefine_extname getpwnam_r __posix_getpwnam_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getpwuid_r(uid_t __uid, struct passwd *__pwd, char *__buf, int __len,
							struct passwd **__res)
{
	extern int __posix_getpwuid_r(uid_t, struct passwd *, char *, int,
							struct passwd **);
	return (__posix_getpwuid_r(__uid, __pwd, __buf, __len, __res));
}
static int
getpwnam_r(const char *__cb, struct passwd *__pwd, char *__buf, int __len,
							struct passwd **__res)
{
	extern int __posix_getpwnam_r(const char *, struct passwd *, char *,
							int, struct passwd **);
	return (__posix_getpwnam_r(__cb, __pwd, __buf, __len, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct passwd *getpwuid_r(uid_t, struct passwd *, char *, int);
extern struct passwd *getpwnam_r(const char *, struct passwd *, char *, int);

#endif  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else  /* __STDC__ */

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int getpwuid_r();
extern int getpwnam_r();
#pragma redefine_extname getpwuid_r __posix_getpwuid_r
#pragma redefine_extname getpwnam_r __posix_getpwnam_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getpwuid_r(__uid, __pwd, __buf, __len, __res)
	uid_t __uid;
	struct passwd *__pwd;
	char *__buf;
	int __len;
	struct passwd **__res;
{
	extern int __posix_getpwuid_r();
	return (__posix_getpwuid_r(__uid, __pwd, __buf, __len, __res));
}
static int
getpwnam_r(__cb, __pwd, __buf, __len, __res)
	char *__cb;
	struct passwd *__pwd;
	char *__buf;
	int __len;
	struct passwd **__res;
{
	extern int __posix_getpwnam_r();
	return (__posix_getpwnam_r(__cb, __pwd, __buf, __len, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct passwd *getpwuid_r();
extern struct passwd *getpwnam_r();

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#endif /* __STDC__ */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 ... */

#ifdef	__cplusplus
}
#endif

#endif /* _PWD_H */
