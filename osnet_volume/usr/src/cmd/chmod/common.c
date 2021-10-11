/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T			*/
/*	  All Rights Reserved						*/
/*									*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T		*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.		*/

#ident	"@(#)common.c	1.3	94/11/29 SMI"	/* SVr4.0 1.9	*/

/*									*/
/*		PROPRIETARY NOTICE (Combined)				*/
/*									*/
/*	This source code is unpublished proprietary information		*/
/*	constituting, or derived under license from AT&T's UNIX(r)	*/
/*	System V.							*/
/*	In addition, portions of such source code were derived from	*/
/*	Berkeley 4.3 BSD under license from the Regents of the		*/
/*	University of California.					*/
/*									*/
/*			Copyright Notice				*/
/*									*/
/*  Notice of copyright on this source code product does not indicate	*/
/*  publication.							*/
/*									*/
/*	 (c) 1986,1987,1988,1989  Sun Microsystems, Inc			*/
/*	 (c) 1983,1984,1985,1986,1987,1988,1989	 AT&T.			*/
/*		   All rights reserved.					*/

/*
 * Use of this object by a utility (so far chmod, mkdir and mkfifo use
 * it) requires that the utility implement an error-processing routine
 * named errmsg(), with a prototype as specified below.
 *
 * This is necessary because the mode-parsing code here makes use of such
 * a routine, located in chmod.c.  The error-reporting style of the
 * utilities sharing this code differs enough that it is difficult to
 * implement a common version of this routine to be used by all.
 */

/*
 *  Note that many convolutions are necessary
 *  due to the re-use of bits between locking
 *  and setgid
 */

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#include <string.h>	/* strerror() */
#include <stdarg.h>

#define	USER	05700	/* user's bits */
#define	GROUP	02070	/* group's bits */
#define	OTHER	00007	/* other's bits */
#define	ALL	07777	/* all */

#define	READ	00444	/* read permit */
#define	WRITE	00222	/* write permit */
#define	EXEC	00111	/* exec permit */
#define	SETID	06000	/* set[ug]id */
#define	LOCK	02000	/* lock permit */
#define	STICKY	01000	/* sticky bit */

#define	WHO_EMPTY 0

static char *msp;

extern void
errmsg(int severity, int code, char *format, ...);

static int
what(void);

static mode_t
abs(mode_t mode),
who(void);

/*
 *  We are parsing a comma-separated list of mode expressions of the form:
 *
 *			 [<who>] <op> [<perms>]
 */

