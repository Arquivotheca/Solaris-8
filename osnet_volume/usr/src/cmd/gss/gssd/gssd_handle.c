/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident  "@(#)gssd_handle.c 1.6     97/10/27 SMI"
/*	from kerbd_handle.c	1.3	92/01/29 SMI */

/*
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986,1987,1988,1989,1995  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

/*
 * gssd_handle.c, Interface to gssd
 *
 */

#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <stdio.h>
#include <string.h>
#include <netconfig.h>
#include <sys/utsname.h>
#include "gssd.h"

#ifdef DEBUG
#define	dprt(msg)	(void) fprintf(stderr, "%s\n", msg);
#else
#define	dprt(msg)
#endif /* DEBUG */


/*
 * Keep the handle cached.  This call may be made quite often.
 */

CLIENT *
getgssd_handle()
{
	void *localhandle;
	struct netconfig *nconf;
	struct netconfig *tpconf;
	static CLIENT *clnt;
	struct timeval wait_time;
	struct utsname u;
	static char *hostname;
	static bool_t first_time = TRUE;

#define	TOTAL_TIMEOUT	1000	/* total timeout talking to gssd */
#define	TOTAL_TRIES	1	/* Number of tries */

	if (clnt)
		return (clnt);
	if (!(localhandle = setnetconfig()))
		return (NULL);
	tpconf = NULL;
	if (first_time == TRUE) {
		if (uname(&u) == -1)
			return ((CLIENT *) NULL);
		if ((hostname = strdup(u.nodename)) == (char *) NULL)
			return ((CLIENT *) NULL);
		first_time = FALSE;
	}
	while (nconf = getnetconfig(localhandle)) {
		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			if (nconf->nc_semantics == NC_TPI_COTS_ORD) {
				clnt = clnt_tp_create(hostname,
					GSSPROG, GSSVERS, nconf);
				if (clnt) {
					dprt("got COTS_ORD\n");
					break;
				}
			} else {
				tpconf = nconf;
			}
		}
	}
	if ((clnt == NULL) && (tpconf)) {

		/* Now, try the connection-oriented loopback transport */

		clnt = clnt_tp_create(hostname, GSSPROG, GSSVERS, tpconf);
#ifdef DEBUG
		if (clnt) {
			dprt("got COTS\n");
		}
#endif DEBUG
	}
	endnetconfig(localhandle);

	/*
	 * This bit of code uses an as yet unimplemented argument to
	 * clnt_control(). CLSET_SVC_PRIV specifies that the underlying
	 * loopback transport should be checked to ensure it is
	 * connected to a process running as root. If so, the clnt_control()
	 * call returns TRUE. If not, it returns FALSE.
	 */

#ifdef CLSET_SVC_PRIV

	if (clnt_control(clnt, CLSET_SVC_PRIV, NULL) != TRUE) {
		clnt_destroy(clnt);
		clnt = NULL;
		return (NULL);
	{
#endif
	if (clnt == NULL)
		return (NULL);

	clnt->cl_auth = authsys_create("", getuid(), 0, 0, NULL);
	if (clnt->cl_auth == NULL) {
		clnt_destroy(clnt);
		clnt = NULL;
		return (NULL);
	}
	wait_time.tv_sec = TOTAL_TIMEOUT/TOTAL_TRIES;
	wait_time.tv_usec = 0;
	(void) clnt_control(clnt, CLSET_RETRY_TIMEOUT, (char *)&wait_time);

	return (clnt);
}
