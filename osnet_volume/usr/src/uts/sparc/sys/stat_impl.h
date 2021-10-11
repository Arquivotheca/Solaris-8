/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STAT_IMPL_H
#define	_SYS_STAT_IMPL_H

#pragma ident	"@(#)stat_impl.h	1.1	99/05/04 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The implementation specific header for <sys/stat.h>
 */

#if !defined(_KERNEL)

#if defined(__STDC__)

extern int fstat(int, struct stat *);
extern int stat(const char *, struct stat *);

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int lstat(const char *, struct stat *);
extern int mknod(const char *, mode_t, dev_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#else	/* !__STDC__ */

extern int fstat(), stat();

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int lstat(), mknod();
#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#endif	/* !__STDC__ */

#endif /* !defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STAT_IMPL_H */
