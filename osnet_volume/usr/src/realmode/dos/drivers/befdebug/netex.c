/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)netex.c	1.5	97/05/09 SMI"
 */

#include "befdebug.h"
#include "netex.h"

/* My addresses.  Set once at start */
static unchar my_ether[ETHERADDRL];
static unchar my_ipaddr[IPADDRL];
static unchar my_ipaddr_known;

/* My boot server addresses */
static unchar bs_ether[ETHERADDRL];
static unchar bs_ipaddr[IPADDRL];

#define	MAX_NAME_SIZE	256
static unchar my_name[MAX_NAME_SIZE];
static unchar my_domain[MAX_NAME_SIZE];

static unchar etherbroadcastaddr[ETHERADDRL] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
static unchar ipbroadcastaddr[IPADDRL] = {
	0xFF, 0xFF, 0xFF, 0xFF
};

static ushort packets_received;
static unchar receive_buffer[ETHERMTU];

#define	SEND_INTERVAL	10

static unsigned short ip_cksum(char *, unsigned int);
static void other_packet(ushort);
static int revarp_check_response(ushort);
static void revarp_no_response(ushort);
static int send_recv(unchar *, ushort, int (*)(ushort), void (*)(ushort));
static int send_revarp(void);
static int send_whoami(void);
static ushort swap_two_bytes(ushort);
static ulong swap_four_bytes(ulong);
static ushort udp_checksum(char *, struct pseudo_udp *);
static int whoami_check_response(ushort);
static void whoami_no_response(ushort);

void
net_exercise(int dev, struct bdev_info far *info, char far *driver_name)
{
	unchar func;
	
	net_addr(my_ether);
	printf("Network address for device %x is "
			"%0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x.\n",
			dev, my_ether[0], my_ether[1], my_ether[2],
			my_ether[3], my_ether[4], my_ether[5]);
	net_open();

	printf("Trying to determine the IP address for "
			"%0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x ... ",
			my_ether[0], my_ether[1], my_ether[2],
			my_ether[3], my_ether[4], my_ether[5]);

	if (send_revarp() == 0) {
		net_close();
		return;
	}

	printf("\nIP address is %d.%d.%d.%d.\n",
			my_ipaddr[0], my_ipaddr[1],
			my_ipaddr[2], my_ipaddr[3]);

	printf("Trying find the boot server for %d.%d.%d.%d ... ",
			my_ipaddr[0], my_ipaddr[1],
			my_ipaddr[2], my_ipaddr[3]);

	if (send_whoami() == 0) {
		net_close();
		return;
	}

	printf("\nThe boot server is %d.%d.%d.%d ("
			"%0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x).\n",
			bs_ipaddr[0], bs_ipaddr[1],
			bs_ipaddr[2], bs_ipaddr[3],
			bs_ether[0], bs_ether[1], bs_ether[2],
			bs_ether[3], bs_ether[4], bs_ether[5]);
	printf("Device %x is \"%s\" in domain \"%s\".\n",
		dev, my_name, my_domain);

	net_close();
}

