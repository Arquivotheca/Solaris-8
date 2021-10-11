/*
 * Copyright (c) 1991-1999, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lookup.c	1.25	99/02/23 SMI"

/*
 * This file contains the file lookup code for NFS.
 */

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include "nfs_prot.h"
#include "mount.h"
#include <pathname.h>
#include <sys/errno.h>
#include <sys/promif.h>
#include "nfs_inet.h"
#include "socket_inet.h"
#include "rpc.h"
#include <sys/types.h>
#include <sys/salib.h>
#include <sys/sacache.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include "mac.h"

static int root_inum = 1;	/* Dummy i-node number for root */
static int next_inum = 1;	/* Next dummy i-node number	*/

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * starting at current directory (root for us), lookup the pathname.
 * return the file handle of said file.
 */

static int getsymlink(struct nfs_fh *fh, struct pathname *pnp);
static int lookuppn(struct pathname *pnp, struct nfs_file *cfile);

int
lookup(char *pathname, struct nfs_file *cur_file)
{
	struct pathname pnp;
	int error;

	static char lkup_path[NFS_MAXPATHLEN];	/* pn_alloc doesn't */

	pnp.pn_buf = &lkup_path[0];
	bzero(pnp.pn_buf, NFS_MAXPATHLEN);
	error = pn_get(pathname, &pnp);
	if (error)
		return (error);
	error = lookuppn(&pnp, cur_file);
	return (error);
}

