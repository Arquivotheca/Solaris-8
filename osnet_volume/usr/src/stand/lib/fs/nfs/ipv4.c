/*
 * Copyright (C) 1991-1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * ipv4.c, Code implementing the IPv4 internet protocol.
 */

#pragma ident	"@(#)ipv4.c	1.4	99/04/15 SMI"

#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/if_arp.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/fcntl.h>
#include <sys/salib.h>
#include <netdb.h>
#include "socket_inet.h"
#include "icmp4.h"
#include "ipv4.h"
#include "mac.h"
#include "v4_sum_impl.h"
#include <sys/bootdebug.h>

static struct ip_frag	fragment[FRAG_MAX];	/* ip fragment buffers */
static int		fragments;		/* Number of fragments */
static uint8_t		ttl = MAXTTL;		/* IP ttl */
static struct in_addr	myip;			/* our network-order IP addr */
static struct in_addr	mynet;			/* net-order netaddr */
static struct in_addr	netmask =
	{ 0xff, 0xff, 0xff, 0xff };		/* our network-order netmask */
static struct in_addr	defaultrouter;		/* net-order defaultrouter */
static int		promiscuous;		/* promiscuous mode */
static char		hostname[MAXHOSTNAMELEN + 1] = {
	'U', 'N', 'K', 'N', 'O', 'W', 'N', '\0'
};
static struct routing table[IPV4_ROUTE_TABLE_SIZE];

#define	dprintf	if (boothowto & RB_DEBUG) printf

#ifdef	DEBUG
#define	FRAG_DEBUG
#endif	/* DEBUG */

#ifdef FRAG_DEBUG
/*
 * display the fragment list. For debugging purposes.
 */
static void
frag_disp(uint16_t size)
{
	int	i;
	int16_t	total = 0;

	printf("Dumping fragment info: (%d)\n\n", fragments);
	printf("More:\tOffset:\tDatap:\t\tDatal:\tIPid:\t\tIPlen:\tIPhlen:\n");
	for (i = 0; i < FRAG_MAX; i++) {
		printf("%d\t%d\t0x%x\t%d\t%d\t%d\t%d\n", fragment[i].more,
		    fragment[i].offset, fragment[i].datap, fragment[i].datal,
		    fragment[i].ipid, fragment[i].iplen, fragment[i].iphlen);
		total += (fragment[i].iplen - fragment[i].iphlen);
	}
	printf("Total length is: %d. It should be: %d\n\n", total, size);
}
#endif /* FRAG_DEBUG */

/*
 * This function returns index of fragment 0 of the current fragmented DGRAM
 * (which would contain the transport header). Return the fragment number
 * for success, -1 if we don't yet have the first fragment.
 */
static int
frag_first(void)
{
	int		i;

	if (fragments == 0)
		return (-1);

	for (i = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap != NULL && fragment[i].offset == 0)
			return (i);
	}
	return (-1);
}

/*
 * This function returns index of the last fragment of the current DGRAM.
 * Returns the fragment number for success, -1 if we don't yet have the
 * last fragment.
 */
static int
frag_last(void)
{
	int		i;

	if (fragments == 0)
		return (-1);

	for (i = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap != NULL && !fragment[i].more)
			return (i);
	}
	return (-1);
}

/*
 * This function adds a fragment to the current pkt fragment list. Returns
 * FRAG_NOSLOTS if there are no more slots, FRAG_DUP if the fragment is
 * a duplicate, or FRAG_SUCCESS if it is successful.
 */
static int
frag_add(int16_t offset, caddr_t datap, int16_t datal, uint16_t ipid,
    int16_t iplen, int16_t iphlen)
{
	int	i;
	int16_t	true_offset = IPV4_OFFSET(offset);

	/* first pass - look for duplicates */
	for (i = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap != NULL &&
		    fragment[i].offset == true_offset)
			return (FRAG_DUP);
	}

	/* second pass - fill in empty slot */
	for (i = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap == NULL) {
			fragment[i].more = (offset & IP_MF);
			fragment[i].offset = true_offset;
			fragment[i].datap = datap;
			fragment[i].datal = datal;
			fragment[i].ipid = ipid;
			fragment[i].iplen = iplen;
			fragment[i].iphlen = iphlen;
			fragments++;
			return (FRAG_SUCCESS);
		}
	}
	return (FRAG_NOSLOTS);
}

/*
 * Nuke a fragment.
 */
