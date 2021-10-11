/*
 *		PROPRIETARY NOTICE (Combined)
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
 *  	Copyright (c) 1986-1989, 1995-1999 by Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)nfs_export.c	1.78	99/03/17 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/tiuser.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/acl.h>
#include <sys/utsname.h>
#include <netinet/in.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/svc.h>

#include <nfs/nfs.h>
#include <nfs/export.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_acl.h>
#include <nfs/nfs_log.h>

#define	EXPTABLESIZE 16

struct exportinfo *exptable[EXPTABLESIZE];

static int	unexport(fsid_t *, fid_t *);
static int	findexivp(struct exportinfo **, vnode_t *, vnode_t *, cred_t *);
static void	exportfree(struct exportinfo *);
static int	loadindex(struct exportdata *);

extern void	nfsauth_cache_free(struct exportinfo *);
extern int	sec_svc_loadrootnames(int, int, caddr_t **, model_t);
extern void	sec_svc_freerootnames(int, int, caddr_t *);

/*
 * exported_lock	Read/Write lock that protects the exportinfo list.
 *			This lock must be held when searching or modifiying
 *			the exportinfo list.
 */
krwlock_t exported_lock;

/*
 * "public" and default (root) location for public filehandle
 */
struct exportinfo *exi_public, *exi_root;

fid_t exi_rootfid;	/* for checking the default public file handle */

fhandle_t nullfh2;	/* for comparing V2 filehandles */

#define	exptablehash(fsid, fid) (nfs_fhhash((fsid), (fid)) & (EXPTABLESIZE - 1))

/*
 * File handle hash function, good for producing hash values 16 bits wide.
 */
int
nfs_fhhash(fsid_t *fsid, fid_t *fid)
{
	short *data;
	int i, len;
	short h;

	ASSERT(fid != NULL);

	data = (short *)fid->fid_data;

	/* fid_data must be aligned on a short */
	ASSERT((((uintptr_t)data) & (sizeof (short) - 1)) == 0);

	if (fid->fid_len == 10) {
		/*
		 * probably ufs: hash on bytes 4,5 and 8,9
		 */
		return (fsid->val[0] ^ data[2] ^ data[4]);
	}

	if (fid->fid_len == 6) {
		/*
		 * probably hsfs: hash on bytes 0,1 and 4,5
		 */
		return ((fsid->val[0] ^ data[0] ^ data[2]));
	}

	/*
	 * Some other file system. Assume that every byte is
	 * worth hashing.
	 */
	h = (short)fsid->val[0];

	/*
	 * Sanity check the length before using it
	 * blindly in case the client trashed it.
	 */
	if (fid->fid_len > NFS_FHMAXDATA)
		len = 0;
	else
		len = fid->fid_len / sizeof (short);

	/*
	 * This will ignore one byte if len is not a multiple of
	 * of sizeof (short). No big deal since we at least get some
	 * variation with fsid->val[0];
	 */
	for (i = 0; i < len; i++)
		h ^= data[i];

	return ((int)h);
}

/*
 * Counted byte string compare routine, optimized for file ids.
 */
int
nfs_fhbcmp(char *d1, char *d2, int l)
{
	int k;

	/*
	 * We are always passed pointers to the data portions of
	 * two fids, where pointers are always 2 bytes from 32 bit
	 * alignment. If the length is also 2 bytes off word alignment,
	 * we can do word compares, because the two bytes before the fid
	 * data are always the length packed into a 16 bit short, so we
	 * can safely start our comparisons at d1-2 and d2-2.
	 * If the length is 2 bytes off word alignment, that probably
	 * means that first two bytes are zeroes. This means that
	 * first word in each fid, including the length are going to be
	 * equal (we wouldn't call fhbcmp if the lengths weren't the
	 * same). Thus it makes the most sense to start comparing the
	 * last words of each data portion.
	 */

	if ((l & 0x3) == 2) {
		/*
		 * We are going move the data pointers to the
		 * last word. Adding just the length, puts us to the
		 * word past end of the data. So reduce length by one
		 * word length.
		 */
		k = l - 4;
		/*
		 * Both adjusted length and the data pointer are offset two
		 * bytes from word alignment. Adding them together gives
		 * us word alignment.
		 */
		d1 += k;
		d2 += k;
		l += 2;
		while (l -= 4) {
			if (*(int *)d1 != *(int *)d2)
				return (1);
			d1 -= 4;
			d2 -= 4;
		}
	} else {
		while (l--) {
			if (*d1++ != *d2++)
				return (1);
		}
	}
	return (0);
}

/*
 * Initialization routine for export routines. Should only be called once.
 */
