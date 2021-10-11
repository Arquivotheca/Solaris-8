/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)putform.c	1.4	97/05/14 SMI"	/* SVr4.0 1.10	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"
#include "sys/stat.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "stdlib.h"

#include "lp.h"
#include "form.h"

/**
 ** putform() - WRITE FORM STRUCTURE TO DISK FILES
 **/

int
putform(char *name, FORM *formp, FALERT *alertp, FILE **p_align_fp)
{
	register char *		path;

	int fd;

	struct stat		statbuf;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Create the parent directory for this form
	 * if it doesn't yet exist.
	 */
	if (!(path = getformfile(name, (char *)0)))
		return (-1);
	if (Stat(path, &statbuf) == 0) {
		if (!(statbuf.st_mode & S_IFDIR)) {
			Free (path);
			errno = ENOTDIR;
			return (-1);
		}
	} else if (errno != ENOENT || mkdir_lpdir(path, MODE_DIR) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);

	/*
	 * Open the configuration file and write out the form
	 * configuration (?)
	 */
	if (formp) {
		if (!(path = getformfile(name, DESCRIBE)))
			return (-1);
		if ((fd = open_locked(path, "w", MODE_READ)) < 0) {
			Free (path);
			return (-1);
		}
		Free (path);

		if (wrform(name, formp, fd, 0, (int *)0) == -1) {
			close(fd);
			return (-1);
		}
		close(fd);
	}

	/*
	 * Write out the alert condition (?)
	 */
	if (alertp) {
		if (
			alertp->shcmd
		     && putalert(Lp_A_Forms, name, alertp) == -1
		)
			return (-1);
	}

	/*
	 * Write out the alignment pattern (?)
	 */
	if (p_align_fp && *p_align_fp) {

		int			size	= 0,
					n;

		char			buf[BUFSIZ];


		if (!(path = getformfile(name, ALIGN_PTRN)))
			return (-1);
		if ((fd = open_locked(path, "w", MODE_READ)) < 0) {
			Free (path);
			return (-1);
		}

		while ((n = fread(buf, 1, BUFSIZ, *p_align_fp)) != 0) {
			size += n;
			write (fd, buf, n);
		}
		close(fd);

		if (!size)
			Unlink(path);

		Free(path);
	}

	return (0);
}
