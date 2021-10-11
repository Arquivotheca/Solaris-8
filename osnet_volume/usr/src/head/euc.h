/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _EUC_H
#define	_EUC_H

#pragma ident	"@(#)euc.h	1.8	96/06/26 SMI"

#include <sys/euc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
extern int csetcol(int n);	/* Returns # of columns for codeset n. */
extern int csetlen(int n);	/* Returns # of bytes excluding SSx. */
extern int euclen(const unsigned char *s);
extern int euccol(const unsigned char *s);
extern int eucscol(const unsigned char *str);
#else	/* __STDC__ */
extern int csetlen(), csetcol();
extern int euclen(), euccol(), eucscol();
#endif	/* __STDC__ */

/* Returns code set number for the first byte of an EUC char. */
#define	csetno(c) \
	(((c)&0x80)?(((c)&0xff) == SS2)?2:((((c)&0xff) == SS3)?3:1):0)

/*
 * Copied from _wchar.h of SVR4
 */
#if defined(__STDC__)
#define	multibyte	(__ctype[520] > 1)
#define	eucw1		__ctype[514]
#define	eucw2		__ctype[515]
#define	eucw3		__ctype[516]
#define	scrw1		__ctype[517]
#define	scrw2		__ctype[518]
#define	scrw3		__ctype[519]
#else
#define	multibyte	(_ctype[520] > 1)
#define	eucw1		_ctype[514]
#define	eucw2		_ctype[515]
#define	eucw3		_ctype[516]
#define	scrw1		_ctype[517]
#define	scrw2		_ctype[518]
#define	scrw3		_ctype[519]
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _EUC_H */
