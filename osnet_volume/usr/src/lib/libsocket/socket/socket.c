/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)socket.c	1.18	97/08/22 SMI"	/* SVr4.0 1.8	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

extern int _so_socket();
extern int _s_netconfig_path();
extern int _setsockopt();

int _socket_create(int, int, int, int);

#pragma weak socket = _socket

int
_socket(int family, int type, int protocol)
{
	return (_socket_create(family, type, protocol, SOV_DEFAULT));
}

/*
 * Used by the BCP library.
 */
int
_socket_bsd(int family, int type, int protocol)
{
	return (_socket_create(family, type, protocol, SOV_SOCKBSD));
}

int
_socket_svr4(int family, int type, int protocol)
{
	return (_socket_create(family, type, protocol, SOV_SOCKSTREAM));
}

int
__xnet_socket(int family, int type, int protocol)
{
	return (_socket_create(family, type, protocol, SOV_XPG4_2));
}

/*
 * Create a socket endpoint for socket() and socketpair().
 * In SunOS 4.X and in SunOS 5.X prior to XPG 4.2 the only error
 * that could be returned due to invalid <family, type, protocol>
 * was EPROTONOSUPPORT. (While the SunOS 4.X source contains EPROTOTYPE
 * error as well that error can only be generated if the kernel is
 * incorrectly configured.)
 * For backwards compatibility only applications that request XPG 4.2
 * (through c89 or XOPEN_SOURCE) will get EPROTOTYPE or EAFNOSUPPORT errors.
 */
int
_socket_create(int family, int type, int protocol, int version)
{
	int fd;

	/*
	 * Try creating without knowing the device assuming that
	 * the transport provider is registered in /etc/sock2path.
	 * If none found fall back to using /etc/netconfig to look
	 * up the name of the transport device name. This provides
	 * backwards compatibility for transport providers that have not
	 * yet been converted to using /etc/sock2path.
	 * XXX When all transport providers use /etc/sock2path this
	 * part of the code can be removed.
	 */
	fd = _so_socket(family, type, protocol, NULL, version);
	if (fd == -1) {
		char *devpath;
		int saved_errno = errno;
		int prototype = 0;

		switch (saved_errno) {
		case EAFNOSUPPORT:
		case EPROTOTYPE:
			if (version != SOV_XPG4_2)
				saved_errno = EPROTONOSUPPORT;
			break;
		case EPROTONOSUPPORT:
			break;

		default:
			errno = saved_errno;
			return (-1);
		}
		if (_s_netconfig_path(family, type, protocol,
					&devpath, &prototype) == -1) {
			errno = saved_errno;
			return (-1);
		}
		fd = _so_socket(family, type, protocol, devpath, version);
		free(devpath);
		if (fd == -1) {
			errno = saved_errno;
			return (-1);
		}
		if (prototype != 0) {
			if (_setsockopt(fd, SOL_SOCKET, SO_PROTOTYPE,
			    (caddr_t)&prototype, (int)sizeof (prototype)) < 0) {
				(void) close(fd);
				/*
				 * setsockopt often fails with ENOPROTOOPT
				 * but socket() should fail with
				 * EPROTONOSUPPORT.
				 */
				errno = EPROTONOSUPPORT;
				return (-1);
			}
		}
	}
	return (fd);
}
