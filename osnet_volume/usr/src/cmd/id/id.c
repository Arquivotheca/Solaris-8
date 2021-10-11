/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/

#ident	"@(#)id.c 1.18	97/03/04 SMI"	/* SVr4.0 1.8   */

#include <locale.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#define	PWNULL  ((struct passwd *)0)
#define	GRNULL  ((struct group *)0)

typedef enum TYPE {
	UID, EUID, GID, EGID, SGID
}	TYPE;

typedef enum PRINT {
	CURR,		/* Print uid/gid only */
	ALLGROUPS,	/* Print all groups */
	GROUP,		/* Print only group */
	USER		/* Print only uid */
}	PRINT;
static PRINT mode = CURR;

static int usage(void);
static void puid(uid_t);
static void pgid(gid_t);
static void prid(TYPE, uid_t);
static int getusergroups(int, gid_t *, char *, gid_t);

static int nflag = 0;		/* Output names, not numbers */
static int rflag = 0;		/* Output real, not effective IDs */

int
main(argc, argv)
int argc;
char **argv;
{
	gid_t *idp;
	uid_t uid, euid;
	gid_t gid, egid, prgid;
	static char stdbuf[BUFSIZ];
	static int c, aflag = 0;
	register struct passwd *pwp;
	register int i, j;
	gid_t groupids[NGROUPS_UMAX];
	static char firstsup = 1;
	struct group *gr;
	char *user = NULL;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
#ifdef XPG4
	while ((c = getopt(argc, argv, "Ggunr")) != EOF) {
#else
	while ((c = getopt(argc, argv, "a")) != EOF) {
#endif
		switch (c) {
			case 'G':
				if (mode != CURR)
					return (usage());
				else
					mode = ALLGROUPS;
				break;

			case 'g':
				if (mode != CURR)
					return (usage());
				else
					mode = GROUP;
				break;

#ifndef XPG4
			case 'a':
				aflag++;
				break;
#endif

			case 'n':
				nflag++;
				break;

			case 'r':
				rflag++;
				break;

			case 'u':
				if (mode != CURR)
					return (usage());
				else
					mode = USER;
				break;

			case '?':
				return (usage());
		}
	}
	setbuf(stdout, stdbuf);
	argc -= optind-1;
	argv += optind-1;

	/* -n and -r must be combined with one of -[Ggu] */
	/* -r cannot be combined with -G */

	if ((mode == CURR && (nflag || rflag)) ||
		(mode == ALLGROUPS && rflag) ||
		(argc != 1 && argc != 2))
		return (usage());
	if (argc == 2) {
		if ((pwp = getpwnam(argv[1])) == PWNULL) {
			(void) fprintf(stderr,
				gettext("id: invalid user name: \"%s\"\n"),
					argv[1]);
			return (1);
		}
		user = argv[1];
		uid = euid = pwp->pw_uid;
		prgid = gid = egid = pwp->pw_gid;
	} else {
		uid = getuid();
		gid = getgid();
		euid = geteuid();
		egid = getegid();
	}

	if (mode != CURR) {
		if (!rflag) {
			uid = euid;
			gid = egid;
		}
		if (mode == USER)
			puid(uid);
		else /* mode == GROUP || mode == ALLGROUPS */ {
			pgid(gid);
			if (user)
				i = getusergroups(NGROUPS_UMAX, groupids, user,
				    prgid);
			else
				i = getgroups(NGROUPS_UMAX, groupids);
			if ((mode == ALLGROUPS) && (i != 0)) {
			if (i == -1)
				perror("getgroups");
			else if (i > 0) {
				for (j = 0; j < i; ++j) {
					if ((gid = groupids[j]) == egid)
						continue;
					putchar(' ');
					pgid(gid);
				}
			}
			}
		}
		(int) putchar('\n');
	} else {
		prid(UID, uid);
		prid(GID, gid);
		if (uid != euid)
			prid(EUID, euid);
		if (gid != egid)
			prid(EGID, egid);
#ifndef XPG4
		if (aflag) {
			if (user)
				i = getusergroups(NGROUPS_UMAX, groupids, user,
				    prgid);
			else
				i = getgroups(NGROUPS_UMAX, groupids);
			if (i == -1)
				perror("getgroups");
			else if (i > 0) {
				(void) printf(" groups=");
				for (idp = groupids; i--; idp++) {
					(void) printf("%u", *idp);
					gr = getgrgid(*idp);
					if (gr)
						(void) printf("(%s)",
							gr->gr_name);
					if (i)
						(int) putchar(',');
				}
			}
		}
#else
		if (user)
			i = getusergroups(NGROUPS_UMAX, groupids, user, prgid);
		else
			i = getgroups(NGROUPS_UMAX, groupids);
		if (i == -1)
			perror("getgroups");
		else if (i > 1) {
			(void) printf(" groups=");
			for (idp = groupids; i--; idp++) {
				if (*idp == egid)
					continue;
				(void) printf("%u", *idp);
				gr = getgrgid(*idp);
				if (gr)
					(void) printf("(%s)", gr->gr_name);
				if (i)
					(int) putchar(',');
			}
		}
#endif
		(int) putchar('\n');
	}
	return (0);
}

static int
usage()
{
#ifdef XPG4
	(void) fprintf(stderr, gettext(
	    "Usage: id [user]\n"
	    "       id -G [-n] [user]\n"
	    "       id -g [-nr] [user]\n"
	    "       id -u [-nr] [user]\n"));
#else
	(void) fprintf(stderr, gettext(
	    "Usage: id [user]\n"
	    "       id -a [user]\n"));
#endif
	return (2);
}

static void
puid(uid)
	uid_t uid;
{
	register struct passwd *pw;

	if (nflag && (pw = getpwuid(uid)) != PWNULL)
		(void) printf("%s", pw->pw_name);
	else
		(void) printf("%u", (int)uid);
}

static void
pgid(gid)
	gid_t gid;
{
	register struct group *gr;

	if (nflag && (gr = getgrgid(gid)) != GRNULL)
		(void) printf("%s", gr->gr_name);
	else
		(void) printf("%u", (int)gid);
}

static void
prid(how, id)
TYPE how;
uid_t id;
{
	register char *s;

	switch ((int)how) {
	case UID:
		s = "uid";
		break;

	case EUID:
		s = " euid";
		break;

	case GID:
		s = " gid";
		break;

	case EGID:
		s = " egid";
		break;

	}
	if (s != NULL)
		(void) printf("%s=", s);
	(void) printf("%u", (int)id);
	switch ((int)how) {
	case UID:
	case EUID: {
		struct passwd *pwp;

		if ((pwp = getpwuid(id)) != PWNULL)
			(void) printf("(%s)", pwp->pw_name);
		break;
	    }

	case GID:
	case EGID: {
		struct group *grp;

		if ((grp = getgrgid(id)) != GRNULL)
			(void) printf("(%s)", grp->gr_name);
		break;
	    }
	}
}

/*
 * Get the supplementary group affiliation for the user
 */
static int getusergroups(gidsetsize, grouplist, user, prgid)
	int	gidsetsize;
	gid_t	*grouplist;
	char	*user;
	gid_t	prgid;
{
	struct group *group;
	char **gr_mem;
	int ngroups = 0;

	setgrent();
	while ((ngroups < gidsetsize) && ((group = getgrent()) != NULL))
		for (gr_mem = group->gr_mem; *gr_mem; gr_mem++)
			if (strcmp(user, *gr_mem) == 0) {
				if (gidsetsize) 
					grouplist[ngroups] = group->gr_gid;
				ngroups++;
			}
	endgrent();
	if (gidsetsize && !ngroups)
		grouplist[ngroups++] = prgid;
	return (ngroups);
}
