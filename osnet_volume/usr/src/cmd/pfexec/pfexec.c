/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident	"@(#)pfexec.c	1.1	99/05/26 SMI"

#include <errno.h>
#include <deflt.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <exec_attr.h>


#ifndef	TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

#define	EXIT_OK		0
#define	EXIT_FATAL	1


static int getrealpath(const char *, char *);
static int checkattrs(char *);
static void sanitize_environ();
static uid_t get_uid(char *);
static gid_t get_gid(char *);
static int _isnumber(char *);
static void usage();


extern char **environ;


main(int argc, char *argv[])
{
	char		*cmd;
	char		**cmdargs;
	char		cmd_realpath[MAXPATHLEN];

	setlocale(LC_ALL, "");
	textdomain(TEXT_DOMAIN);

	if (argc < 2) {
		usage();
		exit(EXIT_FATAL);
	}
	cmd = argv[1];
	cmdargs = &argv[1];

	if (getrealpath(cmd, cmd_realpath) == NOT_OK) {
		exit(EXIT_FATAL);
	}
	if (checkattrs(cmd_realpath) == NOT_OK) {
		exit(EXIT_FATAL);
	}
	execv((const char *)cmd, (char *const *)cmdargs);
	/*
	 * We'd be here only if execv fails.
	 */
	perror("pfexec");
	exit(EXIT_FATAL);
}


/*
 * gets realpath for cmd.
 * return OK(TRUE) on success, NOT_OK(FALSE) on failure.
 */
static int
getrealpath(const char *cmd, char *cmd_realpath)
{
	if (realpath(cmd, cmd_realpath) == NULL) {
		fprintf(stderr, "%s: ", cmd);
		fprintf(stderr, gettext("can't get real path\n"));
		return (NOT_OK);
	}
	return (OK);
}


/*
 * gets execution attributed for cmd, sets uids/gids, checks environ.
 * returns OK(TRUE) on success, NOT_OK(FALSE) on failure.
 */
static int
checkattrs(char *cmd_realpath)
{
	register char		*value;
	register uid_t		uid, euid;
	register gid_t		gid = -1;
	register gid_t		egid = -1;
	register struct passwd	*pwent;
	register execattr_t	*exec;

	uid = euid = getuid();
	if ((pwent = getpwuid(uid)) == NULL) {
		fprintf(stderr, "%d: ", uid);
		fprintf(stderr, gettext("can't get passwd entry\n"));
		return (NOT_OK);
	}
	/*
	 * Get the exec attrs: uid, gid, euid and egid
	 */
	if ((exec = getexecuser(pwent->pw_name,
	    KV_COMMAND, (char *)cmd_realpath, GET_ONE)) == NULL) {
		fprintf(stderr, "%s: ", cmd_realpath);
		fprintf(stderr, gettext("can't get execution attributes\n"));
		return (NOT_OK);
	}
	if ((value = kva_match(exec->attr, EXECATTR_UID_KW)) != NULL) {
		euid = uid = get_uid(value);
	}
	if ((value = kva_match(exec->attr, EXECATTR_GID_KW)) != NULL) {
		egid = gid = get_gid(value);
	}
	if ((value = kva_match(exec->attr, EXECATTR_EUID_KW)) != NULL) {
		euid = get_uid(value);
	}
	if ((value = kva_match(exec->attr, EXECATTR_EGID_KW)) != NULL) {
		egid = get_gid(value);
	}
	/*
	 * Set gids/uids.
	 *
	 */
	if ((gid != -1) || (egid != -1)) {
		if ((setregid(gid, egid) == -1)) {
			fprintf(stderr, "%s: ", cmd_realpath);
			fprintf(stderr, gettext("can't set gid\n"));
			return (NOT_OK);
		}
	}
	if (setreuid(uid, euid) == -1) {
		fprintf(stderr, "%s: ", cmd_realpath);
		fprintf(stderr, gettext("can't set uid\n"));
		return (NOT_OK);
	}
	if (euid == uid) {
		sanitize_environ();
	}

	free_execattr(exec);

	return (OK);
}


/*
 * cleans up environ. code from su.c
 */
static void
sanitize_environ()
{
	register char	**pp = environ;
	register char	**qq, *p;

	while ((p = *pp) != NULL) {
		if (*p == 'L' && p[1] == 'D' && p[2] == '_') {
			for (qq = pp; (*qq = qq[1]) != NULL; qq++) {
				;
			}
		} else {
			pp++;
		}
	}
}


static uid_t
get_uid(char *value)
{
	struct passwd *passwd_ent;

	if (_isnumber(value)) {
		return (atoi(value));
	}
	if ((passwd_ent = getpwnam(value)) == NULL) {
		fprintf(stderr, "%s :", value);
		fprintf(stderr, gettext("can't get user entry\n"));
		return (NOT_OK);
	}

	return (passwd_ent->pw_uid);
}


static uid_t
get_gid(char *value)
{
	struct group *group_ent;

	if (_isnumber(value)) {
		return (atoi(value));
	}
	if ((group_ent = getgrnam(value)) == NULL) {
		fprintf(stderr, "%s :", value);
		fprintf(stderr, gettext("can't get group entry\n"));
		return (NOT_OK);
	}

	return (group_ent->gr_gid);
}


static int
_isnumber(char *s)
{
	int c;

	while ((c = *s++) != '\0') {
		if (!isdigit(c)) {
			return (0);
		}
	}

	return (1);
}


static void
usage()
{
	fprintf(stderr, gettext("pfexec [command]\n"));
}
