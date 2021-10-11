/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#ifndef _OTERMCAP_H
#define	_OTERMCAP_H

#pragma ident	"@(#)otermcap.h	1.6	97/06/25 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

#define	TBUFSIZE	2048		/* double the norm */

/* externs from libtermcap.a */
extern int	otgetflag(char *), otgetnum(char *),
		otgetent(char *, char *), tnamatch(char *), tnchktc(void);
extern char	*otgetstr(char *, char **);
extern char	*tskip(char *);			/* non-standard addition */
extern int	TLHtcfound;			/* non-standard addition */
extern char	TLHtcname[];		/* non-standard addition */

#ifdef	__cplusplus
}
#endif

#endif	/* _OTERMCAP_H */
