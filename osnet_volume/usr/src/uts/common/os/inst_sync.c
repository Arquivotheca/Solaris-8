/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)inst_sync.c	1.18	98/08/28 SMI"

/*
 * Syscall to write out the instance number data structures to
 * stable storage.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/t_lock.h>
#include <sys/modctl.h>
#include <sys/systm.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/dc_ki.h>
#include <sys/cladm.h>
#include <sys/sunddi.h>
#include <sys/dditypes.h>
#include <sys/instance.h>
#include <sys/instance.h>
#include <sys/debug.h>

/*
 * Userland sees:
 *
 *	int inst_sync(pathname, flags);
 *
 * Returns zero if instance number information was successfully
 * written to 'pathname', -1 plus error code in errno otherwise.
 *
 * POC notes:
 *
 * -	This could be done as a case of the modctl(2) system call
 *	though the ability to have it load and unload would disappear.
 *
 * -	Currently, flags are not interpreted.
 *
 * -	Maybe we should pass through two filenames - one to create,
 *	and the other as the 'final' target i.e. do the rename of
 *	/etc/instance.new -> /etc/instance in the kernel.
 */

static int in_sync_sys(char *pathname, u_int flags);

static struct sysent in_sync_sysent = {
	2,			/* number of arguments */
	SE_ARGC | SE_32RVAL1,	/* c-style calling, 32-bit return value */
	in_sync_sys,		/* the handler */
	(krwlock_t *)0		/* rw lock allocated/used by framework */
};

static struct modlsys modlsys = {
	&mod_syscallops, "instance binding syscall", &in_sync_sysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32, "32-bit instance binding syscall", &in_sync_sysent
};
#endif

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

static int in_write_instance(struct vnode *vp);

/*ARGSUSED1*/
static int
in_sync_sys(char *pathname, u_int flags)
{
	struct vnode *vp;
	int error;

	/*
	 * We must be root to do this, since we lock critical data
	 * structures whilst we're doing it ..
	 */
	if (!suser(CRED())) {
		return (set_errno(EPERM));
		/* NOTREACHED */
	}

	/*
	 * Only one process is allowed to get the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);

	/*
	 * Clustering: sync the instances at the root node, this should
	 * cause the global instance allocation to be committed. This
	 * action must take place before we go on with committing
	 * our state. In the install case, this also allows for the dcops
	 * vector to be completely initialized.
	 */

	DC_SYNC_INSTANCES(&dcops);

	/*
	 * Create an instance file for writing, giving it a mode that
	 * will only permit reading.  Note that we refuse to overwrite
	 * an existing file.
	 */
	if ((error = vn_open(pathname, UIO_USERSPACE,
	    FCREAT, 0444, &vp, CRCREAT, 0)) != 0) {
		if (error == EISDIR)
			error = EACCES;	/* SVID compliance? */
		goto end;
		/*NOTREACHED*/
	}

	/*
	 * So far so good.  We're singly threaded, the vnode is beckoning
	 * so let's get on with it.  Any error, and we just give up and
	 * hand the first error we get back to userland.
	 */
	error = in_write_instance(vp);

	/*
	 * If there was any sort of error, we deliberately go and
	 * remove the file we just created so that any attempts to
	 * use it will quickly fail.
	 */
	if (error)
		(void) vn_remove(pathname, UIO_USERSPACE, RMFILE);
end:
	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);

	/*
	 * Clustering: Now that we know the dcops vector is fully installed,
	 * call DC_SYNC_INSTANCES() again, so that minor re-allocations will
	 * happen appropriately.
	 */

	DC_SYNC_INSTANCES(&dcops);

	return (error ? set_errno(error) : 0);
}

/*
 * At the risk of reinventing stdio ..
 */
#define	FBUFSIZE	512

typedef struct _File {
	char	*ptr;
	int	count;
	char	buf[FBUFSIZE];
	vnode_t	*vp;
	offset_t voffset;
} File;

static int
in_write(struct vnode *vp, offset_t *vo, caddr_t buf, int count)
{
	int error;
	ssize_t resid;
	rlim64_t rlimit = *vo + count + 1;

	error = vn_rdwr(UIO_WRITE, vp, buf, count, *vo,
	    UIO_SYSSPACE, 0, rlimit, CRED(), &resid);

	*vo += (offset_t)(count - resid);

	return (error);
}

