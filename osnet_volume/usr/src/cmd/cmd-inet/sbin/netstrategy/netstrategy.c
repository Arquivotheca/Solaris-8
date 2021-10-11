/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)netstrategy.c	1.1	99/02/22 SMI"

/*
 * This program does the following:
 *
 * a) Returns:
 *	0	- if the program successfully determined the net strategy.
 *	!0	- if an error occurred.
 *
 * b) If the program is successful, it prints three tokens to
 *    stdout: <root fs type> <interface name> <net config strategy>.
 *    where:
 *	<root fs type>		-	"nfs" or "ufs"
 *	<interface name>	-	"hme0" or "none"
 *	<net config strategy>	-	"dhcp", "rarp", or "none"
 *
 *    Eg:
 *	# /sbin/netstrategy
 *	#ufs hme0 dhcp
 *
 *    <root fs type> identifies the system's root file system type.
 *
 *    <interface name> is the 16 char name of the root interface, and is only
 *	set if rarp/dhcp was used to configure the interface.
 *
 *    <net config strategy> can be either "rarp", "dhcp", or "none" depending
 *	on which strategy was used to configure the interface. Is "none" if
 *	no interface was configured using a net-based strategy.
 *
 * CAVEATS: what about autoclient systems? XXX
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/isa_defs.h>
#include <unistd.h>
#include <string.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <alloca.h>
#include <sys/systeminfo.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>

#define	MAXIFS	256	/* default max number of interfaces */

#ifndef	TRUE
#define	TRUE	1
#endif	/* TRUE */
#ifndef	FALSE
#define	FALSE	0
#endif	/* FALSE */

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	struct statvfs	vfs;
	char		*root, *interface, *strategy, dummy;
	int		found;
	long		len;
	struct utsname	uts;
	struct hostent	*hp;
	struct in_addr	ip;
	int		fd, numifs, reqsize;
	struct ifreq	*reqbuf, *ifr;
	struct ifconf	ifconf;

	/* root location */
	if (statvfs("/", &vfs) < 0)
		root = "none";
	else {
		if (strncmp(vfs.f_basetype, "nfs", sizeof ("nfs") - 1) == 0)
			vfs.f_basetype[sizeof ("nfs") - 1] = '\0';
		root = vfs.f_basetype;
	}

	/*
	 * Handle the simple case where diskless dhcp tells us everything
	 * we need to know.
	 */
	if (strcmp(root, "nfs") == 0 || strcmp(root, "cachefs") == 0) {
		if ((len = sysinfo(SI_DHCP_CACHE, &dummy, 0)) > 1) {
			/* interface is first thing in cache. */
			strategy = "dhcp";
			interface = (char *)alloca(len);
			(void) sysinfo(SI_DHCP_CACHE, interface, len);
			(void) fprintf(stdout, "%s %s %s", root, interface,
			    strategy);
			return (0);
		}
	}

	/*
	 * Now it gets more interesting.
	 *
	 * If we can find the interface associated with our nodename
	 * (hostname of primary interface), we will test this interface. If
	 * there is no interface associated with our nodename, we pick the
	 * first non-loopback, up interface.
	 *
	 * Once we've selected the interface, we can determine if it is
	 * dhcp-managed if the DHCP flag is set. Unfortunately, there is
	 * no such flag for rarp-configured interfaces; in this case, we
	 * return a strategy of "rarp" only if root is remote; otherwise we
	 * state that the strategy is "none". It's really too bad we didn't
	 * add an interface flag for RARP. Maybe we should do this after
	 * IPv6 is put back (there's more flag real estate).
	 */

	if (uname(&uts) < 0) {
		(void) fprintf(stderr, "%s: uname: %s\n", argv[0],
		    strerror(errno));
		return (2);
	}
	hp = gethostbyname(uts.nodename);
	if (hp != NULL && hp->h_addrtype == AF_INET &&
	    hp->h_length == sizeof (struct in_addr)) {
		(void) memcpy((char *)&ip, hp->h_addr_list[0],
		    sizeof (struct in_addr));
		found = TRUE;
	} else {
		/* No hostname (yet). Settle for first interface. */
		found = FALSE;
	}

	if ((fd = open("/dev/ip", 0)) < 0) {
		(void) fprintf(stderr, "%s: open: %s\n", argv[0],
		    strerror(errno));
		return (2);
	}

	numifs = MAXIFS;
#ifdef	SIOCGIFNUM
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
		(void) fprintf(stderr, "%s: ioctl: %s\n", argv[0],
		    strerror(errno));
	}
#endif	/* SIOCGIFNUM */

	reqsize = numifs * sizeof (struct ifreq);
	reqbuf = (struct ifreq *)alloca(reqsize);

	ifconf.ifc_len = reqsize;
	ifconf.ifc_buf = (caddr_t)reqbuf;

	if (ioctl(fd, SIOCGIFCONF, (char *)&ifconf) < 0) {
		(void) fprintf(stderr, "%s: ioctl: %s\n", argv[0],
		    strerror(errno));
		(void) close(fd);
		return (2);
	}

	for (interface = NULL, strategy = NULL, ifr = ifconf.ifc_req;
	    ifr < &ifconf.ifc_req[ifconf.ifc_len /
	    sizeof (struct ifreq)]; ifr++) {

		struct sockaddr_in	*sin;
		short			flags;

		if (strchr(ifr->ifr_name, ':') != NULL)
			continue;	/* skip virtual interfaces */

		if (ioctl(fd, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
			(void) fprintf(stderr, "%s: ioctl: %s\n", argv[0],
			    strerror(errno));
			continue;
		}

		flags = ifr->ifr_flags;

		if ((flags & IFF_UP) == 0 ||
		    flags & (IFF_LOOPBACK | IFF_POINTOPOINT))
			continue;

		if (ioctl(fd, SIOCGIFADDR, (caddr_t)ifr) < 0) {
			(void) fprintf(stderr, "%s: ioctl: %s\n", argv[0],
			    strerror(errno));
			continue;
		}
		/* LINTED [32 bit alignment ok] */
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		if (!found || sin->sin_addr.s_addr == ip.s_addr) {
			interface = ifr->ifr_name;
			if (flags & IFF_DHCPRUNNING)
				strategy = "dhcp";
			break;
		}
	}

	(void) close(fd);

	if (strcmp(root, "nfs") == 0 || strcmp(root, "cachefs") == 0) {
		if (interface == NULL) {
			(void) fprintf(stderr,
			    "%s: cannot identify root interface.\n", argv[0]);
			return (2);
		}
		if (strategy == NULL)
			strategy = "rarp";	/*  must be rarp/bootparams */
	} else {
		if (interface == NULL)
			interface = strategy = "none";
		else {
			/*
			 * XXX we really should test for
			 * /etc/hostname.<interface>, read the file, if it
			 * is not empty, attempt to convert the hostname into
			 * an IP address, then validate that IP against the
			 * list of configured interfaces. All to determine
			 * what we could have simply by using an interface
			 * flag. Sure, we could have configured this interface
			 * using rarp, but we really can't tell w/o doing
			 * rarp again.
			 */
			if (strategy == NULL)
				strategy = "none";
		}
	}

	(void) fprintf(stdout, "%s %s %s", root, interface, strategy);

	return (0);
}
