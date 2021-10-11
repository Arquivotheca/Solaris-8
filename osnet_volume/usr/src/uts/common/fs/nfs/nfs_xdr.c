/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986-1989, 1996-1999  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)nfs_xdr.c	1.65	99/04/27 SMI"
/* SVr4.0 1.6	*/


#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/dirent.h>
#include <sys/vfs.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/debug.h>
#include <sys/t_lock.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#include <nfs/nfs.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>

static bool_t xdr_fastshorten(XDR *, uint_t);

/*
 * These are the XDR routines used to serialize and deserialize
 * the various structures passed as parameters accross the network
 * between NFS clients and servers.
 */

/*
 * File access handle
 * The fhandle struct is treated a opaque data on the wire
 */
bool_t
xdr_fhandle(XDR *xdrs, fhandle_t *fh)
{
	int32_t *ptr;
	int32_t *fhp;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		fhp = (int32_t *)fh;
		if (xdrs->x_op == XDR_DECODE) {
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr;
		} else {
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr = *fhp;
		}
		return (TRUE);
	}

	return (xdr_opaque(xdrs, (caddr_t)fh, NFS_FHSIZE));
}

bool_t
xdr_fastfhandle(XDR *xdrs, fhandle_t **fh)
{
	int32_t *ptr;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		*fh = (fhandle_t *)ptr;
		return (TRUE);
	}

	return (FALSE);
}

/*
 * Arguments to remote write and writecache
 */
bool_t
xdr_writeargs(XDR *xdrs, struct nfswriteargs *wa)
{
	int32_t *ptr;
	int32_t *fhp;

	if (xdrs->x_op == XDR_DECODE) {
		wa->wa_args = &wa->wa_args_buf;
		ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)) +
		    3 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			fhp = (int32_t *)&wa->wa_fhandle;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr++;
			wa->wa_begoff = IXDR_GET_U_INT32(ptr);
			wa->wa_offset = IXDR_GET_U_INT32(ptr);
			wa->wa_totcount = IXDR_GET_U_INT32(ptr);
			if (xdrs->x_ops == &xdrmblk_ops)
				return (xdrmblk_getmblk(xdrs, &wa->wa_mblk,
				    &wa->wa_count));
			ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT);
			if (ptr == NULL)
				return (xdr_bytes(xdrs, &wa->wa_data,
				    &wa->wa_count, NFS_MAXDATA));

			wa->wa_count = IXDR_GET_U_INT32(ptr);
			if (wa->wa_count > NFS_MAXDATA)
				return (FALSE);
			if (wa->wa_count == 0)
				return (TRUE);
			if (wa->wa_data == NULL) {
				wa->wa_data = kmem_alloc(wa->wa_count,
				    KM_NOSLEEP);
				if (wa->wa_data == NULL)
					return (FALSE);
			}
			ptr = XDR_INLINE(xdrs, RNDUP(wa->wa_count));
			if (ptr == NULL) {
				return (xdr_opaque(xdrs, wa->wa_data,
				    wa->wa_count));
			}
			bcopy(ptr, wa->wa_data, wa->wa_count);
			return (TRUE);
		}
	}

	if (xdrs->x_op == XDR_ENCODE) {
		ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)) +
		    3 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			fhp = (int32_t *)&wa->wa_fhandle;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp;
			IXDR_PUT_U_INT32(ptr, wa->wa_begoff);
			IXDR_PUT_U_INT32(ptr, wa->wa_offset);
			IXDR_PUT_U_INT32(ptr, wa->wa_totcount);
		} else {
			if (!(xdr_fhandle(xdrs, &wa->wa_fhandle) &&
			    xdr_u_int(xdrs, &wa->wa_begoff) &&
			    xdr_u_int(xdrs, &wa->wa_offset) &&
			    xdr_u_int(xdrs, &wa->wa_totcount)))
				return (FALSE);
		}
#if 0 /* notdef */
		if (wa->wa_mblk != NULL && xdrs->x_ops == &xdrmblk_ops) {
			mblk_t *mp;

			mp = dupb(wa->wa_mblk);
			if (mp != NULL) {
				mp->b_wptr += wa->wa_count;
				if (xdrmblk_putmblk(xdrs, mp,
				    wa->wa_count) == TRUE) {
					return (TRUE);
				} else
					freeb(mp);
			}
			/* else Fall thru for the xdr_bytes() */
		}
		/* wa_mblk == NULL || xdrs->x_ops != &xdrmblk_ops Fall thru */
#endif /* notdef */
		return (xdr_bytes(xdrs, &wa->wa_data, &wa->wa_count,
		    NFS_MAXDATA));
	}

	if (xdrs->x_op == XDR_FREE) {
		if (wa->wa_data != NULL) {
			kmem_free(wa->wa_data, wa->wa_count);
			wa->wa_data = NULL;
		}
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, &wa->wa_fhandle) &&
	    xdr_u_int(xdrs, &wa->wa_begoff) &&
	    xdr_u_int(xdrs, &wa->wa_offset) &&
	    xdr_u_int(xdrs, &wa->wa_totcount) &&
	    (xdrs->x_op == XDR_DECODE && xdrs->x_ops == &xdrmblk_ops) ?
	    xdrmblk_getmblk(xdrs, &wa->wa_mblk, &wa->wa_count) :
	    xdr_bytes(xdrs, &wa->wa_data, &wa->wa_count, NFS_MAXDATA)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastwriteargs(XDR *xdrs, struct nfswriteargs **wapp)
{
	int32_t *ptr;
	struct nfswriteargs *wa;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)) +
				3 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		wa = *wapp;
		wa->wa_args = (struct otw_nfswriteargs *)ptr;
#ifdef _LITTLE_ENDIAN
		wa->wa_begoff = ntohl(wa->wa_begoff);
		wa->wa_offset = ntohl(wa->wa_offset);
		wa->wa_totcount = ntohl(wa->wa_totcount);
#endif

		if (xdrs->x_ops == &xdrmblk_ops) {
			wa->wa_data = NULL;
			return (xdrmblk_getmblk(xdrs, &wa->wa_mblk,
			    &wa->wa_count));
		}

		ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT);
		if (ptr == NULL)
			return (xdr_bytes(xdrs, &wa->wa_data,
			    &wa->wa_count, NFS_MAXDATA));

		wa->wa_count = IXDR_GET_U_INT32(ptr);
		if (wa->wa_count > NFS_MAXDATA)
			return (FALSE);
		if (wa->wa_count == 0)
			return (TRUE);
		if (wa->wa_data == NULL) {
			wa->wa_data = kmem_alloc(wa->wa_count, KM_NOSLEEP);
			if (wa->wa_data == NULL)
				return (FALSE);
		}
		ptr = XDR_INLINE(xdrs, RNDUP(wa->wa_count));
		if (ptr == NULL)
			return (xdr_opaque(xdrs, wa->wa_data, wa->wa_count));
		bcopy(ptr, wa->wa_data, wa->wa_count);
		return (TRUE);
	}

	return (FALSE);
}

/*
 * File attributes
 */
