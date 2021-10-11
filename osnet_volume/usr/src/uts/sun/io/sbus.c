/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sbus.c	1.34	99/10/22 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/avintr.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/machsystm.h>

static int sbus_to_sparc(int), sparc_to_sbus(int);
static int sbus_ctl_xlate_intrs(dev_info_t *, dev_info_t *,
    int *, struct ddi_parent_private_data *);

static int
sbus_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static int
sbus_dma_map(dev_info_t *, dev_info_t *,
    struct ddi_dma_req *, ddi_dma_handle_t *);

static int
sbus_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
sbus_dma_mctl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    enum ddi_dma_ctlops, off_t *, size_t *, caddr_t *, uint_t);

static int
sbus_add_intrspec(dev_info_t *, dev_info_t *,
    ddi_intrspec_t, ddi_iblock_cookie_t *, ddi_idevice_cookie_t *,
    uint_t (*int_handler)(caddr_t), caddr_t, int);

static struct bus_ops sbus_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	sbus_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	sbus_dma_map,
	sbus_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	sbus_dma_mctl,
	sbus_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int sbus_identify(dev_info_t *);
static int sbus_probe(dev_info_t *);
static int sbus_attach(dev_info_t *, ddi_attach_cmd_t cmd);
static int sbus_detach(dev_info_t *, ddi_detach_cmd_t cmd);

static struct dev_ops sbus_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	sbus_identify,		/* identify */
	sbus_probe,		/* probe- not the nodev */
	sbus_attach,		/* attach */
	sbus_detach,		/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&sbus_bus_ops,		/* bus operations */
	nulldev			/* power */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"SBus nexus driver 1.34",	/* Name of module. */
	&sbus_ops		/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * These are the module initialization routines.
 */

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

/*
 * Our burstsizes- XXX- fix if we have more than one SBus
 */
static int sbus_burst_sizes;
static int sbus_max_burst_size, sbus_min_burst_size;

static int
sbus_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "sbus") == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

static int
sbus_probe(dev_info_t *dev)
{
	if (ddi_dev_is_sid(dev) == DDI_SUCCESS)
		return (DDI_PROBE_DONTCARE);
	else
		return (DDI_PROBE_FAILURE);
}

static int
sbus_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int sbus_burst_align;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);

	/*
	 * We precompute this stuff here for speed in handling
	 * sbus_dma_map() and CTLOPS_IOMIN.
	 *
	 * XXX	If we ever need to support more than one instance of
	 *	this driver, we'll need to migrate these variables to
	 *	some per-instance state structure.  But for now ..
	 */
	sbus_burst_sizes = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "burst-sizes", 0x17);

	/*
	 * 64-bit SBus defines max 128 byte bursts and min 8 byte
	 * bursts. Take these alignments as they are more restrictive
	 * than on a 32-bit SBus.
	 */
	if (sbus_burst_sizes & 0xff0000) {
		sbus_burst_align = (sbus_burst_sizes >> 16);
	} else {
		sbus_burst_align = sbus_burst_sizes;
	}
	sbus_max_burst_size = 1 << (ddi_fls(sbus_burst_align) - 1);
	sbus_min_burst_size = 1 << (ddi_ffs(sbus_burst_align) - 1);

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
sbus_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

static int
sbus_add_intrspec(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
    ddi_iblock_cookie_t *iblock_cookiep, ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg), caddr_t int_handler_arg,
    int kind)
{
	int r;

	/*
	 * We only need to do idevice_cookie translation here.
	 */

	r = i_ddi_add_intrspec(dip, rdip, intrspec, iblock_cookiep,
		idevice_cookiep, int_handler, int_handler_arg, kind);

	if (r == DDI_SUCCESS && idevice_cookiep) {
		idevice_cookiep->idev_priority
		    = sparc_to_sbus(idevice_cookiep->idev_priority);
	}

	return (r);
}

static int
sbus_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *dma_attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	dma_attr->dma_attr_burstsizes &= (sbus_burst_sizes & 0xff);
	return (ddi_dma_allochdl(dip, rdip, dma_attr, waitfp, arg, handlep));
}