static int
send_whoami(void)
{
	struct whoami_packet packet;
	struct pseudo_udp ck;
	struct ether_header eh;
	unchar		dest_ipaddr[IPADDRL];
	unchar		dest_ether[ETHERADDRL];
	register int	i;

	/*
	 * The normal mechanism for building and sending the whoami
	 * packet is extremely general and complex.  In the following
	 * code we simplify it by coding directly to the actual packet
	 * structure that goes over the net, rather than dealing
	 * with high-level translation.
	 */

	/* We need to broadcast the packet */
	dos_memcpy(dest_ipaddr, ipbroadcastaddr, IPADDRL);
	dos_memcpy(dest_ether, etherbroadcastaddr, ETHERADDRL);

	/* Fill the bootparams portion of the packet */
	packet.bp.client_address.address_type = htonl(IP_ADDR_TYPE);
	for (i = 0; i < IPADDRL; i++) {
		packet.bp.client_address.ip_addr[i] =
			htonl((long)(char)my_ipaddr[i]);
	}

	/* Fill in the portmapper portion of the packet */
	packet.pmap.program = htonl(BOOTPARAMPROG);
	packet.pmap.version = htonl(1);
	packet.pmap.proc = htonl(1);
	packet.pmap.len = htonl(sizeof (struct bp_whoami_arg));

	/* Fill in the Sun RPC header portion of the packet */
	packet.rpch.transaction = htonl(3);	/* Semi-arbitrary value */
	packet.rpch.call_type = htonl(0);	/* CALL */
	packet.rpch.rpc_version = htonl(2);
	packet.rpch.program = htonl(PMAPPROG);
	packet.rpch.version = htonl(2);
	packet.rpch.procedure = htonl(5);
	packet.rpch.cred_flavor = htonl(0);
	packet.rpch.cred_len = htonl(0);
	packet.rpch.verifier_flavor = htonl(0);
	packet.rpch.verifier_len = htonl(0);

	/* Fill in the UDP header portion of the packet  */
	packet.udph.uh_sport = htons(WHOAMI_PORT);
	packet.udph.uh_dport = htons(PMAPPORT);
	packet.udph.uh_ulen = htons(sizeof (packet.udph) +
		sizeof (packet.rpch) + sizeof (packet.pmap) +
		sizeof (packet.bp));
	packet.udph.uh_sum = 0;

	/* Calculate the UDP checksum.  Requires building a pseudo-header */
	dos_memcpy(&ck.src, my_ipaddr, IPADDRL);
	dos_memcpy(&ck.dst, dest_ipaddr, IPADDRL);
	ck.notused = 0;
	ck.hdr = packet.udph;
	ck.proto = IPPROTO_UDP;
	ck.len = packet.udph.uh_ulen;
	packet.udph.uh_sum = udp_checksum((char *)&packet.rpch, &ck);
	if (packet.udph.uh_sum == 0) {
		packet.udph.uh_sum = 0xffff;
	}

	/* Fill in the IP header portion of the packet */
	packet.iph.ip_len_ver = 0x45;	/* Version 4, length = 5 longs */
	packet.iph.ip_tos = 0;
	packet.iph.ip_len = htons(ntohs(packet.udph.uh_ulen) +
		sizeof (packet.iph));
	packet.iph.ip_id = 0;
	packet.iph.ip_off = 0;
	packet.iph.ip_ttl = 0xFF;
	packet.iph.ip_p = IPPROTO_UDP;
	packet.iph.ip_sum = 0;
	dos_memcpy(&packet.iph.ip_src, my_ipaddr, IPADDRL);
	dos_memcpy(&packet.iph.ip_dst, dest_ipaddr, IPADDRL);
	packet.iph.ip_sum = ip_cksum((char *)&packet.iph, sizeof (struct ip));

	/* Fill in the ethernet header portion of the packet */
	dos_memcpy(&eh.ether_dhost, dest_ether, ETHERADDRL);
	dos_memcpy(&eh.ether_shost, my_ether, ETHERADDRL);
	eh.ether_type = htons(ETHERTYPE_IP);
	dos_memcpy(packet.eh, (char *)&eh, sizeof (eh));

	return (send_recv((unchar *)&packet.eh,
		sizeof (packet) - sizeof (packet.fill),
		whoami_check_response, whoami_no_response));
}

