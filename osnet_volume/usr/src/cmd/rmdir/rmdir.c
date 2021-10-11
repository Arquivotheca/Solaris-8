/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rmdir.c	1.12	95/10/24 SMI"	/* SVr4.0 1.12	*/
/*
** Rmdir(1) removes directory.
** If -p option is used, rmdir(1) tries to remove the directory
** and it's parent directories.  It exits with code 0 if the WHOLE
** given path is removed and 2 if part of path remains.
** Results are printed except when -s is used.
*/

#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>


void
main(int argc, char **argv)
{

	char	*prog;
	int c, pflag, sflag, errflg, rc;
	char *ptr, *remain, *msg, *path;
	unsigned int pathlen;

	prog = argv[0];
	pflag = sflag = 0;
	errflg = 0;
	/* set effective uid, euid, to be same as real	*/
	/* uid, ruid.  Rmdir(2) checks search & write	*/
	/* permissions for euid, but for compatibility	*/
	/* the check must be done using ruid.		*/

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (setuid(getuid()) == -1) {
		char	buf[80];

		(void) sprintf(buf, gettext("%s: setuid(2) failed"), prog);
		perror(buf);
		exit(1);
	}

	while ((c = getopt(argc, argv, "ps")) != EOF)
		switch (c) {
			case 'p':
				pflag++;
				break;
			case 's':
				sflag++;
				break;
			case '?':
				errflg++;
				break;
		}
	if (argc < 2 || errflg) {
		(void) fprintf(stderr, gettext("Usage: %s [-ps] dirname ...\n"),
		    prog);
		exit(2);
	}
	errno = 0;
	argc -= optind;
	argv = &argv[optind];
	while (argc--) {
		ptr = *argv++;
		/*
		 * -p option. Remove directory and parents.
		 * Prints results of removing
		 */
		if (pflag) {
			pathlen = (unsigned)strlen(ptr);
			if ((path = (char *)malloc(pathlen + 4)) == NULL ||
			    (remain = (char *)malloc(pathlen + 4)) == NULL) {
				perror(prog);
				exit(2);
			}
			(void) strcpy(path, ptr);

			/*
			 * rmdirp removes directory and parents
			 * rc != 0 implies only part of path removed
			 */

			if (((rc = rmdirp(path, remain)) != 0) && !sflag) {
				switch (rc) {
				case -1:
					if (errno == EEXIST)
						msg = gettext(
						    "Directory not empty");
					else
						msg = strerror(errno);
					break;
				case -2:
					errno = EINVAL;
					msg = gettext("Can not remove . or ..");
					break;
				case -3:
					errno = EINVAL;
					msg = gettext(
					    "Can not remove current directory");
					break;
				}
				(void) fprintf(stderr, gettext("%s: directory"
				    " \"%s\": %s not removed; %s\n"),
				    prog, ptr, remain, msg);
			}
			free(path);
			free(remain);
			continue;
		}

		/* No -p option. Remove only one directory */

		if (rmdir(ptr) == -1) {
			switch (errno) {
			case EEXIST:
				msg = gettext("Directory not empty");
				break;
			case ENOTDIR:
				msg = gettext("Path component not a directory");
				break;
			case ENOENT:
				msg = gettext("Directory does not exist");
				break;
			case EACCES:
				msg = gettext(
				    "Search or write permission needed");
				break;
			case EBUSY:
				msg = gettext(
				    "Directory is a mount point or in use");
				break;
			case EROFS:
				msg = gettext("Read-only file system");
				break;
			case EIO:
				msg = gettext(
				    "I/O error accessing file system");
				break;
			case EINVAL:
				msg = gettext(
				    "Can't remove current directory or ..");
				break;
			case EFAULT:
			default:
				msg = strerror(errno);
				break;
			}
			(void) fprintf(stderr,
			    gettext("%s: directory \"%s\": %s\n"),
			    prog, ptr, msg);
			continue;
		}
	}
	exit(errno ? 2 : 0);
}
