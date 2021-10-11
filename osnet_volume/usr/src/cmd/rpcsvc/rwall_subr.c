#pragma ident	"@(#)rwall_subr.c	1.8	97/04/24 SMI"

/*
 * rwall_subr.c
 *	The server procedure for rwalld
 *
 * Copyright (c) 1984,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <thread.h>
#include <rpcsvc/rwall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>

#define	WALL_PROG	"/usr/sbin/wall"

static mutex_t wall_mutex = DEFAULTMUTEX;
static char *oldmsg;

/* ARGSUSED */
bool_t
wallproc_wall_1_svc(wrapstring *argp, void *res, struct svc_req *rqstp)
{
	char *msg;
	FILE *fp;
	int rval;
	struct stat wall;

	msg = *argp;

	/*
	 * Do not wall the same message twice in case of a retransmission
	 * in the rare case that two walls arrive close enough with
	 * a retransmission we might get a duplicate, but that is OK.
	 */
	(void) mutex_lock(&wall_mutex);
	if ((oldmsg != 0) && (strcmp(msg, oldmsg) == 0)) {
		(void) mutex_unlock(&wall_mutex);
		return (TRUE);
	}

	if (oldmsg)
		free(oldmsg);
	oldmsg = strdup(msg);

	rval = stat(WALL_PROG, &wall);

	/*
	 * Make sure the wall programs exists, is executeable, and runs
	 */
	if (rval == -1 || (wall.st_mode & S_IXUSR) == 0 ||
	    (fp = popen(WALL_PROG, "w")) == NULL) {
		syslog(LOG_NOTICE,
			"rwall message received but could not execute %s",
			WALL_PROG);
		syslog(LOG_NOTICE, msg);
#ifdef	DEBUG
		(void) fprintf(stderr,
			"rwall message received but could not execute %s",
			WALL_PROG);
		(void) fprintf(stderr, msg);
#endif
		return (TRUE);
	}

	(void) fprintf(fp, "%s", msg);
	(void) pclose(fp);
	(void) mutex_unlock(&wall_mutex);

	return (TRUE);
}

/* ARGSUSED */
int
wallprog_1_freeresult(SVCXPRT *transp, xdrproc_t proc, caddr_t arg)
{
	return (TRUE);
}
