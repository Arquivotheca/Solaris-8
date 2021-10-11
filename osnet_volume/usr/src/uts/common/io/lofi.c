/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lofi.c	1.1	99/07/27 SMI"

/*
 * lofi (loopback file) driver - allows you to attach a file to a device,
 * which can then be accessed through that device. The simple model is that
 * you tell lofi to open a file, and then use the block device you get as
 * you would any block device. lofi translates access to the block device
 * into I/O on the underlying file. This is mostly useful for
 * mounting images of filesystems.
 *
 * lofi is controlled through /dev/lofictl - this is the only device exported
 * during attach, and is minor number 0. lofiadm communicates with lofi through
 * ioctls on this device. When a file is attached to lofi, block and character
 * devices are exported in /dev/lofi and /dev/rlofi. Currently, these devices
 * are identified by their minor number, and the minor number is also used
 * as the name in /dev/lofi. If we ever decide to support virtual disks,
 * we'll have to divide the minor number space to identify fdisk partitions
 * and slices, and the name will then be the minor number shifted down a
 * few bits. Minor devices are tracked with state structures handled with
 * ddi_soft_state(9F) for simplicity.
 *
 * A file attached to lofi is opened when attached and not closed until
 * explicitly detached from lofi. This seems more sensible than deferring
 * the open until the /dev/lofi device is opened, for a number of reasons.
 * One is that any failure is likely to be noticed by the person (or script)
 * running lofiadm. Another is that it would be a security problem if the
 * file was replaced by another one after being added but before being opened.
 *
 * The only hard part about lofi is the ioctls. In order to support things
 * like 'newfs' on a lofi device, it needs to support certain disk ioctls.
 * So it has to fake disk geometry and partition information. More may need
 * to be faked if your favorite utility doesn't work and you think it should
 * (fdformat doesn't work because it really wants to know the type of floppy
 * controller to talk to, and that didn't seem easy to fake. Or possibly even
 * necessary, since we have mkfs_pcfs now).
 *
 * Known problems:
 *
 *	UFS logging. Mounting a UFS filesystem image "logging"
 *	works for basic copy testing but wedges during a build of ON through
 *	that image. Some deadlock in lufs holding the log mutex and then
 *	getting stuck on a buf. So for now, don't do that.
 *
 *	Direct I/O. Since the filesystem data is being cached in the buffer
 *	cache, _and_ again in the underlying filesystem, it's tempting to
 *	enable direct I/O on the underlying file. Don't, because that deadlocks.
 *	I think to fix the cache-twice problem we might need filesystem support.
 *
 *	lofi on itself. The simple lock strategy (lofi_lock) precludes this
 *	because you'll be in lofi_ioctl, holding the lock when you open the
 *	file, which, if it's lofi, will grab lofi_lock. We prevent this for
 *	now, though not using ddi_soft_state(9F) would make it possible to
 *	do. Though it would still be silly.
 *
 * Interesting things to do:
 *
 *	Allow multiple files for each device. A poor-man's metadisk, basically.
 *
 *	Pass-through ioctls on block devices. You can (though it's not
 *	documented), give lofi a block device as a file name. Then we shouldn't
 *	need to fake a geometry. But this is also silly unless you're replacing
 *	metadisk.
 *
 *	Encryption. tpm would like this. Apparently Windows 2000 has it, and
 *	so does Linux.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/aio_req.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <sys/lofi.h>
#include <sys/vol.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/filio.h>
#include <sys/fdio.h>
#include <sys/open.h>
#include <sys/disp.h>
#include <vm/seg_map.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static dev_info_t *lofi_dip;
static void	*lofi_statep;
static uint32_t	lofi_maxminor;		/* maximum minor number allocated */
static kmutex_t lofi_lock;		/* state lock */
static int lofi_taskq_nthreads = 4;	/* # of taskq threads per device */

/*
 * Change an 'open type', from open/close, to a bit (since the open types
 * aren't bits).
 */
static int
otyp_to_bit(int otyp)
{
	switch (otyp) {
	case OTYP_CHR:
		return (LOFI_CHR_OPEN);
	case OTYP_BLK:
		return (LOFI_BLK_OPEN);
	case OTYP_LYR:
		return (LOFI_LYR_OPEN);
	default:
		return (-1);
	}
}