bool_t
xdr_fattr(XDR *xdrs, struct nfsfattr *na)
{
	int32_t *ptr;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs, 17 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			na->na_type = IXDR_GET_ENUM(ptr, enum nfsftype);
			na->na_mode = IXDR_GET_U_INT32(ptr);
			na->na_nlink = IXDR_GET_U_INT32(ptr);
			na->na_uid = IXDR_GET_U_INT32(ptr);
			na->na_gid = IXDR_GET_U_INT32(ptr);
			na->na_size = IXDR_GET_U_INT32(ptr);
			na->na_blocksize = IXDR_GET_U_INT32(ptr);
			na->na_rdev = IXDR_GET_U_INT32(ptr);
			na->na_blocks = IXDR_GET_U_INT32(ptr);
			na->na_fsid = IXDR_GET_U_INT32(ptr);
			na->na_nodeid = IXDR_GET_U_INT32(ptr);
			na->na_atime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_atime.tv_usec = IXDR_GET_U_INT32(ptr);
			na->na_mtime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_mtime.tv_usec = IXDR_GET_U_INT32(ptr);
			na->na_ctime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_ctime.tv_usec = IXDR_GET_U_INT32(ptr);
		} else {
			IXDR_PUT_ENUM(ptr, na->na_type);
			IXDR_PUT_U_INT32(ptr, na->na_mode);
			IXDR_PUT_U_INT32(ptr, na->na_nlink);
			IXDR_PUT_U_INT32(ptr, na->na_uid);
			IXDR_PUT_U_INT32(ptr, na->na_gid);
			IXDR_PUT_U_INT32(ptr, na->na_size);
			IXDR_PUT_U_INT32(ptr, na->na_blocksize);
			IXDR_PUT_U_INT32(ptr, na->na_rdev);
			IXDR_PUT_U_INT32(ptr, na->na_blocks);
			IXDR_PUT_U_INT32(ptr, na->na_fsid);
			IXDR_PUT_U_INT32(ptr, na->na_nodeid);
			IXDR_PUT_U_INT32(ptr, na->na_atime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_atime.tv_usec);
			IXDR_PUT_U_INT32(ptr, na->na_mtime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_mtime.tv_usec);
			IXDR_PUT_U_INT32(ptr, na->na_ctime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_ctime.tv_usec);
		}
		return (TRUE);
	}

	if (xdr_enum(xdrs, (enum_t *)&na->na_type) &&
	    xdr_u_int(xdrs, &na->na_mode) &&
	    xdr_u_int(xdrs, &na->na_nlink) &&
	    xdr_u_int(xdrs, &na->na_uid) &&
	    xdr_u_int(xdrs, &na->na_gid) &&
	    xdr_u_int(xdrs, &na->na_size) &&
	    xdr_u_int(xdrs, &na->na_blocksize) &&
	    xdr_u_int(xdrs, &na->na_rdev) &&
	    xdr_u_int(xdrs, &na->na_blocks) &&
	    xdr_u_int(xdrs, &na->na_fsid) &&
	    xdr_u_int(xdrs, &na->na_nodeid) &&
	    xdr_nfs2_timeval(xdrs, &na->na_atime) &&
	    xdr_nfs2_timeval(xdrs, &na->na_mtime) &&
	    xdr_nfs2_timeval(xdrs, &na->na_ctime)) {
		return (TRUE);
	}
	return (FALSE);
}

#ifdef _LITTLE_ENDIAN
bool_t
xdr_fastfattr(XDR *xdrs, struct nfsfattr *na)
{
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	if (xdrs->x_op == XDR_DECODE)
		return (FALSE);

	na->na_type = htonl(na->na_type);
	na->na_mode = htonl(na->na_mode);
	na->na_nlink = htonl(na->na_nlink);
	na->na_uid = htonl(na->na_uid);
	na->na_gid = htonl(na->na_gid);
	na->na_size = htonl(na->na_size);
	na->na_blocksize = htonl(na->na_blocksize);
	na->na_rdev = htonl(na->na_rdev);
	na->na_blocks = htonl(na->na_blocks);
	na->na_fsid = htonl(na->na_fsid);
	na->na_nodeid = htonl(na->na_nodeid);
	na->na_atime.tv_sec = htonl(na->na_atime.tv_sec);
	na->na_atime.tv_usec = htonl(na->na_atime.tv_usec);
	na->na_mtime.tv_sec = htonl(na->na_mtime.tv_sec);
	na->na_mtime.tv_usec = htonl(na->na_mtime.tv_usec);
	na->na_ctime.tv_sec = htonl(na->na_ctime.tv_sec);
	na->na_ctime.tv_usec = htonl(na->na_ctime.tv_usec);
	return (TRUE);
}
#endif

/*
 * Arguments to remote read
 */
bool_t
xdr_readargs(XDR *xdrs, struct nfsreadargs *ra)
{
	int32_t *ptr;
	int32_t *fhp;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs,
			RNDUP(sizeof (fhandle_t)) + 3 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			fhp = (int32_t *)&ra->ra_fhandle;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr++;
			ra->ra_offset = IXDR_GET_INT32(ptr);
			ra->ra_count = IXDR_GET_INT32(ptr);
			ra->ra_totcount = IXDR_GET_INT32(ptr);
		} else {
			fhp = (int32_t *)&ra->ra_fhandle;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp;
			IXDR_PUT_INT32(ptr, ra->ra_offset);
			IXDR_PUT_INT32(ptr, ra->ra_count);
			IXDR_PUT_INT32(ptr, ra->ra_totcount);
		}
		if (ra->ra_count > NFS_MAXDATA)
			return (FALSE);
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, &ra->ra_fhandle) &&
	    xdr_u_int(xdrs, &ra->ra_offset) &&
	    xdr_u_int(xdrs, &ra->ra_count) &&
	    xdr_u_int(xdrs, &ra->ra_totcount)) {
		if (ra->ra_count > NFS_MAXDATA)
			return (FALSE);
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastreadargs(XDR *xdrs, struct nfsreadargs **rapp)
{
	int32_t *ptr;
#ifdef _LITTLE_ENDIAN
	struct nfsreadargs *ra;
#endif

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs,
			RNDUP(sizeof (fhandle_t)) + 3 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		*rapp = (struct nfsreadargs *)ptr;
#ifdef _LITTLE_ENDIAN
		ra = (struct nfsreadargs *)ptr;
		ra->ra_offset = ntohl(ra->ra_offset);
		ra->ra_count = ntohl(ra->ra_count);
		ra->ra_totcount = ntohl(ra->ra_totcount);
#endif
		if ((*rapp)->ra_count > NFS_MAXDATA)
			return (FALSE);
		return (TRUE);
	}

	return (FALSE);
}

/*
 * Status OK portion of remote read reply
 */
