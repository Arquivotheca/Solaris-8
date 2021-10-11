/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYSLOG_H
#define	_SYSLOG_H

#pragma ident	"@(#)syslog.h	1.11	96/05/17 SMI"	/* SVr4.0 1.1	*/

#include <sys/feature_tests.h>
#include <sys/syslog.h>
#include <sys/va_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__

void openlog(const char *, int, int);
void syslog(int, const char *, ...);
void closelog(void);
int setlogmask(int);
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
void vsyslog(int, const char *, __va_list);
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#else	/* __STDC__ */

void openlog();
void syslog();
void closelog();
int setlogmask();
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
void vsyslog();
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYSLOG_H */