/*ARGSUSED3*/
static int
lofi_open(dev_t *devp, int flag, int otyp, struct cred *credp)
{
	minor_t	minor;
	struct lofi_state *lsp;
	int	openflag;

	mutex_enter(&lofi_lock);
	minor = getminor(*devp);
	if (minor == 0) {
		/* master control device */
		/* must be opened exclusively */
		if (((flag & FEXCL) != FEXCL) || (otyp != OTYP_CHR)) {
			mutex_exit(&lofi_lock);
			return (EINVAL);
		}
		lsp = ddi_get_soft_state(lofi_statep, 0);
		if (lsp == NULL) {
			mutex_exit(&lofi_lock);
			return (ENXIO);
		}
		if (lsp->ls_flags & LOFI_CHR_OPEN) {
			mutex_exit(&lofi_lock);
			return (EBUSY);
		}
		lsp->ls_flags |= LOFI_CHR_OPEN;
		mutex_exit(&lofi_lock);
		return (0);
	}

	/* otherwise, the mapping should already exist */
	lsp = ddi_get_soft_state(lofi_statep, minor);
	if (lsp == NULL) {
		mutex_exit(&lofi_lock);
		return (EINVAL);
	}

	/*
	 * We cannot count opens, but we can keep track that there is
	 * an outstanding open of each type. This is needed because we
	 * cannot let you unmap a file that's currently open.
	 */
	openflag = otyp_to_bit(otyp);
	if (openflag == -1) {
		mutex_exit(&lofi_lock);
		return (EINVAL);
	}

	lsp->ls_flags |= openflag;

	mutex_exit(&lofi_lock);
	return (0);
}

/*ARGSUSED3*/
static int
lofi_close(dev_t dev, int flag, int otyp, struct cred *credp)
{
	minor_t	minor;
	struct lofi_state *lsp;
	int closeflag;

#ifdef lint
	flag = flag;
#endif
	mutex_enter(&lofi_lock);
	minor = getminor(dev);
	lsp = ddi_get_soft_state(lofi_statep, minor);
	if (lsp == NULL) {
		mutex_exit(&lofi_lock);
		return (EINVAL);
	}
	if (minor == 0) {
		/* master control device */
		lsp->ls_flags &= ~LOFI_CHR_OPEN;
		mutex_exit(&lofi_lock);
		return (0);
	}
	/* we know we're the last close of this type */
	closeflag = otyp_to_bit(otyp);
	lsp->ls_flags &= ~(closeflag);
	mutex_exit(&lofi_lock);
	return (0);
}

/*
 * This is basically what strategy used to be before we found we
 * needed task queues.
 */
