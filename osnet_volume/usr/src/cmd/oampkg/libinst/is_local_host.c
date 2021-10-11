/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*LINTLIBRARY*/
#ident	"@(#)is_local_host.c	1.2	99/06/21 SMI"

/*
 * These three functions were grabbed from libadmutil. At some time, these
 * should probably migrate to some common library.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>

static void get_net_if_ip_addr(char *if_name, char *ip_addr);
static int get_net_if_names(struct ifconf *pifc);

/*
 * Given a host name, check to see if it points to the local host.
 * If it does, return 1, else return 0.
 *
 * This is done by getting all the names of the local network interfaces
 * and comparing them against the passed in host name.
 */
int
is_local_host(char *host) {
	struct ifconf	ifc;		/* interface config buffer	*/
	struct ifreq	*ifr;		/* ptr to interface request	*/
	struct hostent	*hp;
	u_long		addr;
	int		i;
	char		ip_addr[4*4];
	char		**q;
	int			 ret = 0;

	if ((get_net_if_names(&ifc)) != -1) {
		for (ifr = ifc.ifc_req,
		    i = (ifc.ifc_len / sizeof (struct ifreq));
		    i > 0; --i, ++ifr) {
			get_net_if_ip_addr(ifr->ifr_name, ip_addr);

			addr = inet_addr(ip_addr);
			hp = gethostbyaddr((char *)&addr, sizeof (addr),
			    AF_INET);
			/*
			 * Ignore interface if it doesn't have a name.
			 */
			if (hp != NULL) {

				if (strcasecmp(host, hp->h_name) == 0) {
					ret = 1;
					break;
				}
				for (q = hp->h_aliases; *q != 0; q++) {

					if (strcasecmp(host, *q) == 0) {
						ret = 1;
						break;
				}
			}
		}
	}
	free(ifc.ifc_buf);
	}
	return (ret);
}

static void
get_net_if_ip_addr(char *if_name, char *ip_addr)
{
	int sd;			/* socket descriptor */
	struct ifreq ifr;	/* interface request block */
	char *addrp;
	struct sockaddr_in *sinp;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		return;

	strcpy(ifr.ifr_name, if_name);
	if (ioctl(sd, SIOCGIFADDR, (char *) &ifr) < 0) {
		(void) close(sd);
		return;
	}

	(void) close(sd);

	sinp = (struct sockaddr_in *)&ifr.ifr_addr;
	addrp = inet_ntoa(sinp->sin_addr);

	strcpy(ip_addr, addrp);
}

static int
get_net_if_names(struct ifconf *pifc)
{
	char *buf;		/* buffer for socket info */
	int sd;			/* socket descriptor */
	struct ifconf ifc;	/* interface config buffer */
	int max_if = 0;		/* max interfaces */

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		return (-1);

	if (ioctl(sd, SIOCGIFNUM, (char *) &max_if) < 0) {
		(void) close(sd);
		return (-2);
	}

	if ((buf = (char *)malloc(max_if * sizeof (struct ifreq))) == NULL) {
		(void) close(sd);
		return (-3);
	}

	ifc.ifc_len = max_if * sizeof (struct ifreq);
	ifc.ifc_buf = (caddr_t) buf;
	if (ioctl(sd, SIOCGIFCONF, (char *) &ifc) < 0) {
		(void) close(sd);
		return (-2);
	}

	(void) close(sd);

	*pifc = ifc;

	return (0);
}
