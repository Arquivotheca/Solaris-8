/*
 * Copyright (c) 1989-1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)openprom.c	1.3	99/06/07 SMI"	/* SVr4 */

/*
 * Ported from 4.1.1_PSRA: "@(#)openprom.c 1.19 91/02/19 SMI";
 *
 * Porting notes:
 *
 * OPROMU2P unsupported after SunOS 4.x.
 *
 * Only one of these devices per system is allowed.
 */

/*
 * Openprom eeprom options/devinfo driver.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/openpromio.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/debug.h>
#include <sys/autoconf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/sysmacros.h>	/* offsetof */

#define	MAX_OPENS	32	/* Up to this many simultaneous opens */

/*
 * XXX	Make this dynamic.. or (better still) make the interface stateless
 */
static struct oprom_state {
	dnode_t	current_id;	/* node we're fetching props from */
	int	already_open;	/* if true, this instance is 'active' */
} oprom_state[MAX_OPENS];

static kmutex_t oprom_lock;	/* serialize instance assignment */

static int opromopen(dev_t *, int, int, cred_t *);
static int opromioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int opromclose(dev_t, int, int, cred_t *);

static int opinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int opidentify(dev_info_t *);
static int opattach(dev_info_t *, ddi_attach_cmd_t cmd);
static int opdetach(dev_info_t *, ddi_detach_cmd_t cmd);

/* help functions */
static int oprom_checknodeid(dnode_t, dnode_t);
static int oprom_copyinstr(intptr_t, char *, size_t, size_t);

static struct cb_ops openeepr_cb_ops = {
	opromopen,		/* open */
	opromclose,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	opromioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops openeepr_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	opinfo,			/* info */
	opidentify,		/* identify */
	nulldev,		/* probe */
	opattach,		/* attach */
	opdetach,		/* detach */
	nodev,			/* reset */
	&openeepr_cb_ops,	/* driver operations */
	NULL			/* bus operations */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,
	"OPENPROM/NVRAM Driver v1.3",
	&openeepr_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int	error;

	mutex_init(&oprom_lock, NULL, MUTEX_DRIVER, NULL);

	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&oprom_lock);
		return (error);
	}

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);

	mutex_destroy(&oprom_lock);
	return (0);
}

static dev_info_t *opdip;
static dnode_t options_nodeid;

/*ARGSUSED*/
static int
opinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)opdip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		/* All dev_t's map to the same, single instance */
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (error);
}

static int
opidentify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "openeepr") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}

static int
opattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	switch (cmd) {

	case DDI_ATTACH:
		if (prom_is_openprom()) {
			options_nodeid = prom_optionsnode();
		} else {
			options_nodeid = OBP_BADNODE;
		}

		opdip = dip;

		if (ddi_create_minor_node(dip, "openprom", S_IFCHR,
		    0, NULL, NULL) == DDI_FAILURE) {
			return (DDI_FAILURE);
		}

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
opdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_remove_minor_node(dip, NULL);
	opdip = NULL;

	return (DDI_SUCCESS);
}

/*
 * Allow multiple opens by tweaking the dev_t such that it looks like each
 * open is getting a different minor device.  Each minor gets a separate
 * entry in the oprom_state[] table.
 */
/*ARGSUSED*/
static int
opromopen(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	int m;
	struct oprom_state *st = oprom_state;

	if (getminor(*devp) != 0)
		return (ENXIO);

	mutex_enter(&oprom_lock);
	for (m = 0; m < MAX_OPENS; m++)
		if (st->already_open)
			st++;
		else {
			st->already_open = 1;
			/*
			 * It's ours.
			 */
			st->current_id = (dnode_t)0;
			break;
		}
	mutex_exit(&oprom_lock);

	if (m == MAX_OPENS)  {
		/*
		 * "Thank you for calling, but all our lines are
		 * busy at the moment.."
		 *
		 * We could get sophisticated here, and go into a
		 * sleep-retry loop .. but hey, I just can't see
		 * that many processes sitting in this driver.
		 *
		 * (And if it does become possible, then we should
		 * change the interface so that the 'state' is held
		 * external to the driver)
		 */
		return (EAGAIN);
	}

	*devp = makedevice(getmajor(*devp), (minor_t)m);

	return (0);
}

