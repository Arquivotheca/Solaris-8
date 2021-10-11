/*
 * Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
 *
 */

#ident	"@(#)homedir.c	1.2	92/07/14 SMI"

#include <pwd.h>
#include <stdio.h>

/* homedir: returns home directory of a given user.
 * return status: 0 if successful;
 *		  1 if not.
 */

main()
{
	struct passwd *getpwnam();
	struct passwd *pwstruct;
	char username[20];

	scanf("%s", username);
	pwstruct = getpwnam(username);
	if (pwstruct == NULL) {
		printf("NONE\n");
		exit(1);
	}
	printf("%s\n", pwstruct->pw_dir);
	exit(0);
}