static void
lofi_strategy_task(void *arg)
{
	struct buf *bp = (struct buf *)arg;
	int error;
	struct lofi_state *lsp;
	offset_t	offset;
	offset_t	mapoffset;
	caddr_t	bufaddr;
	caddr_t	mapaddr;
	size_t	xfersize;

	lsp = ddi_get_soft_state(lofi_statep, getminor(bp->b_edev));
	if (lsp->ls_kstat) {
		mutex_enter(lsp->ls_kstat->ks_lock);
		kstat_waitq_to_runq(KSTAT_IO_PTR(lsp->ls_kstat));
		mutex_exit(lsp->ls_kstat->ks_lock);
	}
	bp_mapin(bp);
	bufaddr = bp->b_un.b_addr;
	offset = bp->b_lblkno * DEV_BSIZE;	/* offset within file */

	/*
	 * We used to always use vn_rdwr here, but we cannot do that because
	 * we might decide to read or write from the the underlying
	 * file during this call, which would be a deadlock because
	 * we have the rw_lock. So instead we page, unless it's not
	 * mapable or it's a character device.
	 */
	if (((lsp->ls_vp->v_flag & VNOMAP) == 0) &&
	    (lsp->ls_vp->v_type != VCHR)) {
		/*
		 * segmap always gives us an 8K (MAXBSIZE) chunk, aligned on
		 * an 8K boundary, but the buf transfer address may not be
		 * aligned on more than a 512-byte boundary (we don't
		 * enforce that, though we could). This matters since the
		 * initial part of the transfer may not start at offset 0
		 * within the segmap'd chunk. So we have to compensate for
		 * that with 'mapoffset'. Subsequent chunks always start
		 * off at the beginning, and the last is capped by b_resid.
		 */
		mapoffset = offset & MAXBOFFSET;
		offset -= mapoffset;		/* now map-aligned */
		bp->b_resid = bp->b_bcount;
		do {
			xfersize = MIN(MAXBSIZE - mapoffset, bp->b_resid);
			mapaddr = segmap_getmap(segkmap, lsp->ls_vp, offset);
			if (bp->b_flags & B_READ)
				bcopy(mapaddr + mapoffset, bufaddr, xfersize);
			else
				bcopy(bufaddr, mapaddr + mapoffset, xfersize);
			bp->b_resid -= xfersize;
			bufaddr += xfersize;
			error = segmap_release(segkmap, mapaddr, 0);
			/* only the first map may start partial */
			mapoffset = 0;
			offset += MAXBSIZE;
		} while ((error == 0) && (bp->b_resid > 0));
	} else {
		ssize_t	resid;
		enum uio_rw rw;

		if (bp->b_flags & B_READ)
			rw = UIO_READ;
		else
			rw = UIO_WRITE;
		error = vn_rdwr(rw, lsp->ls_vp, bufaddr, bp->b_bcount,
		    offset, UIO_SYSSPACE, 0, RLIM64_INFINITY, kcred, &resid);
		bp->b_resid = resid;
	}

	if (lsp->ls_kstat) {
		size_t n_done = bp->b_bcount - bp->b_resid;
		kstat_io_t *kioptr;

		mutex_enter(lsp->ls_kstat->ks_lock);
		kioptr = KSTAT_IO_PTR(lsp->ls_kstat);
		if (bp->b_flags & B_READ) {
			kioptr->nread += n_done;
			kioptr->reads++;
		} else {
			kioptr->nwritten += n_done;
			kioptr->writes++;
		}
		kstat_runq_exit(kioptr);
		mutex_exit(lsp->ls_kstat->ks_lock);
	}
	bioerror(bp, error);
	biodone(bp);
}

static int
lofi_strategy(struct buf *bp)
{
	struct lofi_state *lsp;

	/*
	 * We cannot just do I/O here, because the current thread
	 * _might_ end up back in here because the underlying filesystem
	 * wants a buffer, which eventually gets into bio_recycle and
	 * might call into lofi to write out a delayed-write buffer.
	 * This is bad if the filesystem above lofi is the same as below.
	 *
	 * We could come up with a complex strategy using threads to
	 * do the I/O asynchronously, or we could use task queues. task
	 * queues were incredibly easy so they win.
	 */
	lsp = ddi_get_soft_state(lofi_statep, getminor(bp->b_edev));
	if (lsp->ls_kstat) {
		mutex_enter(lsp->ls_kstat->ks_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(lsp->ls_kstat));
		mutex_exit(lsp->ls_kstat->ks_lock);
	}
	(void) taskq_dispatch(lsp->ls_taskq, lofi_strategy_task, bp, KM_SLEEP);
	return (0);
}

/*ARGSUSED2*/
static int
lofi_read(dev_t dev, struct uio *uio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (physio(lofi_strategy, NULL, dev, B_READ, minphys, uio));
}

/*ARGSUSED2*/
static int
lofi_write(dev_t dev, struct uio *uio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (physio(lofi_strategy, NULL, dev, B_WRITE, minphys, uio));
}

/*ARGSUSED2*/
static int
lofi_aread(dev_t dev, struct aio_req *aio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (aphysio(lofi_strategy, anocancel, dev, B_READ, minphys, aio));
}

/*ARGSUSED2*/
static int
lofi_awrite(dev_t dev, struct aio_req *aio, struct cred *credp)
{
	if (getminor(dev) == 0)
		return (EINVAL);
	return (aphysio(lofi_strategy, anocancel, dev, B_WRITE, minphys, aio));
}

/*ARGSUSED*/
static int
lofi_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = lofi_dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
lofi_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	error;

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	error = ddi_create_minor_node(dip, LOFI_CTL_NODE, S_IFCHR, 0,
	    DDI_PSEUDO, NULL);
	if (error == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	lofi_dip = dip;
	error = ddi_soft_state_zalloc(lofi_statep, 0);
	if (error == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	ddi_report_dev(dip);
	return (DDI_SUCCESS);
}

static int
lofi_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_soft_state_free(lofi_statep, 0);
	ddi_remove_minor_node(dip, NULL);
	lofi_dip = NULL;
	return (DDI_SUCCESS);
}

