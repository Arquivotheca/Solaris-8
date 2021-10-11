/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lpfsck.c	1.8	96/04/10 SMI"	/* SVr4.0 1.3.1.6	*/

#include "stdarg.h"
#include "stdlib.h"
#include "fcntl.h"
#include <sys/param.h>
#include "lpsched.h"


static void check_link();

/**
 ** lpfsck()
 **/

#define	F	0
#define D	1
#define P	2
#define S	3

static void		proto (int, int, ...);
static char *		va_makepath(va_list *);
static void		_rename (char *, char *, ...);

void
lpfsck(void)
{
	char *			cmd;
	struct stat		stbuf;
	int			real_am_in_background = am_in_background;


	/*
	 * Force log messages to go into the log file instead of stdout.
	 */
	am_in_background = 1;

	/*
	 * These lines should match what is in the prototype file
	 * for the packaging! (In fact, it probably ought to be
	 * generated from that file, but that work is for a rainy day...)
	 */

	/*
	 * DIRECTORIES:
	 */
proto (D, 0,  Lp_A, NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_A_Classes, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_A_Forms, NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_A_Interfaces, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_A_Printers, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_A_PrintWheels, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 0,  "/var/lp", NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Logs, NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Spooldir, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Admins, NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 0,  Lp_Spooldir, FIFOSDIR, NULL,	    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Private_FIFOs, NULL,		    0771, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Public_FIFOs, NULL,		    0773, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Requests, NULL,		    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Requests, Local_System, NULL,	    0770, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_System, NULL,			    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Tmp, NULL,			    0771, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_Tmp, Local_System, NULL,	    0775, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_NetTmp, NULL,			    0770, Lp_Uid, Lp_Gid);

	/*
	 * The lpNet <-> lpsched job transfer directories.
	 * Strictly used for temporary file transfer, on start-up
	 * we can safely clean them out. Indeed, we should clean
	 * them out in case we had died suddenly and are now
	 * restarting. The directories should never be very big,
	 * so we are not in danger of getting ``arglist too big''.
	 */
proto (D, 1,  Lp_NetTmp, "tmp", NULL,		    0770, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_NetTmp, "tmp", Local_System, NULL, 0770, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_NetTmp, "requests", NULL,	    0770, Lp_Uid, Lp_Gid);
proto (D, 1,  Lp_NetTmp, "requests", Local_System, NULL, 0770, Lp_Uid, Lp_Gid);
	cmd = makestr(RMCMD, " ", Lp_NetTmp, "/tmp/*/*", (char *)0);
	system (cmd);
	Free (cmd);
	cmd = makestr(RMCMD, " ", Lp_NetTmp, "/requests/*/*", (char *)0);
	system (cmd);
	Free (cmd);

	/*
	 * THE MAIN FIFO:
	 */
proto (P, 1,  Lp_FIFO, NULL,			    0666, Lp_Uid, Lp_Gid);

	/*
	 * SYMBOLIC LINKS:
	 * Watch out! These names are given in the reverse
	 * order found in the prototype file (sorry!)
	 */
proto (S, 1,  Lp_Model, NULL,			"/etc/lp/model", NULL);
proto (S, 1,  Lp_Logs, NULL,			"/etc/lp/logs", NULL);
/*     S, 1,  Lp_Tmp, Local_System, ...    DONE BELOW */
proto (S, 1,  Lp_Bin, NULL,			Lp_Spooldir, "bin", NULL);
proto (S, 1,  Lp_A, NULL,			Lp_Admins, "lp", NULL);

	/*
	 * OTHER FILES:
	 */
proto (F, 1,  Lp_NetData, NULL,			    0664, Lp_Uid, Lp_Gid);

	/*
	 * SPECIAL CASE:
	 * If the "temp" symbolic link already exists,
	 * but is not correct, assume the machine's nodename changed.
	 * Rename directories that include the nodename, if possible,
	 * so that unprinted requests are saved. Then change the
	 * symbolic link.
	 * Watch out for a ``symbolic link'' that isn't!
	 */
	if (Lstat(Lp_Temp, &stbuf) == 0)
	    switch (stbuf.st_mode & S_IFMT) {

	    default:
		Unlink (Lp_Temp);
		break;

	    case S_IFDIR:
		Rmdir (Lp_Temp);
		break;

	    case S_IFLNK:
		check_link();
		break;
	    }

	proto(S, 1, Lp_Tmp, Local_System, NULL,	Lp_Temp, NULL);

	am_in_background = real_am_in_background;
	return;
}

