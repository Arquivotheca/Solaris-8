/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cmdk.c	1.57	99/05/20 SMI"

#include <sys/scsi/scsi.h>
#include <sys/dktp/cm.h>
#include <sys/dktp/queue.h>
#include <sys/dktp/flowctrl.h>
#include <sys/dktp/objmgr.h>
#include <sys/dktp/cmdev.h>
#include <sys/dktp/cmdk.h>
#include <sys/dktp/tgdk.h>
#include <sys/dktp/bbh.h>

#include <sys/stat.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/file.h>
#include <sys/dktp/dadkio.h>
#include <sys/dktp/dklb.h>
#include <sys/aio_req.h>

/*
 * Local Static Data
 */
#ifdef CMDK_DEBUG
#define	DENT	0x0001
#define	DIO	0x0002

static	int	cmdk_debug = DIO;
#endif

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

static void *cmdk_state;
static int cmdk_max_instance = 0;

/*
 * Panic dumpsys state
 * There is only a single flag that is not mutex locked since
 * the system is prevented from thread switching and cmdk_dump
 * will only be called in a single threaded operation.
 */
static int	cmdk_indump;

/*
 * the cmdk_attach_mutex protects cmdk_max_instance in multi-threaded
 * attach situations
 */
static kmutex_t cmdk_attach_mutex;

static struct driver_minor_data {
	char    *name;
	int	minor;
	int	type;
} cmdk_minor_data[] = {
	{"a", 0, S_IFBLK},
	{"b", 1, S_IFBLK},
	{"c", 2, S_IFBLK},
	{"d", 3, S_IFBLK},
	{"e", 4, S_IFBLK},
	{"f", 5, S_IFBLK},
	{"g", 6, S_IFBLK},
	{"h", 7, S_IFBLK},
	{"i", 8, S_IFBLK},
	{"j", 9, S_IFBLK},
	{"k", 10, S_IFBLK},
	{"l", 11, S_IFBLK},
	{"m", 12, S_IFBLK},
	{"n", 13, S_IFBLK},
	{"o", 14, S_IFBLK},
	{"p", 15, S_IFBLK},
	{"q", 16, S_IFBLK},
	{"r", 17, S_IFBLK},
	{"s", 18, S_IFBLK},
	{"t", 19, S_IFBLK},
	{"u", 20, S_IFBLK},
	{"a,raw", 0, S_IFCHR},
	{"b,raw", 1, S_IFCHR},
	{"c,raw", 2, S_IFCHR},
	{"d,raw", 3, S_IFCHR},
	{"e,raw", 4, S_IFCHR},
	{"f,raw", 5, S_IFCHR},
	{"g,raw", 6, S_IFCHR},
	{"h,raw", 7, S_IFCHR},
	{"i,raw", 8, S_IFCHR},
	{"j,raw", 9, S_IFCHR},
	{"k,raw", 10, S_IFCHR},
	{"l,raw", 11, S_IFCHR},
	{"m,raw", 12, S_IFCHR},
	{"n,raw", 13, S_IFCHR},
	{"o,raw", 14, S_IFCHR},
	{"p,raw", 15, S_IFCHR},
	{"q,raw", 16, S_IFCHR},
	{"r,raw", 17, S_IFCHR},
	{"s,raw", 18, S_IFCHR},
	{"t,raw", 19, S_IFCHR},
	{"u,raw", 20, S_IFCHR},
	{0}
};

/*
 * Local Function Prototypes
 */
static int cmdk_reopen(struct cmdk *dkp);
static int cmdk_create_obj(dev_info_t *devi, struct cmdk *dkp);
static void cmdk_destroy_obj(dev_info_t *devi, struct cmdk *dkp, int unload);
static int cmdk_create_lbobj(dev_info_t *devi, struct cmdk *dkp);
static void cmdk_destroy_lbobj(dev_info_t *devi, struct cmdk *dkp, int unload);
static void cmdkmin(struct buf *bp);
static int cmdkrw(dev_t dev, struct uio *uio, int flag);
static int cmdkarw(dev_t dev, struct aio_req *aio, int flag);
static int cmdk_part_info(struct cmdk *dkp, int force, daddr_t *startp,
			long *countp, int part);
static void cmdk_part_info_init(struct cmdk *dkp);
static void cmdk_part_info_fini(struct cmdk *dkp);

#ifdef	NOT_USED
static void cmdk_devstatus(struct cmdk *dkp);
#endif	NOT_USED

/*
 * Bad Block Handling Functions Prototypes
 */
static opaque_t cmdk_bbh_gethandle(struct cmdk *dkp, struct buf *bp);
static bbh_cookie_t cmdk_bbh_htoc(struct cmdk *dkp, opaque_t handle);
static void cmdk_bbh_freehandle(struct cmdk *dkp, opaque_t handle);

static struct bbh_objops cmdk_bbh_ops = {
	nulldev,
	nulldev,
	cmdk_bbh_gethandle,
	cmdk_bbh_htoc,
	cmdk_bbh_freehandle,
	0, 0
};

static struct bbh_obj cmdk_bbh_obj = {
	NULL,
	&cmdk_bbh_ops
};

static int cmdkopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p);
static int cmdkclose(dev_t dev, int flag, int otyp, cred_t *cred_p);
static int cmdkstrategy(struct buf *bp);
static int cmdkdump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk);
static int cmdkioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int cmdkread(dev_t dev, struct uio *uio, cred_t *cred_p);
static int cmdkwrite(dev_t dev, struct uio *uio, cred_t *cred_p);
static int cmdk_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
	int mod_flags, char *name, caddr_t valuep, int *lengthp);
