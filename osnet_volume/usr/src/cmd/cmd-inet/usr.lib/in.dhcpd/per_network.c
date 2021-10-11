#ident	"@(#)per_network.c	1.40	99/08/03 SMI"

/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include <locale.h>

/*
 * This file contains the access routines for the dhcp-network databases.
 */
static int ascii_to_pnrec(PER_NET_DB *, enum pn_type, void *, PN_REC *);
static int pnrec_to_ascii(PER_NET_DB *, PN_REC *, PN_ASCII_REC *);
static void dump_pndb_tbl(PER_NET_DB *);

/*
 * Convert a Row into a PN_REC. Returns 0 for success, nonzero otherwise.
 * Doesn't release the Row.
 */
static int
ascii_to_pnrec(PER_NET_DB *pndbp, enum pn_type type, void *vp, PN_REC *pnp)
{
	char		*cp;
	uchar_t		len;
	uchar_t		*ucp;
	Row		*rowp;
	PN_ASCII_REC	*pn_asp;
	char		*save_cp;
	int		minus = 0;

	if (pnp == (PN_REC *)NULL || pndbp == (PER_NET_DB *)NULL)
		return (EINVAL);

	(void) memset((char *)pnp, 0, sizeof (PN_REC));

	/* CID, CID_LEN */
	if (type == PN_ASCII) {
		pn_asp = (PN_ASCII_REC *)vp;
		cp = pn_asp->as_cid;
	} else {
		rowp = (Row *)vp;
		cp = rowp->ca[0];
	}
	save_cp = cp;
	len = strlen(cp);
	ucp = get_octets((uchar_t **)&cp, &len);
	if (ucp == (uchar_t *)NULL) {
		dhcpmsg(LOG_ERR,
		    "Bad client id: %s in dhcp-network database: %s\n",
		    save_cp, pndbp->name);
		return (EINVAL);
	}
	pnp->cid_len = len;
	(void) memcpy(pnp->cid, ucp, len);
	free(ucp);

	/* FLAGS */
	if (type == PN_ASCII)
		cp = pn_asp->as_flags;
	else
		cp = rowp->ca[1];
	save_cp = cp;
	if (get_number(&cp, &pnp->flags, 1) != 0) {
		dhcpmsg(LOG_ERR,
		    "Bad flags field in dhcp-network database: %s. Value: %s\n",
		    pndbp->name, save_cp);
		return (EINVAL);
	}

	/* CLIENTIP */
	if (type == PN_ASCII)
		cp = pn_asp->as_clientip;
	else
		cp = rowp->ca[2];
	if ((pnp->clientip.s_addr = inet_addr(cp)) == (ipaddr_t)-1) {
		dhcpmsg(LOG_ERR,
		    "Bad client IP in dhcp-network database: %s. Value: %s\n",
		    pndbp->name, cp);
		return (EINVAL);
	}

	/* SERVERIP */
	if (type == PN_ASCII)
		cp = pn_asp->as_serverip;
	else
		cp = rowp->ca[3];
	if ((pnp->serverip.s_addr = inet_addr(cp)) == (ipaddr_t)-1) {
		dhcpmsg(LOG_ERR,
		    "Bad server IP in dhcp-network database: %s. Value: %s\n",
		    pndbp->name, cp);
		return (EINVAL);
	}

	/* LEASE */
	if (type == PN_ASCII)
		cp = pn_asp->as_lease;
	else
		cp = rowp->ca[4];
	save_cp = cp;
	if (*cp == '-') {
		minus = 1;
		cp++;
	}
	if (get_number(&cp, &pnp->lease, sizeof (time_t)) != 0) {
		dhcpmsg(LOG_ERR,
		    "Bad lease field in dhcp-network database: %s. Value: %s\n",
		    pndbp->name, save_cp);
		return (EINVAL);
	}

	pnp->lease = ntohl(pnp->lease);

	if (minus)
		pnp->lease = -pnp->lease;

	/* MACRO */
	if (type == PN_ASCII)
		cp = pn_asp->as_macro;
	else
		cp = rowp->ca[5];

	if (cp != NULL) {
		if ((int)strlen(cp) < DT_MAX_MACRO_LEN)
			(void) strcpy(pnp->macro, cp);
		else {
			(void) strncpy(pnp->macro, cp, DT_MAX_MACRO_LEN);
			pnp->macro[DT_MAX_MACRO_LEN] = '\0';
		}
	} else
		pnp->macro[0] = '\0';

	/* COMMENT */
	if (type == PN_ASCII)
		cp = pn_asp->as_comment;
	else
		cp = rowp->ca[6];

	if (cp != NULL) {
		if ((int)strlen(cp) < PN_MAX_COMMENT_LEN)
			(void) strcpy(pnp->comment, cp);
		else {
			(void) strncpy(pnp->comment, cp, PN_MAX_COMMENT_LEN);
			pnp->comment[PN_MAX_COMMENT_LEN] = '\0';
		}
	} else
		pnp->comment[0] = '\0';

	return (0);
}

