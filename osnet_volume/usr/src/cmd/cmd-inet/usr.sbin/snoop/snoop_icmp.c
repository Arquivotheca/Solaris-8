/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)snoop_icmp.c	1.7	99/11/08 SMI"	/* SunOS */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/stropts.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>
#include <inet/ip.h>
#include <arpa/inet.h>
#include "snoop.h"

#ifndef	MIN
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

/* Router advertisement message structure. */
struct icmp_ra_addr {
	uint32_t addr;
	uint32_t preference;
};

interpret_icmp(int flags, struct icmp *icmp, int iplen, int ilen)
{
	char *pt, *pc;
	char *line;
	char buff[2048];
	struct udphdr *orig_uhdr;
	extern char *prot_nest_prefix;

	if (ilen < ICMP_MINLEN)
		return (ilen);		/* incomplete header */

	pt = "Unknown";
	pc = "";

	switch (icmp->icmp_type) {
	case ICMP_ECHOREPLY:
		pt = "Echo reply";
		(void) sprintf(buff, "ID: %d Sequence number: %d",
		    ntohs(icmp->icmp_id), ntohs(icmp->icmp_seq));
		pc = buff;
		break;
	case ICMP_UNREACH:
		pt = "Destination unreachable";
		switch (icmp->icmp_code) {
		case ICMP_UNREACH_NET:
			if (ilen >= ICMP_ADVLENMIN) {
				(void) sprintf(buff, "Net %s unreachable",
				    addrtoname(AF_INET,
				    &icmp->icmp_ip.ip_dst));
				pc = buff;
			} else {
				pc = "Bad net";
			}
			break;
		case ICMP_UNREACH_HOST:
			if (ilen >= ICMP_ADVLENMIN) {
				(void) sprintf(buff, "Host %s unreachable",
				    addrtoname(AF_INET,
				    &icmp->icmp_ip.ip_dst));
				pc = buff;
			} else {
				pc = "Bad host";
			}
			break;
		case ICMP_UNREACH_PROTOCOL:
			if (ilen >= ICMP_ADVLENMIN) {
				(void) sprintf(buff, "Bad protocol %d",
				    icmp->icmp_ip.ip_p);
				pc = buff;
			} else {
				pc = "Bad protocol";
			}
			break;
		case ICMP_UNREACH_PORT:
			if (ilen >= ICMP_ADVLENMIN) {
				orig_uhdr = (struct udphdr *)((uchar_t *)icmp +
				    ICMP_MINLEN + icmp->icmp_ip.ip_hl * 4);
				switch (icmp->icmp_ip.ip_p) {
				case IPPROTO_TCP:
					(void) sprintf(buff, "TCP port %d"
					    " unreachable",
					    ntohs(orig_uhdr->uh_dport));
					pc = buff;
					break;
				case IPPROTO_UDP:
					(void) sprintf(buff, "UDP port %d"
					    " unreachable",
					    ntohs(orig_uhdr->uh_dport));
					pc = buff;
					break;
				default:
					pc = "Port unreachable";
					break;
				}
			} else {
				pc = "Bad port";
			}
			break;
		case ICMP_UNREACH_NEEDFRAG:
			if (ntohs(icmp->icmp_nextmtu) != 0) {
				(void) sprintf(buff, "Needed to fragment:"
				    " next hop MTU = %d",
				    ntohs(icmp->icmp_nextmtu));
				pc = buff;
			} else {
				pc = "Needed to fragment";
			}
			break;
		case ICMP_UNREACH_SRCFAIL:
			pc = "Source route failed";
			break;
		case ICMP_UNREACH_NET_UNKNOWN:
			pc = "Unknown network";
			break;
		case ICMP_UNREACH_HOST_UNKNOWN:
			pc = "Unknown host";
			break;
		case ICMP_UNREACH_ISOLATED:
			pc = "Source host isolated";
			break;
		case ICMP_UNREACH_NET_PROHIB:
			pc = "Net administratively prohibited";
			break;
		case ICMP_UNREACH_HOST_PROHIB:
			pc = "Host administratively prohibited";
			break;
		case ICMP_UNREACH_TOSNET:
			pc = "Net unreachable for this TOS";
			break;
		case ICMP_UNREACH_TOSHOST:
			pc = "Host unreachable for this TOS";
			break;
		case ICMP_UNREACH_FILTER_PROHIB:
			pc = "Communication administratively prohibited";
			break;
		case ICMP_UNREACH_HOST_PRECEDENCE:
			pc = "Host precedence violation";
			break;
		case ICMP_UNREACH_PRECEDENCE_CUTOFF:
			pc = "Precedence cutoff in effect";
			break;
		default:
			break;
		}
		break;
	case ICMP_SOURCEQUENCH:
		pt = "Packet lost, slow down";
		break;
	case ICMP_REDIRECT:
		pt = "Redirect";
		switch (icmp->icmp_code) {
		case ICMP_REDIRECT_NET:
			pc = "for network";
			break;
		case ICMP_REDIRECT_HOST:
			pc = "for host";
			break;
		case ICMP_REDIRECT_TOSNET:
			pc = "for tos and net";
			break;
		case ICMP_REDIRECT_TOSHOST:
			pc = "for tos and host";
			break;
		default:
			break;
		}
		(void) sprintf(buff, "%s %s to %s",
			pc, addrtoname(AF_INET, &icmp->icmp_ip.ip_dst),
			addrtoname(AF_INET, &icmp->icmp_gwaddr));
		pc = buff;
		break;
	case ICMP_ECHO:
		pt = "Echo request";
		(void) sprintf(buff, "ID: %d Sequence number: %d",
		    ntohs(icmp->icmp_id), ntohs(icmp->icmp_seq));
		pc = buff;
		break;
	case ICMP_ROUTERADVERT:

#define	icmp_num_addrs	icmp_hun.ih_rtradv.irt_num_addrs
#define	icmp_wpa	icmp_hun.ih_rtradv.irt_wpa
#define	icmp_lifetime	icmp_hun.ih_rtradv.irt_lifetime

		pt = "Router advertisement";
		(void) sprintf(buff, "Lifetime %ds [%d]:",
		    ntohs(icmp->icmp_lifetime), icmp->icmp_num_addrs);
		if (icmp->icmp_wpa == 2) {
			struct icmp_ra_addr *ra;
			int num_addrs;
			char ra_buf[128];
			struct in_addr sin;

			/* Cannot trust anything from the network... */
			num_addrs = MIN((ilen - ICMP_MINLEN) / 8,
			    icmp->icmp_num_addrs);

			ra = (struct icmp_ra_addr *)icmp->icmp_data;
			while (num_addrs-- > 0) {
				sin.s_addr = ra->addr;
				(void) sprintf(ra_buf, " {%s %u}",
				    addrtoname(AF_INET, &sin),
				    ntohl(ra->preference));
				(void) strcat(buff, ra_buf);
				ra++;
			}
			pc = buff;
		}
		break;
	case ICMP_ROUTERSOLICIT:
		pt = "Router solicitation";
		break;
	case ICMP_TIMXCEED:
		pt = "Time exceeded";
		switch (icmp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			pc = "in transit";
			break;
		case ICMP_TIMXCEED_REASS:
			pc = "in reassembly";
			break;
		default:
			break;
		}
		break;
	case ICMP_PARAMPROB:
		pt = "IP parameter problem";
		switch (icmp->icmp_code) {
		case ICMP_PARAMPROB_OPTABSENT:
			pc = "Required option missing";
			break;
		case ICMP_PARAMPROB_BADLENGTH:
			pc = "Bad length";
			break;
		case 0: /* Should this be the default? */
			(void) sprintf(buff, "Problem at octet %d\n",
			    icmp->icmp_pptr);
			pc = buff;
		default:
			break;
		}
		break;
	case ICMP_TSTAMP:
		pt = "Timestamp request";
		break;
	case ICMP_TSTAMPREPLY:
		pt = "Timestamp reply";
		break;
	case ICMP_IREQ:
		pt = "Information request";
		break;
	case ICMP_IREQREPLY:
		pt = "Information reply";
		break;
	case ICMP_MASKREQ:
		pt = "Address mask request";
		break;
	case ICMP_MASKREPLY:
		pt = "Address mask reply";
		(void) sprintf(buff, "Mask = 0x%x", ntohl(icmp->icmp_mask));
		pc = buff;
		break;
	default:
		break;
	}

	if (flags & F_SUM) {
		line = get_sum_line();
		if (*pc)
			(void) sprintf(line, "ICMP %s (%s)", pt, pc);
		else
			(void) sprintf(line, "ICMP %s", pt);
	}

	if (flags & F_DTAIL) {
		show_header("ICMP:  ", "ICMP Header", ilen);
		show_space();
		(void) sprintf(get_line(0, 0), "Type = %d (%s)",
		    icmp->icmp_type, pt);
		if (*pc) {
			(void) sprintf(get_line(0, 0), "Code = %d (%s)",
			    icmp->icmp_code, pc);
		} else {
			(void) sprintf(get_line(0, 0), "Code = %d",
			    icmp->icmp_code);
		}
		(void) sprintf(get_line(0, 0), "Checksum = %x",
		    ntohs(icmp->icmp_cksum));

		if (icmp->icmp_type == ICMP_UNREACH ||
		    icmp->icmp_type == ICMP_REDIRECT) {
			if (ilen > 28) {
				show_space();
				(void) sprintf(get_line(0, 0),
				    "[ subject header follows ]");
				show_space();
				prot_nest_prefix = "ICMP:";
				interpret_ip(flags, icmp->icmp_data, 28);
				prot_nest_prefix = "";
			}
		}
		if (icmp->icmp_type == ICMP_PARAMPROB) {
			if (ilen > 28) {
				show_space();
				(void) sprintf(get_line(0, 0),
				    "[ subject header follows ]");
				show_space();
				prot_nest_prefix = "ICMP:";
				interpret_ip(flags, icmp->icmp_data, 28);
				prot_nest_prefix = "";
			}
		}
		show_space();
	}
}

