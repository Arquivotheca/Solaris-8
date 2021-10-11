/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)logging.c	1.2	99/08/27 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include <netdb.h>
#include "dhcpd.h"

static char	*dhcp_msg_cats[] = {
	/* L_ASSIGN */		"ASSIGN",
	/* L_REPLY */		"EXTEND",
	/* L_RELEASE */		"RELEASE",
	/* L_DECLINE */		"DECLINE",
	/* L_INFORM */		"INFORM",
	/* L_NAK */		"NAK",
	/* L_ICMP_ECHO */	"ICMP-ECHO",
	/* L_RELAY_REQ */	"RELAY-SRVR",
	/* L_RELAY_REP */	"RELAY-CLNT"
};

static char	*protos[] = {
	/* P_BOOTP */		"BOOTP",
	/* P_DHCP */		"DHCP"
};

/*
 * Transaction logging. Note - if we're in debug mode, the transactions
 * are logged to the console!
 */
void
logtrans(DHCP_PROTO p, DHCP_MSG_CATEGORIES type, time_t lease,
    struct in_addr cip, struct in_addr sip, PKT_LIST *plp)
{
	char	*cat, *proto, *t, *class_id;
	int	maclen;
	char	class_idbuf[DHCP_MAX_OPT_SIZE];
	char	cidbuf[DHCP_MAX_OPT_SIZE];
	char	ntoabc[NTOABUF], ntoabs[NTOABUF];
	char	macbuf[(sizeof (((PKT *)NULL)->chaddr) * 2) + 1];

	if (log_local < 0)
		return;

	proto = protos[p];
	cat = dhcp_msg_cats[type];

	(void) disp_cid(plp, cidbuf, sizeof (cidbuf));

	class_id = get_class_id(plp, class_idbuf, sizeof (class_idbuf));

	/* convert white space in class id into periods (.) */
	if (class_id != NULL) {
		for (t = class_id; *t != '\0'; t++) {
			if (isspace(*t))
				*t = '.';
		}
	} else
		class_id = "N/A";

	maclen = sizeof (macbuf);
	macbuf[0] = '\0';
	(void) octet_to_ascii(plp->pkt->chaddr, plp->pkt->hlen, macbuf,
	    &maclen);

	dhcpmsg(log_local | LOG_NOTICE, "%s %s %010d %010d %s %s %s %s %s\n",
	    proto, cat, time(NULL), lease, inet_ntoa_r(cip, ntoabc),
	    inet_ntoa_r(sip, ntoabs), cidbuf, class_id, macbuf);
}