/*
 * Convert a PN_REC into a PN_ASCII_REC. Returns 0 if successful, nonzero
 * otherwise.
 */
static int
pnrec_to_ascii(PER_NET_DB *pndbp, PN_REC *pnp, PN_ASCII_REC *pn_asp)
{
	int	err = 0;
	int	t_cidlen;
	int	len;

	if (pnp == (PN_REC *)NULL || pn_asp == (PN_ASCII_REC *)NULL)
		return (EINVAL);

	(void) memset((char *)pn_asp, 0, sizeof (PN_ASCII_REC));

	/* CID */
	if (pnp->cid_len == 0)
		t_cidlen = 1;
	else
		t_cidlen = pnp->cid_len;
	len = sizeof (pn_asp->as_cid);
	err = octet_to_ascii(pnp->cid, t_cidlen, pn_asp->as_cid, &len);
	if (err != 0) {
		dhcpmsg(LOG_ERR,
		    "Error converting client id to ASCII. Table: %s\n",
		    pndbp->name);
		return (err);
	}

	/* FLAGS */
	(void) sprintf(pn_asp->as_flags, "%02u", pnp->flags);

	/* CLIENTIP */
	(void) inet_ntoa_r(pnp->clientip, pn_asp->as_clientip);

	/* SERVERIP */
	(void) inet_ntoa_r(pnp->serverip, pn_asp->as_serverip);

	/* LEASE */
	(void) sprintf(pn_asp->as_lease, "%ld", (ulong_t)ntohl(pnp->lease));

	/* MACRO */
	(void) strcpy(pn_asp->as_macro, pnp->macro);

	/* COMMENT */
	(void) strcpy(pn_asp->as_comment, pnp->comment);

	return (0);
}

/*
 * Zap the table. Something went wrong.
 */
static void
dump_pndb_tbl(PER_NET_DB *pndbp)
{
	if (pndbp != (PER_NET_DB *)NULL && pndbp->datatype != PN_UNUSED) {
		free_dd(&pndbp->tbl);
		(void) memset(&pndbp->tbl, 0, sizeof (Tbl));
		pndbp->datatype = PN_UNUSED;
		pndbp->row = 0L;
	}
}

/*
 * Open the appropriate dhcp-network database given a network address and
 * a subnet mask. These in_addr's are expected in network order.
 *
 * Returns: 0 for success or errno if an error occurs.
 */
int
open_per_net(PER_NET_DB *pndbp, struct in_addr *net, struct in_addr *mask)
{
	ulong_t		addr;
	char		*datastore;
	int		err = 0;
	int		tbl_err, ns;
	Tbl_stat	*tbl_statp;
	char		*pathp = NULL;

	if (pndbp == (PER_NET_DB *)NULL || net == (struct in_addr *)NULL ||
	    mask == (struct in_addr *)NULL)
		return (EINVAL);

	if ((ns = dd_ns(&tbl_err, &pathp)) == TBL_FAILURE)
		return (tbl_err);

	if (ns == TBL_NS_UFS)
		datastore = "files";
	else
		datastore = "nisplus";

	addr = ntohl(net->s_addr) & ntohl(mask->s_addr);
	(void) sprintf(pndbp->name, PN_NAME_FORMAT,
	((addr & 0xff000000) >> 24), ((addr & 0x00ff0000) >> 16),
	((addr & 0x0000ff00) >> 8), (addr & 0x000000ff));


	tbl_statp = NULL;
	if (stat_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
	    &tbl_err, &tbl_statp) != TBL_SUCCESS || check_dd_access(tbl_statp,
	    &tbl_err) != 0) {
		switch (tbl_err) {
		case TBL_NO_ACCESS:
			dhcpmsg(LOG_ERR,
"No permission to access table: %1$s in %2$s (%3$s)\n",
			    pndbp->name, pathp, datastore);
			err = EACCES;
			break;
		case TBL_NO_TABLE:
			if (verbose) {
				dhcpmsg(LOG_INFO,
"A request to access nonexistent dhcp-network database: %1$s in %2$s (%3$s)\n",
				    pndbp->name, pathp, datastore);
			}
			err = ENOENT;
			break;
		default:
			if (!ethers_compat || verbose) {
				dhcpmsg(LOG_ERR,
"Open request for table  %1$s in %2$s (%3$s) failed.\n",
				    pndbp->name, pathp, datastore);
			}
			err = EINVAL;
			break;
		}
	}
	if (tbl_statp != NULL)
		free_dd_stat(tbl_statp);

	pndbp->datatype = PN_UNUSED;
	pndbp->row = 0L;
	return (err);
}

