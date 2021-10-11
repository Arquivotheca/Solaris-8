/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mygetwd.c	1.1	99/01/11 SMI"

#include <sys/types.h>	/* needed by stat.h */
#include <sys/stat.h>	/* stat */
#include <stdio.h>	/* NULL */
#include <string.h>	/* string functions */
#include <stdlib.h>

/*
 * if the ksh PWD environment variable matches the current
 * working directory, don't call getwd()
 */

char *
mygetwd(char *dir)
{
	char	*pwd;			/* PWD environment variable value */
	struct	stat	d_sb;  		/* current directory status */
	struct	stat	tmp_sb; 	/* temporary stat buffer */
	char	*getwd();

	/* get the current directory's status */
	if (stat(".", &d_sb) < 0) {
		return (NULL);
	}
	/* use $PWD if it matches this directory */
	if ((pwd = getenv("PWD")) != NULL && *pwd != '\0' &&
	    stat(pwd, &tmp_sb) == 0 &&
	    d_sb.st_ino == tmp_sb.st_ino && d_sb.st_dev == tmp_sb.st_dev) {
		(void) strcpy(dir, pwd);
		return (pwd);
	}
	return (getwd(dir));
}