static int
sbus_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;

	/*
	 * check if device can use 64-bit SBus
	 */
	if (!(dmareq->dmar_flags & DDI_DMA_SBUS_64BIT)) {
		/*
		 * return burst size for 32-bit mode
		 */
		dma_lim->dlim_burstsizes &= (sbus_burst_sizes & 0xff);
	} else {
		/*
		 * check if SBus supports 64 bit and if caller
		 * is child of SBus. No support through bridges
		 */
		if (sbus_burst_sizes & 0xff0000 &&
		    (ddi_get_parent(rdip) == dip)) {
			struct regspec *rp;

			rp = ddi_rnumber_to_regspec(rdip, 0);
			if (rp == (struct regspec *)0) {
				dma_lim->dlim_burstsizes &=
					(sbus_burst_sizes & 0xff);
			} else {
				uint_t slot = rp->regspec_bustype;

				/*
				 * get slot number, program slot conf
				 * register and return burst sizes
				 * for 64-bit mode
				 */
				(void) sbus_set_64bit(slot);
				dma_lim->dlim_burstsizes &=
					(sbus_burst_sizes & 0xff0000);
				/*
				 * lowest is 8 byte bursts in 64-bit mode
				 */
				dma_lim->dlim_minxfer =
					max(dma_lim->dlim_minxfer, 8);
			}
		} else {
			/*
			 * SBus doesn't support it or bridge. Do 32-bit
			 * xfers
			 */
			dma_lim->dlim_burstsizes &=
				(sbus_burst_sizes & 0xff);
		}
	}
	return (ddi_dma_map(dip, rdip, dmareq, handlep));
}

static int
sbus_dma_mctl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    enum ddi_dma_ctlops request, off_t *offp, size_t *lenp,
    caddr_t *objp, uint_t flags)
{
	switch (request) {
	case DDI_DMA_SET_SBUS64:
		/*
		 * check if SBus supports 64 bit and if caller
		 * is child of SBus. No support through bridges
		 */
		if (sbus_burst_sizes & 0xff0000 &&
		    (ddi_get_parent(rdip) == dip)) {
			struct regspec *rp;

			rp = ddi_rnumber_to_regspec(rdip, 0);
			if (rp == NULL) {
				return (DDI_FAILURE);
			} else {
				ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
				uint_t slot = rp->regspec_bustype;
				uint_t burst64;

				/*
				 * get slot number, program slot conf
				 * register and return burst sizes
				 * for 64-bit mode
				 */
				(void) sbus_set_64bit(slot);
				burst64 = ((sbus_burst_sizes & 0xff0000) >> 16);
				mp->dmai_burstsizes = (*lenp & burst64);
				/*
				 * lowest is 8 byte bursts in 64-bit mode
				 */
				mp->dmai_minxfer =
					max(mp->dmai_minxfer, 8);
				return (DDI_SUCCESS);
			}
		} else {
			/*
			 * SBus doesn't support it or bridge. Do 32-bit
			 * xfers
			 */
			return (DDI_FAILURE);
		}
		/*NOTREACHED*/
	default:
		break;
	}

	return (ddi_dma_mctl(dip, rdip, handle, request, offp,
		lenp, objp, flags));
}

/* #define	SBUS_DEBUG */

#ifdef SBUS_DEBUG
int sbus_debug_flag;
#define	sbus_debug	if (sbus_debug_flag) printf
#endif /* SBUS_DEBUG */


