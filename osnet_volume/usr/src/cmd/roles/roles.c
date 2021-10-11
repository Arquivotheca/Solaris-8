/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident	"@(#)roles.c	1.1	99/04/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <user_attr.h>


#define	EXIT_OK		0
#define	EXIT_FATAL	1
#define	EXIT_NON_FATAL	2

#ifndef	TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif


static void usage();
static int show_roles(char *, int);
static void print_roles(char *, char *, int *);


char *progname = "roles";


main(int argc, char *argv[])
{
	int		print_name = FALSE;
	register int	status = EXIT_OK;

	setlocale(LC_ALL, "");
	textdomain(TEXT_DOMAIN);

	switch (argc) {
	case 1:
		status = show_roles((char *)NULL, print_name);
		break;
	case 2:
		status = show_roles(argv[argc-1], print_name);
		break;
	default:
		print_name = TRUE;
		while (*++argv) {
			status = show_roles(*argv, print_name);
			if (status == EXIT_FATAL) {
				break;
			}
		}
		break;
	}
	status = (status == EXIT_OK) ? status : EXIT_FATAL;

	exit(status);
}


static int
show_roles(char *username, int print_name)
{
	register int		status = EXIT_OK;
	register char		*rolelist = (char *)NULL;
	register struct passwd	*pw;
	register userattr_t	*user;

	if (username == NULL) {
		if ((pw = getpwuid(getuid())) == NULL) {
			status = EXIT_NON_FATAL;
			fprintf(stderr, "%s: ", progname);
			fprintf(stderr, gettext("No passwd entry\n"));
			return (status);
		}
		username = pw->pw_name;
	} else if ((pw = getpwnam(username)) == NULL) {
		status = EXIT_NON_FATAL;
		fprintf(stderr, "%s: %s : ", progname, username);
		fprintf(stderr, gettext("No such user\n"));
		return (status);
	}
	if (username != NULL) {
		if ((user = getusernam(username)) != NULL) {
			rolelist = kva_match(user->attr, USERATTR_ROLES_KW);
			if (rolelist == NULL) {
				status = EXIT_NON_FATAL;
			}
			free_userattr(user);
		} else {
			status = EXIT_NON_FATAL;
		}
	}
	if (status == EXIT_NON_FATAL) {
		fprintf(stderr, "%s: %s : ", progname, username);
		fprintf(stderr, gettext("No roles\n"));
	} else if (print_name == TRUE) {
		printf("%s : %s\n", username, rolelist);
	} else {
		printf("%s\n", rolelist);
	}

	return (status);
}


static void
usage()
{
	fprintf(stderr, gettext("  usage: roles [user1 user2 ...]\n"));
}
