/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fuser.c	1.9	98/04/23 SMI"	/* SVr4.0 1.27.1.24	*/

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <kstat.h>
#include <locale.h>
#include <sys/mnttab.h>
#include <sys/errno.h>
#include <sys/var.h>
#include <sys/utssys.h>

void exit(), perror();
extern char *malloc();
extern int errno;

/*
 * Return a pointer to the mount point matching the given special name, if
 * possible, otherwise, exit with 1 if mnttab corruption is detected, else
 * return NULL.
 *
 * NOTE:  the underlying storage for mget and mref is defined static by
 * libos.  Repeated calls to getmntany() overwrite it; to save mnttab
 * structures would require copying the member strings elsewhere.
 */
char *
spec_to_mount(specname)
	char	*specname;
{
	FILE	*frp;
	int 	ret;
	struct mnttab mref, mget;

	/* get mount-point */
	if ((frp = fopen(MNTTAB, "r")) == NULL) {
		return (NULL);
	}
	mntnull(&mref);
	mref.mnt_special = specname;
	ret = getmntany(frp, &mget, &mref);
	fclose(frp);
	if (ret == 0) {
		return (mget.mnt_mountp);
	} else if (ret > 0) {
		fprintf(stderr, gettext("mnttab is corrupted\n"));
		exit(1);
	} else {
		return (NULL);
	}
}

/*
 * The main objective of this routine is to allocate an array of f_user_t's.
 * In order for it to know how large an array to allocate, it must know
 * the value of v.v_proc in the kernel.  To get this, we do a kstat
 * lookup to get the var structure from the kernel.
 */
f_user_t *
get_f_user_buf()
{
	kstat_ctl_t *kc;
	kstat_t *ksp;
	struct var v;

	if ((kc = kstat_open()) == NULL ||
	    (ksp = kstat_lookup(kc, "unix", 0, "var")) == NULL ||
	    kstat_read(kc, ksp, &v) == -1) {
		perror(gettext("kstat_read() of struct var failed"));
		exit(1);
	}
	(void) kstat_close(kc);
	return ((f_user_t *)malloc(v.v_proc * sizeof (f_user_t)));
}

/*
 * display the fuser usage message and exit
 */
void
usage()
{
	fprintf(stderr,
	    gettext("Usage:  fuser [-ku[c|f]] files [-[ku[c|f]] files]\n"));
	exit(1);
}

struct co_tab {
		int	c_flag;
		char	c_char;
};

static struct co_tab code_tab[] = {
	{F_CDIR, 'c'},		/* current directory */
	{F_RDIR, 'r'},		/* root directory (via chroot) */
	{F_TEXT, 't'},		/* textfile */
	{F_OPEN, 'o'},		/* open (creat, etc.) file */
	{F_MAP, 'm'},		/* mapped file */
	{F_TTY, 'y'},		/* controlling tty */
	{F_TRACE, 'a'}		/* trace file */
};

/*
 * Show pids and usage indicators for the nusers processes in the users list.
 * When usrid is non-zero, give associated login names.  When gun is non-zero,
 * issue kill -9's to those processes.
 */
void
report(users, nusers, usrid, gun)
	f_user_t *users;
	int nusers;
	int usrid;
	int gun;
{
	int cind;

	for (; nusers; nusers--, users++) {
		fprintf(stdout, " %7d", users->fu_pid);
		fflush(stdout);
		for (cind = 0;
			cind < sizeof (code_tab) / sizeof (struct co_tab);
				cind++) {
			if (users->fu_flags & code_tab[cind].c_flag) {
				fprintf(stderr, "%c", code_tab[cind].c_char);
			}
		}
		if (usrid) {
			/*
			 * print the login name for the process
			 */
			struct passwd *getpwuid(), *pwdp;

			if ((pwdp = getpwuid(users->fu_uid)) != NULL) {
				fprintf(stderr, "(%s)", pwdp->pw_name);
			}
		}
		if (gun) {
			(void) kill(users->fu_pid, 9);
		}
	}
}

/*
 * Determine which processes are using a named file or file system.
 * On stdout, show the pid of each process using each command line file
 * with indication(s) of its use(s).  Optionally display the login
 * name with each process.  Also optionally, issue a kill to each process.
 *
 * X/Open Commands and Utilites, Issue 5 requires fuser to process
 * the complete list of names it is given, so if an error is encountered
 * it will continue through the list, and then exit with a non-zero
 * value. This is a change from earlier behavior where the command
 * would exit immediately upon an error.
 *
 * The preferred use of the command is with a single file or file system.
 */

main(argc, argv)
	int argc;
	char **argv;
{
	int gun = 0, usrid = 0, contained = 0, file_only = 0;
	int newfile = 0, errors = 0;
	register i, j, k;
	char *mntname;
	char *p = "'c' and 'f' can't both be used for a file\n";
	int nusers;
	f_user_t *users;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if (argc < 2) {
		usage();
	}
	if ((users = get_f_user_buf()) == NULL) {
		fprintf(stderr, gettext("fuser: could not allocate buffer\n"));
		exit(1);
	}
	for (i = 1; i < argc; i++) {
		int okay = 0;

		if (argv[i][0] == '-') {
			/* options processing */
			if (newfile) {
				gun = usrid = contained = file_only =
				    newfile = 0;
			}
			for (j = 1; argv[i][j] != '\0'; j++) {
				switch (argv[i][j]) {
				case 'k':
					if (gun) {
						usage();
					}
					gun = 1;
					break;
				case 'u':
					if (usrid) {
						usage();
					}
					usrid = 1;
					break;
				case 'c':
					if (contained) {
						usage();
					}
					if (file_only) {
						fprintf(stderr, gettext(p));
						usage();
					}
					contained = 1;
					break;
				case 'f':
					if (file_only) {
						usage();
					}
					if (contained) {
						fprintf(stderr, gettext(p));
						usage();
					}
					file_only = 1;
					break;
				default:
					fprintf(stderr,
					    gettext("Illegal option %c.\n"),
					    argv[i][j]);
					usage();
				}
			}
			continue;
		} else {
			newfile = 1;
		}

/*
* if not file_only, attempt to translate special name to mount point, via
* /etc/mnttab.  issue: utssys -c mount point if found, and utssys special, too?
* take union of results for report?????
*/
		fflush(stdout);

		/*
		 *First print file name on stderr (so stdout (pids) can
		 * be piped to kill)
		 */
		fprintf(stderr, "%s: ", argv[i]);

		if (!(file_only || contained) && (mntname =
		    spec_to_mount(argv[i])) != NULL) {
			if ((nusers = utssys(mntname, F_CONTAINED, UTS_FUSERS,
			    users)) != -1) {
				report(users, nusers, usrid, gun);
				okay = 1;
			}
		}
		if ((nusers = utssys(argv[i], contained ? F_CONTAINED : 0,
		    UTS_FUSERS, users)) == -1) {
			if (!okay) {
				perror("fuser");
				errors = 1;
				continue;
			}
		} else {
			report(users, nusers, usrid, gun);
		}
		fprintf(stderr, "\n");
	}

	/*
	 * newfile is set when a file is found.  if it isn't set here,
	 * then the user did not use correct syntax
	 */
	if (!newfile) {
		fprintf(stderr, gettext("fuser: missing file name\n"));
		usage();
	}
	exit(errors);
}
