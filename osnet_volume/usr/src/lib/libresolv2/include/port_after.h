/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)port_after.h 1.3     99/03/21 SMI"

#undef HAS_SA_LEN
#define USE_POSIX
#define POSIX_SIGNALS
#define NETREAD_BROKEN
#define USE_WAITPID
#define HAVE_FCHMOD
#define NEED_PSELECT
#define SETGRENT_VOID
#define SETPWENT_VOID
#define SIOCGIFCONF_ADDR
#define IP_OPT_BUF_SIZE 40

#undef _PATH_NAMED
#define _PATH_NAMED	"/usr/sbin/named"
#undef _PATH_XFER
#define _PATH_XFER	"/usr/sbin/named-xfer"
#undef _PATH_PIDFILE
#define _PATH_PIDFILE	"/etc/named.pid"

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define INADDR_NONE	0xffffffff

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
#endif

/*
 * Note: In BIND distribution constant AF_INET6 is declared here
 * for portability to platforms where AF_INET6 may not exist.
 * Solaris platform defines AF_INET6 in the standard system header
 * <sys/socket.h> where address family constants are defined and
 * definition needs to be imported from that header and is
 * removed from here.
 */


#include <sys/types.h>
/* Solaris 2.6 defines gethostname() in unistd.h, so we do not define it here
*/
/* extern int gethostname(char *, size_t); */

#define NEED_STRSEP
extern char *strsep(char **, const char *);

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

/*
 * Solaris defines this in <netdb.h> instead of in <sys/param.h>.  We don't
 * define it in our <netdb.h>, so we define it here.
 */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

/*
 * Definitions for MT safe libresolv.
 */
#include <resolv_mt.h>
