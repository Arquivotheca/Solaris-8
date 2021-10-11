/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * ADOPTING state of the client state machine.
 */

#pragma ident	"@(#)adopt.c	1.1	99/04/09 SMI"

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/systeminfo.h>
#include <netinet/dhcp.h>
#include <dhcpmsg.h>

#include "async.h"
#include "util.h"
#include "packet.h"
#include "interface.h"
#include "states.h"


typedef struct {
	char		dk_if_name[IFNAMSIZ];
	char		dk_ack[1];
} dhcp_kcache_t;

static int	get_dhcp_kcache(dhcp_kcache_t **, size_t *);

/*
 * dhcp_adopt(): adopts the interface managed by the kernel for diskless boot
 *
 *   input: void
 *  output: int: nonzero on success, zero on failure
 */

int
dhcp_adopt(void)
{
	int		retval;
	dhcp_kcache_t	*kcache = NULL;
	size_t		kcache_size;
	PKT_LIST	*plp = NULL;
	struct ifslist	*ifsp;

	retval = get_dhcp_kcache(&kcache, &kcache_size);
	if (retval == 0 || kcache_size < sizeof (dhcp_kcache_t)) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: cannot fetch kernel cache");
		goto failure;
	}

	dhcpmsg(MSG_DEBUG, "dhcp_adopt: fetched %s kcache", kcache->dk_if_name);

	/*
	 * convert the kernel's ACK into binary
	 */

	plp = calloc(1, sizeof (PKT_LIST));
	if (plp == NULL)
		goto failure;

	plp->len = strlen(kcache->dk_ack) / 2;
	plp->pkt = malloc(plp->len);
	if (plp->pkt == NULL)
		goto failure;

	dhcpmsg(MSG_DEBUG, "dhcp_adopt: allocated ACK of %d bytes", plp->len);

	if (ascii_to_octet(kcache->dk_ack, plp->len * 2, (uchar_t *)plp->pkt,
	    (int *)&plp->len) != 0) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: cannot convert kernel ACK");
		goto failure;
	}

	if (_dhcp_options_scan(plp) != 0) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: cannot parse kernel ACK");
		goto failure;
	}

	/*
	 * make an interface to represent the "cached interface" in
	 * the kernel, hook up the ACK packet we made, and send out
	 * the extend request (to attempt to renew the lease).
	 *
	 * we do a send_extend() instead of doing a dhcp_init_reboot()
	 * because although dhcp_init_reboot() is more correct from a
	 * protocol perspective, it introduces a window where a
	 * diskless client has no IP address but may need to page in
	 * more of this program.  we could mlockall(), but that's
	 * going to be a mess, especially with handling malloc() and
	 * stack growth, so it's easier to just renew().  the only
	 * catch here is that if we are not granted a renewal, we're
	 * totally hosed and can only bail out.
	 */

	ifsp = insert_ifs(kcache->dk_if_name, B_TRUE, &retval);
	if (ifsp == NULL)
		goto failure;

	ifsp->if_state   = ADOPTING;
	ifsp->if_dflags |= DHCP_IF_PRIMARY;

	/*
	 * move to BOUND and use the information in our ACK packet
	 */

	if (dhcp_bound(ifsp, plp) == 0) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: cannot use cached packet");
		goto failure;
	}

	if (async_start(ifsp, DHCP_EXTEND, B_FALSE) == 0) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: async_start failed");
		goto failure;
	}

	if (dhcp_extending(ifsp) == 0) {
		dhcpmsg(MSG_CRIT, "dhcp_adopt: cannot send renew request");
		goto failure;
	}

	free(kcache);
	return (1);

failure:
	free(kcache);
	if (plp != NULL)
		free(plp->pkt);
	free(plp);
	return (0);
}

/*
 * get_dhcp_kcache(): fetches the DHCP ACK and interface name from the kernel
 *
 *   input: dhcp_kcache_t **: a dynamically-allocated cache packet
 *	    size_t *: the length of that packet (on return)
 *  output: int: nonzero on success, zero on failure
 */

static int
get_dhcp_kcache(dhcp_kcache_t **kernel_cachep, size_t *kcache_size)
{
	char	dummy;
	long	size;

	size = sysinfo(SI_DHCP_CACHE, &dummy, 0);
	if (size == -1)
		return (0);

	*kcache_size   = size;
	*kernel_cachep = malloc(*kcache_size);
	if (*kernel_cachep == NULL)
		return (0);

	(void) sysinfo(SI_DHCP_CACHE, (caddr_t)*kernel_cachep, size);
	return (1);
}