/*
 * These two just simplify the rest of the ioctls that need to copyin/out
 * the lofi_ioctl structure.
 */
struct lofi_ioctl *
copy_in_lofi_ioctl(const struct lofi_ioctl *ulip)
{
	struct lofi_ioctl *klip;
	int	error;

	klip = kmem_alloc(sizeof (struct lofi_ioctl), KM_SLEEP);
	error = copyin(ulip, klip, sizeof (struct lofi_ioctl));
	if (error) {
		kmem_free(klip, sizeof (struct lofi_ioctl));
		return (NULL);
	}
	return (klip);
}

int
copy_out_lofi_ioctl(const struct lofi_ioctl *klip, struct lofi_ioctl *ulip)
{
	int	error;

	error = copyout(klip, ulip, sizeof (struct lofi_ioctl));
	if (error)
		return (EFAULT);
	return (0);
}

void
free_lofi_ioctl(struct lofi_ioctl *klip)
{
	kmem_free(klip, sizeof (struct lofi_ioctl));
}

/*
 * Return the minor number 'filename' is mapped to, if it is.
 */
static int
file_to_minor(char *filename)
{
	minor_t	minor;
	struct lofi_state *lsp;

	for (minor = 1; minor <= lofi_maxminor; minor++) {
		lsp = ddi_get_soft_state(lofi_statep, minor);
		if (lsp == NULL)
			continue;
		if (strcmp(lsp->ls_filename, filename) == 0)
			return (minor);
	}
	return (0);
}

/*
 * lofiadm does some validation, but since Joe Random (or crashme) could
 * do our ioctls, we need to do some validation too.
 */
static int
valid_filename(const char *filename)
{
	static char *blkprefix = "/dev/" LOFI_BLOCK_NAME "/";
	static char *charprefix = "/dev/" LOFI_CHAR_NAME "/";

	/* must be absolute path */
	if (filename[0] != '/')
		return (0);
	/* must not be lofi */
	if (strncmp(filename, blkprefix, strlen(blkprefix)) == 0)
		return (0);
	if (strncmp(filename, charprefix, strlen(charprefix)) == 0)
		return (0);
	return (1);
}

/*
 * Fakes up a disk geometry, and one big partition, based on the size
 * of the file. This is needed because we allow newfs'ing the device,
 * and newfs will do several disk ioctls to figure out the geometry and
 * partition information. It uses that information to determine the parameters
 * to pass to mkfs. Geometry is pretty much irrelevent these days, but we
 * have to support it.
 */