static int cmdkaread(dev_t dev, struct aio_req *aio, cred_t *cred_p);
static int cmdkawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p);

/*
 * Configuration Data
 */

/*
 * Device driver ops vector
 */

static struct cb_ops cmdk_cb_ops = {
	cmdkopen, 		/* open */
	cmdkclose, 		/* close */
	cmdkstrategy, 		/* strategy */
	nodev, 			/* print */
	cmdkdump, 		/* dump */
	cmdkread, 		/* read */
	cmdkwrite, 		/* write */
	cmdkioctl, 		/* ioctl */
	nodev, 			/* devmap */
	nodev, 			/* mmap */
	nodev, 			/* segmap */
	nochpoll, 		/* poll */
	cmdk_prop_op, 		/* cb_prop_op */
	0, 			/* streamtab  */
	D_64BIT | D_MP | D_NEW,	/* Driver comaptibility flag */
	CB_REV,			/* cb_rev */
	cmdkaread,		/* async read */
	cmdkawrite		/* async write */
};

static int cmdkinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int cmdkidentify(dev_info_t *devi);
static int cmdkprobe(dev_info_t *devi);
static int cmdkattach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int cmdkdetach(dev_info_t *devi, ddi_detach_cmd_t cmd);

struct dev_ops cmdk_ops = {
	DEVO_REV, 		/* devo_rev, */
	0, 			/* refcnt  */
	cmdkinfo,		/* info */
	cmdkidentify, 		/* identify */
	cmdkprobe, 		/* probe */
	cmdkattach, 		/* attach */
	cmdkdetach,		/* detach */
	nodev, 			/* reset */
	&cmdk_cb_ops, 		/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module. This one is a driver */
	"Common Direct Access Disk Driver", 	/* Name of the module. 	*/
	&cmdk_ops, 		/* driver ops 				*/
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

char _depends_on[] =
	"drv/objmgr misc/snlb misc/dadk misc/strategy";

int
_init(void)
{

	int 	rval;

	if (rval = ddi_soft_state_init(&cmdk_state, sizeof (struct cmdk), 7))
		return (rval);

	mutex_init(&cmdk_attach_mutex, NULL, MUTEX_DRIVER, NULL);
	if ((rval = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&cmdk_attach_mutex);
		ddi_soft_state_fini(&cmdk_state);
	}
	return (rval);
}

int
_fini(void)
{

	return (EBUSY);

	/*
	 * This has been commented out until cmdk is a true
	 * unloadable module. Right now x86's are panicking on
	 * a diskless reconfig boot.
	 */

#if 0 	/* bugid 1186679 */
	int	rval;

	rval = mod_remove(&modlinkage);
	if (rval != 0)
		return (rval);

	mutex_destroy(&cmdk_attach_mutex);
	ddi_soft_state_fini(&cmdk_state);

	return (0);
#endif
}

int
_info(struct modinfo *modinfop)
{

	return (mod_info(&modlinkage, modinfop));
}

/*	pseudo BBH functions						*/
/*ARGSUSED*/
static opaque_t
cmdk_bbh_gethandle(struct cmdk *cmdkp, struct buf *bp)
{
	return (NULL);
}

/*ARGSUSED*/
static bbh_cookie_t
cmdk_bbh_htoc(struct cmdk *dkp, opaque_t handle)
{
	return (NULL);
}

/*ARGSUSED*/
static void
cmdk_bbh_freehandle(struct cmdk *dkp, opaque_t handle)
{
}

/*
 * Autoconfiguration Routines
 */

static int
cmdkidentify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "cmdk") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
cmdkprobe(dev_info_t *devi)
{
	int 	instance;
	int	status;
	struct	cmdk	*dkp;

	instance = ddi_get_instance(devi);

	if (ddi_get_soft_state(cmdk_state, instance))
		return (DDI_PROBE_PARTIAL);

	if ((ddi_soft_state_zalloc(cmdk_state, instance) != DDI_SUCCESS) ||
	    ((dkp = ddi_get_soft_state(cmdk_state, instance)) == NULL))
		return (DDI_PROBE_PARTIAL);
	dkp->dk_dip = devi;

	if (cmdk_create_obj(devi, dkp) != DDI_SUCCESS) {
		ddi_soft_state_free(cmdk_state, instance);
		return (DDI_PROBE_PARTIAL);
	}

	status = TGDK_PROBE(CMDK_TGOBJP(dkp), KM_NOSLEEP);
	if (status != DDI_PROBE_SUCCESS) {
		cmdk_destroy_obj(devi, dkp, 0);
		ddi_soft_state_free(cmdk_state, instance);
		return (status);
	}

	sema_init(&dkp->dk_semoclose, 1, NULL, SEMA_DRIVER, NULL);

#ifdef CMDK_DEBUG
	if (cmdk_debug & DENT)
		PRF("cmdkprobe: instance= %d name= `%s`\n",
			instance, ddi_get_name_addr(devi));
#endif
	return (status);
}

