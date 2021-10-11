/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)can_use_af.c	1.2	99/11/18 SMI"

#include "rpc_mt.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/sockio.h>

/*
 * Determine if we have a configured interface for the specified address
 * family.
 */
int
__can_use_af(sa_family_t af) {

	struct lifnum	lifn;
	int		fd;

	if ((fd =  open("/dev/udp", O_RDONLY)) < 0) {
		return (0);
	}
	lifn.lifn_family = af;
	lifn.lifn_flags = IFF_UP & !(IFF_NOXMIT | IFF_DEPRECATED);
	if (ioctl(fd, SIOCGLIFNUM, &lifn, sizeof (lifn)) < 0) {
		lifn.lifn_count = 0;
	}

	(void) close(fd);
	return (lifn.lifn_count);
}