/*ARGSUSED*/
static int
opromclose(dev_t dev, int flag, int otype, cred_t *cred_p)
{
	struct oprom_state *st;

	st = &oprom_state[getminor(dev)];
	ASSERT(getminor(dev) < MAX_OPENS && st->already_open != 0);
	st->already_open = 0;

	return (0);
}

/*ARGSUSED*/
static int
opromioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp)
{
	struct oprom_state *st;
	struct openpromio *opp;
	int valsize;
	char *valbuf;
	int error = 0;
	uint_t userbufsize;
	dnode_t node_id;
	char propname[OBP_MAXPROPNAME];

	if (getminor(dev) >= MAX_OPENS)
		return (ENXIO);

	st = &oprom_state[getminor(dev)];
	ASSERT(st->already_open);

	/*
	 * Check permissions
	 * and weed out unsupported commands on x86 platform
	 */
	switch (cmd) {
	case OPROMGETOPT:
	case OPROMNXTOPT:
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		node_id = options_nodeid;
		break;

	case OPROMSETOPT:
	case OPROMSETOPT2:
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
		if (mode & FWRITE) {
			node_id = options_nodeid;
			break;
		}
#endif /* !__i386 && !__ia64 */
		return (EPERM);

	case OPROMNEXT:
	case OPROMCHILD:
	case OPROMGETPROP:
	case OPROMGETPROPLEN:
	case OPROMNXTPROP:
	case OPROMSETNODEID:
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		node_id = st->current_id;
		break;

	case OPROMGETCONS:
	case OPROMGETBOOTARGS:
	case OPROMGETVERSION:
	case OPROMPATH2DRV:
	case OPROMPROM2DEVNAME:
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
	case OPROMGETFBNAME:
	case OPROMDEV2PROMNAME:
	case OPROMREADY64:
#endif	/* !__i386 && !__ia64 */
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		break;

	default:
		return (EINVAL);
	}

	/*
	 * Copy in user argument length and allocation memory
	 *
	 * NB do not copyin the entire buffer we may not need
	 *	to. userbufsize can be as big as 32 K.
	 */
	if (copyin((void *)arg, &userbufsize, sizeof (uint_t)) != 0)
		return (EFAULT);

	if (userbufsize == 0 || userbufsize > OPROMMAXPARAM)
		return (EINVAL);

	opp = (struct openpromio *)kmem_zalloc(
	    userbufsize + sizeof (uint_t) + 1, KM_SLEEP);

	/*
	 * Execute command
	 */
	switch (cmd) {

	case OPROMGETOPT:
	case OPROMGETPROP:
	case OPROMGETPROPLEN:

		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE)) {
			error = EINVAL;
			break;
		}

		/*
		 * The argument, a NULL terminated string, is a prop name.
		 */
		if ((error = oprom_copyinstr(arg, opp->oprom_array,
		    (size_t)userbufsize, OBP_MAXPROPNAME)) != 0) {
			break;
		}
		(void) strcpy(propname, opp->oprom_array);
		valsize = prom_getproplen(node_id, propname);

		/*
		 * 4010173: 'name' is a property, but not an option.
		 */
		if ((cmd == OPROMGETOPT) && (strcmp("name", propname) == 0))
			valsize = -1;

		if (cmd == OPROMGETPROPLEN)  {
			int proplen = valsize;

			if (userbufsize < sizeof (int)) {
				error = EINVAL;
				break;
			}
			opp->oprom_size = valsize = sizeof (int);
			bcopy(&proplen, opp->oprom_array, valsize);
		} else if (valsize > 0 && valsize <= userbufsize) {
			bzero(opp->oprom_array, valsize + 1);
			(void) prom_getprop(node_id, propname,
			    opp->oprom_array);
			opp->oprom_size = valsize;
			if (valsize < userbufsize)
				++valsize;	/* Forces NULL termination */
						/* If space permits */
		} else {
			/*
			 * XXX: There is no error code if the buf is too small.
			 * which is consistent with the current behavior.
			 *
			 * NB: This clause also handles the non-error
			 * zero length (boolean) property value case.
			 */
			opp->oprom_size = 0;
			(void) strcpy(opp->oprom_array, "");
			valsize = 1;
		}
		if (copyout(opp, (void *)arg, (valsize + sizeof (uint_t))) != 0)
			error = EFAULT;
		break;

	case OPROMNXTOPT:
	case OPROMNXTPROP:
		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE)) {
			error = EINVAL;
			break;
		}

		/*
		 * The argument, a NULL terminated string, is a prop name.
		 */
		if ((error = oprom_copyinstr(arg, opp->oprom_array,
		    (size_t)userbufsize, OBP_MAXPROPNAME)) != 0) {
			break;
		}
		valbuf = (char *)prom_nextprop(node_id, opp->oprom_array,
		    propname);
		valsize = strlen(valbuf);

		/*
		 * 4010173: 'name' is a property, but it's not an option.
		 */
		if ((cmd == OPROMNXTOPT) && valsize &&
		    (strcmp(valbuf, "name") == 0)) {
			valbuf = (char *)prom_nextprop(node_id, "name",
			    propname);
			valsize = strlen(valbuf);
		}

		if (valsize == 0) {
			opp->oprom_size = 0;
		} else if (++valsize <= userbufsize) {
			opp->oprom_size = valsize;
			bzero((caddr_t)opp->oprom_array, (size_t)valsize);
			bcopy((caddr_t)valbuf, (caddr_t)opp->oprom_array,
			    (size_t)valsize);
		}

		if (copyout(opp, (void *)arg, valsize + sizeof (uint_t)) != 0)
			error = EFAULT;
		break;

	case OPROMNEXT:
	case OPROMCHILD:
	case OPROMSETNODEID:

		if (prom_is_openprom() == 0 ||
		    userbufsize < sizeof (dnode_t)) {
			error = EINVAL;
			break;
		}

		/*
		 * The argument is a phandle. (aka dnode_t)
		 */
		if (copyin(((caddr_t)arg + sizeof (uint_t)),
		    opp->oprom_array, sizeof (dnode_t)) != 0) {
			error = EFAULT;
			break;
		}

		/*
		 * If dnode_t from userland is garbage, we
		 * could confuse the PROM.
		 */
		node_id = *(dnode_t *)opp->oprom_array;
		if (oprom_checknodeid(node_id, st->current_id) == 0) {
			cmn_err(CE_WARN, "nodeid 0x%x not found", (int)node_id);
			error = EINVAL;
			break;
		}

		if (cmd == OPROMNEXT)
			st->current_id = prom_nextnode(node_id);
		else if (cmd == OPROMCHILD)
			st->current_id = prom_childnode(node_id);
		else {
			/* OPROMSETNODEID */
			st->current_id = node_id;
			break;
		}

		opp->oprom_size = sizeof (dnode_t);
		*(dnode_t *)opp->oprom_array = st->current_id;

		if (copyout(opp, (void *)arg,
		    sizeof (dnode_t) + sizeof (uint_t)) != 0)
			error = EFAULT;
		break;

	case OPROMGETCONS:
		/*
		 * What type of console are we using?
		 * Is openboot supported on this machine?
		 */
		opp->oprom_size = sizeof (char);
		opp->oprom_array[0] = prom_stdin_is_keyboard() ?
		    OPROMCONS_STDIN_IS_KBD : OPROMCONS_NOT_WSCONS;

		opp->oprom_array[0] |= prom_stdout_is_framebuffer() ?
		    OPROMCONS_STDOUT_IS_FB : OPROMCONS_NOT_WSCONS;

		opp->oprom_array[0] |= prom_is_openprom() ?
		    OPROMCONS_OPENPROM : 0;

		if (copyout(opp, (void *)arg,
		    sizeof (char) + sizeof (uint_t)) != 0)
			error = EFAULT;
		break;

	case OPROMGETBOOTARGS: {
		extern char kern_bootargs[];

		valsize = strlen(kern_bootargs) + 1;
		if (valsize > userbufsize) {
			error = EINVAL;
			break;
		}
		(void) strcpy(opp->oprom_array, kern_bootargs);
		opp->oprom_size = valsize - 1;

		if (copyout(opp, (void *)arg, valsize + sizeof (uint_t)) != 0)
			error = EFAULT;
	}	break;

	/*
	 * convert a prom device path to an equivalent devfs path
	 */
	case OPROMPROM2DEVNAME: {
		char *dev_name;

		/*
		 * The input argument, a pathname, is a NULL terminated string.
		 */
		if ((error = oprom_copyinstr(arg, opp->oprom_array,
		    (size_t)userbufsize, MAXPATHLEN)) != 0) {
			break;
		}

		dev_name = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		error = i_promname_to_devname(opp->oprom_array, dev_name);
		if (error != 0) {
			kmem_free(dev_name, MAXPATHLEN);
			break;
		}
		valsize = opp->oprom_size = strlen(dev_name);
		if (++valsize > userbufsize) {
			kmem_free(dev_name, MAXPATHLEN);
			error = EINVAL;
			break;
		}
		(void) strcpy(opp->oprom_array, dev_name);
		if (copyout(opp, (void *)arg, sizeof (uint_t) + valsize) != 0)
			error = EFAULT;

		kmem_free(dev_name, MAXPATHLEN);
	}	break;

	/*
	 * Convert a prom device path name to a driver name
	 */
	case OPROMPATH2DRV: {
		char *drv_name;
		major_t maj;

		/*
		 * The input argument, a pathname, is a NULL terminated string.
		 */
		if ((error = oprom_copyinstr(arg, opp->oprom_array,
		    (size_t)userbufsize, MAXPATHLEN)) != 0) {
			break;
		}

		/*
		 * convert path to a driver binding name
		 */
		drv_name = i_path_to_drv((char *)opp->oprom_array);
		if (drv_name == NULL) {
			error = EINVAL;
			break;
		}

		/*
		 * resolve any aliases
		 */
		if (((maj = ddi_name_to_major(drv_name)) == -1) ||
		    ((drv_name = ddi_major_to_name(maj)) == NULL)) {
			error = EINVAL;
			break;
		}

		(void) strcpy(opp->oprom_array, drv_name);
		opp->oprom_size = strlen(drv_name);
		if (copyout(opp, (void *)arg,
		    sizeof (uint_t) + opp->oprom_size + 1) != 0)
			error = EFAULT;
	}	break;

	case OPROMGETVERSION:
		/*
		 * Get a string representing the running version of the
		 * prom. How to create such a string is platform dependent,
		 * so we just defer to a promif function. If no such
		 * association exists, the promif implementation
		 * may copy the string "unknown" into the given buffer,
		 * and return its length (incl. NULL terminator).
		 *
		 * We expect prom_version_name to return the actual
		 * length of the string, but copy at most userbufsize
		 * bytes into the given buffer, including NULL termination.
		 */

		valsize = prom_version_name(opp->oprom_array, userbufsize);
		if (valsize < 0) {
			error = EINVAL;
			break;
		}

		/*
		 * copyout only the part of the user buffer we need to.
		 */
		if (copyout(opp, (void *)arg,
		    (size_t)(min((uint_t)valsize, userbufsize) +
		    sizeof (uint_t))) != 0)
			error = EFAULT;
		break;