static int
cmdkattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int 	instance;
	struct 	driver_minor_data *dmdp;
	struct	cmdk	*dkp;
	char 	*node_type;
	char	name[48];

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	instance = ddi_get_instance(devi);
	if (!(dkp = ddi_get_soft_state(cmdk_state, instance)))
		return (DDI_FAILURE);

	if (TGDK_ATTACH(CMDK_TGOBJP(dkp)) != DDI_SUCCESS) {
		ddi_soft_state_free(cmdk_state, instance);
		return (DDI_FAILURE);
	}

	node_type = TGDK_GETNODETYPE(CMDK_TGOBJP(dkp));
	for (dmdp = cmdk_minor_data; dmdp->name != NULL; dmdp++) {
		(void) sprintf(name, "%s", dmdp->name);
		if (ddi_create_minor_node(devi, name, dmdp->type,
		    (instance << CMDK_UNITSHF)|dmdp->minor, node_type, NULL) ==
			DDI_FAILURE) {

			cmdk_destroy_obj(devi, dkp, 0);

			sema_destroy(&dkp->dk_semoclose);
			ddi_soft_state_free(cmdk_state, instance);

			ddi_remove_minor_node(devi, NULL);
			ddi_prop_remove_all(devi);
			return (DDI_FAILURE);
		}
	}
	mutex_enter(&cmdk_attach_mutex);
	if (instance > cmdk_max_instance)
		cmdk_max_instance = instance;
	mutex_exit(&cmdk_attach_mutex);

	/*
	 * Add a zero-length attribute to tell the world we support
	 * kernel ioctls (for layered drivers)
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    DDI_KERNEL_IOCTL, NULL, 0);
	ddi_report_dev(devi);

	cmdk_part_info_init(dkp);

	return (DDI_SUCCESS);
}


static int
cmdkdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	register struct cmdk	*dkp;
	register int 		instance;
	int			max_instance;

	switch (cmd) {

	case DDI_DETACH:

		mutex_enter(&cmdk_attach_mutex);
		max_instance = cmdk_max_instance;
		mutex_exit(&cmdk_attach_mutex);

		for (instance = 0; instance < max_instance; instance++) {
			dkp = ddi_get_soft_state(cmdk_state, instance);
			if (!dkp)
				continue;
			if (dkp->dk_flag & CMDK_OPEN)
				return (DDI_FAILURE);
		}

		instance = ddi_get_instance(dip);
		if (!(dkp = ddi_get_soft_state(cmdk_state, instance)))
			return (DDI_SUCCESS);
		cmdk_part_info_fini(dkp);
		cmdk_destroy_lbobj(dip, dkp, 1);
		cmdk_destroy_obj(dip, dkp, 1);

		sema_destroy(&dkp->dk_semoclose);
		ddi_soft_state_free(cmdk_state, instance);

		ddi_prop_remove_all(dip);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_SUCCESS);
	default:
#ifdef CMDK_DEBUG
		if (cmdk_debug & DIO) {
			PRF("cmdkdetach: cmd = %d unknown\n", cmd);
		}
#endif
		return (DDI_FAILURE);
	}
}

static int
cmdkinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t		dev = (dev_t)arg;
	int 		instance;
	struct	cmdk	*dkp;
#ifdef lint
	dip = dip;	/* no one ever uses this */
#endif

#ifdef CMDK_DEBUG
	if (cmdk_debug & DENT)
		PRF("cmdkinfo: call\n");
#endif
	instance = CMDKUNIT(dev);

	switch (infocmd) {
		case DDI_INFO_DEVT2DEVINFO:
			if (!(dkp = ddi_get_soft_state(cmdk_state, instance)))
				return (DDI_FAILURE);
			*result = (void *) dkp->dk_dip;
			break;
		case DDI_INFO_DEVT2INSTANCE:
			*result = (void *)instance;
			break;
		default:
			return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
cmdk_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int 		instance;
	int 		km_flags;
	int 		length;
	struct	cmdk	*dkp;
	caddr_t		buffer;
	daddr_t		p_lblksrt;

#ifdef CMDK_DEBUG
	if (cmdk_debug & DENT)
		PRF("cmdk_prop_op: call\n");
#endif

	if (strcmp(name, "nblocks") == 0) {
		instance = CMDKUNIT(dev);
		if (!(dkp = ddi_get_soft_state(cmdk_state, instance)) ||
		    !(dkp->dk_flag & CMDK_VALID_LABEL))
			return (DDI_PROP_NOT_FOUND);


/* 		get callers length set return length.  			*/
		length = *lengthp;		/* Get callers length 	*/
		*lengthp = sizeof (int);	/* Set callers length 	*/

		/*
		 * Allocate buffer, if required.  Either way,
		 * set `buffer' variable.
		 */
		switch (prop_op)  {
		case PROP_LEN:
			/* If length only request, return length value */
			return (DDI_PROP_SUCCESS);

		case PROP_LEN_AND_VAL_ALLOC:

			km_flags = KM_NOSLEEP;

			if (mod_flags & DDI_PROP_CANSLEEP)
				km_flags = KM_SLEEP;

			buffer = (caddr_t)kmem_alloc((size_t)sizeof (int),
				km_flags);
			if (buffer == NULL)  {
				cmn_err(CE_CONT,
					"cmdk%d: no mem for property %s",
					instance, name);
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer; /* Set callers buf ptr */
			break;

		case PROP_LEN_AND_VAL_BUF:

			if (sizeof (int) > (length))
				return (DDI_PROP_BUF_TOO_SMALL);

			buffer = valuep; /* get callers buf ptr */
			break;
		default:
			return (DDI_PROP_INVAL_ARG);
		}

		/*
		 * force re-read of MBR and label and partition info
		 */
		if (!cmdk_part_info(dkp, TRUE, &p_lblksrt, (long *)buffer,
		    CMDKPART(dev)))
			return (DDI_PROP_NOT_FOUND);
		return (DDI_PROP_SUCCESS);
	}

	return (ddi_prop_op(dev, dip, prop_op, mod_flags, name, valuep,
		lengthp));
}

/*
 * dump routine
 */
static int
cmdkdump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk)
{
	int 		instance;
	struct	cmdk	*dkp;
	daddr_t		p_lblksrt;
	long		p_lblkcnt;
	struct	buf	local;
	struct	buf	*bp;

#ifdef CMDK_DEBUG
	if (cmdk_debug & DENT)
		PRF("cmdkdump: call\n");
#endif
	instance = CMDKUNIT(dev);
	if (!(dkp = ddi_get_soft_state(cmdk_state, instance)) || (blkno < 0))
		return (ENXIO);

	DKLB_PARTINFO(dkp->dk_lbobjp, &p_lblksrt, &p_lblkcnt, CMDKPART(dev));

	if ((blkno+nblk) > p_lblkcnt)
		return (EINVAL);

	cmdk_indump = 1;	/* Tell disk targets we are panic dumpping */

	bp = &local;
	bzero((caddr_t)bp, sizeof (*bp));
	bp->b_flags = B_BUSY;
	bp->b_un.b_addr = addr;
	bp->b_bcount = nblk << SCTRSHFT;
	SET_BP_SEC(bp, (p_lblksrt + blkno));

	TGDK_DUMP(CMDK_TGOBJP(dkp), bp);
	return (bp->b_error);
}