static File *
in_fvpopen(struct vnode *vp)
{
	File *fp;

	fp = kmem_zalloc(sizeof (File), KM_SLEEP);
	fp->vp = vp;
	fp->ptr = fp->buf;

	return (fp);
}

static int
in_fclose(File *fp)
{
	int error;

	error = VOP_CLOSE(fp->vp, FCREAT, 1, (offset_t)0, CRED());
	VN_RELE(fp->vp);
	kmem_free(fp, sizeof (File));
	return (error);
}

static int
in_fflush(File *fp)
{
	int error = 0;

	if (fp->count)
		error = in_write(fp->vp, &fp->voffset, fp->buf, fp->count);
	if (error == 0)
		error = VOP_FSYNC(fp->vp, FSYNC,  CRED());
	return (error);
}

static int
in_fputs(File *fp, char *buf)
{
	int error = 0;

	while (*buf) {
		*fp->ptr++ = *buf++;
		if (++fp->count == FBUFSIZE) {
			error = in_write(fp->vp, &fp->voffset, fp->buf,
			    fp->count);
			if (error)
				break;
			fp->count = 0;
			fp->ptr = fp->buf;
		}
	}

	return (error);
}

/*
 * External linkage
 */
static File *in_fp;

/*
 * XXX what is the maximum length of the name of a driver?  Must be maximum
 * XXX file name length (find the correct constant and substitute for this one
 */
#define	DRVNAMELEN (1 + 256)
static char linebuffer[MAXPATHLEN + 1 + 1 + 1 + 1 + 10 + 1 + DRVNAMELEN];

/*
 * XXX	Maybe we should just write 'in_fprintf' instead ..
 */
static int
in_walktree(in_node_t *np, char *this)
{
	char *next;
	int error = 0;
	in_drv_t *dp;

	for (error = 0; np; np = np->in_sibling) {

		if (np->in_unit_addr[0] == '\0')
			(void) sprintf(this, "/%s", np->in_node_name);
		else
			(void) sprintf(this, "/%s@%s", np->in_node_name,
			    np->in_unit_addr);
		next = this + strlen(this);

		ASSERT(np->in_drivers);

		for (dp = np->in_drivers; dp; dp = dp->ind_next_drv) {
			u_int inst_val;

			inst_val = dp->ind_instance;
			/*
			 * Solaris Clustering changes for global instance
			 * allocation.
			 * There is a possiblity for the instance numbers to
			 * get out of sync, if the user changes the instance
			 * numbers by hand, or for the instance numbers to get
			 * allocated when dcs is not operational
			 */
			if (cluster_bootflags & CLUSTER_DCS_ENABLED) {
				if (ddi_name_to_major(dp->ind_driver_name)
				    != (major_t)-1) {
					DC_GET_INSTANCE(&dcops,
					    ddi_name_to_major(
					    dp->ind_driver_name), linebuffer+1,
					    &inst_val);
					if (inst_val != dp->ind_instance) {
						cmn_err(CE_WARN, "Instance for "
						    "device: %s "
						    "changed from %d to %d",
						    linebuffer+1,
						    dp->ind_instance, inst_val);
					}
				}
			}
			(void) sprintf(next, "\" %d \"%s\"\n", inst_val,
			    dp->ind_driver_name);
			if (error = in_fputs(in_fp, linebuffer))
				return (error);
		}

		if (np->in_child)
			if (error = in_walktree(np->in_child, next))
				break;
	}
	return (error);
}


/*
 * Walk the instance tree, writing out what we find.
 *
 * There's some fairly nasty sharing of buffers in this
 * bit of code, so be careful out there when you're
 * rewriting it ..
 */
static int
in_write_instance(struct vnode *vp)
{
	int error;
	char *cp;

	in_fp = in_fvpopen(vp);

	/*
	 * Place a bossy comment at the beginning of the file.
	 */
	error = in_fputs(in_fp,
	    "#\n#\tCaution! This file contains critical kernel state\n#\n");

	if (error == 0) {
		ASSERT(e_ddi_inst_state.ins_busy);

		cp = linebuffer;
		*cp++ = '\"';
		(void) strcpy(cp, i_ddi_get_dpath_prefix());
		cp += strlen(cp);
		error = in_walktree(e_ddi_inst_state.ins_root->in_child, cp);
	}

	if (error == 0) {
		if ((error = in_fflush(in_fp)) == 0)
			error = in_fclose(in_fp);
	} else
		(void) in_fclose(in_fp);

	return (error);
}
