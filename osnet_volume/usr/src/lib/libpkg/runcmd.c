/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)runcmd.c	1.10	93/03/10 SMI"	/* SVr4.0  1.8.2.3	*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <pkglib.h>
#include "pkglocale.h"

static char	errfile[L_tmpnam+1];

void
rpterr(void)
{
	FILE	*fp;
	int	c;

	if (errfile[0]) {
		if (fp = fopen(errfile, "r")) {
			while ((c = getc(fp)) != EOF)
				putc(c, stderr);
			(void) fclose(fp);
		}
		(void) unlink(errfile);
		errfile[0] = '\0';
	}
}

void
ecleanup(void)
{
	if (errfile[0]) {
		(void) unlink(errfile);
		errfile[0] = NULL;
	}
}

int
esystem(char *cmd, int ifd, int ofd)
{
	char	*perrfile;
	int	status = 0;
	pid_t	pid;

	perrfile = tmpnam(NULL);
	if (perrfile == NULL) {
		progerr(
		    pkg_gt("unable to create temp error file, errno=%d"),
		    errno);
		return (-1);
	}
	(void) strcpy(errfile, perrfile);
	pid = vfork();
	if (pid == 0) {
		/* child */
		if (ifd > 0) {
			(void) close(0);
			(void) fcntl(ifd, F_DUPFD, 0);
			(void) close(ifd);
		}
		if (ofd >= 0 && ofd != 1) {
			(void) close(1);
			(void) fcntl(ofd, F_DUPFD, 1);
			(void) close(ofd);
		}
		freopen(errfile, "w", stderr);
		execl("/sbin/sh", "/sbin/sh", "-c", cmd, NULL);
		progerr(pkg_gt("exec of <%s> failed, errno=%d"), cmd, errno);
		exit(99);
	} else if (pid < 0) {
		logerr(pkg_gt("bad vfork(), errno=%d"), errno);
		return (-1);
	}

	/* parent process */
	sighold(SIGINT);
	pid = waitpid(pid, &status, 0);
	sigrelse(SIGINT);

	if (pid < 0)
		return (-1); /* probably interrupted */

	switch (status & 0177) {
		case 0:
		case 0177:
			status = status >> 8;
			/*FALLTHROUGH*/

		default:
			/* terminated by a signal */
			status = status & 0177;
	}
	if (status == 0)
		ecleanup();
	return (status);
}

FILE *
epopen(char *cmd, char *mode)
{
	char	*buffer, *perrfile;
	FILE	*pp;

	if (errfile[0]) {
		/* cleanup previous errfile */
		unlink(errfile);
	}

	perrfile = tmpnam(NULL);
	if (perrfile == NULL) {
		progerr(
		    pkg_gt("unable to create temp error file, errno=%d"),
		    errno);
		return ((FILE *) 0);
	}
	(void) strcpy(errfile, perrfile);

	buffer = (char *) calloc(strlen(cmd)+6+strlen(errfile), sizeof (char));
	if (buffer == NULL) {
		progerr(pkg_gt("no memory in epopen(), errno=%d"), errno);
		return ((FILE *) 0);
	}

	if (strchr(cmd, '|'))
		(void) sprintf(buffer, "(%s) 2>%s", cmd, errfile);
	else
		(void) sprintf(buffer, "%s 2>%s", cmd, errfile);

	pp = popen(buffer, mode);

	free(buffer);
	return (pp);
}

int
epclose(FILE *pp)
{
	int n;

	n = pclose(pp);
	if (n == 0)
		ecleanup();
	return (n);
}