int
nfs_exportinit(void)
{
	int error;
	int exporthash;

	rw_init(&exported_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * Allocate the place holder for the public file handle, which
	 * is all zeroes. It is initially set to the root filesystem.
	 */
	exi_root = kmem_zalloc(sizeof (*exi_root), KM_SLEEP);
	exi_public = exi_root;

	exi_root->exi_export.ex_flags = EX_PUBLIC;
	exi_root->exi_export.ex_pathlen = 2;	/* length of "/" */
	exi_root->exi_export.ex_path =
		kmem_alloc(exi_root->exi_export.ex_pathlen, KM_SLEEP);
	exi_root->exi_export.ex_path[0] = '/';
	exi_root->exi_export.ex_path[1] = '\0';

	exi_root->exi_vp = rootdir;
	exi_rootfid.fid_len = MAXFIDSZ;
	error = VOP_FID(exi_root->exi_vp, &exi_rootfid);
	if (error) {
		kmem_free(exi_root, sizeof (*exi_root));
		return (error);
	}

	/*
	 * Hash for entry (kmem_zalloc() init's fsid and fid to zero)
	 * and place it in table.
	 */
	exporthash = exptablehash(&exi_root->exi_fsid, &exi_root->exi_fid);
	exi_root->exi_hash = exptable[exporthash];
	exptable[exporthash] = exi_root;
	nfslog_init();

	return (0);
}

/*
 * Finalization routine for export routines. Called to cleanup previoulsy
 * initializtion work when the NFS server module could not be loaded correctly.
 */
void
nfs_exportfini(void)
{
	/*
	 * Deallocate the place holder for the public file handle.
	 */
	kmem_free(exi_root, sizeof (*exi_root));

	rw_destroy(&exported_lock);
}

/*
 *  Check if 2 gss mechanism identifiers are the same.
 *
 *  return FALSE if not the same.
 *  return TRUE if the same.
 */
static bool_t
nfs_mech_equal(rpc_gss_OID mech1, rpc_gss_OID mech2)
{
	if ((mech1->length == 0) && (mech2->length == 0))
		return (TRUE);

	if (mech1->length != mech2->length)
		return (FALSE);

	return (bcmp(mech1->elements, mech2->elements, mech1->length) == 0);
}

/*
 *  This routine is used by rpc to map rpc security number
 *  to nfs specific security flavor number.
 *
 *  The gss callback prototype is
 *  callback(struct svc_req *, gss_cred_id_t *, gss_ctx_id_t *,
 *				rpc_gss_lock_t *, void **),
 *  since nfs does not use the gss_cred_id_t/gss_ctx_id_t arguments
 *  we cast them to void.
 */
/*ARGSUSED*/
bool_t
rfs_gsscallback(struct svc_req *req, gss_cred_id_t deleg, void *gss_context,
    rpc_gss_lock_t *lock, void **cookie)
{
	int i, j;
	rpc_gss_rawcred_t *raw_cred;
	struct exportinfo *exi;

	/*
	 * We don't deal with delegated credentials.
	 */
	if (deleg != GSS_C_NO_CREDENTIAL)
		return (FALSE);

	raw_cred = lock->raw_cred;
	*cookie = NULL;

	rw_enter(&exported_lock, RW_READER);
	for (i = 0; i < EXPTABLESIZE; i++) {
	    exi = exptable[i];
	    while (exi) {
		if (exi->exi_export.ex_seccnt > 0) {
		    struct secinfo *secp;

		    secp = exi->exi_export.ex_secinfo;
		    for (j = 0; j < exi->exi_export.ex_seccnt; j++) {
			/*
			 *  If there is a map of the triplet
			 *  (mechanism, service, qop) between raw_cred and
			 *  the exported flavor, get the psudo flavor number.
			 *  Also qop should not be NULL, it should be "default"
			 *  or something else.
			 */
			if ((secp[j].s_secinfo.sc_rpcnum == RPCSEC_GSS) &&
			(nfs_mech_equal(secp[j].s_secinfo.sc_gss_mech_type,
			raw_cred->mechanism)) &&
			(secp[j].s_secinfo.sc_service == raw_cred->service) &&
			(raw_cred->qop == secp[j].s_secinfo.sc_qop)) {
				*cookie = (void *)secp[j].s_secinfo.sc_nfsnum;
				goto done;
			}
		    }
		}
		exi = exi->exi_hash;
	    }
	}
done:
	rw_exit(&exported_lock);

	if (*cookie) {
		lock->locked = TRUE;

		return (TRUE);
	}

	return (FALSE);
}


/*
 * Exportfs system call
 */
int
exportfs(struct exportfs_args *args, model_t model, cred_t *cr)
{
	vnode_t *vp;
	struct exportdata *kex;
	struct exportinfo *exi;
	struct exportinfo *ex, *prev;
	fid_t fid;
	fsid_t fsid;
	int error;
	int exporthash;
	size_t allocsize;
	struct secinfo *sp;
	rpc_gss_callback_t cb;
	char *pathbuf;
	char *log_buffer;
	char *tagbuf;
	int i, callback;
	uint_t vers;
	STRUCT_HANDLE(exportfs_args, uap);
	STRUCT_DECL(exportdata, uexi);

	if (!suser(cr))
		return (EPERM);

	STRUCT_SET_HANDLE(uap, model, args);

	error = lookupname(STRUCT_FGETP(uap, dname), UIO_USERSPACE,
	    FOLLOW, NULL, &vp);
	if (error)
		return (error);

	/*
	 * 'vp' may be an AUTOFS node, so we perform a
	 * VOP_ACCESS() to trigger the mount of the
	 * intended filesystem, so we can share the intended
	 * filesystem instead of the AUTOFS filesystem.
	 */
	(void) VOP_ACCESS(vp, 0, 0, cr);

	/*
	 * We're interested in the top most filesystem.
	 * This is specially important when uap->dname is a trigger
	 * AUTOFS node, since we're really interested in sharing the
	 * filesystem AUTOFS mounted as result of the VOP_ACCESS()
	 * call not the AUTOFS node itself.
	 */
	if (vp->v_vfsmountedhere != NULL) {
		if (error = traverse(&vp)) {
			VN_RELE(vp);
			return (error);
		}
	}

	/*
	 * Get the vfs id
	 */
	bzero(&fid, sizeof (fid));
	fid.fid_len = MAXFIDSZ;
	error = VOP_FID(vp, &fid);
	fsid = vp->v_vfsp->vfs_fsid;
	if (error) {
		VN_RELE(vp);
		/*
		 * If VOP_FID returns ENOSPC then the fid supplied
		 * is too small.  For now we simply return EREMOTE.
		 */
		if (error == ENOSPC)
			error = EREMOTE;
		return (error);
	}

	if (STRUCT_FGETP(uap, uex) == NULL) {
		VN_RELE(vp);
		error = unexport(&fsid, &fid);
		return (error);
	}
	exi = kmem_zalloc(sizeof (*exi), KM_SLEEP);
	exi->exi_fsid = fsid;
	exi->exi_fid = fid;
	exi->exi_vp = vp;

	/*
	 * Initialize auth cache lock
	 */
	rw_init(&exi->exi_cache_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * Build up the template fhandle
	 */
	exi->exi_fh.fh_fsid = fsid;
	if (exi->exi_fid.fid_len > sizeof (exi->exi_fh.fh_xdata)) {
		error = EREMOTE;
		goto error_return;
	}
	exi->exi_fh.fh_xlen = exi->exi_fid.fid_len;
	bcopy(exi->exi_fid.fid_data, exi->exi_fh.fh_xdata,
	    exi->exi_fid.fid_len);

	exi->exi_fh.fh_len = sizeof (exi->exi_fh.fh_data);

	kex = &exi->exi_export;

	/*
	 * Load in everything, and do sanity checking
	 */
	STRUCT_INIT(uexi, model);
	if (copyin(STRUCT_FGETP(uap, uex), STRUCT_BUF(uexi),
	    STRUCT_SIZE(uexi))) {
		error = EFAULT;
		goto error_return;
	}

	kex->ex_version = STRUCT_FGET(uexi, ex_version);
	if (kex->ex_version != EX_CURRENT_VERSION) {
		error = EINVAL;
		cmn_err(CE_WARN,
		"NFS: exportfs requires export struct version 2 - got %d\n",
		kex->ex_version);
		goto error_return;
	}

	/*
	 * Must have at least one security entry
	 */
	kex->ex_seccnt = STRUCT_FGET(uexi, ex_seccnt);
	if (kex->ex_seccnt < 1) {
		error = EINVAL;
		goto error_return;
	}

	kex->ex_path = STRUCT_FGETP(uexi, ex_path);
	kex->ex_pathlen = STRUCT_FGET(uexi, ex_pathlen);
	kex->ex_flags = STRUCT_FGET(uexi, ex_flags);
	kex->ex_anon = STRUCT_FGET(uexi, ex_anon);
	kex->ex_secinfo = STRUCT_FGETP(uexi, ex_secinfo);
	kex->ex_index = STRUCT_FGETP(uexi, ex_index);
	kex->ex_log_buffer = STRUCT_FGETP(uexi, ex_log_buffer);
	kex->ex_log_bufferlen = STRUCT_FGET(uexi, ex_log_bufferlen);
	kex->ex_tag = STRUCT_FGETP(uexi, ex_tag);
	kex->ex_taglen = STRUCT_FGET(uexi, ex_taglen);

	/*
	 * Copy the exported pathname into
	 * an appropriately sized buffer.
	 */
	pathbuf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (copyinstr(kex->ex_path, pathbuf, MAXPATHLEN, &kex->ex_pathlen)) {
		kmem_free(pathbuf, MAXPATHLEN);
		error = EFAULT;
		goto error_return;
	}
	kex->ex_path = kmem_alloc(kex->ex_pathlen + 1, KM_SLEEP);
	bcopy(pathbuf, kex->ex_path, kex->ex_pathlen);
	kex->ex_path[kex->ex_pathlen] = '\0';
	kmem_free(pathbuf, MAXPATHLEN);

	/*
	 * Get the path to the logging buffer and the tag
	 */
	if (kex->ex_flags & EX_LOG) {
		log_buffer = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		if (copyinstr(kex->ex_log_buffer, log_buffer, MAXPATHLEN,
		    &kex->ex_log_bufferlen)) {
			kmem_free(log_buffer, MAXPATHLEN);
			error = EFAULT;
			goto error_return;
		}
		kex->ex_log_buffer =
			kmem_alloc(kex->ex_log_bufferlen + 1, KM_SLEEP);
		bcopy(log_buffer, kex->ex_log_buffer, kex->ex_log_bufferlen);
		kex->ex_log_buffer[kex->ex_log_bufferlen] = '\0';
		kmem_free(log_buffer, MAXPATHLEN);

		tagbuf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		if (copyinstr(kex->ex_tag, tagbuf, MAXPATHLEN,
		    &kex->ex_taglen)) {
			kmem_free(tagbuf, MAXPATHLEN);
			error = EFAULT;
			goto error_return;
		}
		kex->ex_tag = kmem_alloc(kex->ex_taglen + 1, KM_SLEEP);
		bcopy(tagbuf, kex->ex_tag, kex->ex_taglen);
		kex->ex_tag[kex->ex_taglen] = '\0';
		kmem_free(tagbuf, MAXPATHLEN);
	}

	/*
	 * Load the security information for each flavor
	 */
	allocsize = kex->ex_seccnt * SIZEOF_STRUCT(secinfo, model);
	sp = kmem_zalloc(allocsize, KM_SLEEP);
	if (copyin(kex->ex_secinfo, sp, allocsize)) {
		kmem_free(sp, allocsize);
		error = EFAULT;
		goto error_return;
	}

	/*
	 * All of these nested structures need to be converted to
	 * the kernel native format.
	 */
	if (model != DATAMODEL_NATIVE) {
		int i;
		size_t allocsize2;
		struct secinfo *sp2;

		allocsize2 = kex->ex_seccnt * sizeof (struct secinfo);
		sp2 = kmem_zalloc(allocsize2, KM_SLEEP);

		for (i = 0; i < kex->ex_seccnt; i++) {
			STRUCT_HANDLE(secinfo, usi);

			STRUCT_SET_HANDLE(usi, model,
			    (struct secinfo *)((caddr_t)sp +
			    (i * SIZEOF_STRUCT(secinfo, model))));
			bcopy(STRUCT_FGET(usi, s_secinfo.sc_name),
			    sp2[i].s_secinfo.sc_name, MAX_NAME_LEN);
			sp2[i].s_secinfo.sc_nfsnum =
			    STRUCT_FGET(usi, s_secinfo.sc_nfsnum);
			sp2[i].s_secinfo.sc_rpcnum =
			    STRUCT_FGET(usi, s_secinfo.sc_rpcnum);
			bcopy(STRUCT_FGET(usi, s_secinfo.sc_gss_mech),
			    sp2[i].s_secinfo.sc_gss_mech, MAX_NAME_LEN);
			sp2[i].s_secinfo.sc_gss_mech_type =
			    STRUCT_FGETP(usi, s_secinfo.sc_gss_mech_type);
			sp2[i].s_secinfo.sc_qop =
			    STRUCT_FGET(usi, s_secinfo.sc_qop);
			sp2[i].s_secinfo.sc_service =
			    STRUCT_FGET(usi, s_secinfo.sc_service);
			sp2[i].s_flags = STRUCT_FGET(usi, s_flags);
			sp2[i].s_window = STRUCT_FGET(usi, s_window);
			sp2[i].s_rootcnt = STRUCT_FGET(usi, s_rootcnt);
			sp2[i].s_rootnames = STRUCT_FGETP(usi, s_rootnames);
		}
		kmem_free(sp, allocsize);
		sp = sp2;
		allocsize = allocsize2;
	}

	/*
	 * And now copy rootnames for each individual secinfo.
	 */
	callback = 0;
	for (i = 0; i < kex->ex_seccnt; i++) {
		bool_t set_svc_flag;
		struct secinfo *exs;

		exs = &sp[i];
		if (exs->s_rootcnt > 0) {
			if (!sec_svc_loadrootnames(exs->s_secinfo.sc_rpcnum,
			    exs->s_rootcnt, &exs->s_rootnames, model)) {
				error = EFAULT;
				kmem_free(sp, allocsize);
				goto error_return;
			}
		}

		if (exs->s_secinfo.sc_rpcnum == RPCSEC_GSS) {
			char svcname[MAX_GSS_NAME];
			rpc_gss_OID mech_tmp;
			STRUCT_DECL(rpc_gss_OID_s, umech_tmp);
			caddr_t elements_tmp;

			/* Copyin mechanism type */
			STRUCT_INIT(umech_tmp, model);
			mech_tmp = kmem_alloc(sizeof (*mech_tmp), KM_SLEEP);
			if (copyin(exs->s_secinfo.sc_gss_mech_type,
			    STRUCT_BUF(umech_tmp), STRUCT_SIZE(umech_tmp))) {
				kmem_free(mech_tmp, sizeof (*mech_tmp));
				kmem_free(sp, allocsize);
				error = EFAULT;
				goto error_return;
			}
			mech_tmp->length = STRUCT_FGET(umech_tmp, length);
			mech_tmp->elements = STRUCT_FGETP(umech_tmp, elements);

			elements_tmp = kmem_alloc(mech_tmp->length, KM_SLEEP);
			if (copyin(mech_tmp->elements, elements_tmp,
			    mech_tmp->length)) {
				kmem_free(elements_tmp, mech_tmp->length);
				kmem_free(mech_tmp, sizeof (*mech_tmp));
				kmem_free(sp, allocsize);
				error = EFAULT;
				goto error_return;
			}
			mech_tmp->elements = elements_tmp;
			exs->s_secinfo.sc_gss_mech_type = mech_tmp;

			/* Set service information */
			set_svc_flag = FALSE;
			(void) sprintf(svcname, "nfs@%s", utsname.nodename);
			for (vers = NFS_ACL_VERSMIN; vers <= NFS_ACL_VERSMAX;
			    vers++) {
				if (rpc_gss_set_svc_name(svcname,
				    exs->s_secinfo.sc_gss_mech_type, 0,
				    NFS_ACL_PROGRAM, vers)) {
					set_svc_flag = TRUE;
				}
			}
			for (vers = NFS_VERSMIN; vers <= NFS_VERSMAX; vers++) {
				if (rpc_gss_set_svc_name(svcname,
				    exs->s_secinfo.sc_gss_mech_type, 0,
				    NFS_PROGRAM, vers)) {
					set_svc_flag = TRUE;
				}
			}
			if (!set_svc_flag) {
				cmn_err(CE_NOTE,
				    "exportfs: set gss service %s failed",
				    svcname);
				error = EINVAL;
				goto error_return;
			}

			callback = 1;
		}
	}
	kex->ex_secinfo = sp;

	/*
	 *  Set up rpcsec_gss callback routine entry if any.
	 */
	if (callback) {
		cb.callback = rfs_gsscallback;
		cb.program = NFS_ACL_PROGRAM;
		for (cb.version = NFS_ACL_VERSMIN;
		    cb.version <= NFS_ACL_VERSMAX; cb.version++) {
			(void) sec_svc_control(RPC_SVC_SET_GSS_CALLBACK,
			    (void *)&cb);
		}

		cb.program = NFS_PROGRAM;
		for (cb.version = NFS_VERSMIN; cb.version <= NFS_VERSMAX;
		    cb.version++) {
			(void) sec_svc_control(RPC_SVC_SET_GSS_CALLBACK,
			    (void *)&cb);
		}
	}

	/*
	 * Check the index flag. Do this here to avoid holding the
	 * lock while dealing with the index option (as we do with
	 * the public option).
	 */
	if (kex->ex_flags & EX_INDEX) {
		if (!kex->ex_index) {	/* sanity check */
			error = EINVAL;
			goto error_return;
		}
		if (error = loadindex(kex))
			goto error_return;
	}

	if (kex->ex_flags & EX_LOG) {
		if (error = nfslog_setup(exi))
			goto error_return;
	}

	/*
	 * Insert the new entry at the front of the export list
	 */
	rw_enter(&exported_lock, RW_WRITER);
	exporthash = exptablehash(&exi->exi_fsid, &exi->exi_fid);
	exi->exi_hash = exptable[exporthash];
	exptable[exporthash] = exi;

	/*
	 * Check the rest of the list for an old entry for the fs.
	 * If one is found then unlink it, wait until this is the
	 * only reference and then free it.
	 */
	prev = exi;
	for (ex = prev->exi_hash; ex != NULL; prev = ex, ex = ex->exi_hash) {
		if (ex->exi_vp == vp && ex != exi_root) {
			prev->exi_hash = ex->exi_hash;
			break;
		}
	}

	/*
	 * If the public filehandle is pointing at the
	 * old entry, then point it back at the root.
	 */
	if (ex != NULL && ex == exi_public)
		exi_public = exi_root;

	/*
	 * If the public flag is on, make the global exi_public
	 * point to this entry and turn off the public bit so that
	 * we can distinguish it from the place holder export.
	 */
	if (kex->ex_flags & EX_PUBLIC) {
		exi_public = exi;
		kex->ex_flags &= ~EX_PUBLIC;
	}

	rw_exit(&exported_lock);

	if (exi_public == exi || kex->ex_flags & EX_LOG) {
		/*
		 * Log share operation to this buffer only.
		 */
		nfslog_share_record(exi, cr);
	}

	if (ex != NULL)
		exportfree(ex);
	return (0);

error_return:
	VN_RELE(vp);
	rw_destroy(&exi->exi_cache_lock);
	kmem_free(exi, sizeof (*exi));
	return (error);
}


/*
 * Remove the exported directory from the export list
 */
static int
unexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo **tail;
	struct exportinfo *exi;

	rw_enter(&exported_lock, RW_WRITER);
	tail = &exptable[exptablehash(fsid, fid)];
	while (*tail != NULL) {
		if (exportmatch(*tail, fsid, fid)) {
			exi = *tail;
			*tail = (*tail)->exi_hash;

			/*
			 * If this was a public export, restore
			 * the public filehandle to the root.
			 */
			if (exi == exi_public) {
				exi_public = exi_root;
				rw_exit(&exported_lock);

				nfslog_share_record(exi_public, CRED());
			} else
				rw_exit(&exported_lock);

			if (exi->exi_export.ex_flags & EX_LOG) {
				nfslog_unshare_record(exi, CRED());
			}

			exportfree(exi);

			return (0);
		}
		tail = &(*tail)->exi_hash;
	}
	rw_exit(&exported_lock);
	return (EINVAL);
}

/*
 * Get file handle system call.
 * Takes file name and returns a file handle for it.
 */
int
nfs_getfh(struct nfs_getfh_args *args, model_t model, cred_t *cr)
{
	fhandle_t fh;
	vnode_t *vp;
	vnode_t *dvp;
	struct exportinfo *exi;
	int error;
	STRUCT_HANDLE(nfs_getfh_args, uap);

#ifdef lint
	model = model;		/* STRUCT macros don't always use it */
#endif

	if (!suser(cr))
		return (EPERM);

	STRUCT_SET_HANDLE(uap, model, args);

	error = lookupname(STRUCT_FGETP(uap, fname), UIO_USERSPACE,
	    FOLLOW, &dvp, &vp);
	if (error == EINVAL) {
		/*
		 * if fname resolves to / we get EINVAL error
		 * since we wanted the parent vnode. Try again
		 * with NULL dvp.
		 */
		error = lookupname(STRUCT_FGETP(uap, fname), UIO_USERSPACE,
		    FOLLOW, NULL, &vp);
		dvp = NULL;
	}
	if (!error && vp == NULL) {
		/*
		 * Last component of fname not found
		 */
		if (dvp != NULL) {
			VN_RELE(dvp);
		}
		error = ENOENT;
	}
	if (error)
		return (error);

	/*
	 * 'vp' may be an AUTOFS node, so we perform a
	 * VOP_ACCESS() to trigger the mount of the
	 * intended filesystem, so we can share the intended
	 * filesystem instead of the AUTOFS filesystem.
	 */
	(void) VOP_ACCESS(vp, 0, 0, cr);

	/*
	 * We're interested in the top most filesystem.
	 * This is specially important when uap->dname is a trigger
	 * AUTOFS node, since we're really interested in sharing the
	 * filesystem AUTOFS mounted as result of the VOP_ACCESS()
	 * call not the AUTOFS node itself.
	 */
	if (vp->v_vfsmountedhere != NULL) {
		if (error = traverse(&vp)) {
			VN_RELE(vp);
			return (error);
		}
	}

	error = findexivp(&exi, dvp, vp, cr);
	if (!error) {
		error = makefh(&fh, vp, exi);
		if (!error && exi->exi_export.ex_flags & EX_LOG) {
			nfslog_getfh(exi, &fh, STRUCT_FGETP(uap, fname),
				UIO_USERSPACE, cr);
		}
		rw_exit(&exported_lock);
		if (!error) {
			if (copyout(&fh, STRUCT_FGETP(uap, fhp), sizeof (fh)))
				error = EFAULT;
		}
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}

/*
 * Strategy: if vp is in the export list, then
 * return the associated file handle. Otherwise, ".."
 * once up the vp and try again, until the root of the
 * filesystem is reached.
 */
static int
findexivp(struct exportinfo **exip, vnode_t *dvp, vnode_t *vp, cred_t *cr)
{
	fid_t fid;
	int error;

	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	for (;;) {
		bzero(&fid, sizeof (fid));
		fid.fid_len = MAXFIDSZ;
		error = VOP_FID(vp, &fid);
		if (error) {
			/*
			 * If VOP_FID returns ENOSPC then the fid supplied
			 * is too small.  For now we simply return EREMOTE.
			 */
			if (error == ENOSPC)
				error = EREMOTE;
			break;
		}
		*exip = findexport(&vp->v_vfsp->vfs_fsid, &fid);
		if (*exip != NULL) {
			/*
			 * Found the export info
			 */
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;
		}

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
		 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			error = VOP_LOOKUP(vp, "..", &dvp, NULL, 0, NULL, cr);
			if (error)
				break;
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}
/*
 * Return exportinfo struct for a given vnode.
 * Like findexivp, but it uses checkexport()
 * since it is called with the export lock held.
 */
struct   exportinfo *
nfs_vptoexi(vnode_t *dvp, vnode_t *vp, cred_t *cr, int *walk)
{
	fid_t fid;
	int error;
	struct exportinfo *exi;

	ASSERT(vp);
	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	*walk = 0;

	for (;;) {
		bzero(&fid, sizeof (fid));
		fid.fid_len = MAXFIDSZ;
		error = VOP_FID(vp, &fid);
		if (error)
			break;

		exi = checkexport(&vp->v_vfsp->vfs_fsid, &fid);
		if (exi != NULL) {
			/*
			 * Found the export info
			 */
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;
		}

		(*walk)++;

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
		 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			error = VOP_LOOKUP(vp, "..", &dvp, NULL, 0, NULL, cr);
			if (error)
				break;
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	if (error != 0)
		return (NULL);
	else
		return (exi);
}

bool_t
chk_clnt_sec(struct exportinfo *exi, struct svc_req *req)
{
	int i, nfsflavor;
	struct secinfo *sp;
	bool_t sec_found = FALSE;

	/*
	 *  Get the nfs flavor number from xprt.
	 */
	nfsflavor = (int)req->rq_xprt->xp_cookie;

	ASSERT(RW_READ_HELD(&exported_lock));

	sp = exi->exi_export.ex_secinfo;
	for (i = 0; i < exi->exi_export.ex_seccnt; i++) {
		if (nfsflavor == sp[i].s_secinfo.sc_nfsnum) {
			sec_found = TRUE;
			break;
		}
	}
	return (sec_found);
}

/*
 * Make an fhandle from a vnode
 */
int
makefh(fhandle_t *fh, vnode_t *vp, struct exportinfo *exi)
{
	int error;

	ASSERT(RW_READ_HELD(&exported_lock));

	*fh = exi->exi_fh;	/* struct copy */

	error = VOP_FID(vp, (fid_t *)&fh->fh_len);
	if (error) {
		/*
		 * Should be something other than EREMOTE
		 */
		return (EREMOTE);
	}
	return (0);
}

/*
 * This routine makes an overloaded V2 fhandle which contains
 * sec modes.
 *
 * Note that the first four octets contain the length octet,
 * the status octet, and two padded octets to make them XDR
 * four-octet aligned.
 *
 *   1   2   3   4                                          32
 * +---+---+---+---+---+---+---+---+   +---+---+---+---+   +---+
 * | l | s |   |   |     sec_1     |...|     sec_n     |...|   |
 * +---+---+---+---+---+---+---+---+   +---+---+---+---+   +---+
 *
 * where
 *
 *   the status octet s indicates whether there are more security
 *   flavors (1 means yes, 0 means no) that require the client to
 *   perform another 0x81 LOOKUP to get them,
 *
 *   the length octet l is the length describing the number of
 *   valid octets that follow.  (l = 4 * n, where n is the number
 *   of security flavors sent in the current overloaded filehandle.)
 */
int
makefh_ol(fhandle_t *fh, struct exportinfo *exi, uint_t sec_index)
{
	static int max_cnt = (NFS_FHSIZE/sizeof (int)) - 1;
	int totalcnt, i, *ipt, cnt;
	char *c;

	ASSERT(RW_READ_HELD(&exported_lock));

	if (fh == (fhandle_t *)NULL ||
		exi == (struct exportinfo *)NULL ||
		sec_index > exi->exi_export.ex_seccnt ||
		sec_index < 1)
		return (EREMOTE);

	totalcnt = exi->exi_export.ex_seccnt-sec_index+1;
	cnt = totalcnt > max_cnt? max_cnt : totalcnt;

	c = (char *)fh;
	/*
	 * Encode the length octet representing the number of
	 * security flavors (in bytes) in this overloaded fh.
	 */
	*c = cnt * sizeof (int);

	/*
	 * Encode the status octet that indicates whether there
	 * are more security flavors the client needs to get.
	 */
	*(c+1) = totalcnt > max_cnt;

	/*
	 * put security flavors in the overloaded fh
	 */
	ipt = (int *)(c + sizeof (int32_t));
	for (i = 0; i < cnt; i++) {
		*ipt++ = htonl(exi->exi_export.ex_secinfo[i+sec_index-1].
				s_secinfo.sc_nfsnum);
	}
	return (0);
}

/*
 * Make an nfs_fh3 from a vnode
 */
int
makefh3(nfs_fh3 *fh, vnode_t *vp, struct exportinfo *exi)
{
	int error;

	ASSERT(RW_READ_HELD(&exported_lock));

	fh->fh3_length = sizeof (fh->fh3_u.nfs_fh3_i);
	fh->fh3_u.nfs_fh3_i.fh3_i = exi->exi_fh;	/* struct copy */

	error = VOP_FID(vp, (fid_t *)&fh->fh3_len);

	if (error) {
		/*
		 * Should be something other than EREMOTE
		 */
		return (EREMOTE);
	}
	return (0);
}

/*
 * This routine makes an overloaded V3 fhandle which contains
 * sec modes.
 *
 *  1        4
 * +--+--+--+--+
 * |    len    |
 * +--+--+--+--+
 *                                               up to 64
 * +--+--+--+--+--+--+--+--+--+--+--+--+     +--+--+--+--+
 * |s |  |  |  |   sec_1   |   sec_2   | ... |   sec_n   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+     +--+--+--+--+
 *
 * len = 4 * (n+1), where n is the number of security flavors
 * sent in the current overloaded filehandle.
 *
 * the status octet s indicates whether there are more security
 * mechanisms (1 means yes, 0 means no) that require the client
 * to perform another 0x81 LOOKUP to get them.
 *
 * Three octets are padded after the status octet.
 */
int
makefh3_ol(nfs_fh3 *fh, struct exportinfo *exi, uint_t sec_index)
{
	static int max_cnt = NFS3_FHSIZE/sizeof (int) - 1;
	int totalcnt, cnt, *ipt, i;
	char *c;

	ASSERT(RW_READ_HELD(&exported_lock));

	if (fh == (nfs_fh3 *)NULL ||
		exi == (struct exportinfo *)NULL ||
		sec_index > exi->exi_export.ex_seccnt ||
		sec_index < 1) {
		return (EREMOTE);
	}

	totalcnt = exi->exi_export.ex_seccnt-sec_index+1;
	cnt = totalcnt > max_cnt? max_cnt : totalcnt;

	/*
	 * Place the length in fh3_length representing the number
	 * of security flavors (in bytes) in this overloaded fh.
	 */
	fh->fh3_length = (cnt+1) * sizeof (int32_t);

	c = (char *)&fh->fh3_u.nfs_fh3_i.fh3_i;
	/*
	 * Encode the status octet that indicates whether there
	 * are more security flavors the client needs to get.
	 */
	*c = totalcnt > max_cnt;

	/*
	 * put security flavors in the overloaded fh
	 */
	ipt = (int *)(c + sizeof (int32_t));
	for (i = 0; i < cnt; i++) {
		*(ipt+i) = htonl(
		exi->exi_export.ex_secinfo[i+sec_index-1].s_secinfo.sc_nfsnum);
	}
	return (0);
}

/*
 * Convert an fhandle into a vnode.
 * Uses the file id (fh_len + fh_data) in the fhandle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
vnode_t *
nfs_fhtovp(fhandle_t *fh, struct exportinfo *exi)
{
	vfs_t *vfsp;
	vnode_t *vp;
	int error;
	fid_t *fidp;

	TRACE_0(TR_FAC_NFS, TR_FHTOVP_START,
		"fhtovp_start");

	if (exi == NULL) {
		TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
			"fhtovp_end:(%S)", "exi NULL");
		return (NULL);	/* not exported */
	}

	ASSERT(exi->exi_vp != NULL);

	if (PUBLIC_FH2(fh)) {
		if (exi->exi_export.ex_flags & EX_PUBLIC) {
			TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
				"fhtovp_end:(%S)", "root not exported");
			return (NULL);
		}
		vp = exi->exi_vp;
		VN_HOLD(vp);
		return (vp);
	}

	vfsp = exi->exi_vp->v_vfsp;
	ASSERT(vfsp != NULL);
	fidp = (fid_t *)&fh->fh_len;

	error = VFS_VGET(vfsp, &vp, fidp);
	if (error || vp == NULL) {
		TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
			"fhtovp_end:(%S)", "VFS_GET failed or vp NULL");
		return (NULL);
	}
	TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
		"fhtovp_end:(%S)", "end");
	return (vp);
}