/*
 * ioctl routine
 */
static int
cmdkioctl(dev_t dev, int cmd, intptr_t arg, int flag, cred_t *cred_p,
	int *rval_p)
{
	int 		instance;
	struct	scsi_device *devp;
	struct	cmdk	*dkp;
	long 		data[NBPSCTR / (sizeof (long))];

	instance = CMDKUNIT(dev);
	if (!(dkp = ddi_get_soft_state(cmdk_state, instance)))
		return (ENXIO);

	bzero((caddr_t)data, sizeof (data));

	switch (cmd) {
	case DKIOCINFO: {
		struct dk_cinfo *info = (struct dk_cinfo *)data;
/*		controller information					*/
		info->dki_ctype = TGDK_GETCTYPE(CMDK_TGOBJP(dkp));
		info->dki_cnum = ddi_get_instance(ddi_get_parent(dkp->dk_dip));
		(void) strcpy(info->dki_cname,
			ddi_get_name(ddi_get_parent(dkp->dk_dip)));

/* 		Unit Information 					*/
		info->dki_unit = ddi_get_instance(dkp->dk_dip);
		devp = (struct scsi_device *)ddi_get_driver_private
			(dkp->dk_dip);
		info->dki_slave = (CMDEV_TARG(devp)<<3) | CMDEV_LUN(devp);
		(void) strcpy(info->dki_dname, ddi_get_name(dkp->dk_dip));
		info->dki_flags = DKI_FMTVOL;
		info->dki_partition = CMDKPART(dev);

		info->dki_maxtransfer = maxphys / DEV_BSIZE;
		info->dki_addr = 1;
		info->dki_space = 0;
		info->dki_prio = 0;
		info->dki_vec = 0;

		if (ddi_copyout((caddr_t)data, (caddr_t)arg, sizeof (*info),
			flag))
			return (EFAULT);
		else
			return (0);
	}

	case DKIOCPARTINFO: {
		struct part_info	p;

		/*
		 * force re-read of MBR and label and partition info
		 */
		if (!cmdk_part_info(dkp, TRUE, &p.p_start, (long *)&p.p_length,
		    CMDKPART(dev)))
			return (ENXIO);

		if (ddi_copyout((caddr_t)&p, (caddr_t)arg, sizeof (p), flag))
			return (EFAULT);
		return (0);
	}

	case DKIOCSTATE: {
		enum dkio_state state;
		int		rval;
		int 		part;
		daddr_t		p_lblksrt;
		long		p_lblkcnt;

		if (ddi_copyin((caddr_t)arg, (caddr_t)&state, sizeof (int),
		    flag))
			return (EFAULT);

		if (rval = TGDK_CHECK_MEDIA(CMDK_TGOBJP(dkp), &state))
			return (rval);

		if (state == DKIO_INSERTED) {
			part = CMDKPART(dev);
			/*
			 * force re-read of MBR and label and partition info
			 */
			if (!cmdk_part_info(dkp, TRUE, &p_lblksrt, &p_lblkcnt,
			    part))
				return (ENXIO);

			if ((part < 0) || (p_lblkcnt <= 0)) {
				return (ENXIO);
			}
		}

		if (ddi_copyout((caddr_t)&state, (caddr_t)arg,
		    sizeof (int), flag)) {
				return (EFAULT);
		}
		return (0);
	}

	/*
	 * is media removable?
	 */
	case DKIOCREMOVABLE: {
		int	i;

		if (TGDK_RMB(CMDK_TGOBJP(dkp))) {
			i = 1;
		} else {
			i = 0;
		}
		if (ddi_copyout((caddr_t)&i, (caddr_t)arg,
		    sizeof (int), flag)) {
			return (EFAULT);
		}
		return (0);
	}

	case DKIOCG_VIRTGEOM:
	case DKIOCG_PHYGEOM:
	case DKIOCGGEOM:
	case DKIOCSGEOM:
	case DKIOCSVTOC:
	case DKIOCGVTOC:
	case DKIOCGAPART:
	case DKIOCSAPART:
	case DKIOCADDBAD:

		/* If we don't have a label obj we can't call its ioctl */
		if (!dkp->dk_lbobjp)
			return (EIO);

		return (DKLB_IOCTL(dkp->dk_lbobjp, cmd, arg, flag,
			cred_p, rval_p));
#ifdef DIOCTL_RWCMD
	case DIOCTL_RWCMD: {
		register struct dadkio_rwcmd *rwcmdp;
		int		status;

		rwcmdp = (struct dadkio_rwcmd *)kmem_alloc((size_t)
			sizeof (struct dadkio_rwcmd), KM_SLEEP);

		if (ddi_copyin((caddr_t)arg, (caddr_t)rwcmdp,
			sizeof (struct dadkio_rwcmd), flag)) {
			kmem_free((caddr_t)rwcmdp,
				sizeof (struct dadkio_rwcmd));
				return (EFAULT);

		}
		bzero((caddr_t)(&(rwcmdp->status)), sizeof (rwcmdp->status));
		status = TGDK_IOCTL(CMDK_TGOBJP(dkp), dev, cmd, rwcmdp,
			flag, cred_p, rval_p);
		if (status == 0) {
			if (ddi_copyout((caddr_t)rwcmdp, (caddr_t)arg,
			    sizeof (*rwcmdp), flag))
				status = EFAULT;
		}
		kmem_free((caddr_t)rwcmdp, (size_t)
			sizeof (struct dadkio_rwcmd));
		return (status);

	}
#endif
	default:
		return (TGDK_IOCTL(CMDK_TGOBJP(dkp), dev, cmd, arg, flag,
			cred_p, rval_p));
	}
}

