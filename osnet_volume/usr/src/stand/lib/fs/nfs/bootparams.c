/*
 * Copyright (c) 1991, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootparams.c	1.24	99/02/23 SMI"

/*
 * This file contains routines responsible for getting the system's
 * name and boot params. Most of it comes from the SVR4 diskless boot
 * code (dlboot_inet), modified to work in a non socket environment.
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <errno.h>
#include <rpc/auth.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <sys/promif.h>
#include "bootparam.h"
#include "pmap.h"
#include "rpc.h"
#include "socket_inet.h"
#include "ipv4.h"
#include <sys/salib.h>
#include <sys/bootdebug.h>

static struct bp_whoami_res	bp;
static char			bp_hostname[SYS_NMLN+1];
static char			bp_domainname[SYS_NMLN+1];
static struct in_addr		responder; /* network order */

static char *noserver =
	"No bootparam (%s) server responding; still trying...\n";

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * Returns TRUE if it has set the global structure 'bp' to our boot
 * parameters, FALSE if some failure occurred.
 */
bool_t
whoami(void)
{
	struct bp_whoami_arg	arg;
	struct sockaddr_in	to, from;
	struct in_addr		ipaddr;
	enum clnt_stat		stat;
	bool_t			retval = TRUE;
	int			rexmit;		/* retransmission interval */
	int			resp_wait;	/* secs to wait for resp */
	int			namelen;
	int			printed_waiting_msg;

	/*
	 * Set our destination IP address to the limited broadcast address
	 * (INADDR_BROADCAST).
	 */
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	to.sin_port = htons(0);

	/*
	 * Set up the arguments expected by bootparamd.
	 */
	arg.client_address.address_type = IP_ADDR_TYPE;
	ipv4_getipaddr(&ipaddr);
	ipaddr.s_addr = htonl(ipaddr.s_addr);
	bcopy((caddr_t)&ipaddr, (caddr_t)&arg.client_address.bp_address.ip_addr,
	    sizeof (ipaddr));

	/*
	 * Retransmit/wait for up to resp_wait secs.
	 */
	rexmit = 0;	/* start at default retransmission interval. */
	resp_wait = 16;

	bp.client_name = &bp_hostname[0];
	bp.domain_name = &bp_domainname[0];

	/*
	 * Do a broadcast call to find a bootparam daemon that
	 * will tell us our hostname, domainname and any
	 * router that we have to use to talk to our NFS server.
	 */
	printed_waiting_msg = 0;
	do {
		/*
		 * First try the SunOS portmapper and if no reply is
		 * received will then try the SVR4 rpcbind.
		 * Either way, `bootpaddr' will be set to the
		 * correct address for the bootparamd that responds.
		 */
		stat = bpmap_rmtcall((rpcprog_t)BOOTPARAMPROG,
		    (rpcvers_t)BOOTPARAMVERS, (rpcproc_t)BOOTPARAMPROC_WHOAMI,
		    xdr_bp_whoami_arg, (caddr_t)&arg,
		    xdr_bp_whoami_res, (caddr_t)&bp, rexmit, resp_wait,
			&to, &from, AUTH_NONE);
		if (stat == RPC_TIMEDOUT && !printed_waiting_msg) {
			dprintf(noserver, "whoami");
			printed_waiting_msg = 1;
		}
		/*
		 * Retransmission interval for second and subsequent tries.
		 * We expect first bpmap_rmtcall to retransmit and backoff to
		 * at least this value.
		 */
		rexmit = resp_wait;
		resp_wait = 0;		/* go to default wait now. */
	} while (stat == RPC_TIMEDOUT);

	if (stat != RPC_SUCCESS) {
		dprintf("whoami RPC call failed with rpc status: %d\n", stat);
		retval = FALSE;
		goto done;
	} else {
		if (printed_waiting_msg && (boothowto & RB_VERBOSE))
			printf("Bootparam response received\n");

		/* Cache responder... We'll send our getfile here... */
		responder.s_addr = from.sin_addr.s_addr;
	}

	namelen = strlen(bp.client_name);
	if (namelen > SYS_NMLN) {
		dprintf("whoami: hostname too long");
		retval = FALSE;
		goto done;
	}
	if (namelen > 0) {
		if (boothowto & RB_VERBOSE)
			printf("hostname: %s\n", bp.client_name);
		sethostname(bp.client_name, namelen);
	} else {
		dprintf("whoami: no host name\n");
		retval = FALSE;
		goto done;
	}

	namelen = strlen(bp.domain_name);
	if (namelen > SYS_NMLN) {
		dprintf("whoami: domainname too long");
		retval = FALSE;
		goto done;
	}
	if (namelen > 0)
		if (boothowto & RB_VERBOSE)
			printf("domainname: %s\n", bp.domain_name);
	else
		dprintf("whoami: no domain name\n");

	if (bp.router_address.address_type == IP_ADDR_TYPE) {
		bcopy((caddr_t)&bp.router_address.bp_address.ip_addr,
		    (caddr_t)&ipaddr, sizeof (ipaddr));
		if (ntohl(ipaddr.s_addr) != INADDR_ANY) {
			dprintf("whoami: Router ip is: %s\n",
			    inet_ntoa(ipaddr));
			/* ipv4_route expects IP addresses in network order */
			(void) ipv4_route(IPV4_ADD_ROUTE, RT_DEFAULT, NULL,
			    &ipaddr);
		}
	} else
		dprintf("whoami: unknown gateway addr family %d\n",
		    bp.router_address.address_type);
done:
	return (retval);
}