/*
 * Looks in the dhcp-network database for a record in which field 'fld'
 * matches the value 'value'. Optionally, the server id can be passed to
 * further limit the search focus. The server id is ignored on
 * PN_SERVER_IP queries.
 *
 * Returns the number of records matching search criteria for success, -1
 * if a failure occurs. Places results in the pnp structure passed in as
 * an argument.
 *
 * Notes: If the value returned is > 1, then get_per_net() can be used to
 * scan the remaining records from the query. If you call this with valid
 * cached data, the data is released.
 */
int
lookup_per_net(PER_NET_DB *pndbp, enum pn_field fld, void *val, int val_len,
    struct in_addr *sip, PN_REC *pnp)
{
	int		records = 0;
	int		tbl_err;
	char		buffer[DHCP_SCRATCH];
	char		sipbuf[NTOABUF], ntoab[NTOABUF];
	char		*fscp;
	int 		blen;
	struct in_addr	*ip;

	if (pndbp == (PER_NET_DB *)NULL || pnp == (PN_REC *)NULL)
		return (-1);

	if (pndbp->datatype != PN_UNUSED)
		dump_pndb_tbl(pndbp);
	else
		(void) memset((char *)&pndbp->tbl, 0, sizeof (Tbl));

	if (sip != (struct in_addr *)NULL) {
		(void) inet_ntoa_r(*sip, sipbuf);
		fscp = sipbuf;
	} else
		fscp = (char *)NULL;

	switch (fld) {
	case PN_CID:

		if (val_len >= DHCP_SCRATCH)
			break;

		blen = DHCP_SCRATCH;
		(void) octet_to_ascii((uchar_t *)val, val_len, buffer, &blen);

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, buffer, NULL, fscp,
		    NULL, NULL) != TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR, "Lookup by client id in \
%1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_CLIENT_IP:

		if (val_len != sizeof (struct in_addr))
			break;
		else
			ip = (struct in_addr *)val;

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, NULL, inet_ntoa_r(*ip, ntoab), fscp,
		    NULL, NULL) != TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR, "Lookup by client IP \
address in %1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_SERVER_IP:

		if (val_len != sizeof (struct in_addr))
			break;
		else
			ip = (struct in_addr *)val;

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, NULL, NULL, inet_ntoa_r(*ip, ntoab),
		    NULL, NULL) != TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR, "Lookup by server IP \
address in %1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_LEASE:

		if (val_len != sizeof (time_t))
			break;

		blen = DHCP_SCRATCH;
		(void) octet_to_ascii((uchar_t *)val, val_len, buffer, &blen);

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, NULL, NULL, fscp, buffer,
		    NULL) != TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR, "Lookup by lease time in \
%1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_MACRO:

		if (val_len > DT_MAX_MACRO_LEN + 1)
			break;

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, NULL, NULL, fscp, (char *)val,
		    NULL) != TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR, "Lookup by macro name in \
%1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_DONTCARE:

		if (list_dd(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name, NULL,
		    &tbl_err, &pndbp->tbl, NULL, NULL, fscp, NULL, NULL) !=
		    TBL_SUCCESS) {
			if (tbl_err != TBL_NO_ENTRY) {
				dhcpmsg(LOG_ERR,
				    "List of %1$s database failed: %d\n",
				    pndbp->name, tbl_err);
				records = -1;
			}
			break;
		}

		records = pndbp->tbl.rows;
		if (ascii_to_pnrec(pndbp, PN_ROW, (void *)pndbp->tbl.ra[0],
		    pnp) != 0)
			dump_pndb_tbl(pndbp);
		break;

	case PN_UNUSED:
		/* FALLTHRU */
	default:

		pndbp->datatype = PN_UNUSED;
		break;
	}

	if (records >= 0) {
		pndbp->row = 0L;
		pndbp->datatype = fld;
	}
	return (records);
}

