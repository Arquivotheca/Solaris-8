/*
 * Copyright (c) 1995-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ieef.c	1.31	99/05/08 SMI"

/* use ':set ts=4 sw=4' if you edit this file with vi */

/*
 * ieef -- Intel Etherexpress 32
 * Depends on the Generic LAN Driver utility functions in /kernel/misc/gld
 */

/*
 * XXX: This driver needs to be compiled without -O because a while loop
 *	defined in the ieef.h file will be optimizd away.  A feature
 *	that only happens when the device is writing to main memory
 *	directly for status.
 */

/*
 * This file is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#ifndef lint
static char	sccsid[] =
	"@(#)gldconfig 1.1 93/02/12 Copyright 1993 Sun Microsystems";
#endif

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/devops.h>
#include <sys/sunddi.h>
#include <sys/eisarom.h>
#include <sys/ksynch.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/strsun.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/gld.h>
#include <sys/ieef.h>
#include <sys/nvm.h>
#include <sys/pci.h>

/*
 *  Declarations and Module Linkage
 */

static char ident[] = "Intel 82596/82556 Drivers";
#ifdef IEEFDEBUG
/* used for debugging */
int	ieefdebug = 0;
#endif

/* Required system entry points */
static	ieefidentify(dev_info_t *);
static	ieefdevinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static	ieefprobe(dev_info_t *);
static	ieefattach(dev_info_t *, ddi_attach_cmd_t);
static	ieefdetach(dev_info_t *, ddi_detach_cmd_t);
static	ieefdie(dev_info_t *, ddi_reset_cmd_t);

/* Required driver entry points for GLD */
int	ieef_reset(gld_mac_info_t *);
int	ieef_start_board(gld_mac_info_t *);
int	ieef_stop_board(gld_mac_info_t *);
int	ieef_saddr(gld_mac_info_t *);
int	ieef_dlsdmult(gld_mac_info_t *, struct ether_addr *, int);
int	ieef_prom(gld_mac_info_t *, int);
int	ieef_gstat(gld_mac_info_t *);
int	ieef_send(gld_mac_info_t *, mblk_t *);
uint_t	ieefintr(gld_mac_info_t *);
static void	ieef_wdog(void *);
static void	ieef_wreset(gld_mac_info_t *);

#if defined(PCI_DDI_EMULATION) || defined(COMMON_IO_EMULATION)
char _depends_on[] = "misc/xpci misc/gld";
#else
char _depends_on[] = "misc/gld";
#endif

/* Standard Streams initialization */

static struct module_info minfo = {
	IEEFIDNUM, "ieef", 0, INFPSZ, IEEFHIWAT, IEEFLOWAT
};

static struct qinit rinit = {	/* read queues */
	NULL, gld_rsrv, gld_open, gld_close, NULL, &minfo, NULL
};

static struct qinit winit = {	/* write queues */
	gld_wput, gld_wsrv, NULL, NULL, NULL, &minfo, NULL
};

struct streamtab ieefinfo = {&rinit, &winit, NULL, NULL};

/* Standard Module linkage initialization for a Streams driver */

extern struct mod_ops mod_driverops;

static 	struct cb_ops cb_ieefops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&ieefinfo,		/* cb_stream */
	(int)(D_MP)		/* cb_flag */
};

struct dev_ops ieefops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ieefdevinfo,		/* devo_getinfo */
	ieefidentify,		/* devo_identify */
	ieefprobe,		/* devo_probe */
	ieefattach,		/* devo_attach */
	ieefdetach,		/* devo_detach */
	ieefdie,		/* devo_reset */
	&cb_ieefops,		/* devo_cb_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	ident,			/* short description */
	&ieefops			/* driver specific ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

static kmutex_t ieef_probe_lock;
static kmutex_t ieef_wdog_lock;
static int ieef_cmd_complete(gld_mac_info_t *macinfo,
	struct ieefinstance *ieefp);
static int ieef_xmit_complete(gld_mac_info_t *macinfo,
	struct ieefinstance *ieefp);
static int ieef_process_recv(gld_mac_info_t *macinfo,
	struct ieefinstance *ieefp);
static void ieef_reset_board(struct ieefinstance *ieefp);
static void ieef_reset_rfa(struct ieefinstance *ieefp);
static int ieef_avail_cmd(struct ieefinstance *ieefp);
static void ieef_xmit_free(struct ieefinstance *ieefp);
static void ieef_rbuf_free(struct ieefinstance *ieefp);
static void ieef_rfa_fix(struct ieefinstance *ieefp);
static int ieef_avail_tbd(struct ieefinstance *ieefp);
static int ieef_add_command(struct ieefinstance *ieefp,
	gen_cmd_t *cmd, int len);
void ieef_stop_board_int(gld_mac_info_t *macinfo);
static void ieef_start_ru(struct ieefinstance *ieefp);
static void ieef_start_board_int(gld_mac_info_t *macinfo);
static int ieef_configure(struct ieefinstance *ieefp);
static int ieef_configure_ee100(struct ieefinstance *ieefp);
static int ieef_configure_flash32(struct ieefinstance *ieefp);
static int ieef_alloc_buffers(struct ieefinstance *ieefp);
static int ieef_init_board(gld_mac_info_t *macinfo);
static void ieef_dealloc_memory(struct ieefinstance *ieefp);

static short ieef_type(dev_info_t *devinfo);
static void ieef_port_ca_nid(struct ieefinstance *ieefp);
static long ieef_pci_get_port(dev_info_t *devinfo);
static void lock_data_rate(struct ieefinstance *ieefp);
static int ee100_old_eisa_probe(dev_info_t *devinfo);

static void ieef_set_media(gld_mac_info_t *, uchar_t);
static int ieef_send_test(gld_mac_info_t *);
static void ieef_detect_media(gld_mac_info_t *);
static void ieef_detect_speed(gld_mac_info_t *);

extern int pci_config_setup(dev_info_t *dip, ddi_acc_handle_t *handle);

extern int eisa_nvm(char *data, KEY_MASK key_mask, ...);

static crc_error_cnt = 0, alignment_error_cnt = 0, no_resources_cnt = 0,
	dma_overrun_cnt = 0, frame_too_short_cnt = 0, no_eop_flag_cnt = 0,
	receive_collision_cnt = 0, length_error_cnt;

int
_init(void)
{
	int	status;

	mutex_init(&ieef_probe_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&ieef_wdog_lock, NULL, MUTEX_DRIVER, NULL);
	status = mod_install(&modlinkage);

	if (status != 0) {
		mutex_destroy(&ieef_probe_lock);
		mutex_destroy(&ieef_wdog_lock);
	}

	return (status);
}

int
_fini(void)
{
	int	status;

	status = mod_remove(&modlinkage);
	if (status == 0) {
		mutex_destroy(&ieef_probe_lock);
		mutex_destroy(&ieef_wdog_lock);
	}

	return (status);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 *  DDI Entry Points
 */

/* identify(9E) -- See if we know about this device */

