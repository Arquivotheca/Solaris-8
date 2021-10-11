/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PW_H
#define	_PW_H

#pragma ident	"@(#)pw.h	1.8	92/07/14 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

extern char *logname(void);
extern char *regcmp(const char *, ...);
extern char *regex(const char *, const char *, ...);
#else
extern char *logname();
extern char *regcmp();
extern char *regex();

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _PW_H */
