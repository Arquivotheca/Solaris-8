/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)rrmdir.c	1.11	93/03/09 SMI"	/* SVr4.0  1.4	*/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pkglocale.h"

int
rrmdir(char *path)
{
	char cmd[PATH_MAX+13];
	int  i;

	/*
	 * For some reason, a simple "rm -rf" will remove the contents
	 * of the directory, but will fail to remove the directory itself
	 * with "No such file or directory" when running the pkg commands
	 * under a virtual root via the "chroot" command.  This has been
	 * seen so far only with the `pkgremove' command, and when the
	 * the directory is NFS mounted from a 4.x server.  This should
	 * probably be revisited at a later time, but for now we'll just
	 * remove the directory contents first, then the directory.
	 */
	/* bug id #1081589  remove from root all file */
	if (path == NULL){
		(void) fprintf(stderr,
		    pkg_gt("warning: rrmdir(path==NULL): nothing deleted \n"));
		return (0);
	}

	(void) sprintf(cmd, "/bin/rm -rf %s/*", path);
	i = system(cmd);
	if (i == 0) {
		(void) sprintf(cmd, "/bin/rm -rf %s", path);
		i = system(cmd);
	}
	return (i ? 1 : 0);
}