static int
whoami_check_response(ushort len)
{
#define	BEYOND(x, y)	(((unchar *)(x)) + (y))
	struct ether_header *eh = (struct ether_header *)receive_buffer;
	struct ip *iph;
	struct udphdr *udph;
	struct rpc_hr *rpch;
	struct pmap *pmap;
	ushort sys_size;
	ushort dom_size;
	unchar *sys_start;
	unchar *dom_start;
	ushort i;
	unchar *p;
	unchar *d;
	unchar *end = receive_buffer + len;

	UDprintf(("Received a %d-byte packet: ", len));

	if (len < sizeof (struct ether_header)) {
		UDprintf(("too short for an ethernet packet.\n"));
		return (0);
	}

	if (dos_memcmp(&eh->ether_dhost, my_ether, ETHERADDRL) != 0) {
		if (dos_memcmp(&eh->ether_dhost, etherbroadcastaddr,
				ETHERADDRL) == 0)
			UDprintf(("broadcast packet (ignored).\n"));
		else
			UDprintf(("not addressed to me.\n"));
		return (0);
	}

	/*
	 * The supposed packet was long enough to have a valid ethernet
	 * header and seemed to be addressed to me.  Keep a count of
	 * such packets to help with diagnosis.
	 */
	packets_received++;

	if (eh->ether_type != htons(ETHERTYPE_IP)) { 
		UDprintf(("not an IP packet.\n"));
		return (0);
	}

	iph = (struct ip *)BEYOND(eh, sizeof (struct ether_header));
	if (BEYOND(iph, sizeof (struct ip)) > end) {
		UDprintf(("too short to contain IP header.\n"));
		return (0);
	}
	if (iph->ip_p != IPPROTO_UDP) {
		UDprintf(("not UDP protocol.\n"));
		return (0);
	}

	udph = (struct udphdr *)BEYOND(iph,
		(iph->ip_len_ver & 0xF) * sizeof (ulong));
	if (BEYOND(udph, sizeof (struct udphdr)) > end) {
		UDprintf(("too short to contain UDP header.\n"));
		return (0);
	}

	rpch = (struct rpc_hr *)BEYOND(udph, sizeof (struct udphdr));
	if (BEYOND(rpch, 16) > end) {
		UDprintf(("too short to contain RPC reply header.\n"));
		return (0);
	}
	if (rpch->call_type != htonl(1)) {
		UDprintf(("RPC header is not for a reply.\n"));
		return (0);
	}

	pmap = (struct pmap *)BEYOND(rpch, 16);
	if (BEYOND(pmap, sizeof (struct pmap)) > end) {
		UDprintf(("too short to contain PMAP header.\n"));
		return (0);
	}

	p = BEYOND(pmap, sizeof (struct pmap));
	if (p + ntohl(pmap->len) > end) {
		UDprintf(("too short to contain reported data.\n"));
		return (0);
	}
	end = p + ntohl(pmap->len);

	/* Find my system name */
	sys_size = ntohl(*(ulong *)p);
	p += sizeof (ulong);
	if (p + sys_size > end) {
		UDprintf(("error in system name length.\n"));
		return (0);
	}
	sys_start = p;
	p += ((sys_size + 3) & ~3);

	/* Find my domain name */
	dom_size = ntohl(*(ulong *)p);
	p += sizeof (ulong);
	if (p + dom_size > end) {
		UDprintf(("error in domain name length.\n"));
		return (0);
	}
	dom_start = p;

	UDprintf(("the response to my \"whoami\" request%s.\n",
		(bd.misc_flags & IGNORE_WHOAMI) ? " (ignored)" : ""));

	/* IGNORE_WHOAMI is set by the -zw option */
	if (bd.misc_flags & IGNORE_WHOAMI) {
		packets_received--;
		return (0);
	}

	/* Save the names and the boot server addresses */
	if (sys_size < MAX_NAME_SIZE) {
		dos_memcpy(my_name, sys_start, sys_size);
		my_name[sys_size] = 0;
	} else {
		dos_memcpy(my_name, sys_start, MAX_NAME_SIZE);
		my_name[MAX_NAME_SIZE - 1] = 0;
	}
	if (dom_size < MAX_NAME_SIZE) {
		dos_memcpy(my_domain, dom_start, dom_size);
		my_domain[dom_size] = 0;
	} else {
		dos_memcpy(my_domain, dom_start, MAX_NAME_SIZE);
		my_domain[MAX_NAME_SIZE - 1] = 0;
	}
	dos_memcpy(bs_ether, &eh->ether_shost, ETHERADDRL);
	dos_memcpy(bs_ipaddr, &iph->ip_src, IPADDRL);

	return (1);
}