bool_t
xdr_rrok(XDR *xdrs, struct nfsrrok *rrok)
{
	bool_t ret;
	mblk_t *mp;

	if (xdr_fattr(xdrs, &rrok->rrok_attr) == FALSE) {
		if (xdrs->x_op == XDR_ENCODE && rrok->rrok_mp != NULL)
			freeb(rrok->rrok_mp);
		return (FALSE);
	}
	if (xdrs->x_op == XDR_ENCODE) {
		int i, rndup;

		mp = rrok->rrok_mp;
		if (mp != NULL && xdrs->x_ops == &xdrmblk_ops) {
			mp->b_wptr += rrok->rrok_count;
			rndup = BYTES_PER_XDR_UNIT -
				(rrok->rrok_count % BYTES_PER_XDR_UNIT);
			if (rndup != BYTES_PER_XDR_UNIT)
				for (i = 0; i < rndup; i++)
					*mp->b_wptr++ = '\0';
			if (xdrmblk_putmblk(xdrs, mp,
					    rrok->rrok_count) == TRUE)
				return (TRUE);
		}
		/*
		 * Fall thru for the xdr_bytes() - freeb() the mblk
		 * after the copy
		 */
	}
	ret = xdr_bytes(xdrs, (char **)&rrok->rrok_data,
	    &rrok->rrok_count, NFS_MAXDATA);
	if (xdrs->x_op == XDR_ENCODE) {
		if (mp != NULL)
			freeb(mp);
	}
	return (ret);
}

static struct xdr_discrim rdres_discrim[2] = {
	{ NFS_OK, xdr_rrok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Reply from remote read
 */
bool_t
xdr_rdresult(XDR *xdrs, struct nfsrdresult *rr)
{
	return (xdr_union(xdrs, (enum_t *)&(rr->rr_status),
	    (caddr_t)&(rr->rr_ok), rdres_discrim, xdr_void));
}

/*
 * File attributes which can be set
 */
bool_t
xdr_sattr(XDR *xdrs, struct nfssattr *sa)
{
	if (xdr_u_int(xdrs, &sa->sa_mode) &&
	    xdr_u_int(xdrs, &sa->sa_uid) &&
	    xdr_u_int(xdrs, &sa->sa_gid) &&
	    xdr_u_int(xdrs, &sa->sa_size) &&
	    xdr_nfs2_timeval(xdrs, &sa->sa_atime) &&
	    xdr_nfs2_timeval(xdrs, &sa->sa_mtime)) {
		return (TRUE);
	}
	return (FALSE);
}

static struct xdr_discrim attrstat_discrim[2] = {
	{ (int)NFS_OK, xdr_fattr },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Reply status with file attributes
 */
bool_t
xdr_attrstat(XDR *xdrs, struct nfsattrstat *ns)
{
	return (xdr_union(xdrs, (enum_t *)&(ns->ns_status),
	    (caddr_t)&(ns->ns_attr), attrstat_discrim, xdr_void));
}

/*
 * Fast reply status with file attributes
 */
bool_t
xdr_fastattrstat(XDR *xdrs, struct nfsattrstat *ns)
{
#if defined(_LITTLE_ENDIAN)
	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (!xdr_fastenum(xdrs, (enum_t *)&ns->ns_status))
		return (FALSE);

	if (ns->ns_status == NFS_OK)
		return (xdr_fastfattr(xdrs, &ns->ns_attr));
#elif defined(_BIG_ENDIAN)
	if (ns->ns_status == NFS_OK)
		return (TRUE);
#endif
	return (xdr_fastshorten(xdrs, sizeof (*ns)));
}

/*
 * NFS_OK part of read sym link reply union
 */
bool_t
xdr_srok(XDR *xdrs, struct nfssrok *srok)
{
	int32_t *ptr;
	uint32_t size;
	int i;
	int rndup;
	char *cptr;

	if (xdrs->x_op == XDR_DECODE) {
		ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			size = IXDR_GET_U_INT32(ptr);
			srok->srok_count = (uint32_t)size;
			if (size > NFS_MAXPATHLEN)
				return (FALSE);
			if (size == 0)
				return (TRUE);
			if (srok->srok_data == NULL) {
				srok->srok_data = kmem_alloc(size, KM_NOSLEEP);
				if (srok->srok_data == NULL)
					return (FALSE);
			}
			ptr = XDR_INLINE(xdrs, RNDUP(size));
			if (ptr == NULL)
				return (xdr_opaque(xdrs, srok->srok_data,
						    size));
			bcopy(ptr, srok->srok_data, size);
			return (TRUE);
		}
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (srok->srok_count > NFS_MAXPATHLEN)
			return (FALSE);
		size = srok->srok_count;
		ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT + RNDUP(size));
		if (ptr != NULL) {
			IXDR_PUT_U_INT32(ptr, (uint32_t)size);
			bcopy(srok->srok_data, ptr, size);
			rndup = BYTES_PER_XDR_UNIT -
				(size % BYTES_PER_XDR_UNIT);
			if (rndup != BYTES_PER_XDR_UNIT) {
				cptr = (char *)ptr + size;
				for (i = 0; i < rndup; i++)
					*cptr++ = '\0';
			}
			return (TRUE);
		}
	}

	if (xdrs->x_op == XDR_FREE) {
		if (srok->srok_data != NULL) {
			kmem_free(srok->srok_data, srok->srok_count);
			srok->srok_data = NULL;
		}
		return (TRUE);
	}

	if (xdr_bytes(xdrs, &srok->srok_data, &srok->srok_count,
	    NFS_MAXPATHLEN)) {
		return (TRUE);
	}
	return (FALSE);
}

static struct xdr_discrim rdlnres_discrim[2] = {
	{ (int)NFS_OK, xdr_srok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Result of reading symbolic link
 */
bool_t
xdr_rdlnres(XDR *xdrs, struct nfsrdlnres *rl)
{
	return (xdr_union(xdrs, (enum_t *)&(rl->rl_status),
	    (caddr_t)&(rl->rl_srok), rdlnres_discrim, xdr_void));
}

/*
 * Arguments to readdir
 */
bool_t
xdr_rddirargs(XDR *xdrs, struct nfsrddirargs *rda)
{
	int32_t *ptr;
	int32_t *fhp;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs,
	    RNDUP(sizeof (fhandle_t)) + 2 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			fhp = (int32_t *)&rda->rda_fh;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr++;
			rda->rda_offset = IXDR_GET_U_INT32(ptr);
			rda->rda_count = IXDR_GET_U_INT32(ptr);
		} else {
			fhp = (int32_t *)&rda->rda_fh;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp;
			IXDR_PUT_U_INT32(ptr, rda->rda_offset);
			IXDR_PUT_U_INT32(ptr, rda->rda_count);
		}
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, &rda->rda_fh) &&
	    xdr_u_int(xdrs, &rda->rda_offset) &&
	    xdr_u_int(xdrs, &rda->rda_count)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastrddirargs(XDR *xdrs, struct nfsrddirargs **rdapp)
{
	int32_t *ptr;
#ifdef _LITTLE_ENDIAN
	struct nfsrddirargs *rda;
#endif

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs,
	    RNDUP(sizeof (fhandle_t)) + 2 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		*rdapp = (struct nfsrddirargs *)ptr;
#ifdef _LITTLE_ENDIAN
		rda = (struct nfsrddirargs *)ptr;
		rda->rda_offset = ntohl(rda->rda_offset);
		rda->rda_count = ntohl(rda->rda_count);
#endif
		return (TRUE);
	}