interpret_icmpv6(flags, icmp6, iplen, ilen)
	icmp6_t *icmp6;
	int iplen, ilen;
{
	char *pt, *pc;
	char *line;
	extern char *prot_nest_prefix;
	int mask = 0xffff;
	char addrstr[INET6_ADDRSTRLEN];
	char buff[2048];

	if (ilen < ICMP_MINLEN)
		return (ilen);		/* incomplete header */

	pt = "Unknown";
	pc = "";

	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
		pt = "Destination unreachable";
		switch (icmp6->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			pc = "No route to destination";
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			pc = "Communication adminly probhibited";
			break;
		case ICMP6_DST_UNREACH_ADDR:
			pc = "Address unreachable";
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			pc = "Port unreachable";
			break;
		default:
			break;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		pt = "Packet too big";
		break;
	case ND_REDIRECT:
		pt = "Redirect";
		break;
	case ICMP6_TIME_EXCEEDED:
		pt = "Time exceeded";
		switch (icmp6->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			pc = "Hop limit exceeded in transit";
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			pc = "Fragment reassambly time exceeded";
			break;
		default:
			break;
		}
		break;
	case ICMP6_PARAM_PROB:
		pt = "Parameter problem";
		switch (icmp6->icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
			pc = "Erroneous header field";
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			pc = "Unrecognized next header type";
			break;
		case ICMP6_PARAMPROB_OPTION:
			pc = "Unrecognized IPv6 option";
			break;
		}
		break;
	case ICMP6_ECHO_REQUEST:
		pt = "Echo request";
		(void) sprintf(buff, "ID: %d Sequence number: %d",
		    ntohs(icmp6->icmp6_id), ntohs(icmp6->icmp6_seq));
		pc = buff;
		break;
	case ICMP6_ECHO_REPLY:
		pt = "Echo reply";
		(void) sprintf(buff, "ID: %d Sequence number: %d",
		    ntohs(icmp6->icmp6_id), ntohs(icmp6->icmp6_seq));
		pc = buff;
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		pt = "Group membership query";
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		pt = "Group membership report";
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		pt = "Group membership termination";
		break;
	case ND_ROUTER_SOLICIT:
		pt = "Router solicitation";
		break;
	case ND_ROUTER_ADVERT:
		pt = "Router advertisement";
		break;
	case ND_NEIGHBOR_SOLICIT:
		pt = "Neighbor solicitation";
		break;
	case ND_NEIGHBOR_ADVERT:
		pt = "Neighbor advertisement";
		break;
	default:
		break;
	}

	if (flags & F_SUM) {
		line = get_sum_line();
		if (*pc)
			(void) sprintf(line, "ICMPv6 %s (%s)", pt, pc);
		else
			(void) sprintf(line, "ICMPv6 %s", pt);
	}

	if (flags & F_DTAIL) {
		show_header("ICMPv6:  ", "ICMPv6 Header", ilen);
		show_space();
		(void) sprintf(get_line(0, 0), "Type = %d (%s)",
		    icmp6->icmp6_type, pt);
		if (*pc)
			(void) sprintf(get_line(0, 0), "Code = %d (%s)",
			    icmp6->icmp6_code, pc);
		else
			(void) sprintf(get_line(0, 0), "Code = %d",
			    icmp6->icmp6_code);
		(void) sprintf(get_line(0, 0), "Checksum = %x",
		    ntohs(icmp6->icmp6_cksum));

		switch (icmp6->icmp6_type) {
		case ICMP6_PACKET_TOO_BIG:
			show_space();
			(void) sprintf(get_line(0, 0),
			    " Packet too big MTU = %d", icmp6->icmp6_mtu);
			show_space();
			break;
		case ND_REDIRECT: {
			nd_redirect_t *rd = (nd_redirect_t *)icmp6;

			(void) sprintf(get_line(0, 0), "Target address= %s",
			    inet_ntop(AF_INET6, (char *)&rd->nd_rd_target,
			    addrstr, INET6_ADDRSTRLEN));

			(void) sprintf(get_line(0, 0),
			    "Destination address= %s",
			    inet_ntop(AF_INET6, (char *)&rd->nd_rd_dst,
			    addrstr, INET6_ADDRSTRLEN));
			show_space();
			interpret_options(flags, (char *)icmp6 + sizeof (*rd),
			    iplen, ilen - sizeof (*rd));
			break;
		}
		case ND_NEIGHBOR_SOLICIT: {
			struct nd_neighbor_solicit *ns;
			if (ilen < sizeof (*ns))
				break;
			ns = (struct nd_neighbor_solicit *)icmp6;
			(void) sprintf(get_line(0, 0), "Target node = %s, %s",
			    inet_ntop(AF_INET6, (char *)&ns->nd_ns_target,
			    addrstr, INET6_ADDRSTRLEN),
			    addrtoname(AF_INET6, &ns->nd_ns_target));
			show_space();
			interpret_options(flags, (char *)icmp6 + sizeof (*ns),
			    iplen, ilen - sizeof (*ns));
			break;
		}

		case ND_NEIGHBOR_ADVERT: {
			struct nd_neighbor_advert *na;

			if (ilen < sizeof (*na))
				break;
			na = (struct nd_neighbor_advert *)icmp6;
			(void) sprintf(get_line(0, 0), "Target node = %s, %s",
			    inet_ntop(AF_INET6, (char *)&na->nd_na_target,
			    addrstr, INET6_ADDRSTRLEN),
			    addrtoname(AF_INET6, &na->nd_na_target));
			(void) sprintf(get_line(0, 0),
			    "Router flag: %s, Solicited flag: %s, "
			    "Override flag: %s",
			    na->nd_na_flags_reserved & ND_NA_FLAG_ROUTER ?
			    "SET" : "NOT SET",
			    na->nd_na_flags_reserved & ND_NA_FLAG_SOLICITED ?
			    "SET" : "NOT SET",
			    na->nd_na_flags_reserved & ND_NA_FLAG_OVERRIDE ?
			    "SET" : "NOT SET");

			show_space();
			interpret_options(flags, (char *)icmp6 + sizeof (*na),
			    iplen, ilen - sizeof (*na));
		}
		break;

		case ND_ROUTER_SOLICIT: {
			if (ilen < sizeof (struct nd_router_solicit))
				break;
			interpret_options(flags,
			    (char *)icmp6 + sizeof (struct nd_router_solicit),
			    iplen, ilen - sizeof (struct nd_router_solicit));
			break;
		}

		case ND_ROUTER_ADVERT: {
			struct nd_router_advert *ra;

			if (ilen < sizeof (*ra))
				break;
			ra = (struct nd_router_advert *)icmp6;
			(void) sprintf(get_line(0, 0),
			    "Max hops= %d, Router lifetime= %d",
			    ra->nd_ra_curhoplimit,
			    ntohs(ra->nd_ra_router_lifetime));

			(void) sprintf(get_line(0, 0),
			    "Managed addr conf flag: %s, Other conf flag: %s",
			    ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED ?
			    "SET" : "NOT SET",
			    ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER ?
			    "SET" : "NOT SET");

			(void) sprintf(get_line(0, 0),
			    "Reachable time: %u, Reachable retrans time %u",
			    ntohl(ra->nd_ra_reachable),
			    ntohl(ra->nd_ra_retransmit));
			show_space();

			interpret_options(flags, (char *)icmp6 + sizeof (*ra),
			    iplen, ilen - sizeof (*ra));
			break;
		}
		case ICMP6_PARAM_PROB:
			if (ilen < sizeof (*icmp6))
				break;
			(void) sprintf(get_line(0, 0), "Ptr = %u",
			    ntohl(icmp6->icmp6_pptr));
			show_space();
			break;

		case ICMP6_MEMBERSHIP_REPORT: {
			icmp6_mld_t *icmp6g;

			if (ilen < sizeof (*icmp6g))
				break;
			icmp6g = (icmp6_mld_t *)icmp6;
			(void) sprintf(get_line(0, 0), "Multicast address= %s",
			    inet_ntop(AF_INET6,
			    (char *)icmp6g->icmp6m_group.s6_addr, addrstr,
			    INET6_ADDRSTRLEN));
			show_space();
			break;
		}
		default:
			break;
		}
	}
}

