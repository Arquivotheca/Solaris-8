/*
 * Copyright (c) 1986-1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)openchild.c	1.15	98/07/13 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)openchild.c 1.2 89/03/10 Copyr 1986 Sun Micro";
#endif
/*
 * openchild.c,
 *
 * Open two pipes to a child process, one for reading, one for writing. The
 * pipes are accessed by FILE pointers. This is NOT a public interface, but
 * for internal use only!
 */
#include <stdio.h>
#include <sys/types.h>
#include <rpc/types.h>
#include <rpc/trace.h>
#include <unistd.h>
#include <sys/resource.h>
#include "rpc_mt.h"


/*
 * returns pid, or -1 for failure
 */
__rpc_openchild(command, fto, ffrom)
	char	*command;
	FILE	**fto;
	FILE	**ffrom;
{
	int	i;
	int	pid;
	int	pdto[2];
	int	pdfrom[2];
	static int dtbsize = 0;

	trace1(TR___rpc_openchild, 0);

	if (dtbsize == 0) {
		struct rlimit rl;
		dtbsize = FD_SETSIZE;
		if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
			dtbsize = rl.rlim_cur;
		}
	}

	if (pipe(pdto) < 0) {
		goto error1;
	}
	if (pipe(pdfrom) < 0) {
		goto error2;
	}
	switch (pid = fork()) {
	case -1:
		goto error3;

	case 0:
		/*
		 * child: read from pdto[0], write into pdfrom[1]
		 */
		(void) close(0);
		dup(pdto[0]);
		(void) close(1);
		dup(pdfrom[1]);

		fflush(stderr);
		for (i = dtbsize - 1; i >= 3; i--) {
			(void) close(i);
		}
		fflush(stderr);
		execlp(command, command, 0);
		perror("exec");
		trace1(TR___rpc_openchild, 1);
		_exit(~0);

	default:
		/*
		 * parent: write into pdto[1], read from pdfrom[0]
		 */
		*fto = fdopen(pdto[1], "w");
		(void) close(pdto[0]);
		*ffrom = fdopen(pdfrom[0], "r");
		(void) close(pdfrom[1]);
		break;
	}
	trace1(TR___rpc_openchild, 1);
	return (pid);

	/*
	 * error cleanup and return
	 */
error3:
	(void) close(pdfrom[0]);
	(void) close(pdfrom[1]);
error2:
	(void) close(pdto[0]);
	(void) close(pdto[1]);
error1:
	trace1(TR___rpc_openchild, 1);
	return (-1);
}
