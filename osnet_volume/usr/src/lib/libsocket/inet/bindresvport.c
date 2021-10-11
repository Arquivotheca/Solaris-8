/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)bindresvport.c	1.8	99/10/25 SMI"	/* SVr4.0 1.2	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988, 1989, 1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <string.h>
#include <unistd.h>

#ifdef SYSV
#define	bzero(s, len)	(void) memset(s, 0, len)
#endif SYSV


/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(int sd, struct sockaddr_in *sin)
{
	struct sockaddr_in myaddr;
	struct sockaddr_in *bindaddr;
	int level, optname;
	int optval, len;
	int ret;

	bindaddr = sin;
	if (bindaddr == (struct sockaddr_in *)0) {
		bindaddr = &myaddr;
		bzero(bindaddr, sizeof (*bindaddr));
		bindaddr->sin_family = AF_INET;
	} else if (bindaddr->sin_family != AF_INET) {
		errno = EPFNOSUPPORT;
		return (-1);
	}

	len = sizeof (optval);
	if (getsockopt(sd, SOL_SOCKET, SO_TYPE, &optval, &len) < 0) {
		return (-1);
	}
	/*
	 * Use *_ANONPRIVBIND to ask the kernel to pick a port in the
	 * priviledged range for us.
	 */
	if (optval == SOCK_STREAM) {
		level = IPPROTO_TCP;
		optname = TCP_ANONPRIVBIND;
	} else if (optval == SOCK_DGRAM) {
		level = IPPROTO_UDP;
		optname = UDP_ANONPRIVBIND;
	} else {
		errno = EPROTONOSUPPORT;
		return (-1);
	}

	optval = 1;
	if (setsockopt(sd, level, optname, &optval, sizeof (optval)) < 0) {
		return (-1);
	}

	bindaddr->sin_port = 0;
	ret = bind(sd, (struct sockaddr *)bindaddr,
	    sizeof (struct sockaddr_in));

	/*
	 * Always turn off the option when we are done.  Note that by doing
	 * this, if the caller has set this option before calling
	 * bindresvport(), it will be unset.  But this should never happen...
	 */
	optval = 0;
	(void) setsockopt(sd, level, optname, &optval, sizeof (optval));

	if (ret >= 0 && sin != NULL) {
		/*
		 * Historical note:
		 *
		 * Past versions of this bindresvport() code have
		 * returned with the reserved port number bound
		 * filled in its "sin" parameter (if passed in), perhaps
		 * "accidently" because of the structure of historical code.
		 *
		 * This is not documented but the behavior is
		 * explicitly retained here for compatibility to minimize
		 * risk to applications, even though it is not clear if this
		 * was a design intent.
		 */
		len = sizeof (struct sockaddr_in);
		(void) getsockname(sd, (struct sockaddr *)bindaddr, &len);
	}
	return (ret);
}