ieefidentify(dev_info_t *devinfo)
{
	if (strcmp(ddi_get_name(devinfo), "ieef") == 0)
		return (DDI_IDENTIFIED);
	else if (strcmp(ddi_get_name(devinfo), "pci8086,1227") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/* getinfo(9E) -- Get device driver information */

ieefdevinfo(dev_info_t *devinfo, ddi_info_cmd_t cmd, void *arg, void **result)
{
	register int error;

	arg = arg;	/* make 'make lint' happy */
	/* This code is not DDI compliant: the correct semantics	*/
	/* for CLONE devices is not well-defined yet.			*/
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (devinfo == NULL) {
			error = DDI_FAILURE;	/* Unfortunate */
		} else {
			*result = (void *)devinfo;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;	/* This CLONEDEV always returns zero */
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}


static	ieef_get_irq(int slot, int *irq, short *type)
{
	struct {
		short		slotnum;
		NVM_SLOTINFO	slot;
		NVM_FUNCINFO	func;
	} buff;
	NVM_SLOTINFO		*nvm;
	int			rc;
	int			function_num;
	KEY_MASK		key_mask = {0};

	/* EISA_SLOT */
	key_mask.slot = 1;
	/* EISA_CFUNCTION */
	key_mask.function = 1;

	*irq = 0;

	for (function_num = 0; ; function_num++) {

		/* get slot info and just the next function record */
		rc = eisa_nvm((char *)&buff, key_mask, slot, function_num);
		if (function_num == 0) {
			if (rc == 0) {
				/* it's an unconfigured slot */
				return (B_FALSE);
			}

			if (slot != buff.slotnum) {
				/* shouldn't happen */
				return (B_FALSE);
			}

			nvm = (NVM_SLOTINFO *)&buff.slot;

			if ((*(long *)(gldnvm(nvm)->boardid)) == IEEF_UNISYS_ID)
				*type = IEEF_HW_UNISYS;
			else if ((*(long *)(gldnvm(nvm)->boardid)) ==
				IEEF_EE100_ID)
				*type = IEEF_HW_EE100_EISA;
			else if (((*(long *)(gldnvm(nvm)->boardid)) ==
				IEEF_FLASH_ID) ||
				((*(long *)(gldnvm(nvm)->boardid)) ==
				IEEF_UNISYS_FLASH))
				*type = IEEF_HW_FLASH;
			else {
				return (B_FALSE);
			}
		}
		if (rc == 0) {
			/* end of functions, no irq defined */
			break;
		}

		if (buff.func.fib.irq) {
			if ((*type == IEEF_HW_FLASH) ||
			    (*type == IEEF_HW_EE100_EISA))
				goto got_it;
			if (!buff.func.fib.type)
				continue;
			if (strncmp((char *)buff.func.type, "SYS,ETH", 7) == 0)
				goto got_it;

		}
	}

	if (*type != IEEF_HW_UNISYS) {
		cmn_err(CE_WARN, "ieef: no irq found.\n");
		return (B_FALSE);
	}
	*irq = 10;
	return (B_TRUE);

got_it:
	*irq = buff.func.un.r.irq[0].line;
	return (B_TRUE);
}


/* probe(9E) -- Determine if a device is present */

static int
ee100_eisa_probe(dev_info_t *devinfo)
{
	static int unisys = 1;
	int	i;
	short	adap_type;
	int	*irqarr, irqlen, irq;
	int	ioaddr, found_adapter = 0;
	int	irqval;
	struct {
		int bustype;
		int base;
		int size;
	} *reglist;
	int nregs, reglen;

	/*
	 * The first thing that we have to do is to determine if this is an old
	 * style 2.4 boot or the new 2.6 bootstrap. We do this by checking for
	 * the property 'ignore-hardware-nodes'. If this property exists it
	 * means we will not expect the nodes generated by the new bootstrap
	 * and should use the old mechanism.
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
			"ignore-hardware-nodes", 0) != 0) {
		return (ee100_old_eisa_probe(devinfo));
	}

	if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo,
		DDI_PROP_DONTPASS, "reg", (caddr_t)&reglist, &reglen) !=
		DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
		    "ieef: reg property not found in devices property list");
		return (DDI_PROBE_FAILURE);
	}
	nregs = reglen / sizeof (*reglist);
	for (i = 0; i < nregs; i++) {
		if (reglist[i].bustype == 1) {
			ioaddr = reglist[i].base;
			break;
		}
	}
	kmem_free(reglist, reglen);
	if (i >= nregs) {
		cmn_err(CE_WARN,
		    "ieef: invalid reg property, base address not specified");
		return (DDI_PROBE_FAILURE);
	}

	if ((ioaddr < 0x1000) && (ioaddr != 0x300)) {
		cmn_err(CE_WARN,
			"ieef: bootstrap provided an invalid base address 0x%x",
			ioaddr);
		return (DDI_PROBE_FAILURE);
	}

	/* EISA adapters */
	if (ioaddr >= 0x1000) {
		if (!ieef_get_irq((int)ioaddr/0x1000, &irq, &adap_type)) {
			return (DDI_PROBE_FAILURE);
		}
	}

	/* ISA adapter */
	if (ioaddr == 0x300) {
		for (unisys = 1; unisys < EISA_MAXSLOT; unisys++) {
			if (ieef_get_irq(unisys, &irq, &adap_type))
				if (adap_type == IEEF_UNISYS_ID)
					break;
		}

		if (unisys > EISA_MAXSLOT) {
			cmn_err(CE_WARN,
		"ieef: adapter probed at address 0x300 but no slot info found");
			return (DDI_PROBE_FAILURE);
		}
	}

	if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
		"interrupts", (caddr_t)&irqarr, &irqlen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
		"ieef: interrupts property not found in devices property list");
		return (DDI_PROBE_FAILURE);
	}

	irqval = irqarr[(irqlen / sizeof (int))-1];
	kmem_free(irqarr, irqlen);

	if (irq != irqval) {
		cmn_err(CE_WARN,
		"ieef: interrupts property does not match that in NVRAM");
		return (DDI_PROBE_FAILURE);
	}

	if (adap_type == IEEF_HW_UNISYS)
		found_adapter = 1;
	else {
		/* this is the Intel Flash 32 product ID */
		if ((((inb(ioaddr + IEEF_ID0)) == 0x25) &&
			((inb(ioaddr + IEEF_ID1)) == 0xd4) &&
			((inb(ioaddr + IEEF_ID2)) == 0x10) &&
			((inb(ioaddr + IEEF_ID3)) == 0x10)) ||

			/* This is for EEPRO100/EISA */
			(((inb(ioaddr + IEEF_ID0)) == 0x25) &&
			((inb(ioaddr + IEEF_ID1)) == 0xd4) &&
			((inb(ioaddr + IEEF_ID2)) == 0x10) &&
			((inb(ioaddr + IEEF_ID3)) == 0x60)) ||

			/*
			 * I guess this is the product ID for the
			 * Unisys EISA card
			 */
			(((inb(ioaddr + IEEF_ID0)) == 0x55) &&
			((inb(ioaddr + IEEF_ID1)) == 0xc2) &&
			((inb(ioaddr + IEEF_ID2)) == 0x00) &&
			((inb(ioaddr + IEEF_ID3)) == 0x48))) {
			found_adapter = 1;
		}
	}

	if (found_adapter) {
		return (DDI_PROBE_SUCCESS);
	}
	return (DDI_PROBE_FAILURE);
}

static int
ee100_old_eisa_probe(dev_info_t *devinfo)
{
	int		base_io_address;
	static int	lastslot = -1;
	int		regbuf[3];
	short		type;
	int		irq;
	int		i;
	int		len;
	int		slot;
	struct intrprop {
		int spl;
		int irq;
	} *intrprop;


	mutex_enter(&ieef_probe_lock);

	/*
	 * Loop through all EISA slots, and check the ID string for
	 * the INTEL ETHEREXPRESS 32.  Each time we come through this
	 * routine, we increment the static 'lastslot' so we can start
	 * searching from the one at which we left off last time.
	 */
	for (slot = lastslot + 1; slot < EISA_MAXSLOT; slot++) {

		if (!ieef_get_irq(slot, &irq, &type))
			continue;

		if ((type == IEEF_HW_FLASH) || (type == IEEF_HW_EE100_EISA)) {
			/*
			 * For some unknown reason, on an Intel Xpress MP two
			 * Pentium/66 EISA machine with card in slot 1, slot 0
			 * appears to contain a card.
			 */
			if (slot == 0)
				continue;
			base_io_address = slot * 0x1000;
		} else
			base_io_address = 0x300;

		/* Check the ID bytes, make sure the card is really there */

		if ((type == IEEF_HW_FLASH) || (type == IEEF_HW_EE100_EISA)) {
			/* this is the Intel Flash 32 product ID */
			if ((((inb(base_io_address + IEEF_ID0)) != 0x25) ||
			    ((inb(base_io_address + IEEF_ID1)) != 0xd4) ||
			    ((inb(base_io_address + IEEF_ID2)) != 0x10) ||
			    ((inb(base_io_address + IEEF_ID3)) != 0x10)) &&
			    /* Not EEPRO/100 EISA either */
			    (((inb(base_io_address + IEEF_ID0)) != 0x25) ||
			    ((inb(base_io_address + IEEF_ID1)) != 0xd4) ||
			    ((inb(base_io_address + IEEF_ID2)) != 0x10) ||
			    ((inb(base_io_address + IEEF_ID3)) != 0x60)) &&

			/* Guess this is the product ID for Unisys EISA card */
			    (((inb(base_io_address + IEEF_ID0)) != 0x55) ||
			    ((inb(base_io_address + IEEF_ID1)) != 0xc2) ||
			    ((inb(base_io_address + IEEF_ID2)) != 0x00) ||
			    ((inb(base_io_address + IEEF_ID3)) != 0x48))) {
				cmn_err(CE_WARN,
					"ieef: CMOS has card in slot %d,"
					" card not there.\n", slot);
				continue;
			}
		}
		goto found_board;
	}
	lastslot = slot;
	mutex_exit(&ieef_probe_lock);
	return (DDI_PROBE_FAILURE);

found_board:
#ifdef IEEFDEBUG
	if (ieefdebug & IEEFDDI) {
		cmn_err(CE_CONT,
		    "ieefprobe: found slot=%d irq=%d ioaddr=0x%x type=%d\n",
		    slot, irq, base_io_address, type);
	}
#endif
	lastslot = slot;
	regbuf[0] = base_io_address;
	(void) ddi_prop_create(DDI_DEV_T_NONE, devinfo,
		DDI_PROP_CANSLEEP, "ioaddr", (caddr_t)regbuf, sizeof (int));

	/*
	 * Compare the IRQ with the intr property, and save the
	 * property index so that GLD can properly add our interrupt
	 */

	if ((i = ddi_getlongprop(DDI_DEV_T_ANY, devinfo,
	    DDI_PROP_DONTPASS, "intr", (caddr_t)&intrprop, &len)) !=
	    DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
			"ieef: Could not locate intr property.\n");
		mutex_exit(&ieef_probe_lock);
		return (DDI_PROBE_FAILURE);
	}

	len /= sizeof (struct intrprop);

	for (i = 0; i < len; i++)
		if (irq == intrprop[i].irq)
			break;

	kmem_free(intrprop, len * sizeof (struct intrprop));

	if (i >= len) {
		cmn_err(CE_WARN,
			"ieef: irq in conf file does not match CMOS\n");
		mutex_exit(&ieef_probe_lock);
		return (DDI_PROBE_FAILURE);
	}

	ddi_set_driver_private(devinfo, (caddr_t)i);
	mutex_exit(&ieef_probe_lock);
	return (DDI_PROBE_SUCCESS);
}

static int
ee100_pci_probe(dev_info_t *devinfo)
{
	ddi_acc_handle_t	handle;
	ushort_t vendor_id, device_id, iline, cmdreg;
	int	pci_probe_result;

	pci_probe_result = DDI_PROBE_FAILURE;

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (pci_probe_result);

	vendor_id = pci_config_getw(handle, PCI_CONF_VENID);
	device_id = pci_config_getw(handle, PCI_CONF_DEVID);

	if ((vendor_id == IEEF_EE100_PCI_VENDOR) &&
		(device_id == IEEF_EE100_PCI_DEVICE)) {
		iline = pci_config_getb(handle, PLXP_INTERRUPT_LINE);
		cmdreg = pci_config_getb(handle, PLXP_COMMAND_REGISTER);
		if ((iline > 15) || (iline == 0))
			cmn_err(CE_CONT,
				"ee100_pci_probe(): iline out of range\n");
		else
		{	pci_probe_result = DDI_PROBE_SUCCESS;
			if (!(cmdreg & CMD_BUS_MASTER)) {
				pci_config_putb(handle, PLXP_COMMAND_REGISTER,
					cmdreg | CMD_BUS_MASTER);
			}
		}
	}
	pci_config_teardown(&handle);
	return (pci_probe_result);
}

static short
ieef_bus_type_check(dev_info_t *devinfo)
{
	char	parent_type[16];
	int	parentlen;

	parentlen = sizeof (parent_type);

	if (ddi_prop_op(DDI_DEV_T_ANY, devinfo, PROP_LEN_AND_VAL_BUF, 0,
	    "device_type", (caddr_t)parent_type, &parentlen) !=
	    DDI_PROP_SUCCESS && ddi_prop_op(DDI_DEV_T_ANY, devinfo,
	    PROP_LEN_AND_VAL_BUF, 0, "bus-type", (caddr_t)parent_type,
	    &parentlen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "ieef_bus_type_check(): "
			"Don't know device type for parent \"%s\"\n",
				ddi_get_name(ddi_get_parent(devinfo)));
		return (-1);
	}

	if (strcmp(parent_type, "eisa") == 0)
		return (IEEF_BUS_EISA);
	else if (strcmp(parent_type, "pci") == 0)
		return (IEEF_BUS_PCI);
	else return (-1);
}

ieefprobe(dev_info_t *devinfo)
{
	short   bus_type;
	int ddi_probe_result;

	bus_type = ieef_bus_type_check(devinfo);
	if (bus_type == IEEF_BUS_EISA) {
		ddi_probe_result = ee100_eisa_probe(devinfo);
	} else if (bus_type == IEEF_BUS_PCI) {
		ddi_probe_result = ee100_pci_probe(devinfo);
	} else {
		ddi_probe_result = DDI_PROBE_FAILURE;
	}

	return (ddi_probe_result);
}


static long ieef_pci_get_port(dev_info_t *devinfo)
{
	ddi_acc_handle_t	handle;
	long ioaddr;

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (-1);

	ioaddr = pci_config_getl(handle, PLXP_BAR_1_REGISTER) & 0xfffffffc;

	pci_config_teardown(&handle);
	return (ioaddr);
}

/*
 *  attach(9E) -- Attach a device to the system
 *
 *  Called once for each board successfully probed.
 */

ieefattach(dev_info_t *devinfo, ddi_attach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;		/* GLD structure */
	struct ieefinstance *ieefp;		/* Our private device info */
	int	i;
	struct {
		int bustype;
		int base;
		int size;
	} *reglist;
	int	nregs, reglen;
	int	autospeed = 0;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFDDI) {
		debug_enter("\n\nieef attach\n\n");
		cmn_err(CE_CONT, "ieefattach(0x%x)", (int)devinfo);
	}
#endif

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	/*
	 *  Allocate gld_mac_info_t and ieefinstance structures
	 */
	macinfo = (gld_mac_info_t *)kmem_zalloc(
		sizeof (gld_mac_info_t) + sizeof (struct ieefinstance),
			KM_NOSLEEP);
	if (macinfo == NULL) {
		cmn_err(CE_WARN, "ieef: kmem_zalloc failure for macinfo");
		return (DDI_FAILURE);
	}

	ieefp = (struct ieefinstance *)(macinfo + 1);

	/*  Initialize our private fields in macinfo and ieefinstance */
	macinfo->gldm_private = (caddr_t)ieefp;

	ieefp->ieef_type = ieef_type(devinfo);
	ieef_port_ca_nid(ieefp);

	if (ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
		"ignore-hardware-nodes", 0) != 0) {

		if (ieefp->ieef_type != IEEF_HW_EE100_PCI) {
			ieefp->ieef_ioaddr = macinfo->gldm_port =
			    ddi_getprop(DDI_DEV_T_ANY, devinfo,
				DDI_PROP_DONTPASS, "ioaddr", 0);
			macinfo->gldm_irq_index =
				(long)ddi_get_driver_private(devinfo);
		} else {
			ieefp->ieef_ioaddr = macinfo->gldm_port =
			    ieef_pci_get_port(devinfo);
			macinfo->gldm_irq_index = 0;
		}
	} else { /* 2.6 Boot */
		if (ieefp->ieef_type != IEEF_HW_EE100_PCI) {
			if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo,
			    DDI_PROP_DONTPASS, "reg", (caddr_t)&reglist,
			    &reglen) != DDI_PROP_SUCCESS) {
				cmn_err(CE_WARN,
				    "ieef: reg property not in property list");
				goto afail;
			}
			nregs = reglen / sizeof (*reglist);
			for (i = 0; i < nregs; i++)
				if (reglist[i].bustype == 1) {
					ieefp->ieef_ioaddr = macinfo->gldm_port
					    = reglist[i].base;
					break;
				}

			kmem_free(reglist, reglen);
			ASSERT(i < nregs);
			/*
			 * Already checked the "interrupts" property in probe
			 * routine. No need to check it again. It should not
			 * change between there and here.
			 */
			macinfo->gldm_irq_index  = 0;
		} else {
			ieefp->ieef_ioaddr = macinfo->gldm_port =
			    ieef_pci_get_port(devinfo);
			macinfo->gldm_irq_index  = 0;
		}
	}
	macinfo->gldm_state = IEEF_IDLE;
	macinfo->gldm_flags = 0;

	ieefp->ieef_dip = devinfo;

	macinfo->gldm_reg_index = -1;

	/*
	 *  Initialize pointers to device specific functions which will be
	 *  used by the generic layer.
	 */
	macinfo->gldm_reset   = ieef_reset;
	macinfo->gldm_start   = ieef_start_board;
	macinfo->gldm_stop    = ieef_stop_board;
	macinfo->gldm_saddr   = ieef_saddr;
	macinfo->gldm_sdmulti = ieef_dlsdmult;
	macinfo->gldm_prom    = ieef_prom;
	macinfo->gldm_gstat   = ieef_gstat;
	macinfo->gldm_send    = ieef_send;
	macinfo->gldm_intr    = ieefintr;
	macinfo->gldm_ioctl   = NULL;    /* if you have one, NULL otherwise */

	/*
	 *  Initialize board characteristics needed by the generic layer.
	 */
	macinfo->gldm_ident = ident;
	macinfo->gldm_type = DL_ETHER;
	macinfo->gldm_minpkt = 0;		/* assumes we pad ourselves */
	macinfo->gldm_maxpkt = IEEFMAXPKT;
	macinfo->gldm_addrlen = ETHERADDRL;
	macinfo->gldm_saplen = -2;

