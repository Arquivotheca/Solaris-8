/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)storepri.c	1.7	97/05/14 SMI"	/* SVr4.0 1.6	*/
/* LINTLIBRARY */

# include	<stdio.h>

# include	"lp.h"
# include	"users.h"
# include	<locale.h>

/*
Inputs:
Outputs:
Effects:
*/
void
print_tbl(struct user_priority * ppri_tbl)
{
    int limit;

    printf(gettext("Default priority: %d\n"), ppri_tbl->deflt);
    printf(gettext("Priority limit for users not listed below: %d\n"), ppri_tbl->deflt_limit);
    printf(gettext("Priority  Users\n"));
    printlist_setup ("", "", ",", "\n");
    for (limit = PRI_MIN; limit <= PRI_MAX; limit++) {
	if (ppri_tbl->users[limit - PRI_MIN])
	{
	    printf("   %2d     ", limit);
	    fdprintlist(1, ppri_tbl->users[limit - PRI_MIN]);
	}
    }
}

/*
Inputs:
Outputs:
Effects:
*/
void
output_tbl(int fd, struct user_priority *ppri_tbl)
{
    int		limit;

    fdprintf(fd, "%d\n%d:\n", ppri_tbl->deflt, ppri_tbl->deflt_limit);
    printlist_setup ("	", "\n", "", "");
    for (limit = PRI_MIN; limit <= PRI_MAX; limit++)
	if (ppri_tbl->users[limit - PRI_MIN])
	{
	    fdprintf(fd, "%d:", limit);
	    fdprintlist(fd, ppri_tbl->users[limit - PRI_MIN]);
	}
}