static int
sbus_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t op, void *arg, void *result)
{
	switch (op) {
	case DDI_CTLOPS_IOMIN: {
		int val = *((int *)result);

		/*
		 * The 'arg' value of nonzero indicates 'streaming' mode.
		 * If in streaming mode, pick the largest of our burstsizes
		 * available and say that that is our minimum value (modulo
		 * what mincycle is).
		 */
		if ((int)arg)
			val = maxbit(val, sbus_max_burst_size);
		else
			val = maxbit(val, sbus_min_burst_size);

		*((int *)result) = val;
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}

	case DDI_CTLOPS_INITCHILD: {
		if (strcmp(ddi_get_name((dev_info_t *)arg), "sbusmem") == 0) {
			dev_info_t *cdip = (dev_info_t *)arg;
			int i, n;
			int slot, size;
			char ident[10];

			/* should we set DDI_PROP_NOTPROM? */

			slot = ddi_getprop(DDI_DEV_T_NONE, cdip,
			    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "slot", -1);
			if (slot == -1) {
#ifdef  SBUS_DEBUG
				sbus_debug("can't get slot property\n");
#endif  SBUS_DEBUG
				return (DDI_FAILURE);
			}

#ifdef SBUS_DEBUG
			sbus_debug("number of range properties (slots) %d\n",
			    sparc_pd_getnrng(dip));
#endif /* SBUS_DEBUG */

			for (i = 0, n = sparc_pd_getnrng(dip); i < n; i++) {
				struct rangespec *rp = sparc_pd_getrng(dip, i);

				if (rp->rng_cbustype == (uint_t)slot) {
					struct regspec r;

					/* create reg property */

					r.regspec_bustype = (uint_t)slot;
					r.regspec_addr = 0;
					r.regspec_size = rp->rng_size;
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "reg",
					    (caddr_t)&r,
					    sizeof (struct regspec));

#ifdef SBUS_DEBUG
		sbus_debug("range property: cbustype %x\n", rp->rng_cbustype);
		sbus_debug("		    coffset  %x\n", rp->rng_coffset);
		sbus_debug("		    bustype  %x\n", rp->rng_bustype);
		sbus_debug("		    offset   %x\n", rp->rng_offset);
		sbus_debug("		    size     %x\n\n", rp->rng_size);
		sbus_debug("reg property:   bustype  %x\n", r.regspec_bustype);
		sbus_debug("		    addr     %x\n", r.regspec_addr);
		sbus_debug("                size     %x\n", r.regspec_size);
#endif /* SBUS_DEBUG */
					/* create size property for slot */

					size = rp->rng_size;
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "size",
					    (caddr_t)&size, sizeof (int));

					/* create identification property */

					(void) sprintf(ident, "slot%x", slot);
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "ident",
					    ident, sizeof (ident));

					return (impl_ddi_sbus_initchild(arg));
				}
			}
			return (DDI_FAILURE);
		}

		return (impl_ddi_sbus_initchild(arg));
	}

	case DDI_CTLOPS_UNINITCHILD: {
		impl_ddi_sunbus_removechild(arg);
		return (DDI_SUCCESS);
	}

	/*
	 * These specific uglinesses are needed right now because
	 * we do not store our child addresses as 'sbus' addresses
	 * as yet. This will also have to change for OBPV2. This
	 * could also be much more robust in that it should check
	 * to make sure that the address really is an SBus address.
	 */

	case DDI_CTLOPS_REPORTDEV: {

#define	REPORTDEV_BUFSIZE	1024

		dev_info_t *pdev;
		int i, n, len, f_len = 0;
		char *buf;

		if (DEVI_PD(rdip) == NULL)
			return (DDI_FAILURE);

		pdev = (dev_info_t *)DEVI(rdip)->devi_parent;
		buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);
		f_len += snprintf(buf, REPORTDEV_BUFSIZE, "%s%d at %s%d",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    ddi_driver_name(pdev), ddi_get_instance(pdev));
		len = strlen(buf);

		for (i = 0, n = sparc_pd_getnreg(rdip); i < n; i++) {

			struct regspec *rp = sparc_pd_getreg(rdip, i);

			if (i == 0) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, ": ");
			} else {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, " and ");
			}
			len = strlen(buf);

			/*
			 * Maybe we should print 'onboard SBus'
			 * .. though 4c onboard is slot 0 and 4m
			 * onboard is slot f .. so this is tricky.
			 */
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "SBus slot %x 0x%x",
			    rp->regspec_bustype, rp->regspec_addr);
			len = strlen(buf);
		}

		for (i = 0, n = sparc_pd_getnintr(rdip); i < n; i++) {

			int pri, sbuslevel;

			if (i != 0) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, ",");
				len = strlen(buf);
			}

			pri = INT_IPL(sparc_pd_getintr(rdip, i)->intrspec_pri);
			/*
			 * When might 'sbuslevel' be -1 .. well on the 4m of
			 * course - the 'onboard SBus' generates interrupts
			 * at non-SBus levels .. sigh.
			 */
			if ((sbuslevel = sparc_to_sbus(pri)) != -1) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len,
				    " SBus level %d", sbuslevel);
				len = strlen(buf);
			}
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " sparc ipl %d", pri);
			len = strlen(buf);
		}
