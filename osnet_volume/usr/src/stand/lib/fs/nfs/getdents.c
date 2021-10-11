/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 *  Stuff relating to directory reading ...
 */

#pragma ident	"@(#)getdents.c	1.8	99/02/23 SMI"

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include "nfs_prot.h"
#include "nfs_inet.h"
#include "rpc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/sysmacros.h>
#include "socket_inet.h"
#include <sys/salib.h>
#include <sys/bootdebug.h>

#define	MAXDENTS 16
#define	SLOP (sizeof (struct dirent) - (int)&((struct dirent *)0)->d_name[1])
#define	MINSIZ 20
#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 *  Get directory entries:
 *
 *	Uses the nfs "READDIR" operation to read directory entries
 *	into a local buffer.  These are then translated into file
 *	system independent "dirent" structs and returned in the
 *	caller's buffer.  Returns the number of entries converted
 *	(-1 if there's an error).
 *
 *	The standalone xdr routines do not allocate memory, so
 *	we have to assume the worst case of 256 byte names.
 *	This is a space hog in our local buffer, so we want
 *	the number of buffers to be small. To make sure we don't
 *	get more names than we can handle, we tell the rpc
 *	routine that we only have space for MAXDENT names if
 *	they are all the minimum size. This keeps the return
 *	packet unfragmented, but may result in lots of reads
 *	to process a large directory. Since this is standalone
 *	we don't worry about speed. With MAXDENTs at 16, the
 *	local buffer is 4k.
 */

int
nfs_getdents(struct nfs_file *nfp, struct dirent *dep, unsigned size)
{
	struct sockaddr_in	*to;
	entry *ep;
	readdirargs rda;
	readdirres  res;
	enum clnt_stat status;
	struct {
		entry etlist[MAXDENTS];
		char names[MAXDENTS][NFS_MAXNAMLEN+1];
	} rdbuf;

	int j, cnt = 0;
	bzero((caddr_t)&res, sizeof (res));
	bzero((caddr_t)&rda, sizeof (rda));
	bzero((caddr_t)rdbuf.etlist, sizeof (rdbuf.etlist));
	bcopy((caddr_t)&nfp->fh, (caddr_t)&rda.dir, NFS_FHSIZE);
	bcopy((caddr_t)nfp->cookie, (caddr_t)rda.cookie, sizeof (nfscookie));

	to = nfs_server_sa();

	while (!res.readdirres_u.reply.eof) {
		/*
		 *  Keep issuing nfs calls until EOF is reached on
		 *  the directory or the user buffer is filled.
		 */

		for (j = 0; j < MAXDENTS; j++) {
			/*
			 *  Link our buffers together for the benefit of
			 *  XDR.  We do this each time we issue the rpc call
			 *  JIC the xdr decode
			 *  routines screw up the linkage!
			 */

			rdbuf.etlist[j].name = rdbuf.names[(MAXDENTS-1) - j];
			rdbuf.etlist[j].nextentry =
				(j < (MAXDENTS-1)) ? &rdbuf.etlist[j+1] : 0;
		}

		res.readdirres_u.reply.entries = rdbuf.etlist;
		/*
		 * Cannot give the whole buffer unless every name is
		 * 256 bytes! Assume the worst case of all 1 byte names.
		 * This results in MINSIZ bytes/name in the xdr stream.
		 */
		rda.count = sizeof (res) + MAXDENTS*MINSIZ;
		bzero((caddr_t)rdbuf.names, sizeof (rdbuf.names));

		status = brpc_call((rpcprog_t)NFS_PROGRAM,
		    (rpcvers_t)NFS_VERSION, (rpcproc_t)NFSPROC_READDIR,
		    xdr_readdirargs, (caddr_t)&rda,
		    xdr_readdirres, (caddr_t)&res, 0, 0, to, NULL, AUTH_UNIX);

		if (status != RPC_SUCCESS) {
			dprintf("nfs_getdents: RPC error\n");
			return (-1);
		}
		if (res.status != NFS_OK) {
			/*
			 *  The most common failure here would be trying to
			 *  issue a getdents call on a non-directory!
			 */

			nfs_error(res.status);
			return (-1);
		}

		for (ep = rdbuf.etlist; ep; ep = ep->nextentry) {
			/*
			 *  Step thru all entries returned by NFS, converting
			 *  to the cannonical form and copying out to the
			 *  user's buffer.
			 */

			int n;

			/*
			 * catch the case user called at EOF
			 */
			if ((n = strlen(ep->name)) == 0)
				break;

			n = roundup((sizeof (struct dirent) +
			    ((n > SLOP) ? n : 0)), sizeof (off_t));

			if (n > size)
				return (cnt);
			size -= n;

			(void) strcpy(dep->d_name, ep->name);
			dep->d_ino = ep->fileid;
			dep->d_off = cnt++;
			dep->d_reclen = (u_short)n;

			dep = (struct dirent *)((char *)dep + n);
			bcopy(ep->cookie, rda.cookie, sizeof (nfscookie));
			bcopy(ep->cookie, nfp->cookie, sizeof (nfscookie));
		}
	}

	return (cnt);
}