/*ARGSUSED1*/
static int
cmdkclose(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	int 		part;
	ulong		partbit;
	int 		instance;
	register struct	cmdk	*dkp;
	int		i;

	instance = CMDKUNIT(dev);
	if (!(dkp = ddi_get_soft_state(cmdk_state, instance)) ||
	    (otyp >= OTYPCNT))
		return (ENXIO);

	sema_p(&dkp->dk_semoclose);

/*	check for device has been opened				*/
	if (!(dkp->dk_flag & CMDK_OPEN)) {
		sema_v(&dkp->dk_semoclose);
		return (ENXIO);
	}

	part = CMDKPART(dev);
	if (part < 0) {
		sema_v(&dkp->dk_semoclose);
		return (ENXIO);
	}
	partbit = 1 << part;

	if (otyp == OTYP_LYR) {
		if (dkp->dk_lyr[part] == 0) {
			sema_v(&dkp->dk_semoclose);
			return (ENXIO);
		}
		dkp->dk_lyr[part]--;
	}
	if ((dkp->dk_lyr[part] == 0) ||
	    (otyp != OTYP_LYR)) {
		dkp->dk_open.dk_exl[otyp] &= ~partbit;
		dkp->dk_open.dk_reg[otyp] &= ~partbit;
	}

	for (i = 0; i < OTYPCNT; i++) {
		if (dkp->dk_open.dk_reg[i])
			break;
	}

/*	check for last close						*/
	if (i >= OTYPCNT) {
		TGDK_CLOSE(CMDK_TGOBJP(dkp));
		(void) kmem_free(dkp->dk_lyr, (sizeof (ulong) * CMDK_MAXPART));
		dkp->dk_flag = 0;
	}
	sema_v(&dkp->dk_semoclose);
	return (DDI_SUCCESS);
}

/*ARGSUSED3*/
static int
cmdkopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	dev_t		dev = *dev_p;
	int 		part;
	ulong		partbit;
	int 		instance;
	register struct	cmdk	*dkp;
	daddr_t		p_lblksrt;
	long		p_lblkcnt;

	instance = CMDKUNIT(dev);
	if (!(dkp = ddi_get_soft_state(cmdk_state, instance)))
		return (ENXIO);

	if (otyp >= OTYPCNT)
		return (EINVAL);

	if ((part = CMDKPART(dev)) < 0)
		return (ENXIO);

	/* for property create inside DKLB_*() */
	dkp->dk_dev = dev & ~(CMDK_MAXPART - 1);

	sema_p(&dkp->dk_semoclose);

	/* re-do the target open */
	if (cmdk_part_info(dkp, TRUE, &p_lblksrt, &p_lblkcnt, part)) {
		if ((p_lblkcnt <= 0) &&
			((flag & (FNDELAY|FNONBLOCK)) == 0 ||
			otyp != OTYP_CHR)) {
			sema_v(&dkp->dk_semoclose);
			return (ENXIO);
		}
	} else {
/*		fail if not doing non block open 			*/
		if ((flag & (FNONBLOCK|FNDELAY)) == 0) {
			sema_v(&dkp->dk_semoclose);
			return (ENXIO);
		}

	}
	if (TGDK_RDONLY(CMDK_TGOBJP(dkp)) && (flag & FWRITE)) {
		sema_v(&dkp->dk_semoclose);
		return (EROFS);
	}

/* ************************************************************************* */

	/*
	 * This exclusive open stuff is probably wrong and needs
	 * to be fixed. The DDI doesn't specify FEXCL so I've no idea
	 * what's the right way to fix this.
	 */
	partbit = 1 << part;

	if ((dkp->dk_open.dk_exl[otyp] & partbit) ||
	    ((flag & FEXCL) && (dkp->dk_open.dk_reg[otyp] & partbit))) {
		sema_v(&dkp->dk_semoclose);
		/* 1203574: changed from ENXIO to EBUSY to match sd */
		return (EBUSY);
	}

	if (!(dkp->dk_flag & CMDK_OPEN)) {
		dkp->dk_flag |= CMDK_OPEN;
		dkp->dk_lyr = (ulong *)kmem_zalloc(sizeof (ulong) *
			CMDK_MAXPART, KM_SLEEP);
	}

	dkp->dk_open.dk_reg[otyp] |= partbit;
	if (otyp == OTYP_LYR)
		dkp->dk_lyr[part]++;
	if (flag & FEXCL)
		dkp->dk_open.dk_exl[otyp] |= partbit;