	return (FALSE);
}

/*
 * Directory read reply:
 * union (enum status) {
 *	NFS_OK: entlist;
 *		boolean eof;
 *	default:
 * }
 *
 * Directory entries
 *	struct  direct {
 *		off_t   d_off;			* offset of next entry *
 *		u_int	d_fileno;		* inode number of entry *
 *		u_short d_reclen;		* length of this record *
 *		u_short d_namlen;		* length of string in d_name *
 *		char    d_name[MAXNAMLEN + 1];	* name no longer than this *
 *	};
 * are on the wire as:
 * union entlist (boolean valid) {
 * 	TRUE:	struct otw_dirent;
 *		u_int nxtoffset;
 *		union entlist;
 *	FALSE:
 * }
 * where otw_dirent is:
 * 	struct dirent {
 *		u_int	de_fid;
 *		string	de_name<NFS_MAXNAMELEN>;
 *	}
 */

#ifdef nextdp
#undef	nextdp
#endif
#define	nextdp(dp)	((struct dirent64 *)((char *)(dp) + (dp)->d_reclen))
#ifdef roundup
#undef	roundup
#endif
#define	roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

/* decline to serve files with large inode numbers if set; on by default */
int nfs2_limit_inum = 1;

/*
 * ENCODE ONLY
 */
bool_t
xdr_putrddirres(XDR *xdrs, struct nfsrddirres *rd)
{
	struct dirent64 *dp;
	char *name;
	int size;
	uint_t namlen;
	bool_t true = TRUE;
	bool_t false = FALSE;
	int entrysz;
	int tofit;
	int bufsize;
	uint32_t ino, off;

	if (xdrs->x_op != XDR_ENCODE)
		return (FALSE);
	if (!xdr_enum(xdrs, (enum_t *)&rd->rd_status))
		return (FALSE);
	if (rd->rd_status != NFS_OK)
		return (TRUE);

	bufsize = 1 * BYTES_PER_XDR_UNIT;
	for (size = rd->rd_size, dp = rd->rd_entries;
		size > 0;
		size -= dp->d_reclen, dp = nextdp(dp)) {
		if (dp->d_reclen == 0 /* || DIRSIZ(dp) > dp->d_reclen */)
			return (FALSE);
		if (dp->d_ino == 0)
			continue;
		if (dp->d_ino > MAXOFF32_T && nfs2_limit_inum)
			return (FALSE);
		ino = (uint32_t)dp->d_ino; /* for LP64 we clip the bits */
		off = (uint32_t)dp->d_off;
		name = dp->d_name;
		namlen = (uint_t)strlen(name);
		entrysz = (1 + 1 + 1 + 1) * BYTES_PER_XDR_UNIT +
		    roundup(namlen, BYTES_PER_XDR_UNIT);
		tofit = entrysz + 2 * BYTES_PER_XDR_UNIT;
		if (bufsize + tofit > rd->rd_bufsize) {
			rd->rd_eof = FALSE;
			break;
		}
		if (!xdr_bool(xdrs, &true) ||
		    !xdr_u_int(xdrs, &ino) ||
		    !xdr_bytes(xdrs, &name, &namlen, NFS_MAXNAMLEN) ||
		    !xdr_u_int(xdrs, &off)) {
			return (FALSE);
		}
		bufsize += entrysz;
	}
	if (!xdr_bool(xdrs, &false))
		return (FALSE);
	if (!xdr_bool(xdrs, &rd->rd_eof))
		return (FALSE);
	return (TRUE);
}

/*
 * DECODE ONLY
 */
bool_t
xdr_getrddirres(XDR *xdrs, struct nfsrddirres *rd)
{
	struct dirent64 *dp;
	uint_t namlen;
	int size;
	bool_t valid;
	uint32_t offset;
	uint_t fileid;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	if (!xdr_enum(xdrs, (enum_t *)&rd->rd_status))
		return (FALSE);
	if (rd->rd_status != NFS_OK)
		return (TRUE);

	size = rd->rd_size;
	dp = rd->rd_entries;
	offset = rd->rd_offset;
	for (;;) {
		if (!xdr_bool(xdrs, &valid))
			return (FALSE);
		if (!valid)
			break;
		if (!xdr_u_int(xdrs, &fileid) ||
		    !xdr_u_int(xdrs, &namlen))
			return (FALSE);
		if (DIRENT64_RECLEN(namlen) > size) {
			rd->rd_eof = FALSE;
			goto bufovflw;
		}
		if (!xdr_opaque(xdrs, dp->d_name, namlen)||
		    !xdr_u_int(xdrs, &offset)) {
			return (FALSE);
		}
		dp->d_ino = (long)fileid;
		dp->d_reclen = DIRENT64_RECLEN(namlen);
		dp->d_name[namlen] = '\0';
		dp->d_off = (off64_t)offset;
		size -= dp->d_reclen;
		dp = nextdp(dp);
	}
	if (!xdr_bool(xdrs, &rd->rd_eof))
		return (FALSE);
bufovflw:
	rd->rd_size = (uint32_t)((char *)dp - (char *)(rd->rd_entries));
	rd->rd_offset = offset;
	return (TRUE);
}

/*
 * Arguments for directory operations
 */
bool_t
xdr_diropargs(XDR *xdrs, struct nfsdiropargs *da)
{
	int32_t *ptr;
	int32_t *fhp;
	uint32_t size;
	uint32_t nodesize;
	int i;
	int rndup;
	char *cptr;

	if (xdrs->x_op == XDR_DECODE) {
		da->da_fhandle = &da->da_fhandle_buf;
		ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)) +
		    1 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			fhp = (int32_t *)da->da_fhandle;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr++;
			size = IXDR_GET_U_INT32(ptr);
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				return (TRUE);
			if (da->da_name == NULL) {
				da->da_name = kmem_alloc(nodesize, KM_NOSLEEP);
				if (da->da_name == NULL)
					return (FALSE);
				da->da_flags |= DA_FREENAME;
			}
			ptr = XDR_INLINE(xdrs, RNDUP(size));
			if (ptr == NULL) {
				if (!xdr_opaque(xdrs, da->da_name, size))
					return (FALSE);
				da->da_name[size] = '\0';
				if (strlen(da->da_name) != size) {
					if (da->da_flags & DA_FREENAME) {
						kmem_free(da->da_name,
						    nodesize);
						da->da_name = NULL;
					}
					return (FALSE);
				}
				return (TRUE);
			}
			bcopy(ptr, da->da_name, size);
			da->da_name[size] = '\0';
			if (strlen(da->da_name) != size) {
				if (da->da_flags & DA_FREENAME) {
					kmem_free(da->da_name, nodesize);
					da->da_name = NULL;
				}
				return (FALSE);
			}
			return (TRUE);
		}
		if (da->da_name == NULL)
			da->da_flags |= DA_FREENAME;
	}

	if (xdrs->x_op == XDR_ENCODE) {
		size = (uint32_t)strlen(da->da_name);
		if (size > NFS_MAXNAMLEN)
			return (FALSE);
		ptr = XDR_INLINE(xdrs, (int)(RNDUP(sizeof (fhandle_t)) +
		    1 * BYTES_PER_XDR_UNIT + RNDUP(size)));
		if (ptr != NULL) {
			fhp = (int32_t *)da->da_fhandle;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp;
			IXDR_PUT_U_INT32(ptr, (uint32_t)size);
			bcopy(da->da_name, ptr, size);
			rndup = BYTES_PER_XDR_UNIT -
				(size % BYTES_PER_XDR_UNIT);
			if (rndup != BYTES_PER_XDR_UNIT) {
				cptr = (char *)ptr + size;
				for (i = 0; i < rndup; i++)
					*cptr++ = '\0';
			}
			return (TRUE);
		}
	}

	if (xdrs->x_op == XDR_FREE) {
		if (da->da_name == NULL)
			return (TRUE);
		size = (uint32_t)strlen(da->da_name);
		if (size > NFS_MAXNAMLEN)
			return (FALSE);
		if (da->da_flags & DA_FREENAME)
			kmem_free(da->da_name, size + 1);
		da->da_name = NULL;
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, da->da_fhandle) &&
	    xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN)) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * `fast' XDR decode of directory operation arguments.  The algorithm
 * attempts to avoid copies as much as possible by mapping an args
 * structure over the memory containing the incoming request.
 *
 * The incoming structure looks like:
 * ------------------------------------------------
 * | sizeof(fhandle_t) (32) bytes of file handle  |
 * ------------------------------------------------
 * | 1 BYTES_PER_XDR_UNIT (4) unit containing the |
 * | length of the file name			  |
 * ------------------------------------------------
 * | bytes containing the file name, the actual	  |
 * | number of bytes of rounded up to a multiple  |
 * | of BYTES_PER_XDR_UNIT.			  |
 * ------------------------------------------------
 *
 * The macro, RNDUP, is used to round values to a multiple of
 * BYTES_PER_XDR_UNIT.
 *
 * Some trickiness is done to avoid copying the string if at all
 * possible.  Since XDR always rounds the number of bytes in a
 * string up to a multiple of BYTES_PER_XDR_UNIT, unless the
 * length of the string was a multiple of BYTES_PER_XDR_UNIT,
 * then there will be anywhere from 1 to 3 bytes of extra space
 * in the last BYTES_PER_XDR_UNIT space that the string was
 * encoded into.  This space is used to null terminate the string.
 * If length of the string happens to be a multiple of
 * BYTES_PER_XDR_UNIT, then instead of allocating space to
 * store the string, the string is copied up and over the bytes
 * containing the length to make room null terminate the string.
 * This avoids a space allocation.
 */
bool_t
xdr_fastdiropargs(XDR *xdrs, struct nfsdiropargs **dapp)
{
	int32_t *ptr;
	uint32_t size;
	uint32_t nodesize;
	struct nfsdiropargs *da;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		da = *dapp;
		da->da_fhandle = (fhandle_t *)ptr;
		da->da_name = NULL;
		da->da_flags = 0;
		if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
			da->da_flags |= DA_FREENAME;
			return (xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN));
		}
		if (size > NFS_MAXNAMLEN)
			return (FALSE);
		nodesize = size + 1;
		if (nodesize == 0)
			return (TRUE);
		ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT + RNDUP(size));
		if (ptr != NULL) {
			if ((size % BYTES_PER_XDR_UNIT) != 0)
				da->da_name = (char *)(ptr + 1);
			else {
				da->da_name = (char *)ptr;
				bcopy(ptr + 1, da->da_name, size);
			}
			da->da_name[size] = '\0';
			return (TRUE);
		}
		da->da_flags |= DA_FREENAME;
		return (xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN));
	}

	return (FALSE);
}

/*
 * NFS_OK part of directory operation result
 */
bool_t
xdr_drok(XDR *xdrs, struct nfsdrok *drok)
{
	int32_t *ptr;
	int32_t *fhp;
	struct nfsfattr *na;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs,
	    RNDUP(sizeof (fhandle_t)) + 17 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			fhp = (int32_t *)&drok->drok_fhandle;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp++ = *ptr++;
			*fhp = *ptr++;
			na = &drok->drok_attr;
			na->na_type = IXDR_GET_ENUM(ptr, enum nfsftype);
			na->na_mode = IXDR_GET_U_INT32(ptr);
			na->na_nlink = IXDR_GET_U_INT32(ptr);
			na->na_uid = IXDR_GET_U_INT32(ptr);
			na->na_gid = IXDR_GET_U_INT32(ptr);
			na->na_size = IXDR_GET_U_INT32(ptr);
			na->na_blocksize = IXDR_GET_U_INT32(ptr);
			na->na_rdev = IXDR_GET_U_INT32(ptr);
			na->na_blocks = IXDR_GET_U_INT32(ptr);
			na->na_fsid = IXDR_GET_U_INT32(ptr);
			na->na_nodeid = IXDR_GET_U_INT32(ptr);
			na->na_atime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_atime.tv_usec = IXDR_GET_U_INT32(ptr);
			na->na_mtime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_mtime.tv_usec = IXDR_GET_U_INT32(ptr);
			na->na_ctime.tv_sec = IXDR_GET_U_INT32(ptr);
			na->na_ctime.tv_usec = IXDR_GET_U_INT32(ptr);
		} else {
			fhp = (int32_t *)&drok->drok_fhandle;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp++;
			*ptr++ = *fhp;
			na = &drok->drok_attr;
			IXDR_PUT_ENUM(ptr, na->na_type);
			IXDR_PUT_U_INT32(ptr, na->na_mode);
			IXDR_PUT_U_INT32(ptr, na->na_nlink);
			IXDR_PUT_U_INT32(ptr, na->na_uid);
			IXDR_PUT_U_INT32(ptr, na->na_gid);
			IXDR_PUT_U_INT32(ptr, na->na_size);
			IXDR_PUT_U_INT32(ptr, na->na_blocksize);
			IXDR_PUT_U_INT32(ptr, na->na_rdev);
			IXDR_PUT_U_INT32(ptr, na->na_blocks);
			IXDR_PUT_U_INT32(ptr, na->na_fsid);
			IXDR_PUT_U_INT32(ptr, na->na_nodeid);
			IXDR_PUT_U_INT32(ptr, na->na_atime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_atime.tv_usec);
			IXDR_PUT_U_INT32(ptr, na->na_mtime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_mtime.tv_usec);
			IXDR_PUT_U_INT32(ptr, na->na_ctime.tv_sec);
			IXDR_PUT_U_INT32(ptr, na->na_ctime.tv_usec);
		}
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, &drok->drok_fhandle) &&
	    xdr_fattr(xdrs, &drok->drok_attr)) {
		return (TRUE);
	}
	return (FALSE);
}

#ifdef _LITTLE_ENDIAN
bool_t
xdr_fastdrok(XDR *xdrs, struct nfsdrok *drok)
{
	struct nfsfattr *na;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	if (xdrs->x_op == XDR_DECODE)
		return (FALSE);

	na = &drok->drok_attr;
	na->na_type = (enum nfsftype)htonl(na->na_type);
	na->na_mode = (uint32_t)htonl(na->na_mode);
	na->na_nlink = (uint32_t)htonl(na->na_nlink);
	na->na_uid = (uint32_t)htonl(na->na_uid);
	na->na_gid = (uint32_t)htonl(na->na_gid);
	na->na_size = (uint32_t)htonl(na->na_size);
	na->na_blocksize = (uint32_t)htonl(na->na_blocksize);
	na->na_rdev = (uint32_t)htonl(na->na_rdev);
	na->na_blocks = (uint32_t)htonl(na->na_blocks);
	na->na_fsid = (uint32_t)htonl(na->na_fsid);
	na->na_nodeid = (uint32_t)htonl(na->na_nodeid);
	na->na_atime.tv_sec = htonl(na->na_atime.tv_sec);
	na->na_atime.tv_usec = htonl(na->na_atime.tv_usec);
	na->na_mtime.tv_sec = htonl(na->na_mtime.tv_sec);
	na->na_mtime.tv_usec = htonl(na->na_mtime.tv_usec);
	na->na_ctime.tv_sec = htonl(na->na_ctime.tv_sec);
	na->na_ctime.tv_usec = htonl(na->na_ctime.tv_usec);
	return (TRUE);
}
#endif

static struct xdr_discrim diropres_discrim[2] = {
	{ NFS_OK, xdr_drok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Results from directory operation
 */
bool_t
xdr_diropres(XDR *xdrs, struct nfsdiropres *dr)
{
	return (xdr_union(xdrs, (enum_t *)&(dr->dr_status),
	    (caddr_t)&(dr->dr_drok), diropres_discrim, xdr_void));
}

/*
 * Results from directory operation
 */
bool_t
xdr_fastdiropres(XDR *xdrs, struct nfsdiropres *dr)
{
#if defined(_LITTLE_ENDIAN)
	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (!xdr_fastenum(xdrs, (enum_t *)&dr->dr_status))
		return (FALSE);

	if (dr->dr_status == NFS_OK)
		return (xdr_fastdrok(xdrs, &dr->dr_drok));
#elif defined(_BIG_ENDIAN)
	if (dr->dr_status == NFS_OK)
		return (TRUE);
#endif
	return (xdr_fastshorten(xdrs, sizeof (*dr)));
}

/*
 * Time Structure, unsigned
 */
bool_t
xdr_nfs2_timeval(XDR *xdrs, struct nfs2_timeval *tv)
{
	if (xdr_u_int(xdrs, &tv->tv_sec) &&
	    xdr_u_int(xdrs, &tv->tv_usec))
		return (TRUE);
	return (FALSE);
}

/*
 * arguments to setattr
 */
bool_t
xdr_saargs(XDR *xdrs, struct nfssaargs *argp)
{
	int32_t *ptr;
	int32_t *arg;
	struct nfssattr *sa;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs,
	    RNDUP(sizeof (fhandle_t)) + 8 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			arg = (int32_t *)&argp->saa_fh;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg++ = *ptr++;
			*arg = *ptr++;
			sa = &argp->saa_sa;
			sa->sa_mode = IXDR_GET_U_INT32(ptr);
			sa->sa_uid = IXDR_GET_U_INT32(ptr);
			sa->sa_gid = IXDR_GET_U_INT32(ptr);
			sa->sa_size = IXDR_GET_U_INT32(ptr);
			sa->sa_atime.tv_sec = IXDR_GET_U_INT32(ptr);
			sa->sa_atime.tv_usec = IXDR_GET_U_INT32(ptr);
			sa->sa_mtime.tv_sec = IXDR_GET_U_INT32(ptr);
			sa->sa_mtime.tv_usec = IXDR_GET_U_INT32(ptr);
		} else {
			arg = (int32_t *)&argp->saa_fh;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg++;
			*ptr++ = *arg;
			sa = &argp->saa_sa;
			IXDR_PUT_U_INT32(ptr, sa->sa_mode);
			IXDR_PUT_U_INT32(ptr, sa->sa_uid);
			IXDR_PUT_U_INT32(ptr, sa->sa_gid);
			IXDR_PUT_U_INT32(ptr, sa->sa_size);
			IXDR_PUT_U_INT32(ptr, sa->sa_atime.tv_sec);
			IXDR_PUT_U_INT32(ptr, sa->sa_atime.tv_usec);
			IXDR_PUT_U_INT32(ptr, sa->sa_mtime.tv_sec);
			IXDR_PUT_U_INT32(ptr, sa->sa_mtime.tv_usec);
		}
		return (TRUE);
	}

	if (xdr_fhandle(xdrs, &argp->saa_fh) &&
	    xdr_sattr(xdrs, &argp->saa_sa)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastsaargs(XDR *xdrs, struct nfssaargs **argpp)
{
	int32_t *ptr;
#ifdef _LITTLE_ENDIAN
	struct nfssattr *sa;
#endif

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs,
	    RNDUP(sizeof (fhandle_t)) + 8 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		*argpp = (struct nfssaargs *)ptr;
#ifdef _LITTLE_ENDIAN
		sa = &(*argpp)->saa_sa;
		sa->sa_mode = ntohl(sa->sa_mode);
		sa->sa_uid = ntohl(sa->sa_uid);
		sa->sa_gid = ntohl(sa->sa_gid);
		sa->sa_size = ntohl(sa->sa_size);
		sa->sa_atime.tv_sec = ntohl(sa->sa_atime.tv_sec);
		sa->sa_atime.tv_usec = ntohl(sa->sa_atime.tv_usec);
		sa->sa_mtime.tv_sec = ntohl(sa->sa_mtime.tv_sec);
		sa->sa_mtime.tv_usec = ntohl(sa->sa_mtime.tv_usec);
#endif
		return (TRUE);
	}

	return (FALSE);
}

/*
 * arguments to create and mkdir
 */
bool_t
xdr_creatargs(XDR *xdrs, struct nfscreatargs *argp)
{
	argp->ca_sa = &argp->ca_sa_buf;

	if (xdrs->x_op == XDR_DECODE)
		argp->ca_sa = &argp->ca_sa_buf;
	if (xdr_diropargs(xdrs, &argp->ca_da) &&
	    xdr_sattr(xdrs, argp->ca_sa)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastcreatargs(XDR *xdrs, struct nfscreatargs **argpp)
{
	int32_t *ptr;
	uint32_t size;
	uint32_t nodesize;
	struct nfscreatargs *arg;
	struct nfsdiropargs *da;
#ifdef _LITTLE_ENDIAN
	struct nfssattr *sa;
#endif

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		arg = *argpp;
		da = &arg->ca_da;
		da->da_fhandle = (fhandle_t *)ptr;
		da->da_name = NULL;
		da->da_flags = 0;
		if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
			da->da_flags |= DA_FREENAME;
			if (!xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN))
				return (FALSE);
		} else {
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				(void) XDR_GETINT32(xdrs, (int32_t *)&size);
			else {
				ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
				    RNDUP(size));
				if (ptr != NULL) {
					if ((size % BYTES_PER_XDR_UNIT) != 0)
						da->da_name = (char *)(ptr + 1);
					else {
						da->da_name = (char *)ptr;
						bcopy(ptr + 1, da->da_name,
						    size);
					}
					da->da_name[size] = '\0';
				} else {
					da->da_flags |= DA_FREENAME;
					if (!xdr_string(xdrs, &da->da_name,
					    NFS_MAXNAMLEN))
						return (FALSE);
				}
			}
		}
		ptr = XDR_INLINE(xdrs, 8 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			arg->ca_sa = (struct nfssattr *)ptr;
#ifdef _LITTLE_ENDIAN
			sa = arg->ca_sa;
			sa->sa_mode = ntohl(sa->sa_mode);
			sa->sa_uid = ntohl(sa->sa_uid);
			sa->sa_gid = ntohl(sa->sa_gid);
			sa->sa_size = ntohl(sa->sa_size);
			sa->sa_atime.tv_sec = ntohl(sa->sa_atime.tv_sec);
			sa->sa_atime.tv_usec = ntohl(sa->sa_atime.tv_usec);
			sa->sa_mtime.tv_sec = ntohl(sa->sa_mtime.tv_sec);
			sa->sa_mtime.tv_usec = ntohl(sa->sa_mtime.tv_usec);
#endif
			return (TRUE);
		}
		arg->ca_sa = &arg->ca_sa_buf;
		return (xdr_sattr(xdrs, arg->ca_sa));
	}

	return (FALSE);
}

/*
 * arguments to link
 */
bool_t
xdr_linkargs(XDR *xdrs, struct nfslinkargs *argp)
{
	if (xdrs->x_op == XDR_DECODE)
		argp->la_from = &argp->la_from_buf;
	if (xdr_fhandle(xdrs, argp->la_from) &&
	    xdr_diropargs(xdrs, &argp->la_to)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastlinkargs(XDR *xdrs, struct nfslinkargs **argpp)
{
	int32_t *ptr;
	struct nfslinkargs *argp;
	uint32_t size;
	uint32_t nodesize;
	struct nfsdiropargs *da;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		argp = *argpp;
		argp->la_from = (fhandle_t *)ptr;
		da = &argp->la_to;
		da->da_flags = 0;
		ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
		if (ptr != NULL) {
			da->da_fhandle = (fhandle_t *)ptr;
			if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
				da->da_name = NULL;
				da->da_flags |= DA_FREENAME;
				return (xdr_string(xdrs, &da->da_name,
				    NFS_MAXNAMLEN));
			}
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				return (TRUE);
			ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
			    RNDUP(size));
			if (ptr != NULL) {
				if ((size % BYTES_PER_XDR_UNIT) != 0)
					da->da_name = (char *)(ptr + 1);
				else {
					da->da_name = (char *)ptr;
					bcopy(ptr + 1, da->da_name, size);
				}
				da->da_name[size] = '\0';
				return (TRUE);
			}
			da->da_name = NULL;
			da->da_flags |= DA_FREENAME;
			return (xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN));
		}
		da->da_name = NULL;
		return (xdr_diropargs(xdrs, &argp->la_to));
	}

	return (FALSE);
}

/*
 * arguments to rename
 */
bool_t
xdr_rnmargs(XDR *xdrs, struct nfsrnmargs *argp)
{
	if (xdr_diropargs(xdrs, &argp->rna_from) &&
	    xdr_diropargs(xdrs, &argp->rna_to))
		return (TRUE);
	return (FALSE);
}

bool_t
xdr_fastrnmargs(XDR *xdrs, struct nfsrnmargs **argpp)
{
	int32_t *ptr;
	uint32_t size;
	uint32_t nodesize;
	struct nfsrnmargs *argp;
	struct nfsdiropargs *da;

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		argp = *argpp;
		da = &argp->rna_from;
		da->da_fhandle = (fhandle_t *)ptr;
		da->da_flags = 0;
		if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
			da->da_name = NULL;
			da->da_flags |= DA_FREENAME;
			if (!xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN))
				return (FALSE);
		} else {
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize != 0) {
				ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
				    RNDUP(size));
				if (ptr != NULL) {
					if ((size % BYTES_PER_XDR_UNIT) != 0)
						da->da_name = (char *)(ptr + 1);
					else {
						da->da_name = (char *)ptr;
						bcopy(ptr + 1, da->da_name,
						    size);
					}
					da->da_name[size] = '\0';
				} else {
					da->da_name = NULL;
					da->da_flags |= DA_FREENAME;
					if (!xdr_string(xdrs, &da->da_name,
					    NFS_MAXNAMLEN))
						return (FALSE);
				}
			} else {
				(void) XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT);
			}
		}
		ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
		da = &argp->rna_to;
		da->da_flags = 0;
		if (ptr != NULL) {
			da->da_fhandle = (fhandle_t *)ptr;
			if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
				da->da_name = NULL;
				da->da_flags |= DA_FREENAME;
				return (xdr_string(xdrs, &da->da_name,
				    NFS_MAXNAMLEN));
			}
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				return (TRUE);
			ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
			    RNDUP(size));
			if (ptr != NULL) {
				if ((size % BYTES_PER_XDR_UNIT) != 0)
					da->da_name = (char *)(ptr + 1);
				else {
					da->da_name = (char *)ptr;
					bcopy(ptr + 1, da->da_name, size);
				}
				da->da_name[size] = '\0';
				return (TRUE);
			}
			da->da_name = NULL;
			da->da_flags |= DA_FREENAME;
			return (xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN));
		}
		da->da_name = NULL;
		return (xdr_diropargs(xdrs, &argp->rna_to));
	}

	return (FALSE);
}

/*
 * arguments to symlink
 */
bool_t
xdr_slargs(XDR *xdrs, struct nfsslargs *argp)
{
	if (xdrs->x_op == XDR_FREE) {
		if (!xdr_diropargs(xdrs, &argp->sla_from))
			return (FALSE);
		if ((argp->sla_tnm_flags & SLA_FREETNM) &&
		    !xdr_string(xdrs, &argp->sla_tnm, (uint_t)NFS_MAXPATHLEN))
			return (FALSE);
		return (TRUE);
	}

	if (xdrs->x_op == XDR_DECODE) {
		argp->sla_sa = &argp->sla_sa_buf;
		if (argp->sla_tnm == NULL)
			argp->sla_tnm_flags |= SLA_FREETNM;
	}

	if (xdr_diropargs(xdrs, &argp->sla_from) &&
	    xdr_string(xdrs, &argp->sla_tnm, (uint_t)NFS_MAXPATHLEN) &&
	    xdr_sattr(xdrs, argp->sla_sa)) {
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_fastslargs(XDR *xdrs, struct nfsslargs **argpp)
{
	int32_t *ptr;
	uint32_t size;
	uint32_t nodesize;
	struct nfsslargs *argp;
	struct nfsdiropargs *da;
#ifdef _LITTLE_ENDIAN
	struct nfssattr *sa;
#endif

	if (xdrs->x_op != XDR_DECODE)
		return (FALSE);

	ptr = XDR_INLINE(xdrs, RNDUP(sizeof (fhandle_t)));
	if (ptr != NULL) {
		argp = *argpp;
		da = &argp->sla_from;
		da->da_fhandle = (fhandle_t *)ptr;
		da->da_flags = 0;
		if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
			da->da_name = NULL;
			da->da_flags |= DA_FREENAME;
			if (!xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN))
				return (FALSE);
		} else {
			if (size > NFS_MAXNAMLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				(void) XDR_GETINT32(xdrs, (int32_t *)&size);
			else {
				ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
				    RNDUP(size));
				if (ptr != NULL) {
					if ((size % BYTES_PER_XDR_UNIT) != 0)
						da->da_name = (char *)(ptr + 1);
					else {
						da->da_name = (char *)ptr;
						bcopy(ptr + 1, da->da_name,
						    size);
					}
					da->da_name[size] = '\0';
				} else {
					da->da_name = NULL;
					da->da_flags |= DA_FREENAME;
					if (!xdr_string(xdrs, &da->da_name,
					    NFS_MAXNAMLEN))
						return (FALSE);
				}
			}
		}
		argp->sla_tnm_flags = 0;
		if (!XDR_CONTROL(xdrs, XDR_PEEK, (void *)&size)) {
			argp->sla_tnm = NULL;
			argp->sla_tnm_flags |= SLA_FREETNM;
			if (!xdr_string(xdrs, &argp->sla_tnm, NFS_MAXPATHLEN))
				return (FALSE);
		} else {
			if (size > NFS_MAXPATHLEN)
				return (FALSE);
			nodesize = size + 1;
			if (nodesize == 0)
				(void) XDR_GETINT32(xdrs, (int32_t *)&size);
			else {
				ptr = XDR_INLINE(xdrs, 1 * BYTES_PER_XDR_UNIT +
				    RNDUP(size));
				if (ptr != NULL) {
					if ((size % BYTES_PER_XDR_UNIT) != 0) {
						argp->sla_tnm =
						    (char *)(ptr + 1);
					} else {
						argp->sla_tnm = (char *)ptr;
						bcopy(ptr + 1, argp->sla_tnm,
						    size);
					}
					argp->sla_tnm[size] = '\0';
				} else {
					argp->sla_tnm = NULL;
					argp->sla_tnm_flags |= SLA_FREETNM;
					if (!xdr_string(xdrs, &argp->sla_tnm,
					    NFS_MAXPATHLEN))
						return (FALSE);
				}
			}
		}
		ptr = XDR_INLINE(xdrs, 8 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			argp->sla_sa = (struct nfssattr *)ptr;
#ifdef _LITTLE_ENDIAN
			sa = argp->sla_sa;
			sa->sa_mode = ntohl(sa->sa_mode);
			sa->sa_uid = ntohl(sa->sa_uid);
			sa->sa_gid = ntohl(sa->sa_gid);
			sa->sa_size = ntohl(sa->sa_size);
			sa->sa_atime.tv_sec = ntohl(sa->sa_atime.tv_sec);
			sa->sa_atime.tv_usec = ntohl(sa->sa_atime.tv_usec);
			sa->sa_mtime.tv_sec = ntohl(sa->sa_mtime.tv_sec);
			sa->sa_mtime.tv_usec = ntohl(sa->sa_mtime.tv_usec);
#endif
			return (TRUE);
		}
		argp->sla_sa = &argp->sla_sa_buf;
		return (xdr_sattr(xdrs, argp->sla_sa));
	}

	return (FALSE);
}

/*
 * NFS_OK part of statfs operation
 */
bool_t
xdr_fsok(XDR *xdrs, struct nfsstatfsok *fsok)
{
	int32_t *ptr;

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	ptr = XDR_INLINE(xdrs, 5 * BYTES_PER_XDR_UNIT);
	if (ptr != NULL) {
		if (xdrs->x_op == XDR_DECODE) {
			fsok->fsok_tsize = IXDR_GET_INT32(ptr);
			fsok->fsok_bsize = IXDR_GET_INT32(ptr);
			fsok->fsok_blocks = IXDR_GET_INT32(ptr);
			fsok->fsok_bfree = IXDR_GET_INT32(ptr);
			fsok->fsok_bavail = IXDR_GET_INT32(ptr);
		} else {
			IXDR_PUT_INT32(ptr, fsok->fsok_tsize);
			IXDR_PUT_INT32(ptr, fsok->fsok_bsize);
			IXDR_PUT_INT32(ptr, fsok->fsok_blocks);
			IXDR_PUT_INT32(ptr, fsok->fsok_bfree);
			IXDR_PUT_INT32(ptr, fsok->fsok_bavail);
		}
		return (TRUE);
	}

	if (xdr_u_int(xdrs, &fsok->fsok_tsize) &&
	    xdr_u_int(xdrs, &fsok->fsok_bsize) &&
	    xdr_u_int(xdrs, &fsok->fsok_blocks) &&
	    xdr_u_int(xdrs, &fsok->fsok_bfree) &&
	    xdr_u_int(xdrs, &fsok->fsok_bavail)) {
		return (TRUE);
	}
	return (FALSE);
}

#ifdef _LITTLE_ENDIAN
bool_t
xdr_fastfsok(XDR *xdrs, struct nfsstatfsok *fsok)
{

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	if (xdrs->x_op == XDR_DECODE)
		return (FALSE);

	fsok->fsok_tsize = htonl(fsok->fsok_tsize);
	fsok->fsok_bsize = htonl(fsok->fsok_bsize);
	fsok->fsok_blocks = htonl(fsok->fsok_blocks);
	fsok->fsok_bfree = htonl(fsok->fsok_bfree);
	fsok->fsok_bavail = htonl(fsok->fsok_bavail);
	return (TRUE);
}
#endif

static struct xdr_discrim statfs_discrim[2] = {
	{ NFS_OK, xdr_fsok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Results of statfs operation
 */
bool_t
xdr_statfs(XDR *xdrs, struct nfsstatfs *fs)
{
	return (xdr_union(xdrs, (enum_t *)&(fs->fs_status),
	    (caddr_t)&(fs->fs_fsok), statfs_discrim, xdr_void));
}

/*
 * Results of statfs operation
 */
bool_t
xdr_faststatfs(XDR *xdrs, struct nfsstatfs *fs)
{
#if defined(_LITTLE_ENDIAN)
	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (!xdr_fastenum(xdrs, (enum_t *)&fs->fs_status))
		return (FALSE);

	if (fs->fs_status == NFS_OK)
		return (xdr_fastfsok(xdrs, &fs->fs_fsok));
#elif defined(_BIG_ENDIAN)
	if (fs->fs_status == NFS_OK)
		return (TRUE);
#endif
	return (xdr_fastshorten(xdrs, sizeof (*fs)));
}

#ifdef _LITTLE_ENDIAN
/*
 * XDR enumerations
 */
#ifndef lint
static enum sizecheck { SIZEVAL } sizecheckvar;	/* used to find the size of */
						/* an enum */
#endif
bool_t
xdr_fastenum(XDR *xdrs, enum_t *ep)
{
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	if (xdrs->x_op == XDR_DECODE)
		return (FALSE);

#ifndef lint
	/*
	 * enums are treated as ints
	 */
	if (sizeof (sizecheckvar) == sizeof (int32_t)) {
		*ep = (enum_t)htonl((int32_t)(*ep));
	} else if (sizeof (sizecheckvar) == sizeof (short)) {
		*ep = (enum_t)htons((short)(*ep));
	} else {
		return (FALSE);
	}
	return (TRUE);
#else
	(void) (xdr_short(xdrs, (short *)ep));
	return (xdr_int(xdrs, (int *)ep));
#endif
}
#endif

static bool_t
xdr_fastshorten(XDR *xdrs, uint_t ressize)
{
	uint_t curpos;

	curpos = XDR_GETPOS(xdrs);
	ressize -= BYTES_PER_XDR_UNIT;
	curpos -= ressize;
	return (XDR_SETPOS(xdrs, curpos));
}
