/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ping_aux6.c	1.5	99/09/24 SMI"	/* SVr4.0 1.4	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stropts.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <stdlib.h>

#include <ifaddrlist.h>
#include "ping.h"

void check_reply6(struct hostinfo *, struct msghdr *, int, ushort_t);
extern void find_dstaddr(ushort_t, union any_in_addr *);
static int IPv6_hdrlen(ip6_t *, int, uint8_t *);
extern boolean_t is_a_target(struct hostinfo *, union any_in_addr *);
static void pr_ext_headers(struct msghdr *);
extern char *pr_name(char *, int);
extern char *pr_protocol(int);
static void pr_rthdr(unsigned char *);
static char *pr_type6(uchar_t);
extern void schedule_sigalrm();
extern void send_scheduled_probe();
extern boolean_t seq_match(ushort_t, int, ushort_t);
void set_ancillary_data(struct msghdr *, int, union any_in_addr *, int, uint_t);
extern void sigalrm_handler();
extern void tvsub(struct timeval *, struct timeval *);


/*
 * Initialize the msghdr for specifying the hoplimit, outgoing interface and
 * routing header.
 */
void
set_ancillary_data(struct msghdr *msgp, int hoplimit,
    union any_in_addr *gwIPlist, int gw_cnt, uint_t if_index)
{
	size_t hoplimit_space;
	size_t rthdr_space;
	size_t pktinfo_space;
	size_t bufspace;
	struct cmsghdr *cmsgp;
	uchar_t *cmsg_datap;
	static boolean_t first = _B_TRUE;
	int i;

	if (hoplimit == -1 && gw_cnt == 0 && if_index == 0)
		return;

	/*
	 * Need to figure out size of buffer needed for ancillary data
	 * containing routing header and packet info options.
	 *
	 * Portable heuristic to compute upper bound on space needed for
	 * N ancillary data options. It assumes up to _MAX_ALIGNMENT padding
	 * after both header and data as the worst possible upper bound on space
	 * consumed by padding.
	 * It also adds one extra "sizeof (struct cmsghdr)" for the last option.
	 * This is needed because we would like to use CMSG_NXTHDR() while
	 * composing the buffer. The CMSG_NXTHDR() macro is designed better for
	 * parsing than composing the buffer. It requires the pointer it returns
	 * to leave space in buffer for addressing a cmsghdr and we want to make
	 * sure it works for us while we skip beyond the last ancillary data
	 * option.
	 *
	 * bufspace[i]  = sizeof(struct cmsghdr) + <pad after header> +
	 *		<option[i] content length> + <pad after data>;
	 *
	 * total_bufspace = bufspace[0] + bufspace[1] + ...
	 *		    ... + bufspace[N-1] + sizeof (struct cmsghdr);
	 */

	rthdr_space = 0;
	pktinfo_space = 0;
	hoplimit_space = 0;
	bufspace = 0;

	if (hoplimit != -1) {
		hoplimit_space = sizeof (int);
		bufspace += sizeof (struct cmsghdr) + _MAX_ALIGNMENT +
		    hoplimit_space + _MAX_ALIGNMENT;
	}

	if (gw_cnt > 0) {
		rthdr_space = __inet6_rthdr_space(IPV6_RTHDR_TYPE_0, gw_cnt);
		bufspace += sizeof (struct cmsghdr) + _MAX_ALIGNMENT +
		    rthdr_space + _MAX_ALIGNMENT;
	}

	if (if_index != 0) {
		pktinfo_space = sizeof (struct in6_pktinfo);
		bufspace += sizeof (struct cmsghdr) + _MAX_ALIGNMENT +
		    pktinfo_space + _MAX_ALIGNMENT;
	}

	/*
	 * We need to temporarily set the msgp->msg_controllen to bufspace
	 * (we will later trim it to actual length used). This is needed because
	 * CMSG_NXTHDR() uses it to check we have not exceeded the bounds.
	 */
	bufspace += sizeof (struct cmsghdr);
	msgp->msg_controllen = bufspace;

	/*
	 * This function is called more than once only if -l/-S used,
	 * since we need to modify the middle gateway. So, don't alloc
	 * new memory, just reuse what msg6 points to.
	 */
	if (first) {
		first = _B_FALSE;
		msgp->msg_control = (struct cmsghdr *)malloc(bufspace);
		if (msgp->msg_control == NULL) {
			Fprintf(stderr, "%s: malloc %s\n",
			    progname, strerror(errno));
			exit(EXIT_FAILURE);
		}
	};
	cmsgp = CMSG_FIRSTHDR(msgp);