interpret_options(flags, optc, iplen, ilen)
	char *optc;
	int iplen, ilen;
{
#define	PREFIX_OPTION_LENGTH    4
#define	MTU_OPTION_LENGTH	1

#define	PREFIX_INFINITY		0xffffffffUL

	struct nd_opt_hdr *opt;
	int i;

	for (; ilen >= sizeof (*opt); ) {
		opt = (struct nd_opt_hdr *)optc;
		if (opt->nd_opt_len == 0)
			return;
		switch (opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		{
			struct nd_opt_lla *lopt;
			char *buf, chbuf[128];

			if (ilen < (int)opt->nd_opt_len * 8)
				break;

			buf = chbuf;

			lopt = (struct nd_opt_lla *)opt;
			if (lopt->nd_opt_lla_type == ND_OPT_SOURCE_LINKADDR) {
				(void) sprintf(get_line(0, 0),
				    "+++ ICMPv6 Source LL Addr option +++");
			} else {
				(void) sprintf(get_line(0, 0),
				    "+++ ICMPv6 Target LL Addr option +++");
			}
			for (i = 0; i < (int)lopt->nd_opt_lla_len * 8 - 2;
			    i++) {
				sprintf(buf, "%x:",
				    (uint_t)lopt->nd_opt_lla_hdw_addr[i]);
				buf += strlen(buf);
			}
			*(buf - 1) = '\0'; /* Erase last colon */
			(void) sprintf(get_line(0, 0),
			    "Link Layer address: %s", chbuf);
			show_space();
			break;
		}
		case ND_OPT_MTU: {
			struct nd_opt_mtu *mopt;
			if (opt->nd_opt_len != MTU_OPTION_LENGTH ||
			    ilen < sizeof (struct nd_opt_mtu))
				break;
			(void) sprintf(get_line(0, 0),
			    "+++ ICMPv6 MTU option +++");
			mopt = (struct nd_opt_mtu *)opt;
			(void) sprintf(get_line(0, 0),
			    "MTU = %u ", mopt->nd_opt_mtu_mtu);
			show_space();
			break;
		}
		case ND_OPT_PREFIX_INFORMATION: {
			struct nd_opt_prefix_info *popt;
			char validstr[30];
			char preferredstr[30];
			char prefixstr[INET6_ADDRSTRLEN];

			if (opt->nd_opt_len != PREFIX_OPTION_LENGTH ||
			    ilen < sizeof (struct nd_opt_prefix_info))
				break;
			popt = (struct nd_opt_prefix_info *)opt;
			(void) sprintf(get_line(0, 0),
			    "+++ ICMPv6 Prefix option +++");
			(void) sprintf(get_line(0, 0),
			    "Prefix length = %d ", popt->nd_opt_pi_prefix_len);
			(void) sprintf(get_line(0, 0),
			    "Onlink flag: %s, Autonomous addr conf flag: %s",
			    popt->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK ? "SET" : "NOT SET",
			    popt->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO ? "SET" : "NOT SET");

			if (ntohl(popt->nd_opt_pi_valid_time) ==
			    PREFIX_INFINITY)
				sprintf(validstr, "INFINITY");
			else
				sprintf(validstr, "%lu",
				    ntohl(popt->nd_opt_pi_valid_time));

			if (ntohl(popt->nd_opt_pi_preferred_time) ==
			    PREFIX_INFINITY)
				sprintf(preferredstr, "INFINITY");
			else
				sprintf(preferredstr, "%lu",
				    ntohl(popt->nd_opt_pi_preferred_time));

			(void) sprintf(get_line(0, 0),
			    "Valid Lifetime %s, Preferred Lifetime %s",
			    validstr, preferredstr);
			(void) sprintf(get_line(0, 0), "Prefix %s",
			    inet_ntop(AF_INET6,
			    (char *)&popt->nd_opt_pi_prefix, prefixstr,
			    INET6_ADDRSTRLEN));
			show_space();
		}
		default:
			break;
		}
		optc += opt->nd_opt_len * 8;
		ilen -= opt->nd_opt_len * 8;
	}
}
