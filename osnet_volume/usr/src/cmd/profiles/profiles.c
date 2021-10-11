/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident	"@(#)profiles.c	1.1	99/06/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <user_attr.h>
#include <prof_attr.h>
#include <exec_attr.h>


#define	EXIT_OK		0
#define	EXIT_FATAL	1
#define	EXIT_NON_FATAL	2

#define	MAX_LINE_LEN	80		/* max 80 chars per line of output */
#define	TMP_BUF_LEN	128		/* size of temp string buffer */

#define	PRINT_DEFAULT	0x0000
#define	PRINT_NAME	0x0010
#define	PRINT_LONG	0x0020

#ifndef TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif


static void usage();
static int show_profs(char *, int *);
static int list_profs(userattr_t *, int *);
static void print_profs(char *, void *, int *);
static void format_attr(int *, int, char *, int);


char *progname = "profiles";


main(int argc, char *argv[])
{
	extern int	optind;
	register int	i;
	register int	c;
	register int	status = EXIT_OK;
	int		print_flag = PRINT_DEFAULT;

	setlocale(LC_ALL, "");
	textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "l")) != EOF) {
		switch (c) {
		case 'l':
			print_flag |= PRINT_LONG;
			break;
		default:
			usage();
			exit(EXIT_FATAL);
		}
	}
	argc -= optind;
	argv += optind;

	if (*argv == NULL) {
		status = show_profs((char *)NULL, &print_flag);
	} else {
		do {
			status = show_profs(*argv, &print_flag);
			if (status == EXIT_FATAL) {
				break;
			}
		} while (*++argv);
	}
	status = (status == EXIT_OK) ? status : EXIT_FATAL;

	exit(status);
}


static int
show_profs(char *username, int *print_flag)
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
		if ((user = getusernam(username)) != NULL) {
			status = list_profs(user, print_flag);
		} else {
			status = EXIT_NON_FATAL;
		}
	}

	if (status == EXIT_NON_FATAL) {
		fprintf(stderr, "%s: %s : ", progname, username);
		fprintf(stderr, gettext("No profiles\n"));
	}

	return (status);
}


static int
list_profs(userattr_t *user, int *print_flag)
{
	register int	status = EXIT_OK;
	char		*proflist = (char *)NULL;
	execattr_t	*exec = (execattr_t *)NULL;

	if (*print_flag & PRINT_LONG) {
		exec = getexecuser(user->name, KV_COMMAND, NULL, GET_ALL);
		if (exec == NULL) {
			status = EXIT_NON_FATAL;
		}
	} else {
		proflist = kva_match(user->attr, USERATTR_PROFILES_KW);
		if (proflist == NULL) {
			status = EXIT_NON_FATAL;
		}
	}
	if (status == EXIT_OK) {
		if (*print_flag & PRINT_LONG) {
			print_profs(user->name, exec, print_flag);
			free_execattr(exec);
		} else {
			print_profs(user->name, proflist, print_flag);
		}
	}
	free_userattr(user);

	return (status);
}


static void
print_profs(char *user, void *data, int *print_flag)
{

	register int		i;
	register int		len;
	int			outlen;
	register int		newline = FALSE;
	char			tmpstr[TMP_BUF_LEN];
	register char		*empty = "";
	register char		*lastname = empty;
	register char		*outstr;
	register char		*key;
	register char		*val;
	register kv_t		*kv_pair;
	register execattr_t	*exec;

	memset(tmpstr, NULL, TMP_BUF_LEN);
	if (*print_flag & PRINT_NAME) {
		printf("%s : ", user);
	}
	if (*print_flag & PRINT_LONG) {
		printf("\n");
		exec = (execattr_t *)data;
		while (exec != (execattr_t *)NULL) {
			if (strcmp(exec->name, lastname) != NULL) {
				sprintf(tmpstr, "      %s:", exec->name);
				printf("%s\n", tmpstr);
			}
			sprintf(tmpstr, "          %s    ", exec->id);
			outlen = strlen(tmpstr);
			len = outlen;
			printf("%s", tmpstr);
			if ((exec->attr == NULL) ||
			    (kv_pair = exec->attr->data) == NULL) {
				printf("\n");
				lastname = exec->name;
				exec = exec->next;
				continue;
			}
			for (i = 0; i < exec->attr->length; i++) {
				key = kv_pair[i].key;
				val = kv_pair[i].value;
				if ((key == NULL) || (val == NULL)) {
					break;
				}
				if (i > 0) {
					strncpy(tmpstr, ", ", TMP_BUF_LEN);
					format_attr(&outlen, len, tmpstr,
					    FALSE);
				}
				sprintf(tmpstr, "%s=%s", key, val);
				format_attr(&outlen, len, tmpstr, TRUE);
			}
			printf("\n");
			lastname = exec->name;
			exec = exec->next;
		}
	} else {
		outstr = (char *)data;
		printf("%s\n", outstr);
	}
}


static void
format_attr(int *outlen, int len, char *str, int keyval)
{
	int newline = FALSE;

	if (keyval == TRUE) {
		if ((MAX_LINE_LEN - *outlen) < strlen(str)) {
			newline = TRUE;
		}
	} else {
		if ((MAX_LINE_LEN - *outlen) < strlen(str)) {
			newline = TRUE;
		}
	}
	if (newline == TRUE) {
		printf("\n");
		len += strlen(str);
		printf("%*s", len, str);
		*outlen = len;
	} else {
		*outlen += strlen(str);
		printf("%s", str);
	}
}

static void
usage()
{
	fprintf(stderr,
	    gettext("  usage: profiles [-l] [user1 user2 ...]\n"));
}
