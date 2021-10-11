/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_invari.h 1.1	96/01/17 SMI"

/*
 * Copyright 1993 by Mortice Kern Systems Inc.  All rights reserved.
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
 * $Header: /rd/h/rcs/m_invari.h 1.12 1994/07/06 17:41:39 miked Exp $
 */

/*
 * m_invari.h: 
 *    Configuration and definitions for support of on systems (e.g EBCDIC)
 *    where the POSIX.2 portable characters are not invariant
 */

#ifndef __M_M_INVARI_H__
#define	__M_M_INVARI_H__

/*
 * Are all characters in the portable character set invariant across
 * all locales?  The results, for posix, if not, are undefined.
 * For systems that want to try to deal with this, M_VARIANTS is set.
 */
#ifdef	M_VARIANTS

extern char	__m_invariant[];
extern char	__m_unvariant[];

/*
 * The M_INVARIANTINIT macro must be called to initialize: it returns -1
 * on memory allocation error.  It may be called multiple times, but has
 * no effect after the first call.  To reinitialize the variant <-->
 * invariant tables after a new setlocale(), use M_INVARIANTREINIT().
 * On error, m_error will have been invoked with an appropriate message.
 */
#define	M_INVARIANTINIT()	m_invariantinit()
extern void	m_invariantinit(void);
#define	M_INVARIANTREINIT()	__m_setinvariant()
extern void	__m_setinvariant(void);

/*
 * Assume wide characters are always ok.
 * Otherwise, always indirect thru the narrow un/invariant table.
 * INVARIANT takes the character in the current locale, and produces an
 * invariant value, equal to that the C compiler would have compiled.
 * UNVARIANT is the inverse; it takes what the C compiler would have
 * compiled, and returns the value in the current locale.
 */
#define	M_INVARIANT(c)	(wctob(c) == EOF ? (c) : __m_invariant[c])
#define	M_UNVARIANT(c)	(wctob(c) == EOF ? (c) : __m_unvariant[c])
#define	M_UNVARIANTSTR(s)	m_unvariantstr(s)
char *m_unvariantstr(char const *);
#define	M_WUNVARIANTSTR(ws)	m_wunvariantstr(ws)
wchar_t *m_wunvariantstr(wchar_t const *);

#else	/* M_VARIANTS */

/* Normal system */
#define	M_INVARIANTINIT()	/* NULL */
#define M_INVARIANTREINIT()	/* NULL */
#define	M_INVARIANT(c)		(c)
#define	M_UNVARIANT(c)		(c)
#define	M_UNVARIANTSTR(s)	(s)
#define	M_WUNVARIANTSTR(ws)	(ws)

#endif	/* M_VARIANTS */

#endif /*__M_M_INVARI_H__*/



