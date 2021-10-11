/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)mount.c	1.24	99/02/23 SMI"

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#undef NFSSERVER
#include "rpc.h"
#include "pmap.h"
#include "nfs_prot.h"
#include "nfs_inet.h"
#include "bootparam.h"
#include <sys/dhcpboot.h>
#include "mount.h"
#include <sys/promif.h>
#include <sys/salib.h>
#include "socket_inet.h"
#include "ipv4.h"
#include "mac.h"
#include <sys/bootdebug.h>

struct nfs_file		roothandle;			/* root file handle */
static char		root_hostname[SYS_NMLN];	/* server hostname */
static char		root_pathbuf[NFS_MAXPATHLEN];	/* the root's path */
static char		root_boot_file[NFS_MAXPATHLEN];	/* optional boot file */
static struct sockaddr_in	root_to;		/* server sock ip */
							/* in network order */
int dontroute = FALSE;	/* In case rarp/bootparams was selected */

extern void set_default_filename(char *);	/* boot.c */

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * xdr routines used by mount.
 */

xdr_fhstatus(XDR *xdrs, struct fhstatus *fhsp)
{
	if (!xdr_int(xdrs, (int *)&fhsp->fhs_status))
		return (FALSE);
	if (fhsp->fhs_status == 0) {
		if (!xdr_fhandle(xdrs, &(fhsp->fhstatus_u.fhs_fhandle)))
			return (FALSE);
	}
	return (TRUE);
}

xdr_fhandle(XDR *xdrs, nfs_fh *fhp)
{
	if (xdr_opaque(xdrs, (char *)fhp, NFS_FHSIZE)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_path(XDR *xdrs, char **pathp)
{
	if (xdr_string(xdrs, pathp, 1024)) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * This routine will open a device as it is known by the V2 OBP. It
 * then goes thru the stuff necessary to initialize the network device,
 * get our network parameters, (using DHCP or rarp/bootparams), and
 * finally actually go and get the root filehandle. Sound like fun?
 * Suuurrrree. Take a look.
 *
 * Returns 0 if things worked. -1 if we crashed and burned.
 */
int
boot_nfs_mountroot(char *str)
{
	enum clnt_stat	status;
	char		*root_path = &root_pathbuf[0];	/* to make XDR happy */
	struct fhstatus	root_tmp;			/* to pass to rpc/xdr */
	int		rexmit;
	int		resp_wait;

	root_to.sin_family = AF_INET;
	root_to.sin_addr.s_addr = htonl(INADDR_ANY);
	root_to.sin_port = htons(0);

	mac_init(str);

	(void) ipv4_setpromiscuous(TRUE);

	if (get_netconfig_strategy() == NCT_BOOTP_DHCP) {
		if (boothowto & RB_VERBOSE)
			printf("Using BOOTP/DHCP...\n");
		if (dhcp(&root_to.sin_addr, root_hostname,
		    sizeof (root_hostname), root_pathbuf, sizeof (root_pathbuf),
		    root_boot_file, sizeof (root_boot_file)) != 0) {
			if (boothowto & RB_VERBOSE)
				printf("BOOTP/DHCP configuration failed!\n");
			return (-1);
		}
		/* now that we have an IP address, turn off promiscuous mode */
		(void) ipv4_setpromiscuous(FALSE);

		/* if we got a boot file name, use it as the default */
		if (root_boot_file[0] != '\0')
			set_default_filename(root_boot_file);
	} else {
		/* Use RARP/BOOTPARAMS. RARP will try forever... */
		if (boothowto & RB_VERBOSE)
			printf("Using RARP/BOOTPARAMS...\n");
		mac_state.mac_rarp();

		/*
		 * Since there is no way to determine our netmask, and therefore
		 * figure out if the router we got is useful, we assume all
		 * services are local. Use DHCP if this bothers you.
		 */
		dontroute = TRUE;

		/* now that we have an IP address, turn off promiscuous mode */
		(void) ipv4_setpromiscuous(FALSE);

		/* get our hostname */
		if (whoami() == FALSE)
			return (-1);

		/* get our bootparams. */
		if (getfile("root", root_hostname, &root_to.sin_addr,
		    root_pathbuf) == FALSE)
			return (-1);
	}

	/* mount root */
	if (boothowto & RB_VERBOSE) {
		printf("root server: %s (%s)\n", root_hostname,
		    inet_ntoa(root_to.sin_addr));
		printf("root directory: %s\n", root_pathbuf);
	}

	/*
	 * Wait up to 16 secs for first response, retransmitting expon.
	 */
	rexmit = 0;	/* default retransmission interval */
	resp_wait = 16;
	do {
		status = brpc_call((rpcprog_t)MOUNTPROG, (rpcvers_t)MOUNTVERS,
		    (rpcproc_t)MOUNTPROC_MNT, xdr_path, (caddr_t)&root_path,
		    xdr_fhstatus, (caddr_t)&(root_tmp), rexmit, resp_wait,
		    &root_to, NULL, AUTH_UNIX);
		if (status == RPC_TIMEDOUT) {
			dprintf("boot: %s:%s mount server not responding.\n",
			    root_hostname, root_pathbuf);
		}
		rexmit = resp_wait;
		resp_wait = 0;	/* use default wait time. */
	} while (status == RPC_TIMEDOUT);

	if ((status == RPC_SUCCESS) && (root_tmp.fhs_status == 0)) {
		/*
		 * Since the mount succeeded, we'll mark the roothandle's
		 * status as NFS_OK, and its type as NFDIR. If these
		 * points aren't the case, then we wouldn't be here.
		 */
		bcopy(&root_tmp.fhstatus_u.fhs_fhandle, &roothandle.fh, FHSIZE);
		roothandle.status = NFS_OK;
		roothandle.type = NFDIR;
		roothandle.offset = (u_int)0;	/* it's a directory! */

		root_to.sin_port = htons(0);	/* NFS is next after mount */
		return (0);
	}
	nfs_error(root_tmp.fhs_status);
	return (-1);
}

struct sockaddr_in *
nfs_server_sa(void)
{
	return (&root_to);
}
