/*
 * Copyright (c) 1991-1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * Simple nfs ops - open, close, read, and lseek.
 */

#pragma ident	"@(#)nfsops.c	1.48	99/02/23 SMI"

#include <rpc/types.h>
#include <rpc/auth.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <errno.h>
#undef NFSSERVER
#include "nfs_prot.h"
#include <sys/promif.h>
#include <rpc/xdr.h>
#include "nfs_inet.h"
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include <sys/salib.h>
#include <sys/sacache.h>
#include "rpc.h"
#include "socket_inet.h"
#include "mac.h"

struct nfs_file current_file;

static int file_desc = 1;
static struct nfs_files {
	struct nfs_file file;
	int	desc;
	struct nfs_files *next;
} nfs_files[1] = {
	{0, 0, 0},
};

#define	dprintf	if (boothowto & RB_DEBUG) printf

extern int	boot_nfs_mountroot(char *str);
extern int	boot_nfs_unmountroot(void);
static int	boot_nfs_open(char *filename, int flags);
static int	boot_nfs_close(int fd);
static ssize_t	boot_nfs_read(int fd, caddr_t buf, size_t size);
static off_t	boot_nfs_lseek(int, off_t, int);
static int	boot_nfs_fstat(int fd, struct stat *stp);
static void	boot_nfs_closeall(int flag);
static int	boot_nfs_getdents(int fd, struct dirent *dep, unsigned size);

struct boot_fs_ops boot_nfs_ops = {
	"nfs",
	boot_nfs_mountroot,
	boot_nfs_unmountroot,
	boot_nfs_open,
	boot_nfs_close,
	boot_nfs_read,
	boot_nfs_lseek,
	boot_nfs_fstat,
	boot_nfs_closeall,
	boot_nfs_getdents
};

/*
 * Xdr routines for NFS ops.
 */