#if 0
	/*
	 * Get the 'framesize' property.  This must be a factor of
	 * the size of a physical page.
	 */

	ieefp->ieef_framesize = ddi_getprop(DDI_DEV_T_NONE, devinfo,
		DDI_PROP_DONTPASS, "framesize", 0);

	if (ieefp->ieef_framesize == 0) {
		cmn_err(CE_WARN, "ieef: Could not find framesize property.\n");
		cmn_err(CE_WARN, "ieef: Will not attach.\n");
		goto afail;
	}
#else
	ieefp->ieef_framesize = 0x800;		/* 2048 bytes */
#endif

	/*
	 * Get the 'nframes' property.
	 */
	ieefp->ieef_nframes = ddi_getprop(DDI_DEV_T_NONE, devinfo,
		DDI_PROP_DONTPASS, "nframes", 0);

	if (ieefp->ieef_nframes <= 0) {
		ieefp->ieef_nframes = 30;
	}

	if (ieefp->ieef_nframes > IEEF_NFRAMES)
		ieefp->ieef_nframes = IEEF_NFRAMES;

#if 0
	/*
	 * Get the 'xbufsize' property.  Must be a factor of pagesize.
	 */
	ieefp->ieef_xbufsize = ddi_getprop(DDI_DEV_T_NONE, devinfo,
		DDI_PROP_DONTPASS, "xbufsize", 0);

	if (ieefp->ieef_xbufsize == 0) {
		cmn_err(CE_WARN, "ieef: Could not find xbufsize property.\n");
		cmn_err(CE_WARN, "ieef: Will not attach.\n");
		goto afail;
	}
#else
	ieefp->ieef_xbufsize = 2048;
#endif

	/*
	 * Get the 'xmits' property.
	 */
	ieefp->ieef_xmits = ddi_getprop(DDI_DEV_T_NONE, devinfo,
		DDI_PROP_DONTPASS, "xmits", 0);

	if (ieefp->ieef_xmits <= 0) {
		ieefp->ieef_xmits = 30;
	}

	if (ieefp->ieef_xmits > IEEF_NXMIT)
		ieefp->ieef_xmits = IEEF_NXMIT;
	ieefp->ieef_ncmds = ieefp->ieef_xmits;

	/*
	 * if speed property is present, then don't auto-sense
	 */
	ieefp->ieef_speed = ddi_getprop(DDI_DEV_T_NONE, devinfo,
	    DDI_PROP_DONTPASS, "speed", 0xffff);
	if (ieefp->ieef_speed == 0xffff) {
		ieefp->ieef_speed = 10;
		autospeed = 1;
	}

	/* Get the board's vendor-assigned hardware network address */
	if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		ddi_acc_handle_t	handle;

		if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"ieefattach: Could not get pci handler.\n");
			cmn_err(CE_WARN, "ieefattach: Will not attach.\n");
			goto afail;
		}
		for (i = 0; i < ETHERADDRL; i++) {
			macinfo->gldm_vendor[i] =
				pci_config_getb(handle, ieefp->ieef_nid + i);
		}
		pci_config_teardown(&handle);
	} else {
		for (i = 0; i < ETHERADDRL; i++) {
			macinfo->gldm_vendor[i] = inb(ieefp->ieef_ioaddr +
				ieefp->ieef_nid + i);
		}
	}

	/* set the connector/media type if it can be determined */
	macinfo->gldm_media = GLDM_UNKNOWN;

	bcopy((caddr_t)gldbroadcastaddr,
		(caddr_t)macinfo->gldm_broadcast, ETHERADDRL);
	bcopy((caddr_t)macinfo->gldm_vendor,
		(caddr_t)macinfo->gldm_macaddr, ETHERADDRL);

	/*
	 * reset the 82596/82556, to ensure chip is inactive
	 */
	ieef_reset_board(ieefp);

	/* set latched-mode-interrupts option */
	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		uchar_t	tmp;
		tmp = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		tmp |= 0x18;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		ulong_t	tmp;
		tmp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		tmp |= 0x118;
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp);
	}
	WHENUNISYS(ieefp) {
		/* reset any pending interrupt status */
		(void) inb(ieefp->ieef_ioaddr + 0x10);
	}

	ieefp->ieef_ready_intr = 0;	/* Ignore interrupts until ready */

	/*
	 *  Register ourselves with the GLD interface
	 *
	 *  gld_register will:
	 *	link us with the GLD system;
	 *	set our ddi_set_driver_private(9F) data to the macinfo pointer;
	 *	save the devinfo pointer in macinfo->gldm_devinfo;
	 *	map the registers, putting the kvaddr into macinfo->gldm_memp;
	 *	add the interrupt, putting the cookie in gldm_cookie;
	 *	init the gldm_intrlock mutex which will block that interrupt;
	 *	create the minor node.
	 */
	if (gld_register(devinfo, "ieef", macinfo) != DDI_SUCCESS) {
		goto afail;
	}

	/* need to hold the lock to add init commands to device queue */
	mutex_enter(&macinfo->gldm_maclock);

	ieefp->ieef_ready_intr++;

	/*
	 *  Do anything necessary to prepare the board for operation
	 *  Board must be started in order to process config commands
	 */
	if ((ieef_init_board(macinfo)) == DDI_FAILURE) {
		mutex_exit(&macinfo->gldm_maclock);
		gld_unregister(macinfo);
		goto afail;
	}

	/* Make sure we have our address set */
	ieef_start_board_int(macinfo);
	(void) ieef_saddr(macinfo);
	macinfo->gldm_state = IEEF_WAITRCV;

	/*
	 * Initialise the timeout variable to be used for transmit
	 * commands.
	 */
	ieefp->detaching = 0;
	ieefp->wdog_lbolt = 0;
	ieefp->wdog_id = timeout(ieef_wdog, (void *)macinfo, WDOGTICKS);

	mutex_exit(&macinfo->gldm_maclock);

	if (ieefp->ieef_type & IEEF_HW_EE100) {
		if (autospeed)
			ieef_detect_speed(macinfo);
#ifdef IEEFDEBUG
	if (ieefdebug & (IEEFSEND|IEEFRECV)) {
		cmn_err(CE_CONT, "ieef_speed %d", ieefp->ieef_speed);
	}
#endif
	} else
		ieef_detect_media(macinfo);

	return (DDI_SUCCESS);

afail:
	kmem_free(macinfo, sizeof (gld_mac_info_t) +
		sizeof (struct ieefinstance));
	return (DDI_FAILURE);
}

static void
ieef_reset_board(struct ieefinstance *ieefp)
{

	if ((ieefp->ieef_type == IEEF_HW_FLASH) &&
		inb(ieefp->ieef_ioaddr + IEEF_ID0) == 0x55) {
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port, 0);

	} else if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port, 0);
		drv_usecwait(100);
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2, 0);
	}
	WHENUNISYS(ieefp) {
		outb(ieefp->ieef_ioaddr + 0x10, 1);
		outb(ieefp->ieef_ioaddr + 0x10, 0);
	}
	drv_usecwait(100);
}


/* die (undoc'd) -- stop the driver, we are going down */

ieefdie(dev_info_t *devinfo, ddi_reset_cmd_t rst)
{
	gld_mac_info_t *macinfo;		/* GLD structure */
	struct ieefinstance *ieefp;		/* Our private device info */

	rst = rst; /* To make 'make lint' happy */
	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);
	ieefp = (struct ieefinstance *)(macinfo->gldm_private);

	ieef_reset_board(ieefp);
	return (SUCCESS);
}

/*  detach(9E) -- Detach a device from the system */

ieefdetach(dev_info_t *devinfo, ddi_detach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;		/* GLD structure */
	struct ieefinstance *ieefp;		/* Our private device info */

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFDDI)
		cmn_err(CE_CONT, "ieefdetach(0x%x)", (int)devinfo);
#endif

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);
	ieefp = (struct ieefinstance *)(macinfo->gldm_private);

	mutex_enter(&macinfo->gldm_maclock);
	ieefp->detaching = 1;
	mutex_exit(&macinfo->gldm_maclock);

	/* wdog_id could be stale, see Bug #1178973 */
	(void) untimeout(ieefp->wdog_id);

	/* stop the board if it is running */
	ieef_reset_board(ieefp);
	ieef_dealloc_memory(ieefp);

	/*
	 *  Unregister ourselves from the GLD interface
	 *
	 *  gld_unregister will:
	 *	remove the minor node;
	 *	unmap the registers;
	 *	remove the interrupt;
	 *	destroy the gldm_intrlock mutex;
	 *	unlink us from the GLD system.
	 */
	if (gld_unregister(macinfo) == DDI_SUCCESS) {
		kmem_free((caddr_t)macinfo, sizeof (gld_mac_info_t) +
			sizeof (struct ieefinstance));
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 *  GLD Entry Points
 */

/*
 *  ieef_reset() -- reset the board to initial state; restore the machine
 *  address afterwards.
 */

/*ARGSUSED0*/
int
ieef_reset(gld_mac_info_t *macinfo)
{
#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_reset(0x%x)", (int)macinfo);
#endif
	return (SUCCESS);
}

int	ieef_burst = 0x40;		/* XXX disable burst and early ready */
int	ieef_qtime = IEEF_QTIME;	/* XXX set reser timeout to 500ms */

/*
 *  ieef_init_board() -- initialize the specified network board.
 */

int
ieef_init_board(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	ddi_dma_lim_t limits;

	(void) ddi_dmae_getlim(ieefp->ieef_dip, &limits);
	/*
	 * Ignoing return status.  It may fail during initialization, but
	 * it does not impair the driver operation.
	 */
	limits.dlim_minxfer = 0;

	/* struct and dump buffer both need to be padded to 16 bytes */
	if (ddi_iopb_alloc(ieefp->ieef_dip, (ddi_dma_lim_t *)0,
	    sizeof (struct ieef_shmem) + 0xf + 0xf,
	    (caddr_t *)&ieefp->kmem_map_alloced) == DDI_FAILURE)
		return (DDI_FAILURE);

	bzero((caddr_t)ieefp->kmem_map_alloced,
	    sizeof (struct ieef_shmem) + 0xf);

	ieefp->kmem_map
	    = (struct ieef_shmem *)IEEF_ALIGN(ieefp->kmem_map_alloced, 0xf);

	ieefp->pmem_map =  (struct ieef_shmem *)
		(((long)(hat_getkpfnum((caddr_t)ieefp->kmem_map)) *
			(long)ptob(1)) + ((long)ieefp->kmem_map & 0xfff));

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef: shmem V: %p P: %p hg %lx\n",
			(void*)ieefp->kmem_map, (void*)ieefp->pmem_map,
			hat_getkpfnum((caddr_t)ieefp->kmem_map));
#endif

	/* TODO, check location of sysbus, and value */

	ieefp->kmem_map->ieef_scp.scp_sysbus = 4 | 64;

	ieefp->kmem_map->ieef_scp.scp_iscp = &(ieefp->pmem_map->ieef_iscp);
	ieefp->kmem_map->ieef_iscp.iscp_busy = 1;
	ieefp->kmem_map->ieef_iscp.iscp_scb = &(ieefp->pmem_map->ieef_scb);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_NOP;
	ieefp->kmem_map->ieef_scb.scb_cbl = (long)&(ieefp->pmem_map->ieef_cmds);
	ieefp->kmem_map->ieef_scb.scb_rfa = (long)&(ieefp->pmem_map->ieef_rfd);

	ieefp->last_cb = 0xffff;
	ieefp->current_cb = 0xffff;

	ieefp->current_frame = 0;
	ieefp->last_frame = ieefp->ieef_nframes - 1;

	ieefp->mcs_count = 0;

	if ((ieef_alloc_buffers(ieefp)) == DDI_FAILURE) {
		ddi_iopb_free((caddr_t)ieefp->kmem_map_alloced);
		return (DDI_FAILURE);
	}

	ieef_reset_rfa(ieefp);

	/*
	 * Issue PORT command to tell 596 top of memory.  Note that
	 * it must begin on a 16 byte boundary because the first
	 * 4 bits tell the 596 the type of PORT command.
	 */
	if ((ieefp->ieef_type == IEEF_HW_FLASH &&
		inb(ieefp->ieef_ioaddr + IEEF_ID0) == 0x55) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port,
			(long)ieefp->pmem_map | IEEF_NEWSCP);

	} else {
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port,
		    (ushort_t)(((long)ieefp->pmem_map & 0xffff) | IEEF_NEWSCP));
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2,
			(ushort_t)((long)ieefp->pmem_map >> 16));
	}
	CHANNEL_ATTENTION(ieefp);

	/* wait for the init to complete */
	while (ieefp->kmem_map->ieef_iscp.iscp_busy)
		drv_usecwait(100);

	/* enable burst if supported */
