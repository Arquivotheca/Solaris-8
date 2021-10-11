/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)msgdefs.h	1.7	93/03/09 SMI"	/* SVr4.0  1.2	*/

#define	ASK_CONTINUE	"Do you want to continue with package removal"

#define	ERR_NOPKGS	"no packages were found in <%s>"

#define	ERR_CHDIR	"unable to change directory to <%s>"

#define	MSG_SUSPEND	"Removals of <%s> has been suspended."

#define	MSG_1MORETODO	"\nThere is 1 more package to be removed."

#define	MSG_MORETODO	"\nThere are %d more packages to be removed."

#define	ERR_NOTROOT	"You must be \"root\" for %s to execute properly."

#define	INFO_SPOOLED	"\nThe following package is currently spooled:"

#define	INFO_INSTALL	"\nThe following package is currently installed:"

#define	INFO_RMSPOOL	"\nRemoving spooled package instance <%s>"
