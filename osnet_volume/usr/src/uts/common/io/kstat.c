/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)kstat.c	1.14	97/12/22 SMI"

/*
 * kernel statistics driver
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/t_lock.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/cpuvar.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/kstat.h>

static int kstat_open(dev_t *, int, int, struct cred *);
static int kstat_close(dev_t, int, int, struct cred *);
static int kstat_ioctl(dev_t, int, intptr_t, int, struct cred *, int *);
static dev_info_t *kstat_devi;

static int read_kstat_data(int *, void *, int, cred_t *);
static int write_kstat_data(int *, void *, int, cred_t *);

static struct cb_ops kstat_cb_ops = {
	kstat_open,		/* open */
	kstat_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	kstat_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static int kstat_identify(dev_info_t *);
static int kstat_attach(dev_info_t *, ddi_attach_cmd_t);
static int kstat_detach(dev_info_t *, ddi_detach_cmd_t);
static int kstat_info(dev_info_t *, ddi_info_cmd_t, void *, void **);

static struct dev_ops kstat_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	kstat_info,		/* get_dev_info */
	kstat_identify,		/* identify */
	nulldev,		/* probe */
	kstat_attach,		/* attach */
	kstat_detach,		/* detach */
	nodev,			/* reset */
	&kstat_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* no bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"kernel statistics driver",
	&kstat_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
kstat_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "kstat") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

static int
kstat_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if (ddi_create_minor_node(devi, "kstat", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	kstat_devi = devi;
	return (DDI_SUCCESS);
}

static int
kstat_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
kstat_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)kstat_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * kstat_active is non-zero if and only if /dev/kstat is open.
 * Since kstat_open() is called for every open, but kstat_close() is
 * only called for the last close, the intuitive thing to do
 * (kstat_active++ on open, kstat_active-- on close) is wrong.
 * So instead, we simply set kstat_active = 1 on each open, and
 * set kstat_active = 0 on the last close.
 */

/*ARGSUSED*/
static int
kstat_open(dev_t *devp, int flag, int otyp, struct cred *cred)
{
	mutex_enter(&kstat_chain_lock);
	kstat_active = 1;
	mutex_exit(&kstat_chain_lock);
	return (0);
}

/*ARGSUSED*/
static int
kstat_close(dev_t dev, int flag, int otyp, struct cred *cred)
{
	mutex_enter(&kstat_chain_lock);
	kstat_active = 0;
	mutex_exit(&kstat_chain_lock);
	return (0);
}

/*ARGSUSED*/
static int
kstat_ioctl(dev_t dev, int cmd, intptr_t data, int flag,
	cred_t *cred, int *rvalp)
{
	int rc = 0;

	switch (cmd) {

	case KSTAT_IOC_CHAIN_ID:
		*rvalp = kstat_chain_id;
		break;

	case KSTAT_IOC_READ:
		rc = read_kstat_data(rvalp, (void *)data, flag, cred);
		break;

	case KSTAT_IOC_WRITE:
		rc = write_kstat_data(rvalp, (void *)data, flag, cred);
		break;

	default:
		/* invalid request */
		rc = EINVAL;
	}
	return (rc);
}

