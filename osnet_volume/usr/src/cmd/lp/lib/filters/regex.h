/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)regex.h	1.3	90/03/01 SMI"	/* SVr4.0 1.2	*/


#if	defined(__STDC__)

int		match ( char * , char * );
size_t		replace ( char ** , char * , char * , int );

#else

int		match();
size_t		replace();

#endif
