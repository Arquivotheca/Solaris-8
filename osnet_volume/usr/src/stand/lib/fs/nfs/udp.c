/*
 * Copyright (C) 1991-1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * udp.c, Code implementing the UDP internet protocol.
 */

#pragma ident	"@(#)udp.c	1.1	99/02/22 SMI"

#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/salib.h>
#include "socket_inet.h"
#include "udp_inet.h"
#include "ipv4.h"
#include "mac.h"
#include "v4_sum_impl.h"
#include <sys/bootdebug.h>

/* Checksum all architectures */
static int udp_cksum_flag = TRUE;	/* checksum on if set */

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * Initialize the udp-specific parts of a socket.
 */
void
udp_socket_init(struct inetboot_socket *isp)
{
	isp->type = INETBOOT_DGRAM;
	isp->proto = IPPROTO_UDP;
	isp->input[TRANSPORT_LVL] = udp_input;
	isp->output[TRANSPORT_LVL] = udp_output;
	isp->headerlen[TRANSPORT_LVL] = udp_header_len;
	isp->ports = udp_ports;
}

/*
 * Return the size of an UDP header
 */
int
udp_header_len(void)
{
	return (sizeof (struct udphdr));
}

/*
 * Return the requested port number in network order.
 */
in_port_t
udp_ports(uint16_t *udphp, enum Ports request)
{
	if (request == SOURCE)
		return (((struct udphdr *)udphp)->uh_sport);
	return (((struct udphdr *)udphp)->uh_dport);
}

/*
 * Process the IPv4 datagram that IPv4 has given us. We:
 *
 *	1) Checksum the datagram if checksum is turned on.
 *	2) Strip the udp header from the inetgram.
 *	3) Return the number of TRANSPORT frames for success, -1 if a
 *		failure occurred.
 *
 * Arguments: index: index into the open socket table, ip is the inetgram
 * we got from IPv4, which will contain the udp header and all the data.
 */
int
udp_input(int index)
{
	int frames = 0, header_len;
	struct inetgram	*igp, *ugp = NULL;
	struct udphdr	*udphp;

	while ((igp = sockets[index].inq) != NULL) {
		if (igp->igm_level != TRANSPORT_LVL) {
#ifdef	DEBUG
			printf("udp_input(%d): level %d datagram discarded.\n",
			    index, igp->igm_level);
#endif	/* DEBUG */
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}
		udphp = (struct udphdr *)igp->igm_bufp;
		header_len = (sockets[index].headerlen[TRANSPORT_LVL])();
		igp->igm_datap = igp->igm_bufp + header_len;

		/* generate checksum */
		if (udp_cksum_flag && udphp->uh_sum != 0) {
			if (udp_chksum(udphp, &igp->igm_saddr.sin_addr,
			    &igp->igm_target, sockets[index].proto) != 0) {
				dprintf("udp_input(%d): bad udp chksum "
				    "from %s.\n", index,
				    inet_ntoa(igp->igm_saddr.sin_addr));
				del_gram(&sockets[index].inq, igp, TRUE);
				continue;
			}
		}

		/* validate port number */
		if (sockets[index].bind.sin_port != udphp->uh_dport) {
			dprintf("udp_input(%d): Unexpected port number: "
			    "%d != %d from %s.\n", index,
			    ntohs(udphp->uh_dport), ntohs(
			    sockets[index].bind.sin_port),
			    inet_ntoa(igp->igm_saddr.sin_addr));
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}

		igp->igm_level = APP_LVL;
		del_gram(&sockets[index].inq, igp, FALSE);
		add_grams(&ugp, igp);
		frames++;
	}

	add_grams(&sockets[index].inq, ugp);

	return (frames);
}

/*
 * Create a UDP datagram given the data and sockaddr_in we got from sendto().
 * We will calculate the checksum if checksumming is turned on, and fill in
 * appropriate length and port fields. We convert the inetgram from a
 * block of data into a udp datagram... Returns the number of bytes contained
 * in the udp datagram (including header).
 *
 * Arguments: index: index into the open socket table, ogp is the inetgram
 * we got from sendto(), which will contain just the data and sockaddr_in.
 */
int
udp_output(int index, struct inetgram *ogp)
{
	int		i;
	struct udphdr	*udphp;
	int		offset;
	uint16_t	len;

#ifdef	DEBUG
	printf("udp_output(%d): 0x%x, %d\n", index, ogp->igm_bufp,
	    ogp->igm_len);
#endif	/* DEBUG */

	for (len = ogp->igm_len, i = MEDIA_LVL; i < TRANSPORT_LVL; i++)
		len -= (sockets[index].headerlen[i])();

	/* add udp header */
	for (i = MEDIA_LVL, offset = 0; i < TRANSPORT_LVL; i++)
		offset += (sockets[index].headerlen[i])();

	udphp = (struct udphdr *)(ogp->igm_bufp + offset);

	udphp->uh_dport = ogp->igm_saddr.sin_port;
	if (sockets[index].bound)
		udphp->uh_sport = sockets[index].bind.sin_port;
	else
		udphp->uh_sport = ogp->igm_saddr.sin_port;
	udphp->uh_ulen = htons(len);
	udphp->uh_sum = 0;

	if (udp_cksum_flag) {
		udphp->uh_sum = udp_chksum(udphp, &sockets[index].bind.sin_addr,
		    &ogp->igm_saddr.sin_addr, sockets[index].proto);
	}

	ogp->igm_level = NETWORK_LVL;

	return (0);
}
