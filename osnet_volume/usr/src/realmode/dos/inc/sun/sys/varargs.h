/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)varargs.h	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		varargs.h
 *
 *   Description:	contains implementation-specific details of
 *			variable arguments (used by printf)
 *
 */
/*	Copyright (c) 1984 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* the original ISC 3.0 header file from skylark */

typedef char _FAR_ *va_list;
/* #define va_dcl int va_alist; the original version */
#define va_dcl long va_alist
#define va_start(list, fmt) list = (char _FAR_ *) &va_alist
#define va_end(list)
#ifdef u370
#define va_arg(list, mode) ((mode *)(list = \
	(char *) ((int)list + 2*sizeof(mode) - 1 & -sizeof(mode))))[-1]
#else
#define va_arg(list, mode) ((mode _FAR_ *)(list += sizeof(mode)))[-1]
#endif
