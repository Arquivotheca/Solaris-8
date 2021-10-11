/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)access.h	1.4	93/04/02 SMI"	/* SVr4.0 1.7	*/

#if	!defined(_LP_ACCESS_H)
#define	_LP_ACCESS_H

#include "stdio.h"

/*
 * To speed up reading in each allow/deny file, ACC_MAX_GUESS slots
 * will be preallocated for the internal copy. If these files
 * are expected to be substantially larger than this, bump it up.
 */
#define ACC_MAX_GUESS	100

int	allow_form_printer ( char **, char * );
int	allow_user_form ( char ** , char * );
int	allow_user_printer ( char **, char * );
int	allowed ( char *, char **, char ** );
int	deny_form_printer ( char **, char * );
int	deny_user_form ( char ** , char * );
int	deny_user_printer ( char **, char * );
int	dumpaccess ( char *, char *, char *, char ***, char *** );
int	is_form_allowed_printer ( char *, char * );
int	is_user_admin ( void );
int	is_user_allowed ( char *, char ** , char ** );
int	is_user_allowed_form ( char *, char * );
int	is_user_allowed_printer ( char *, char * );
int	load_formprinter_access ( char *, char ***, char *** );
int	load_paperprinter_access(char *, char ***, char ***);
int	load_userform_access ( char *, char ***, char *** );
int	load_userprinter_access ( char *, char ***, char *** );
int	loadaccess ( char *, char *, char *, char ***, char *** );
int	bangequ ( char * , char * );
int	bang_searchlist ( char * , char ** );
int	bang_dellist ( char *** , char * );

char *	getaccessfile ( char *, char *, char *, char * );

#endif