static void
fake_disk_geometry(struct lofi_state *lsp)
{
	/* dk_geom - see dkio(7I) */
	/*
	 * dkg_ncyl _could_ be set to one here (one big cylinder with gobs
	 * of sectors), but that breaks programs like fdisk which want to
	 * partition a disk by cylinder. With one cylinder, you can't create
	 * an fdisk partition and put pcfs on it for testing (hard to pick
	 * a number between one and one).
	 *
	 * The cheezy floppy test is an attempt to not have too few cylinders
	 * for a small file, or so many on a big file that you waste space
	 * for backup superblocks or cylinder group structures.
	 */
	if (lsp->ls_vp_size < (2 * 1024 * 1024)) /* floppy? */
		lsp->ls_dkg.dkg_ncyl = lsp->ls_vp_size / (100 * 1024);
	else
		lsp->ls_dkg.dkg_ncyl = lsp->ls_vp_size / (300 * 1024);
	/* in case file file is < 100k */
	if (lsp->ls_dkg.dkg_ncyl == 0)
		lsp->ls_dkg.dkg_ncyl = 1;
	lsp->ls_dkg.dkg_acyl = 0;
	lsp->ls_dkg.dkg_bcyl = 0;
	lsp->ls_dkg.dkg_nhead = 1;
	lsp->ls_dkg.dkg_obs1 = 0;
	lsp->ls_dkg.dkg_intrlv = 0;
	lsp->ls_dkg.dkg_obs2 = 0;
	lsp->ls_dkg.dkg_obs3 = 0;
	lsp->ls_dkg.dkg_apc = 0;
	lsp->ls_dkg.dkg_rpm = 7200;
	lsp->ls_dkg.dkg_pcyl = lsp->ls_dkg.dkg_ncyl + lsp->ls_dkg.dkg_acyl;
	lsp->ls_dkg.dkg_nsect = lsp->ls_vp_size /
	    (DEV_BSIZE * lsp->ls_dkg.dkg_ncyl);
	lsp->ls_dkg.dkg_write_reinstruct = 0;
	lsp->ls_dkg.dkg_read_reinstruct = 0;

	/* vtoc - see dkio(7I) */
	bzero(&lsp->ls_vtoc, sizeof (struct vtoc));
	lsp->ls_vtoc.v_sanity = VTOC_SANE;
	lsp->ls_vtoc.v_version = V_VERSION;
	bcopy(LOFI_DRIVER_NAME, lsp->ls_vtoc.v_volume, 7);
	lsp->ls_vtoc.v_sectorsz = DEV_BSIZE;
	lsp->ls_vtoc.v_nparts = 1;
	lsp->ls_vtoc.v_part[0].p_tag = V_UNASSIGNED;
	lsp->ls_vtoc.v_part[0].p_flag = V_UNMNT;
	lsp->ls_vtoc.v_part[0].p_start = (daddr_t)0;
	/*
	 * The partition size cannot just be the number of sectors, because
	 * that might not end on a cylinder boundary. And if that's the case,
	 * newfs/mkfs will print a scary warning. So just figure the size
	 * based on the number of cylinders and sectors/cylinder.
	 */
	lsp->ls_vtoc.v_part[0].p_size = lsp->ls_dkg.dkg_pcyl *
	    lsp->ls_dkg.dkg_nsect * lsp->ls_dkg.dkg_nhead;

	/* dk_cinfo - see dkio(7I) */
	bzero(&lsp->ls_ci, sizeof (struct dk_cinfo));
	(void) strcpy(lsp->ls_ci.dki_cname, LOFI_DRIVER_NAME);
	lsp->ls_ci.dki_ctype = DKC_MD;
	lsp->ls_ci.dki_flags = 0;
	lsp->ls_ci.dki_cnum = 0;
	lsp->ls_ci.dki_addr = 0;
	lsp->ls_ci.dki_space = 0;
	lsp->ls_ci.dki_prio = 0;
	lsp->ls_ci.dki_vec = 0;
	(void) strcpy(lsp->ls_ci.dki_dname, LOFI_DRIVER_NAME);
	lsp->ls_ci.dki_unit = 0;
	lsp->ls_ci.dki_slave = 0;
	lsp->ls_ci.dki_partition = 0;
	/*
	 * newfs uses this to set maxcontig. Must not be < 16, or it
	 * will be 0 when newfs multiplies it by DEV_BSIZE and divides
	 * it by the block size. Then tunefs doesn't work because
	 * maxcontig is 0.
	 */
	lsp->ls_ci.dki_maxtransfer = 16;
}

/*
 * map a file to a minor number. Return the minor number.
 */