#if 0
	if ((ieefp->ieef_type != IEEF_HW_FLASH) &&
		(ieefp->ieef_type != IEEF_HW_EE100_EISA) &&
		(inb(ieefp->ieef_ioaddr + IEEF_PC3) & 0x10) &&
		(ieefp->ieef_type != IEEF_HW_EE100_PCI)) {
		outb(ieefp->ieef_ioaddr + IEEF_PC3,
			PLX_RDBURST | PLX_WRBURST | PLX_ERLYRDY | 0x10);
		outb(ieefp->ieef_ioaddr + IEEF_PC2, 0);
	}
#endif
	if (ieefp->ieef_type == IEEF_HW_EE100_EISA) {
		outb(ieefp->ieef_ioaddr + IEEF_PC3, ieef_burst | PLX_ERLYRDY);
		outb(ieefp->ieef_ioaddr + IEEF_PC2, 0);
	}

	return (DDI_SUCCESS);
}

/*
 *  ieef_start_board_int() -- start the board receiving and allow transmits.
 */

void
ieef_start_board_int(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_start_board_int(0x%x)", (int)macinfo);
#endif

	drv_usecwait(100000);
	if (ieef_configure(ieefp) != SUCCESS)
		cmn_err(CE_WARN, "ieef start board int: configure failed");
	drv_usecwait(100000);

	lock_data_rate(ieefp);
	ieef_start_ru(ieefp);

	/* reset the latch but don't clear pending interrupts */
	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		uchar_t	tmp;
		tmp = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp | 0x30);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp & ~0x30);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		ulong_t	tmp;
		tmp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp | 0x30);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp & ~0x30);
	}
}

void
ieef_start_ru(struct ieefinstance *ieefp)
{
	long	cnt;

	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return;

	ieefp->kmem_map->ieef_scb.scb_rfa = (long)
		&(ieefp->pmem_map->ieef_rfd[ieefp->current_frame]);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_RUC_STRT;
	CHANNEL_ATTENTION(ieefp);

	COMMAND_QUIESCE(ieefp, cnt);
}


/*
 *  ieef_stop_board_int() -- stop board receiving
 */

void
ieef_stop_board_int(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	long	cnt;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_stop_board(0x%x)", (int)macinfo);
#endif

	/* stop the board and disable receiving */
	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return;

	ieefp->kmem_map->ieef_scb.scb_command = SCB_RUC_SUSPND;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp, cnt);
}

/*
 *  ieef_saddr() -- set the physical network address on the board
 */

int
ieef_saddr(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;

	ias_cmd_t icmd;
	caddr_t	a1, a2;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_saddr(0x%x)", (int)macinfo);
#endif

	icmd.ias_status = CS_EL | CS_INT | CS_CMD_IASET;
	a1 = (caddr_t)&(macinfo->gldm_macaddr[0]);
	a2 = (caddr_t)&(icmd.ias_addr[0]);

	bcopy(a1, a2, ETHERADDRL);

	if (ieef_add_command(ieefp, (gen_cmd_t *)&icmd, sizeof (icmd)) == -1) {
		cmn_err(CE_WARN, "ieef: Could not set address.\n");
		return (-1);
	}
	return (0);
}

/*
 *  ieef_dlsdmult() -- set (enable) or disable a multicast address
 *
 *  Program the hardware to enable/disable the multicast address
 *  in "mcast".  Enable if "op" is non-zero, disable if zero.
 */

int
ieef_dlsdmult(gld_mac_info_t *macinfo, struct ether_addr *mcast, int op)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;

	mcs_cmd_t 	mcmd;
	int		i, s;
	int		found = 0;
	int		free_index = -1;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_dlsdmult(0x%x, %s)", (int)macinfo,
		    op ? "ON" : "OFF");
#endif

	/*
	 * GLD maintains a table of per-device multicast addresses and an
	 * associated reference count. This code will be called when a
	 * multicast address that is not already in the table needs to be
	 * programmed or when the reference count for a multicast address
	 * that needs to be disabled goes to zero.
	 */

	/*
	 * Loop through all the previously programmed multicast addresses to
	 * check if the address we wish to enable or disable is already in the
	 * array. Also, mark the first free location in the array.
	 */
	for (s = 0; s < IEEF_NMCS; s++) {
		if (ieefp->mcs_addrs_valid[s] == 0) {
			if (free_index == -1)
				free_index = s;
		} else {
			if (bcmp((caddr_t)ieefp->mcs_addrs[s].ether_addr_octet,
			    (caddr_t)
				mcast->ether_addr_octet, ETHERADDRL) == 0) {
				found = 1;
				break;
			}
		}
	}

	if (!op) {
		/* We want to disable the multicast address. */
		if (found) {
			ieefp->mcs_addrs_valid[s] = 0;
		} else {
			/*
			 * Multicast address not previously programmed, so do
			 * nothing. GLD should prevent this situation from ever
			 * occurring.
			 */
			cmn_err(CE_WARN,
	"ieef_dlsdmult: Cannot remove a non-existing multicast address");
			return (0);
		}
	} else {
		/* We want to enable the multicast address. */
		if (!found) {
			/*
			 * Multicast address not previously programmed, so add
			 * the address to the array & validate the entry.
			 */
			if (free_index == -1) {
				cmn_err(CE_WARN,
		"ieef_dlsdmult: no space left to program multicast address");
				return (0);
			}
			bcopy((caddr_t)(mcast->ether_addr_octet),
			    (caddr_t)
			    (ieefp->mcs_addrs[free_index].ether_addr_octet),
			    ETHERADDRL);
		    ieefp->mcs_addrs_valid[free_index] = 1;
		} else
		    cmn_err(CE_WARN,
		"ieef_dlsdmult: The multicast address is already in the table");
	}

	bzero((caddr_t)&mcmd, sizeof (mcmd));
	mcmd.mcs_status = CS_EL | CS_INT | CS_CMD_MCSET;
	i = 0;
	for (s = 0; s < IEEF_NMCS; s++) {
		if (ieefp->mcs_addrs_valid[s] != 0) {
			bcopy((caddr_t)(ieefp->mcs_addrs[s].ether_addr_octet),
				(caddr_t)&(mcmd.mcs_addr[i * ETHERADDRL]),
				ETHERADDRL);
			i++;
		}
	}
	mcmd.mcs_count = i * ETHERADDRL;
	if (ieef_add_command(ieefp, (gen_cmd_t *)&mcmd, sizeof (mcmd)) == -1) {
		cmn_err(CE_WARN, "!Could not add configure command.\n");
		return (-1);
	}

	return (0);
}

/*
 * ieef_prom() -- set or reset promiscuous mode on the board
 *
 *  Program the hardware to enable/disable promiscuous mode.
 *  Enable if "on" is non-zero, disable if zero.
 */

int
ieef_prom(gld_mac_info_t *macinfo, int on)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_prom(0x%x, %s)", (int)macinfo,
		    on ? "ON" : "OFF");
#endif

	ieefp->promiscuous = (on) ? 1 : 0;
	if (ieef_configure(ieefp) != SUCCESS)
		cmn_err(CE_WARN, "ieef start board int: configure failed");

	return (0);
}

/*
 * ieef_gstat() -- update statistics
 *
 *  GLD calls this routine just before it reads the driver's statistics
 *  structure.  If your board maintains statistics, this is the time to
 *  read them in and update the values in the structure.  If the driver
 *  maintains statistics continuously, this routine need do nothing.
 */

#ifdef IEEFDEBUG
int ieefgerr = 0;
#endif

int
ieef_gstat(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFTRACE)
		cmn_err(CE_CONT, "ieef_gstat(0x%x)", (int)macinfo);
#endif

	macinfo->gldm_stats.glds_crc += ieefp->kmem_map->ieef_scb.scb_crc;
	ieefp->kmem_map->ieef_scb.scb_crc = 0;
	macinfo->gldm_stats.glds_missed += ieefp->kmem_map->ieef_scb.scb_rsc;
	ieefp->kmem_map->ieef_scb.scb_rsc = 0;
	macinfo->gldm_stats.glds_missed += ieefp->kmem_map->ieef_scb.scb_ovr;
	ieefp->kmem_map->ieef_scb.scb_ovr = 0;
	/* nothing to do */
	return (0);
}

/*
 *  ieef_send() -- send a packet
 *
 *  Called when a packet is ready to be transmitted. A pointer to an
 *  M_DATA message that contains the packet is passed to this routine.
 *  The complete LLC header is contained in the message's first message
 *  block, and the remainder of the packet is contained within
 *  additional M_DATA message blocks linked to the first message block.
 *
 *  This routine may NOT free the packet.
 */

int
ieef_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	register int len;
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	int	length = 0;
	xmit_cmd_t xcmd;
	int	s;
	int	i;
	int	offset = 0;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFSEND)
		cmn_err(CE_CONT, "ieef_send(0x%x, 0x%x)", (int)macinfo,
		    (int)mp);
#endif

	/* Get an available TBD */
	s = ieef_avail_tbd(ieefp);

	if (s == -1 || macinfo->gldm_state == IEEF_WDOG) {
		macinfo->gldm_stats.glds_defer++;
		return (1);
	}


	/*
	 *  Load the packet onto the board by chaining through the M_DATA
	 *  blocks of the STREAMS message.  The list of data messages
	 *  ends when the pointer to the current message block is NULL.
	 *
	 *  Note that if the mblock is going to have to * stay around, it
	 *  must be dupmsg() since the caller is going to freemsg() the
	 *  message.
	 */

	len = msgdsize(mp);
	if (len > ieefp->ieef_xbufsize) {
		cmn_err(CE_WARN,
			"ieef: packet size %d greater than framesize\n", len);
		return (0);
	}

	for (i = 0; i < ETHERADDRL; i++)
		xcmd.xmit_dest[i] = 0;
	xcmd.xmit_length = 0;

	ASSERT(mp);

	/* Copy the mblks to the TBD's associated buffers */
	do {
		length = (int)(mp->b_wptr - mp->b_rptr);
		bcopy((caddr_t)mp->b_rptr,
		    (caddr_t)ieefp->kmem_map->ieef_tbd[s].tbd_v_buffer + offset,
		    length);
		mp->b_rptr = mp->b_wptr;
		offset += length;
		mp = mp->b_cont;
	} while (mp != NULL);

	ieefp->kmem_map->ieef_tbd[s].tbd_size = offset | CS_EOF;
	ieefp->kmem_map->ieef_tbd[s].tbd_next = (tbd_t *)0xffffffff;

	/* Form the XMIT command */
	xcmd.xmit_status = CS_EL | CB_SF | CS_INT | CS_CMD_XMIT;
	xcmd.xmit_tbd = &(ieefp->pmem_map->ieef_tbd[s]);
	xcmd.xmit_v_tbd = &(ieefp->kmem_map->ieef_tbd[s]);
	xcmd.xmit_tcb_cnt = 0;

	/*
	 * Pro 100 crc errors fix - set tx_threshold between 192 and 0
	 * (200 is maximum according to pro100 troubleshooting page)
	 */
	if (ieefp->ieef_type & IEEF_HW_EE100)
		xcmd.xmit_tcb_tx_thresh = (len >> 3) & 0xff;
	else
		xcmd.xmit_tcb_tx_thresh = 0;

	/* Add it to the command chain */
	if (ieef_add_command(ieefp, (gen_cmd_t *)&xcmd, sizeof (xcmd)) == -1) {
		macinfo->gldm_stats.glds_defer++;
		ieefp->tbdavail[s] = B_TRUE;
		return (1);
	}

	return (0);		/* successful transmit attempt */
}