/* ************************************************************************* */


	sema_v(&dkp->dk_semoclose);
	return (DDI_SUCCESS);
}

static int
cmdk_reopen(register struct cmdk *dkp)
{
	register struct cmdk_label *dklbp;
	int	 i;

/*	open the target disk						*/
	if (TGDK_OPEN(CMDK_TGOBJP(dkp), 0) != DDI_SUCCESS)
		return (FALSE);

/*	check for valid label object					*/
	if (!dkp->dk_lbobjp) {
		if (cmdk_create_lbobj(dkp->dk_dip, dkp) != DDI_SUCCESS)
			return (FALSE);
	} else {
/*		reset back to pseudo bbh				*/
		TGDK_SET_BBHOBJ(CMDK_TGOBJP(dkp), &cmdk_bbh_obj);
	}
	dkp->dk_flag |= CMDK_VALID_LABEL;

/*	search for proper disk label object				*/
	for (i = 0, dklbp = dkp->dk_lb; i < CMDK_LABEL_MAX; i++, dklbp++) {
		if (!dklbp->dkl_objp)
			continue;
		dkp->dk_lbobjp = dklbp->dkl_objp;
		if (DKLB_OPEN(dkp->dk_lbobjp, dkp->dk_dev, dkp->dk_dip)
		    == DDI_SUCCESS)
			return (TRUE);
	}

/*	the last opened label object will become the installed label	*/
/*	this allows whole raw disk access for disk preparations		*/
	return (TRUE);
}

/*
 * read routine
 */
/*ARGSUSED2*/
static int
cmdkread(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	return (cmdkrw(dev, uio, B_READ));
}

/*
 * async read routine
 */
/*ARGSUSED2*/
static int
cmdkaread(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	return (cmdkarw(dev, aio, B_READ));
}

/*
 * write routine
 */
/*ARGSUSED2*/
static int
cmdkwrite(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	return (cmdkrw(dev, uio, B_WRITE));
}

/*
 * async write routine
 */
/*ARGSUSED2*/
static int
cmdkawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	return (cmdkarw(dev, aio, B_WRITE));
}

static void
cmdkmin(register struct buf *bp)
{
	if (bp->b_bcount > DK_MAXRECSIZE)
		bp->b_bcount = DK_MAXRECSIZE;
}

static int
cmdkrw(dev_t dev, struct uio *uio, int flag)
{
	return (physio(cmdkstrategy, (struct buf *)0, dev, flag, cmdkmin, uio));
}

static int
cmdkarw(dev_t dev, struct aio_req *aio, int flag)
{
	return (aphysio(cmdkstrategy, anocancel, dev, flag, cmdkmin, aio));
}

/*
 * strategy routine
 */
static int
cmdkstrategy(register struct buf *bp)
{
	int 		instance;
	struct	cmdk 	*dkp;
	long		d_cnt;
	daddr_t		p_lblksrt;
	long		p_lblkcnt;

	instance = CMDKUNIT(bp->b_edev);
	if (cmdk_indump || !(dkp = ddi_get_soft_state(cmdk_state, instance)) ||
	    (dkblock(bp) < 0)) {
		bp->b_resid = bp->b_bcount;
		SETBPERR(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	bp->b_flags &= ~(B_DONE|B_ERROR);
	bp->b_resid = 0;
	bp->av_back = NULL;

	/*
	 * only re-read the vtoc if necessary (force == FALSE)
	 */
	if (!cmdk_part_info(dkp, FALSE, &p_lblksrt, &p_lblkcnt,
	    CMDKPART(bp->b_edev))) {
		SETBPERR(bp, ENXIO);
	}

	if ((bp->b_bcount & (NBPSCTR-1))||(dkblock(bp) > p_lblkcnt))
		SETBPERR(bp, ENXIO);

	if ((bp->b_flags & B_ERROR) || (dkblock(bp) == p_lblkcnt)) {
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return (0);
	}

	d_cnt = bp->b_bcount >> SCTRSHFT;
	if ((dkblock(bp) + d_cnt) > p_lblkcnt) {
		bp->b_resid = ((dkblock(bp) + d_cnt) - p_lblkcnt) << SCTRSHFT;
		bp->b_bcount -= bp->b_resid;
	}

	SET_BP_SEC(bp, (p_lblksrt + dkblock(bp)));
	if (TGDK_STRATEGY(CMDK_TGOBJP(dkp), bp) != DDI_SUCCESS) {
		bp->b_resid += bp->b_bcount;
		biodone(bp);
	}
	return (0);
}

static int
cmdk_create_obj(dev_info_t *devi, struct cmdk *dkp)
{
	struct scsi_device *devp;
	opaque_t	queobjp = NULL;
	opaque_t	flcobjp = NULL;
	char		que_keyvalp[OBJNAMELEN];
	int		que_keylen;
	char		flc_keyvalp[OBJNAMELEN];
	int		flc_keylen;
	char		dsk_keyvalp[OBJNAMELEN];
	int		dsk_keylen;
	major_t 	objmgr_maj;

	if (((objmgr_maj = ddi_name_to_major("objmgr")) == -1) ||
	    (ddi_hold_installed_driver(objmgr_maj) == NULL))
		return (DDI_FAILURE);

	que_keylen = sizeof (que_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "queue",
		(caddr_t)que_keyvalp, &que_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "cmdk_create_obj: queue property undefined");
		return (DDI_FAILURE);
	}
	que_keyvalp[que_keylen] = (char)0;

	flc_keylen = sizeof (flc_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "flow_control",
		(caddr_t)flc_keyvalp, &flc_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
			"cmdk_create_obj: flow-control property undefined");
		return (DDI_FAILURE);
	}
	flc_keyvalp[flc_keylen] = (char)0;

	dsk_keylen = sizeof (dsk_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "disk",
		(caddr_t)dsk_keyvalp, &dsk_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
			"cmdk_create_obj: target disk property undefined");
		return (DDI_FAILURE);
	}
	dsk_keyvalp[dsk_keylen] = (char)0;

	if ((objmgr_load_obj(que_keyvalp) != DDI_SUCCESS) ||
	    !(queobjp = objmgr_create_obj(que_keyvalp))) {
		cmn_err(CE_WARN,
			"cmdk_create_obj: ERROR creating queue method %s\n",
			que_keyvalp);
		return (DDI_FAILURE);
	}

	if ((objmgr_load_obj(flc_keyvalp) != DDI_SUCCESS) ||
	    !(flcobjp = objmgr_create_obj(flc_keyvalp))) {
		QUE_FREE(queobjp);
		(void) objmgr_destroy_obj(que_keyvalp);
		cmn_err(CE_WARN,
			"cmdk_create_obj: ERROR creating flow control %s\n",
			flc_keyvalp);
		return (DDI_FAILURE);
	}

	if ((objmgr_load_obj(dsk_keyvalp) != DDI_SUCCESS) ||
	    !(dkp->dk_tgobjp = objmgr_create_obj(dsk_keyvalp))) {
		QUE_FREE(queobjp);
		(void) objmgr_destroy_obj(que_keyvalp);
		FLC_FREE(flcobjp);
		(void) objmgr_destroy_obj(flc_keyvalp);
		cmn_err(CE_WARN,
			"cmdk_create_obj: ERROR creating target disk %s\n",
			dsk_keyvalp);
		return (DDI_FAILURE);
	}

	devp = (struct scsi_device *)ddi_get_driver_private(devi);

	TGDK_INIT(CMDK_TGOBJP(dkp), devp, flcobjp,
			queobjp, &cmdk_bbh_obj, NULL);

	return (DDI_SUCCESS);
}

