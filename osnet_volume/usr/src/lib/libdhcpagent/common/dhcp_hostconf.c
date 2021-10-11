/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcp_hostconf.c	1.1	99/03/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/dhcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>			/* memcpy */
#include <fcntl.h>

#include "dhcp_hostconf.h"

static void		relativize_time(DHCP_OPT *, time_t, time_t);

/* LINTLIBRARY */

/*
 * ifname_to_hostconf(): converts an interface name into a hostconf file for
 *			 that interface
 *
 *   input: const char *: the interface name
 *  output: char *: the hostconf filename
 *    note: uses an internal static buffer (not threadsafe)
 */

char *
ifname_to_hostconf(const char *ifname)
{
	static char filename[sizeof (DHCP_HOSTCONF_TMPL) + IFNAMSIZ];

	(void) sprintf(filename, "%s%s%s", DHCP_HOSTCONF_PREFIX, ifname,
	    DHCP_HOSTCONF_SUFFIX);

	return (filename);
}

/*
 * remove_hostconf(): removes an interface.dhc file
 *
 *   input: const char *: the interface name
 *  output: int: 0 if the file is removed, -1 if it can't be removed
 *          (errno is set)
 */

int
remove_hostconf(const char *ifname)
{
	return (unlink(ifname_to_hostconf(ifname)));
}

/*
 * read_hostconf(): reads the contents of an <if>.dhc file into a PKT_LIST
 *
 *   input: const char *: the interface name
 *	    PKT_LIST **: a pointer to a PKT_LIST * to store the info in
 *  output: int: 0 if the file is read and loaded into the PKT_LIST *
 *	    successfully, -1 otherwise (errno is set)
 *    note: the PKT and PKT_LIST are dynamically allocated by read_hostconf()
 */

int
read_hostconf(const char *ifname, PKT_LIST **plpp)
{
	PKT_LIST	*plp = NULL;
	PKT		*pkt = NULL;
	int		fd;
	time_t		orig_time, current_time = time(NULL);
	uint32_t	lease;
	uint32_t	magic;

	fd = open(ifname_to_hostconf(ifname), O_RDONLY);
	if (fd == -1)
		return (-1);
	/*
	 * read the packet back in from disk, and run it through
	 * _dhcp_options_scan(). note that we use calloc() since
	 * _dhcp_options_scan() relies on the packet being zeroed.
	 */

	if ((plp = calloc(1, sizeof (PKT_LIST))) == NULL)
		goto failure;

	if (read(fd, &magic, sizeof (magic)) != sizeof (magic))
		goto failure;

	if (magic != DHCP_HOSTCONF_MAGIC)
		goto failure;

	if (read(fd, &orig_time, sizeof (orig_time)) != sizeof (orig_time))
		goto failure;

	if (read(fd, &plp->len, sizeof (plp->len)) != sizeof (plp->len))
		goto failure;

	if ((pkt = malloc(plp->len)) == NULL)
		goto failure;

	if (read(fd, pkt, plp->len) != plp->len)
		goto failure;

	plp->pkt = pkt;

	if (_dhcp_options_scan(plp) != 0)
		goto failure;

	/*
	 * make sure the lease is still valid.
	 */

	if (plp->opts[CD_LEASE_TIME] != NULL &&
	    plp->opts[CD_LEASE_TIME]->len == sizeof (lease_t)) {

		(void) memcpy(&lease, plp->opts[CD_LEASE_TIME]->value,
		    sizeof (lease_t));

		lease = ntohl(lease);
		if ((lease != DHCP_PERM) && (orig_time + lease) <= current_time)
			goto failure;
	}

	relativize_time(plp->opts[CD_T1_TIME], orig_time, current_time);
	relativize_time(plp->opts[CD_T2_TIME], orig_time, current_time);
	relativize_time(plp->opts[CD_LEASE_TIME], orig_time, current_time);

	*plpp = plp;
	(void) close(fd);
	return (0);

failure:
	*plpp = NULL;
	free(pkt);
	free(plp);
	(void) close(fd);
	return (-1);
}

/*
 * write_hostconf(): writes the contents of a PKT_LIST into an <if>.dhc file
 *
 *   input: const char *: the interface name
 *	    PKT_LIST *: a pointer to a PKT_LIST to get info from
 *	    time_t: a starting time to treat the relative lease times
 *		    in the packet as relative to
 *  output: int: 0 if the file is written successfully, -1 otherwise
 *	    (errno is set)
 */

int
write_hostconf(const char *ifname, PKT_LIST *pl, time_t relative_to)
{
	int		fd;
	struct iovec	iov[4];
	int		retval;
	uint32_t	magic = DHCP_HOSTCONF_MAGIC;

	fd = open(ifname_to_hostconf(ifname), O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fd == -1)
		return (-1);

	/*
	 * first write our magic number, then the relative time of the
	 * leases, the length of the packet, and the packet itself.
	 * we will then use the relative time in read_hostconf() to
	 * recalculate the lease times.
	 */

	iov[0].iov_base = (caddr_t)&magic;
	iov[0].iov_len  = sizeof (magic);
	iov[1].iov_base = (caddr_t)&relative_to;
	iov[1].iov_len  = sizeof (relative_to);
	iov[2].iov_base = (caddr_t)&pl->len;
	iov[2].iov_len	= sizeof (pl->len);
	iov[3].iov_base = (caddr_t)pl->pkt;
	iov[3].iov_len	= pl->len;

	retval = writev(fd, iov, sizeof (iov) / sizeof (*iov));

	(void) close(fd);

	if (retval != (pl->len + sizeof (pl->len) + sizeof (relative_to) +
	    sizeof (magic)))
		return (-1);

	return (0);
}

/*
 * relativize_time(): re-relativizes a time in a DHCP option
 *
 *   input: DHCP_OPT *: the DHCP option parameter to convert
 *	    time_t: the time the leases in the packet are currently relative to
 *	    time_t: the current time which leases will become relative to
 *  output: void
 */

static void
relativize_time(DHCP_OPT *option, time_t orig_time, time_t current_time)
{
	uint32_t	pkt_time;
	time_t		time_diff = current_time - orig_time;

	if (option == NULL || option->len != sizeof (lease_t))
		return;

	(void) memcpy(&pkt_time, option->value, option->len);
	if (ntohl(pkt_time) != DHCP_PERM)
		pkt_time = htonl(ntohl(pkt_time) - time_diff);

	(void) memcpy(option->value, &pkt_time, option->len);
}
