/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */

#ifndef _SYS_UTSNAME_H
#define	_SYS_UTSNAME_H

#pragma ident	"@(#)utsname.h	1.26	98/12/03 SMI"	/* From SVr4.0 11.14 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_SYS_NMLN	257	/* 4.0 size of utsname elements	*/
				/* Must be at least 257 to	*/
				/* support Internet hostnames.	*/

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
#ifndef	SYS_NMLN
#define	SYS_NMLN	_SYS_NMLN
#endif
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || ... */

struct utsname {
	char	sysname[_SYS_NMLN];
	char	nodename[_SYS_NMLN];
	char	release[_SYS_NMLN];
	char	version[_SYS_NMLN];
	char	machine[_SYS_NMLN];
};

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern struct utsname utsname;
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */

#if !defined(_KERNEL)

#if defined(i386) || defined(__i386)

#if defined(__STDC__)

#if !defined(lint) && !defined(__lint)
static int uname(struct utsname *);
static int _uname(struct utsname *);
#else
extern int uname(struct utsname *);
extern int _uname(struct utsname *);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int nuname(struct utsname *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */
extern int _nuname(struct utsname *);

#else	/* defined(__STDC__) */

#if !defined(lint) && !defined(__lint)
static int uname();
static int _uname();
#else
extern int uname();
extern int _uname();
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int nuname();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */
extern int _nuname();

#endif	/* defined(__STDC__) */


#if !defined(lint) && !defined(__lint)
static int
#if defined(__STDC__)
_uname(struct utsname *_buf)
#else
_uname(_buf)
struct utsname *_buf;
#endif
{
	return (_nuname(_buf));
}

static int
#if defined(__STDC__)
uname(struct utsname *_buf)
#else
uname(_buf)
struct utsname *_buf;
#endif
{
	return (_nuname(_buf));
}
#endif /* !defined(lint) && !defined(__lint) */

#else	/* defined(i386) || defined(__i386) */

#if defined(__STDC__)
extern int uname(struct utsname *);
#else
extern int uname();
#endif	/* (__STDC__) */

#endif	/* defined(i386) || defined(__i386) */

#endif	/* !(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_UTSNAME_H */