/*
 * Returns:
 *	1) The ascii form of our root servers name in `server_name'.
 *	2) Pathname of our root on the server in `server_path'.
 *
 * NOTE: it's ok for getfile() to do dynamic allocation - it's only
 * used locally, then freed. If the server address returned from the
 * getfile call is different from our current destination address,
 * reset destination IP address to the new value.
 */
bool_t
getfile(char *fileid, char *server_name, struct in_addr *server_ip,
    char *server_path)
{
	struct bp_getfile_arg	arg;
	struct bp_getfile_res	res;
	enum clnt_stat		stat;
	struct sockaddr_in	to, from;
	int			rexmit;
	int			wait;
	int			printed_waiting_msg;

	arg.client_name = bp.client_name;
	arg.file_id = fileid;

	res.server_name = (bp_machine_name_t)bkmem_zalloc(SYS_NMLN + 1);
	res.server_path = (bp_path_t)bkmem_zalloc(SYS_NMLN + 1);

	if (res.server_name == NULL || res.server_path == NULL) {
		dprintf("getfile: rpc_call failed: No memory\n");
		errno = ENOMEM;
		if (res.server_name != NULL)
			bkmem_free(res.server_name, SYS_NMLN + 1);
		if (res.server_path != NULL)
			bkmem_free(res.server_path, SYS_NMLN + 1);
		return (FALSE);
	}

	to.sin_family = AF_INET;
	to.sin_addr.s_addr = responder.s_addr;
	to.sin_port = htons(0);

	/*
	 * Our addressing information was filled in by the call to
	 * whoami(), so now send an rpc message to the
	 * bootparam daemon requesting our server information.
	 *
	 * Wait only 32 secs for rpc_call to succeed.
	 */
	stat = brpc_call((rpcprog_t)BOOTPARAMPROG, (rpcvers_t)BOOTPARAMVERS,
	    (rpcproc_t)BOOTPARAMPROC_GETFILE, xdr_bp_getfile_arg, (caddr_t)&arg,
	    xdr_bp_getfile_res, (caddr_t)&res, 0, 32, &to, &from, AUTH_NONE);

	if (stat == RPC_TIMEDOUT) {
		/*
		 * The server that answered the whoami doesn't
		 * answer our getfile. Broadcast the call to all. Keep
		 * trying forever. Set up for limited broadcast.
		 */
		to.sin_addr.s_addr = htonl(INADDR_BROADCAST);

		rexmit = 0;	/* use default rexmit interval */
		wait = 32;	/* only wait 32 secs for first response */
		printed_waiting_msg = 0;
		do {
			stat = bpmap_rmtcall((rpcprog_t)BOOTPARAMPROG,
			    (rpcvers_t)BOOTPARAMVERS,
			    (rpcproc_t)BOOTPARAMPROC_GETFILE,
			    xdr_bp_getfile_arg, (caddr_t)&arg,
			    xdr_bp_getfile_res, (caddr_t)&res, rexmit,
			    wait, &to, &from, AUTH_NONE);

			if (stat == RPC_SUCCESS) {
				/*
				 * set our destination addresses to
				 * those of the server that responded.
				 * It's probably our server, and we
				 * can thus save arping for no reason later.
				 */
				responder.s_addr = from.sin_addr.s_addr;
				if (printed_waiting_msg &&
				    (boothowto & RB_VERBOSE)) {
					printf(
					    "Bootparam response received.\n");
				}
				break;
			}
			if (stat == RPC_TIMEDOUT && !printed_waiting_msg) {
				dprintf(noserver, "getfile");
				printed_waiting_msg = 1;
			}
			/*
			 * Retransmission interval for second and
			 * subsequent tries. We expect first bpmap_rmtcall
			 * to retransmit and backoff to at least this
			 * value.
			 */
			rexmit = wait;
			wait = 0;	/* use default wait time now. */
		} while (stat == RPC_TIMEDOUT);
	}

	if (stat == RPC_SUCCESS) {
		/* got the goods */
		bcopy(res.server_name, server_name, strlen(res.server_name));
		bcopy(res.server_path, server_path, strlen(res.server_path));
		switch (res.server_address.address_type) {
		case IP_ADDR_TYPE:
			/*
			 * server_address is where we will get our root
			 * from. Replace destination entries in address if
			 * necessary.
			 */
			bcopy((caddr_t)&res.server_address.bp_address.ip_addr,
			    (caddr_t)server_ip, sizeof (struct in_addr));
			break;
		default:
			dprintf("getfile: unknown address type %d\n",
				res.server_address.address_type);
			server_ip->s_addr = htonl(INADDR_ANY);
			bkmem_free(res.server_name, SYS_NMLN + 1);
			bkmem_free(res.server_path, SYS_NMLN + 1);
			return (FALSE);
		}
	} else {
		dprintf("getfile: rpc_call failed.\n");
		bkmem_free(res.server_name, SYS_NMLN + 1);
		bkmem_free(res.server_path, SYS_NMLN + 1);
		return (FALSE);
	}

	bkmem_free(res.server_name, SYS_NMLN + 1);
	bkmem_free(res.server_path, SYS_NMLN + 1);

	return (TRUE);
}

