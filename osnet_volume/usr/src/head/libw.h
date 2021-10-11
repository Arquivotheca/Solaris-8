/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _LIBW_H
#define	_LIBW_H

#pragma ident	"@(#)libw.h	1.10	96/12/09 SMI"	/* SVr4.0 1.1	*/

#include <stdlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _EUCWIDTH_T
#define	_EUCWIDTH_T
typedef struct {
	short int _eucw1, _eucw2, _eucw3;	/*	EUC width	*/
	short int _scrw1, _scrw2, _scrw3;	/*	screen width	*/
	short int _pcw;		/*	WIDE_CHAR width	*/
	char _multibyte;	/*	1=multi-byte, 0=single-byte	*/
} eucwidth_t;
#endif

#ifdef __STDC__
void getwidth(eucwidth_t *);
#else
void getwidth();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBW_H */