/*
 * Convert an fhandle into a vnode.
 * Uses the file id (fh_len + fh_data) in the fhandle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 * This is just like nfs_fhtovp() but without the exportinfo argument.
 * This routine is moved from klmops module to nfssrv so both nfs and
 * klm fhtovps can be maintained together.
 */

vnode_t *
lm_fhtovp(fhandle_t *fh)
{
	register vfs_t *vfsp;
	vnode_t *vp;
	int error;

	vfsp = getvfs(&fh->fh_fsid);
	if (vfsp == NULL)
		return (NULL);

	error = VFS_VGET(vfsp, &vp, (fid_t *)&(fh->fh_len));
	VFS_RELE(vfsp);
	if (error || vp == NULL)
		return (NULL);

	return (vp);
}

/*
 * Convert an nfs_fh3 into a vnode.
 * Uses the file id (fh_len + fh_data) in the file handle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
vnode_t *
nfs3_fhtovp(nfs_fh3 *fh, struct exportinfo *exi)
{
	vfs_t *vfsp;
	vnode_t *vp;
	int error;
	fid_t *fidp;

	if (exi == NULL)
		return (NULL);	/* not exported */

	ASSERT(exi->exi_vp != NULL);

	if (PUBLIC_FH3(fh)) {
		if (exi->exi_export.ex_flags & EX_PUBLIC)
			return (NULL);
		vp = exi->exi_vp;
		VN_HOLD(vp);
		return (vp);
	}

	if (fh->fh3_length != NFS3_CURFHSIZE)
		return (NULL);

	vfsp = exi->exi_vp->v_vfsp;
	ASSERT(vfsp != NULL);
	fidp = (fid_t *)&fh->fh3_len;

	error = VFS_VGET(vfsp, &vp, fidp);
	if (error || vp == NULL)
		return (NULL);

	return (vp);
}

