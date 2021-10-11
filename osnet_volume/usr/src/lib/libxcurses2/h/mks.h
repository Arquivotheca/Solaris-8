/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mks.h 1.2	98/08/08 SMI"

/*
 * MKS header file.  Defines that make programming easier for us.
 * Includes MKS-specific things and posix routines.
 *
 * Copyright 1985, 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 *
 * $Header: /rd/h/rcs/mks.h 1.233 1995/09/28 19:45:19 mark Exp $
 */

#ifndef	__M_MKS_H__
#define	__M_MKS_H__

/*
 * This should be a feature test macro defined in the Makefile or
 * cc command line.
 */
#ifndef	MKS
#define	MKS	1
#endif

typedef	void	(*_sigfun_t)(int);

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

#define	M_TERMINFO_DIR		"/usr/share/lib/terminfo"
#define	M_CURSES_VERSION	"MKS I/XCU 4.3 Curses"

/*
 * MKS-specific library entry points.
 */
#if defined(_LP64)
extern void	m_crcposix(unsigned int *, const unsigned char *, size_t);
#else
extern void	m_crcposix(unsigned long *, const unsigned char *, size_t);
#endif

#endif	/* __M_MKS_H__ */