static void
whoami_no_response(ushort count)
{
	if (count == 3) {
		printf("\n\nThe test program has been attempting to "
			"send \"whoami\" broadcast request\npackets "
			"to obtain this system's boot parameters.  ");
		printf("No matching response\nhas been seen.  ");
		printf("The program has already successfully sent and "
			"received\npackets to obtain the IP address.  ");
		printf("Either this machine is not registered\nwith a "
			"boot server or there is a problem in the driver "
			"that affects only\ncertain kinds of packet.  ");
		printf("For more information use the Solaris snoop\nprogram "
			"or another network "
			"monitoring mechanism to examine "
			"packets from\nand to this system.  ");
		printf("If you do not see any outgoing packets, there "
			"is\nprobably a problem with the driver's "
			"transmit code.  ");
		printf("If you see outgoing\npackets but no incoming "
			"packets the test machine is probably not "
			"registered\nwith a boot server.  ");
		printf("If you see outgoing and incoming packets there "
			"is\nprobably a problem with the driver's receive "
			"code.\n\n");
		printf("To aid with snooping, this program will "
			"send \"whoami\" requests at about\n"
			"%d second intervals indefinitely, or until "
			"it receives a reply.\n\n", SEND_INTERVAL);
		printf("You can also try pinging this system from "
			"another system.  Ping typically\nsends a "
			"broadcast ARP packet to request the target "
			"system's IP address,\nthen one or more ICMP "
			"packets to that IP address.  This program "
			"will\nreport reception of those packets and "
			"reply if possible.\n\n");
		printf("Press any key to terminate the program.\n");
		packets_received = 0;
	}
	if (count > 3 && packets_received > 0) {
		printf("%d other packet%s received.\n",
			packets_received, packets_received == 1 ? "" : "s");
		packets_received = 0;
	}
}

static ushort
udp_checksum(char *addr, struct pseudo_udp *ck)
{
	/* variables */
	ushort *end_hdr;
	ulong sum = 0;
	ushort cnt;
	ushort *sp;
	int flag = 0;

	/*
	 * Start on the pseudo header. Note that pseudo_udp already takes
	 * account of the udphdr...
	 */
	sp = (ushort *)ck;
	cnt = ntohs(ck->len) + sizeof (struct pseudo_udp) -
		sizeof (struct udphdr);
	end_hdr = (ushort *)((char *)sp + sizeof (struct pseudo_udp));

	cnt >>= 1;
	while (cnt--) {
		sum += *sp++;
		if (sum >= 0x10000) {	/* Wrap carries into low bit */
			sum -= 0x10000;
			sum++;
		}
		if (!flag && (sp >= end_hdr)) {
			sp = (ushort *)addr;	/* Start on the data */
			flag = 1;
		}
	}
	return ((ushort)~sum);
}

/*
 * Reverse ARP client side
 * Determine our Internet address given our Ethernet address
 * See RFC 903
 *
 * Returns 1 for success, 0 for failure.
 */
static int
send_revarp(void)
{
	struct arp_packet out;

	dos_memset(&out, 0, sizeof (struct arp_packet));

	out.arp_eh.ether_type = htons(ETHERTYPE_REVARP);
	out.arp_ea.arp_op = htons(REVARP_REQUEST);
	dos_memcpy(&out.arp_ea.arp_tha, my_ether, ETHERADDRL);
	/* What we want to find out... */
	dos_memset(out.arp_ea.arp_tpa, 0, IPADDRL);

	dos_memcpy(&out.arp_eh.ether_dhost, etherbroadcastaddr, ETHERADDRL);
	dos_memcpy(&out.arp_eh.ether_shost, my_ether, ETHERADDRL);
	out.arp_ea.arp_hrd = htons(ARPHRD_ETHER);
	out.arp_ea.arp_pro = htons(ETHERTYPE_IP);
	out.arp_ea.arp_hln = ETHERADDRL;
	out.arp_ea.arp_pln = IPADDRL;
	dos_memcpy(&out.arp_ea.arp_sha, my_ether, ETHERADDRL);
	dos_memcpy(out.arp_ea.arp_spa, my_ipaddr, IPADDRL);

	return (send_recv((unchar *)&out, sizeof (out),
		revarp_check_response, revarp_no_response));
}