/*ARGSUSED3*/
static int
read_kstat_data(int *rvalp, void *user_ksp, int flag, cred_t *cred)
{
	kstat_t user_kstat, *ksp;
#ifdef _MULTI_DATAMODEL
	kstat32_t user_kstat32;
#endif
	void *kbuf = NULL;
	size_t kbufsize, ubufsize, copysize;
	int error = 0;
	uint_t model;

	switch (model = ddi_model_convert_from(flag & FMODELS)) {
#ifdef _MULTI_DATAMODEL
	case DDI_MODEL_ILP32:
		if (copyin(user_ksp, &user_kstat32, sizeof (kstat32_t)))
			return (EFAULT);
		user_kstat.ks_crtime	= user_kstat32.ks_crtime;
		user_kstat.ks_next	= (void *)user_kstat32.ks_next;
		user_kstat.ks_kid	= user_kstat32.ks_kid;
		(void) strcpy(user_kstat.ks_module, user_kstat32.ks_module);
		user_kstat.ks_resv	= user_kstat32.ks_resv;
		user_kstat.ks_instance	= user_kstat32.ks_instance;
		(void) strcpy(user_kstat.ks_name, user_kstat32.ks_name);
		user_kstat.ks_type	= user_kstat32.ks_type;
		(void) strcpy(user_kstat.ks_class, user_kstat32.ks_class);
		user_kstat.ks_flags	= user_kstat32.ks_flags;
		user_kstat.ks_data	= (void *)user_kstat32.ks_data;
		user_kstat.ks_ndata	= user_kstat32.ks_ndata;
		user_kstat.ks_data_size = (size_t)user_kstat32.ks_data_size;
		user_kstat.ks_snaptime	= user_kstat32.ks_snaptime;
		break;
#endif
	default:
	case DDI_MODEL_NONE:
		if (copyin(user_ksp, &user_kstat, sizeof (kstat_t)))
			return (EFAULT);
	}

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_bykid(user_kstat.ks_kid);
	if (ksp == NULL) {
		/*
		 * There is no kstat with the specified KID
		 */
		mutex_exit(&kstat_chain_lock);
		return (ENXIO);
	}
	if (ksp->ks_flags & KSTAT_FLAG_INVALID) {
		/*
		 * The kstat exists, but is momentarily in some
		 * indeterminate state (e.g. the data section is not
		 * yet initialized).  Try again in a few milliseconds.
		 */
		mutex_exit(&kstat_chain_lock);
		return (EAGAIN);
	}
	/*
	 * If it's a fixed-size kstat, allocate the buffer now, so we
	 * don't have to do it under the kstat's data lock.  (If it's a
	 * var-size kstat, we don't know the size until after the update
	 * routine is called, so we can't do this optimization.)
	 * The allocator relies on this behavior to prevent recursive
	 * mutex_enter in its (fixed-size) kstat update routine.
	 * It's a zalloc to prevent unintentional exposure of random
	 * juicy morsels of (old) kernel data.
	 */
	if (!(ksp->ks_flags & KSTAT_FLAG_VAR_SIZE)) {
		kbufsize = ksp->ks_data_size;
		kbuf = kmem_zalloc(kbufsize + 1, KM_NOSLEEP);
		if (kbuf == NULL) {
			mutex_exit(&kstat_chain_lock);
			return (EAGAIN);
		}
	}
	KSTAT_ENTER(ksp);
	if ((error = KSTAT_UPDATE(ksp, KSTAT_READ)) != 0) {
		KSTAT_EXIT(ksp);
		mutex_exit(&kstat_chain_lock);
		if (kbuf != NULL)
			kmem_free(kbuf, kbufsize + 1);
		return (error);
	}

	kbufsize = ksp->ks_data_size;
	ubufsize = user_kstat.ks_data_size;

	if (ubufsize < kbufsize) {
		error = ENOMEM;
	} else {
		if (kbuf == NULL)
			kbuf = kmem_zalloc(kbufsize + 1, KM_NOSLEEP);
		if (kbuf == NULL) {
			error = EAGAIN;
		} else {
			error = KSTAT_SNAPSHOT(ksp, kbuf, KSTAT_READ);
		}
	}
	/*
	 * The following info must be returned to user level,
	 * even if the the update or snapshot failed.  This allows
	 * kstat readers to get a handle on variable-size kstats,
	 * detect dormant kstats, etc.
	 */
	user_kstat.ks_ndata	= ksp->ks_ndata;
	user_kstat.ks_data_size	= kbufsize;
	user_kstat.ks_flags	= ksp->ks_flags;
	user_kstat.ks_snaptime	= ksp->ks_snaptime;

	KSTAT_EXIT(ksp);
	*rvalp = kstat_chain_id;
	mutex_exit(&kstat_chain_lock);

	/*
	 * Copy the buffer containing the kstat back to userland.
	 */
	copysize = kbufsize;
	if (kbuf != NULL) {
#ifdef _MULTI_DATAMODEL
		kstat32_t *k32;
		kstat_t *k;
		int i;

		switch (model) {
		case DDI_MODEL_ILP32:

			if (user_kstat.ks_type == KSTAT_TYPE_NAMED) {
				kstat_named_t *kn = kbuf;

				/*
				 * Named statistics have 'long' fields.
				 * For a 32-bit application looking at a
				 * 64-bit kernel, forcibly truncate these
				 * 64-bit quantities to 32-bit values.
				 */
				for (i = 0; i < user_kstat.ks_ndata; kn++, i++)
					switch (kn->data_type) {
					case KSTAT_DATA_LONG:
						kn->value.i32 =
						    (int32_t)kn->value.l;
						kn->data_type =
						    KSTAT_DATA_INT32;
						break;
					case KSTAT_DATA_ULONG:
						kn->value.ui32 =
						    (uint32_t)kn->value.ul;
						kn->data_type =
						    KSTAT_DATA_UINT32;
						break;
					default:
						break;
					}
			}

			if (user_kstat.ks_kid != 0)
				break;

			/*
			 * This is the special case of the kstat header
			 * list for the entire system.  Reshape the
			 * array in place, then copy it out.
			 */
			k32 = kbuf;
			k = kbuf;
			for (i = 0; i < user_kstat.ks_ndata; k32++, k++, i++) {
				k32->ks_crtime		= k->ks_crtime;
				k32->ks_next		= 0;
				k32->ks_kid		= k->ks_kid;
				(void) strcpy(k32->ks_module, k->ks_module);
				k32->ks_resv		= k->ks_resv;
				k32->ks_instance	= k->ks_instance;
				(void) strcpy(k32->ks_name, k->ks_name);
				k32->ks_type		= k->ks_type;
				(void) strcpy(k32->ks_class, k->ks_class);
				k32->ks_flags		= k->ks_flags;
				k32->ks_data		= 0;
				k32->ks_ndata		= k->ks_ndata;
				if (k->ks_data_size > UINT32_MAX) {
					error = EOVERFLOW;
					break;
				}
				k32->ks_data_size = (size32_t)k->ks_data_size;
				k32->ks_snaptime	= k->ks_snaptime;
			}

			/*
			 * XXX	In this case we copy less data than is
			 *	claimed in the header.
			 */
			copysize = user_kstat.ks_ndata * sizeof (kstat32_t);
			break;

		default:
		case DDI_MODEL_NONE:
#ifdef _LP64
			if (user_kstat.ks_type == KSTAT_TYPE_NAMED) {
				kstat_named_t *kn = kbuf;

				for (i = 0; i < user_kstat.ks_ndata; kn++, i++)
					switch (kn->data_type) {
					case KSTAT_DATA_LONG:
						kn->data_type =
						    KSTAT_DATA_INT64;
						break;
					case KSTAT_DATA_ULONG:
						kn->data_type =
						    KSTAT_DATA_UINT64;
						break;
					default:
						break;
					}
			}
#endif	/* _LP64 */
			break;
		}
#endif	/* _MULTI_DATAMODEL */

		if (error == 0 &&
		    copyout(kbuf, user_kstat.ks_data, copysize))
			error = EFAULT;
		kmem_free(kbuf, kbufsize + 1);
	}

	/*
	 * We have modified the ks_ndata, ks_data_size, ks_flags, and
	 * ks_snaptime fields of the user kstat; now copy it back to userland.
	 */
	switch (model) {
#ifdef _MULTI_DATAMODEL
	case DDI_MODEL_ILP32:
		if (kbufsize > UINT32_MAX) {
			error = EOVERFLOW;
			break;
		}
		user_kstat32.ks_ndata		= user_kstat.ks_ndata;
		user_kstat32.ks_data_size	= (size32_t)kbufsize;
		user_kstat32.ks_flags		= user_kstat.ks_flags;
		user_kstat32.ks_snaptime	= user_kstat.ks_snaptime;
		if (copyout(&user_kstat32, user_ksp, sizeof (kstat32_t)) &&
		    error == 0)
			error = EFAULT;
		break;
#endif
	default:
	case DDI_MODEL_NONE:
		if (copyout(&user_kstat, user_ksp, sizeof (kstat_t)) &&
		    error == 0)
			error = EFAULT;
		break;
	}

	return (error);
}