#ifdef DEBUG
	if (f_len + 1 >= REPORTDEV_BUFSIZE) {
		cmn_err(CE_NOTE, "next message is truncated: "
		    "printed length 1024, real length %d", f_len);
	}
#endif DEBUG

		cmn_err(CE_CONT, "?%s\n", buf);
		kmem_free(buf, REPORTDEV_BUFSIZE);
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_XLATE_INTRS: {
		return (sbus_ctl_xlate_intrs(dip, rdip, arg, result));
	}

	case DDI_CTLOPS_SLAVEONLY: {
		if (DEVI_PD(rdip) && sparc_pd_getnreg(rdip) > 0) {
			uint_t slot = sparc_pd_getreg(rdip, 0)->regspec_bustype;
			int slaveslot = ddi_getprop(DDI_DEV_T_ANY, dip, 0,
			    "slave-only", 0);
			if ((1 << slot) & slaveslot)
				return (DDI_SUCCESS);
		}
		return (DDI_FAILURE);
	}

	case DDI_CTLOPS_AFFINITY: {
		dev_info_t *dipb = (dev_info_t *)arg;
		if ((DEVI_PD(rdip) && sparc_pd_getnreg(rdip) > 0) &&
		    (DEVI_PD(dipb) && sparc_pd_getnreg(dipb) > 0)) {
			uint_t slot = sparc_pd_getreg(rdip, 0)->regspec_bustype;
			uint_t slot_b =
			    sparc_pd_getreg(dipb, 0)->regspec_bustype;
			if (slot == slot_b)
				return (DDI_SUCCESS);
		}
		return (DDI_FAILURE);
	}

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}

/*
 * We're prepared to claim that the interrupt string is in
 * the form of a list of <SBusintr> specifications. Translate it.
 */
static int
sbus_ctl_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	int n;
	size_t size;
	struct intrspec *new;

	static char bad_sbusintr_fmt[] =
	    "sbus%d: bad interrupt spec for %s%d - SBus level %d\n";

	/*
	 * The list consists of <SBuspri> elements
	 */
	if ((n = *in++) < 1)
		return (DDI_FAILURE);

	pdptr->par_nintr = n;
	size = n * sizeof (struct intrspec);
	new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

	while (n--) {
		int level = *in++;

		if (level < 1 || level > 7) {
			cmn_err(CE_CONT, bad_sbusintr_fmt,
			    DEVI(dip)->devi_instance, DEVI(rdip)->devi_name,
			    DEVI(rdip)->devi_instance, level);
			goto broken;
			/*NOTREACHED*/
		}

		new->intrspec_pri = sbus_to_sparc(level) | INTLEVEL_SBUS;
		new++;
	}

	return (DDI_SUCCESS);
	/*NOTREACHED*/

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	return (DDI_FAILURE);
}

/*
 * Here's a slight ugliness.  Because the sun4m architecture decided to
 * change the mapping of SBus levels to sparc ipl's, we can't both hide
 * all the details of interrupt mappings here -and- maintain implementation
 * architecture dependence, sigh.  So, we reference a table that's defined
 * in autoconf.c of the relevant kernel architecture. Either this
 * remapping should never have happened, or the sun4m SBus shouldn't have
 * had the name 'sbus'.  But -hey- we all have to live with our past
 * misdemeanours .. and this doesn't seem too horrendous.
 */

#include <sys/machsystm.h>

static int
sbus_to_sparc(int sbuslevel)
{
	if (sbuslevel < 1 || sbuslevel > 7)
		return (-1);
	else
		return ((int)sbus_to_sparc_tbl[sbuslevel]);
}

static int
sparc_to_sbus(int pri)
{
	int sbuslevel;

	for (sbuslevel = 1; sbuslevel <= 7; sbuslevel++)
		if (sbus_to_sparc(sbuslevel) == pri)
			return (sbuslevel);
	return (-1);
}
