/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights Reserved.
 */

#ifndef	_PER_NETWORK_H
#define	_PER_NETWORK_H

#pragma ident	"@(#)per_network.h	1.17	98/10/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <dhcdata.h>

/*
 * per_network.h -- DHCP-NETWORK database definitions.
 */

typedef struct {
	u_char		cid[DT_MAX_CID_LEN];	/* opaque client id */
	u_char		cid_len;		/* Length of client id */
	u_char		flags;			/* flags field */
	struct in_addr	clientip;		/* client IP address */
	struct in_addr	serverip;		/* ours (server's) IP addr */
	time_t		lease;			/* Abs time lease expires */
	char		macro[DT_MAX_MACRO_LEN + 1];	/* Packet macro */
	char		comment[PN_MAX_COMMENT_LEN + 1]; /* record comment */
} PN_REC;

/*
 * Same as above, but instead of binary, network order octets, ASCII.
 */
typedef struct {
	char 	as_cid[DT_MAX_CID_LEN * 2 + 1];
	char	as_flags[5];
	char	as_clientip[sizeof (PN_NAME_TEMPLATE) + 1];
	char	as_serverip[sizeof (PN_NAME_TEMPLATE) + 1];
	char	as_lease[PN_LEASE_AS_SIZE];
	char	as_macro[DT_MAX_MACRO_LEN + 1];
	char	as_comment[PN_MAX_COMMENT_LEN + 1];
} PN_ASCII_REC;

enum pn_type {
	PN_ASCII =	0,	/* it's a PN_ASCII_REC */
	PN_ROW =	1	/* it's a Row */
};

enum pn_field {
	PN_CID =	0,
	PN_CLIENT_IP =	1,
	PN_SERVER_IP =	2,
	PN_LEASE =	3,
	PN_MACRO =	4,
	PN_UNUSED =	5,
	PN_DONTCARE =	6
};

/*
 * DHCP offer list. One list per active interface.
 */
typedef	struct offer {
	PN_REC		pn;		/* Record w/ Abs time */
	time_t		stamp;		/* timeout value */
	struct in_addr	netip;		/* IP address of client's network */
	struct offer	*next;		/* next offer */
} OFFLST;

typedef	struct {
	char		name[PN_MAX_NAME_SIZE + 1];	/* name */
	enum pn_field	datatype;			/* type of query */
	Tbl		tbl;				/* data returned */
	u_long		row;				/* current row # */
} PER_NET_DB;

/*
 * Per Network database access routines.
 */
extern int	open_per_net(PER_NET_DB *, struct in_addr *,
    struct in_addr *);
extern int	lookup_per_net(PER_NET_DB *, enum pn_field, void *, int,
    struct in_addr *, PN_REC *);
extern int	get_per_net(PER_NET_DB *, enum pn_field, PN_REC *);
extern int	put_per_net(PER_NET_DB *, PN_REC *, enum pn_field);
extern void	close_per_net(PER_NET_DB *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PER_NETWORK_H */
