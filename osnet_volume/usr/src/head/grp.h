/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GRP_H
#define	_GRP_H

#pragma ident	"@(#)grp.h	1.18	96/03/12 SMI"	/* SVr4.0 1.3.3.1 */

#include <sys/feature_tests.h>

#include <sys/types.h>

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
#include <stdio.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

struct	group {	/* see getgrent(3) */
	char	*gr_name;
	char	*gr_passwd;
	gid_t	gr_gid;
	char	**gr_mem;
};

#if defined(__STDC__)

extern struct group *getgrgid(gid_t);		/* MT-unsafe */
extern struct group *getgrnam(const char *);	/* MT-unsafe */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern struct group *getgrent_r(struct group *, char *, int);
extern struct group *fgetgrent_r(FILE *, struct group *, char *, int);


extern struct group *fgetgrent(FILE *);		/* MT-unsafe */
extern int initgroups(const char *, gid_t);
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2)
extern void endgrent(void);
extern void setgrent(void);
extern struct group *getgrent(void);		/* MT-unsafe */
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */


#else

extern struct group *getgrgid();		/* MT-unsafe */
extern struct group *getgrnam();		/* MT-unsafe */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern struct group *getgrent_r();
extern struct group *fgetgrent_r();

extern struct group *fgetgrent();		/* MT-unsafe */
extern int initgroups();
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2)
extern void endgrent();
extern void setgrent();
extern struct group *getgrent();		/* MT-unsafe */
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#endif	/* __STDC__ */


/*
 * getgrgid_r() & getgrnam_r() prototypes are defined here.
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
extern int getgrgid_r(gid_t, struct group *, char *, int, struct group **);
extern int getgrnam_r(const char *, struct group *, char *, int,
							struct group **);
#pragma redefine_extname getgrgid_r __posix_getgrgid_r
#pragma redefine_extname getgrnam_r __posix_getgrnam_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getgrgid_r(gid_t __gid, struct group *__grp, char *__buf, int __len,
							struct group **__res)
{
	extern int __posix_getgrgid_r(gid_t, struct group *, char *,
							int, struct group **);
	return (__posix_getgrgid_r(__gid, __grp, __buf, __len, __res));
}
static int
getgrnam_r(const char *__cb, struct group *__grp, char *__buf, int __len,
							struct group **__res)
{
	extern int __posix_getgrnam_r(const char *, struct group *, char *,
							int, struct group **);
	return (__posix_getgrnam_r(__cb, __grp, __buf, __len, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct group *getgrgid_r(gid_t, struct group *, char *, int);
extern struct group *getgrnam_r(const char *, struct group *, char *, int);

#endif  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else  /* __STDC__ */

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int getgrgid_r();
extern int getgrnam_r();
#pragma redefine_extname getgrgid_r __posix_getgrgid_r
#pragma redefine_extname getgrnam_r __posix_getgrnam_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getgrgid_r(__gid, __grp, __buf, __len, __res)
	gid_t __gid;
	struct group *__grp;
	char *__buf;
	int __len;
	struct group **__res;
{
	extern int __posix_getgrgid_r();
	return (__posix_getgrgid_r(__gid, __grp, __buf, __len, __res));
}
static int
getgrnam_r(__cb, __grp, __buf, __len, __res)
	char *__cb;
	struct group *__grp;
	char *__buf;
	int __len;
	struct group **__res;
{
	extern int __posix_getgrnam_r();
	return (__posix_getgrnam_r(__cb, __grp, __buf, __len, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct group *getgrgid_r();
extern struct group *getgrnam_r();

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#endif /* __STDC__ */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _GRP_H */