static void
cmdk_destroy_obj(dev_info_t *devi, struct cmdk *dkp, int unload)
{
	char		que_keyvalp[OBJNAMELEN];
	int		que_keylen;
	char		flc_keyvalp[OBJNAMELEN];
	int		flc_keylen;
	char		dsk_keyvalp[OBJNAMELEN];
	int		dsk_keylen;

	TGDK_FREE(CMDK_TGOBJP(dkp));
	CMDK_TGOBJP(dkp) = NULL;

	que_keylen = sizeof (que_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "queue",
		(caddr_t)que_keyvalp, &que_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "cmdk_destroy_obj: queue property undefined");
		return;
	}
	que_keyvalp[que_keylen] = (char)0;

	flc_keylen = sizeof (flc_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "flow_control",
		(caddr_t)flc_keyvalp, &flc_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
			"cmdk_destroy_obj: flow-control property undefined");
		return;
	}
	flc_keyvalp[flc_keylen] = (char)0;

	dsk_keylen = sizeof (dsk_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "disk",
		(caddr_t)dsk_keyvalp, &dsk_keylen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
			"cmdk_destroy_obj: target disk property undefined");
		return;
	}
	dsk_keyvalp[dsk_keylen] = (char)0;

	(void) objmgr_destroy_obj(que_keyvalp);
	(void) objmgr_destroy_obj(flc_keyvalp);
	(void) objmgr_destroy_obj(dsk_keyvalp);

	if (unload) {
		objmgr_unload_obj(que_keyvalp);
		objmgr_unload_obj(flc_keyvalp);
		objmgr_unload_obj(dsk_keyvalp);
	}
}

static int
cmdk_create_lbobj(dev_info_t *devi, struct cmdk *dkp)
{
	register struct cmdk_label *dklbp;
	char		alb_keyvalp[OBJNAMELEN];
	int		alb_keylen;
	register int	i;
	int		j;
	int		mystatus;

	dklbp = dkp->dk_lb;
	alb_keylen = sizeof (alb_keyvalp);
	if (ddi_prop_op(DDI_DEV_T_NONE, devi, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP, "disklabel",
		(caddr_t)alb_keyvalp, &alb_keylen) != DDI_PROP_SUCCESS) {
		(void) strcpy(dklbp->dkl_name, "snlb");
	} else {
		alb_keyvalp[alb_keylen] = (char)0;
		alb_keylen++;
		for (i = 0, j = 0; i < alb_keylen; i++) {
			if ((dklbp->dkl_name[j] = alb_keyvalp[i]) == (char)0) {
				dklbp++;
				j = 0;
			} else {
				j++;
			}
		}
	}

	dklbp = dkp->dk_lb;
	for (i = 0, mystatus = DDI_FAILURE; i < CMDK_LABEL_MAX; i++, dklbp++) {
		if (dklbp->dkl_name[0] == (char)0)
			break;
		if ((objmgr_load_obj(dklbp->dkl_name) != DDI_SUCCESS) ||
		    !(dklbp->dkl_objp = objmgr_create_obj(dklbp->dkl_name))) {
			cmn_err(CE_WARN,
			"cmdk_create_lbobj: ERROR creating disklabel %s\n",
				dklbp->dkl_name);
			dklbp->dkl_name[0] = (char)0;
		} else {
			DKLB_INIT(dklbp->dkl_objp, CMDK_TGOBJP(dkp), NULL);
			mystatus = DDI_SUCCESS;
		}
	}
	return (mystatus);
}