static void
frag_free(int index)
{
	if (fragment[index].datap != NULL) {
		bkmem_free(fragment[index].datap, fragment[index].datal);
		fragments--;
	}
	bzero((caddr_t)&fragment[index], sizeof (struct ip_frag));
}

/*
 * zero the frag list.
 */
static void
frag_flush(void)
{
	int i;

	for (i = 0; i < FRAG_MAX; i++)
		frag_free(i);

	fragments = 0;
}

/*
 * Analyze the fragment list - see if we captured all our fragments.
 *
 * Returns TRUE if we've got all the fragments, and FALSE if we don't.
 */
static int
frag_chk(void)
{
	int		i, first_frag, last_frag;
	int16_t		actual, total;
	uint16_t	ip_id;

	if (fragments == 0 || (first_frag = frag_first()) < 0 ||
	    (last_frag = frag_last()) < 0)
		return (FALSE);

	/*
	 * Validate the ipid's of our fragments - nuke those that don't
	 * match the id of the first fragment.
	 */
	ip_id = fragment[first_frag].ipid;
	for (i = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap != NULL && ip_id != fragment[i].ipid) {
#ifdef FRAG_DEBUG
			printf("ipv4: Frag id mismatch: %x != %x\n",
			    fragment[i].ipid, ip_id);
#endif /* FRAG_DEBUG */
			frag_free(i);
		}
	}

	if (frag_last() < 0)
		return (FALSE);

	total = fragment[last_frag].offset + fragment[last_frag].iplen -
	    fragment[last_frag].iphlen;

	for (i = 0, actual = 0; i < FRAG_MAX; i++)
		actual += (fragment[i].iplen - fragment[i].iphlen);

#ifdef FRAG_DEBUG
	frag_disp(total);
#endif /* FRAG_DEBUG */

	return (total == actual);
}

/*
 * Load the assembled fragments into igp. Returns 0 for success, nonzero
 * otherwise.
 */
static int
frag_load(struct inetgram *igp)
{
	int	i;
	int16_t	len, total_len;

	if (fragments == 0)
		return (ENOENT);

	for (i = 0, len = 0, total_len = 0; i < FRAG_MAX; i++) {
		if (fragment[i].datap != 0) {
			/* Copy just the data (omit the ip header) */
			len = fragment[i].iplen - fragment[i].iphlen;
			total_len += len;
			if (total_len > igp->igm_len)
				return (E2BIG);
			bcopy((caddr_t)(fragment[i].datap + fragment[i].iphlen),
			    (caddr_t)(igp->igm_bufp + fragment[i].offset), len);
		}
	}
	return (0);
}

/*
 * Locate a routing table entry based upon arguments. IP addresses expected
 * in network order. Returns index for success, -1 if entry not found.
 */
static int
find_route(uint8_t *flagp, struct in_addr *destp, struct in_addr *gatewayp)
{
	int i, table_entry = -1;

	for (i = 0; table_entry == -1 && i < IPV4_ROUTE_TABLE_SIZE; i++) {
		if (flagp != NULL) {
			if (*flagp & table[i].flag)
				table_entry = i;
		}
		if (destp != NULL) {
			if (destp->s_addr == table[i].dest.s_addr)
				table_entry = i;
			else
				table_entry = -1;
		}
		if (gatewayp != NULL) {
			if (gatewayp->s_addr == table[i].gateway.s_addr)
				table_entry = i;
			else
				table_entry = -1;
		}
	}
	return (table_entry);
}

/*
 * ADD or DEL a routing table entry. Returns 0 for success, -1 and errno
 * otherwise. IP addresses are expected in network order.
 */
int
ipv4_route(int cmd, uint8_t flag, struct in_addr *destp,
    struct in_addr *gatewayp)
{
	static	int	routing_table_initialized;
	int		index;
	uint8_t 	tmp_flag;