/*
 * Simply return the next pnp.
 *  If an error occurs, the tbl is dumped. Note that lookup_per_net()
 * establishes context for this call (server id, etc). Calling get_per_net()
 * in the *wrong* context (lookup pn_field != get_per_net pn_field) results
 * in EINVAL being returned.
 */
int
get_per_net(PER_NET_DB *pndbp, enum pn_field field, PN_REC *pnp)
{
	int err = 0;

	if (pndbp == (PER_NET_DB *)NULL || field == PN_UNUSED ||
	    pnp == (PN_REC *)NULL) {
		return (EINVAL);
	}

	if (pndbp->datatype != field)
		return (EINVAL);

	pndbp->row++;
	if (pndbp->row >= pndbp->tbl.rows)
		pndbp->row = 0L;

	err = ascii_to_pnrec(pndbp, PN_ROW,
	    (void *)pndbp->tbl.ra[pndbp->row], pnp);

	if (err != 0)
		dump_pndb_tbl(pndbp);

	return (err);
}

/*
 * Write a pnp to the database. The search parameter is identified by
 * the pn_field parameter. Returns 0 for success, nonzero otherwise.
 */
int
put_per_net(PER_NET_DB *pndbp, PN_REC *pnp, enum pn_field field)
{
	PN_ASCII_REC	pn_as;
	int		err = 0;
	int		tbl_err;

	if (pndbp == (PER_NET_DB *)NULL || pnp == (PN_REC *)NULL)
		return (EINVAL);

	(void) memset(&pn_as, 0, sizeof (pn_as));
	err = pnrec_to_ascii(pndbp, pnp, &pn_as);
	if (err != 0)
		return (err);

	switch (field) {
	case PN_CID:
		err = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name,
		    NULL, &tbl_err,
		    pn_as.as_cid, NULL, NULL, NULL, NULL,
		    pn_as.as_cid, pn_as.as_flags, pn_as.as_clientip,
		    pn_as.as_serverip, pn_as.as_lease, pn_as.as_macro,
		    pn_as.as_comment);
		break;

	case PN_CLIENT_IP:
		err = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name,
		    NULL, &tbl_err,
		    NULL, pn_as.as_clientip, NULL, NULL, NULL,
		    pn_as.as_cid, pn_as.as_flags, pn_as.as_clientip,
		    pn_as.as_serverip, pn_as.as_lease, pn_as.as_macro,
		    pn_as.as_comment);
		break;

	case PN_SERVER_IP:
		err = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name,
		    NULL, &tbl_err,
		    NULL, NULL, pn_as.as_serverip, NULL, NULL,
		    pn_as.as_cid, pn_as.as_flags, pn_as.as_clientip,
		    pn_as.as_serverip, pn_as.as_lease, pn_as.as_macro,
		    pn_as.as_comment);
		break;

	case PN_LEASE:
		err = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name,
		    NULL, &tbl_err,
		    NULL, NULL, NULL, pn_as.as_lease, NULL,
		    pn_as.as_cid, pn_as.as_flags, pn_as.as_clientip,
		    pn_as.as_serverip, pn_as.as_lease, pn_as.as_macro,
		    pn_as.as_comment);
		break;

	case PN_MACRO:
		err = mod_dd_entry(TBL_DHCPIP, TBL_NS_DFLT, pndbp->name,
		    NULL, &tbl_err,
		    NULL, NULL, NULL, NULL, pn_as.as_macro,
		    pn_as.as_cid, pn_as.as_flags, pn_as.as_clientip,
		    pn_as.as_serverip, pn_as.as_lease, pn_as.as_macro,
		    pn_as.as_comment);
		break;

	case PN_UNUSED:
		/* FALLTHRU */
	default:
		return (EINVAL);

	}

	if (err != TBL_SUCCESS) {
		dhcpmsg(LOG_ERR, "Cannot write dhcp-network record \
(client IP = %1$s) to: %2$s %d\n",
		    pn_as.as_clientip, pndbp->name, tbl_err);
	} else
		dump_pndb_tbl(pndbp);	/* flush cache */

	return (err);
}

/*
 * Closes specified dhcp-network database.
 */
void
close_per_net(PER_NET_DB *pndbp)
{
	if (pndbp == (PER_NET_DB *)NULL)
		return;

	dump_pndb_tbl(pndbp);	/* dump cache */

	(void) memset(pndbp->name, 0, PN_MAX_NAME_SIZE + 1);
}