	/*
	 * Fill ancillary data. First hoplimit, then rthdr and pktinfo.
	 */

	/* set hoplimit ancillary data if needed */
	if (hoplimit != -1) {
		cmsgp->cmsg_level = IPPROTO_IPV6;
		cmsgp->cmsg_type = IPV6_HOPLIMIT;
		cmsg_datap = CMSG_DATA(cmsgp);
		*(int *)cmsg_datap = hoplimit;
		cmsgp->cmsg_len = cmsg_datap + hoplimit_space -
		    (uchar_t *)cmsgp;
		cmsgp = CMSG_NXTHDR(msgp, cmsgp);
	}

	/* set rthdr ancillary data if needed */
	if (gw_cnt > 0) {
		struct ip6_rthdr0 *rthdr0p;

		cmsgp->cmsg_level = IPPROTO_IPV6;
		cmsgp->cmsg_type = IPV6_RTHDR;
		cmsg_datap = CMSG_DATA(cmsgp);

		/*
		 * Initialize rthdr structure
		 */
		rthdr0p = (struct ip6_rthdr0 *)cmsg_datap;
		if (__inet6_rthdr_init(rthdr0p, rthdr_space,
		    IPV6_RTHDR_TYPE_0, gw_cnt) == NULL) {
			Fprintf(stderr, "%s: __inet6_rthdr_init failed\n",
			    progname);
			exit(EXIT_FAILURE);
		}

		/*
		 * Stuff in gateway addresses
		 */
		for (i = 0; i < gw_cnt; i++) {
			if (__inet6_rthdr_add(rthdr0p,
			    &gwIPlist[i].addr6) == -1) {
				Fprintf(stderr,
				    "%s: __inet6_rthdr_add\n", progname);
				exit(EXIT_FAILURE);
			}
		}

		cmsgp->cmsg_len = cmsg_datap + rthdr_space - (uchar_t *)cmsgp;
		cmsgp = CMSG_NXTHDR(msgp, cmsgp);
	}

	/* set pktinfo ancillary data if needed */
	if (if_index != 0) {
		struct in6_pktinfo *pktinfop;

		cmsgp->cmsg_level = IPPROTO_IPV6;
		cmsgp->cmsg_type = IPV6_PKTINFO;
		cmsg_datap = CMSG_DATA(cmsgp);

		pktinfop = (struct in6_pktinfo *)cmsg_datap;
		/*
		 * We don't know if pktinfop->ipi6_addr is aligned properly,
		 * therefore let's use bcopy, instead of assignment.
		 */
		(void) bcopy(&in6addr_any, &pktinfop->ipi6_addr,
		sizeof (struct in6_addr));

		/*
		 *  We can assume pktinfop->ipi6_ifindex is 32 bit aligned.
		*/
		pktinfop->ipi6_ifindex = if_index;
		cmsgp->cmsg_len = cmsg_datap + pktinfo_space - (uchar_t *)cmsgp;
		cmsgp = CMSG_NXTHDR(msgp, cmsgp);
	}

	msgp->msg_controllen = (char *)cmsgp - (char *)msgp->msg_control;
}

