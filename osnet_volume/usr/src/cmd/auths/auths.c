/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident	"@(#)auths.c	1.1	99/05/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <deflt.h>
#include <libintl.h>
#include <locale.h>
#include <user_attr.h>
#include <prof_attr.h>
#include <auth_attr.h>


#define	ALL_AUTHS	"All"
#define	ALL_SUN_AUTHS	"com.sun.*"

#define	EXIT_OK		0
#define	EXIT_FATAL	1
#define	EXIT_NON_FATAL	2

#ifndef	TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif


static void usage();
static int show_auths(char *, char **, int);
static int list_auths(userattr_t *, char **);
static char *get_default_auths(void);
static int realloc_allauths(char **, char *, userattr_t *);


char *progname = "auths";


main(int argc, char *argv[])
{
	int		print_name = FALSE;
	register int	status = EXIT_OK;
	register char	*defauths = NULL;
	char		*allauths = NULL;

	setlocale(LC_ALL, "");
	textdomain(TEXT_DOMAIN);

	if ((defauths = get_default_auths()) != NULL) {
		if ((strcmp(defauths, KV_WILDCARD) == 0) ||
		    (strcmp(defauths, ALL_SUN_AUTHS) == 0)) {
			allauths = ALL_AUTHS;
		} else {
			status = realloc_allauths(&allauths, defauths, NULL);
			if (status == EXIT_FATAL) {
				exit(EXIT_FATAL);
			}
		}
	}
	switch (argc) {
	case 1:
		status = show_auths(NULL, &allauths, print_name);
		break;
	case 2:
		status = show_auths(argv[argc-1], &allauths, print_name);
		break;
	default:
		print_name = TRUE;
		while (*++argv) {
			status = show_auths(*argv, &allauths, print_name);
			if (status == EXIT_FATAL) {
				break;
			}
			if ((allauths == NULL) ||
			    (strcmp(allauths, ALL_AUTHS) == 0)) {
				continue;
			}
			if (defauths == NULL) {
				memset(allauths, 0, strlen(allauths));
			} else {
				strcpy(allauths, defauths);
			}
		}
		break;
	}
	if (allauths != NULL) {
		free(allauths);
	}
	status = (status == EXIT_OK) ? status : EXIT_FATAL;

	exit(status);
}


static int
show_auths(char *username, char **allauths, int print_name)
{
	register int		status = EXIT_OK;
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
		if ((*allauths != NULL) &&
		    (strcmp(*allauths, ALL_AUTHS) == 0)) {
			status = EXIT_OK;
		} else if ((user = getusernam(username)) != NULL) {
			status = list_auths(user, allauths);
		} else if ((*allauths == NULL) || (**allauths == NULL)) {
			status = EXIT_NON_FATAL;
		}
	}
	if (status == EXIT_NON_FATAL) {
		fprintf(stderr, "%s: %s : ", progname, username);
		fprintf(stderr, gettext("No authorizations\n"));
	} else if (print_name == TRUE) {
		printf("%s : %s\n", username, *allauths);
	} else {
		printf("%s\n", *allauths);
	}

	return (status);
}


static int
list_auths(userattr_t *user, char **allauths)
{
	register int		status = EXIT_OK;
	const char		*sep = ",";
	char			*authlist = NULL;
	char			*last;
	char			*proflist = NULL;
	char			*profname = NULL;
	register profattr_t	*prof;

	authlist = kva_match(user->attr, USERATTR_AUTHS_KW);
	if (authlist != NULL) {
		status = realloc_allauths(allauths, authlist, user);
		if (status == EXIT_FATAL) {
			return (status);
		}
	}
	if ((proflist = kva_match(user->attr, USERATTR_PROFILES_KW)) == NULL) {
		if (authlist == NULL) {
			status = EXIT_NON_FATAL;
		}
	} else {
		for (profname = strtok_r(proflist, sep, &last);
		    profname != NULL;
		    profname = strtok_r(NULL, sep, &last)) {
			if ((prof = getprofnam(profname)) == NULL) {
				continue;
			}
			if ((authlist = kva_match(prof->attr,
			    PROFATTR_AUTHS_KW)) == NULL) {
				continue;
			}
			status = realloc_allauths(allauths, authlist, user);
			if (status == EXIT_FATAL) {
				free_profattr(prof);
				free_userattr(user);
				return (status);
			}
			free_profattr(prof);
		}
	}
	if ((*allauths == NULL) || (**allauths == NULL)) {
		status = EXIT_NON_FATAL;
	}
	free_userattr(user);

	return (status);
}


static char *
get_default_auths()
{
	char *auths = NULL;

	if (defopen(AUTH_POLICY) == NULL) {
		auths = defread(DEF_AUTH);
	}

	return (auths);
}


static int
realloc_allauths(char **allauths, char *auths, userattr_t *user)
{
	register int		status = EXIT_OK;
	register int		all_len = 0;
	register int		newauths = FALSE;
	register const char	*sep = ",";
	register const char	*newstr = NULL;
	char			*last;

	if (auths == NULL) {
		return (EXIT_NON_FATAL);
	}
	if (*allauths == NULL) {
		newauths = TRUE;
		all_len = strlen(auths) + 1;
		*allauths = (char *)calloc(1, all_len);
	} else {
		all_len = strlen(*allauths) + strlen(auths) + 1;
		if (all_len > strlen(*allauths)) {
			*allauths = (char *)realloc(*allauths, all_len);
		}
	}
	if (*allauths == NULL) {
		status = EXIT_FATAL;
		fprintf(stderr, "%s: ", progname);
		fprintf(stderr, gettext("Insufficient memory\n"));
		if (user != NULL) {
			free_userattr(user);
		}
		return (status);
	}
	if ((newauths == TRUE) || (**allauths == '\0')) {
		strcat(*allauths, auths);
	} else {
		for (newstr = strtok_r(auths, sep, &last);
		    newstr != NULL;
		    newstr = strtok_r(NULL, sep, &last)) {
			if (strstr(*allauths, newstr) == NULL) {
				sprintf(*allauths, "%s,%s", *allauths, newstr);
			}
		}
	}

	return (status);
}


static void
usage()
{
	fprintf(stderr, gettext("  usage: auths [user1 user2 ...]\n"));
}