/*
 *  ieefintr() -- interrupt from board to inform us that a receive or
 *  transmit has completed.
 */

uint_t
ieefintr(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	ushort_t status;
	uchar_t	pl_stat;
	ulong_t	pl_stat_l;
	long	cnt;

	if (!(ieefp->ieef_ready_intr))
		return (DDI_INTR_UNCLAIMED);
	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		pl_stat = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			pl_stat | 0x30);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		pl_stat_l = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			pl_stat_l | 0x30);
	}
	WHENUNISYS(ieefp) {
		if (inb(ieefp->ieef_ioaddr + 0x10) == 0)
			return (DDI_INTR_UNCLAIMED);
	}

	status = ieefp->kmem_map->ieef_scb.scb_status & SCB_ACK_MSK;

#ifdef IEEFDEBUG
	if (ieefdebug & IEEFINT)
		cmn_err(CE_CONT, "ieefintr(0x%x 0x%x)", (int)macinfo, status);
#endif

	if (status == 0) {
		if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
			(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
			pl_stat = inb(ieefp->ieef_ioaddr +
				ieefp->ieef_int_control);
			pl_stat &= (~0x30);
			outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
				pl_stat);
		} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
			pl_stat_l = inl(ieefp->ieef_ioaddr +
				ieefp->ieef_int_control);
			pl_stat_l &= (~0x30);
			outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
				pl_stat_l);
		}
		return (DDI_INTR_UNCLAIMED);
	}

	macinfo->gldm_stats.glds_intr++;

	while (status != 0) {
		/* reset the interrupt status bit */
		if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
			(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
			pl_stat = inb(ieefp->ieef_ioaddr +
				ieefp->ieef_int_control);
			pl_stat |= 0x10;
			outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
				pl_stat);
		} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
			pl_stat_l = inl(ieefp->ieef_ioaddr +
				ieefp->ieef_int_control);
			pl_stat_l |= 0x10;
			outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
				pl_stat_l);
		}

		/* Inform the board that the interrupt has been received */
		COMMAND_QUIESCE(ieefp, cnt);
		if (cnt <= 0)
			return (DDI_INTR_CLAIMED);

		ieefp->kmem_map->ieef_scb.scb_command = status;
		CHANNEL_ATTENTION(ieefp);

		/* Check for transmit complete interrupt */
		if (status & SCB_ACK_CX)
			(void) ieef_cmd_complete(macinfo, ieefp);

		/* Check for receive completed */
		if (status & SCB_ACK_FR)
			(void) ieef_process_recv(macinfo, ieefp);

		/* Check for Receive No Resources */
		if (status & SCB_ACK_RNR) {
			/*
			 * make certain nothing arrived since last time
			 * I checked
			 */
			(void) ieef_process_recv(macinfo, ieefp);

			/* Resume the receive unit */
			if (!(ieefp->kmem_map->ieef_scb.scb_status &
				SCB_RUS_READY)) {
				ieef_rfa_fix(ieefp);
				ieef_start_ru(ieefp);
			}
#ifdef IEEFDEBUG
			if (ieefdebug & IEEFINT)
				cmn_err(CE_WARN, "ieef: RNR interrupt\n");
#endif
		}

		/* Wait for board to process acks */
		COMMAND_QUIESCE(ieefp, cnt);
		if (cnt <= 0)
			return (DDI_INTR_CLAIMED);

		/* check for any stacked interrupts before exiting */
		status = ieefp->kmem_map->ieef_scb.scb_status & SCB_ACK_MSK;
	}

	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		pl_stat = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat = pl_stat & (~0x30);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		pl_stat_l = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat_l = pl_stat_l & (~0x30);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat_l);
	}

	return (DDI_INTR_CLAIMED);	/* Indicate it was our interrupt */
}

/*
 * Function to allocate buffers used for transmit and receive frames.
 * Also initializes command buffers, and xmit buffers.
 */

int
ieef_alloc_buffers(struct ieefinstance *ieefp)
{
	int	s;
	caddr_t	rcvbuf, xmitbuf, last;
	int	pagesize = ptob(1);

	/*
	 * The 'framesize' property contains the size of the receive frames.
	 * This number should be higher for server machines than it should
	 * on clients.  It must also be a factor of pagesize, so that our
	 * memory management routines can properly keep track of the physical
	 * to virtual page mappings.
	 */
	if (pagesize % ieefp->ieef_framesize) {
		cmn_err(CE_WARN,
			"ieef: Could not attach because framesize not a"
			" factor of pagesize.\n");
		return (DDI_FAILURE);
	}

	if (pagesize % ieefp->ieef_xbufsize) {
		cmn_err(CE_WARN,
			"ieef: Could not attach because xbufsize not a"
			" factor of pagesize.\n");
		return (DDI_FAILURE);
	}

	ieefp->rcvbuf_total = IEEF_ALIGN((ieefp->ieef_nframes) *
		ieefp->ieef_framesize + pagesize - 1,
		pagesize - 1);
	ieefp->rcvbuf_area = kmem_alloc(ieefp->rcvbuf_total, KM_SLEEP);
	if (ieefp->rcvbuf_area == NULL) {
		cmn_err(CE_WARN,
			"ieef: Could not allocate receive buffers\n");
		return (DDI_FAILURE);
	}

	rcvbuf = (caddr_t)IEEF_ALIGN(ieefp->rcvbuf_area + pagesize - 1,
	    pagesize - 1);

	last = rcvbuf;

	for (s = 0; s < (((ieefp->ieef_nframes * ieefp->ieef_framesize)
					+ pagesize - 1)/ pagesize); s++) {
	/* We allocate one page at a time */
		ieefp->rbufsv[s] = last;
		last += pagesize;

		/* We store the physical address */
		ieefp->rbufsp[s] = ((hat_getkpfnum(ieefp->rbufsv[s]) *
			pagesize) + ((long)ieefp->rbufsv[s] & 0xfff));
		ieefp->numrpages = s + 1;
	}

	ieefp->xmitbuf_total = IEEF_ALIGN((ieefp->ieef_xmits) *
		ieefp->ieef_xbufsize + pagesize - 1,
		pagesize - 1);
	ieefp->xmitbuf_area = kmem_alloc(ieefp->xmitbuf_total, KM_SLEEP);
	if (ieefp->xmitbuf_area == NULL) {
		cmn_err(CE_WARN,
			"ieef: Could not allocate transmit buffers\n");
		return (DDI_FAILURE);
	}

	xmitbuf = (caddr_t)IEEF_ALIGN(ieefp->xmitbuf_area + pagesize - 1,
	    pagesize - 1);

	last = xmitbuf;

	/* Allocate the transmit buffers */

	for (s = 0; s < (((ieefp->ieef_xmits * ieefp->ieef_xbufsize)
					+ pagesize - 1)/ pagesize); s++) {

		ieefp->xbufsv[s] = last;
		last += pagesize;

		/* Save the physical addresses */
		ieefp->xbufsp[s] = (hat_getkpfnum(ieefp->xbufsv[s]) * pagesize);
		ieefp->numxpages = s + 1;
	}

	/* Set up the command buffers */
	for (s = 0; s < ieefp->ieef_ncmds; s++)
		ieefp->cmdavail[s] = B_TRUE;

	/* Set up the xmit buffers and tbds */
	for (s = 0; s < ieefp->ieef_xmits; s++) {
		int	bufpag;
		int	bufoff;

		ieefp->tbdavail[s] = B_TRUE;

		bufpag = (s * ieefp->ieef_xbufsize) / ptob(1);
		bufoff = (s * ieefp->ieef_xbufsize) % ptob(1);

		ASSERT(bufpag < ieefp->numxpages);

		ieefp->kmem_map->ieef_tbd[s].tbd_size = 0;
		ieefp->kmem_map->ieef_tbd[s].tbd_next = 0;
		ieefp->kmem_map->ieef_tbd[s].tbd_buffer =
			(ieefp->xbufsp[bufpag]) + bufoff;
		ieefp->kmem_map->ieef_tbd[s].tbd_v_buffer =
			(ieefp->xbufsv[bufpag]) + bufoff;
	}
	return (DDI_SUCCESS);
}

/*
 * Function to set up the Receive Frame Area
 */

void
ieef_reset_rfa(struct ieefinstance *ieefp)
{
	int	s;

	/* Set up the RFDs first */
	for (s = 0; s < (ieefp->ieef_nframes); s++) {
		/*
		 * We use the 'flexible mode' (i.e. all data stored in
		 * the receive buffer and none in the RFD itself)
		 */
		ieefp->kmem_map->ieef_rfd[s].rfd_status = 0;
		ieefp->kmem_map->ieef_rfd[s].rfd_ctlflags = RF_FLEX;

		/* Each RFD points to the next (except 1st, see below) */
		ieefp->kmem_map->ieef_rfd[s].rfd_next =
			&(ieefp->pmem_map->ieef_rfd[s + 1]);

		/*
		 * The 596 will point the RFD to the available RBD
		 * (except 1st, see below)
		 */
		ieefp->kmem_map->ieef_rfd[s].rfd_rbd = (rbd_t *)0xffffffff;

		/* Filled in by the 596 */
		ieefp->kmem_map->ieef_rfd[s].rfd_count = 0;
	}

	/* Special values for the 1st and last RFD */

	/* First RFD points to first RBD */
	ieefp->kmem_map->ieef_rfd[0].rfd_rbd = &(ieefp->pmem_map->ieef_rbd[0]);

	/* Last RFD is flagged as the end of list, for now */
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes - 1].rfd_ctlflags
							= RF_FLEX | RF_EL;

	/* Last RFD points to first RFD */
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes - 1].rfd_next =
		&(ieefp->pmem_map->ieef_rfd[0]);

	/* Now, set up the RBDs */
	for (s = 0; s < (ieefp->ieef_nframes); s++) {
		int	bufpag;
		int	bufoff;

		/* Count is filled in by the 596 */
		ieefp->kmem_map->ieef_rbd[s].rbd_count = 0;

		/* Each RBD points to the next (except 1st, see below) */
		ieefp->kmem_map->ieef_rbd[s].rbd_next =
			&(ieefp->pmem_map->ieef_rbd[s + 1]);

		/*
		 * Figure out the actual physical address of the buffer
		 * associated with this RBD.  Since the buffer size is
		 * a factor of the pagesize, we can simply divide by pagesize
		 * and get the page number, then take the remainder to get
		 * the offset in that page.  We use the mappings we
		 * stored during allocation to convert the page number to
		 * the physical address of the page
		 */
		bufpag = (s * ieefp->ieef_framesize) / ptob(1);
		bufoff = (s * ieefp->ieef_framesize) % ptob(1);

		ASSERT(bufpag < ieefp->numrpages);

		/* Put the physical address in the RBD for the 596 to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_buffer =
			(ieefp->rbufsp[bufpag]) + bufoff;

		/* Put the virtual address in the RBD for *us* to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_v_buffer =
			(ieefp->rbufsv[bufpag]) + bufoff;

		/* Store the framesize in the RBD for the 596 to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_size = ieefp->ieef_framesize;
	}

	/* The last RBD has the EOF Bit set */
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes - 1].rbd_size |= CS_EOF;

	/* The last RBD points to the first */
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes - 1].rbd_next =
		&(ieefp->pmem_map->ieef_rbd[0]);

	ieefp->end_rbd = &(ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes - 1]);
}