#if !defined(i386) && !defined(__i386) && !defined(__ia64)
	case OPROMGETFBNAME:
		/*
		 * Return stdoutpath, if it's a frame buffer.
		 * Yes, we are comparing a possibly longer string against
		 * the size we're really going to copy, but so what?
		 */
		if ((prom_stdout_is_framebuffer() != 0) &&
		    (userbufsize > strlen(prom_stdoutpath()))) {
			prom_strip_options(prom_stdoutpath(),
			    opp->oprom_array);	/* strip options and copy */
			valsize = opp->oprom_size = strlen(opp->oprom_array);
			if (copyout(opp, (void *)arg,
			    valsize + 1 + sizeof (uint_t)) != 0)
				error = EFAULT;
		} else
			error = EINVAL;
		break;

	/*
	 * Convert a logical or physical device path to prom device path
	 */
	case OPROMDEV2PROMNAME: {
		char *prom_name;

		/*
		 * The input argument, a pathname, is a NULL terminated string.
		 */
		if ((error = oprom_copyinstr(arg, opp->oprom_array,
		    (size_t)userbufsize, MAXPATHLEN)) != 0) {
			break;
		}

		prom_name = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		/*
		 * convert the devfs path to an equivalent prom path
		 */
		error = i_devname_to_promname(opp->oprom_array, prom_name);
		if (error != 0) {
			kmem_free(prom_name, MAXPATHLEN);
			break;
		}

		valsize = opp->oprom_size = strlen(prom_name);
		if (++valsize > userbufsize) {
			kmem_free(prom_name, MAXPATHLEN);
			error = EINVAL;
			break;
		}

		(void) strcpy(opp->oprom_array, prom_name);
		if (copyout(opp, (void *)arg, sizeof (uint_t) + valsize) != 0)
			error = EFAULT;

		kmem_free(prom_name, MAXPATHLEN);
	}	break;

	case OPROMSETOPT:
	case OPROMSETOPT2:
		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE)) {
			error = EINVAL;
			break;
		}

		/*
		 * The arguments are a property name and a value.
		 * Copy in the entire user buffer.
		 */
		if (copyin(((caddr_t)arg + sizeof (uint_t)),
		    opp->oprom_array, userbufsize) != 0) {
			error = EFAULT;
			break;
		}

		/*
		 * The property name is the first string, value second
		 */
		valbuf = opp->oprom_array + strlen(opp->oprom_array) + 1;
		if (cmd == OPROMSETOPT) {
			valsize = strlen(valbuf) + 1;  /* +1 for the '\0' */
		} else
			valsize = (opp->oprom_array + userbufsize) - valbuf;

		/*
		 * 4010173: 'name' is not an option, but it is a property.
		 */
		if (strcmp(opp->oprom_array, "name") == 0)
			error = EINVAL;
		else if (prom_setprop(node_id, opp->oprom_array,
		    valbuf, valsize) < 0)
			error = EINVAL;

		break;

	case OPROMREADY64: {
		struct openprom_opr64 *opr =
		    (struct openprom_opr64 *)opp->oprom_array;
		int i;
		dnode_t id;

		if (userbufsize < sizeof (*opr)) {
			error = EINVAL;
			break;
		}

		valsize = userbufsize -
		    offsetof(struct openprom_opr64, message);

		i = prom_version_check(opr->message, valsize, &id);
		opr->return_code = i;
		opr->nodeid = (int)id;

		valsize = offsetof(struct openprom_opr64, message);
		valsize += strlen(opr->message) + 1;

		/*
		 * copyout only the part of the user buffer we need to.
		 */
		if (copyout(opp, (void *)arg,
		    (size_t)(min((uint_t)valsize, userbufsize) +
		    sizeof (uint_t))) != 0)
			error = EFAULT;
		break;

	}	/* case OPROMREADY64 */
