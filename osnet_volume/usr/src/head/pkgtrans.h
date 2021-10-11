/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PKGTRANS_H
#define	_PKGTRANS_H

#pragma ident	"@(#)pkgtrans.h	1.7	92/07/14 SMI"	/* SVr4.0 1.2.1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	PT_OVERWRITE	0x01
#define	PT_INFO_ONLY	0x02
#define	PT_RENAME	0x04
#define	PT_DEBUG	0x08
#define	PT_SILENT	0x10
#define	PT_ODTSTREAM	0x40

#ifdef	__cplusplus
}
#endif

#endif	/* _PKGTRANS_H */