/*
 * Function to return the next available TBD.  Returns -1 if none.
 */

int
ieef_avail_tbd(struct ieefinstance *ieefp)
{
	int	s;

	for (s = 0; s < ieefp->ieef_xmits; s++)
		if (ieefp->tbdavail[s])
			break;

	if (s == ieefp->ieef_xmits)
		return (-1);

	ieefp->tbdavail[s] = B_FALSE;

	return (s);
}

/*
 * Function to add a command block to the list of commands to be processed
 */

ieef_add_command(struct ieefinstance *ieefp, gen_cmd_t *cmd, int len)
{
	int	newcmd;
	long	cnt;

	/* All commands must have the interrupt bit and point to -1 (EOL) */
	cmd->cmd_next = (gen_cmd_t *)0xffffffff;
	cmd->cmd_status |= CS_INT;

	/* If our pointer to the last command block is (short)-1, it is empty */
	if (ieefp->last_cb == 0xffff) {
		/*
		 * Since the list is empty, we will use the first
		 * command buffer
		 */
		ieefp->last_cb = 0;
		ieefp->current_cb = 0;

		/* Mark it as busy */
		ieefp->cmdavail[0] = B_FALSE;

		/* Copy the data to the buffer */
		bcopy((caddr_t)cmd, (caddr_t)&(ieefp->kmem_map->ieef_cmds[0]),
			len);

		/* Wait for the 596 to complete whatever command is current */
		COMMAND_QUIESCE(ieefp, cnt);
		if (cnt <= 0)
			return (-1);

		/* Point the SCB to command area */
		ieefp->kmem_map->ieef_scb.scb_cbl =
			(long)&(ieefp->pmem_map->ieef_cmds[0]);
		ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_STRT;

		/* Before we issue the channel attention, we record the	*/
		/* lbolt value.						*/
		(void) drv_getparm(LBOLT, &ieefp->wdog_lbolt);

		CHANNEL_ATTENTION(ieefp);

	} else {
		/*
		 * Find an available command block.  If none available, wait
		 * for one.
		 */
		if ((newcmd = ieef_avail_cmd(ieefp)) == -1) {
			return (-1);
		}

		/* Copy the command into the next command buffer */
		bcopy((caddr_t)cmd,
			(caddr_t)&(ieefp->kmem_map->ieef_cmds[newcmd]), len);

		/* Point the currently last command to this one */
		ieefp->kmem_map->ieef_cmds[ieefp->last_cb].cmd_next =
			&(ieefp->pmem_map->ieef_cmds[newcmd]);

		/* This is the new last command */
		ieefp->last_cb = newcmd;
	}

	return (0);
}

/*
 * Function called at interrupt time when the interrupt handler determines
 * that the 596 has signaled that a command has completed.  This could
 * be due to a transmit command, or any other command.  If it is a
 * transmit command completion, we must do other processing.
 */

ieef_cmd_complete(gld_mac_info_t *macinfo, struct ieefinstance *ieefp)
{
	int	cmd;
	long	cnt;

	/* Is there even a command pending? */
	if (ieefp->current_cb == 0xffff) {
		return (SUCCESS);
	}

	/* Ignore this interrupt if the current command not marked complete */
	if (!(ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_status &
	    CS_CMPLT))
		return (SUCCESS);

	/* Get the command type from the actual command block */
	cmd = ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_status;
#ifdef IEEFDEBUG
	if (ieefdebug & IEEFRECV)
		cmn_err(CE_CONT,
			"ieefintr: command complete, command %x\n", cmd);
#endif

	/* Check if it is a transmit command */
	if ((cmd & CS_CMD_MSK) == CS_CMD_XMIT)
		(void) ieef_xmit_complete(macinfo, ieefp);

	/*
	 * Done processing command.  We now re-arrange the command buffers
	 * and pointers to show that this one is free
	 */

	/* Mark it as available */
	ieefp->cmdavail[ieefp->current_cb] = B_TRUE;

	/* Is this the last block on the chain? */
	if (ieefp->current_cb == ieefp->last_cb) {
		ieefp->last_cb = ieefp->current_cb = 0xffff;
		ieefp->wdog_lbolt = 0;
	}

	/* If there are sill more blocks... */
	if (ieefp->current_cb != 0xffff) {
		volatile gen_cmd_t	*cur;

		/* Make the next one the current one */
		cur = ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_next;
		ieefp->current_cb = cur - &ieefp->pmem_map->ieef_cmds[0];

		/* Start the command unit again */
		COMMAND_QUIESCE(ieefp, cnt);
		if (cnt <= 0)
			return (FAILURE);

		ieefp->kmem_map->ieef_scb.scb_cbl = (long)cur;
		ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_STRT;

		/* Before we issue the channel attention, record the	*/
		/* lbolt value.						*/
		(void) drv_getparm(LBOLT, &ieefp->wdog_lbolt);

		CHANNEL_ATTENTION(ieefp);

#ifdef IEEFDEBUG
		if (ieefdebug & IEEFRECV)
			cmn_err(CE_CONT, "ieefintr: NXT CMD(%x)\n", cmd);
#endif
	}
#ifdef IEEFDEBUG
	else if (ieefdebug & IEEFRECV)
		cmn_err(CE_CONT, "ieefintr: EMPTY(%x)\n", cmd);
#endif
	return (SUCCESS);
}

/*
 * A transmit command has completed, mark the buffer as available,
 * and keep statistics.
 */

int
ieef_xmit_complete(gld_mac_info_t *macinfo, struct ieefinstance *ieefp)
{
	volatile tbd_t	*tbd;
	int	tbdnum;
	xmit_cmd_t *xcmd;

	xcmd = (xmit_cmd_t *)&(ieefp->kmem_map->ieef_cmds[ieefp->current_cb]);
	if (xcmd->xmit_status & CB_SF) {
		tbd = xcmd->xmit_v_tbd;
		tbdnum = tbd - &ieefp->kmem_map->ieef_tbd[0];
		ieefp->tbdavail[tbdnum] = B_TRUE;
	}

	if (xcmd->xmit_status & XMIT_ERRS) {
		macinfo->gldm_stats.glds_errxmt++;
		if (xcmd->xmit_status & XERR_UND)
			macinfo->gldm_stats.glds_underflow++;
		if (xcmd->xmit_status & XERR_NC)
			macinfo->gldm_stats.glds_nocarrier++;
		if (xcmd->xmit_status & XERR_COLL)
			macinfo->gldm_stats.glds_excoll++;
	}
	return (SUCCESS);
}

ieef_re_q(struct ieefinstance *ieefp, rfd_t *rfd)
{
	rfd_t	*lrfd;
	rbd_t	*rbd;
	long	 fsize;

	if (rfd->rfd_rbd == (rbd_t *)0xffffffff)
		goto no_rbd;

	rbd = (rbd_t *)IEEF_PTOV(ieefp, rfd->rfd_rbd);
	fsize = ieefp->ieef_framesize;

	for (;;) {
		if ((rbd->rbd_count & CS_EOF) || (rbd->rbd_size & CS_EOF))
			break;

		/* Mark this RBD as empty and available */
		rbd->rbd_size = fsize;
		rbd->rbd_count = 0;

		rbd = (rbd_t *)IEEF_PTOV(ieefp, rbd->rbd_next);
	}

	/* Make this RBD the new last one */
	rbd->rbd_size = fsize | CS_EOF;
	rbd->rbd_count = 0;

	/* Clear the EOF bit from the former 'last' rbd */
	ieefp->end_rbd->rbd_size = fsize;
	ieefp->end_rbd = rbd;

	/*
	 * Done processing this frame, make it the new last frame
	 */
no_rbd:

	rfd->rfd_status = 0;
	rfd->rfd_ctlflags = RF_FLEX | RF_EL;
	rfd->rfd_rbd = (rbd_t *)0xffffffff;

	lrfd = &(ieefp->kmem_map->ieef_rfd[ieefp->last_frame]);

	/* Set the former last frame to not be last */
	lrfd->rfd_ctlflags &= ~(RF_EL);
	return (SUCCESS);
}


/*
 * Function called from interrupt level when a frame has been received.
 */