/*
 * Convert an nfs_fh3 into a vnode.
 * Uses the file id (fh_len + fh_data) in the file handle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 * BTW: This is just like nfs3_fhtovp() but without the exportinfo arg.
 * Also, vfsp is accessed through getvfs() rather using exportinfo !!
 * This routine is moved from klmops module to nfssrv so both nfs and
 * klm fhtovps can be maintained together.
 */

vnode_t *
lm_nfs3_fhtovp(nfs_fh3 *fh)
{
	vfs_t *vfsp;
	vnode_t *vp;
	int error;

	if (fh->fh3_length != NFS3_CURFHSIZE)
		return (NULL);

	vfsp = getvfs(&fh->fh3_fsid);
	if (vfsp == NULL)
		return (NULL);

	error = VFS_VGET(vfsp, &vp, (fid_t *)&(fh->fh3_len));
	VFS_RELE(vfsp);
	if (error || vp == NULL)
		return (NULL);

	return (vp);
}

/*
 * Find the export structure associated with the given filesystem
 * If found, then the read lock on the exports list is left to
 * indicate that an entry is still busy.
 */
/*
 * findexport() is split into findexport() and checkexport() to fix
 * 1177604 so that checkexport() can be called by procedures that had
 * already obtained exported_lock to check the exptable.
 */
