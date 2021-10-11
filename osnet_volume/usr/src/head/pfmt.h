/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PFMT_H
#define	_PFMT_H

#pragma ident	"@(#)pfmt.h	1.3	94/01/06 SMI"

#include <stdio.h>
#ifndef va_args
#include <stdarg.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	MM_STD		0
#define	MM_NOSTD	0x100
#define	MM_GET		0
#define	MM_NOGET	0x200

#define	MM_ACTION	0x400

#define	MM_NOCONSOLE	0
#define	MM_CONSOLE	0x800

/* Classification */
#define	MM_NULLMC	0
#define	MM_HARD		0x1000
#define	MM_SOFT		0x2000
#define	MM_FIRM		0x4000
#define	MM_APPL		0x8000
#define	MM_UTIL		0x10000
#define	MM_OPSYS	0x20000

/* Most commonly used combinations */
#define	MM_SVCMD	MM_UTIL|MM_SOFT

#define	MM_ERROR	0
#define	MM_HALT		1
#define	MM_WARNING	2
#define	MM_INFO		3

#ifdef __STDC__
int pfmt(FILE *, long, const char *, ...);
int lfmt(FILE *, long, const char *, ...);
int vpfmt(FILE *, long, const char *, va_list);
int vlfmt(FILE *, long, const char *, va_list);
const char *setcat(const char *);
int setlabel(const char *);
int addsev(int, const char *);
#else
int pfmt();
int lfmt();
int vpfmt();
int vlfmt();
char *setcat();
int setlabel();
int addsev();
#endif

#define	DB_NAME_LEN		15
#define	MAXLABEL		25

#ifdef	__cplusplus
}
#endif

#endif	/* _PFMT_H */
