/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)print.h	1.17	99/01/05 SMI"	/* SVr4.0 1.9	*/

#include "file64.h"
#include <floatingpoint.h>
#include <thread.h>
#include <synch.h>
#include <stdio.h>
#include "stdiom.h"

extern ssize_t
_doprnt(const char *format, va_list in_args, FILE *iop);

extern ssize_t
_dostrfmon_unlocked(size_t limit_cnt, const char *format, \
    va_list in_args, FILE *iop);

/* Maximum number of digits in any integer representation */
#define	MAXDIGS 11

/* Maximum number of digits in any long long representation */
#define	MAXLLDIGS 21

/* Maximum total number of digits in E format */
#define	MAXECVT (DECIMAL_STRING_LENGTH-1)

/* Maximum number of digits after decimal point in F format */
#define	MAXFCVT (DECIMAL_STRING_LENGTH-1)

/* Maximum significant figures in a floating-point number	*/
/* DECIMAL_STRING_LENGTH in floatingpoint.h is max buffer size	*/
#define	MAXFSIG (DECIMAL_STRING_LENGTH-1)

/* Maximum number of characters in an exponent */
#define	MAXESIZ 6		/* Max for quadruple precision */

/* Maximum (positive) exponent */
#define	MAXEXP 4950		/* Max for quadruple precision */

/* Data type for flags */
typedef char bool;

/* Convert a digit character to the corresponding number */
#define	tonumber(x) ((x)-'0')

/* Convert a number between 0 and 9 to the corresponding digit */
#define	todigit(x) ((x)+'0')

/* Max and Min macros */
#define	max(a, b) ((a) > (b)? (a): (b))
#define	min(a, b) ((a) < (b)? (a): (b))

/* Max neg. long long */
#define	HIBITLL	(1ULL << 63)