static void
cmdk_destroy_lbobj(dev_info_t *devi, struct cmdk *dkp, int unload)
{
	register struct cmdk_label *dklbp;
	register int		i;

#ifdef lint
	devi = devi;
#endif

	if (!dkp->dk_lbobjp)
		return;

	dklbp = dkp->dk_lb;
	for (i = 0; i < CMDK_LABEL_MAX; i++, dklbp++) {
		if (dklbp->dkl_name[0] == (char)0)
			continue;
		DKLB_FREE(dklbp->dkl_objp);
		(void) objmgr_destroy_obj(dklbp->dkl_name);
		if (unload)
			objmgr_unload_obj(dklbp->dkl_name);
		dklbp->dkl_name[0] = (char)0;
	}

	dkp->dk_lbobjp = 0;
}


/*
 * cmdk_part_info()
 *
 *	Make the device valid if possible. The dk_pinfo_lock is only
 *	held for very short periods so that there's very little
 *	contention with the cmdk_devstatus() function which can
 *	be called from interrupt context.
 *
 *	This function implements a simple state machine which looks
 *	like this:
 *
 *
 *	   +---------------------------------+
 *         |				     |
 *	   +--> invalid --> busy --> valid --+
 *			     ^|
 *			     |v
 *			    busy2
 *
 *	This function can change the state from invalid to busy, or from
 *	busy2 to busy, or from busy to valid.
 *
 *	The cmdk_devstatus() function can change the state from valid
 *	to invalid or from busy to busy2.
 *
 */


static int
cmdk_part_info(struct cmdk *dkp, int force, daddr_t *startp, long *countp,
		int part)
{

	/*
	 * The dk_pinfo_state variable (and by implication the partition
	 * info) is always protected by the dk_pinfo_lock mutex.
	 */
	mutex_enter(&dkp->dk_pinfo_lock);

	for (;;) {
		switch (dkp->dk_pinfo_state) {

		case CMDK_PARTINFO_VALID:
			/* it's already valid */
			if (!force) {
				goto done;
			}
		/*FALLTHROUGH*/

		case CMDK_PARTINFO_INVALID:
			/*
			 * It's invalid or we're being forced to reread
			 */
			goto reopen;

		case CMDK_PARTINFO_BUSY:
		case CMDK_PARTINFO_BUSY2:
			/*
			 * Some other thread has already called
			 * cmdk_reopen(), wait for it to complete and then
			 * start over from the top.
			 */
			cv_wait(&dkp->dk_pinfo_cv, &dkp->dk_pinfo_lock);
		}
	}

reopen:
	/*
	 * ASSERT: only one thread at a time can possibly reach this point
	 * and invoke cmdk_reopen()
	 */
	dkp->dk_pinfo_state = CMDK_PARTINFO_BUSY;

	for (;;)  {
		int	rc;

		/*
		 * drop the mutex while in cmdk_reopen() because
		 * it may take a long time to return
		 */
		mutex_exit(&dkp->dk_pinfo_lock);
		rc = cmdk_reopen(dkp);
		mutex_enter(&dkp->dk_pinfo_lock);

		if (rc == FALSE) {
			/*
			 * bailout, probably due to no device,
			 * or invalid label
			 */
			goto error;
		}

		switch (dkp->dk_pinfo_state) {

		case CMDK_PARTINFO_BUSY:
			dkp->dk_pinfo_state = CMDK_PARTINFO_VALID;
			cv_broadcast(&dkp->dk_pinfo_cv);
			goto done;

		case CMDK_PARTINFO_BUSY2:
			/*
			 * device status changed by cmdk_devstatus(),
			 * redo the reopen
			 */
			dkp->dk_pinfo_state = CMDK_PARTINFO_BUSY;
		}
	}


done:
	/*
	 * finished cmdk_reopen() without any device status change
	 */
	DKLB_PARTINFO(dkp->dk_lbobjp, startp, countp, part);
	mutex_exit(&dkp->dk_pinfo_lock);
	return (TRUE);

error:
	dkp->dk_pinfo_state = CMDK_PARTINFO_INVALID;
	cv_broadcast(&dkp->dk_pinfo_cv);
	mutex_exit(&dkp->dk_pinfo_lock);
	return (FALSE);
}

#ifdef	NOT_USED
static void
cmdk_devstatus(struct cmdk *dkp)
{

	mutex_enter(&dkp->dk_pinfo_lock);
	switch (dkp->dk_pinfo_state) {

	case CMDK_PARTINFO_VALID:
		dkp->dk_pinfo_state = CMDK_PARTINFO_INVALID;
		break;

	case CMDK_PARTINFO_INVALID:
		break;

	case CMDK_PARTINFO_BUSY:
		dkp->dk_pinfo_state = CMDK_PARTINFO_BUSY2;
		break;

	case CMDK_PARTINFO_BUSY2:
		break;
	}
	mutex_exit(&dkp->dk_pinfo_lock);
}
#endif	NOT_USED


/*
 * initialize the state for cmdk_part_info()
 */
static void
cmdk_part_info_init(struct cmdk *dkp)
{
	mutex_init(&dkp->dk_pinfo_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&dkp->dk_pinfo_cv, NULL, CV_DRIVER, NULL);
	dkp->dk_pinfo_state = CMDK_PARTINFO_INVALID;
}

static void
cmdk_part_info_fini(struct cmdk *dkp)
{
	mutex_destroy(&dkp->dk_pinfo_lock);
	cv_destroy(&dkp->dk_pinfo_cv);
}