static int
lookuppn(struct pathname *pnp, struct nfs_file *cfile)
{
	struct sockaddr_in	*to;
	enum clnt_stat status;
	char component[NFS_MAXNAMLEN+1];	/* buffer for component */
	int nlink = 0;
	diropargs dirop;
	diropargs *diropp;
	int error = 0;
	int dino, cino;
	diropres res_lookup;
	diropres *resp;

	struct cached_dir {
		diropargs arg;
		diropres  res;
	} *cdp;

	*cfile = roothandle;	/* structure copy - start at the root. */
	to = nfs_server_sa();
	dino = root_inum;
begin:
	/*
	 * Each time we begin a new name interpretation (e.g.
	 * when first called and after each symbolic link is
	 * substituted), we allow the search to start at the
	 * root directory if the name starts with a '/', otherwise
	 * continuing from the current directory.
	 */
	component[0] = '\0';
	if (pn_peekchar(pnp) == '/') {
		*cfile = roothandle;
		dino = root_inum;
		pn_skipslash(pnp);
	}

next:
	/*
	 * Make sure we have a directory.
	 */
	if (cfile->type != NFDIR) {
		error = ENOTDIR;
		goto bad;
	}
	/*
	 * Process the next component of the pathname.
	 */
	error = pn_stripcomponent(pnp, component);
	if (error)
		goto bad;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. "/." or ".".
	 */
	if (component[0] == '\0') {
		return (0);
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If at root directory (e.g. after chroot)
	 *    then ignore it so can't get out.
	 * 2. If this vnode is the root of a mounted
	 *    file system, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other file system.
	 */
	if (strcmp(component, "..") == 0) {
		if (cfile == &roothandle)
			goto skip;
	}

	/*
	 * Perform a lookup in the current directory.
	 */

	if (((cino = get_dcache(mac_state.mac_dev, component, dino)) != NULL) &&
	    ((cdp = (struct cached_dir *)get_icache(mac_state.mac_dev, cino))
	    != NULL)) {
		/*
		 *  We've seen this file before!  Set pointers to the data
		 *  we've cached.
		 */

		diropp = &cdp->arg;
		resp = &cdp->res;

	} else {
		/*
		 *  Requested component wasn't cached, perform a lookup
		 *  in the current directory.
		 */

		diropp = &dirop;
		resp = &res_lookup;
		bcopy(&cfile->fh, &diropp->dir, FHSIZE);

		diropp->name = component;
		status = brpc_call((rpcprog_t)NFS_PROGRAM,
		    (rpcvers_t)NFS_VERSION, (rpcproc_t)NFSPROC_LOOKUP,
		    xdr_diropargs, (caddr_t)diropp,
		    xdr_diropres, (caddr_t)resp, 0, 0, to, NULL, AUTH_UNIX);
		if (status != RPC_SUCCESS) {
			dprintf("lookup: RPC error.\n");
			return (-1);
		}
		if (resp->status != NFS_OK) {
			nfs_error(resp->status);
			return (resp->status);
		}

		if (cdp = (struct cached_dir *)
		    bkmem_alloc(sizeof (struct cached_dir))) {
			/*
			 *  Save this entry in cache for next time ...
			 */

			if (!cino) cino = ++next_inum;
			cdp->arg = *diropp;
			cdp->res = *resp;
			set_dcache(mac_state.mac_dev, component, dino, cino);
			set_icache(mac_state.mac_dev, cino, cdp,
			    sizeof (struct cached_dir));
		} else {
			/*
			 *  Out of memory, clear cache keys so we don't get
			 *  confused later.
			 */

			cino = 0;
		}
	}

	dino = cino;

	/*
	 * If we hit a symbolic link and there is more path to be
	 * translated or this operation does not wish to apply
	 * to a link, then place the contents of the link at the
	 * front of the remaining pathname.
	 */
	if (resp->diropres_u.diropres.attributes.type == NFLNK) {

		struct pathname linkpath;
		static char path_tmp[NFS_MAXPATHLEN];	/* used for symlinks */

		linkpath.pn_buf = &path_tmp[0];

		nlink++;
		if (nlink > MAXSYMLINKS) {
			error = ELOOP;
			goto bad;
		}
		error = getsymlink(&resp->diropres_u.diropres.file,
		    &linkpath);
		if (error)
			goto bad;
		if (pn_pathleft(&linkpath) == 0)
			(void) pn_set(&linkpath, ".");
		error = pn_combine(pnp, &linkpath);	/* linkpath before pn */
		if (error)
			goto bad;
		goto begin;
	}

	bcopy(&resp->diropres_u.diropres.file, &cfile->fh, FHSIZE);
	cfile->type = (int)resp->diropres_u.diropres.attributes.type;
	cfile->status = resp->status;

skip:
	/*
	 * Skip to next component of the pathname.
	 * If no more components, return last directory (if wanted)  and
	 * last component (if wanted).
	 */
	if (pn_pathleft(pnp) == 0) {
		(void) pn_set(pnp, component);
		return (0);
	}
	/*
	 * skip over slashes from end of last component
	 */
	pn_skipslash(pnp);

	goto next;
bad:
	/*
	 * Error.
	 */
	return (error);

}

/*
 * Gets symbolic link into pathname.
 */
static int
getsymlink(struct nfs_fh *fh, struct pathname *pnp)
{
	struct sockaddr_in	*to;
	enum clnt_stat status;
	struct readlinkres linkres;
	static char symlink_path[NFS_MAXPATHLEN];

	to = nfs_server_sa();

	/*
	 * linkres needs a zeroed buffer to place path data into:
	 */
	bzero(symlink_path, NFS_MAXPATHLEN);
	linkres.readlinkres_u.data = &symlink_path[0];

	status = brpc_call((rpcprog_t)NFS_PROGRAM, (rpcvers_t)NFS_VERSION,
	    (rpcproc_t)NFSPROC_READLINK, xdr_nfs_fh, (caddr_t)fh,
	    xdr_readlinkres, (caddr_t)&linkres, 0, 0, to, NULL, AUTH_UNIX);
	if (status != RPC_SUCCESS) {
		dprintf("getsymlink: RPC call failed.\n");
		return (-1);
	}
	if (linkres.status != NFS_OK) {
		nfs_error(linkres.status);
		return (linkres.status);
	}

	pn_get(linkres.readlinkres_u.data, pnp);

	return (NFS_OK);
}