struct exportinfo *
checkexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo *exi;

	ASSERT(RW_READ_HELD(&exported_lock));
	for (exi = exptable[exptablehash(fsid, fid)];
	    exi != NULL;
	    exi = exi->exi_hash) {
		if (exportmatch(exi, fsid, fid)) {
			/*
			 * If this is the place holder for the
			 * public file handle, then return the
			 * real export entry for the public file
			 * handle.
			 */
			if (exi->exi_export.ex_flags & EX_PUBLIC) {
				exi = exi_public;
				/*
				 * If this is still the place holder
				 * for the public file handle, then
				 * check to see if the root file system
				 * is exported.  If so, then return that
				 * entry instead of the place holder
				 * entry.  If it isn't, then return the
				 * place holder anyway.
				 */
				if (exi->exi_export.ex_flags & EX_PUBLIC) {
					exi = checkexport(
					    &exi->exi_vp->v_vfsp->vfs_fsid,
					    &exi_rootfid);
					/*
					 * If the root isn't exported, then
					 * return the place holder.  This
					 * will cause fhtovp attempts to
					 * fail.
					 */
					if (exi == NULL)
						exi = exi_public;
				}
			}
			return (exi);
		}
	}
	return (NULL);
}

struct exportinfo *
findexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo *exi;

	rw_enter(&exported_lock, RW_READER);
	if ((exi = checkexport(fsid, fid)) != NULL)
		return (exi);
	rw_exit(&exported_lock);
	return (NULL);
}

