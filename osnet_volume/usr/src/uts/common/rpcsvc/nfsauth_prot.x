/*
 *	nfsauth_prot.x
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident   "@(#)nfsauth_prot.x 1.7     96/12/19 SMI"

/*
 * nfsauth protocol
 *
 * This protocol is used by the kernel to
 * authorize NFS clients.  This service
 * lives in the mount daemon and checks
 * the client's access for an export
 * with a given authentication flavor.
 *
 * The status result determines what kind
 * of access the client is permitted.
 *
 * The result is cached in the kernel, so
 * the authorization call will be made
 * only the first time the client mounts
 * the filesystem.
 */

const A_MAXPATH	= 1024;

struct auth_req {
	netobj 	req_client;		/* client's address */
	string	req_netid<>;		/* Netid of address */
	string	req_path<A_MAXPATH>;	/* export path */
	int	req_flavor;		/* auth flavor */
};

const NFSAUTH_DENIED= 0x01;		/* Access denied */
const NFSAUTH_RO   = 0x02;		/* Read-only */
const NFSAUTH_RW   = 0x04;		/* Read-write */
const NFSAUTH_ROOT = 0x08;		/* Root access */

/*
 * The following are not part of the protocol.
 */
const NFSAUTH_DROP = 0x10;		/* Drop request */
const NFSAUTH_MAPNONE = 0x20;		/* Mapped flavor to AUTH_NONE */

struct auth_res {
	int auth_perm;
};

program NFSAUTH_PROG {
	version NFSAUTH_VERS {

		/*
		 * Authorization Request
		 */
		auth_res
		NFSAUTH_ACCESS(auth_req) = 1;

	} = 1;
} = 100231;
