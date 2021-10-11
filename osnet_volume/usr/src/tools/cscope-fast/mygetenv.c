/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mygetenv.c	1.1	99/01/11 SMI"

/* return the non-null environment value or the default argument */

#include <stdlib.h>
#include <stdio.h>

char *
mygetenv(char *variable, char *deflt)
{
	char	*value;

	value = getenv(variable);
	if (value == (char *)NULL || *value == '\0') {
		return (deflt);
	}
	return (value);
}