/*
 * Check out the packet to see if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
check_reply6(struct hostinfo *hi_dst6, struct msghdr *msg, int cc,
    ushort_t udp_src_port)
{
	struct icmp6_hdr *icmp6;
	ip6_t *ip6h;
	nd_redirect_t *nd_rdrct;
	struct udphdr *up;
	union any_in_addr dst_addr;
	uchar_t *buf;
	int32_t *intp;
	struct sockaddr_in6 *from6;
	struct timeval tv;
	struct timeval *tp;
	int triptime;
	boolean_t valid_reply = _B_FALSE;
	boolean_t reply_matched_current_target;	/* Is the source address of */
						/* this reply same as where */
						/* we're sending currently? */
	boolean_t last_reply_from_targetaddr = _B_FALSE; /* Is this stats, */
						/* probe all with npackets>0 */
						/* and we received reply for */
						/* the last probe sent to */
						/* targetaddr */
	uint32_t ip6hdr_len;
	uint8_t last_hdr;
	int cc_left;
	int i;
	char tmp_buf[INET6_ADDRSTRLEN];
	static char *unreach6[] = {
	    "No Route to Destination",
	    "Communication Administratively Prohibited",
	    "Not a Neighbor (obsoleted ICMPv6 code)",
	    "Address Unreachable",
	    "Port Unreachable"
	};
	static char *timexceed6[] = {
	    "Hop limit exceeded in transit",
	    "Fragment reassembly time exceeded"
	};
	static char *param_prob6[] = {
	    "Erroneous header field encountered",
	    "Unrecognized next header type encountered",
	    "Unrecognized IPv6 option encountered"
	};
	boolean_t print_newline = _B_FALSE;

	/* decompose msghdr into useful pieces */
	buf = (uchar_t *)msg->msg_iov->iov_base;
	from6 = (struct sockaddr_in6 *)msg->msg_name;

	intp = (int32_t *)buf;

	/* get time now for most accurate time calculation */
	(void) gettimeofday(&tv, (struct timezone *)NULL);

	/* Ignore packets > 64k or control buffers that don't fit */
	if (msg->msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
		if (verbose) {
			Printf("Truncated message: msg_flags 0x%x from %s\n",
			    msg->msg_flags,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));
		}
		return;
	}
	if (cc < ICMP6_MINLEN) {
		if (verbose) {
			Printf("packet too short (%d bytes) from %s\n", cc,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));
		}
		return;
	}
	icmp6 = (struct icmp6_hdr *)buf;
	cc_left = cc - ICMP6_MINLEN;

	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
		ip6h = (ip6_t *)((char *)icmp6 + ICMP6_MINLEN);
		if (cc_left < sizeof (ip6_t)) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			return;
		}

		/*
		 * Determine the total length of IPv6 header and extension
		 * headers, also the upper layer header (UDP, TCP, ICMP, etc.)
		 * following.
		 */
		ip6hdr_len = IPv6_hdrlen(ip6h, cc_left, &last_hdr);

		cc_left -= ip6hdr_len;

		up = (struct udphdr *)((char *)ip6h + ip6hdr_len);
		if (cc_left < sizeof (struct udphdr)) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			return;
		}
		cc_left -= sizeof (struct udphdr);

		/* determine if this is *the* reply */
		if (icmp6->icmp6_code == ICMP6_DST_UNREACH_NOPORT &&
		    last_hdr == IPPROTO_UDP &&
		    udp_src_port == up->uh_sport &&
		    use_udp) {
			valid_reply = _B_TRUE;
		} else {
			valid_reply = _B_FALSE;
		}

		if (valid_reply) {
			/*
			 * For this valid reply, if we are still sending to
			 * this target IP address, we'd like to do some
			 * updates to targetaddr, so hold SIGALRMs.
			 */
			(void) sighold(SIGALRM);
			is_alive = _B_TRUE;
			nreceived++;
			reply_matched_current_target =
			    seq_match(current_targetaddr->starting_seq_num,
				current_targetaddr->num_sent,
				ntohs(up->uh_dport));
			if (reply_matched_current_target) {
				current_targetaddr->got_reply = _B_TRUE;
				nreceived_last_target++;
				/*
				 * Determine if stats, probe-all, and
				 * npackets != 0, and this is the reply for
				 * the last probe we sent to current target
				 * address.
				 */
				if (stats && probe_all && npackets > 0 &&
				    ((current_targetaddr->starting_seq_num +
				    current_targetaddr->num_probes - 1) %
				    (MAX_PORT + 1) == ntohs(up->uh_dport)) &&
				    (current_targetaddr->num_probes ==
				    current_targetaddr->num_sent))
					last_reply_from_targetaddr = _B_TRUE;
			} else {
				/*
				 * If it's just probe_all and we just received
				 * a reply from a target address we were
				 * probing and had timed out (now we are probing
				 * some other target address), we ignore
				 * this reply.
				 */
				if (probe_all && !stats) {
					valid_reply = _B_FALSE;
					/*
					 * Only if it's verbose, we get a
					 * message regarding this reply,
					 * otherwise we are done here.
					 */
					if (!verbose) {
						(void) sigrelse(SIGALRM);
						return;
					}
				}
			}
		}

		if (valid_reply && !stats) {
			/*
			 * if we are still sending to the same target address,
			 * then stop it, because we know it's alive.
			 */
			if (reply_matched_current_target) {
				(void) alarm(0);	/* cancel alarm */
				(void) sigset(SIGALRM, SIG_IGN);
				current_targetaddr->probing_done = _B_TRUE;
			}
			(void) sigrelse(SIGALRM);

			if (!probe_all) {
				Printf("%s is alive\n", targethost);
			} else {
				(void) inet_ntop(AF_INET6,
				    (void *)&ip6h->ip6_dst,
				    tmp_buf, sizeof (tmp_buf));
				if (nflag) {
					Printf("%s is alive\n", tmp_buf);
				} else {
					Printf("%s (%s) is alive\n",
					    targethost, tmp_buf);
				}
			}
			if (reply_matched_current_target) {
				/*
				 * Let's get things going again, but now
				 * ping will start sending to next target IP
				 * address.
				 */
				send_scheduled_probe();
				(void) sigset(SIGALRM, sigalrm_handler);
				schedule_sigalrm();
			}
			return;
		} else {
			/*
			 * If we are not moving to next targetaddr, let's
			 * release the SIGALRM now. We don't want to stall in
			 * the middle of probing a targetaddr if the pr_name()
			 * call (see below) takes longer.
			 */
			if (!last_reply_from_targetaddr)
				(void) sigrelse(SIGALRM);
			/* else, we'll release it later */
		}

		dst_addr.addr6 = ip6h->ip6_dst;
		if (valid_reply) {
			Printf("%d bytes from %s: ", cc,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));
			Printf("udp_port=%d. ", ntohs(up->uh_dport));
			print_newline = _B_TRUE;
		} else if (is_a_target(hi_dst6, &dst_addr)|| verbose) {
			if (icmp6->icmp6_code >= A_CNT(unreach6)) {
				Printf("ICMPv6 %d Unreachable from gateway "
				    "%s\n", icmp6->icmp6_code,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			} else {
				Printf("ICMPv6 %s from gateway %s\n",
				    unreach6[icmp6->icmp6_code],
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			Printf(" for %s from %s", pr_protocol(last_hdr),
			    pr_name((char *)&ip6h->ip6_src, AF_INET6));
			Printf(" to %s", pr_name((char *)&ip6h->ip6_dst,
			    AF_INET6));
			if (last_hdr == IPPROTO_TCP || last_hdr == IPPROTO_UDP)
				Printf(" port %d ", ntohs(up->uh_dport));
			print_newline = _B_TRUE;
		}

		/*
		 * Update and print the stats, if it's a valid reply and
		 * contains a timestamp.
		 */
		if (valid_reply && datalen >= sizeof (struct timeval) &&
		    cc_left >= sizeof (struct timeval)) {
			tp = (struct timeval *)((char *)up +
			    sizeof (struct udphdr));
			(void) tvsub(&tv, tp);
			triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
			Printf("time=%d. ms", triptime);
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
			print_newline = _B_TRUE;
		}
		if (print_newline)
			(void) putchar('\n');
		/*
		 * If it's stats, probe-all, npackets > 0, and we received reply
		 * for the last probe sent to this target address, then we
		 * don't need to wait anymore, let's move on to next target
		 * address, now!
		 */
		if (last_reply_from_targetaddr) {
			(void) alarm(0);	/* cancel alarm */
			current_targetaddr->probing_done = _B_TRUE;
			(void) sigrelse(SIGALRM);
			send_scheduled_probe();
			schedule_sigalrm();
		}
		break;

	case ICMP6_PACKET_TOO_BIG:
		ip6h = (ip6_t *)((char *)icmp6 + ICMP6_MINLEN);
		if (cc_left < sizeof (ip6_t)) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc, pr_name((char *)&from6->sin6_addr,
				    AF_INET6));
			}
			return;
		}
		ip6hdr_len = IPv6_hdrlen(ip6h, cc_left, &last_hdr);

		dst_addr.addr6 = ip6h->ip6_dst;
		if (is_a_target(hi_dst6, &dst_addr) || verbose) {
			Printf("ICMPv6 %d packet too big from %s\n",
			    icmp6->icmp6_code,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));

			Printf(" for %s from %s", pr_protocol(last_hdr),
			    pr_name((char *)&ip6h->ip6_src, AF_INET6));
			Printf(" to %s", pr_name((char *)&ip6h->ip6_dst,
			    AF_INET6));
			if ((last_hdr == IPPROTO_TCP ||
			    last_hdr == IPPROTO_UDP) &&
			    (cc_left >= (ip6hdr_len + 4))) {
				up = (struct udphdr *)
				    ((char *)ip6h + ip6hdr_len);
				Printf(" port %d ", ntohs(up->uh_dport));
			}
			Printf(" MTU = %d\n", ntohl(icmp6->icmp6_mtu));
		}
		break;

	case ICMP6_TIME_EXCEEDED:
		ip6h = (ip6_t *)((char *)icmp6 + ICMP6_MINLEN);
		if (cc_left < sizeof (ip6_t)) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			return;
		}
		ip6hdr_len = IPv6_hdrlen(ip6h, cc_left, &last_hdr);

		dst_addr.addr6 = ip6h->ip6_dst;
		if (is_a_target(hi_dst6, &dst_addr) || verbose) {
			if (icmp6->icmp6_code >= A_CNT(timexceed6)) {
				Printf("ICMPv6 %d time exceeded from %s\n",
				    icmp6->icmp6_code,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			} else {
				Printf("ICMPv6 %s from %s\n",
				    timexceed6[icmp6->icmp6_code],
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			Printf(" for %s from %s", pr_protocol(last_hdr),
			    pr_name((char *)&ip6h->ip6_src, AF_INET6));
			Printf(" to %s", pr_name((char *)&ip6h->ip6_dst,
			    AF_INET6));
			if ((last_hdr == IPPROTO_TCP ||
			    last_hdr == IPPROTO_UDP) &&
			    (cc_left >= (ip6hdr_len + 4))) {
				up = (struct udphdr *)
				    ((char *)ip6h + ip6hdr_len);
				Printf(" port %d", ntohs(up->uh_dport));
			}
			(void) putchar('\n');
		}
		break;

	case ICMP6_PARAM_PROB:
		ip6h = (ip6_t *)((char *)icmp6 + ICMP6_MINLEN);
		if (cc_left < sizeof (ip6_t)) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			return;
		}
		ip6hdr_len = IPv6_hdrlen(ip6h, cc_left, &last_hdr);

		dst_addr.addr6 = ip6h->ip6_dst;
		if (is_a_target(hi_dst6, &dst_addr) || verbose) {
			if (icmp6->icmp6_code >= A_CNT(param_prob6)) {
				Printf("ICMPv6 %d parameter problem from %s\n",
				    icmp6->icmp6_code,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			} else {
				Printf("ICMPv6 %s from %s\n",
				    param_prob6[icmp6->icmp6_code],
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			Printf(" in byte %d", icmp6->icmp6_pptr);
			if (icmp6->icmp6_pptr <= ip6hdr_len) {
				Printf(" (value 0x%x)",
				    *((char *)ip6h + icmp6->icmp6_pptr));
			}
			Printf(" for %s from %s", pr_protocol(last_hdr),
			    pr_name((char *)&ip6h->ip6_src, AF_INET6));
			Printf(" to %s", pr_name((char *)&ip6h->ip6_dst,
			    AF_INET6));
			if ((last_hdr == IPPROTO_TCP ||
			    last_hdr == IPPROTO_UDP) &&
			    (cc_left >= (ip6hdr_len + 4))) {
				up = (struct udphdr *)
				    ((char *)ip6h + ip6hdr_len);
				Printf(" port %d", ntohs(up->uh_dport));
			}
			(void) putchar('\n');
		}
		break;

	case ICMP6_ECHO_REQUEST:
		return;

	case ICMP6_ECHO_REPLY:
		if (ntohs(icmp6->icmp6_id) == ident) {
			if (!use_udp)
				valid_reply = _B_TRUE;
			else
				valid_reply = _B_FALSE;
		} else {
			return;
		}

		if (valid_reply) {
			/*
			 * For this valid reply, if we are still sending to
			 * this target IP address, we'd like to do some
			 * updates to targetaddr, so hold SIGALRMs.
			 */
			(void) sighold(SIGALRM);
			is_alive = _B_TRUE;
			nreceived++;
			reply_matched_current_target =
			    seq_match(current_targetaddr->starting_seq_num,
				current_targetaddr->num_sent,
				ntohs(icmp6->icmp6_seq));
			if (reply_matched_current_target) {
				current_targetaddr->got_reply = _B_TRUE;
				nreceived_last_target++;
				/*
				 * Determine if stats, probe-all, and
				 * npackets != 0, and this is the reply for
				 * the last probe we sent to current target
				 * address.
				 */
				if (stats && probe_all && npackets > 0 &&
				    ((current_targetaddr->starting_seq_num +
				    current_targetaddr->num_probes - 1) %
				    (MAX_ICMP_SEQ + 1) ==
				    ntohs(icmp6->icmp6_seq)) &&
				    (current_targetaddr->num_probes ==
				    current_targetaddr->num_sent))
					last_reply_from_targetaddr = _B_TRUE;
			} else {
				/*
				 * If it's just probe_all and we just received
				 * a reply from a target address we were
				 * probing and had timed out (now we are probing
				 * some other target address), we ignore
				 * this reply.
				 */
				if (probe_all && !stats) {
					valid_reply = _B_FALSE;
					/*
					 * Only if it's verbose, we get a
					 * message regarding this reply,
					 * otherwise we are done here.
					 */
					if (!verbose) {
						(void) sigrelse(SIGALRM);
						return;
					}
				}
			}
		}

		if (!stats && valid_reply) {
			/*
			 * if we are still sending to the same target address,
			 * then stop it, because we know it's alive.
			 */
			if (reply_matched_current_target) {
				(void) alarm(0);	/* cancel alarm */
				(void) sigset(SIGALRM, SIG_IGN);
				current_targetaddr->probing_done = _B_TRUE;
			}
			(void) sigrelse(SIGALRM);

			if (!probe_all) {
				Printf("%s is alive\n", targethost);
			} else {
				/*
				 * If we are using send_reply, the real
				 * target address is not the src address of the
				 * replies. Use icmp_seq to find out where this
				 * probe was sent to.
				 */
				if (send_reply) {
					(void) find_dstaddr(
					    ntohs(icmp6->icmp6_seq), &dst_addr);
					(void) inet_ntop(AF_INET6,
					    (void *)&dst_addr.addr6,
					    tmp_buf, sizeof (tmp_buf));
				} else {
					(void) inet_ntop(AF_INET6,
					    (void *)&from6->sin6_addr,
					    tmp_buf, sizeof (tmp_buf));
				}

				if (nflag) {
					Printf("%s is alive\n", tmp_buf);
				} else {
					Printf("%s (%s) is alive\n",
					    targethost, tmp_buf);
				}
			}
			if (reply_matched_current_target) {
				/*
				 * Let's get things going again, but now
				 * ping will start sending to next target IP
				 * address.
				 */
				send_scheduled_probe();
				(void) sigset(SIGALRM, sigalrm_handler);
				schedule_sigalrm();
			}
			return;
		} else {
			/*
			 * If we are not moving to next targetaddr, let's
			 * release the SIGALRM now. We don't want to stall in
			 * the middle of probing a targetaddr if the pr_name()
			 * call (see below) takes longer.
			 */
			if (!last_reply_from_targetaddr)
				(void) sigrelse(SIGALRM);
			/* else, we'll release it later */
		}

		/*
		 * If we are using send_reply, the real target address is
		 * not the src address of the replies. Use icmp_seq to find out
		 * where this probe was sent to.
		 */
		if (send_reply) {
			(void) find_dstaddr(ntohs(icmp6->icmp6_seq), &dst_addr);
			Printf("%d bytes from %s: ", cc,
			    pr_name((char *)&dst_addr.addr6,  AF_INET6));
		} else {
			Printf("%d bytes from %s: ", cc,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));
		}
		Printf("icmp_seq=%d. ", ntohs(icmp6->icmp6_seq));

		if (valid_reply && datalen >= sizeof (struct timeval) &&
		    cc_left >= sizeof (struct timeval)) {
			tp = (struct timeval *)&icmp6->icmp6_data16[2];
			(void) tvsub(&tv, tp);
			triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
			Printf("time=%d. ms", triptime);
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}
		(void) putchar('\n');
		/*
		 * If it's stats, probe-all, npackets > 0, and we received reply
		 * for the last probe sent to this target address, then we
		 * don't need to wait anymore, let's move on to next target
		 * address, now!
		 */
		if (last_reply_from_targetaddr) {
			(void) alarm(0);	/* cancel alarm */
			current_targetaddr->probing_done = _B_TRUE;
			(void) sigrelse(SIGALRM);
			send_scheduled_probe();
			schedule_sigalrm();
		}
		break;

	case ICMP6_MEMBERSHIP_QUERY:
	case ICMP6_MEMBERSHIP_REPORT:
	case ICMP6_MEMBERSHIP_REDUCTION:
	case ND_ROUTER_SOLICIT:
	case ND_ROUTER_ADVERT:
	case ND_NEIGHBOR_SOLICIT:
	case ND_NEIGHBOR_ADVERT:
		return;

	case ND_REDIRECT:
		nd_rdrct = (nd_redirect_t *)icmp6;

		if (cc_left < sizeof (nd_redirect_t) - ICMP6_MINLEN) {
			if (verbose) {
				Printf("packet too short (%d bytes) from %s\n",
				    cc,
				    pr_name((char *)&from6->sin6_addr,
					AF_INET6));
			}
			return;
		}
		dst_addr.addr6 = nd_rdrct->nd_rd_dst;
		if (is_a_target(hi_dst6, &dst_addr) || verbose) {
			Printf("ICMPv6 redirect from gateway %s\n",
			    pr_name((char *)&from6->sin6_addr, AF_INET6));

			Printf(" to %s",
			    pr_name((char *)&nd_rdrct->nd_rd_target, AF_INET6));
			Printf(" for %s\n",
			    pr_name((char *)&nd_rdrct->nd_rd_dst, AF_INET6));
		}
		break;

	default:
		if (verbose) {
			Printf("%d bytes from %s:\n", cc,
			    pr_name((char *)&from6->sin6_addr, AF_INET6));
			Printf("icmp6_type=%d (%s) ", icmp6->icmp6_type,
			    pr_type6(icmp6->icmp6_type));
			Printf("icmp6_code=%d\n", icmp6->icmp6_code);
			for (i = 0; i < 12; i++) {
				Printf("x%2.2x: x%8.8x\n",
				    i * sizeof (int32_t), *intp++);
			}
		}
		break;
	}

	/*
	 * If it's verbose mode and we recv'd ancillary data, print extension
	 * headers.
	 */
	if (verbose && msg->msg_controllen > 0)
		pr_ext_headers(msg);
}

/*
 * Convert an ICMP6 "type" field to a printable string.
 */
static char *
pr_type6(uchar_t icmp6_type)
{
	static struct icmptype_table ttab6[] = {
		{ICMP6_DST_UNREACH,		"Dest Unreachable"},
		{ICMP6_PACKET_TOO_BIG,		"Packet Too Big"},
		{ICMP6_TIME_EXCEEDED,		"Time Exceeded"},
		{ICMP6_PARAM_PROB,		"Parameter Problem"},
		{ICMP6_ECHO_REQUEST,		"Echo Request"},
		{ICMP6_ECHO_REPLY,		"Echo Reply"},
		{ICMP6_MEMBERSHIP_QUERY,	"Multicast Listener Query"},
		{ICMP6_MEMBERSHIP_REPORT,	"Multicast Listener Report"},
		{ICMP6_MEMBERSHIP_REDUCTION,	"Multicast Listener Done"},
		{ND_ROUTER_SOLICIT,		"Router Solicitation"},
		{ND_ROUTER_ADVERT,		"Router Advertisement"},
		{ND_NEIGHBOR_SOLICIT,		"Neighbor Solicitation"},
		{ND_NEIGHBOR_ADVERT,		"Neighbor Advertisement"},
		{ND_REDIRECT,			"Redirect Message"},
        };
	int i;

	for (i = 0; i < A_CNT(ttab6); i++) {
		if (ttab6[i].type == icmp6_type)
			return (ttab6[i].message);

	}

	return ("OUT-OF-RANGE");
}

/*
 * Return the length of the IPv6 related headers (including extension headers).
 * It also sets the *last_hdr_rtrn to the first upper layer protocol header
 * following IPv6 header and extension headers. If print_flag is _B_TRUE, it
 * prints extension headers.
 */
static int
IPv6_hdrlen(ip6_t *ip6h, int pkt_len, uint8_t *last_hdr_rtrn)
{
	int length;
	int exthdrlength;
	uint8_t nexthdr;
	uint8_t *whereptr;
	ip6_hbh_t *hbhhdr;
	ip6_dest_t *desthdr;
	ip6_rthdr_t *rthdr;
	ip6_frag_t *fraghdr;
	uint8_t	*endptr;

	length = sizeof (ip6_t);

	whereptr = ((uint8_t *)&ip6h[1]); 	/* point to next hdr */
	endptr = ((uint8_t *)ip6h) + pkt_len;

	nexthdr = ip6h->ip6_nxt;
	*last_hdr_rtrn = IPPROTO_NONE;

	if (whereptr >= endptr)
		return (length);

	while (whereptr < endptr) {
		*last_hdr_rtrn = nexthdr;
		switch (nexthdr) {
		case IPPROTO_HOPOPTS:
			hbhhdr = (ip6_hbh_t *)whereptr;
			exthdrlength = 8 * (hbhhdr->ip6h_len + 1);
			if ((uchar_t *)hbhhdr + exthdrlength > endptr)
				return (length);
			nexthdr = hbhhdr->ip6h_nxt;
			length += exthdrlength;
			break;

		case IPPROTO_DSTOPTS:
			desthdr = (ip6_dest_t *)whereptr;
			exthdrlength = 8 * (desthdr->ip6d_len + 1);
			if ((uchar_t *)desthdr + exthdrlength > endptr)
				return (length);
			nexthdr = desthdr->ip6d_nxt;
			length += exthdrlength;
			break;

		case IPPROTO_ROUTING:
			rthdr = (ip6_rthdr_t *)whereptr;
			exthdrlength = 8 * (rthdr->ip6r_len + 1);
			if ((uchar_t *)rthdr + exthdrlength > endptr)
				return (length);
			nexthdr = rthdr->ip6r_nxt;
			length += exthdrlength;
			break;

		case IPPROTO_FRAGMENT:
			fraghdr = (ip6_frag_t *)whereptr;
			if ((uchar_t *)&fraghdr[1] > endptr)
				return (length);
			nexthdr = fraghdr->ip6f_nxt;
			length += sizeof (struct ip6_frag);
			break;

		case IPPROTO_NONE:
		default:
			return (length);
		}
		whereptr = (uint8_t *)ip6h + length;
	}
	*last_hdr_rtrn = nexthdr;

	return (length);
}

/*
 * Print extension headers
 */
static void
pr_ext_headers(struct msghdr *msg)
{
	struct cmsghdr *cmsg;

	Printf("  IPv6 extension headers: ");

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IPV6) {
			switch (cmsg->cmsg_type) {
			case IPV6_HOPOPTS:
				Printf(" <hop-by-hop options>");
				break;

			case IPV6_DSTOPTS:
				Printf(" <destination options (after routing"
				    "header)>");
				break;

			case IPV6_RTHDRDSTOPTS:
				Printf(" <destination options (before routing"
				    "header)>");
				break;

			case IPV6_RTHDR:
				pr_rthdr((uchar_t *)CMSG_DATA(cmsg));
				break;

			default:
				Printf(" <option type %d>", cmsg->cmsg_type);
				break;
			}
		}
	}
	(void) putchar('\n');
}

/*
 * Print the routing header 0 information
 */
static void
pr_rthdr(uchar_t *buf)
{
	ip6_rthdr_t *rthdr;
	ip6_rthdr0_t *rthdr0;
	struct in6_addr *gw_addr;
	int i, num_addr;

	rthdr = (ip6_rthdr_t *)buf;
	Printf(" <type %d routing header, segleft %u> ",
	    rthdr->ip6r_type, rthdr->ip6r_segleft);

	if (rthdr->ip6r_type == 0) {
		rthdr0 = (ip6_rthdr0_t *)buf;
		gw_addr = rthdr0->ip6r0_addr;
		num_addr = rthdr0->ip6r0_len / 2;

		for (i = 0; i < num_addr; i++) {
			Printf("%s", pr_name((char *)gw_addr, AF_INET6));
			if (i == (num_addr - rthdr0->ip6r0_segleft))
				Printf("(Current)");
			gw_addr++;
			if (i != num_addr - 1)
				Printf(",  ");
		}
	}
}