static void
check_link()
{
	int len;
	char symbolic[MAXPATHLEN + 1];
	char *real_dir;
	char *old_system;

	if ((len = Readlink(Lp_Temp, symbolic, MAXPATHLEN)) <= 0) {
		Unlink(Lp_Temp);
		return;
	}
	symbolic[len] = 0;

	/* check that symlink points into /var/spool/lp/tmp */
	if (strncmp(Lp_Tmp, symbolic, strlen(Lp_Tmp)) != 0) {
		Unlink(Lp_Temp);
		return;
	}

	/* check that symlink points to something */
	if (symbolic[len - 1] == '/') {
		Unlink(Lp_Temp);
		return;
	}

	real_dir = makepath(Lp_Tmp, Local_System, NULL);
	if (!STREQU(real_dir, symbolic)) {
		if (!(old_system = strrchr(symbolic, '/')))
			old_system = symbolic;
		else
			old_system++;

		/*
		 * The "rename()" system call (buried
		 * inside the "_rename()" routine) should
		 * succeed, even though we blindly created
		 * the new directory earlier, as the only
		 * directory entries should be . and ..
		 * (although if someone already created
		 * them, we'll note the fact).
		 */
		_rename(old_system, Local_System, Lp_Tmp, NULL);
		_rename(old_system, Local_System, Lp_Requests, NULL);
		_rename(old_system, Local_System, Lp_NetTmp, "tmp", NULL);
		_rename(old_system, Local_System, Lp_NetTmp, "requests", NULL);

		Unlink(Lp_Temp);
	}
	Free(real_dir);
}


/**
 ** proto()
 **/

static void
proto(int type, int rm_ok, ...)
{
	va_list			ap;

	char			*path,
				*symbolic;

	int			exist;

	mode_t			mode;

	uid_t			uid;

	gid_t			gid;

	struct stat		stbuf;


	va_start (ap, type);

	path = va_makepath(&ap);

	exist = (stat(path, &stbuf) == 0);

	switch (type) {

	case S:
		if (!exist)
			fail ("%s is missing!\n", path);
		symbolic = va_makepath(&ap);
		Symlink (path, symbolic);
		Free (symbolic);
		Free (path);
		return;

	case D:
		if (exist && (stbuf.st_mode & S_IFDIR) == 0)
			if (!rm_ok)
				fail ("%s is not a directory!\n", path);
			else {
				Unlink (path);
				exist = 0;
			}
		if (!exist)
			Mkdir (path, 0);
		break;

	case F:
		if (exist && (stbuf.st_mode & S_IFREG) == 0)
			if (!rm_ok)
				fail ("%s is not a file!\n", path);
			else {
				Unlink (path);
				exist = 0;
			}
		if (!exist)
			Close(Creat(path, 0));
		break;

	case P:
		/*
		 * Either a pipe or a file.
		 */
		if (exist && (stbuf.st_mode & (S_IFREG|S_IFIFO)) == 0)
			if (!rm_ok)
				fail ("%s is not a file or pipe!\n", path);
			else {
				Unlink (path);
				exist = 0;
			}
		if (!exist)
			Close(Creat(path, 0));
		break;

	}

	mode = va_arg(ap, mode_t);
	uid = va_arg(ap, uid_t);
	gid = va_arg(ap, gid_t);
	Chmod (path, mode);
	Chown (path, uid, gid);

	Free (path);
	return;
}

static char *
va_makepath (va_list *pap)
{
	char			*component;
	char 			buf[MAXPATHLEN];

	memset(buf, NULL, sizeof (buf));
	while ((component = va_arg((*pap), char *)) != NULL) {
		strcat(buf, component);
		strcat(buf, "/");
	}

	return (strdup(buf));
}

/**
 ** _rename()
 **/

static void
_rename(char *old_system, char *new_system, ...)
{
	va_list			ap;

	char *			prefix;
	char *			old;
	char *			new;


	va_start (ap, new_system);
	prefix = va_makepath(&ap);
	va_end (ap);

	old = makepath(prefix, old_system, (char *)0);
	new = makepath(prefix, new_system, (char *)0);

	if (Rename(old, new) == 0)
		note ("Renamed %s to %s.\n", old, new);
	else if (errno == EEXIST)
		note (
			"Rename of %s to %s failed because %s exists.\n",
			old,
			new,
			new
		);
	else
		fail (
			"Rename of %s to %s failed (%s).\n",
			old,
			new,
			PERROR
		);

	Free (new);
	Free (old);
	Free (prefix);

	return;
}