static int
write_kstat_data(int *rvalp, void *user_ksp, int flag, cred_t *cred)
{
	kstat_t user_kstat, *ksp;
	void *buf = NULL;
	size_t bufsize;
	int error = 0;

	if (!suser(cred))
		return (EPERM);

	switch (ddi_model_convert_from(flag & FMODELS)) {
#ifdef _MULTI_DATAMODEL
		kstat32_t user_kstat32;

	case DDI_MODEL_ILP32:
		if (copyin(user_ksp, &user_kstat32, sizeof (kstat32_t)))
			return (EFAULT);
		/*
		 * These are the only fields we actually look at.
		 */
		user_kstat.ks_kid	= user_kstat32.ks_kid;
		user_kstat.ks_data	= (void *)user_kstat32.ks_data;
		user_kstat.ks_data_size = (size_t)user_kstat32.ks_data_size;
		break;
#endif
	default:
	case DDI_MODEL_NONE:
		if (copyin(user_ksp, &user_kstat, sizeof (kstat_t)))
			return (EFAULT);
	}

	bufsize = user_kstat.ks_data_size;
	buf = kmem_alloc(bufsize + 1, KM_NOSLEEP);
	if (buf == NULL)
		return (EAGAIN);

	if (copyin(user_kstat.ks_data, buf, bufsize)) {
		kmem_free(buf, bufsize + 1);
		return (EFAULT);
	}

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_bykid(user_kstat.ks_kid);
	if (ksp == NULL) {
		mutex_exit(&kstat_chain_lock);
		kmem_free(buf, bufsize + 1);
		return (ENXIO);
	}
	if (ksp->ks_flags & KSTAT_FLAG_INVALID) {
		mutex_exit(&kstat_chain_lock);
		kmem_free(buf, bufsize + 1);
		return (EAGAIN);
	}
	if (!(ksp->ks_flags & KSTAT_FLAG_WRITABLE)) {
		mutex_exit(&kstat_chain_lock);
		kmem_free(buf, bufsize + 1);
		return (EACCES);
	}
#ifdef _LP64
	/*
	 * For the 64-bit kernel, we have to ensure that we don't
	 * accidentally change the type of existing kstat_named statistics
	 * when writing over them.
	 */
	if (ksp->ks_type == KSTAT_TYPE_NAMED) {
		kstat_named_t *kold = ksp->ks_data;
		kstat_named_t *knew = buf;
		int i;

		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32:
			for (i = 0; i < ksp->ks_ndata; i++, kold++, knew++) {
				if (kold->data_type == KSTAT_DATA_LONG &&
				    knew->data_type == KSTAT_DATA_INT32)
					knew->value.l = (long)knew->value.i32;
				else if (kold->data_type == KSTAT_DATA_ULONG &&
				    knew->data_type == KSTAT_DATA_UINT32)
					knew->value.ul =
					    (ulong_t)knew->value.ui32;
				knew->data_type = kold->data_type;
			}
			break;

		default:
		case DDI_MODEL_NATIVE:
			for (i = 0; i < ksp->ks_ndata; i++)
				knew[i].data_type = kold[i].data_type;
			break;
		}
	}
#endif
	KSTAT_ENTER(ksp);
	error = KSTAT_SNAPSHOT(ksp, buf, KSTAT_WRITE);
	if (!error)
		error = KSTAT_UPDATE(ksp, KSTAT_WRITE);
	KSTAT_EXIT(ksp);
	*rvalp = kstat_chain_id;
	mutex_exit(&kstat_chain_lock);
	kmem_free(buf, bufsize + 1);
	return (error);
}