ieef_process_recv(gld_mac_info_t *macinfo, struct ieefinstance *ieefp)
{
	mblk_t	*mp;
	rbd_t	*rbd;
	rbd_t	*first_rbd;
	rfd_t	*rfd;
	int	len;
	caddr_t	dp;
	int	count;
	ushort_t status;

	/* Start at the current frame */
	rfd = &(ieefp->kmem_map->ieef_rfd[ieefp->current_frame]);

	/* If it is not marked as complete, ignore it and just return */
	if (!(rfd->rfd_status & CS_CMPLT)) {
		return (SUCCESS);
	}

	while (rfd->rfd_status & CS_CMPLT) {
		status = rfd->rfd_status;
		mp = NULL;
		if ((rfd->rfd_status & CS_OK) == 0) {
			cmn_err(CE_CONT, "?ieef: frame not ok status=0x%x\n",
				rfd->rfd_status);
			goto frame_done;
		}
		if (rfd->rfd_rbd == (rbd_t *)0xffffffff) {
			cmn_err(CE_CONT,
				"?ieef: rfd_rbd invalid status=0x%x\n",
				rfd->rfd_status);
			goto frame_done;
		}
		if ((ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
			(ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
			if (status & RFD_LENGTH_ERROR) {
				length_error_cnt++;
				macinfo->gldm_stats.glds_errrcv++;
			}
			if (status & RFD_CRC_ERROR) {
				crc_error_cnt++;
				macinfo->gldm_stats.glds_crc++;
			}
			if (status & RFD_ALIGNMENT_ERROR) {
				alignment_error_cnt++;
				macinfo->gldm_stats.glds_errrcv++;
			}
			if (status & RFD_NO_RESOURCES) {
				no_resources_cnt++;
				macinfo->gldm_stats.glds_norcvbuf++;
			}
			if (status & RFD_DMA_OVERRUN) {
				dma_overrun_cnt++;
				macinfo->gldm_stats.glds_errrcv++;
			}
			if (status & RFD_FRAME_TOO_SHORT) {
				frame_too_short_cnt++;
				macinfo->gldm_stats.glds_short++;
			}
			if (status & RFD_NO_EOP_FLAG) {
				no_eop_flag_cnt++;
				macinfo->gldm_stats.glds_errrcv++;
			}
			if (status & RFD_RECEIVE_COLLISION) {
				receive_collision_cnt++;
				macinfo->gldm_stats.glds_errrcv++;
			}
			if (status & (RFD_CRC_ERROR | RFD_ALIGNMENT_ERROR |
				RFD_NO_RESOURCES | RFD_DMA_OVERRUN |
				RFD_FRAME_TOO_SHORT | RFD_NO_EOP_FLAG |
				RFD_RECEIVE_COLLISION)) {
			cmn_err(CE_CONT, "ieef_process_recv(): status=0x%x\n",
				status);
				goto frame_done;
			}
		}

		first_rbd = (rbd_t *)IEEF_PTOV(ieefp, rfd->rfd_rbd);
		rbd = first_rbd;

		len = 0;
		for (;;) {
			len += rbd->rbd_count & CS_RBD_CNT_MSK;

			if ((rbd->rbd_count & CS_EOF) ||
				(rbd->rbd_size & CS_EOF))
				break;

			rbd = (rbd_t *)IEEF_PTOV(ieefp, rbd->rbd_next);
		}

		/* Allocate an mblk big enough for the frame */
		if ((mp = allocb(len, BPRI_MED)) == NULL) {
			macinfo->gldm_stats.glds_norcvbuf++;
			goto frame_done;
		}
		/* Get the first RBD associated with this frame */
		rbd = first_rbd;
		for (;;) {
			/* Copy this RBD to the mblk */
			count = rbd->rbd_count & CS_RBD_CNT_MSK;
			dp = (caddr_t)mp->b_wptr;
			mp->b_wptr = mp->b_wptr + count;

			/*
			 * This should never happen.  Fix put in for
			 * machines with cache memory corruption.
			 */
			if (!((mp->b_wptr - mp->b_rptr) > len)) {
				bcopy(rbd->rbd_v_buffer, dp, count);
			}

			/*
			 * If CS_EL is set, then we have reached the end
			 * of the list.  If CS_EOF is set, then we have
			 * reached the end of the frame
			 */
			if ((rbd->rbd_size & CS_EOF) ||
				(rbd->rbd_count & CS_EOF))
				break;

			/* Get next RBD */
			rbd = (rbd_t *)IEEF_PTOV(ieefp, rbd->rbd_next);
		}
	frame_done:
		/* add this rfd and its rbd's to the free list */
		(void) ieef_re_q(ieefp, rfd);

		/* Go on to next frame */
		ieefp->last_frame = ieefp->current_frame;
		if (ieefp->current_frame == (ieefp->ieef_nframes - 1))
			ieefp->current_frame = 0;
		else
			ieefp->current_frame++;
		rfd = &(ieefp->kmem_map->ieef_rfd[ieefp->current_frame]);

		/* Send the frame up to the gld layer */
		if (mp != NULL)
			gld_recv(macinfo, mp);
	}
	return (SUCCESS);
}

ieef_configure_flash32(struct ieefinstance *ieefp)
{
	conf32_cmd_t ccmd;

	bzero((caddr_t)&ccmd, sizeof (conf32_cmd_t));
	ccmd.conf_status = CS_EL | CS_INT | CS_CMD_CONF;

	/* Default configuration */
	ccmd.conf_conf.cnf_fifo_byte = 0xf80e;
	ccmd.conf_conf.cnf_add_mode = 0x2e40;
	ccmd.conf_conf.cnf_pri_data = 0x6000;
	ccmd.conf_conf.cnf_slot = 0xf200;

	/* Set promiscuous mode as appropriate */
	ccmd.conf_conf.cnf_hrdwr = (0x0 | ((ushort_t)ieefp->promiscuous));
	ccmd.conf_conf.cnf_min_len = 0xc040;
	ccmd.conf_conf.cnf_more = 0x3f00;

	if (ieef_add_command(ieefp, (gen_cmd_t *)&ccmd, sizeof (ccmd)) == -1) {
		cmn_err(CE_WARN, "!Could not add configure command.\n");
		return (-1);
	}
	return (SUCCESS);
}


ieef_configure_ee100(struct ieefinstance *ieefp)
{

	conf100_cmd_t ccmd;

	ccmd.conf_status = CS_EL | CS_INT | CS_CMD_CONF;

	ccmd.conf_conf.conf_bytes01 = CB_556_CFIG_DEFAULT_PARM0_1;
	ccmd.conf_conf.conf_bytes23 = CB_556_CFIG_DEFAULT_PARM2_3;
	ccmd.conf_conf.conf_bytes45 = CB_556_CFIG_DEFAULT_PARM4_5;
	ccmd.conf_conf.conf_bytes67 = CB_556_CFIG_DEFAULT_PARM6_7;
	ccmd.conf_conf.conf_bytes89 = CB_556_CFIG_DEFAULT_PARM8_9;
	ccmd.conf_conf.conf_bytes1011 = (CB_556_CFIG_DEFAULT_PARM10_11 |
		(ieefp->promiscuous << 8));
	ccmd.conf_conf.conf_bytes1213 = CB_556_CFIG_DEFAULT_PARM12_13;
	ccmd.conf_conf.conf_bytes1415 = CB_556_CFIG_DEFAULT_PARM14_15;
	ccmd.conf_conf.conf_bytes1617 = CB_556_CFIG_DEFAULT_PARM16_17;
	ccmd.conf_conf.conf_bytes1819 = CB_556_CFIG_DEFAULT_PARM18_19;

	if (ieefp->ieef_speed == 10)
		ccmd.conf_conf.conf_bytes45 &= ~BIT_0;
	else
		ccmd.conf_conf.conf_bytes45 |= BIT_0;

	if (ieef_add_command(ieefp, (gen_cmd_t *)&ccmd, sizeof (ccmd)) == -1) {
		cmn_err(CE_WARN, "!Could not add configure command.\n");
		return (-1);
	}
	return (SUCCESS);
}

/* Set up a configure command */
static int
ieef_configure(struct ieefinstance *ieefp)
{
	int	rc, cnt;

	/*
	 * load TON-TOFF timer values with values recommended by Intel
	 */

	ieefp->kmem_map->ieef_scb.scb_timer = 0x28 << 16 | 0x8;
	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return (FAILURE);

	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_TIMERS;
	CHANNEL_ATTENTION(ieefp);

	if (ieefp->ieef_type & IEEF_HW_EE100)
		rc = ieef_configure_ee100(ieefp);
	else
		rc = ieef_configure_flash32(ieefp);
	COMMAND_QUIESCE(ieefp, cnt);
	return (rc);
}

/*
 * Function to deallocate all of the memory allocated (for detach time)
 */

void
ieef_dealloc_memory(struct ieefinstance *ieefp)
{
	ddi_iopb_free((caddr_t)ieefp->kmem_map_alloced);

	ieef_rbuf_free(ieefp);
	ieef_xmit_free(ieefp);
}

/*
 * Function to find an available command buffer.  Returns -1 if none.
 * Assumes cmdlock already taken.
 */

ieef_avail_cmd(struct ieefinstance *ieefp)
{
	int	s;

	for (s = 0; s < ieefp->ieef_ncmds; s++)
		if (ieefp->cmdavail[s] == B_TRUE)
			break;

	if (s == ieefp->ieef_ncmds)
		return (-1);

	ieefp->cmdavail[s] = B_FALSE;

	return (s);
}

/*
 * Function to deallocate allocated receive buffers. Can be used
 * in the case where some were allocated, but we ran out
 * of resources in the middle
 */

void
ieef_rbuf_free(struct ieefinstance *ieefp)
{
	kmem_free(ieefp->rcvbuf_area, ieefp->rcvbuf_total);
}

/*
 * Same function for xmit buffers
 */

void
ieef_xmit_free(struct ieefinstance *ieefp)
{
	kmem_free(ieefp->xmitbuf_area, ieefp->xmitbuf_total);
}

void
ieef_rfa_fix(struct ieefinstance *ieefp)
{
	/*
	 * Reset the RFA.  Since we have at least one RBD per RFD,
	 * we cannot run out of RFDs and still have RBDs.  If we simply
	 * reset the beginning and end pointers, and set the first
	 * RFD to point to the first RBD, we should be ready to start
	 * again.  We do not have to re-set-up the whole RFA.
	 */
	ieefp->end_rbd->rbd_size = ieefp->ieef_framesize;
	ieefp->kmem_map->ieef_rfd[ieefp->last_frame].rfd_ctlflags = RF_FLEX;
	ieefp->kmem_map->ieef_rfd[ieefp->last_frame].rfd_status = 0;

	ieefp->current_frame = 0;
	ieefp->last_frame = ieefp->ieef_nframes - 1;
	ieefp->end_rbd = &(ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes - 1]);

	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes - 1].rfd_ctlflags =
		RF_FLEX | RF_EL;
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes - 1].rfd_status = 0;

	ieefp->kmem_map->ieef_rfd[0].rfd_rbd =
		(rbd_t *)&(ieefp->pmem_map->ieef_rbd[0]);
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes - 1].rbd_size =
		ieefp->ieef_framesize | CS_EOF;
	ieefp->kmem_map->ieef_scb.scb_rfa = (long)
					&(ieefp->pmem_map->ieef_rfd[0]);
}

/*
 *  ieef_start_board() -- start the board receiving and allow transmits.
 */

ieef_start_board(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	long	cnt;

	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return (FAILURE);

	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_RSUM;
	CHANNEL_ATTENTION(ieefp);

	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return (FAILURE);

	if (!(ieefp->kmem_map->ieef_scb.scb_status & SCB_RUS_READY))
		ieef_start_ru(ieefp);
	return (SUCCESS);
}


int
ieef_stop_board(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =	/* Our private device info */
		(struct ieefinstance *)macinfo->gldm_private;
	long	cnt;

	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return (FAILURE);

	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_SUSPND;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp, cnt);
	if (cnt <= 0)
		return (FAILURE);

	return (SUCCESS);
}

short
ieef_type(dev_info_t *devinfo)
{
	short type;
	int base_io_address;
	struct {
		int bustype;
		int base;
		int size;
	} *reglist;
	int nregs, reglen, i;


	if (!(ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
		"ignore-hardware-nodes", 0))) {
		if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo,
		    DDI_PROP_DONTPASS, "reg", (caddr_t)&reglist,
		    &reglen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "ieef: reg property not in devices property list");
		}
		nregs = reglen / sizeof (*reglist);
		for (i = 0; i < nregs; i++)
			if (reglist[i].bustype == 1) {
				base_io_address = reglist[i].base;
				break;
			}
		kmem_free(reglist, reglen);
	} else {
		base_io_address = ddi_getprop(DDI_DEV_T_ANY, devinfo,
		    DDI_PROP_DONTPASS, "ioaddr", 0);
	}

	if (ieef_bus_type_check(devinfo) == IEEF_BUS_PCI)
		type = IEEF_HW_EE100_PCI;
	else if (base_io_address == 0x300)
		type = IEEF_HW_UNISYS;
	else if (((inb(base_io_address + IEEF_ID0)) == 0x25) &&
		((inb(base_io_address + IEEF_ID1)) == 0xd4) &&
		((inb(base_io_address + IEEF_ID2)) == 0x10) &&
		((inb(base_io_address + IEEF_ID3)) == 0x60))
		type = IEEF_HW_EE100_EISA;
	else type = IEEF_HW_FLASH;
	return (type);
}

void
ieef_port_ca_nid(struct ieefinstance *ieefp)
{
	switch (ieefp->ieef_type) {
	case IEEF_HW_UNISYS:
		ieefp->ieef_port = IEEF_UNISYS_PORT;
		ieefp->ieef_ca = IEEF_UNISYS_CA;
		ieefp->ieef_nid = IEEF_UNISYS_NID;
		ieefp->ieef_user_pins = PLXE_REGISTER_1;
		break;
	case IEEF_HW_FLASH:
	case IEEF_HW_EE100_EISA:
		ieefp->ieef_port = PLXE_PORT_OFFSET;
		ieefp->ieef_ca = PLXE_CA_OFFSET;
		ieefp->ieef_nid = PLXE_ADDRESS_PROM;
		ieefp->ieef_int_control = FL32_EXTEND_INT_CONTROL;
		ieefp->ieef_user_pins = PLXE_REGISTER_1;
		break;
	case IEEF_HW_EE100_PCI:
		ieefp->ieef_port = PLXP_PORT_OFFSET;
		ieefp->ieef_ca = PLXP_CA_OFFSET;
		ieefp->ieef_nid = PLXP_NODE_ADDR_REGISTER;
		ieefp->ieef_int_control = PLXP_INTERRUPT_CONTROL;
		ieefp->ieef_user_pins = PLXP_USER_PINS;
		break;
	default:
		cmn_err(CE_WARN, "ieef_port_ca_nid(): illeagle type (%d).\n",
			ieefp->ieef_type);
	}
}

void
lock_data_rate(struct ieefinstance *ieefp)
{
	uchar_t	portbyte;
	ulong_t	portword;

	if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		portword = 0;
		portword |= 0xf0;
		portword |= BIT_0;
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portword);
		drv_usecwait(10000);

		portword = inl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portword &= ~4;
		portword |= 2;
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portword);
		drv_usecwait(10000);
		portword = inl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portword &= ~2;
		portword |= 4;
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portword);
		drv_usecwait(10000);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_EISA) {
		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte |= 1;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portbyte);
		drv_usecwait(10000);

		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte &= ~4;
		portbyte |= 2;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portbyte);
		drv_usecwait(10000);
		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte &= ~2;
		portbyte |= 4;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
			portbyte);
		drv_usecwait(10000);
	}
}