bool_t
xdr_nfsstat(xdrs, objp)
	XDR *xdrs;
	nfsstat *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ftype(xdrs, objp)
	XDR *xdrs;
	ftype *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_nfs_fh(xdrs, objp)
	XDR *xdrs;
	nfs_fh *objp;
{
	if (!xdr_opaque(xdrs, objp->data, NFS_FHSIZE)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_nfstime(xdrs, objp)
	XDR *xdrs;
	nfstime *objp;
{
	if (!xdr_u_int(xdrs, &objp->seconds)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->useconds)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_fattr(xdrs, objp)
	XDR *xdrs;
	fattr *objp;
{
	if (!xdr_ftype(xdrs, &objp->type)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->mode)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->nlink)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->uid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->gid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocksize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rdev)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocks)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->fsid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->fileid)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->atime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->mtime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->ctime)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_sattr(xdrs, objp)
	XDR *xdrs;
	sattr *objp;
{
	if (!xdr_u_int(xdrs, &objp->mode)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->uid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->gid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->atime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->mtime)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_filename(xdrs, objp)
	XDR *xdrs;
	filename *objp;
{
	if (!xdr_string(xdrs, objp, NFS_MAXNAMLEN)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_nfspath(xdrs, objp)
	XDR *xdrs;
	nfspath *objp;
{
	if (!xdr_string(xdrs, objp, NFS_MAXPATHLEN)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_attrstat(xdrs, objp)
	XDR *xdrs;
	attrstat *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_fattr(xdrs, &objp->attrstat_u.attributes)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_sattrargs(xdrs, objp)
	XDR *xdrs;
	sattrargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_diropargs(xdrs, objp)
	XDR *xdrs;
	diropargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->dir)) {
		return (FALSE);
	}
	if (!xdr_filename(xdrs, &objp->name)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_diropokres(xdrs, objp)
	XDR *xdrs;
	diropokres *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_fattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_diropres(xdrs, objp)
	XDR *xdrs;
	diropres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_diropokres(xdrs, &objp->diropres_u.diropres)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_readlinkres(xdrs, objp)
	XDR *xdrs;
	readlinkres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_nfspath(xdrs, &objp->readlinkres_u.data)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_readargs(xdrs, objp)
	XDR *xdrs;
	readargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->count)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->totalcount)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_readokres(xdrs, objp)
	XDR *xdrs;
	readokres *objp;
{
	if (!xdr_fattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
	    (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_readres(xdrs, objp)
	XDR *xdrs;
	readres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_readokres(xdrs, &objp->readres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_writeargs(xdrs, objp)
	XDR *xdrs;
	writeargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->beginoffset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->totalcount)) {
		return (FALSE);
	}
	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
	    (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_createargs(xdrs, objp)
	XDR *xdrs;
	createargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->where)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_renameargs(xdrs, objp)
	XDR *xdrs;
	renameargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_diropargs(xdrs, &objp->to)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_linkargs(xdrs, objp)
	XDR *xdrs;
	linkargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_diropargs(xdrs, &objp->to)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_symlinkargs(xdrs, objp)
	XDR *xdrs;
	symlinkargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_nfspath(xdrs, &objp->to)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_nfscookie(xdrs, objp)
	XDR *xdrs;
	nfscookie objp;
{
	if (!xdr_opaque(xdrs, objp, NFS_COOKIESIZE)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_readdirargs(xdrs, objp)
	XDR *xdrs;
	readdirargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->dir)) {
		return (FALSE);
	}
	if (!xdr_nfscookie(xdrs, objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->count)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_entry(xdrs, objp)
	XDR *xdrs;
	entry *objp;
{
	if (!xdr_u_int(xdrs, &objp->fileid)) {
		return (FALSE);
	}
	if (!xdr_filename(xdrs, &objp->name)) {
		return (FALSE);
	}
	if (!xdr_nfscookie(xdrs, objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->nextentry, sizeof (entry),
	    (xdrproc_t)xdr_entry)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_dirlist(xdrs, objp)
	XDR *xdrs;
	dirlist *objp;
{
	if (!xdr_pointer(xdrs, (char **)&objp->entries, sizeof (entry),
	    (xdrproc_t)xdr_entry)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->eof)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_readdirres(xdrs, objp)
	XDR *xdrs;
	readdirres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_dirlist(xdrs, &objp->readdirres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_statfsokres(xdrs, objp)
	XDR *xdrs;
	statfsokres *objp;
{
	if (!xdr_u_int(xdrs, &objp->tsize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bsize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocks)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bfree)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bavail)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_statfsres(xdrs, objp)
	XDR *xdrs;
	statfsres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_statfsokres(xdrs, &objp->statfsres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

/*
 * bootops.c calls a closeall() function to close all open files. Since
 * we only permit one open file at a time (not counting the device), this
 * is simple to implement.
 */

/*ARGSUSED*/
static void
boot_nfs_closeall(int flag)
{
	struct nfs_files	*filep, *prevp;

	/* clear the first, static file */
	filep = nfs_files;
	bzero((caddr_t)&filep->file, sizeof (struct nfs_file));
	filep->desc = 0;

	/* delete any dynamically allocated entries */
	filep = filep->next;
	while (filep != NULL) {
		prevp = filep;
		filep = filep->next;
		bkmem_free((caddr_t)prevp, sizeof (struct  nfs_files));
	}

	/* Close device */
	release_cache(mac_state.mac_dev);

	mac_fini();
}

/*
 * Get a file pointer given a file descriptor.  Return 0 on error
 */
static struct nfs_files *
get_filep(int fd)
{
	struct nfs_files *filep;

	for (filep = nfs_files; filep; filep = filep->next) {
		if (fd == filep->desc)
			return (filep);
	}
	return (NULL);
}

/*
 * Unmount the root fs -- not supported for this fstype.
 */

int
boot_nfs_unmountroot(void)
{
	return (-1);
}

/*
 * open a file for reading. Note: writing is NOT supported.
 */

static int
boot_nfs_open(char *path, int flags)
{
	struct nfs_files *filep, *newfilep;
	int got_filep;
	extern int lookup();

	/* file can only be opened readonly. */
	if (flags & ~O_RDONLY) {
		dprintf("open: files can only be opened O_RDONLY.\n");
		return (-1);
	}

	if (path == NULL || *path == '\0') {
		dprintf("open: NULL or EMPTY pathname argument.\n");
		return (-1);
	}

	/* Try and find a vacant file pointer */
	filep = nfs_files;
	got_filep = FALSE;
	do {
		if (filep->desc == 0) {
			filep->desc = file_desc++;
			got_filep = TRUE;
			break;		/* We've got a file pointer */
		}
		/* Get next entry if not at end of list */
		if (filep->next)
			filep = filep->next;
	} while (filep->next);

	/* If a a vacant file pointer cannot be found, make one */
	if (!got_filep) {
		if ((newfilep = (struct nfs_files *)
		    bkmem_zalloc(sizeof (struct nfs_files))) == 0) {
			dprintf("open: Cannot allocate file pointer\n");
			return (-1);
		}
		filep->next = newfilep;
		filep = newfilep;
		filep->desc = file_desc++;
	}

	/* NFS lookup */
	if (lookup(path, &filep->file) != 0) {
#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_open(): Cannot open '%s'.\n", path);
#endif
		/* zero file pointer */
		bzero((caddr_t)filep, sizeof (struct nfs_file));
		filep->desc = 0;
		return (-1);
	}
	bzero(filep->file.cookie, sizeof (nfscookie));
#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_open(): '%s' successful, fd = 0x%x\n",
			path, filep->desc);
#endif
	return (filep->desc);
}

/*
 * close a previously opened file.
 */
static int
boot_nfs_close(int fd)
{
	struct nfs_files *filep;

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_close(): fd %d\n", fd);
#endif
	if ((filep = get_filep(fd)) == 0)
		return (0);

	/* zero file pointer */
	bzero((caddr_t)&filep->file, sizeof (struct nfs_file));
	filep->desc = 0; /* "close" the fd. */

	return (0);
}

/*
 * read from a file.
 */
static ssize_t
boot_nfs_read(int fd, char *buf, size_t size)
{
	struct sockaddr_in	*to;
	struct nfs_files	*filep;
	readargs		read_args;
	readres			read_res;
	enum clnt_stat		read_stat;
	u_int			readcnt = 0;	/* # bytes read by nfs */
	u_int			count = 0;	/* # bytes transferred to buf */
	int			done = FALSE;	/* last block has come in */
	int			rexmit;		/* retrans interval (secs) */
	int			resp_wait;	/* how long to wait for resp */
	int			framing_errs = 0;	/* stack errors */
	char			*buf_offset;	/* current buffer offset */
	static u_int		pos;		/* progress indicator counter */
	static char		ind[] = "|/-\\";	/* progress indicator */
	static int		blks_read;

	if (fd == 0) {
		dprintf("read: Bad file number.\n");
		return (-1);
	}
	if (buf == NULL) {
		dprintf("read: Bad address.\n");
		return (-1);
	}

	to = nfs_server_sa();

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_read(): 0x%x bytes buf: %x, fd: %x\n",
			size, buf, fd);
#endif

	/* initialize for read */
	if ((filep = get_filep(fd)) == 0)
		return (-1);

	read_args.file = filep->file.fh;	/* structure copy */
	read_args.offset = filep->file.offset;
	buf_offset = buf;

	/* Optimize for reads of less than one block size */

	if (nfs_readsize == 0)
		nfs_readsize = READ_SIZE;

	if (size < nfs_readsize)
		read_args.count = size;
	else
		read_args.count = nfs_readsize;

	do {
		/* use the user's buffer to stuff the data into. */
		read_res.readres_u.reply.data.data_val = buf_offset;

		/*
		 * Handle the case where the file does not end
		 * on a block boundary.
		 */
		if ((count + read_args.count) > size)
			read_args.count = size - count;

		rexmit = 0; /* use default RPC retransmission interval */
		resp_wait = NFS_REXMIT_MIN;	/* Total wait for call */
		do {
			read_stat = brpc_call((rpcprog_t)NFS_PROGRAM,
			    (rpcvers_t)NFS_VERSION, (rpcproc_t)NFSPROC_READ,
			    xdr_readargs, (caddr_t)&read_args, xdr_readres,
			    (caddr_t)&read_res, rexmit, resp_wait, to, NULL,
			    AUTH_UNIX);
			if (read_stat == RPC_TIMEDOUT) {
				dprintf("NFS read(%d) timed out. Retrying...\n",
				    read_args.count);
				/*
				 * If the remote is there and trying to respond,
				 * but our stack is having trouble reassembling
				 * the reply, reduce the read size in an
				 * attempt to compensate. Reset the
				 * transmission and reply wait timers.
				 */
				if (errno == ETIMEDOUT)
					framing_errs++;

				if (framing_errs > NFS_MAX_FERRS &&
				    read_args.count > NFS_READ_DECR) {
					read_args.count -= NFS_READ_DECR;
					nfs_readsize -= NFS_READ_DECR;
					dprintf("NFS Read size now %d.\n",
					    nfs_readsize);
					rexmit = 0;
					resp_wait = NFS_REXMIT_MIN;
					framing_errs = 0;
				} else {
					if (rexmit < NFS_REXMIT_MAX)
						rexmit++;
					if (resp_wait < NFS_REXMIT_MAX)
						resp_wait++;
					else
						resp_wait = 0; /* default RPC */
				}
			}
		} while (read_stat == RPC_TIMEDOUT);

		if (read_stat != RPC_SUCCESS)
			return (-1);

		readcnt = read_res.readres_u.reply.data.data_len;
		/*
		 * Handle the case where the file is simply empty, and
		 * nothing could be read.
		 */
		if (readcnt == 0)
			break; /* eof */

		/*
		 * Handle the case where the file is smaller than
		 * the size of the read request, thus the request
		 * couldn't be completely filled.
		 */
		if (readcnt < read_args.count) {
#ifdef NFS_OPS_DEBUG
			if ((boothowto & DBFLAGS) == DBFLAGS)
				printf("nfs_read(): partial read %d"
					" instead of %d\n",
					readcnt, read_args.count);
#endif
			done = TRUE; /* update the counts and exit */
		}

		/* update various offsets */
		count += readcnt;
		filep->file.offset += readcnt;
		buf_offset += readcnt;
		read_args.offset += readcnt;

		/*
		 * round and round she goes (though not on every block..
		 * - OBP's take a fair bit of time to actually print stuff)
		 */
		if ((blks_read++ & 0x3) == 0)
			dprintf("%c\b", ind[pos++ & 3]);

	} while (count < size && !done);

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_read(): 0x%x bytes.\n", count);
#endif
	return (count);
}

/*
 * lseek - move read file pointer.
 */

static off_t
boot_nfs_lseek(fd, offset, whence)
	int fd;		/* current open fd */
	off_t offset;	/* number of byte offset */
	int whence;	/* where the offset is calculated from */
{
	struct nfs_files *filep;

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_lseek(): fd 0x%x offset: 0x%x, whence: %d\n",
			fd, offset, whence);
#endif

	if (fd == 0) {
		dprintf("lseek: Bad file number.\n");
		return (-1);
	}

	if ((filep = get_filep(fd)) == 0)
		return (-1);

	switch (whence) {

	case SEEK_SET:
		/* file ptr is set to offset from beginning of file */
		filep->file.offset = offset;
		break;
	case SEEK_CUR:
		/* file ptr is set to offset from current position */
		filep->file.offset += offset;
		break;
	case SEEK_END:
		/*
		 * file ptr is set to current size of file plus offset.
		 * But since we only support reading, this is illegal.
		 */
	default:
		/* invalid offset origin */
		dprintf("lseek: invalid whence value.\n");
		return (-1);
	}

#ifdef XXXX
	return (filep->file.offset);
#else
	/*
	 * BROKE - lseek should return the offset seeked to on a
	 * successful seek, not zero - This must be fixed in the
	 * kernel before It can be fixed here.
	 */
	return (0);
#endif /* XXXX */
}

/*
 * nfs_getattr() with a fid_t pointer.
 */
int
boot_nfs_getattr(fid_t *fidp, struct vattr *vap)
{
	struct sockaddr_in	*to;
	enum clnt_stat getattr_stat;
	attrstat getattr_res;
	struct nfs_fid *nfsfidp = (struct nfs_fid *)fidp;
	fattr *na;

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS) {
		printf("boot_nfs_getattr():\n");
		if (fidp->fid_len != (sizeof (struct nfs_fid) - sizeof (short)))
			printf("assert failed for boot_nfs_getattr()");
	}
#endif

	to = nfs_server_sa();
	getattr_stat = brpc_call((rpcprog_t)NFS_PROGRAM,
	    (rpcvers_t)NFS_VERSION, (rpcproc_t)NFSPROC_GETATTR,
	    xdr_nfs_fh, (caddr_t)&(nfsfidp->fh), xdr_attrstat,
	    (caddr_t)&getattr_res, 0, 0, to, NULL, AUTH_UNIX);

	if (getattr_stat != RPC_SUCCESS) {
		dprintf("nfs_getattr: RPC error %d\n", getattr_stat);
		return (-1);
	}
	if (getattr_res.status != NFS_OK) {
		nfs_error(getattr_res.status);
		return (getattr_res.status);
	}

	/* adapted from nattr_to_vattr() in nfs_client.c */

	na = &getattr_res.attrstat_u.attributes;
	if (vap->va_mask & AT_MODE)
		vap->va_mode = na->mode;
	if (vap->va_mask & AT_SIZE)
		vap->va_size = na->size;
	if (vap->va_mask & AT_NODEID)
		vap->va_nodeid = na->fileid;
	if (vap->va_mask & AT_MTIME) {
		vap->va_mtime.tv_sec  = na->mtime.seconds;
		vap->va_mtime.tv_nsec = na->mtime.useconds*1000;
	}

#ifdef NFS_OPS_DEBUG
	if ((boothowto & DBFLAGS) == DBFLAGS)
		printf("nfs_getattr(): done.\n");
#endif
	return (getattr_res.status);
}

/*
 * This version of fstat supports mode, size, and inode # only.
 * It can be enhanced if more is required,
 */

static int
boot_nfs_fstat(int fd, struct stat *stp)
{	struct vattr va;
	struct nfs_files *filep;
	struct nfs_fid nfsfid;
	fid_t *fidp;

	if (fd == 0) {
		dprintf("nfs_fstat(): Bad file number 0.\n");
		return (-1);
	}

	if ((filep = get_filep(fd)) == 0)
		return (-1);

	nfsfid.fh = filep->file.fh;
	fidp = (fid_t *)&nfsfid;
	fidp->fid_len = sizeof (struct nfs_fid) - sizeof (short);

	bzero((char *)&va, sizeof (va));
	va.va_mask = AT_SIZE | AT_MODE | AT_NODEID;
	if (boot_nfs_getattr(fidp, &va) != NFS_OK)
		return (-1);

	if (va.va_size <= (offset_t)MAXOFF_T)
		stp->st_size = (off_t)va.va_size;
	else {
		dprintf("nfs_getattr(): File too large.\n");
		return (-1);
	}
	stp->st_mode = va.va_mode;
	stp->st_ino = va.va_nodeid;

	return (0);
}

static int
boot_nfs_getdents(int fd, struct dirent *dep, unsigned size)
{
	struct nfs_files *filep;
	extern int nfs_getdents(struct nfs_file *nfp,
	    struct dirent *dep, unsigned size);

	if (fd == 0) {
		dprintf("nfs_getdents(): Bad file number 0.\n");
		return (-1);
	}

	if ((filep = get_filep(fd)) == 0)
		return (-1);

	return (nfs_getdents(&filep->file, dep, size));
}

/*
 * Display nfs error messages.
 */
/*ARGSUSED*/
void
nfs_error(enum nfsstat status)
{
	if (boothowto & RB_DEBUG) {
		switch (status) {
		case NFSERR_PERM:
			printf("NFS: Not owner.\n");
			break;
		case NFSERR_NOENT:
#ifdef	NFS_OPS_DEBUG
			printf("NFS: No such file or directory.\n");
#endif	/* NFS_OPS_DEBUG */
			break;
		case NFSERR_IO:
			printf("NFS: IO ERROR occurred on NFS server.\n");
			break;
		case NFSERR_NXIO:
			printf("NFS: No such device or address.\n");
			break;
		case NFSERR_ACCES:
			printf("NFS: Permission denied.\n");
			break;
		case NFSERR_EXIST:
			printf("NFS: File exists.\n");
			break;
		case NFSERR_NODEV:
			printf("NFS: No such device.\n");
			break;
		case NFSERR_NOTDIR:
			printf("NFS: Not a directory.\n");
			break;
		case NFSERR_ISDIR:
			printf("NFS: Is a directory.\n");
			break;
		case NFSERR_FBIG:
			printf("NFS: File too large.\n");
			break;
		case NFSERR_NOSPC:
			printf("NFS: No space left on device.\n");
			break;
		case NFSERR_ROFS:
			printf("NFS: Read-only filesystem.\n");
			break;
		case NFSERR_NAMETOOLONG:
			printf("NFS: File name too long.\n");
			break;
		case NFSERR_NOTEMPTY:
			printf("NFS: Directory not empty.\n");
			break;
		case NFSERR_DQUOT:
			printf("NFS: Disk quota exceeded.\n");
			break;
		case NFSERR_STALE:
			printf("NFS: Stale file handle.\n");
			break;
		case NFSERR_WFLUSH:
			printf("NFS: server's write cache has been flushed.\n");
			break;
		default:
			printf("NFS: unknown error.\n");
			break;
		}
	}
}
