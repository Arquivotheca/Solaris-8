#ident	"@(#)ethers.c	1.10	97/10/24 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc. All rights reserved.
 */

/*
 * This file contains routines that implement the ETHERS compatibility mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "ethers.h"
#include <locale.h>

extern int ether_ntohost(char *, ether_addr_t);
extern char *ether_ntoa(ether_addr_t);

/*
 * Check for prerequisites for ETHERS compatibility mode.
 * Returns 0 if /tftpboot exists with inetboot.* entries, nonzero otherwise.
 */
int
stat_boot_server(void)
{
	register int	err = ENOENT;
	DIR		*dirp;
	struct dirent	*dp;
	struct stat	sb;

	if (stat(ETHERS_TFTPDIR, &sb) == 0 &&
	    ((sb.st_mode & S_IFMT) == S_IFDIR ||
	    (sb.st_mode & S_IFMT) == S_IFLNK)) {
		if ((dirp = opendir(ETHERS_TFTPDIR)) != NULL) {
			while ((dp = readdir(dirp)) != NULL) {
				if (strncasecmp(ETHERS_BOOTFILE, dp->d_name,
				    strlen(ETHERS_BOOTFILE)) == 0) {
					err = 0;
					break;
				}
			}
			(void) closedir(dirp);
		}
	}
	return (err);
}

/*
 * Given an ethernet address, return 0 if a machine has an IP address binding.
 * No address verification is done. We let the higher level routines do this.
 *
 * Fill in the passed struct in_addr with the client's IP address.
 *
 * XXXX - can we assume the first address is primary???
 */
static int
ether_to_ip(ether_addr_t e, struct in_addr *ip)
{
	register int err = ENOENT;
	register struct hostent	*hp;
	char		name[MAXHOSTNAMELEN + 1];

	if (e == NULL || ip == NULL)
		return (EINVAL);

	if (ether_ntohost(name, e) == 0) {
		hp = gethostbyname(name);
		if (hp != NULL && hp->h_addrtype == AF_INET &&
		    hp->h_length == sizeof (struct in_addr)) {
			(void) memcpy((char *)ip, hp->h_addr_list[0],
			    sizeof (struct in_addr));
			err = 0;
		}
	}
	return (err);
}

/*
 * Search the ethers table for a VALID entry. Returns number of records
 * found, and fills in pnp.
 */
int
lookup_ethers(struct in_addr *netp, struct in_addr *subnetp,
    ether_addr_t e, PN_REC *pnp)
{
	struct in_addr ip;
	char ntoab_a[NTOABUF], ntoab_b[NTOABUF];

	if (netp == NULL || subnetp == NULL || e == NULL || pnp == NULL)
		return (0);

	if (ether_to_ip(e, &ip) != 0)
		return (0);

	if (netp->s_addr != (ip.s_addr & subnetp->s_addr)) {
		ip.s_addr &= subnetp->s_addr;
		dhcpmsg(LOG_ERR,
"Ethers entry for client: %1$s incorrect for network (%2$s != %3$s).\n",
		    ether_ntoa(e), inet_ntoa_r(*netp, ntoab_a),
		    inet_ntoa_r(ip, ntoab_b));
		return (0);
	}

	/* Valid entry. Fill in PN_REC. */
	pnp->cid_len = sizeof (ether_addr_t);
	(void) memcpy(pnp->cid, &e[0], pnp->cid_len);
	pnp->flags = F_AUTOMATIC | F_MANUAL;
	pnp->clientip.s_addr = ip.s_addr;
	pnp->serverip.s_addr = server_ip.s_addr;
	pnp->lease = DHCP_PERM;
	pnp->macro[0] = '\0';
	pnp->comment[0] = '\0';

	return (1);
}

/*
 * Produce an encode list containing values for BootSrv{A,N}, BootFile.
 * BootSrv{A,N} will be the server's interface address and hostname. BootFile
 * will be the concatentation of ETHERS_TFTPDIR and the ASCII hex representation
 * of the client's IP address.
 *
 * Returns 0 for success, nonzero otherwise.
 */
int
ethers_encode(IF *ifp, struct in_addr *ip, ENCODE **ecpp)
{
	int	uc;
	char	hexip[40];
	char	bootfile[128];
	ENCODE	*ecp;
	struct	hostent *hp;

	if (ifp == NULL || ip == NULL || ecpp == NULL)
		return (EINVAL);

	uc = 39;
	(void) octet_to_ascii((u_char *)ip, sizeof (struct in_addr), hexip,
	    &uc);
	(void) strcat(hexip, ETHERS_SUFFIX);
	uc = strlen(hexip);

	(void) sprintf(bootfile, "%s/%s", ETHERS_TFTPDIR, hexip);
	if (access(bootfile, F_OK) == 0) {
		ecp = make_encode(CD_BOOTFILE, uc, (void *)hexip, 1);
		*ecpp = ecp;

		ecp->next = make_encode(CD_SIADDR, sizeof (struct in_addr),
		    (void *)&ifp->addr, 1);
		ecp = ecp->next;

		hp = gethostbyaddr((char *)&ifp->addr, sizeof (struct in_addr),
		    AF_INET);
		if (hp != NULL) {
			ecp->next = make_encode(CD_SNAME, strlen(hp->h_name),
			    hp->h_name, 1);
		}

		return (0);
	}

	return (errno);
}