void
ieef_wdog(void *arg)
{
	gld_mac_info_t *macinfo = (gld_mac_info_t *)arg;
	struct ieefinstance
		*ieefp = (struct ieefinstance *)macinfo->gldm_private;
	unsigned long	val;
	int 		locked = 0;
	int		ostate;


	/*
	 * This is the watchdog timer handler routine which is called when
	 * the watchdog timer expires. The timer is started when a command
	 * is added to the command queue. If TIMEOUT time has elapsed when
	 * the watchdog routine is scheduled this means that the command has
	 * hung and the 82596 must be reset to get it out of the wedged state.
	 * Bug ID: 1160936
	 */

	/*
	 * if a recursive call to wdog reset routine - give up
	 */
	if (mutex_tryenter(&ieef_wdog_lock) == 0)
		return;

	/*
	 * lock driver mutex
	 */
	locked = mutex_tryenter(&macinfo->gldm_maclock);

	/*
	 * remove any pending watchdog timeout
	 */
	(void) untimeout(ieefp->wdog_id);

	(void) drv_getparm(LBOLT, &val);

	if ((val-ieefp->wdog_lbolt > TIMEOUT) && (ieefp->wdog_lbolt != 0)) {

		/* Enter watchdog reset state. */
		ostate = macinfo->gldm_state;
		macinfo->gldm_state = IEEF_WDOG;

		ieef_wreset(macinfo);

		/* Leave wdog reset state. */
		macinfo->gldm_state = ostate;
	}

	if (!ieefp->detaching)
	    ieefp->wdog_id = timeout(ieef_wdog, (void *)macinfo, WDOGTICKS);

	/* Release the mutex which was held for initialisation.	*/
	if (locked)
		mutex_exit(&macinfo->gldm_maclock);
	mutex_exit(&ieef_wdog_lock);
}

/*
 * set speed to 10 and attempt to send a test packet
 * if test fails, default to 100
 */
static void
ieef_detect_speed(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =
		(struct ieefinstance *)macinfo->gldm_private;

	if (ieef_send_test(macinfo) == SUCCESS)
		return;

	mutex_enter(&macinfo->gldm_maclock);
	ieefp->ieef_speed = ieefp->ieef_speed == 10 ? 100 : 10;
	ieef_wreset(macinfo);
	mutex_exit(&macinfo->gldm_maclock);
}        

static void
ieef_set_media(gld_mac_info_t *macinfo, uchar_t bits)
{
	struct ieefinstance *ieefp =
		(struct ieefinstance *)macinfo->gldm_private;
	uchar_t portbyte;

	mutex_enter(&macinfo->gldm_maclock);
	portbyte = inb(ieefp->ieef_ioaddr+ieefp->ieef_user_pins);
	portbyte &= 0xFC;
	portbyte |= (bits & 3);
	outb(ieefp->ieef_ioaddr+ieefp->ieef_user_pins, portbyte);
	mutex_exit(&macinfo->gldm_maclock);
	/* Allow selected transceiver time to initialize */
	drv_usecwait(2000);
}

static int
ieef_send_test(gld_mac_info_t *macinfo)
{
	int	length = 0, status;
	xmit_cmd_t	xcmd;
	int	i;
	struct ieefinstance *ieefp =
	    (struct ieefinstance *)macinfo->gldm_private;

	/* Form the XMIT command (simplified mode) */
	xcmd.xmit_status = CS_EL | CS_CMD_XMIT;
	xcmd.xmit_tbd = (tbd_t *)0xffffffff;
	xcmd.xmit_tcb_cnt = sizeof (struct ether_header) | CS_EOF;
	xcmd.xmit_length = 0;
	bcopy((caddr_t)macinfo->gldm_macaddr,
	    (caddr_t)&xcmd.xmit_dest[0], ETHERADDRL);

	if (ieefp->ieef_type & IEEF_HW_EE100)
		xcmd.xmit_tcb_tx_thresh = (length >> 3) & 0xff;
	else
		xcmd.xmit_tcb_tx_thresh = 0;

	/* make sure we are the first command to be executed */
	for (i = 0; i < 50; i++) {
		mutex_enter(&macinfo->gldm_maclock);
		if (ieefp->current_cb == 0xffff)
			break;
		mutex_exit(&macinfo->gldm_maclock);
		delay(drv_usectohz(1000));
	}

	if (i == 50)
		return (-1);

	/* Add it to the command chain */
	if (ieef_add_command(ieefp, (gen_cmd_t *)&xcmd, sizeof (xcmd)) == -1) {
		mutex_exit(&macinfo->gldm_maclock);
		return (-1);
	}

	i = 50;
	while (--i) {
		status = ieefp->kmem_map->ieef_cmds[0].cmd_status;
		if (status & CS_CMPLT)
			break;
		drv_usecwait(100);
	}

	if (i && status & CS_OK) {
		mutex_exit(&macinfo->gldm_maclock);
		return (SUCCESS);
	}

	mutex_exit(&macinfo->gldm_maclock);
	return (-1);
}

static void
ieef_detect_media(gld_mac_info_t *macinfo)
{
	ieef_set_media(macinfo, IEEF_MEDIA_BNC);
	if (ieef_send_test(macinfo) == SUCCESS)
		return;
	ieef_set_media(macinfo, IEEF_MEDIA_AUI);
	if (ieef_send_test(macinfo) == SUCCESS)
		return;
	ieef_set_media(macinfo, IEEF_MEDIA_TP);
}


void
ieef_wreset(gld_mac_info_t *macinfo)
{
	struct ieefinstance *ieefp =
		(struct ieefinstance *)macinfo->gldm_private;
	int		cmd;
	volatile tbd_t	*tbd;
	int		tbdnum;
	int		i, s;
	xmit_cmd_t	*xcmd;
	mcs_cmd_t	mcmd;

#ifdef IEEFDEBUG
	if (ieefdebug) {
		ulong_t		portcmd;
		ulong_t		*dumpb;

		/* Dump port command to /var/adm/messages. */
		portcmd = (ulong_t)IEEF_ALIGN(ieefp->pmem_map->ieef_dump_buffer,
			0xF);
		portcmd |= 3;			/* "dump" port cmd */

		bzero((caddr_t)ieefp->kmem_map->ieef_dump_buffer,
			IEEF_DUMP_BUFFER_SIZE);
		dumpb = (ulong_t *)IEEF_ALIGN(ieefp->kmem_map->ieef_dump_buffer,
			0xF);

		if ((ieefp->ieef_type == IEEF_HW_FLASH &&
		    inb(ieefp->ieef_ioaddr + IEEF_ID0) == 0x55) ||
		    (ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
		    (ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
			outl(ieefp->ieef_ioaddr + ieefp->ieef_port, portcmd);
		} else {
			outw(ieefp->ieef_ioaddr + ieefp->ieef_port,
				(ushort_t)portcmd);
			outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2,
				(ushort_t)(portcmd >> 16));
		}
		drv_usecwait(1000);

		cmn_err(CE_CONT, "ieef reset: dump 0x%lx at 0x%p\n", portcmd,
			(void *)dumpb);

		dumpb = (ulong_t *)(ieefp->kmem_map);
		for (i = 0; i <= (sizeof (struct ieef_shmem) + 0xf + 0xf)/4;
		    i += 16) {
			for (s = 0; s < 16 && !dumpb[i + s]; s++)
				;
			if (s >= 16)
				continue;
			cmn_err(CE_CONT,
		    "!%p: %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx",
			    (void*)&dumpb[i], dumpb[i], dumpb[i+1], dumpb[i+2],
			    dumpb[i+3], dumpb[i+4], dumpb[i+5], dumpb[i+6],
			    dumpb[i+7], dumpb[i+8], dumpb[i+9], dumpb[i+10],
			    dumpb[i+11], dumpb[i+12]);
			cmn_err(CE_CONT, "%lx %lx %lx\n", dumpb[i+13],
			    dumpb[i+14], dumpb[i+15]);
		}
	}
#endif

	/* Reset the 82596 chip. */
	ieef_reset_board(ieefp);

	/* set latched-mode-interrupts option */
	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		uchar_t	tmp;
		tmp = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		tmp |= 0x18;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		ulong_t	tmp;
		tmp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		tmp |= 0x118;
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, tmp);
	}
	WHENUNISYS(ieefp) {
		/* reset any pending interrupt status */
		(void) inb(ieefp->ieef_ioaddr + 0x10);
	}

	(void) ieef_reset_rfa(ieefp);

	/*
	 * Set up the pointers so that the 82596 initialises
	 * correctly. All values not re-initialised here have
	 * already been set up in the ieefattach routine.
	 */
	ieefp->kmem_map->ieef_iscp.iscp_busy = 1;
	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_NOP;

	/* Send a PORT command to tell the 82596 the top of memory */
	if ((ieefp->ieef_type == IEEF_HW_FLASH &&
	    inb(ieefp->ieef_ioaddr + IEEF_ID0) == 0x55) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port,
		    (long)ieefp->pmem_map | IEEF_NEWSCP);
	} else {
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port,
		    (ushort_t)(((long)ieefp->pmem_map & 0xffff) |
			IEEF_NEWSCP));
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2,
		    (ushort_t)((long)ieefp->pmem_map >> 16));
	}

	/*
	 * Issue a CHANNEL_ATTENTION to 82596. The first channel
	 * attention after a reset causes the 82596 to go through
	 * its initialisation sequence.
	 */
	CHANNEL_ATTENTION(ieefp);

	/* Wait for the init to complete. */
	while (ieefp->kmem_map->ieef_iscp.iscp_busy)
		drv_usecwait(100);

	/* enable burst if supported */
	if (ieefp->ieef_type == IEEF_HW_EE100_EISA) {
		outb(ieefp->ieef_ioaddr + IEEF_PC3,
			ieef_burst|PLX_ERLYRDY);
		outb(ieefp->ieef_ioaddr + IEEF_PC2, 0);
	}

	/*
	 * We are going to wipe out all the existing commands on the
	 * queue. For each transmit command, we need to free the
	 * transmit buffer associated with it.
	 */

	for (i = 0; i < ieefp->ieef_ncmds; i++) {
		ieefp->cmdavail[i] = B_TRUE;
		cmd = ieefp->kmem_map->ieef_cmds[i].cmd_status;
		if ((cmd & CS_CMD_MSK) == CS_CMD_XMIT) {
			xcmd = (xmit_cmd_t *)&(ieefp->kmem_map->ieef_cmds[i]);
			if (xcmd->xmit_status & CB_SF) {
				tbd = xcmd->xmit_v_tbd;
				tbdnum = tbd - &ieefp->kmem_map->ieef_tbd[0];
				ieefp->tbdavail[tbdnum] = B_TRUE;
			}
		}
	}

	/* Re-initialise variables. */
	ieefp->current_cb = ieefp->last_cb = 0xffff;
	ieefp->wdog_lbolt = 0;

	/* Do all that is neccessary to get the board going. */
	drv_usecwait(10000);
	if (ieef_configure(ieefp) != SUCCESS)
		cmn_err(CE_WARN, "ieef start board int: configure failed");
	drv_usecwait(10000);
	ieef_rfa_fix(ieefp);
	lock_data_rate(ieefp);
	ieef_start_ru(ieefp);

	/* reset the latch but don't clear pending interrupts */
	if ((ieefp->ieef_type == IEEF_HW_FLASH) ||
		(ieefp->ieef_type == IEEF_HW_EE100_EISA)) {
		uchar_t	tmp;
		tmp = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			tmp | 0x30);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			tmp & ~0x30);
	} else if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		ulong_t	tmp;
		tmp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			tmp | 0x30);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			tmp & ~0x30);
	}

	(void) ieef_saddr(macinfo);

	/*
	 * If multicast addresses exist, program the card
	 * with the valid entries in the multicast array.
	 */
	bzero((caddr_t)&mcmd, sizeof (mcmd));
	mcmd.mcs_status = CS_EL | CS_INT | CS_CMD_MCSET;
	i = 0;
	for (s = 0; s < IEEF_NMCS; s++) {
		if (ieefp->mcs_addrs_valid[s] != 0) {
			bcopy((caddr_t)(ieefp->mcs_addrs[s].ether_addr_octet),
				(caddr_t)&(mcmd.mcs_addr[i * ETHERADDRL]),
				ETHERADDRL);
			i++;
		}
	}
	mcmd.mcs_count = i * ETHERADDRL;
	if (ieef_add_command(ieefp, (gen_cmd_t *)&mcmd, sizeof (mcmd))
		== -1) {
		cmn_err(CE_WARN, "!Could not add configure command.\n");
	}
}
