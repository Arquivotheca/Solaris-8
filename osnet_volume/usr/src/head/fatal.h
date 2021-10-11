/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_FATAL_H
#define	_FATAL_H

#pragma ident	"@(#)fatal.h	1.7	92/07/14 SMI"	/* SVr4.0 1.4.1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

extern	int	Fflags;
extern	char	*Ffile;
extern	int	Fvalue;
extern	int	(*Ffunc)();
extern	int	Fjmp[10];

#define	FTLMSG		0100000
#define	FTLCLN		0040000
#define	FTLFUNC		0020000
#define	FTLACT		0000077
#define	FTLJMP		0000002
#define	FTLEXIT		0000001
#define	FTLRET		0000000

#define	FSAVE(val)	SAVE(Fflags, old_Fflags); Fflags = val;
#define	FRSTR()	RSTR(Fflags, old_Fflags);

#ifdef	__cplusplus
}
#endif

#endif	/* _FATAL_H */
