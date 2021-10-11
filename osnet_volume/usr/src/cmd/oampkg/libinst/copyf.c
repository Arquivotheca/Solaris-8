/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)copyf.c	1.9	93/03/10 SMI"	/* SVr4.0 1.5.1.1	*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"

#define	ERR_NODIR	"unable to create directory <%s>, errno=%d"
#define	ERR_STAT	"unable to stat pathname <%s>, errno=%d"
#define	ERR_READ	"unable to read <%s>, errno=%d"
#define	ERR_WRITE	"unable to write <%s>, errno=%d"
#define	ERR_OPEN_READ	"unable to open <%s> for reading, errno=%d"
#define	ERR_OPEN_WRITE	"unable to open <%s> for writing, errno=%d"
#define	ERR_MODTIM	"unable to reset access/modification time of <%s>, " \
			"errno=%d"

int
copyf(char *from, char *to, long mytime)
{
	static char buf[BUFSIZ*8];
	struct stat status;
	struct utimbuf times;
	int	fd1, fd2, nread;
	char	*pt;

	if (mytime == 0) {
		if (stat(from, &status)) {
			progerr(gettext(ERR_STAT), from, errno);
			return (-1);
		}
		times.actime = status.st_atime;
		times.modtime = status.st_mtime;
	} else {
		times.actime = mytime;
		times.modtime = mytime;
	}

	if ((fd1 = open(from, O_RDONLY, 0)) == -1) {
		progerr(gettext(ERR_OPEN_READ), from, errno);
		return (-1);
	}
	if ((fd2 = open(to, O_WRONLY | O_TRUNC | O_CREAT, 0666)) == -1) {
		pt = to;
		while (pt = strchr(pt+1, '/')) {
			*pt = '\0';
			if (isdir(to)) {
				if (mkdir(to, 0755)) {
					progerr(gettext(ERR_NODIR), to, errno);
					*pt = '/';
					return (-1);
				}
			}
			*pt = '/';
		}
		if ((fd2 = open(to, O_WRONLY | O_TRUNC | O_CREAT, 0666)) ==
		    -1) {
			progerr(gettext(ERR_OPEN_WRITE), to, errno);
			(void) close(fd1);
			return (-1);
		}
	}
	while ((nread = read(fd1, buf, sizeof (buf))) > 0) {
		if (write(fd2, buf, nread) != nread) {
			progerr(gettext(ERR_WRITE), to, errno);
			return (-1);
		}
	}
	if (nread < 0) {
		progerr(gettext(ERR_READ), from, errno);
		return (-1);
	}
	(void) close(fd1);
	(void) close(fd2);

	if (utime(to, &times)) {
		progerr(gettext(ERR_MODTIM), to, errno);
		return (-1);
	}

	return (0);
}
