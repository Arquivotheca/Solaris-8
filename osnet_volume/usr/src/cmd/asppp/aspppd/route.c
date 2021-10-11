#ident	"@(#)route.c	1.3	95/02/25 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/sockio.h>

#include "ifconfig.h"
#include "ipd_ioctl.h"
#include "log.h"
#include "path.h"
#include "route.h"

void
add_default_route(struct path *p)
{
	struct rtentry rt;
	int ip;

	if (p->inf.iftype != IPD_PTP) {
		log(1, "add_default_route: default route not allowed on "
			"ipd device\n");
		return;
	}

	memset((void *)&rt, 0, sizeof (rt));  /* sets default route for dst */
	((struct sockaddr_in *)&rt.rt_gateway)->sin_family = AF_INET;
	((struct sockaddr_in *)&rt.rt_gateway)->sin_addr.s_addr =
							get_if_dst_addr(p);
	rt.rt_flags = RTF_UP | RTF_GATEWAY;

	if ((ip = open("/dev/ip", O_RDONLY)) < 0)
		fail("add_default_route: open failed\n");

	if (ioctl(ip, SIOCADDRT, &rt) < 0)
		switch (errno) {
		case EEXIST:
		case ENETUNREACH:
		case ENOMEM:
			log(1, "add_default_route: ioctl failed\n"
			"         Error %d: %s\n", errno, strerror(errno));
		break;
		default:
			fail("add_default_route: ioctl failed\n");
	}

	if (close(ip) < 0)
		fail("add_default_route: close failed\n");
}

void
delete_default_route(struct path *p)
{
	struct rtentry rt;
	int ip;

	if (p->inf.iftype != IPD_PTP)
		return;

	memset((void *)&rt, 0, sizeof (rt));   /* sets default route for dst */
	((struct sockaddr_in *)&rt.rt_gateway)->sin_family = AF_INET;
	((struct sockaddr_in *)&rt.rt_gateway)->sin_addr.s_addr =
							get_if_dst_addr(p);

	if ((ip = open("/dev/ip", O_RDWR)) < 0)
		fail("delete_default_route: open failed\n");

	if (ioctl(ip, SIOCDELRT, &rt) < 0 && errno != ESRCH)
		fail("delete_default_route: ioctl failed\n");

	if (close(ip) < 0)
		fail("delete_default_route: close failed\n");
}