mode_t
newmode(char *ms, mode_t new_mode, mode_t umsk, char *file)
{
	/*
	 * new_mode  contains the mode value constructed by parsing the
	 *           expression pointed to by ms
	 * old_mode  contains the mode provided by the caller
	 * oper      contains +|-|= information
	 * perms_msk contains rwx(slt) information
	 * umsk      contains the umask value to be assumed.
	 * who_empty is non-zero if the <who> clause did not appear.
	 * who_msk   contains USER|GROUP|OTHER information
	 */

	register int oper;	/* <op> */
	register int lcheck;
	register int scheck;
	register int xcheck;
	register int goon;

	int who_empty;
	int first_eq = 1;	/* Marks the first occurance of oper '=' */

	mode_t who_msk;
	mode_t perms_msk;
	mode_t old_mode = new_mode;	/* save original mode */

	msp = ms;

	if (isdigit(*msp))
		return (abs(old_mode));

	do {
		/*
		 * When <who> is empty, and <oper> == `=`, the umask is
		 * obeyed.  So we need to make note of it here, for use
		 * later.
		 */

		if ((who_msk = who()) == WHO_EMPTY) {
			who_empty = 1;
			who_msk = ALL;
		} else {
			who_empty = 0;
		}

		while (oper = what()) {
			/*
			 *  this section processes permissions
			 */

			perms_msk = 0;
			goon = 0;
			lcheck = scheck = xcheck = 0;

			switch (*msp) {
			case 'u':
				perms_msk = (new_mode & USER) >> 6;
				goto dup;
			case 'g':
				perms_msk = (new_mode & GROUP) >> 3;
				goto dup;
			case 'o':
				perms_msk = (new_mode & OTHER);
			dup:
				perms_msk &= (READ|WRITE|EXEC);
				perms_msk |= (perms_msk << 3) |
				    (perms_msk << 6);
				msp++;
				goon = 1;
			}

			while (goon == 0) {
				switch (*msp++) {
				case 'r':
					perms_msk |= READ;
					continue;
				case 'w':
					perms_msk |= WRITE;
					continue;
				case 'x':
					perms_msk |= EXEC;
					xcheck = 1;
					continue;
				case 'X':
					if (((old_mode & S_IFMT) == S_IFDIR) ||
					    (old_mode & EXEC)) {
						perms_msk |= EXEC;
						xcheck = 1;
					}
					continue;
				case 'l':
					perms_msk |= LOCK;
					who_msk |= LOCK;
					lcheck = 1;
					continue;
				case 's':
					perms_msk |= SETID;
					scheck = 1;
					continue;
				case 't':
					perms_msk |= STICKY;
					continue;
				default:
					msp--;
					goon = 1;
				}
			}

			perms_msk &= who_msk;

			switch (oper) {
			case '+':
				if (who_empty) {
					perms_msk &= ~umsk;
				}


				/* is group execution requested? */
				if (xcheck == 1 &&
				    (perms_msk & GROUP & EXEC) ==
				    (GROUP & EXEC)) {

					/* not locking, too! */
					if (lcheck == 1) {
						errmsg(1, 3,
						    gettext("Group execution "
						    "and locking not "
						    "permitted "
						    "together\n"));
					}

					/*
					 * not if the file is already
					 * lockable.
					 */

					if ((new_mode & GROUP &
					    (LOCK | EXEC)) == LOCK) {
						errmsg(2, 0,
						    gettext("Group execution "
						    "not permitted on "
						    "%s, a lockable file\n"),
						    file);
						return (old_mode);
					}
				}

				/* is setgid on execution requested? */
				if (scheck == 1 && (perms_msk & GROUP & SETID)
				    == (GROUP & SETID)) {
					/* not locking, too! */
					if (lcheck == 1 &&
					    (perms_msk & GROUP & EXEC) ==
					    (GROUP & EXEC)) {
						errmsg(1, 4,
						    gettext("Set-group-ID and "
						    "locking not permitted "
						    "together\n"));
					}

					/*
					 * not if the file is already
					 * lockable
					 */

					if ((new_mode & GROUP &
					    (LOCK | EXEC)) == LOCK) {
						errmsg(2, 0,
						    gettext("Set-group-ID not "
						    "permitted on %s, "
						    "a lockable file\n"),
						    file);
						return (old_mode);
					}
				}

				/* is setid on execution requested? */
				if ((scheck == 1) &&
				    ((new_mode & S_IFMT) != S_IFDIR)) {

					/*
					 * the corresponding execution must
					 * be requested or already set
					 */
					if (((new_mode | perms_msk) &
					    who_msk & EXEC & (USER | GROUP)) !=
					    (who_msk & EXEC & (USER | GROUP))) {
						errmsg(2, 0,
						    gettext("Execute "
						    "permission required "
						    "for set-ID on "
						    "execution for %s\n"),
						    file);
						return (old_mode);
					}
				}

				/* is locking requested? */
				if (lcheck == 1) {
					/*
					 * not if the file has group execution
					 * set.
					 * NOTE: this also covers files with
					 * setgid
					 */
					if ((new_mode & GROUP & EXEC) ==
					    (GROUP & EXEC)) {
						errmsg(2, 0,
						    gettext("Locking not "
						    "permitted on %s, "
						    "a group executable "
						    "file\n"),
						    file);
						return (old_mode);
					}
				}

				/* create new mode */
				new_mode |= perms_msk;
				break;

			case '-':
				if (who_empty) {
					perms_msk &= ~umsk;
				}

				/* don't turn off locking, unless it's on */
				if (lcheck == 1 && scheck == 0 &&
				    (new_mode & GROUP & (LOCK | EXEC)) !=
				    LOCK) {
					perms_msk &= ~LOCK;
				}

				/* don't turn off setgid, unless it's on */
				if (scheck == 1 &&
				    ((new_mode & S_IFMT) != S_IFDIR) &&
				    lcheck == 0 &&
				    (new_mode & GROUP & (LOCK | EXEC)) ==
				    LOCK) {
					perms_msk &= ~(GROUP & SETID);
				}

				/*
				 * if execution is being turned off and the
				 * corresponding setid is not, turn setid off,
				 * too & warn the user
				 */
				if (xcheck == 1 && scheck == 0 &&
				    ((who_msk & GROUP) == GROUP ||
				    (who_msk & USER) == USER) &&
				    (new_mode & who_msk & (SETID | EXEC)) ==
				    (who_msk & (SETID | EXEC))) {
					errmsg(2, 0,
					    gettext("Corresponding set-ID "
					    "also disabled on %s since "
					    "set-ID requires execute "
					    "permission\n"),
					    file);

					if ((perms_msk & USER & SETID) !=
					    (USER & SETID) && (new_mode &
					    USER & (SETID | EXEC)) ==
					    (who_msk & USER &
					    (SETID | EXEC))) {
						perms_msk |= USER & SETID;
					}
					if ((perms_msk & GROUP & SETID) !=
					    (GROUP & SETID) &&
					    (new_mode & GROUP &
					    (SETID | EXEC)) ==
					    (who_msk & GROUP &
					    (SETID | EXEC))) {
						perms_msk |= GROUP & SETID;
					}
				}

				/* create new mode */
				new_mode &= ~perms_msk;
				break;

			case '=':
				if (who_empty) {
					perms_msk &= ~umsk;
				}

				/* is locking requested? */
				if (lcheck == 1) {

					/* not group execution, too! */
					if ((perms_msk & GROUP & EXEC) ==
					    (GROUP & EXEC)) {
						errmsg(1, 3,
						    gettext("Group execution "
						    "and locking not "
						    "permitted together\n"));
					}

					/*
					 * if the file has group execution set,
					 * turn it off!
					 */

					if ((who_msk & GROUP) != GROUP) {
						new_mode &= ~(GROUP & EXEC);
					}
				}

				/*
				 * is setid on execution requested? the
				 * corresponding execution must be requested,
				 * too!
				 */

				if (scheck == 1 &&
				    (perms_msk & EXEC & (USER | GROUP)) !=
				    (who_msk & EXEC & (USER | GROUP))) {
					errmsg(1, 2,
					    gettext("Execute permission "
					    "required for set-ID on "
					    "execution\n"));
				}

				/*
				 * The ISGID bit on directories will not be
				 * changed when the mode argument is a string
				 * with "=".
				 */

				if ((old_mode & S_IFMT) == S_IFDIR)
					perms_msk = (perms_msk &
					    ~S_ISGID) | (old_mode & S_ISGID);

				/*
				 * create new mode:
				 *   clear the who_msk bits
				 *   set the perms_mks bits (which have
				 *   been trimmed to fit the who_msk.
				 */

				new_mode &= ~who_msk;
				new_mode |= perms_msk;
				break;
			}
		}
	} while (*msp++ == ',');

	if (*--msp) {
		errmsg(1, 5, gettext("invalid mode\n"));
	}

	return (new_mode);
}

mode_t
abs(mode_t mode)
{
	register c;
	mode_t i;

	for (i = 0; (c = *msp) >= '0' && c <= '7'; msp++)
		i = (mode_t)((i << 3) + (c - '0'));
	if (*msp)
		errmsg(1, 6, gettext("invalid mode\n"));

/*
 * The ISGID bit on directories will not be changed when the mode argument is
 * octal numeric. Only "g+s" and "g-s" arguments can change ISGID bit when
 * applied to directories.
 */
	if ((mode & S_IFMT) == S_IFDIR)
		return ((i & ~S_ISGID) | (mode & S_ISGID));
	return (i);
}

static mode_t
who(void)
{
	register mode_t m;

	m = WHO_EMPTY;

	for (; ; msp++) {
		switch (*msp) {
		case 'u':
			m |= USER;
			continue;
		case 'g':
			m |= GROUP;
			continue;
		case 'o':
			m |= OTHER;
			continue;
		case 'a':
			m |= ALL;
			continue;
		default:
			return (m);
		}
	}
}

static int
what(void)
{
	switch (*msp) {
	case '+':
	case '-':
	case '=':
		return (*msp++);
	}
	return (0);
}
