/*
 * Copyright (c) 1993,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)aflt.c	1.8	97/03/14 SMI"

#include <sys/systm.h>
#include <sys/spl.h>		/* ipltospl */
#include <sys/syserr.h>		/* IPL_ECC */
#include <sys/kmem.h>
#include <sys/obpdefs.h>	/* OBP_REG */
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <vm/hat_srmmu.h>	/* xxx pa_t */
#include <sys/nvsimm.h>
#include <sys/aflt.h>
#include <sys/debug.h>

/*
 * Utilities to register asynchronous error handlers.
 * Currenly used only by the nvsimm driver to handle ECC errors.
 */

/*
 * list of attached nvram simms
 * could be static, except for impl_bustype().
 */

static struct simmslot *nvsimmlist;

/*
 * Returns true if the ECC error was handled by a driver
 */
int
aflt_handled(u_longlong_t addr, int ue)
{
	struct simmslot *ss;
	struct ecc_handler_args eha;
	int (*func)(void *, void *);

	/*
	 * List is sorted in ascending order
	 */
	for (ss = nvsimmlist; ss; ss = ss->ss_next) {
		if (addr < ss->ss_addr_lo)
			return (0);
		if (addr > ss->ss_addr_hi)
			continue;
		func = ss->ss_func;
		if (func != NULL) {
			eha.e_uncorrectable = ue;
			eha.e_addrhi = addr >> 32;
			eha.e_addrlo = addr;
			return ((*func)(ss->ss_arg,
			    (void *) &eha) == AFLT_HANDLED);
		}
	}
	return (0);
}

/*
 * The next 3 entry points are for support for drivers which need to
 * be able to register a callback for an async fault, currently only nvsimm
 * drivers do this, and they exist only on sun4m and sun4d
 */

/*ARGSUSED*/
int
aflt_get_iblock_cookie(dev_info_t *dip, int fault_type,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	/*
	 * Currently we only offer this service for nvsimms
	 */
	if (!nvsimmlist || fault_type != AFLT_ECC) {
		return (AFLT_NOTSUPPORTED);
	}
	*iblock_cookiep = (ddi_iblock_cookie_t)ipltospl(IPL_ECC);
	return (AFLT_SUCCESS);
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
int
aflt_add_handler(dev_info_t *dip, int fault_type, void **hid,
    int (*func)(void *, void *), void *arg)
{
	struct simmslot *ss;

	*hid = NULL;

	/*
	 * Currently we only offer this service for nvsimms
	 */
	if (!nvsimmlist || fault_type != AFLT_ECC) {
		return (AFLT_NOTSUPPORTED);
	}
	switch (fault_type) {
	case AFLT_ECC:
		for (ss = nvsimmlist; ss; ss = ss->ss_next) {
			if (ss->ss_dip == (void *)dip)
			    break;
		}
		if (ss->ss_dip != (void *)dip || ss->ss_func != NULL) {
			return (AFLT_FAILURE);
		}
		ss->ss_arg = arg;
		ss->ss_func = func;
		*hid = (void *)ss;
		return (AFLT_SUCCESS);

	default:
		return (AFLT_NOTSUPPORTED);
	}
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
int
aflt_remove_handler(void *hid)
{
	extern void wait_till_seen(int);
	struct simmslot *ss = (struct simmslot *)hid;

	if (ss == NULL || (int)ss->ss_func == 0)
		return (AFLT_FAILURE);
	ss->ss_func = (int (*)(void *, void *)) NULL;
	/*
	 * Since any one of these could be active...
	 */
	wait_till_seen(IPL_ECC);
	return (AFLT_SUCCESS);
}

/*
 * for memerr_init()
 */
void
nvsimmlist_add(dev_info_t *dip)
{
	u_int tmp;
	struct simmslot *ns, *ss, *lastp;
	u_int regsize;
	struct regspec *rp;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    0, OBP_REG, (int **)&rp, &regsize) != DDI_PROP_SUCCESS)
		return;

	/* convert to bytes */
	regsize = regsize * sizeof (int);
	ASSERT(regsize >= 2 * sizeof (struct regspec));

	ns = kmem_zalloc(sizeof (struct simmslot), KM_SLEEP);
	ns->ss_dip = (void *)dip;
	ns->ss_addr_lo = (((u_longlong_t)rp->regspec_bustype) << 32) +
		    (u_longlong_t)rp->regspec_addr;
	tmp = rp->regspec_size - 1;
	ns->ss_addr_hi = ns->ss_addr_lo + (u_longlong_t)tmp;
	ddi_prop_free((void *)rp);

	if (nvsimmlist) {
		/* keep list sorted in ascending addr order */
		lastp = NULL;
		for (ss = nvsimmlist; ss; ss = ss->ss_next) {
			if (ns->ss_addr_lo < ss->ss_addr_lo) {
				if (lastp) {
					ns->ss_next = ss->ss_next;
					lastp->ss_next = ns;
				} else {
					ns->ss_next = ss;
					nvsimmlist = ns;
				}
				break;
			}
			lastp = ss;
		}
		if (!ss) {
			lastp->ss_next = ns;
		}
	} else {
		nvsimmlist = ns;
	}
}

int
pa_in_nvsimmlist(pa_t physaddr)
{
	struct simmslot *ss;

	/* We know the list is sorted in ascending order. */
	for (ss = nvsimmlist; ss; ss = ss->ss_next) {
		if (physaddr < ss->ss_addr_lo)
			return (0);
		if (physaddr > ss->ss_addr_hi)
			continue;
		return (1);
	}
	return (0);
}
