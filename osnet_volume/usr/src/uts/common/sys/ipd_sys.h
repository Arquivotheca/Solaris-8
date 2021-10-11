/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_IPD_SYS_H
#define	_SYS_IPD_SYS_H

#pragma ident	"@(#)ipd_sys.h	1.10	98/06/11 SMI"

#include <netinet/in.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * structure describing one upper stream
 */
struct ipd_str {
	struct ipd_str		*st_next;	/* next in list */
	queue_t			*st_rq;		/* read queue pointer */
	struct ipd_softc	*st_ifp;	/* attached interface */
	uint_t			st_state;	/* DLPI state */
	uint_t			st_sap;		/* sap attached */
	minor_t			st_minor;	/* corresponding minor */
	uint_t 			dst;		/* destination address */
	char			st_type;	/* IP/dialup interface type */
	char			st_raw;		/* IP/dialup raw flag */
	char			st_all;		/* IP/dialup promiscuous flag */
};

/*
 * structure describing one IP interface
 */
struct ipd_softc {
	dev_info_t		*if_dip;	/* device instance */
	char			*if_name;	/* interface name */
	char			if_unit;	/* interface unit number */
	char			if_type;	/* IP/dialup interface type */
	uint_t			if_ipackets;	/* packets in */
	uint_t			if_ierrors;	/* errors in */
	uint_t			if_opackets;	/* packets out */
	uint_t			if_oerrors;	/* errors out */
	uint_t			if_nocanput;	/* canput failed */
	uint_t			if_allocbfail;	/* allocb failed */
	kstat_t			*if_stats;	/* kernel statistics */
	struct ipd_addr_tbl	*if_conn;	/* connection details */
	struct ipd_softc	*if_next;	/* next in list */
	queue_t			*to_cnxmgr;	/* Cnx mgr queue */
};

/*
 * structure describing one connection (lower stream)
 */
struct ipd_addr_tbl	{
	struct ipd_addr_tbl	*next;		/* ptr to next in list */
	struct ipd_addr_tbl	*prev;		/* ptr to prev in list */
	ipaddr_t 		dst;		/* destination address */
	int 			mux_id;		/* lower stream mux id */
	queue_t			*rq;		/* ptr to attached stream */
	mblk_t			*pkt;		/* short term holding queue */
	int			act_count;	/* activity counter */
	timestruc_t		act_time;	/* activity timer */
	int			timeout;	/* timeout counter */
	int			addr_timeout;	/* user specified timeout */
	struct ipd_softc	*ifp;		/* pointer to the ipd_softc */
};

/*
 * structure describing interface statistics
 */
struct ipd_stat {
	kstat_named_t		ipd_ipackets;
	kstat_named_t		ipd_ierrors;
	kstat_named_t		ipd_opackets;
	kstat_named_t		ipd_oerrors;
	kstat_named_t		ipd_nocanput;
	kstat_named_t		ipd_allocbfail;
};

struct ipdcm_minor_info {
	queue_t	*rq;
	int	registree;
};

/*
 * IP pseudo device default names
 */
#define	IPDCM			"ipdcm"
#define	IPD_MTP_NAME		"ipd"
#define	IPD_PTP_NAME		"ipdptp"

/*
 * sap length
 */
#define	IPD_SAPL		0

/*
 * IP address length from layer above
 */
#ifndef	IP_ADDR_LEN
#define	IP_ADDR_LEN		4
#endif

/*
 * address length - 4 for AF_INET, 0 for none (point-to-point links)
 */
#define	IPD_MTP_ADDRL		IP_ADDR_LEN
#define	IPD_PTP_ADDRL		0

/*
 * Maximum transmission unit size, based on PPP recommended maximum size
 * of information field
 */
#define	IPD_MTU			(8232)

/*
 * Amount of space to reserve for a lower-layer datalink header
 */
#define	IPD_LINKHDR		(4)

/*
 * Maximum number of packets to queue when waiting for a connection
 */
#define	IPD_MAX_PKTS		(10)

/*
 * define a special timeout value which indicates a static link
 */
#define	IPD_IF_STATIC	(-1)

/*
 * minor device number for connection manager access node
 */
#define	IPD_MINOR 		(0)

/*
 * Maximum time to wait for connection manager to respond
 */
#define	IPD_HOLDING_TIME	(10)

/*
 * Maximum number of point-to-point and multipoint interfaces to have
 * IPD_MAXIFS of each allowed
 */
#define	IPD_MAXIFS (64)

/*
 * Maximum number of minor devices for ipdcm device= maximum number
 * of cnx managers.
 */

#define	IPD_MAXIPDCMS (20)

/*
 * debugging defines
 */
#define	IPD_DLPI	1
#define	IPD_FLOWCTRL	2
#define	IPD_DDI		4
#define	IPD_DATA	8

#ifdef IPD_DEBUG_MSGS
#define	IPD_DLPI2(s, t, q)	if (ipd_debug & IPD_DLPI) { \
					printf(s, t, q); \
				}
#define	IPD_DLPI3(s, t, q, u)	if (ipd_debug & IPD_DLPI) { \
					printf(s, t, q, u); \
				}
#define	IPD_DDIDEBUG(s)		if (ipd_debug & IPD_DDI) { \
					printf(s); \
				}
#define	IPD_DDIDEBUG1(s, t)	if (ipd_debug & IPD_DDI) { \
					printf(s, t); \
				}
#define	IPD_DDIDEBUG2(s, t, v)	if (ipd_debug & IPD_DDI) { \
					printf(s, t, v); \
				}
#define	IPD_FLDEBUG1(s, t)	if (ipd_debug & IPD_FLOWCTRL) { \
					printf(s, t); \
				}
#define	IPD_DATADBG1(s, t)	if (ipd_debug & IPD_DATA) { \
					printf(s, t); \
				}
#define	IPD_DEBUG(s)    if (ipd_debug) printf(s)
#define	IPD_DEBUG1(s, t)    if (ipd_debug) printf(s, t)
#define	IPD_DEBUG2(s, t, q)    if (ipd_debug) printf(s, t, q)

#else
#define	IPD_DLPI2(s, t, q)
#define	IPD_DLPI3(s, t, q, u)
#define	IPD_DDIDEBUG(s)
#define	IPD_DDIDEBUG1(s, t)
#define	IPD_DDIDEBUG2(s, t, v)
#define	IPD_FLDEBUG1(s, t)
#define	IPD_DATADBG1(s, t)
#define	IPD_DEBUG(s)
#define	IPD_DEBUG1(s, t)
#define	IPD_DEBUG2(s, t, q)

#endif /* IPD_DEBUG_MSGS */

/*
 * DLPI support code and general STREAMS stuff
 */
struct  ipdpriminfo {
	int	pi_minlen;		/* minimum primitive length */
	uint_t	pi_state;		/* acceptable starting state */
	int	(*pi_funcp)();		/* function() to call */
};

#ifndef DL_MAXPRIM
#define	DL_MAXPRIM DL_GET_STATISTICS_ACK
#endif
#define	MTYPE(mp)	(mp->b_datap->db_type)
#define	EQUAL(a1, a2) \
	(bcmp((a1), (a2), (IP_ADDR_LEN)) == 0)
#define	GETSTRUCT(structure, number) \
	(kmem_zalloc(sizeof (structure) * (number), KM_NOSLEEP))
#define	GETBUF(structure, size) \
	(kmem_zalloc(size, KM_NOSLEEP))

#define	DRIVER_IS(dev, name) \
	(strcmp(ddi_major_to_name(getmajor(dev)), name) == 0)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IPD_SYS_H */
