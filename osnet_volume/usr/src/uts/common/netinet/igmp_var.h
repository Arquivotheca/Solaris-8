/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_NETINET_IGMP_VAR_H
#define	_NETINET_IGMP_VAR_H

#pragma ident	"@(#)igmp_var.h	1.16	99/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Internet Group Management Protocol (IGMP),
 * implementation-specific definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Ajit Thyagarajan, PARC, August 1994.
 *
 * MULTICAST 3.5.1.1
 */

struct igmpstat {
	uint_t	igps_rcv_total;		/* total IGMP messages received    */
	uint_t	igps_rcv_tooshort;	/* received with too few bytes	   */
	uint_t	igps_rcv_badsum;	/* received with bad checksum	   */
	uint_t	igps_rcv_queries;	/* received membership queries	   */
	uint_t	igps_rcv_badqueries;	/* received invalid queries	   */
	uint_t	igps_rcv_reports;	/* received membership reports	   */
	uint_t	igps_rcv_badreports;	/* received invalid reports	   */
	uint_t	igps_rcv_ourreports;	/* received reports for our groups */
	uint_t	igps_snd_reports;	/* sent membership reports	   */
};

#ifdef _KERNEL
struct igmpstat igmpstat;

#define	IGMP_TIMEOUT_FREQUENCY	10	/* 10 times per second */

#define	IGMP_TIMEOUT_INTERVAL	(1000/IGMP_TIMEOUT_FREQUENCY)
					/* milliseconds */
#define	MLD_TIMEOUT_INTERVAL	1 /* milliseconds */
/*
 * NOTE: BSD timer is based on fastimo, which is 200ms.
 * Solaris is based on IGMP_TIMEOUT_INTERVAL, which is 100ms.
 * Therefore, scaling factor is different, and Solaris timer value
 * is twice that of BSD's
*/

/*
 * Macro to compute a random timer value between 1 and maxticks.  We
 * generate a "random" number by adding the millisecond clock,  the
 * low-order  32-bits of the IPv6 address of the "first" interface on
 * the machine, and the multicast address being timed-out.  The 4.3
 * random() routine really ought to be available in the kernel!
 */
#define	IGMP_RANDOM_DELAY(multiaddr, ipif, maxticks)		       	\
	/* ulong_t multiaddr; ipif_t * ipif; int maxticks */		\
	((TICK_TO_MSEC(lbolt) +						\
	((ipif != NULL) ? (ipif->ipif_lcl_addr) : 0) +			\
	ntohl(multiaddr)) % (maxticks) + 1)

/*
 * Macro to compute a random timer value between 1 and maxticks.  We
 * generate a "random" number by adding the
 * low-order  32-bits of the IPv6 address of the "first" interface on
 * the machine, and the last 32 bits of the v6 multicast
 * address being timed-out.  The 4.3
 * random() routine really ought to be available in the kernel!
 */
#define	MLD_RANDOM_DELAY(multiaddr, ipif, maxticks)		       	\
	/* in6_addr_t multiaddr; ipif_t * ipif; int maxticks */		\
	((((ipif != NULL) ? (ipif->ipif_v6lcl_addr.s6_addr32[3]) : 0) +	\
	ntohl(multiaddr.s6_addr32[3])) % (maxticks) + 1)

/*
 * States for IGMPv2's leave processing
 */
#define	IGMP_OTHERMEMBER			0
#define	IGMP_IREPORTEDLAST			1

/*
 * We must remember what version the subnet's querier is.
 */
#define	IGMP_V1_ROUTER				0
#define	IGMP_V2_ROUTER				1

/*
 * Revert to new router if we haven't heard from an old router in
 * this amount of time.
 */
#define	IGMP_AGE_THRESHOLD			270	/* in seconds */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_IGMP_VAR_H */
