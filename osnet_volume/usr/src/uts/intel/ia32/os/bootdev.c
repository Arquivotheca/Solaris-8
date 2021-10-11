/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootdev.c	1.8	99/05/31 SMI"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pathname.h>
#include <sys/autoconf.h>

/*
 * internal functions
 */
static int validate_dip(dev_info_t *dip);
static dev_info_t *path_to_dip(char *prom_path, dev_t *devp);

/* internal global data */
static struct modlmisc modlmisc = {
	&mod_miscops, "bootdev misc module 1.8"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	/*
	 * misc modules are not safely unloadable: 1170668
	 */
	return (EBUSY);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Return the corresponding device info node for a path.
 * The corresponding dev_t is returned in devp.
 * If successful, we return a 'held' dip.
 */
static dev_info_t *
path_to_dip(char *prom_path, dev_t *devp)
{
	dev_t dev;
	major_t maj;
	dev_info_t *dip;
	int ret;
	struct devnames *dnp;
	int circular;


	/*
	 * get the device number
	 */
	dev = ddi_pathname_to_dev_t(prom_path);
	if (dev == (dev_t)-1) {
		return (NULL);
	}

	/*
	 * hold the driver
	 */
	maj = getmajor(dev);
	if (ddi_hold_installed_driver(maj) == NULL) {
		return (NULL);
	}
	(void) e_ddi_deferred_attach(maj, NODEV);

	dip = e_ddi_get_dev_info(dev, VCHR);
	if (dip == NULL) {
		return (NULL);
	}
	ddi_rele_driver(maj);	/* for e_ddi_get_dev_info */

	dnp = &devnamesp[maj];
	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_enter_driver_list(dnp, &circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	if (validate_dip(dip)) {
		ret = 0;
	} else {
		ret = -1;
	}

	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_exit_driver_list(dnp, circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	if (ret == 0) {
		*devp = dev;
		return (dip);	/* return the driver held */
	} else {
		ddi_rele_driver(maj);
		return (NULL);
	}
}

/*
 * convert a prom device path to an equivalent path in /devices
 */
int
i_promname_to_devname(char *prom_name, char *ret_buf)
{
	char *minor;
	char *devpath;
	int ret;
	dev_t dev;
	dev_info_t *dip;

	if (prom_name == NULL) {
		return (EINVAL);
	}
	if (ret_buf == NULL) {
		return (EINVAL);
	}
	if (strlen(prom_name) >= MAXPATHLEN) {
		return (EINVAL);
	}

	if (*prom_name != '/') {
		return (EINVAL);
	}

	minor = strchr(prom_name, ':');
	if ((minor != NULL) && (strchr(minor, '/'))) {
		return (EINVAL);
	}

	dip = path_to_dip(prom_name, &dev);
	if (dip == NULL) {
		return (EINVAL);
	}
	devpath = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (ddi_pathname(dip, devpath) == NULL) {
		ret = EINVAL;
	} else {
		if (minor != NULL) {
			/*
			 * restore the minor name
			 */
			(void) sprintf(ret_buf, "%s%s", devpath, minor);
		} else {
			(void) sprintf(ret_buf, "%s", devpath);
		}
		ret = 0;
	}
	ddi_rele_driver(getmajor(dev));	/* path_to_dip returns dip held */
	kmem_free(devpath, MAXPATHLEN);
	return (ret);
}

/*
 * valid means:
 *	- a driver can be loaded and attached to the instance of the
 * 		device this dip represents.
 * This routine assumes the driver for dip is held and the devinfo
 * node is locked.
 */
static int
validate_dip(dev_info_t *dip)
{
	if (DDI_CF2(dip)) {
		return (1);
	} else {
		return (0);
	}
}