static int
lofi_map_file(dev_t dev, struct lofi_ioctl *ulip, int pickminor,
    int *rvalp, struct cred *credp)
{
	minor_t	newminor;
	struct lofi_state *lsp;
	struct lofi_ioctl *klip;
	int	error;
	char	namebuf[50];
	struct vnode *vp;
	int64_t	Nblocks_prop_val;
	vattr_t	vattr;
	int	flag;
	enum vtype v_type;

	klip = copy_in_lofi_ioctl(ulip);
	if (klip == NULL)
		return (EFAULT);

	mutex_enter(&lofi_lock);

	if (!valid_filename(klip->li_filename)) {
		error = EINVAL;
		goto out;
	}

	if (file_to_minor(klip->li_filename) != 0) {
		error = EBUSY;
		goto out;
	}

	if (pickminor) {
		/* Find a free one */
		for (newminor = 1; newminor <= lofi_maxminor; newminor++)
			if (ddi_get_soft_state(lofi_statep, newminor) == NULL)
				break;
	} else {
		newminor = klip->li_minor;
		if (ddi_get_soft_state(lofi_statep, newminor) != NULL) {
			error = EEXIST;
			goto out;
		}
	}

	/* make sure it's valid */
	error = lookupname(klip->li_filename, UIO_SYSSPACE, FOLLOW,
	    NULLVPP, &vp);
	if (error) {
		goto out;
	}
	v_type = vp->v_type;
	VN_RELE(vp);
	if (!V_ISLOFIABLE(v_type)) {
		error = EINVAL;
		goto out;
	}
	flag = FREAD | FWRITE | FOFFMAX | FEXCL;
	error = vn_open(klip->li_filename, UIO_SYSSPACE, flag, 0, &vp, 0, 0);
	if (error) {
		/* try read-only */
		flag &= ~FWRITE;
		error = vn_open(klip->li_filename, UIO_SYSSPACE, flag, 0,
		    &vp, 0, 0);
		if (error) {
			goto out;
		}
	}
	error = VOP_GETATTR(vp, &vattr, AT_ALL, credp);
	if (error) {
		goto closeout;
	}
	Nblocks_prop_val = vattr.va_size / DEV_BSIZE;
	if (ddi_prop_update_byte_array(dev, lofi_dip, "Nblocks",
	    (uchar_t *)&Nblocks_prop_val, sizeof (int64_t)) != DDI_SUCCESS) {
		error = EINVAL;
		goto closeout;
	}
	error = ddi_soft_state_zalloc(lofi_statep, newminor);
	if (error == DDI_FAILURE) {
		error = ENOMEM;
		goto propout;
	}
	(void) snprintf(namebuf, sizeof (namebuf), "%d", newminor);
	(void) ddi_create_minor_node(lofi_dip, namebuf, S_IFBLK, newminor,
	    DDI_PSEUDO, NULL);
	if (error != DDI_SUCCESS) {
		error = ENXIO;
		goto propout;
	}
	(void) snprintf(namebuf, sizeof (namebuf), "%d,raw", newminor);
	error = ddi_create_minor_node(lofi_dip, namebuf, S_IFCHR, newminor,
	    DDI_PSEUDO, NULL);
	if (error != DDI_SUCCESS) {
		/* remove block node */
		(void) snprintf(namebuf, sizeof (namebuf), "%d", newminor);
		ddi_remove_minor_node(lofi_dip, namebuf);
		error = ENXIO;
		goto propout;
	}
	lsp = ddi_get_soft_state(lofi_statep, newminor);
	lsp->ls_filename_sz = strlen(klip->li_filename) + 1;
	lsp->ls_filename = kmem_alloc(lsp->ls_filename_sz, KM_SLEEP);
	(void) snprintf(namebuf, sizeof (namebuf), "%s_taskq_%d",
	    LOFI_DRIVER_NAME, newminor);
	lsp->ls_taskq = taskq_create(namebuf, lofi_taskq_nthreads,
	    minclsyspri, 1, 2 * lofi_taskq_nthreads, 0);
	lsp->ls_kstat = kstat_create(LOFI_DRIVER_NAME, newminor,
	    NULL, "disk", KSTAT_TYPE_IO, 1, 0);
	if (lsp->ls_kstat) {
		mutex_init(&lsp->ls_kstat_lock, NULL, MUTEX_DRIVER, NULL);
		lsp->ls_kstat->ks_lock = &lsp->ls_kstat_lock;
		kstat_install(lsp->ls_kstat);
	}
	lsp->ls_vp = vp;
	lsp->ls_vp_size = vattr.va_size;
	(void) strcpy(lsp->ls_filename, klip->li_filename);
	if (newminor > lofi_maxminor)
		lofi_maxminor = newminor;
	if (rvalp)
		*rvalp = (int)newminor;
	klip->li_minor = newminor;

	fake_disk_geometry(lsp);
	mutex_exit(&lofi_lock);
	(void) copy_out_lofi_ioctl(klip, ulip);
	free_lofi_ioctl(klip);
	return (0);

propout:
	(void) ddi_prop_remove(dev, lofi_dip, "Nblocks");
closeout:
	(void) VOP_CLOSE(vp, flag, 1, 0, credp);
	VN_RELE(vp);
out:
	mutex_exit(&lofi_lock);
	free_lofi_ioctl(klip);
	return (error);
}

/*
 * unmap a file.
 */
static int
lofi_unmap_file(dev_t dev, struct lofi_ioctl *ulip, int byfilename,
    struct cred *credp)
{
	struct lofi_state *lsp;
	struct lofi_ioctl *klip;
	minor_t	minor;
	char	namebuf[20];
	int	flag;

	klip = copy_in_lofi_ioctl(ulip);
	if (klip == NULL)
		return (EFAULT);

