/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)reqexec.c	1.15	96/03/06 SMI"	/* SVr4.0 1.4.1.1 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>	/* creat() declaration */
#include <pwd.h>
#include <grp.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libadm.h"
#include "libinst.h"
#include "pkginstall.h"

extern char	tmpdir[], instdir[];
extern int	nointeract, pkgverbose;

static int	do_exec(char *script, char *output, char *inport, char
		    *alt_user);
static char	path[PATH_MAX], *resppath = NULL;
static int	fd;
static gid_t	instgid;
static uid_t	instuid;
static int	respfile_defined = 0;
static int	respfile_ro = 0;	/* read only resp file */

/* from main.c */
extern int	non_abi_scripts;

#define	ERR_TMPRESP	"unable to create temporary response file"
#define	ERR_RMRESP	"unable to remove response file <%s>"
#define	ERR_ACCRESP	"unable to access response file <%s>"
#define	ERR_CRERESP	"unable to create response file <%s>"
#define	ERR_INTR	"Interactive request script supplied by package"
#define	ERR_BADUSER	"unable to find user %s or %s."
#define	ERR_NOCOPY	"unable to copy <%s>\n\tto <%s>"

/*
 * This informs the calling routine if a read-only response file has been
 * provided on the command line.
 */
int
rdonly_respfile(void)
{
	return (respfile_ro);
}

int
is_a_respfile(void)
{
	return (respfile_defined);
}

/*
 * This function creates a temporary working copy of a read-only response
 * file. It changes the resppath pointer to point to the working copy.
 */
static int
dup_respfile(void)
{
	char tpath[PATH_MAX];
	char cmd[sizeof ("/usr/bin/cp  ") + PATH_MAX*2 + 1];
	int status;

	strcpy(tpath, path);

	(void) sprintf(path, "%s/respXXXXXX", tmpdir);
	resppath = mktemp(path);
	if (resppath == NULL) {
		progerr(gettext(ERR_TMPRESP));
		return (99);
	}

	/* Copy the contents of the user's response file to the working copy. */
	sprintf(cmd, "/usr/bin/cp %s %s", tpath, resppath);
	status = system(cmd);
	if (status == -1 || WEXITSTATUS(status)) {
		progerr(gettext(ERR_NOCOPY), tpath, resppath);
		return (99);
	}

	respfile_ro = 0;

	return (0);
}

/*
 * This function establishes the response file passed on the command line if
 * it's called with a valid string. If called with NULL, it checks to see if
 * there's a response file already. If there isn't, it creates a temporary.
 */
int
set_respfile(char *respfile, char *pkginst, int resp_stat)
{
	if (respfile == NULL && !respfile_defined) {
		/* A temporary response file needs to be constructed. */
		(void) sprintf(path, "%s/respXXXXXX", tmpdir);
		resppath = mktemp(path);
		if (resppath == NULL) {
			progerr(gettext(ERR_TMPRESP));
			return (99);
		}
	} else {
		/* OK, we're being passed a response file or directory. */
		if (isdir(respfile) == 0)
			(void) sprintf(path, "%s/%s", respfile, pkginst);
		else
			(void) strcpy(path, respfile);

		resppath = path;
		respfile_ro = resp_stat;
	}

	respfile_defined++;

	return (0);
}

/* This exposes the working response file. */
char *
get_respfile(void)
{
	return (resppath);
}

/*
 * Execute the request script if present assuming the response file
 * isn't read only.
 */
int
reqexec(char *script)
{
	/*
	 * If we can't get to the the script or the response file, skip this.
	 */
	if (access(script, 0) != 0 || respfile_ro)
		return (0);

	/* No interact means no interact. */
	if (nointeract) {
		ptext(stderr, gettext(ERR_INTR));
		return (5);
	}

	/* If there's no response file, create one. */
	if (!respfile_defined)
		if (set_respfile(NULL, NULL, 0))
			return (99);

	/* Clear out the old response file (if there is one). */
	if ((access(resppath, 0) == 0) && unlink(resppath)) {
		progerr(gettext(ERR_RMRESP), resppath);
		return (99);
	}

	/*
	 * Create a zero length response file which is only writable
	 * by the non-privileged installation user-id, but is readable
	 * by the world
	 */
	if ((fd = creat(resppath, 0644)) < 0) {
		progerr(gettext(ERR_CRERESP), resppath);
		return (99);
	}
	(void) close(fd);

	/*
	 * NOTE : For 2.7 uncomment the non_abi_scripts line and delete
	 * the one below it.
	 */
	return (do_exec(script, resppath, REQ_STDIN,
	/*    non_abi_scripts ? CHK_USER_NON : CHK_USER_ALT)); */
	    CHK_USER_NON));
}

int
chkexec(char *script)
{
	/*
	 * If we're up against a read-only response file from the command
	 * line. Create a working copy.
	 */
	if (respfile_ro) {
		if (dup_respfile())
			return (99);

		/* Make sure we can get to it. */
		if ((access(resppath, 0) != 0)) {
			progerr(gettext(ERR_ACCRESP), resppath);
			return (7);
		}
	}

	/* If there's no response file, create a fresh one. */
	else if (!respfile_defined) {
		if (set_respfile(NULL, NULL, 0))
			return (99);

		/*
		 * create a zero length response file which is only writable
		 * by the non-priveledged installation user-id, but is readable
		 * by the world
		 */
		if ((fd = creat(resppath, 0644)) < 0) {
			progerr(gettext(ERR_CRERESP), resppath);
			return (99);
		}
		(void) close(fd);
	}

	return (do_exec(script, resppath, CHK_STDIN, CHK_USER_ALT));
}

static int
do_exec(char *script, char *output, char *inport, char *alt_user)
{
	char		*uname;
	struct passwd	*pwp;
	struct group	*grp;

	gid_t instgid = (gid_t) 1; /* other */
	uid_t instuid;

	int	retcode = 0;

	if ((pwp = getpwnam(CHK_USER)) != (struct passwd *) NULL) {
		instuid = pwp->pw_uid;
		uname = CHK_USER;
	} else if ((pwp = getpwnam(alt_user)) != (struct passwd *) NULL) {
		instuid = pwp->pw_uid;
		uname = alt_user;
	} else {
		ptext(stderr, gettext(ERR_BADUSER), CHK_USER, CHK_USER_ALT);
		return (1);
	}

	if ((grp = getgrnam(CHK_GRP)) != (struct group *) NULL)
		instgid = grp->gr_gid;

	(void) chown(output, instuid, instgid);

	if (pkgverbose)
		retcode = pkgexecl(inport, CHK_STDOUT, uname, CHK_GRP, SHELL,
		    "-x", script, output, NULL);
	else
		retcode = pkgexecl(inport, CHK_STDOUT, uname, CHK_GRP, SHELL,
		    script, output, NULL);

	return (retcode);
}
