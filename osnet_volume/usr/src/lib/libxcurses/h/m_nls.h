/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_nls.h 1.1	96/01/17 SMI"

/*
 * m_nls.h: mks NLS (National Language Support) header file
 * The client may choose to use a different messaging scheme than the xpg
 * one -- in that case this file will be replaced.
 *
 * Copyright 1992, 1995 by Mortice Kern Systems Inc.  All rights reserved.
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
 * $Header: /rd/h/rcs/m_nls.h 1.5 1995/01/20 01:42:10 fredw Exp $
 */

#ifndef	__M_NLS_H__
#define	__M_NLS_H__

extern char	*m_nlspath(const char* catalog, int mode);

#define	m_textstr(id, str, cls)	#id "##" str
extern void	 m_textdomain(char * str);
extern char	*m_textmsg(int id, const char *str, char *cls);
extern char	*m_strmsg(const char *str);
/*l
 * The following two routines may need to be defined, if you need
 * to do special processing:
 *
 * extern char *m_msgdup(char *m);
 * extern void m_msgfree(char *m);
 */
#define m_msgdup(m) (strdup(m))
#define m_msgfree(m) (free(m))

#endif /*__M_NLS_H__*/
