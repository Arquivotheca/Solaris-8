/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * nfs_inet.h contains definitions specific to inetboot's nfs v2 implementation.
 */

#ifndef _NFS_INET_H
#define	_NFS_INET_H

#pragma ident	"@(#)nfs_inet.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/saio.h>
#include <nfs_prot.h>

/*
 * NFS: This structure represents the current open file.
 */
struct nfs_file {
	int status;
	nfs_fh fh;
	int type;
	u_long offset;
	nfscookie cookie;
};

struct nfs_fid {
	u_short nf_len;
	u_short nf_pad;
	struct nfs_fh fh;
};

#define	NFSBUF_SIZE	(READ_SIZE+1024)
#define	READ_SIZE	(8192)	/* NFS readsize */
#define	NFS_READ_DECR	(1024)	/* NFS readsize decrement */
#define	NFS_MAX_FERRS	(3)	/* MAX frame errors before decr read size */
#define	NFS_REXMIT_MIN	(3)	/* NFS retry min in secs */
#define	NFS_REXMIT_MAX	(15)	/* NFS retry max in secs */

extern int nfs_readsize;
extern void nfs_error(enum nfsstat);
extern struct nfs_file roothandle;

#ifdef	__cplusplus
}
#endif

#endif /* _NFS_INET_H */