	if (gatewayp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* initialize routing table */
	if (routing_table_initialized == 0) {
		for (index = 0; index < IPV4_ROUTE_TABLE_SIZE; index++)
			table[index].flag = RT_UNUSED;
		routing_table_initialized = 1;
	}

	switch (cmd) {
	case IPV4_ADD_ROUTE:
		tmp_flag = (uint8_t)RT_UNUSED;
		if ((index = find_route(&tmp_flag, NULL, NULL)) == -1) {
			dprintf("ipv4_route: routing table full.\n");
			errno = ENOSPC;
			return (-1);
		}
		table[index].flag = flag;
		if (destp != NULL)
			table[index].dest.s_addr = destp->s_addr;
		else
			table[index].dest.s_addr = htonl(INADDR_ANY);
		table[index].gateway.s_addr = gatewayp->s_addr;
		break;
	case IPV4_BAD_ROUTE:
		/* FALLTHRU */
	case IPV4_DEL_ROUTE:
		if ((index = find_route(&flag, destp, gatewayp)) == -1) {
			dprintf("ipv4_route: No such routing entry.\n");
			errno = ENOENT;
			return (-1);
		}
		if (cmd == IPV4_DEL_ROUTE) {
			table[index].flag = RT_UNUSED;
			table[index].dest.s_addr = htonl(INADDR_ANY);
			table[index].gateway.s_addr = htonl(INADDR_ANY);
		} else
			table[index].flag = RT_NG;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

/*
 * Return gateway to destination. Returns gateway IP address in network order
 * for success, NULL if no route to destination exists.
 */
struct in_addr *
ipv4_get_route(uint8_t flag, struct in_addr *destp, struct in_addr *gatewayp)
{
	int index;
	if ((index = find_route(&flag, destp, gatewayp)) == -1)
		return (NULL);
	return (&table[index].gateway);
}

/*
 * Initialize the IPv4 generic parts of the socket, as well as the routing
 * table.
 */
void
ipv4_socket_init(struct inetboot_socket *isp)
{
	isp->input[NETWORK_LVL] = ipv4_input;
	isp->output[NETWORK_LVL] = ipv4_output;
	isp->headerlen[NETWORK_LVL] = ipv4_header_len;
}

/*
 * Initialize a raw ipv4 socket.
 */
void
ipv4_raw_socket(struct inetboot_socket *isp, uint8_t proto)
{
	isp->type = INETBOOT_RAW;
	if (proto == 0)
		isp->proto = IPPROTO_IP;
	else
		isp->proto = proto;
	isp->input[TRANSPORT_LVL] = NULL;
	isp->output[TRANSPORT_LVL] = NULL;
	isp->headerlen[TRANSPORT_LVL] = NULL;
	isp->ports = NULL;
}

/*
 * Return the size of an IPv4 header (no options)
 */
int
ipv4_header_len(void)
{
	return (sizeof (struct ip));
}

/*
 * Set our source address.
 * Argument is assumed to be host order.
 */
void
ipv4_setipaddr(struct in_addr *ip)
{
	myip.s_addr = htonl(ip->s_addr);
}

/*
 * Returns our current source address in host order.
 */
void
ipv4_getipaddr(struct in_addr *ip)
{
	ip->s_addr = ntohl(myip.s_addr);
}

/*
 * Set our netmask.
 * Argument is assumed to be host order.
 */
void
ipv4_setnetmask(struct in_addr *ip)
{
	netmask.s_addr = htonl(ip->s_addr);
	mynet.s_addr = netmask.s_addr & myip.s_addr; /* implicit */
}

/*
 * Returns our current netmask in host order.
 */
void
ipv4_getnetmask(struct in_addr *ip)
{
	ip->s_addr = ntohl(netmask.s_addr);
}

/*
 * Set our default router.
 * Argument is assumed to be host order, and *MUST* be on the same network
 * as our source IP address.
 */
void
ipv4_setdefaultrouter(struct in_addr *ip)
{
	defaultrouter.s_addr = htonl(ip->s_addr);
}

/*
 * Returns our current default router in host order.
 */
void
ipv4_getdefaultrouter(struct in_addr *ip)
{
	ip->s_addr = ntohl(defaultrouter.s_addr);
}

/*
 * Toggle promiscuous flag. If set, client disregards destination IP
 * address. Otherwise, only limited broadcast, network broadcast, and
 * unicast traffic get through. Returns previous setting.
 */
int
ipv4_setpromiscuous(int toggle)
{
	int old = promiscuous;

	promiscuous = toggle;

	return (old);
}

/*
 * Set IP TTL.
 */
void
ipv4_setmaxttl(uint8_t cttl)
{
	ttl = cttl;
}

/*
 * Get system hostname
 */
int
gethostname(caddr_t ret, int max)
{
	int len;
	if (max > MAXHOSTNAMELEN)
		len = MAXHOSTNAMELEN;
	else
		len = max;
	bcopy(hostname, ret, len);
	ret[len] = '\0';
	return (0);
}

/*
 * Set system hostname
 */
int
sethostname(caddr_t hnp, int len)
{
	if (len > MAXHOSTNAMELEN)
		len = MAXHOSTNAMELEN;
	bcopy(hnp, hostname, len);
	hostname[len] = '\0';
	return (0);
}

/*
 * Convert an ipv4 address to dotted notation.
 * Returns ptr to statically allocated buffer containing dotted string.
 */
char *
inet_ntoa(struct in_addr ip)
{
	uint8_t *p;
	static char ipaddr[16];

	p = (uint8_t *)&ip.s_addr;
	(void) sprintf(ipaddr, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
	return (ipaddr);
}

/*
 * Construct a transport datagram from a series of IP fragments (igp == NULL)
 * or from a single IP datagram (igp != NULL). Return the address of the
 * contructed transport datagram.
 */
struct inetgram *
make_trans_datagram(int index, struct inetgram *igp, struct in_addr ipsrc,
    struct in_addr ipdst, uint16_t iphlen)
{
	uint16_t	trans_len, *transp;
	int		first_frag, last_frag, fragmented;
	struct inetgram	*ngp;

	if (igp == NULL)
		fragmented = TRUE;
	else
		fragmented = FALSE;

	ngp = (struct inetgram *)bkmem_zalloc(sizeof (struct inetgram));
	if (ngp == NULL) {
		errno = ENOMEM;
		if (fragmented)
			frag_flush();
		return (NULL);
	}

	if (fragmented) {
		last_frag = frag_last();
		trans_len = fragment[last_frag].offset +
		    fragment[last_frag].iplen - fragment[last_frag].iphlen;
		first_frag = frag_first();
		transp = (uint16_t *)(fragment[first_frag].datap +
		    fragment[first_frag].iphlen);
	} else {
		trans_len = igp->igm_len - iphlen;
		transp = (uint16_t *)(igp->igm_bufp + iphlen);
	}

	ngp->igm_saddr.sin_addr.s_addr = ipsrc.s_addr;
	ngp->igm_saddr.sin_port = sockets[index].ports(transp, SOURCE);
	ngp->igm_target.s_addr = ipdst.s_addr;
	ngp->igm_level = TRANSPORT_LVL;

	/* Align to 16bit value */
	ngp->igm_len = ((trans_len + sizeof (int16_t) - 1) &
	    ~(sizeof (int16_t) - 1));
	if ((ngp->igm_bufp = bkmem_zalloc(ngp->igm_len)) == NULL) {
		errno = ENOMEM;
		bkmem_free((caddr_t)ngp, sizeof (struct inetgram));
		if (fragmented)
			frag_flush();
		return (NULL);
	}

	if (fragmented) {
		if (frag_load(ngp) != 0) {
			bkmem_free((caddr_t)ngp, sizeof (struct inetgram));
			frag_flush();
			return (NULL);
		}
		frag_flush();
	} else {
		ngp->igm_datap = ngp->igm_bufp;
		bcopy((caddr_t)(igp->igm_bufp + iphlen), ngp->igm_bufp,
		    trans_len);
	}
	return (ngp);
}

/*
 * ipv4_input: Pull in IPv4 datagrams addressed to us. Handle IP fragmentation
 * (fragments received in any order) and ICMP at this level.
 *
 * Note that because our network is serviced by polling when we expect
 * something (upon a referenced socket), we don't go through the work of
 * locating the appropriate socket a datagram is destined for. We'll only
 * accept data for the referenced socket. This means we don't have
 * asynchronous networking, but since we can't service the net using an
 * interrupt handler, it doesn't do us any good to try to service datagrams
 * destined for sockets other than the referenced one. Data is handled in
 * a fifo manner.
 *
 * The mac layer will grab all frames for us. If we find we don't have all
 * the necessary fragments to reassemble the datagram, we'll call the mac
 * layer again for FRAG_ATTEMPTS to see if it has any more frames.
 *
 * Supported protocols: IPPROTO_IP, IPPROTO_ICMP, IPPROTO_UDP.
 *
 * Returns: number of NETWORK_LVL datagrams placed on socket , -1 if error
 * occurred.
 *
 * Note: errno is set to ETIMEDOUT if fragment reassembly fails.
 */
int
ipv4_input(int index)
{
	int			datagrams = 0;
	int			frag_stat, input_attempts = 0;
	uint16_t		iphlen, iplen, ip_id;
	int16_t			curr_off;
	struct ip		*iphp;
	struct inetgram		*igp, *newgp = NULL, *ipv4_listp = NULL;
	struct in_addr		ipdst, ipsrc;

#ifdef	DEBUG
	printf("ipv4_input(%d): start ######################################\n",
	    index);
#endif	/* DEBUG */

	frag_flush();

ipv4_try_again:

	while ((igp = sockets[index].inq) != NULL) {
		if (igp->igm_level != NETWORK_LVL) {
#ifdef	DEBUG
			printf("ipv4_input(%d): unexpected frame type: %d\n",
			    index, igp->igm_level);
#endif	/* DEBUG */
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}
		iphp = (struct ip *)igp->igm_bufp;
		if (iphp->ip_v != IPVERSION) {
			dprintf("ipv4_input(%d): IPv%d datagram discarded\n",
			index, iphp->ip_v);
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}
		iphlen = iphp->ip_hl << 2;
		if (iphlen < sizeof (struct ip)) {
			dprintf("ipv4_input(%d): IP msg too short (%d < %d)\n",
			    index, iphlen, sizeof (struct ip));
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}
		iplen = ntohs(iphp->ip_len);
		if (iplen > igp->igm_len) {
			dprintf("ipv4_input(%d): IP len/buffer mismatch "
			    "(%d > %d)\n", index, iplen, igp->igm_len);
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}

		bcopy((caddr_t)&(iphp->ip_dst), (caddr_t)&ipdst,
		    sizeof (ipdst));
		bcopy((caddr_t)&(iphp->ip_src), (caddr_t)&ipsrc,
		    sizeof (ipsrc));

		/* LINTED [igp->igm_bufp is guaranteed to be 64 bit aligned] */
		if (ipv4cksum((uint16_t *)igp->igm_bufp, iphlen) != 0) {
			dprintf("ipv4_input(%d): Bad IP header checksum "
			    "(to %s)\n", index, inet_ntoa(ipdst));
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}

		if (!promiscuous) {
			/* validate destination address */
			if (ipdst.s_addr != htonl(INADDR_BROADCAST) &&
			    ipdst.s_addr != (mynet.s_addr | ~netmask.s_addr) &&
			    ipdst.s_addr != myip.s_addr) {
#ifdef	DEBUG
				printf("ipv4_input(%d): msg to %s discarded.\n",
				    index, inet_ntoa(ipdst));
#endif	/* DEBUG */
				/* not ours */
				del_gram(&sockets[index].inq, igp, TRUE);
				continue;
			}
		}

		/* Intercept ICMP first */
		if (!promiscuous && (iphp->ip_p == IPPROTO_ICMP)) {
			icmp4(igp, iphp, iphlen, ipsrc);
			del_gram(&sockets[index].inq, igp, TRUE);
			continue;
		}

#ifdef	DEBUG
		printf("ipv4_input(%d): processing ID: 0x%x (0x%x) (0x%x,%d)\n",
		    index, ntohs(iphp->ip_id), igp, igp->igm_bufp,
		    igp->igm_len);
#endif	/* DEBUG */
		switch (sockets[index].type) {
		case INETBOOT_DGRAM:
			if (iphp->ip_p != IPPROTO_UDP) {
				/* Wrong protocol. */
				dprintf("ipv4_input(%d): unexpected protocol: "
				    "%d != %d\n", index, iphp->ip_p,
				    IPPROTO_UDP);
				del_gram(&sockets[index].inq, igp, TRUE);
				continue;
			}

			/*
			 * Once we process the first fragment, we won't have
			 * the transport header, so we'll have to  match on
			 * IP id.
			 */
			curr_off = ntohs(iphp->ip_off);
			if ((curr_off & ~(IP_DF | IP_MF)) == 0) {
				uint16_t	*transp;
				/* Validate transport header. */
				if ((igp->igm_len - iphlen) <
				    sockets[index].headerlen[TRANSPORT_LVL]()) {
					dprintf("ipv4_input(%d): datagram 0 "
					"too small to hold transport header "
					"(from %s)\n", index, inet_ntoa(ipsrc));
					del_gram(&sockets[index].inq, igp,
					    TRUE);
					continue;
				}
				/*
				 * check alignment - transport elements are 16
				 * bit aligned..
				 */
				transp = (uint16_t *)(igp->igm_bufp + iphlen);
				if ((uintptr_t)transp % sizeof (uint16_t)) {
					dprintf("ipv4_input(%d): Transport "
					    "header is not 16bit aligned "
					    "(0x%x, from %s)\n", index, transp,
					    inet_ntoa(ipsrc));
					del_gram(&sockets[index].inq, igp,
					    TRUE);
					continue;
				}
				if (curr_off & IP_MF) {
					/* fragment 0 of fragmented datagram */
					ip_id = ntohs(iphp->ip_id);
					frag_stat = frag_add(curr_off,
					    igp->igm_bufp, igp->igm_len, ip_id,
					    iplen, iphlen);
					if (frag_stat != FRAG_SUCCESS) {
#ifdef	FRAG_DEBUG
						if (frag_stat == FRAG_DUP) {
							printf("ipv4_input"
							    "(%d): Frag dup.\n",
							    index);
						} else {
							printf("ipv4_input"
							    "(%d): too many "
							    "frags\n", index);
						}
#endif	/* FRAG_DEBUG */
						del_gram(&sockets[index].inq,
						igp, TRUE);
						continue;
					}

					del_gram(&sockets[index].inq, igp,
					    FALSE);
					/* keep the data, lose the inetgram */
					bkmem_free((caddr_t)igp,
					    sizeof (struct inetgram));
#ifdef	FRAG_DEBUG
					printf("ipv4_input(%d): Frag/Off/Id "
					    "(%d/%d/%x)\n", index, fragments,
					    IPV4_OFFSET(curr_off), ip_id);
#endif	/* FRAG_DEBUG */
				} else {
					/* Single, unfragmented datagram */
					newgp = make_trans_datagram(index, igp,
					    ipsrc, ipdst, iphlen);
					if (newgp != NULL) {
						add_grams(&ipv4_listp, newgp);
						datagrams++;
					}
					del_gram(&sockets[index].inq, igp,
					    TRUE);
					continue;
				}
			} else {
				/* fragments other than 0 */
				frag_stat = frag_add(curr_off, igp->igm_bufp,
				    igp->igm_len, ntohs(iphp->ip_id), iplen,
				    iphlen);

				if (frag_stat == FRAG_SUCCESS) {
#ifdef	FRAG_DEBUG
					printf("ipv4_input(%d): Frag(%d) "
					    "off(%d) id(%x)\n", index,
					    fragments, IPV4_OFFSET(curr_off),
					    ntohs(iphp->ip_id));
#endif	/* FRAG_DEBUG */
					del_gram(&sockets[index].inq, igp,
					    FALSE);
					/* keep the data, lose the inetgram */
					bkmem_free((caddr_t)igp,
					    sizeof (struct inetgram));
				} else {
#ifdef	FRAG_DEBUG
					if (frag_stat == FRAG_DUP)
						printf("ipv4_input(%d): Frag "
						    "dup.\n", index);
					else {
						printf("ipv4_input(%d): too "
						    "many frags\n", index);
					}
#endif	/* FRAG_DEBUG */
					del_gram(&sockets[index].inq, igp,
					    TRUE);
					continue;
				}
			}

			/*
			 * Determine if we have all of the fragments.
			 *
			 * NOTE: at this point, we've placed the data in the
			 * fragment table, and the inetgram (igp) has been
			 * deleted.
			 */
			if (!frag_chk())
				continue;

			newgp = make_trans_datagram(index, NULL, ipsrc, ipdst,
			    iphlen);
			if (newgp == NULL)
				continue;
			add_grams(&ipv4_listp, newgp);
			datagrams++;
			break;
		case INETBOOT_RAW:
			/* No fragmentation - Just the raw packet. */
#ifdef	DEBUG
			printf("ipv4_input(%d): Raw packet.\n", index);
#endif	/* DEBUG */
			del_gram(&sockets[index].inq, igp, FALSE);
			add_grams(&ipv4_listp, igp);
			igp->igm_datap = (caddr_t)(igp->igm_bufp + iphlen);
			datagrams++;
			break;
		}
	}
	if (ipv4_listp == NULL && fragments != 0) {
		if (++input_attempts > FRAG_ATTEMPTS) {
			dprintf("ipv4_input(%d): reassembly(%d) timed out in "
			    "%d msecs.\n", index, fragments,
			    sockets[index].in_timeout * input_attempts);
			frag_flush();
			errno = ETIMEDOUT;
			return (-1);
		} else {
			/*
			 * Call the media layer again... there may be more
			 * packets waiting.
			 */
			if (sockets[index].input[MEDIA_LVL](index) < 0) {
				/* errno will be set appropriately */
				frag_flush();
				return (-1);
			}
			goto ipv4_try_again;
		}
	}

	add_grams(&sockets[index].inq, ipv4_listp);

	return (datagrams);
}

/*
 * ipv4_output: Generate IPv4 datagram(s) for the payload and deliver them.
 * Routing is handled here as well, by reusing the saddr field to hold the
 * router's IP address.
 *
 * We don't deal with fragmentation on the outgoing side.
 *
 * Arguments: index to socket, inetgram to send.
 *
 * Returns: 0 for success, -1 if error occurred.
 */
int
ipv4_output(int index, struct inetgram *ogp)
{
	static uint16_t	ip_id;
	struct ip	*iphp;
	int		offset;
	uint64_t	iphbuffer[sizeof (struct ip)];

#ifdef	DEBUG
	printf("ipv4_output(%d): 0x%x, %d\n", index, ogp->igm_bufp,
	    ogp->igm_len);
#endif	/* DEBUG */

	/* we don't deal (yet) with fragmentation. Maybe never will */
	if (ogp->igm_len > mac_state.mac_mtu) {
		dprintf("ipv4: datagram too big for MAC layer.\n");
		errno = E2BIG;
		return (-1);
	}

	if (ogp->igm_level != NETWORK_LVL) {
#ifdef	DEBUG
		printf("ipv4_output(%d): unexpected frame type: %d\n", index,
		    ogp->igm_level);
#endif	/* DEBUG */
		errno = EINVAL;
		return (-1);
	}

	if (sockets[index].out_flags & SO_DONTROUTE)
		ogp->igm_oflags |= MSG_DONTROUTE;

	offset = sockets[index].headerlen[MEDIA_LVL]();

	iphp = (struct ip *)&iphbuffer;
	iphp->ip_v = IPVERSION;
	iphp->ip_hl = sizeof (struct ip) / 4;
	iphp->ip_tos = 0;
	iphp->ip_len = htons(ogp->igm_len - offset);
	iphp->ip_id = htons(++ip_id);
	iphp->ip_off = htons(IP_DF);
	iphp->ip_p = sockets[index].proto;
	iphp->ip_sum = htons(0);
	iphp->ip_ttl = ttl;

	/* struct copies */
	iphp->ip_src = myip;
	iphp->ip_dst = ogp->igm_saddr.sin_addr;

	/*
	 * On local / limited broadcasts, don't route. From a purist's
	 * perspective, we should be setting the TTL to 1. But
	 * operational experience has shown that some BOOTP relay agents
	 * (ciscos) discard our packets. Furthermore, these devices also
	 * *don't* reset the TTL to MAXTTL on the unicast side of the
	 * BOOTP relay agent! Sigh. Thus to work correctly in these
	 * environments, we leave the TTL as it has been been set by
	 * the application layer, and simply don't check for a route.
	 */
	if (iphp->ip_dst.s_addr == htonl(INADDR_BROADCAST) ||
	    (netmask.s_addr != htonl(INADDR_BROADCAST) &&
	    iphp->ip_dst.s_addr == (mynet.s_addr | ~netmask.s_addr))) {
		ogp->igm_oflags |= MSG_DONTROUTE;
	}

	/* Routing necessary? */
	if ((ogp->igm_oflags & MSG_DONTROUTE) == 0 &&
	    ((iphp->ip_dst.s_addr & netmask.s_addr) != mynet.s_addr)) {
		struct in_addr *rip;
		if ((rip = ipv4_get_route(RT_HOST, &iphp->ip_dst,
		    NULL)) == NULL) {
			rip = ipv4_get_route(RT_DEFAULT, NULL, NULL);
		}
		if (rip == NULL) {
			dprintf("ipv4(%d): No route to %s.\n",
			    index, inet_ntoa(iphp->ip_dst));
			errno = EHOSTUNREACH;
			return (-1);
		}
		ogp->igm_router.s_addr = rip->s_addr;
	} else
		ogp->igm_router.s_addr = htonl(INADDR_ANY);

	iphp->ip_sum = ipv4cksum((uint16_t *)iphp, sizeof (struct ip));
	bcopy((caddr_t)iphp, (caddr_t)(ogp->igm_bufp + offset),
	    sizeof (struct ip));

	ogp->igm_level = MEDIA_LVL;

	return (0);
}
