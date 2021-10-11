/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)groups.c	1.8	95/10/24 SMI"	/* SVr4.0 1.4	*/

/*
 * groups - show group memberships
 */
/*LINTLIBRARY PROTOLIB1*/

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

extern struct group *getgrgid();
extern struct passwd *getpwnam();
extern int _getgroupsbymember(const char *, gid_t[], int, int); 

static void showgroups();

static int ngroups_max;

void
main(argc, argv)
	int argc;
	char *argv[];
{
	register int xval = 0;
	register struct passwd *pw;

	ngroups_max = sysconf(_SC_NGROUPS_MAX);

	if (ngroups_max < 0) {
		(void)fprintf(stderr, "groups: could not get configuration info\n");
		exit(1);
	}

	if (ngroups_max == 0)
		exit(0);

	if (argc == 1) {

		if ((pw = getpwuid(getuid())) == NULL) {
			(void)fprintf(stderr, "groups: No passwd entry\n");
			xval = 1;
		} else
			showgroups(pw);

	} else while (*++argv) {

		if ((pw = getpwnam(*argv)) == NULL) {
			(void)fprintf(stderr, "groups: %s : No such user\n", *argv);
			xval = 1;
		} else {
			if (argc > 2)
				(void)printf("%s : ", *argv);
			showgroups(pw);
		}
	}

	exit(xval);

}

static void
showgroups(pw)
	register struct passwd *pw;
{
	register struct group *gr;
	static gid_t *groups = NULL;
	int ngroups;
	int i;

	if (ngroups_max == 0)
		return;

	if (groups == NULL) {
		if ((groups = (gid_t *)calloc((uint)ngroups_max,
						sizeof (gid_t))) == 0) {
			(void)fprintf(stderr, "allocation of %d bytes failed\n",
				ngroups_max * sizeof (gid_t));
			exit(1);
		}
	}
	groups[0] = pw->pw_gid;

	ngroups = _getgroupsbymember(pw->pw_name, groups, ngroups_max, 1);

	if (gr = getgrgid(groups[0]))
		(void)printf("%s", gr->gr_name);
	else
		(void)printf("%d", (int)pw->pw_gid);

	for (i = 1; i < ngroups; i++) {
		if ((gr = getgrgid(groups[i])))
			(void)printf(" %s", gr->gr_name);
		else
			(void)printf(" %d", (int)gr->gr_gid);
	}

	(void)printf("\n");
}