/*
 * Free an entire export list node
 */
static void
exportfree(struct exportinfo *exi)
{
	int i;
	struct exportdata *ex;

	ex = &exi->exi_export;

	ASSERT(exi->exi_vp != NULL && !(exi->exi_export.ex_flags & EX_PUBLIC));
	VN_RELE(exi->exi_vp);

	if (ex->ex_flags & EX_INDEX)
		kmem_free(ex->ex_index, strlen(ex->ex_index) + 1);

	kmem_free(ex->ex_path, ex->ex_pathlen + 1);
	nfsauth_cache_free(exi);

	if (exi->exi_logbuffer)
		nfslog_disable(exi);

	if (ex->ex_flags & EX_LOG) {
		kmem_free(ex->ex_log_buffer, ex->ex_log_bufferlen + 1);
		kmem_free(ex->ex_tag, ex->ex_taglen + 1);
	}

	for (i = 0; i < ex->ex_seccnt; i++) {
		struct secinfo *secp;

		secp = &ex->ex_secinfo[i];
		if (secp->s_rootcnt > 0) {
			if (secp->s_rootnames != NULL) {
				sec_svc_freerootnames(secp->s_secinfo.sc_rpcnum,
				    secp->s_rootcnt, secp->s_rootnames);
			}
		}

		if ((secp->s_secinfo.sc_rpcnum == RPCSEC_GSS) &&
		    (secp->s_secinfo.sc_gss_mech_type)) {
			kmem_free(secp->s_secinfo.sc_gss_mech_type->elements,
			    secp->s_secinfo.sc_gss_mech_type->length);
			kmem_free(secp->s_secinfo.sc_gss_mech_type,
			    sizeof (rpc_gss_OID_desc));
		}
	}
	if (ex->ex_secinfo) {
		kmem_free(ex->ex_secinfo,
		    ex->ex_seccnt * sizeof (struct secinfo));
	}

	rw_destroy(&exi->exi_cache_lock);

	kmem_free(exi, sizeof (*exi));
}

/*
 * Free the export lock
 */
void
export_rw_exit(void)
{
	rw_exit(&exported_lock);
}

/*
 * load the index file from user space into kernel space.
 */
static int
loadindex(struct exportdata *kex)
{
	int error;
	char index[MAXNAMELEN+1];
	size_t len;

	/*
	 * copyinstr copies the complete string including the NULL and
	 * returns the len with the NULL byte included in the calculation
	 * as long as the max length is not exceeded.
	 */
	if (error = copyinstr(kex->ex_index, index, sizeof (index), &len))
		return (error);

	kex->ex_index = kmem_alloc(len, KM_SLEEP);
	bcopy(index, kex->ex_index, len);

	return (0);
}