/*
 * XDR routines for bootparams.
 */

bool_t
xdr_bp_machine_name_t(XDR *xdrs, bp_machine_name_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_MACHINE_NAME));
}

bool_t
xdr_bp_path_t(XDR *xdrs, bp_path_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_PATH_LEN));
}

bool_t
xdr_bp_fileid_t(XDR *xdrs, bp_fileid_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_FILEID));
}

bool_t
xdr_ip_addr_t(XDR *xdrs, ip_addr_t *objp)
{
	if (!xdr_char(xdrs, &objp->net))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->host))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->lh))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->impno))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_address(XDR *xdrs, bp_address *objp)
{
	static struct xdr_discrim choices[] = {
		{ IP_ADDR_TYPE, xdr_ip_addr_t },
		{ __dontcare__, NULL }
	};

	return (xdr_union(xdrs, (enum_t *)&objp->address_type,
	    (char *)&objp->bp_address, choices, (xdrproc_t)NULL));
}

bool_t
xdr_bp_whoami_arg(XDR *xdrs, bp_whoami_arg *objp)
{
	return (xdr_bp_address(xdrs, &objp->client_address));
}

bool_t
xdr_bp_whoami_res(XDR *xdrs, bp_whoami_res *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->client_name))
		return (FALSE);
	if (!xdr_bp_machine_name_t(xdrs, &objp->domain_name))
		return (FALSE);
	if (!xdr_bp_address(xdrs, &objp->router_address))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_getfile_arg(XDR *xdrs, bp_getfile_arg *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->client_name))
		return (FALSE);
	if (!xdr_bp_fileid_t(xdrs, &objp->file_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_getfile_res(XDR *xdrs, bp_getfile_res *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->server_name))
		return (FALSE);
	if (!xdr_bp_address(xdrs, &objp->server_address))
		return (FALSE);
	if (!xdr_bp_path_t(xdrs, &objp->server_path))
		return (FALSE);
	return (TRUE);
}