	mutex_enter(&lofi_lock);
	if (byfilename) {
		minor = file_to_minor(klip->li_filename);
	} else {
		minor = klip->li_minor;
	}
	if (minor == 0) {
		mutex_exit(&lofi_lock);
		free_lofi_ioctl(klip);
		return (ENXIO);
	}
	lsp = ddi_get_soft_state(lofi_statep, minor);
	if (lsp == NULL) {
		mutex_exit(&lofi_lock);
		free_lofi_ioctl(klip);
		return (ENXIO);
	}
	if (lsp->ls_flags & LOFI_OPEN_FLAGS) {
		mutex_exit(&lofi_lock);
		free_lofi_ioctl(klip);
		return (EBUSY);
	}
	flag = FREAD | FWRITE | FOFFMAX;
	flag |= FEXCL;
	(void) VOP_CLOSE(lsp->ls_vp, flag, 1, 0, credp);
	VN_RELE(lsp->ls_vp);
	lsp->ls_vp = NULL;
	(void) ddi_prop_remove(dev, lofi_dip, "Nblocks");

	(void) snprintf(namebuf, sizeof (namebuf), "%d", minor);
	ddi_remove_minor_node(lofi_dip, namebuf);
	(void) snprintf(namebuf, sizeof (namebuf), "%d,raw", minor);
	ddi_remove_minor_node(lofi_dip, namebuf);

	kmem_free(lsp->ls_filename, lsp->ls_filename_sz);
	taskq_destroy(lsp->ls_taskq);
	if (lsp->ls_kstat) {
		kstat_delete(lsp->ls_kstat);
		mutex_destroy(&lsp->ls_kstat_lock);
	}
	ddi_soft_state_free(lofi_statep, minor);
	klip->li_minor = minor;
	mutex_exit(&lofi_lock);
	(void) copy_out_lofi_ioctl(klip, ulip);
	free_lofi_ioctl(klip);
	return (0);
}

/*
 * get the filename given the minor number, or the minor number given
 * the name.
 */
/*ARGSUSED3*/
static int
lofi_get_info(dev_t dev, struct lofi_ioctl *ulip, int which,
    struct cred *credp)
{
	struct lofi_state *lsp;
	struct lofi_ioctl *klip;
	int	error;
	minor_t	minor;

#ifdef lint
	dev = dev;
#endif
	klip = copy_in_lofi_ioctl(ulip);
	if (klip == NULL)
		return (EFAULT);

	switch (which) {
	case LOFI_GET_FILENAME:
		minor = klip->li_minor;
		mutex_enter(&lofi_lock);
		lsp = ddi_get_soft_state(lofi_statep, minor);
		if (lsp == NULL) {
			mutex_exit(&lofi_lock);
			free_lofi_ioctl(klip);
			return (ENXIO);
		}
		(void) strcpy(klip->li_filename, lsp->ls_filename);
		mutex_exit(&lofi_lock);
		error = copy_out_lofi_ioctl(klip, ulip);
		free_lofi_ioctl(klip);
		return (error);
	case LOFI_GET_MINOR:
		mutex_enter(&lofi_lock);
		klip->li_minor = file_to_minor(klip->li_filename);
		mutex_exit(&lofi_lock);
		if (klip->li_minor == 0)
			return (ENOENT);
		error = copy_out_lofi_ioctl(klip, ulip);
		free_lofi_ioctl(klip);
		return (error);
	default:
		return (EINVAL);
	}

}