#endif	/* !__i386 && !__ia64 */
	}	/* switch (cmd)	*/

	kmem_free(opp, userbufsize + sizeof (uint_t) + 1);
	return (error);
}

/*
 * Copyin string and verify the actual string length is less than maxsize
 * specified by the caller.
 *
 * Currently, maxsize is either OBP_MAXPROPNAME for property names
 * or MAXPATHLEN for device path names. userbufsize is specified
 * by the userland caller.
 */
static int
oprom_copyinstr(intptr_t arg, char *buf, size_t bufsize, size_t maxsize)
{
	int error;
	size_t actual_len;

	if ((error = copyinstr(((caddr_t)arg + sizeof (uint_t)),
	    buf, bufsize, &actual_len)) != 0) {
		return (error);
	}
	if ((actual_len == 0) || (actual_len > maxsize)) {
		return (EINVAL);
	}

	return (0);
}

/*
 * Check dnode_t passed in from userland
 */
static int
oprom_checknodeid(dnode_t node_id, dnode_t current_id)
{
	int depth;
	dnode_t id[OBP_STACKDEPTH];

	/*
	 * optimized path
	 */
	if ((node_id == 0) || (node_id == current_id) ||
	    (node_id == prom_nextnode(current_id)) ||
	    (node_id == prom_childnode(current_id))) {
		return (1);
	}

	/*
	 * long path: walk from root till we find node_id
	 */
	depth = 1;
	id[0] = prom_nextnode((dnode_t)0);

	while (depth) {
		if (id[depth - 1] == node_id)
			return (1);	/* node_id found */

		if (id[depth] = prom_childnode(id[depth - 1])) {
			depth++;
			continue;
		}

		while (depth &&
		    ((id[depth - 1] = prom_nextnode(id[depth - 1])) == 0))
			depth--;
	}
	return (0);	/* node_id not found */
}
