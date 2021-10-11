/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ifndef	LOG_MGMT_H
#define	LOG_MGMT_H
/*==================================================================*/
/*
**
*/
#ident	"@(#)logMgmt.h	1.3	90/03/01 SMI"	/* SVr4.0 1.4	*/

#include	"boolean.h"


/*------------------------------------------------------------------*/
/*
*/
#ifdef	__STDC__

extern	void	WriteLogMsg (char *);
extern	void	SetLogMsgTagFn (char *(*) ());
extern	boolean	OpenLogFile (char *);

#else

extern	void	WriteLogMsg ();
extern	void	SetLogMsgTagFn ();
extern	boolean	OpenLogFile ();

#endif
/*==================================================================*/
#endif