static int
lofi_ioctl(dev_t dev, int cmd, intptr_t arg, int flag, cred_t *credp,
    int *rvalp)
{
	int	error;
	enum dkio_state dkstate;
	struct lofi_state *lsp;
	minor_t	minor;

#ifdef lint
	credp = credp;
#endif

	minor = getminor(dev);
	/* lofi ioctls only apply to the master device */
	if (minor == 0) {
		struct lofi_ioctl *lip = (struct lofi_ioctl *)arg;

		/*
		 * the query command only need read-access - i.e., normal
		 * users are allowed to do those on the ctl device as
		 * long as they can open it read-only.
		 */
		switch (cmd) {
		case LOFI_MAP_FILE:
			if ((flag & FWRITE) == 0)
				return (EPERM);
			return (lofi_map_file(dev, lip, 1, rvalp, credp));
		case LOFI_MAP_FILE_MINOR:
			if ((flag & FWRITE) == 0)
				return (EPERM);
			return (lofi_map_file(dev, lip, 0, rvalp, credp));
		case LOFI_UNMAP_FILE:
			if ((flag & FWRITE) == 0)
				return (EPERM);
			return (lofi_unmap_file(dev, lip, 1, credp));
		case LOFI_UNMAP_FILE_MINOR:
			if ((flag & FWRITE) == 0)
				return (EPERM);
			return (lofi_unmap_file(dev, lip, 0, credp));
		case LOFI_GET_FILENAME:
			return (lofi_get_info(dev, lip, LOFI_GET_FILENAME,
			    credp));
		case LOFI_GET_MINOR:
			return (lofi_get_info(dev, lip, LOFI_GET_MINOR,
			    credp));
		case LOFI_GET_MAXMINOR:
			mutex_enter(&lofi_lock);
			error = copyout(&lofi_maxminor, &lip->li_minor,
			    sizeof (lofi_maxminor));
			mutex_exit(&lofi_lock);
			return (error);
		default:
			break;
		}
	}

	lsp = ddi_get_soft_state(lofi_statep, minor);
	if (lsp == NULL)
		return (ENXIO);

	/* these are for faking out utilities like newfs */
	switch (cmd) {
	case VOLIOCINFO:
		/* pcfs does this to see if it needs to set PCFS_NOCHK */
		/* 0 means it should set it */
		return (0);
	case DKIOCGVTOC:
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct vtoc32 vtoc32;

			vtoctovtoc32(lsp->ls_vtoc, vtoc32);
			if (ddi_copyout(&vtoc32, (void *)arg,
			    sizeof (struct vtoc32), flag))
				return (EFAULT);
				break;
			}

		case DDI_MODEL_NONE:
			if (ddi_copyout(&lsp->ls_vtoc, (void *)arg,
			    sizeof (struct vtoc), flag))
				return (EFAULT);
			break;
		}
		return (0);
	case DKIOCINFO:
		error = copyout(&lsp->ls_ci, (void *)arg,
		    sizeof (struct dk_cinfo));
		if (error)
			return (EFAULT);
		return (0);
	case DKIOCG_VIRTGEOM:
	case DKIOCG_PHYGEOM:
	case DKIOCGGEOM:
		error = copyout(&lsp->ls_dkg, (void *)arg,
		    sizeof (struct dk_geom));
		if (error)
			return (EFAULT);
		return (0);
	case DKIOCSTATE:
		/* the file is always there */
		dkstate = DKIO_INSERTED;
		error = copyout(&dkstate, (void *)arg,
		    sizeof (enum dkio_state));
		if (error)
			return (EFAULT);
		return (0);
	default:
		return (ENOTTY);
	}
}

static struct cb_ops lofi_cb_ops = {
	lofi_open,		/* open */
	lofi_close,		/* close */
	lofi_strategy,		/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	lofi_read,		/* read */
	lofi_write,		/* write */
	lofi_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_64BIT | D_NEW | D_MP,	/* Driver compatibility flag */
	CB_REV,
	lofi_aread,
	lofi_awrite
};

static struct dev_ops lofi_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	lofi_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	lofi_attach,		/* attach */
	lofi_detach,		/* detach */
	nodev,			/* reset */
	&lofi_cb_ops,		/* driver operations */
	NULL			/* no bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"loopback file driver (1.1)",
	&lofi_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int error;

	error = ddi_soft_state_init(&lofi_statep,
	    sizeof (struct lofi_state), 0);
	if (error)
		return (error);

	mutex_init(&lofi_lock, NULL, MUTEX_DRIVER, NULL);
	error = mod_install(&modlinkage);
	if (error) {
		mutex_destroy(&lofi_lock);
		ddi_soft_state_fini(&lofi_statep);
	}

	return (error);
}

int
_fini(void)
{
	int	error;
	minor_t	minor;

	/*
	 * We need to make sure no mappings exist - mod_remove won't
	 * help because the device isn't open.
	 */
	mutex_enter(&lofi_lock);
	for (minor = 1; minor <= lofi_maxminor; minor++) {
		if (ddi_get_soft_state(lofi_statep, minor) != NULL) {
			mutex_exit(&lofi_lock);
			return (EBUSY);
		}
	}
	mutex_exit(&lofi_lock);

	error = mod_remove(&modlinkage);
	if (error)
		return (error);

	mutex_destroy(&lofi_lock);
	ddi_soft_state_fini(&lofi_statep);

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
