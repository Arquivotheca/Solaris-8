#ident	"@(#)ifconfig.c	1.10	97/01/02 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/sockio.h>

#include "ifconfig.h"
#include "iflist.h"
#include "ipd_ioctl.h"
#include "log.h"
#include "path.h"

static void	set_if_name(struct path *);
static int	if_socket(void);

static struct ifreq	ifr;

static void
set_if_name(struct path *p)
{
	memset((void *)&ifr, 0, sizeof (ifr));
	if (sprintf(ifr.ifr_name, "%s%d",
	    (p->inf.iftype == IPD_MTP ? "ipd" : "ipdptp"), p->inf.ifunit) < 0)
		fail("set_if_name: sprintf failed\n");
}

static int
if_socket(void)
{
	static int s = -1;
	if (s < 0)
	    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
		fail("if_socket: socket failed\n");
	return (s);
}

u_long
get_if_addr(struct path *p)
{
	u_long addr;
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFADDR, &ifr) < 0)
	    fail("get_if_addr: ioctl failed\n");
	addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	log(42, "get_if_addr: returned %x\n", addr);
	return (addr);
}

void
set_if_addr(struct path *p, u_long if_addr)
{
	log(42, "set_if_addr: setting to %x\n", if_addr);
	set_if_name(p);
	ifr.ifr_addr.sa_family = AF_INET;
	((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = if_addr;
	if (ioctl(if_socket(), SIOCSIFADDR, &ifr) < 0)
	    fail("set_if_addr: ioctl failed\n");
}

u_long
get_if_dst_addr(struct path *p)
{
	u_long addr;
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFDSTADDR, &ifr) < 0)
	    fail("get_if_dst_addr: ioctl failed\n");
	addr = ((struct sockaddr_in *)&ifr.ifr_dstaddr)->sin_addr.s_addr;
	log(42, "get_if_dst_addr: returned %x\n", addr);
	return (addr);
}

void
set_if_dst_addr(struct path *p, u_long if_dst_addr)
{
	log(42, "set_if_dst_addr: setting to %x\n", if_dst_addr);
	set_if_name(p);
	ifr.ifr_dstaddr.sa_family = AF_INET;
	((struct sockaddr_in *)&ifr.ifr_dstaddr)->sin_addr.s_addr =
	    if_dst_addr;
	if (ioctl(if_socket(), SIOCSIFDSTADDR, &ifr) < 0)
	    fail("set_if_dst_addr: ioctl failed\n");
}

u_short
get_if_mtu(struct path *p)
{
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFMTU, &ifr) < 0)
	    fail("get_if_mtu: ioctl failed\n");
	log(42, "get_if_mtu: %d\n", (u_short)ifr.ifr_metric);
	return ((u_short)ifr.ifr_metric);
}

void
set_if_mtu(struct path *p, u_short mtu)
{
	log(42, "set_if_mtu: %d\n", mtu);
	set_if_name(p);
	ifr.ifr_metric = (int)mtu;
	if (ioctl(if_socket(), SIOCSIFMTU, &ifr) < 0)
	    fail("get_if_mtu: ioctl failed\n");
}

char
*get_new_if(void)
{
	struct iflist	*ifitem = iflist;
	static char	ifrname[IFNAMSIZ];

	ifrname[0] = '\0';

	while (ifitem) {
		if (strncmp("ipdptp", ifitem->name, 6) == 0) {
			memset((void *)&ifr, 0, sizeof (ifr));
			strcpy(ifr.ifr_name, ifitem->name);
			if (ioctl(if_socket(), SIOCGIFFLAGS, &ifr) < 0)
			    fail("get_new_if: SIOCGIFFLAGS failed\n");
			if ((ifr.ifr_flags & IFF_UP) == 0) {  /* found one */
				log(42, "get_new_if: flags = 0x%x\n",
				    ifr.ifr_flags);
				ifr.ifr_flags |= IFF_UP;
				if (ioctl(if_socket(), SIOCSIFFLAGS, &ifr) < 0)
				    fail("get_new_if: SIOCSSFFLAGS failed\n");
				log(42, "get_new_if: new flags = 0x%x\n",
				    ifr.ifr_flags);
				strcpy(ifrname, ifr.ifr_name);
				break;
			}
		}
		ifitem = ifitem->next;
	}

	if (ifrname[0] == '\0') {
		log(42, "get_new_if: wild card interface not available\n");
		return (NULL);
	} else {
		/* bug 4014892 */
		mark_interface_used(ifrname);

		log(42, "get_new_if: interface %s will be used\n", ifrname);
		return (ifrname);
	}
}

void
mark_if_down(struct path *p)
{
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFFLAGS, &ifr) < 0)
	    fail("mark_if_down: SIOCGIFFLAGS failed\n");
	log(42, "mark_if_down: flags = 0x%x\n", ifr.ifr_flags);
	ifr.ifr_flags &= ~IFF_UP;
	log(42, "mark_if_down: new flags = 0x%x\n", ifr.ifr_flags);
	if (ioctl(if_socket(), SIOCSIFFLAGS, &ifr) < 0)
	    fail("mark_if_down: SIOCSIFFLAGS failed\n");
}

void
mark_if_up(struct path *p)
{
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFFLAGS, &ifr) < 0)
	    fail("mark_if_up: SIOCGIFFLAGS failed\n");
	log(42, "mark_if_up: flags = 0x%x\n", ifr.ifr_flags);
	ifr.ifr_flags |= IFF_UP;
	log(42, "mark_if_up: new flags = 0x%x\n", ifr.ifr_flags);
	if (ioctl(if_socket(), SIOCSIFFLAGS, &ifr) < 0)
	    fail("mark_if_up: SIOCSIFFLAGS failed\n");
}

boolean_t
is_if_up(struct path *p)
{
	set_if_name(p);
	if (ioctl(if_socket(), SIOCGIFFLAGS, &ifr) < 0)
	    fail("is_if_up: SIOCGIFFLAGS failed\n");

	if ((ifr.ifr_flags & IFF_UP) != 0) {
		return (B_TRUE);
	} else {
		return (B_FALSE);
	}
}