static int
revarp_check_response(ushort len)
{
	register struct arp_packet *in = (struct arp_packet *)receive_buffer;

	UDprintf(("Received a %d-byte packet: ", len));

	if (len < sizeof (struct ether_header)) {
		UDprintf(("too short for an ethernet packet.\n"));
		return (0);
	}

	if (dos_memcmp(&in->arp_eh.ether_dhost, my_ether, ETHERADDRL) != 0) {
		if (dos_memcmp(&in->arp_eh.ether_dhost, etherbroadcastaddr,
				ETHERADDRL) == 0)
			UDprintf(("broadcast packet (ignored).\n"));
		else
			UDprintf(("not addressed to me.\n"));
		return (0);
	}

	/*
	 * The supposed packet was long enough to have a valid ethernet
	 * header and seemed to be addressed to me.  Keep a count of
	 * such packets to help with diagnosis.
	 */
	packets_received++;

	if (len < sizeof (struct arp_packet)) {
		UDprintf(("too short for a reverse ARP response.\n"));
		return (0);
	}

	if (in->arp_eh.ether_type != ntohs(ETHERTYPE_REVARP)) {
		UDprintf(("wrong type (not reverse ARP).\n"));
		return (0);
	}

	if (in->arp_ea.arp_pro != ntohs(ETHERTYPE_IP)) {
		UDprintf(("wrong type (not IP packet).\n"));
		return (0);
	}

	if (in->arp_ea.arp_op != ntohs(REVARP_REPLY)) {
		UDprintf(("wrong type (not reverse ARP reply).\n"));
		return (0);
	}

	if (dos_memcmp(&in->arp_ea.arp_tha, my_ether, ETHERADDRL) != 0) {
		UDprintf(("not a response to my request.\n"));
		return (0);
	}

	UDprintf(("the response to my reverse ARP request%s.\n",
		(bd.misc_flags & IGNORE_REVARP) ? " (ignored)" : ""));

	/* IGNORE_REVARP is set by the -zr option */
	if (bd.misc_flags & IGNORE_REVARP) {
		packets_received--;
		return (0);
	}

	dos_memcpy(my_ipaddr, in->arp_ea.arp_tpa, IPADDRL);
	my_ipaddr_known = 1;

	return (1);
}

static void
revarp_no_response(ushort count)
{
	if (count == 3) {
		printf("\n\nThe test program has been attempting to "
			"send reverse ARP broadcast request\npackets "
			"to obtain this system's IP address.  ");
		printf("No matching response has been\nseen.  ");
		printf("Use the Solaris snoop program or another network "
			"monitoring mechanism\nto examine "
			"packets from and to this system.  ");
		printf("If you do not see any outgoing\npackets, there "
			"is probably a problem with the driver's "
			"transmit code.  ");
		printf("If\nyou see outgoing packets but no incoming "
			"packets the network address of your\ntest machine "
			"is probably not registered "
			"with a name server.  ");
		printf("If you see\noutgoing and incoming packets there "
			"is probably a problem with the driver's\nreceive "
			"code.\n\n");
		printf("To aid with snooping, this program will "
			"send reverse ARP requests at about\n"
			"%d second intervals indefinitely, or until "
			"it receives a reply.\n\n", SEND_INTERVAL);
		printf("You can also try pinging this system from "
			"another system.  Ping typically\nsends a "
			"broadcast ARP packet to request the target "
			"system's IP address,\nthen one or more ICMP "
			"packets to that IP address.  This program "
			"will\nreport reception of those packets and "
			"reply if possible.\n\n");
		printf("Press any key to terminate the program.\n");
		packets_received = 0;
	}
	if (count > 3 && packets_received > 0) {
		printf("%d other packet%s received.\n",
			packets_received, packets_received == 1 ? "" : "s");
		packets_received = 0;
	}
}

static int
send_recv(unchar *out, ushort out_len, int (*valid_response)(ushort),
		void (*no_response_message)(ushort))
{
	ushort send_count;
	ushort time_elapsed;
	int len;
#define	SEND_INTERVAL	10 	/* Retry interval in seconds */
#define	POLL_INTERVAL	10 	/* Receive check interval in milliseconds */

	for (send_count = 0; /* NO TEST */; send_count++) {
		net_send_packet(out, out_len);

		time_elapsed = 0;
		while (time_elapsed < SEND_INTERVAL * 1000) {
			len = net_receive_packet(receive_buffer,
				sizeof (receive_buffer));

			/*
			 * If there was no packet, terminate if there was
			 * a keystroke.  Otherwise wait a bit then look again.
			 */
			if (len == 0) {
				if (dos_kb_char_nowait() != -1)
					return (0);
				dos_msecwait(POLL_INTERVAL);
				time_elapsed += POLL_INTERVAL;
				continue;
			}

			/* There was a packet.  Was it the expected one? */
			if ((*valid_response)(len))
				return (1);

			/* Not the expected packet.  Worth reporting? */
			other_packet(len);
		}
		(*no_response_message)(send_count);
	}
}

