/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_ord.h 1.1	96/01/17 SMI"

/*
 * m_ord.h
 *
 * Copyright 1986, 1992 by Mortice Kern Systems Inc.  All rights reserved.
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
 * $Header: /rd/h/rcs/m_ord.h 1.15 1994/05/29 16:17:02 mark Exp $
 */

#ifndef __M_M_ORD_H__
#define __M_M_ORD_H__

#ifndef UCHAR_MAX
#include <limits.h>
#endif

/*
 * Used with CURSES in order to decern whether or not 'x' is a byte
 * or a KEY_xxxx macro, which are defined to be values greater than 
 * UCHAR_MAX.
 */
#define m_ischarset(x)	((unsigned)(x) <= UCHAR_MAX)

#ifdef M_NOT_646
LEXTERN int m_ord ANSI((wint_t));
LEXTERN wint_t m_chr ANSI((int));
#else
/* ASCII based macros */
/*
 * m_ord(c) : convert alpha character(case insensitive) to an an ordinal value.
 *            if c is an alphabetic character (A-Z,a-z), this returns
 *            a number between 1 and 26
 * m_chr(i) : convert an ordinal value to its corresponding alpha character
 *            using the reverse mapping as m_ord().
 *            if i is a number between 1 and 26 it returns the corresponding
 *  	      alphabetic character A to Z
 */
#include <ctype.h>
#define m_ord(c) \
	(m_ischarset(c)&&('A'<=towupper(c)&&towupper(c)<='Z')?towupper(c)-'@':-1) 
#define m_chr(c)	(1 <= c && c <= 26 ? c+'@' : -1)
#endif /* _POSIX_SOURCE */

#endif /* __M_M_ORD_H__ */