/*
 * The driver returned a packet that was not the expected response to a
 * packet that was sent.  Check whether it is something worth reporting.
 * For now that means either the ARP or ICMP from a ping.
 *
 * This routine does not provide debugging output describing the packet
 * because the routine that checked for the expected response did that.
 */
static void
other_packet(ushort len)
{
	static struct arp_packet arp_out;
	static struct icmp_packet icmp_out;
	static struct ether_header out_eh;
	struct ether_header *eh = (struct ether_header *)receive_buffer;
	struct ip *iph;
	unchar *end = receive_buffer + len;
	unchar *icmph;

	if (len < sizeof (struct ether_header))
		return;

	if (dos_memcmp(&eh->ether_dhost, etherbroadcastaddr,
				ETHERADDRL) == 0) {
		struct arp_packet *in = (struct arp_packet *)receive_buffer;

		/* Broadcast packet.  Could be an ARP for me */
		if (len < sizeof (struct arp_packet))
			return;

		if (in->arp_eh.ether_type != ntohs(ETHERTYPE_ARP))
			return;

		if (in->arp_ea.arp_pro != ntohs(ETHERTYPE_IP))
			return;

		if (in->arp_ea.arp_op != ntohs(ARPOP_REQUEST))
			return;

		/*
		 * If I already know my IP addr, ignore an ARP request
		 * not intended for me.
		 */
		if (my_ipaddr_known && dos_memcmp(&in->arp_ea.arp_tpa,
				my_ipaddr, IPADDRL) != 0)
			return;

		/* Prevent multiple reports for the same packet */
		if (packets_received > 0)
			packets_received--;

		printf("Received ARP request for IP %d.%d.%d.%d from "
			"%d.%d.%d.%d.\n", in->arp_ea.arp_tpa[0],
			in->arp_ea.arp_tpa[1], in->arp_ea.arp_tpa[2],
			in->arp_ea.arp_tpa[3], in->arp_ea.arp_spa[0],
			in->arp_ea.arp_spa[1], in->arp_ea.arp_spa[2],
			in->arp_ea.arp_spa[3]);

		if (my_ipaddr_known == 0)
			return;

		/* I know my IP address, so I can respond to the ARP req */

		dos_memset(&arp_out, 0, sizeof (struct arp_packet));

		arp_out.arp_eh.ether_type = htons(ETHERTYPE_ARP);
		arp_out.arp_ea.arp_op = htons(ARPOP_REPLY);
		dos_memcpy(&arp_out.arp_ea.arp_sha, my_ether, ETHERADDRL);
		dos_memcpy(&arp_out.arp_ea.arp_tha, &in->arp_ea.arp_sha,
			ETHERADDRL);
		dos_memcpy(arp_out.arp_ea.arp_spa, my_ipaddr, IPADDRL);
		dos_memcpy(arp_out.arp_ea.arp_tpa, &in->arp_ea.arp_spa,
			IPADDRL);

		dos_memcpy(&arp_out.arp_eh.ether_dhost,
			&in->arp_eh.ether_shost, ETHERADDRL);
		dos_memcpy(&arp_out.arp_eh.ether_shost, my_ether, ETHERADDRL);
		arp_out.arp_ea.arp_hrd = htons(ARPHRD_ETHER);
		arp_out.arp_ea.arp_pro = htons(ETHERTYPE_IP);
		arp_out.arp_ea.arp_hln = ETHERADDRL;
		arp_out.arp_ea.arp_pln = IPADDRL;

		/* Simple send, no retries */
		printf("Sending response to ARP request.\n");
		net_send_packet((unchar *)&arp_out, sizeof (arp_out));

		return;
	}

	if (dos_memcmp(&eh->ether_dhost, my_ether, ETHERADDRL) != 0)
		return;

	/* Packet was addressed to me: could be ICMP */

	if (eh->ether_type != htons(ETHERTYPE_IP))
		return;

	iph = (struct ip *)BEYOND(eh, sizeof (struct ether_header));
	if (BEYOND(iph, sizeof (struct ip)) > end)
		return;

	if (iph->ip_p != IPPROTO_ICMP)
		return;

	icmph = BEYOND(iph, (iph->ip_len_ver & 0xF) * sizeof (ulong));
	if (BEYOND(icmph, ICMP_MINLEN) > end)
		return;

	if (*icmph != ICMP_ECHO)
		return;

	/* Prevent multiple reports for the same packet */
	if (packets_received > 0)
		packets_received--;

	printf("Received ICMP echo request from %d.%d.%d.%d.\n",
		iph->ip_src.in_addr_octet[0], iph->ip_src.in_addr_octet[1],
		iph->ip_src.in_addr_octet[2], iph->ip_src.in_addr_octet[3]);

	/* Attempt to respond to the ICMP packet */
	dos_memcpy(&icmp_out.icmp, icmph, sizeof (icmp_out.icmp));
	icmp_out.icmp.icmp_type = ICMP_ECHOREPLY;
	icmp_out.icmp.icmp_cksum += ICMP_ECHO - ICMP_ECHOREPLY;

	/* Fill in the IP header portion of the packet */
	icmp_out.iph.ip_len_ver = 0x45;	/* Version 4, length = 5 longs */
	icmp_out.iph.ip_tos = 0;
	icmp_out.iph.ip_len = htons(sizeof (icmp_out.icmp) +
		sizeof (icmp_out.iph));
	icmp_out.iph.ip_id = 0;
	icmp_out.iph.ip_off = 0;
	icmp_out.iph.ip_ttl = 0xFF;
	icmp_out.iph.ip_p = IPPROTO_ICMP;
	icmp_out.iph.ip_sum = 0;
	dos_memcpy(&icmp_out.iph.ip_src, my_ipaddr, IPADDRL);
	dos_memcpy(&icmp_out.iph.ip_dst, &iph->ip_src, IPADDRL);
	icmp_out.iph.ip_sum = ip_cksum((char *)&icmp_out.iph,
		sizeof (struct ip));

	/* Fill in the ethernet header portion of the packet */
	dos_memcpy(&out_eh.ether_dhost, &eh->ether_shost, ETHERADDRL);
	dos_memcpy(&out_eh.ether_shost, my_ether, ETHERADDRL);
	out_eh.ether_type = htons(ETHERTYPE_IP);
	dos_memcpy(icmp_out.eh, (char *)&out_eh, sizeof (out_eh));

	/* Simple send, no retries */
	printf("Sending response to ICMP request.\n");
	net_send_packet((unchar *)&icmp_out.eh,
		sizeof (icmp_out) - sizeof (icmp_out.fill));
}

/*
 * Compute one's complement checksum
 * for IP packet headers.  Based on ipcksum in inet.c.
 */
static unsigned short
ip_cksum(char *cp, unsigned int count)
{
	register unsigned short *sp = (unsigned short *)cp;
	register unsigned long  sum = 0;
	register unsigned long  oneword = 0x00010000;

	if (count == 0)
		return (0);
	count >>= 1;
	while (count--) {
		sum += *sp++;
		if (sum >= oneword) {	/* Wrap carries into low bit */
			sum -= oneword;
			sum++;
		}
	}
	return ((unsigned short)~sum);
}

static ushort
swap_two_bytes(ushort in)
{
	union {
		unchar b[2];
		ushort s;
	} u1, u2;

	u1.s = in;
	u2.b[0] = u1.b[1];
	u2.b[1] = u1.b[0];
	return (u2.s);
}

static ulong
swap_four_bytes(ulong in)
{
	union {
		unchar b[4];
		ulong l;
	} u1, u2;

	u1.l = in;
	u2.b[0] = u1.b[3];
	u2.b[1] = u1.b[2];
	u2.b[2] = u1.b[1];
	u2.b[3] = u1.b[0];
	return (u2.l);
}
