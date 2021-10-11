/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)pcic.c	1.76	99/11/12 SMI"

/*
 * PCIC device/interrupt handler
 *	The "pcic" driver handles the Intel 82365SL, Cirrus Logic
 *	and Toshiba (and possibly other clones) PCMCIA adapter chip
 *	sets.  It implements a subset of Socket Services as defined
 *	in the Solaris PCMCIA design documents
 */


/*
 * currently defined "properties"
 *
 * clock-frequency		bus clock frequency
 * smi				system management interrupt override
 * need-mult-irq		need status IRQ for each pair of sockets
 * disable-audio		don't route audio signal to speaker
 * pcic_pm_time			test interval for pseudo suspend/resume
 * pcic_pm_detwin
 * pcic_pm_methods		which method of detection for suspend/resume
 */


#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/autoconf.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/dma_engine.h>
#include <sys/kstat.h>
#include <sys/kmem.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>

#include <sys/pctypes.h>
#include <sys/pcmcia.h>
#include <sys/sservice.h>

#include <sys/pcic_reg.h>
#include <sys/pcic_var.h>

/*
 * Power Management (Suspend/Resume) timer and detection
 *	values
 */
int pcic_pm_detwin = PCIC_PM_DETWIN;
int pcic_pm_time = PCIC_PM_TIME;
int pcic_pm_methods = PCIC_PM_DEF_METHOD;

int pcic_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
int pcic_identify(dev_info_t *);
int pcic_attach(dev_info_t *, ddi_attach_cmd_t);
int pcic_detach(dev_info_t *, ddi_detach_cmd_t);
uint32_t pcic_intr(caddr_t);
static int pcic_do_io_intr(pcicdev_t *, uint32_t);

int pcic_probe(dev_info_t *);
void pcic_init_assigned(dev_info_t *);




/*
 * On x86 platforms the ddi_iobp_alloc(9F) and ddi_mem_alloc(9F) calls
 * are xlated into DMA ctlops. To make this nexus work on x86, we
 * need to have the default ddi_dma_mctl ctlops in the bus_ops
 * structure, just to pass the request to the parent. The correct
 * ctlops should be ddi_no_dma_mctl because so far we don't do DMA.
 */
static
struct bus_ops pcmciabus_ops = {
	BUSO_REV,
	pcmcia_bus_map,
	pcmcia_get_intrspec,
	pcmcia_add_intrspec,
	pcmcia_remove_intrspec,
	i_ddi_map_fault,
	ddi_no_dma_map,
	ddi_no_dma_allochdl,
	ddi_no_dma_freehdl,
	ddi_no_dma_bindhdl,
	ddi_no_dma_unbindhdl,
	ddi_no_dma_flush,
	ddi_no_dma_win,
	ddi_dma_mctl,
	pcmcia_ctlops,
	pcmcia_prop_op,
	NULL,	/* (*bus_get_eventcookie)();	*/
	NULL,	/* (*bus_add_eventcall)();	*/
	NULL,	/* (*bus_remove_eventcall)();	*/
	NULL,	/* (*bus_post_event)();		*/
	pcmcia_intr_ctlops
};

static struct cb_ops pcic_cbops = {
	pcmcia_open,
	pcmcia_close,
	nodev,
	nodev,
	nodev,
	nodev,
	nodev,
	pcmcia_ioctl,
	nodev,
	nodev,
	nodev,
	nochpoll,
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

static struct dev_ops pcic_devops = {
	DEVO_REV,
	0,
	pcic_getinfo,
	pcic_identify,
	pcic_probe,
	pcic_attach,
	pcic_detach,
	nulldev,
	&pcic_cbops,
	&pcmciabus_ops,
	NULL
};

void *pcic_soft_state_p = NULL;

struct irqmap {
	int irq;
	int count;
} pcic_irq_map[16];


#if defined(PCIC_DEBUG)
static void xxdmp_all_regs(pcicdev_t *, int, uint32_t);
int pcic_debug = 0x0;
#endif

#define	PCIC_VCC_3VLEVEL	1
#define	PCIC_VCC_5VLEVEL	2
#define	PCIC_VCC_12LEVEL	3

/* bit patterns to select voltage levels */
int pcic_vpp_levels[13] = {
	0, 0, 0,
	PCIC_VCC_3VLEVEL,	/* 3.3V */
	0,
	PCIC_VCC_5VLEVEL,	/* 5V */
	0, 0, 0, 0, 0, 0,
	PCIC_VCC_12LEVEL	/* 12V */
};
struct power_entry pcic_power[4] = {
	{
		0, VCC|VPP1|VPP2
	},
	{
		33,		/* 3.3Volt */
		VCC|VPP1|VPP2
	},
	{
		5*10,		/* 5Volt */
		VCC|VPP1|VPP2	/* currently only know about this */
	},
	{
		12*10,		/* 12Volt */
		VPP1|VPP2
	}
};

static inthandler_t *pcic_handlers;

static void pcic_setup_adapter(pcicdev_t *);
static int pcic_change(pcicdev_t *, int);
static int pcic_ll_reset(pcicdev_t *, int);
static void pcic_mswait(int);
static void pcic_set_cdtimers(pcicdev_t *, int, uint32_t, int);
static void pcic_ready_wait(pcicdev_t *, int);
extern int pcmcia_get_intr(dev_info_t *, int);
extern int pcmcia_return_intr(dev_info_t *, int);
static int pcic_system(dev_info_t *dip);

static int pcic_callback(dev_info_t *, int (*)(), int);
static int pcic_inquire_adapter(dev_info_t *, inquire_adapter_t *);
static int pcic_get_adapter(dev_info_t *, get_adapter_t *);
static int pcic_get_page(dev_info_t *, get_page_t *);
static int pcic_get_socket(dev_info_t *, get_socket_t *);
static int pcic_get_status(dev_info_t *, get_ss_status_t *);
static int pcic_get_window(dev_info_t *, get_window_t *);
static int pcic_inquire_socket(dev_info_t *, inquire_socket_t *);
static int pcic_inquire_window(dev_info_t *, inquire_window_t *);
static int pcic_reset_socket(dev_info_t *, int, int);
static int pcic_set_page(dev_info_t *, set_page_t *);
static int pcic_set_window(dev_info_t *, set_window_t *);
static int pcic_set_socket(dev_info_t *, set_socket_t *);
static int pcic_set_interrupt(dev_info_t *, set_irq_handler_t *);
static int pcic_clear_interrupt(dev_info_t *, clear_irq_handler_t *);
static void pcic_pm_detection(void *);
static void pcic_iomem_pci_ctl(ddi_acc_handle_t, uchar_t *, unsigned);
static int clext_reg_read(pcicdev_t *, int, uchar_t);
static void clext_reg_write(pcicdev_t *, int, uchar_t, uchar_t);
static pcic_calc_speed(pcicdev_t *, uint32_t);
static int pcic_card_state(pcicdev_t *, pcic_socket_t *);
static int pcic_find_pci_type(pcicdev_t *);
static void pcic_82092_smiirq_ctl(pcicdev_t *, int, int, int);
static void pcic_handle_cd_change(pcicdev_t *, pcic_socket_t *, int);
uint8_t pcic_getb(pcicdev_t *, int, int);
void pcic_putb(pcicdev_t *, int, int, int8_t);
static int pcic_set_vcc_level(pcicdev_t *, set_socket_t *);

/*
 * pcmcia interface operations structure
 * this is the private interface that is exported to the nexus
 */
pcmcia_if_t pcic_if_ops = {
	PCIF_MAGIC,
	PCIF_VERSION,
	pcic_callback,
	pcic_get_adapter,
	pcic_get_page,
	pcic_get_socket,
	pcic_get_status,
	pcic_get_window,
	pcic_inquire_adapter,
	pcic_inquire_socket,
	pcic_inquire_window,
	pcic_reset_socket,
	pcic_set_page,
	pcic_set_window,
	pcic_set_socket,
	pcic_set_interrupt,
	pcic_clear_interrupt,
	NULL,
};

/* PCIC IRQ to index map */
int pcic_irq_to_index[32];

/*
 * chip type identification routines
 * this list of functions is searched until one of them succeeds
 * or all fail.  i82365SL is assumed if failed.
 */
int pcic_ci_cirrus(pcicdev_t *);
int pcic_ci_vadem(pcicdev_t *);
int pcic_ci_ricoh(pcicdev_t *);

int (*pcic_ci_funcs[])(pcicdev_t *) = {
	pcic_ci_cirrus,
	pcic_ci_vadem,
	pcic_ci_ricoh,
	NULL
};

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"PCIC PCMCIA adapter driver",	/* Name of the module. */
	&pcic_devops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init()
{
	int stat;

	/* Allocate soft state */
	if ((stat = ddi_soft_state_init(&pcic_soft_state_p,
		sizeof (pcicdev_t), 2)) != DDI_SUCCESS)
	    return (stat);

	if ((stat = mod_install(&modlinkage)) != DDI_SUCCESS)
	    ddi_soft_state_fini(&pcic_soft_state_p);

	return (stat);
}

int
_fini()
{
	int stat = 0;

	if ((stat = mod_remove(&modlinkage)) != DDI_SUCCESS)
	    return (stat);

	ddi_soft_state_fini(&pcic_soft_state_p);

	return (stat);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
pcic_identify(dev_info_t *dip)
{
	char *dname, *name = ddi_get_name(dip);
	int major;

	if ((major = ddi_name_to_major(name)) > 0)
		dname = ddi_major_to_name(major);
	else
		dname = name;

	if (strcmp(dname, PCIC_ID_NAME) == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*
 * pcic_getinfo()
 *	provide instance/device information about driver
 */
pcic_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	anp_t *anp;
	int error = DDI_SUCCESS;
#ifdef lint
	dip = dip;
#endif

	switch (cmd) {
	    case DDI_INFO_DEVT2DEVINFO:
		if (!(anp = ddi_get_soft_state(pcic_soft_state_p,
						getminor((dev_t)arg))))
			*result = NULL;
		else
			*result = anp->an_dip;
		break;
	    case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		break;
	    default:
		error = DDI_FAILURE;
		break;
	}
	return (error);
}

int
pcic_probe(register dev_info_t *dip)
{
	int value;
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t handle;
	uchar_t *index, *data;

	if (ddi_dev_is_sid(dip) == DDI_SUCCESS)
	    return (DDI_PROBE_DONTCARE);

	/*
	 * find a PCIC device (any vendor)
	 * while there can be up to 4 such devices in
	 * a system, we currently only look for 1
	 * per probe.  There will be up to 2 chips per
	 * instance since they share I/O space
	 */
	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	if (ddi_regs_map_setup(dip, PCIC_ISA_CONTROL_REG_NUM,
				(caddr_t *)&index,
				PCIC_ISA_CONTROL_REG_OFFSET,
				PCIC_ISA_CONTROL_REG_LENGTH,
				&attr, &handle) != DDI_SUCCESS)
	    return (DDI_PROBE_FAILURE);

	data = index + 1;

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "pcic_probe: entered\n");
	if (pcic_debug)
		cmn_err(CE_CONT, "\tindex=%p\n", (void *)index);
#endif
	ddi_put8(handle, index, PCIC_CHIP_REVISION);
	ddi_put8(handle, data, 0);
	value = ddi_get8(handle, data);
#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "\tchip revision register = %x\n", value);
#endif
	if ((value & PCIC_REV_MASK) >= PCIC_REV_LEVEL_LOW &&
	    (value & 0x30) == 0) {
		/*
		 * we probably have a PCIC chip in the system
		 * do a little more checking.  If we find one,
		 * reset everything in case of softboot
		 */
		ddi_put8(handle, index, PCIC_MAPPING_ENABLE);
		ddi_put8(handle, data, 0);
		value = ddi_get8(handle, data);
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "\tzero test = %x\n", value);
#endif
		/* should read back as zero */
		if (value == 0) {
			/*
			 * we do have one and it is off the bus
			 */
#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT, "pcic_probe: success\n");
#endif
			ddi_regs_map_free(&handle);
			return (DDI_PROBE_SUCCESS);
		}
	}
#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "pcic_probe: failed\n");
#endif
	ddi_regs_map_free(&handle);
	return (DDI_PROBE_FAILURE);
}

/*
 * pcic_attach()
 *	attach the PCIC (Intel 82365SL/CirrusLogic/Toshiba) driver
 *	to the system.  This is a child of "sysbus" since that is where
 *	the hardware lives, but it provides services to the "pcmcia"
 *	nexus driver.  It gives a pointer back via its private data
 *	structure which contains both the dip and socket services entry
 *	points
 */
int
pcic_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	anp_t *pcic_nexus;
	pcicdev_t *pcic;
	int irqlevel, value;
	int i, j, smi;
	char *typename;
	char bus_type[16] = "(unknown)";
	int len = sizeof (bus_type);
	ddi_device_acc_attr_t attr;
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_attach: entered\n");
	}
#endif
	switch (cmd) {
	case DDI_RESUME:
		pcic = anp->an_private;
		/*
		 * for now, this is a simulated resume.
		 * a real one may need different things.
		 */
		if (pcic != NULL && pcic->pc_flags & PCF_SUSPENDED) {
			mutex_enter(&pcic->pc_lock);
			/* should probe for new sockets showing up */
			pcic_setup_adapter(pcic);
			pcic->pc_flags &= ~PCF_SUSPENDED;
			mutex_exit(&pcic->pc_lock);
			(void) pcmcia_begin_resume(dip);
			/*
			 * this will do the CARD_INSERTION
			 * due to needing time for threads to
			 * run, it must be delayed for a short amount
			 * of time.  pcmcia_wait_insert checks for all
			 * children to be removed and then triggers insert.
			 */
			(void) pcmcia_wait_insert(dip);
			/*
			 * for complete implementation need END_RESUME (later)
			 */
			return (DDI_SUCCESS);

		}
		return (DDI_SUCCESS);
	}
	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	/*
	 * Allocate soft state associated with this instance.
	 */
	if (ddi_soft_state_zalloc(pcic_soft_state_p,
				ddi_get_instance(dip)) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "pcic%d: Unable to alloc state\n",
			ddi_get_instance(dip));
		return (DDI_FAILURE);
	}

	pcic_nexus = ddi_get_soft_state(pcic_soft_state_p,
					ddi_get_instance(dip));

	pcic = (pcicdev_t *)kmem_zalloc(sizeof (pcicdev_t), KM_SLEEP);

	pcic->dip = dip;
	pcic_nexus->an_dip = dip;
	pcic_nexus->an_if = &pcic_if_ops;
	pcic_nexus->an_private = pcic;
	pcic->pc_numpower = 4;
	pcic->pc_power = pcic_power;

	ddi_set_driver_private(dip, (caddr_t)pcic_nexus);

	/*
	 * pcic->pc_irq is really the IPL level we want to run at
	 * set the default values here and override from intr spec
	 */
	pcic->pc_irq = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_CANSLEEP,
					"interrupt-priorities", -1);
	if (pcic->pc_irq == -1) {
		struct intrspec *ispec;
		/* see if intrspec tells us different */
		ispec = (struct intrspec *)i_ddi_get_intrspec(dip, dip, 0);
		if (ispec != NULL)
			pcic->pc_irq = ispec->intrspec_pri; /* XXX */
		else
			pcic->pc_irq = LOCK_LEVEL + 1;
	}
	pcic_nexus->an_ipl = pcic->pc_irq;

	/*
	 * Check our parent bus type. We do different things based on which
	 *	bus we're on.
	 * XXX - Should we really fail if we can't find our parent bus type?
	 */
	if (ddi_prop_op(DDI_DEV_T_ANY, ddi_get_parent(dip),
				PROP_LEN_AND_VAL_BUF, DDI_PROP_CANSLEEP,
				"device_type", (caddr_t)&bus_type[0], &len) !=
							DDI_PROP_SUCCESS) {
		if (ddi_prop_op(DDI_DEV_T_ANY, ddi_get_parent(dip),
				PROP_LEN_AND_VAL_BUF, DDI_PROP_CANSLEEP,
				"bus-type", (caddr_t)&bus_type[0], &len) !=
							DDI_PROP_SUCCESS) {

			cmn_err(CE_CONT,
				"pcic%d: can't find parent bus type\n",
				ddi_get_instance(dip));

			kmem_free(pcic, sizeof (pcicdev_t));
			return (DDI_FAILURE);
		}
	} /* ddi_prop_op("device_type") */

	if (strcmp(bus_type, DEVI_PCI_NEXNAME) == 0) {
		pcic->pc_flags = PCF_PCIBUS;
	} else {
#if defined(sparc) || defined(__sparc)
		cmn_err(CE_CONT, "pcic%d: unsupported parent bus type: [%s]\n",
			ddi_get_instance(dip), bus_type);

		kmem_free(pcic, sizeof (pcicdev_t));
		return (DDI_FAILURE);
#else
		pcic->pc_flags = 0;
#endif
	}

	if ((pcic->bus_speed = ddi_getprop(DDI_DEV_T_ANY, ddi_get_parent(dip),
						DDI_PROP_CANSLEEP,
						"clock-frequency", 0)) == 0) {
		if (pcic->pc_flags & PCF_PCIBUS)
			pcic->bus_speed = PCIC_PCI_DEF_SYSCLK;
		else
			pcic->bus_speed = PCIC_ISA_DEF_SYSCLK;

	} /* ddi_prop_op("clock-frequency") */

	if (pcic_system(dip) != DDI_SUCCESS) {
		kmem_free(pcic, sizeof (pcicdev_t));
		return (DDI_FAILURE);
	}

	pcic->pc_io_type = PCIC_IO_TYPE_82365SL; /* default mode */

#ifdef	PCIC_DEBUG
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic%d: parent bus type = [%s], speed = %d MHz\n",
			ddi_get_instance(dip),
			bus_type, pcic->bus_speed);
	}
#endif

	/*
	 * The reg properties on a PCI node are different than those
	 *	on a non-PCI node. Handle that difference here.
	 *	If it turns out to be a CardBus chip, we have even more
	 *	differences.
	 */
	if (pcic->pc_flags & PCF_PCIBUS) {
		int class_code;
		/*
		 * might not know about device capabilities
		 * so default to the same as ISA and fix later
		 * when we know which chip we have.
		 */
		pcic->pc_base = 0x10000;
		pcic->pc_bound = (uint32_t)~0;
#if defined(i386)
		pcic->pc_iobase = 0x200;
		pcic->pc_iobound = 0xfdff;
#else
		pcic->pc_iobase = 0x00;
#if defined(sparc)
		pcic->pc_iobound = (uint32_t)~0;
#else
		pcic->pc_iobound = 0x1000;
#endif
#endif

		/* usually need to get at config space so map first */
		attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
		attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

		if (ddi_regs_map_setup(dip, PCIC_PCI_CONFIG_REG_NUM,
					(caddr_t *)&pcic->cfgaddr,
					PCIC_PCI_CONFIG_REG_OFFSET,
					PCIC_PCI_CONFIG_REG_LENGTH,
					&attr,
					&pcic->cfg_handle) !=
		    DDI_SUCCESS) {
			cmn_err(CE_CONT,
				"pcic%d: unable to map config space"
				"regs\n",
				ddi_get_instance(dip));

			kmem_free(pcic, sizeof (pcicdev_t));
			return (DDI_FAILURE);
		} /* ddi_regs_map_setup */

		class_code = ddi_getprop(DDI_DEV_T_ANY, dip,
					DDI_PROP_CANSLEEP|DDI_PROP_DONTPASS,
					"class-code", -1);

		switch (class_code) {
		case PCIC_PCI_CARDBUS:
			pcic->pc_flags |= PCF_CARDBUS;
			pcic->pc_io_type = PCIC_IO_TYPE_YENTA;
			/*
			 * Get access to the adapter registers on the
			 * PCI bus.  A 4K memory page
			 */
			attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
			attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
			attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

			if (ddi_regs_map_setup(dip, PCIC_PCI_CONTROL_REG_NUM,
						(caddr_t *)&pcic->ioaddr,
						PCIC_PCI_CONTROL_REG_OFFSET,
						PCIC_CB_CONTROL_REG_LENGTH,
						&attr, &pcic->handle) !=
			    DDI_SUCCESS) {
				cmn_err(CE_CONT,
					"pcic%d: unable to map PCI regs\n",
					ddi_get_instance(dip));
				ddi_regs_map_free(&pcic->cfg_handle);
				kmem_free(pcic, sizeof (pcicdev_t));
				return (DDI_FAILURE);
			} /* ddi_regs_map_setup */


			/*
			 * Find out the chip type - If we're on a PCI bus,
			 *	the adapter has that information in the PCI
			 *	config space.
			 * Note that we call pcic_find_pci_type here since
			 *	it needs a valid mapped pcic->handle to
			 *	access some of the adapter registers in
			 *	some cases.
			 */
			if (pcic_find_pci_type(pcic) != DDI_SUCCESS) {
				ddi_regs_map_free(&pcic->handle);
				ddi_regs_map_free(&pcic->cfg_handle);
				kmem_free(pcic, sizeof (pcicdev_t));
				cmn_err(CE_WARN, "pcic: %s: unsupported "
								"bridge\n",
							ddi_get_name_addr(dip));
				return (DDI_FAILURE);
			}
			break;

		default:
		case PCIC_PCI_PCMCIA:
			/*
			 * Get access to the adapter IO registers on the
			 * PCI bus config space.
			 */
			attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
			attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
			attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

			/*
			 * We need a default mapping to the adapter's IO
			 *	control register space. For most adapters
			 *	that are of class PCIC_PCI_PCMCIA (or of
			 *	a default class) the control registers
			 *	will be using the 82365-type control/data
			 *	format.
			 */
			if (ddi_regs_map_setup(dip, PCIC_PCI_CONTROL_REG_NUM,
						(caddr_t *)&pcic->ioaddr,
						PCIC_PCI_CONTROL_REG_OFFSET,
						PCIC_PCI_CONTROL_REG_LENGTH,
						&attr,
						&pcic->handle) != DDI_SUCCESS) {
				cmn_err(CE_CONT,
					"pcic%d: unable to map PCI regs\n",
					ddi_get_instance(dip));
				ddi_regs_map_free(&pcic->cfg_handle);
				kmem_free(pcic, sizeof (pcicdev_t));
				return (DDI_FAILURE);
			} /* ddi_regs_map_setup */

			/*
			 * Find out the chip type - If we're on a PCI bus,
			 *	the adapter has that information in the PCI
			 *	config space.
			 * Note that we call pcic_find_pci_type here since
			 *	it needs a valid mapped pcic->handle to
			 *	access some of the adapter registers in
			 *	some cases.
			 */
			if (pcic_find_pci_type(pcic) != DDI_SUCCESS) {
				ddi_regs_map_free(&pcic->handle);
				ddi_regs_map_free(&pcic->cfg_handle);
				kmem_free(pcic, sizeof (pcicdev_t));
				cmn_err(CE_WARN, "pcic: %s: unsupported "
								"bridge\n",
							ddi_get_name_addr(dip));
				return (DDI_FAILURE);
			}

			/*
			 * Some PCI-PCMCIA(R2) adapters are Yenta-compliant
			 *	for extended registers even though they are
			 *	not CardBus adapters. For those adapters,
			 *	re-map pcic->handle to be large enough to
			 *	encompass the Yenta registers.
			 */
			switch (pcic->pc_type) {
			    case PCIC_TI_PCI1031:
				ddi_regs_map_free(&pcic->handle);

				if (ddi_regs_map_setup(dip,
						PCIC_PCI_CONTROL_REG_NUM,
						(caddr_t *)&pcic->ioaddr,
						PCIC_PCI_CONTROL_REG_OFFSET,
						PCIC_CB_CONTROL_REG_LENGTH,
						&attr,
						&pcic->handle) != DDI_SUCCESS) {
					cmn_err(CE_CONT,
						"pcic%d: unable to map "
								"PCI regs\n",
						ddi_get_instance(dip));
					ddi_regs_map_free(&pcic->cfg_handle);
					kmem_free(pcic, sizeof (pcicdev_t));
					return (DDI_FAILURE);
				} /* ddi_regs_map_setup */
				break;
			    default:
				break;
			} /* switch (pcic->pc_type) */
			break;
		} /* switch (class_code) */
	} else {
		/*
		 * We're not on a PCI bus, so assume an ISA bus type
		 * register property. Get access to the adapter IO
		 * registers on a non-PCI bus.
		 * XXX We might be able to collapse this code into the
		 * code	above.
		 */
		attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
		attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
		pcic->mem_reg_num = PCIC_ISA_MEM_REG_NUM;
		pcic->io_reg_num = PCIC_ISA_IO_REG_NUM;

		if (ddi_regs_map_setup(dip, PCIC_ISA_CONTROL_REG_NUM,
					(caddr_t *)&pcic->ioaddr,
					PCIC_ISA_CONTROL_REG_OFFSET,
					PCIC_ISA_CONTROL_REG_LENGTH,
					&attr,
					&pcic->handle) != DDI_SUCCESS) {
			cmn_err(CE_CONT,
				"pcic%d: unable to map ISA registers\n",
				ddi_get_instance(dip));

			kmem_free(pcic, sizeof (pcicdev_t));
			return (DDI_FAILURE);
		} /* ddi_regs_map_setup */

		/* ISA bus is limited to 24-bits, but not first 64K */
		pcic->pc_base = 0x10000;
		pcic->pc_bound = (uint32_t)~0;
		pcic->pc_iobase = 0x200;
		pcic->pc_iobound = 0xffff;
	} /* !PCF_PCIBUS */

	/*
	 * Setup various adapter registers for the PCI case. For the
	 * non-PCI case, find out the chip type.
	 */
	if (pcic->pc_flags & PCF_PCIBUS) {
		int iline;
#if defined(sparc)
		iline = 0;
#else
		iline = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCI_CONF_ILINE);
#endif

		/*
		 * PCI devices have 32-bit memory addressing
		 * so change the default base and set flags appropriately
		 */
		switch (pcic->pc_type) {
			/* put PCF_MEM_PAGE devices first */
		case PCIC_TI_PCI1130:
			/* the 1130 has all windows in 1 16M page */
			pcic->pc_flags |= PCF_MEM_PAGE;
			/* FALLTHROUGH */
		case PCIC_TI_PCI1131: /* page per window unlike 1130 */
		case PCIC_TI_PCI1250:
		case PCIC_TI_PCI1221:
		case PCIC_TI_PCI1031:
		case PCIC_CL_PD6730:
		case PCIC_CL_PD6832:
		case PCIC_INTEL_i82092:
		case PCIC_RICOH_RL5C466:
		case PCIC_SMC_34C90:
			pcic->pc_base = 0x1000000;
			break;
		case PCIC_CL_PD6729:
		default:
			pcic->pc_base  = 0x10000;
			break;
		}

		/* set flags and socket counts based on chip type */
		switch (pcic->pc_type) {
			uint32_t cfg;
		case PCIC_INTEL_i82092:
			cfg = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_82092_PCICON);
			/* we can only support 4 Socket version */
			if (cfg & PCIC_82092_4_SOCKETS) {
			    pcic->pc_numsockets = 4;
			    pcic->pc_type = PCIC_INTEL_i82092;
			    if (iline != 0xFF)
				    pcic->pc_intr_mode = PCIC_INTR_MODE_PCI_1;
			    else
				    pcic->pc_intr_mode = PCIC_INTR_MODE_ISA;
			} else {
			    cmn_err(CE_CONT,
				    "pcic%d: Intel 82092 adapter "
				    "in unsupported configuration: 0x%x",
				    ddi_get_instance(pcic->dip), cfg);
			    pcic->pc_numsockets = 0;
			} /* PCIC_82092_4_SOCKETS */
			break;
		case PCIC_CL_PD6730:
		case PCIC_CL_PD6729:
			pcic->pc_intr_mode = PCIC_INTR_MODE_PCI_1;
			cfg = ddi_getprop(DDI_DEV_T_ANY, dip,
						DDI_PROP_CANSLEEP,
						"interrupts", 0);
			/* if not interrupt pin then must use ISA style IRQs */
			if (cfg == 0 || iline == 0xFF)
				pcic->pc_intr_mode = PCIC_INTR_MODE_ISA;
			else {
				/*
				 * we have the option to use PCI interrupts.
				 * this might not be optimal but in some cases
				 * is the only thing possible (sparc case).
				 * we now deterine what is possible.
				 */
				pcic->pc_intr_mode = PCIC_INTR_MODE_PCI_1;
			}
			pcic->pc_numsockets = 2;
			pcic->pc_flags |= PCF_IO_REMAP;
			break;
		case PCIC_TI_PCI1031:
			/* this chip doesn't do CardBus but looks like one */
			pcic->pc_flags &= ~PCF_CARDBUS;
			/* FALLTHROUGH */
		case PCIC_CL_PD6832:
		case PCIC_TI_PCI1130:
		case PCIC_TI_PCI1131:
		case PCIC_TI_PCI1250:
		case PCIC_TI_PCI1221:
		case PCIC_RICOH_RL5C466:
		case PCIC_SMC_34C90:
			pcic->pc_flags |= PCF_IO_REMAP;
			/* FALLTHROUGH */
			/* indicate feature even if not supported */
			pcic->pc_flags |= PCF_DMA | PCF_ZV;
			pcic->pc_numsockets = 1; /* one per function */
			if (iline != 0xFF)
				pcic->pc_intr_mode = PCIC_INTR_MODE_PCI_1;
			else
				pcic->pc_intr_mode = PCIC_INTR_MODE_ISA;
			pcic->pc_io_type = PCIC_IOTYPE_YENTA;
			break;

		}
	} else {
		/*
		 * We're not on a PCI bus so do some more
		 *	checking for adapter type here.
		 * For the non-PCI bus case:
		 * It could be any one of a number of different chips
		 * If we can't determine anything else, it is assumed
		 * to be an Intel 82365SL.  The Cirrus Logic PD6710
		 * has an extension register that provides unique
		 * identification. Toshiba chip isn't detailed as yet.
		 */

		/* Init the CL id mode */
		pcic_putb(pcic, 0, PCIC_CHIP_INFO, 0);
		value = pcic_getb(pcic, 0, PCIC_CHIP_INFO);

		/* default to Intel i82365SL and then refine */
		pcic->pc_type = PCIC_I82365SL;
		pcic->pc_chipname = PCIC_TYPE_I82365SL;
		for (value = 0; pcic_ci_funcs[value] != NULL; value++) {
			/* go until one succeeds or none left */
			if (pcic_ci_funcs[value](pcic))
				break;
		}

		/* any chip specific flags get set here */
		switch (pcic->pc_type) {
		case PCIC_CL_PD6722:
			pcic->pc_flags |= PCF_DMA;
		}

		for (i = 0; i < PCIC_MAX_SOCKETS; i++) {
			/*
			 * look for total number of sockets.
			 * basically check each possible socket for
			 * presence like in probe
			 */

			/* turn all windows off */
			pcic_putb(pcic, i, PCIC_MAPPING_ENABLE, 0);
			value = pcic_getb(pcic, i, PCIC_MAPPING_ENABLE);

			/*
			 * if a zero is read back, then this socket
			 * might be present. It would be except for
			 * some systems that map the secondary PCIC
			 * chip space back to the first.
			 */
			if (value != 0) {
				/* definitely not so skip */
				/* note: this is for Compaq support */
				continue;
			}

			/* further tests */
			value = pcic_getb(pcic, i, PCIC_CHIP_REVISION) &
				PCIC_REV_MASK;
			if (!(value >= PCIC_REV_LEVEL_LOW &&
				value <= PCIC_REV_LEVEL_HI))
				break;

			pcic_putb(pcic, i, PCIC_SYSMEM_0_STARTLOW, 0xaa);
			pcic_putb(pcic, i, PCIC_SYSMEM_1_STARTLOW, 0x55);
			value = pcic_getb(pcic, i, PCIC_SYSMEM_0_STARTLOW);

			j = pcic_getb(pcic, i, PCIC_SYSMEM_1_STARTLOW);
			if (value != 0xaa || j != 0x55)
				break;

			/*
			 * at this point we know if we have hardware
			 * of some type and not just the bus holding
			 * a pattern for us. We still have to determine
			 * the case where more than 2 sockets are
			 * really the same due to peculiar mappings of
			 * hardware.
			 */
			j = pcic->pc_numsockets++;
			pcic->pc_sockets[j].pcs_flags = 0;
			pcic->pc_sockets[j].pcs_io = pcic->ioaddr;
			pcic->pc_sockets[j].pcs_socket = i;

			/* put PC Card into RESET, just in case */
			value = pcic_getb(pcic, i, PCIC_INTERRUPT);
			pcic_putb(pcic, i, PCIC_INTERRUPT,
					value & ~PCIC_RESET);
		}

#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "num sockets = %d\n",
				pcic->pc_numsockets);
#endif
		if (pcic->pc_numsockets == 0) {
			ddi_regs_map_free(&pcic->handle);
			kmem_free(pcic, sizeof (pcicdev_t));
			return (DDI_FAILURE);
		}

		/*
		 * need to think this through again in light of
		 * Compaq not following the model that all the
		 * chip vendors recommend.  IBM 755 seems to be
		 * afflicted as well.  Basically, if the vendor
		 * wired things wrong, socket 0 responds for socket 2
		 * accesses, etc.
		 */
		if (pcic->pc_numsockets > 2) {
			int count = pcic->pc_numsockets / 4;
			for (i = 0; i < count; i++) {
				/* put pattern into socket 0 */
				pcic_putb(pcic, i,
						PCIC_SYSMEM_0_STARTLOW, 0x11);

				/* put pattern into socket 2 */
				pcic_putb(pcic, i + 2,
						PCIC_SYSMEM_0_STARTLOW, 0x33);

				/* read back socket 0 */
				value = pcic_getb(pcic, i,
						    PCIC_SYSMEM_0_STARTLOW);

				/* read back chip 1 socket 0 */
				j = pcic_getb(pcic, i + 2,
						PCIC_SYSMEM_0_STARTLOW);
				if (j == value) {
					pcic->pc_numsockets -= 2;
				}
			}
		}

		smi = 0xff;	/* no more override */

		if (ddi_getprop(DDI_DEV_T_NONE, dip,
				DDI_PROP_DONTPASS, "need-mult-irq",
				0xffff) != 0xffff)
			pcic->pc_flags |= PCF_MULT_IRQ;

	} /* !PCF_PCIBUS */

	/*
	 * some platforms/busses need to have resources setup
	 * this is temporary until a real resource allocator is
	 * implemented.
	 */

	pcic_init_assigned(dip);

	typename = pcic->pc_chipname;

#ifdef	PCIC_DEBUG
	if (pcic_debug) {
		int nregs, nintrs;

		if (ddi_dev_nregs(dip, &nregs) != DDI_SUCCESS)
			nregs = 0;

		if (ddi_dev_nintrs(dip, &nintrs) != DDI_SUCCESS)
			nintrs = 0;

		cmn_err(CE_CONT,
			"pcic%d: %d register sets, %d interrupts\n",
			ddi_get_instance(dip), nregs, nintrs);

		nintrs = 0;
		while (nregs--) {
			off_t size;

			if (ddi_dev_regsize(dip, nintrs, &size) ==
			    DDI_SUCCESS) {
				cmn_err(CE_CONT,
					"\tregnum %d size %ld (0x%lx)"
					"bytes",
					nintrs, size, size);
				if (nintrs == PCIC_ISA_CONTROL_REG_NUM)
					cmn_err(CE_CONT,
						" mapped at: 0x%p\n",
						(void *)pcic->ioaddr);
				else
					cmn_err(CE_CONT, "\n");
			} else {
				cmn_err(CE_CONT,
					"\tddi_dev_regsize(rnumber"
					"= %d) returns DDI_FAILURE\n",
					nintrs);
			}
			nintrs++;
		} /* while */
	} /* if (pcic_debug) */
#endif

	cv_init(&pcic->pm_cv, NULL, CV_DRIVER, NULL);

	if (!ddi_getprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
						"disable-audio", 0))
		pcic->pc_flags |= PCF_AUDIO;

	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, 0, PCICPROP_CTL,
				(caddr_t)typename, strlen(typename) + 1);

	/*
	 * Init all socket SMI levels to 0 (no SMI)
	 */
	for (i = 0; i < PCIC_MAX_SOCKETS; i++) {
	    pcic->pc_sockets[i].pcs_smi = 0;
	    pcic->pc_lastreg = -1; /* just to make sure we are in sync */
	}

	/*
	 * Setup the IRQ handler(s)
	 */
	switch (pcic->pc_intr_mode) {
		int xx;
	case PCIC_INTR_MODE_ISA:
	/*
	 * On a non-PCI bus, we just use whatever SMI IRQ level was
	 *	specified above, and the IO IRQ levels are allocated
	 *	dynamically.
	 * XXX Note that this will have to be looked at if we allow
	 *	swapping of PCMCIA adapters over a checkpoint/resume.
	 */
		for (xx = 15, smi = 0; xx >= 0; xx--) {
			if (PCIC_IRQ(xx) &
			    PCIC_AVAIL_IRQS) {
				smi = pcmcia_get_intr(dip, xx);
				if (smi >= 0)
					break;
			}
		}
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_NOTE, "\tselected IRQ %d as SMI\n", smi);
#endif
		/* init to same so share is easy */
		for (i = 0; i < pcic->pc_numsockets; i++)
			pcic->pc_sockets[i].pcs_smi = smi;
		/* any special handling of IRQ levels */
		if (pcic->pc_flags & PCF_MULT_IRQ) {
			for (i = 2; i < pcic->pc_numsockets; i++) {
				if ((i & 1) == 0) {
					int xx;
					for (xx = 15, smi = 0; xx >= 0; xx--) {
						if (PCIC_IRQ(xx) &
						    PCIC_AVAIL_IRQS) {
							smi =
							    pcmcia_get_intr(dip,
									    xx);
							if (smi >= 0)
								break;
						}
					}
				}
				if (smi >= 0)
					pcic->pc_sockets[i].pcs_smi = smi;
			}
		}
		for (i = 0, irqlevel = -1; i < pcic->pc_numsockets; i++) {
			struct intrspec intrspec;
			if (irqlevel == pcic->pc_sockets[i].pcs_smi)
				continue;
			else {
				irqlevel = pcic->pc_sockets[i].pcs_smi;
			}
			/*
			 * now convert the allocated IRQ into an intrspec
			 * and ask our parent to add it.  Don't use
			 * the ddi_add_intr since we don't have a
			 * default intrspec in all cases.
			 *
			 * note: this sort of violates DDI but we don't
			 *	 get hardware intrspecs for many of the devices.
			 *	 at the same time, we know how to allocate them
			 *	 so we do the right thing.
			 */
			bzero((caddr_t)&intrspec, sizeof (struct intrspec));
			intrspec.intrspec_vec = irqlevel;
			intrspec.intrspec_pri = pcic->pc_irq;
			if ((value = i_ddi_add_intrspec(ddi_get_parent(dip),
							dip, &intrspec,
							&pcic->pc_icookie,
							&pcic->pc_dcookie,
							pcic_intr,
							(caddr_t)pcic,
							0)) !=
			    DDI_SUCCESS) {
				cmn_err(CE_WARN,
					"%s: ddi_add_intr failed %x%s\n",
					ddi_get_name(dip), value,
					value == DDI_INTR_NOTFOUND ?
					" DDI_INTR_NOTFOUND" : "");
				(void) pcmcia_return_intr(dip,
						pcic->pc_sockets[i].pcs_smi);
				ddi_regs_map_free(&pcic->handle);
				if (pcic->pc_flags & PCF_PCIBUS)
					ddi_regs_map_free(&pcic->cfg_handle);
				kmem_free(pcic, sizeof (pcicdev_t));
				return (DDI_FAILURE);
			}
			if (i == 0) {
				mutex_init(&pcic->pc_lock, NULL, MUTEX_DRIVER,
				    pcic->pc_icookie);
			}
		}
		break;
	case PCIC_INTR_MODE_PCI_1:
	case PCIC_INTR_MODE_PCI:
		/*
		 * If we're on a PCI bus, we route all interrupts, both SMI
		 * and IO interrupts, through a single interrupt line.
		 * Assign the SMI IRQ level to the IO IRQ level here.
		 */
		(void) ddi_get_iblock_cookie(dip, 0, &pcic->pc_icookie);
		if (ddi_add_intr(dip, 0, NULL, &pcic->pc_dcookie,
					pcic_intr,
					(caddr_t)pcic) != DDI_SUCCESS) {
			ddi_regs_map_free(&pcic->handle);
			if (pcic->pc_flags & PCF_PCIBUS)
				ddi_regs_map_free(&pcic->cfg_handle);
			kmem_free(pcic, sizeof (pcicdev_t));
			return (DDI_FAILURE);
		}

		/* init to same (PCI) so share is easy */
		for (i = 0; i < pcic->pc_numsockets; i++)
			pcic->pc_sockets[i].pcs_smi = 0xF; /* any valid */
		mutex_init(&pcic->pc_lock, NULL, MUTEX_DRIVER,
		    pcic->pc_icookie);
		break;
	}


	/*
	 * Setup the adapter hardware to some reasonable defaults.
	 */
	mutex_enter(&pcic->pc_lock);
	/* mark the driver state as attached */
	pcic->pc_flags |= PCF_ATTACHED;

	pcic_setup_adapter(pcic);

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "type = %s sockets = %d\n", typename,
						pcic->pc_numsockets);
#endif

	pcic_nexus->an_iblock = &pcic->pc_icookie;
	pcic_nexus->an_idev = &pcic->pc_dcookie;

	mutex_exit(&pcic->pc_lock);


	/*
	 * Check for the PM detection override properties
	 */
	pcic_pm_time = ddi_getprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
				"pcic_pm_time", PCIC_PM_TIME);

	pcic_pm_detwin = ddi_getprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
				"pcic_pm_detwin", PCIC_PM_DETWIN);

	pcic_pm_methods = ddi_getprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
				"pcic_pm_methods", PCIC_PM_DEF_METHOD);


	if (pcic_pm_methods)
	    cmn_err(CE_CONT, "?pcic: pcic_pm_time = %d secs, "
				"pcic_pm_detwin = %d secs, methods = %s%s\n",
					pcic_pm_time, pcic_pm_detwin,
					(pcic_pm_methods & PCIC_PM_METHOD_TIME)?
						"TIME ":"",
					(pcic_pm_methods & PCIC_PM_METHOD_REG)?
						"REG ":"");
#if 0
	/*
	 * If we have any PM methods to use, then start the
	 *	PM/CPR detection timer running
	 */
	if (pcic_pm_methods != 0) {
		pmt = &pcic->pmt;
		pmt->state = PCIC_PM_INIT;
		pmt->dip = dip;
		pcic_pm_detection(pcic);	/* starts timer */
	}
#endif

	/*
	 * Give the PCMCIA misc module a chance to do it's per-adapter
	 *	instance setup.
	 */
	if ((i = pcmcia_attach(dip, pcic_nexus)) != DDI_SUCCESS) {
		ddi_regs_map_free(&pcic->handle);
		if (pcic->pc_flags & PCF_PCIBUS)
			ddi_regs_map_free(&pcic->cfg_handle);
		kmem_free(pcic, sizeof (pcicdev_t));
	}

	return (i);
}

/*
 * pcic_detach()
 *	request to detach from the system
 */
int
pcic_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int i;

	switch (cmd) {
	case DDI_DETACH:
		/* don't detach if the nexus still talks to us */
		if (pcic->pc_callback != NULL)
			return (DDI_FAILURE);

		/* kill off the pm simulation */
		if (pcic->pc_pmtimer)
			(void) untimeout(pcic->pc_pmtimer);
		/*
		 * XXX Need to check to be sure that socket
		 *	addresses were setup.
		 */
		/* turn everything off for all sockets and chips */
		for (i = 0; i < pcic->pc_numsockets; i++) {
			pcic_putb(pcic, i, PCIC_MANAGEMENT_INT, 0);
			pcic_putb(pcic, i, PCIC_CARD_DETECT, 0);
			pcic_putb(pcic, i, PCIC_MAPPING_ENABLE, 0);
			/* disable interrupts and put card into RESET */
			pcic_putb(pcic, i, PCIC_INTERRUPT, 0);
		}
		ddi_remove_intr(dip, 0, pcic->pc_icookie);
		pcic->pc_flags = 0;
		mutex_destroy(&pcic->pc_lock);
		cv_destroy(&pcic->pm_cv);
		if (pcic->pc_flags & PCF_PCIBUS)
		    ddi_regs_map_free(&pcic->cfg_handle);
		ddi_regs_map_free(&pcic->handle);
		kmem_free(pcic, sizeof (pcicdev_t));
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		/*
		 * we got a suspend event (either real or imagined)
		 * so notify the nexus proper that all existing cards
		 * should go away.
		 */
		mutex_enter(&pcic->pc_lock);
		pcic->pc_flags |= PCF_SUSPENDED;
		mutex_exit(&pcic->pc_lock);


		/*
		 * when true power management exists, save the adapter
		 * state here to enable a recovery.  For the emulation
		 * condition, the state is gone
		 */
		return (DDI_SUCCESS);

	default:
		return (EINVAL);
	}
}

static void
pcic_setup_adapter(pcicdev_t *pcic)
{
	int i;
	int value, flags;

	if (pcic->pc_flags & PCF_PCIBUS) {
		/*
		 * all PCI-to-PCMCIA bus bridges need memory and I/O enabled
		 */
		flags = (PCIC_ENABLE_IO | PCIC_ENABLE_MEM);
		pcic_iomem_pci_ctl(pcic->cfg_handle, pcic->cfgaddr, flags);
	}
	/* enable each socket */
	for (i = 0; i < pcic->pc_numsockets; i++) {
		pcic->pc_sockets[i].pcs_flags = 0;
		/* find out the socket capabilities (I/O vs memory) */
		value = pcic_getb(pcic, i,
					PCIC_CHIP_REVISION) & PCIC_REV_ID_MASK;
		if (value == PCIC_REV_ID_IO || value == PCIC_REV_ID_BOTH)
			pcic->pc_sockets[i].pcs_flags |= PCS_SOCKET_IO;

		/* disable all windows just in case */
		pcic_putb(pcic, i, PCIC_MAPPING_ENABLE, 0);

		switch (pcic->pc_type) {
		    uchar_t cfg;

		    /* enable extended registers for Vadem */
		    case PCIC_VADEM_VG469:
		    case PCIC_VADEM:

			/* enable card status change interrupt for socket */
			break;

		    case PCIC_I82365SL:
			break;

		    case PCIC_CL_PD6710:
		    pcic_putb(pcic, 0, PCIC_MISC_CTL_2, PCIC_LED_ENABLE);
			break;

			/*
			 * On the CL_6730, we need to set up the interrupt
			 *	signalling mode (PCI mode) and set the SMI and
			 *	IRQ interrupt lines to PCI/level-mode.
			 * XXX Note that forcing PCI interrupt signalling
			 *	mode might not be the right thing to do here
			 *	if the chip is wired into the system some other
			 *	way. I hate inconsistant PC hardware designs.
			 */
		    case PCIC_CL_PD6730:
			switch (pcic->pc_intr_mode) {
			case PCIC_INTR_MODE_PCI_1:
				clext_reg_write(pcic, i, PCIC_CLEXT_MISC_CTL_3,
						((clext_reg_read(pcic, i,
						PCIC_CLEXT_MISC_CTL_3) &
						~PCIC_CLEXT_INT_PCI) |
						PCIC_CLEXT_INT_PCI));
				clext_reg_write(pcic, i, PCIC_CLEXT_EXT_CTL_1,
						(PCIC_CLEXT_IRQ_LVL_MODE |
						PCIC_CLEXT_SMI_LVL_MODE));
				cfg = PCIC_CL_LP_DYN_MODE;
				pcic_putb(pcic, i, PCIC_MISC_CTL_2, cfg);
				break;
			case PCIC_INTR_MODE_ISA:
				break;
			}
			break;
			/*
			 * On the CL_6729, we set the SMI and IRQ interrupt
			 *	lines to PCI/level-mode. as well as program the
			 *	correct clock speed divider bit.
			 */
		    case PCIC_CL_PD6729:
			switch (pcic->pc_intr_mode) {
			case PCIC_INTR_MODE_PCI_1:
				clext_reg_write(pcic, i, PCIC_CLEXT_EXT_CTL_1,
						(PCIC_CLEXT_IRQ_LVL_MODE |
						PCIC_CLEXT_SMI_LVL_MODE));

				break;
			case PCIC_INTR_MODE_ISA:
				break;
			}
			if (pcic->bus_speed > PCIC_PCI_25MHZ && i == 0) {
				cfg = 0;
				cfg |= PCIC_CL_TIMER_CLK_DIV;
				pcic_putb(pcic, i, PCIC_MISC_CTL_2, cfg);
			}
			break;
		    case PCIC_INTEL_i82092:
			/*
			 * XXX
			 * cfg = PCIC_82092_EN_TIMING | PCIC_82092_PWB |
			 *	PCIC_82092_RPFB;
			 * XXX
			 */
			cfg = PCIC_82092_EN_TIMING;
			if (pcic->bus_speed < PCIC_SYSCLK_33MHZ)
			    cfg |= PCIC_82092_PCICLK_25MHZ;
			ddi_put8(pcic->cfg_handle, pcic->cfgaddr +
						PCIC_82092_PCICON, cfg);
			break;
		    case PCIC_TI_PCI1130:
		    case PCIC_TI_PCI1131:
		    case PCIC_TI_PCI1250:
		    case PCIC_TI_PCI1221:
		    case PCIC_TI_PCI1031:
			cfg = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_DEVCTL_REG);
			cfg &= ~PCIC_DEVCTL_INTR_MASK;
			switch (pcic->pc_intr_mode) {
			case PCIC_INTR_MODE_ISA:
				cfg |= PCIC_DEVCTL_INTR_ISA;
				break;
			}
			ddi_put8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_DEVCTL_REG,
					cfg);
			cfg = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_CRDCTL_REG);
			cfg &= ~(PCIC_CRDCTL_PCIINTR|PCIC_CRDCTL_PCICSC|
					PCIC_CRDCTL_PCIFUNC);
			switch (pcic->pc_intr_mode) {
			case PCIC_INTR_MODE_PCI_1:
				cfg |= PCIC_CRDCTL_PCIINTR |
					PCIC_CRDCTL_PCICSC |
						PCIC_CRDCTL_PCIFUNC;
				pcic->pc_flags |= PCF_USE_SMI;
				break;
			}
			ddi_put8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_CRDCTL_REG,
					cfg);
			break;
		    default:
			break;
		} /* switch */

		/* setup general card status change interrupt */
		pcic_putb(pcic, i, PCIC_MANAGEMENT_INT,
				PCIC_CHANGE_DEFAULT |
				(pcic->pc_sockets[i].pcs_smi << 4));
		pcic->pc_flags |= PCF_INTRENAB;

		/* take card out of RESET */
		pcic_putb(pcic, i, PCIC_INTERRUPT, PCIC_RESET);
		/* turn power off and let CS do this */
		pcic_putb(pcic, i, PCIC_POWER_CONTROL, 0);

		/* final chip specific initialization */
		switch (pcic->pc_type) {
		    case PCIC_VADEM:
			pcic_putb(pcic, i, PCIC_VG_CONTROL,
					PCIC_VC_DELAYENABLE);
			pcic->pc_flags |= PCF_DEBOUNCE;
			/* FALLTHROUGH */
		    case PCIC_I82365SL:
			pcic_putb(pcic, i, PCIC_GLOBAL_CONTROL,
					PCIC_GC_CSC_WRITE);
			/* clear any pending interrupts */
			value = pcic_getb(pcic, i, PCIC_CARD_STATUS_CHANGE);
			pcic_putb(pcic, i, PCIC_CARD_STATUS_CHANGE, value);
			break;
		    /* The 82092 uses PCI config space to enable interrupts */
		    case PCIC_INTEL_i82092:
			pcic_82092_smiirq_ctl(pcic, i, PCIC_82092_CTL_SMI,
							PCIC_82092_INT_ENABLE);
			break;
		    case PCIC_CL_PD6729:
			if (pcic->bus_speed >= PCIC_PCI_DEF_SYSCLK && i == 0) {
				value = pcic_getb(pcic, i, PCIC_MISC_CTL_2);
				pcic_putb(pcic, i, PCIC_MISC_CTL_2,
						value | PCIC_CL_TIMER_CLK_DIV);
			}
			break;
		} /* switch */

		value = pcic_getb(pcic, i, PCIC_INTERFACE_STATUS);
		if ((value & PCIC_ISTAT_CD_MASK) == PCIC_CD_PRESENT_OK) {
			pcic->pc_sockets[i].pcs_flags |= PCS_CARD_PRESENT;
		}
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT,
				"socket %d value=%x, flags = %x (%s)\n",
				i, value, pcic->pc_sockets[i].pcs_flags,
				(pcic->pc_sockets[i].pcs_flags &
					PCS_CARD_PRESENT) ?
						"card present" : "no card");
#endif
	}
}

/*
 * pcic_intr(caddr_t)
 *	interrupt handler for the PCIC style adapter
 *	handles all basic interrupts and also checks
 *	for status changes and notifies the nexus if
 *	necessary
 *
 *	On PCI bus adapters, also handles all card
 *	IO interrupts.
 */
uint32_t
pcic_intr(caddr_t arg)
{
	pcicdev_t *pcic = (pcicdev_t *)arg;
	int value, i, status, ret = DDI_INTR_UNCLAIMED;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_intr: enter pc_flags=0X%x PCF_ATTACHED=0X%x\n",
			pcic->pc_flags, PCF_ATTACHED);
	}
#endif

	if (!(pcic->pc_flags & PCF_ATTACHED))
	    return (DDI_INTR_UNCLAIMED);

	mutex_enter(&pcic->pc_lock);

	/*
	 * need to change to only ACK and touch the slot that
	 * actually caused the interrupt.  Currently everything
	 * is acked
	 *
	 * we need to look at all known sockets to determine
	 * what might have happened, so step through the list
	 * of them
	 */

	for (i = 0; i < pcic->pc_numsockets; i++) {
		int card_type;
		pcic_socket_t *sockp;

		sockp = &pcic->pc_sockets[i];
		/* get the socket's I/O addresses */

		if (sockp->pcs_flags & PCS_CARD_IO)
			card_type = IF_IO;
		else
			card_type = IF_MEMORY;

		if ((value = pcic_change(pcic, i)) != 0) {
			int x = pcic->pc_cb_arg;

			ret = DDI_INTR_CLAIMED;

#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT,
					"\tchange on socket %d (%x)\n", i,
					value);
#endif
			/* acknowledge the interrupt */
			pcic_putb(pcic, i, PCIC_CARD_STATUS_CHANGE, value);

			/* find out what happened */
			status = pcic_getb(pcic, i, PCIC_INTERFACE_STATUS);

			if (pcic->pc_callback == NULL) {
				/* if not callback handler, nothing to do */
				continue;
			}

			/* Card Detect */
			if (value & PCIC_CD_DETECT) {
#if defined(PCIC_DEBUG)
				if (pcic_debug)
					cmn_err(CE_CONT,
						"\tcd_detect: status=%x,"
						" flags=%x\n",
						status, sockp->pcs_flags);
#else
#ifdef lint
				if (status == 0)
				    status++;
#endif
#endif

				pcic_handle_cd_change(pcic, sockp, i);
			} /* PCIC_CD_DETECT */

			/* Ready/Change Detect */
			sockp->pcs_state ^= SBM_RDYBSY;
			if (card_type == IF_MEMORY && value & PCIC_RD_DETECT) {
				sockp->pcs_flags |= PCS_READY;
				PC_CALLBACK(pcic->dip, x, PCE_CARD_READY, i);
			}

			/* Battery Warn Detect */
			if (card_type == IF_MEMORY &&
			    value & PCIC_BW_DETECT &&
			    !(sockp->pcs_state & SBM_BVD2)) {
				sockp->pcs_state |= SBM_BVD2;
				PC_CALLBACK(pcic->dip, x,
						PCE_CARD_BATTERY_WARN, i);
			}

			/* Battery Dead Detect */
			if (value & PCIC_BD_DETECT) {
				/*
				 * need to work out event if RI not enabled
				 * and card_type == IF_IO
				 */
				if (card_type == IF_MEMORY &&
					!(sockp->pcs_state & SBM_BVD1)) {
					sockp->pcs_state |= SBM_BVD1;
					PC_CALLBACK(pcic->dip, x,
							PCE_CARD_BATTERY_DEAD,
							i);
				} else {
					/*
					 * information in pin replacement
					 * register if one is available
					 */
					PC_CALLBACK(pcic->dip, x,
							PCE_CARD_STATUS_CHANGE,
							i);
				} /* IF_MEMORY */
			} /* PCIC_BD_DETECT */
		} /* if pcic_change */
		/*
		 * for any controllers that we can detect whether a socket
		 * had an interrupt for the PC Card, we should sort that out
		 * here.
		 */
	} /* for pc_numsockets */

	/*
	 * If we're on a PCI bus, we may need to cycle through each IO
	 *	interrupt handler that is registered since they all
	 *	share the same interrupt line.
	 */


#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_intr: pc_intr_mode=%d pc_type=%x\n",
			pcic->pc_intr_mode, pcic->pc_type);
	}
#endif

	switch (pcic->pc_intr_mode) {
	case PCIC_INTR_MODE_PCI_1:
		value = ~0;
		/* fix value if adapter can tell us about I/O intrs */
		switch (pcic->pc_type) {
		case PCIC_TI_PCI1031:
		case PCIC_TI_PCI1130:
		case PCIC_TI_PCI1131:
		case PCIC_TI_PCI1250:
		case PCIC_TI_PCI1221:
			i = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_CRDCTL_REG);
			if (i & PCIC_CRDCTL_IFG)
				value = 1;
			else
				value = 0;
			i &= ~PCIC_CRDCTL_IFG;
			/* clear the condition */
			ddi_put8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_CRDCTL_REG, i);
			break;
		}
		if (value && pcic_do_io_intr(pcic, value) == DDI_INTR_CLAIMED)
			ret = DDI_INTR_CLAIMED;
		break;
	default:
		break;
	}

	mutex_exit(&pcic->pc_lock);

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_intr: ret=%d value=%d DDI_INTR_CLAIMED=%d\n",
			ret, value, DDI_INTR_CLAIMED);
	}
#endif

	return (ret);
}

/*
 * pcic_change()
 *	check to see if this socket had a change in state
 *	by checking the status change register
 */
static int
pcic_change(pcicdev_t *pcic, int socket)
{

	return (pcic_getb(pcic, socket, PCIC_CARD_STATUS_CHANGE));
}

/*
 * pcic_do_io_intr - calls client interrupt handlers
 */
static int
pcic_do_io_intr(pcicdev_t *pcic, uint32_t sockets)
{
	inthandler_t *tmp;
	int ret = DDI_INTR_UNCLAIMED;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_do_io_intr: pcic=%p sockets=%d irq_top=%p\n",
			(void *)pcic, (int)sockets, (void *)pcic->irq_top);
	}
#endif

	if (pcic->irq_top != NULL) {
	    tmp = pcic->irq_current;

	    do {
		int cur = pcic->irq_current->socket;
		pcic_socket_t *sockp =
				&pcic->pc_sockets[cur];

#if defined(PCIC_DEBUG)
		if (pcic_debug) {
			cmn_err(CE_CONT,
			"\t pcs_flags=0X%x PCS_CARD_PRESENT=0X%x\n",
			sockp->pcs_flags, PCS_CARD_PRESENT);
			cmn_err(CE_CONT,
				"\t sockets=%d cur=%d intr=%p arg=%p\n",
				sockets, cur, (void *)pcic->irq_current->intr,
				pcic->irq_current->arg);
		}
#endif
		if (sockp->pcs_flags & PCS_CARD_PRESENT &&
		    sockets & (1 << cur)) {

		    if ((*pcic->irq_current->intr)(pcic->irq_current->arg) ==
							DDI_INTR_CLAIMED)
			ret = DDI_INTR_CLAIMED;

#if defined(PCIC_DEBUG)
			if (pcic_debug) {
				cmn_err(CE_CONT,
					"\t ret=%d DDI_INTR_CLAIMED=%d\n",
					ret, DDI_INTR_CLAIMED);
			}
#endif

		}


		if ((pcic->irq_current = pcic->irq_current->next) == NULL)
					pcic->irq_current = pcic->irq_top;

	    } while (pcic->irq_current != tmp);

	    if ((pcic->irq_current = pcic->irq_current->next) == NULL)
					pcic->irq_current = pcic->irq_top;

	} else {
		ret = DDI_INTR_CLAIMED;
	}

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_do_io_intr: exit ret=%d DDI_INTR_CLAIMED=%d\n",
			ret, DDI_INTR_CLAIMED);
	}
#endif

	return (ret);

}

/*
 * pcic_inquire_adapter()
 *	SocketServices InquireAdapter function
 *	get characteristics of the physical adapter
 */
static
pcic_inquire_adapter(dev_info_t *dip, inquire_adapter_t *config)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;

#ifdef lint
	dip = dip;
#endif
	config->NumSockets = pcic->pc_numsockets;
	config->NumWindows = pcic->pc_numsockets * PCIC_NUMWINSOCK;
	config->NumEDCs = 0;
	config->AdpCaps = 0;
	config->ActiveHigh = 0;
	config->ActiveLow = PCIC_AVAIL_IRQS;
	config->NumPower = pcic->pc_numpower;
	config->power_entry = pcic->pc_power; /* until we resolve this */
#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_inquire_adapter:\n");
		cmn_err(CE_CONT, "\tNumSockets=%d\n", config->NumSockets);
		cmn_err(CE_CONT, "\tNumWindows=%d\n", config->NumWindows);
	}
#endif
	config->ResourceFlags = 0;
	switch (pcic->pc_intr_mode) {
	case PCIC_INTR_MODE_PCI_1:
		config->ResourceFlags |= RES_OWN_IRQ | RES_IRQ_NEXUS |
			RES_IRQ_SHAREABLE;
		break;
	}
	return (SUCCESS);
}

/*
 * pcic_callback()
 *	The PCMCIA nexus calls us via this function
 *	in order to set the callback function we are
 *	to call the nexus with
 */
static
pcic_callback(dev_info_t *dip, int (*handler)(), int arg)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;

#ifdef lint
	dip = dip;
#endif
	if (handler != NULL) {
		pcic->pc_callback = handler;
		pcic->pc_cb_arg  = arg;
		pcic->pc_flags |= PCF_CALLBACK;
	} else {
		pcic->pc_callback = NULL;
		pcic->pc_cb_arg = 0;
		pcic->pc_flags &= ~PCF_CALLBACK;
	}
	/*
	 * we're now registered with the nexus
	 * it is acceptable to do callbacks at this point.
	 * don't call back from here though since it could block
	 */
	return (PC_SUCCESS);
}

/*
 * pcic_calc_speed (pcicdev_t *pcic, uint32_t speed)
 *	calculate the speed bits from the specified memory speed
 *	there may be more to do here
 */

static
pcic_calc_speed(pcicdev_t *pcic, uint32_t speed)
{
	uint32_t wspeed = 1;	/* assume 1 wait state when unknown */
	uint32_t bspeed = PCIC_ISA_DEF_SYSCLK;

	switch (pcic->pc_type) {
	    case PCIC_I82365SL:
	    case PCIC_VADEM:
	    case PCIC_VADEM_VG469:
	    default:
		/* Intel chip wants it in waitstates */
		wspeed = mhztons(PCIC_ISA_DEF_SYSCLK) * 3;
		if (speed <= wspeed)
			wspeed = 0;
		else if (speed <= (wspeed += mhztons(bspeed)))
			wspeed = 1;
		else if (speed <= (wspeed += mhztons(bspeed)))
			wspeed = 2;
		else
			wspeed = 3;
		wspeed <<= 6; /* put in right bit positions */
		break;

	    case PCIC_INTEL_i82092:
		wspeed = SYSMEM_82092_80NS;
		if (speed > 80)
		    wspeed = SYSMEM_82092_100NS;
		if (speed > 100)
		    wspeed = SYSMEM_82092_150NS;
		if (speed > 150)
		    wspeed = SYSMEM_82092_200NS;
		if (speed > 200)
		    wspeed = SYSMEM_82092_250NS;
		if (speed > 250)
		    wspeed = SYSMEM_82092_600NS;
		wspeed <<= 5;	/* put in right bit positions */
		break;

#ifdef	XXX
	/* The Cirrus Logic calulations are done by the caller now */
	case PCIC_CL_PD6710:
		/* Cirrus chip has timing registers */
		/* later we want to check timer registers */
		if (speed > wspeed) {
			/* use second register set */
			wspeed = 1;
		} else {
			wspeed = 0;
		}
		wspeed <<= 6; /* put in right bit positions */
		break;
#endif
	} /* switch */

	return (wspeed);
}

/*
 * pcic_set_cdtimers
 *	This is specific to several Cirrus Logic chips
 */
static void
pcic_set_cdtimers(pcicdev_t *pcic, int socket, uint32_t speed, int tset)
{
	int cmd, set, rec, offset;

	if ((tset == IOMEM_CLTIMER_SET_1) || (tset == SYSMEM_CLTIMER_SET_1))
		offset = 3;
	else
		offset = 0;

	/* command timer */
#define	PCIC_CALC(time)		((((time) + 10) / 40) - 1)
	cmd = PCIC_CALC(speed);
	if (cmd < 0)
		cmd = 0;
	/* setup timer */
#define	PCIC_SETUP(time)	(((time) + 69) / 4)
	set = PCIC_CALC(PCIC_SETUP(speed));
	if (set < 0)
		set = 0;
	/* recovery timer */
#define	PCIC_RECOVER(time)	(((time) + 29) / 9)
	rec = 0;
	if (rec < 0)
		rec = 0;

#if	defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_set_cdtimers(%x) "
			"cmd=%x, set=%x, rec=%x\n",
			(unsigned)speed, cmd, set, rec);
	}
#endif

	pcic_putb(pcic, socket, PCIC_TIME_COMMAND_0 + offset, cmd);
	pcic_putb(pcic, socket, PCIC_TIME_SETUP_0 + offset, set);
	pcic_putb(pcic, socket, PCIC_TIME_RECOVER_0 + offset, rec);
}

/*
 * pcic_set_window
 *	essentially the same as the Socket Services specification
 *	We use socket and not adapter since they are identifiable
 *	but the rest is the same
 *
 *	dip	pcic driver's device information
 *	window	parameters for the request
 */
pcic_set_window(dev_info_t *dip, set_window_t *window)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	register int select;
	int socket, pages, which, ret;
	pcic_socket_t *sockp = &pcic->pc_sockets[window->socket];
	ra_return_t res;
	ndi_ra_request_t req;
	uint32_t base = window->base;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_set_window: entered\n");
		cmn_err(CE_CONT,
			"\twindow=%d, socket=%d, WindowSize=%d, speed=%d\n",
			window->window, window->socket, window->WindowSize,
			window->speed);
		cmn_err(CE_CONT,
			"\tbase=%x, state=%x\n", (unsigned)window->base,
			(unsigned)window->state);
	}
#endif

	/*
	 * do some basic sanity checking on what we support
	 * we don't do paged mode
	 */
	if (window->state & WS_PAGED)
		return (BAD_ATTRIBUTE);

	/*
	 * we don't care about previous mappings.
	 * Card Services will deal with that so don't
	 * even check
	 */

	socket = window->socket;

	if (!(window->state & WS_IO)) {
		int win, tmp;
		pcs_memwin_t *memp;
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "\twindow type is memory\n");
#endif
		/* this is memory window mapping */
		win = window->window % PCIC_NUMWINSOCK;
		tmp = window->window / PCIC_NUMWINSOCK;

		/* only windows 2-6 can do memory mapping */
		if (tmp != window->socket || win < PCIC_IOWINDOWS) {
#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT,
					"\tattempt to map to non-mem window\n");
#endif
			return (BAD_WINDOW);
		}

		if (window->WindowSize == 0)
			window->WindowSize = MEM_MIN;
		else if ((window->WindowSize & (PCIC_PAGE-1)) != 0) {
			return (BAD_SIZE);
		}

		mutex_enter(&pcic->pc_lock); /* protect the registers */

		/*
		 * Don't call this if the socket is in IO mode since
		 *	the READY bit is in the PRR and not on the
		 *	socket interface
		 */
		pcic_ready_wait(pcic, socket);

		memp = &sockp->pcs_windows[win].mem;
		memp->pcw_speed = window->speed;

		win -= PCIC_IOWINDOWS; /* put in right range */

		if (window->WindowSize != memp->pcw_len)
			which = memp->pcw_len;
		else
			which = 0;

		if (window->state & WS_ENABLED) {
			uint32_t wspeed;
#if defined(PCIC_DEBUG)
			if (pcic_debug) {
				cmn_err(CE_CONT,
					"\tbase=%x, win=%d\n", (unsigned)base,
					win);
				if (which)
					cmn_err(CE_CONT,
						"\tneed to remap window\n");
			}
#endif

			if (which && (memp->pcw_status & PCW_MAPPED)) {
				ddi_regs_map_free(&memp->pcw_handle);
				res.ra_addr_lo = memp->pcw_base;
				res.ra_len = memp->pcw_len;
				(void) pcmcia_free_mem(dip, &res);
				memp->pcw_status &= ~(PCW_MAPPED|PCW_ENABLED);
				memp->pcw_hostmem = NULL;
				memp->pcw_base = NULL;
				memp->pcw_len = 0;
			}

			which = window->WindowSize >> PAGE_SHIFT;

			if (!(memp->pcw_status & PCW_MAPPED)) {
				ret = 0;

				memp->pcw_base = base;
				bzero((caddr_t)&req, sizeof (req));
				req.ra_len = which << PAGE_SHIFT;
				req.ra_addr = (uint64_t)memp->pcw_base;
				req.ra_boundbase = pcic->pc_base;
				req.ra_boundlen  = pcic->pc_bound;
				req.ra_flags = (memp->pcw_base ?
					NDI_RA_ALLOC_SPECIFIED : 0) |
					NDI_RA_ALLOC_BOUNDED;
				req.ra_align_mask = PCIC_PAGE - 1;
				ret = pcmcia_alloc_mem(dip, &req, &res);
				if (ret == DDI_FAILURE) {
					mutex_exit(&pcic->pc_lock);
					return (BAD_SIZE);
				}
				memp->pcw_base = res.ra_addr_lo;
				base = memp->pcw_base;

#if defined(PCIC_DEBUG)
				if (pcic_debug)
					cmn_err(CE_CONT,
						"\tsetwindow: new base=%x\n",
						(unsigned)memp->pcw_base);
#endif
				memp->pcw_len = window->WindowSize;

				which = pcmcia_map_reg(pcic->dip,
						window->child,
						&res,
						(uint32_t)window->state,
						(caddr_t *)&memp->pcw_hostmem,
						&memp->pcw_handle,
						&window->attr, NULL);

				if (which != DDI_SUCCESS) {
				    res.ra_addr_lo = memp->pcw_base;
				    res.ra_len = memp->pcw_len;
				    (void) pcmcia_free_mem(pcic->dip, &res);

				    mutex_exit(&pcic->pc_lock);

				    return (BAD_WINDOW);
				}
				memp->pcw_status |= PCW_MAPPED;
#if defined(PCIC_DEBUG)
				if (pcic_debug)
					cmn_err(CE_CONT,
						"\tmap=%x, hostmem=%p\n",
						which,
						(void *)memp->pcw_hostmem);
#endif
			} else {
				base = memp->pcw_base;
			}

			/* report the handle back to caller */
			window->handle = memp->pcw_handle;

#if defined(PCIC_DEBUG)
			if (pcic_debug) {
				cmn_err(CE_CONT,
					"\twindow mapped to %x@%x len=%d\n",
					(unsigned)window->base,
					(unsigned)memp->pcw_base,
					memp->pcw_len);
			}
#endif
			if (sockp->pcs_flags & PCS_READY) {
				/* shouldn't happen but who knows */
				pcic_mswait(10);
			}

			/* find the register set offset */
			select = win * PCIC_MEM_1_OFFSET;
#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT, "\tselect=%x\n", select);
#endif

			/*
			 * at this point, the register window indicator has
			 * been converted to be an offset from the first
			 * set of registers that are used for programming
			 * the window mapping and the offset used to select
			 * the correct set of registers to access the
			 * specified socket.  This allows basing everything
			 * off the _0 window
			 */

			/* map the physical page base address */
			which = (window->state & WS_16BIT) ? SYSMEM_DATA_16 : 0;
			which |= (window->speed <= MEM_SPEED_MIN) ?
				SYSMEM_ZERO_WAIT : 0;

			/* need to select register set */
			select = PCIC_MEM_1_OFFSET * win;

			pcic_putb(pcic, socket,
					PCIC_SYSMEM_0_STARTLOW + select,
					SYSMEM_LOW(base));
			pcic_putb(pcic, socket,
					PCIC_SYSMEM_0_STARTHI + select,
					SYSMEM_HIGH(base) | which);

			/*
			 * Some adapters can decode window addresses greater
			 *	than 16-bits worth, so handle them here.
			 * XXX This is really a per-socket resource and perhaps
			 *	should be set somewhere else.
			 */
			switch (pcic->pc_type) {
			case PCIC_INTEL_i82092:
				pcic_putb(pcic, socket,
						PCIC_82092_CPAGE,
						SYSMEM_EXT(base));
				break;
			case PCIC_CL_PD6729:
			case PCIC_CL_PD6730:
				clext_reg_write(pcic, socket,
						PCIC_CLEXT_MMAP0_UA + win,
						SYSMEM_EXT(base));
				break;
			case PCIC_TI_PCI1130:
				/*
				 * Note that the TI chip has one upper byte
				 * per socket so all windows get bound to a
				 * 16MB segment.  This must be detected and
				 * handled appropriately.  We can detect that
				 * it is done by seeing if the pc_base has
				 * changed and changing when the register
				 * is first set.  This will force the bounds
				 * to be correct.
				 */
				if (pcic->pc_bound == 0xffffffff) {
					pcic_putb(pcic, socket,
						    PCIC_TI_WINDOW_PAGE_PCI,
						    SYSMEM_EXT(base));
					pcic->pc_base = SYSMEM_EXT(base) << 24;
					pcic->pc_bound = 0x1000000;
				}
				break;
			case PCIC_TI_PCI1031:
			case PCIC_TI_PCI1131:
			case PCIC_TI_PCI1250:
			case PCIC_TI_PCI1221:
			case PCIC_SMC_34C90:
			case PCIC_CL_PD6832:
			case PCIC_RICOH_RL5C466:
				pcic_putb(pcic, socket,
						PCIC_YENTA_MEM_PAGE + win,
						SYSMEM_EXT(base));
				break;
			default:
				if (SYSMEM_EXT(base) != 0)
					return (BAD_BASE);
				break;
			} /* switch */

			/*
			 * specify the length of the mapped range
			 * we convert to pages (rounding up) so that
			 * the hardware gets the right thing
			 */
			pages = (window->WindowSize+PCIC_PAGE-1)/PCIC_PAGE;

			/*
			 * Setup this window's timing.
			 */
			switch (pcic->pc_type) {
			case PCIC_CL_PD6729:
			case PCIC_CL_PD6730:
			case PCIC_CL_PD6710:
			case PCIC_CL_PD6722:
				wspeed = SYSMEM_CLTIMER_SET_0;
				pcic_set_cdtimers(pcic, socket,
							window->speed,
							wspeed);
				break;

			case PCIC_INTEL_i82092:
			default:
				wspeed = pcic_calc_speed(pcic, window->speed);
				break;
			} /* switch */

#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT,
					"\twindow %d speed bits = %x for "
					"%dns\n",
					win, (unsigned)wspeed, window->speed);
#endif

			pcic_putb(pcic, socket, PCIC_SYSMEM_0_STOPLOW + select,
					SYSMEM_LOW(base +
						    (pages * PCIC_PAGE)-1));

			wspeed |= SYSMEM_HIGH(base + (pages * PCIC_PAGE)-1);
			pcic_putb(pcic, socket, PCIC_SYSMEM_0_STOPHI + select,
					wspeed);

			/*
			 * now map the card's memory pages - we start with page
			 * 0
			 * we also default to AM -- set page might change it
			 */
			base = memp->pcw_base;
			pcic_putb(pcic, socket,
					PCIC_CARDMEM_0_LOW + select,
					CARDMEM_LOW(0 - (uint32_t)base));

			pcic_putb(pcic, socket,
					PCIC_CARDMEM_0_HI + select,
					CARDMEM_HIGH(0 - (uint32_t)base) |
					CARDMEM_REG_ACTIVE);

			/*
			 * enable the window even though redundant
			 * and SetPage may do it again.
			 */
			select = pcic_getb(pcic, socket,
					PCIC_MAPPING_ENABLE);
			select |= SYSMEM_WINDOW(win);
			pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE, select);
			memp->pcw_offset = 0;
			memp->pcw_status |= PCW_ENABLED;
			pcic_mswait(1);
		} else {
			/*
			 * not only do we unmap the memory, the
			 * window has been turned off.
			 */
			if (which && memp->pcw_status & PCW_MAPPED) {
				select = pcic_getb(pcic, socket,
							PCIC_MAPPING_ENABLE) |
							~SYSMEM_WINDOW(win);
				pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE,
						select);
				ddi_regs_map_free(&memp->pcw_handle);
				res.ra_addr_lo = memp->pcw_base;
				res.ra_len = memp->pcw_len;
				(void) pcmcia_free_mem(pcic->dip, &res);
				memp->pcw_hostmem = NULL;
				memp->pcw_status &= ~(PCW_MAPPED|PCW_ENABLED);
			}
		}
		memp->pcw_len = window->WindowSize;
		window->handle = memp->pcw_handle;
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			xxdmp_all_regs(pcic, window->socket, -1);
#endif
	} else {
		/*
		 * This is a request for an IO window
		 */
		int win, tmp;
		pcs_iowin_t *winp;
				/* I/O windows */
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "\twindow type is I/O\n");
#endif

		/* only windows 0 and 1 can do I/O */
		win = window->window % PCIC_NUMWINSOCK;
		tmp = window->window / PCIC_NUMWINSOCK;

		if (win >= PCIC_IOWINDOWS || tmp != window->socket) {
#if defined(PCIC_DEBUG)
			if (pcic_debug)
				cmn_err(CE_CONT,
					"\twindow is out of range (%d)\n",
					window->window);
#endif
			return (BAD_WINDOW);
		}

		mutex_enter(&pcic->pc_lock); /* protect the registers */

		winp = &sockp->pcs_windows[win].io;
		winp->pcw_speed = window->speed;
		if (window->WindowSize != 1 && window->WindowSize & 1) {
			/* we don't want an odd-size window */
			window->WindowSize++;
		}
		winp->pcw_len = window->WindowSize;

		if (window->state & WS_ENABLED) {
			/*
			 * Don't call this if the socket is in IO mode since
			 *	the READY bit is in the PRR and not on the
			 *	socket interface.
			 * XXX Really should fix this once and for all.
			 */
			pcic_ready_wait(pcic, socket);

			if (winp->pcw_status & PCW_MAPPED) {
				ddi_regs_map_free(&winp->pcw_handle);
				res.ra_addr_lo = winp->pcw_base;
				res.ra_len = winp->pcw_len;
				(void) pcmcia_free_io(pcic->dip, &res);
				winp->pcw_status &= ~(PCW_MAPPED|PCW_ENABLED);
			}

			/*
			 * if the I/O address wasn't allocated, allocate
			 *	it now. If it was allocated, it better
			 *	be free to use.
			 * The winp->pcw_offset value is set and used
			 *	later on if the particular adapter
			 *	that we're running on has the ability
			 *	to translate IO accesses to the card
			 *	(such as some adapters  in the Cirrus
			 *	Logic family).
			 */
			winp->pcw_offset = 0;

			/*
			 * Setup the request parameters for the
			 *	requested base and length. If
			 *	we're on an adapter that has
			 *	IO window offset registers, then
			 *	we don't need a specific base
			 *	address, just a length, and then
			 *	we'll cause the correct IO address
			 *	to be generated on the socket by
			 *	setting up the IO window offset
			 *	registers.
			 * For adapters that support this capability, we
			 *	always use the IO window offset registers,
			 *	even if the passed base/length would be in
			 *	range.
			 */
			base = window->base;
			bzero((caddr_t)&req, sizeof (req));
			req.ra_len = window->WindowSize;

			req.ra_addr = (uint64_t)
				((pcic->pc_flags & PCF_IO_REMAP) ? 0 : base);
			req.ra_flags = (req.ra_addr) ?
						NDI_RA_ALLOC_SPECIFIED : 0;

			req.ra_flags |= NDI_RA_ALIGN_SIZE;
			/* need to rethink this */
			req.ra_boundbase = pcic->pc_iobase;
			req.ra_boundlen = pcic->pc_iobound;
			req.ra_flags |= NDI_RA_ALLOC_BOUNDED;

			/*
			 * Try to allocate the space. If we fail this,
			 *	return the appropriate error depending
			 *	on whether the caller specified a
			 *	specific base address or not.
			 */
			if (pcmcia_alloc_io(dip, &req, &res) == DDI_FAILURE) {
				winp->pcw_status &= ~PCW_ENABLED;
				mutex_exit(&pcic->pc_lock);
				return (base?BAD_BASE:BAD_SIZE);
			} /* pcmcia_alloc_io */

			window->base = winp->pcw_base = res.ra_addr_lo;

			if ((which = pcmcia_map_reg(pcic->dip,
						window->child,
						&res,
						(uint32_t)window->state,
						(caddr_t *)&winp->pcw_hostmem,
						&winp->pcw_handle,
						&window->attr,
						base)) != DDI_SUCCESS) {

				    res.ra_addr_lo = winp->pcw_base;
				    res.ra_len = winp->pcw_len;
				    (void) pcmcia_free_io(pcic->dip, &res);

				    mutex_exit(&pcic->pc_lock);
				    return (BAD_WINDOW);
			}

			window->handle = winp->pcw_handle;
			winp->pcw_status |= PCW_MAPPED;

			/* find the register set offset */
			select = win * PCIC_IO_OFFSET;

#if defined(PCIC_DEBUG)
			if (pcic_debug) {
				cmn_err(CE_CONT,
					"\tenable: window=%d, select=%x, "
					"base=%x, handle=%p\n",
					win, select,
					(unsigned)window->base,
					window->handle);
			}
#endif
			/*
			 * at this point, the register window indicator has
			 * been converted to be an offset from the first
			 * set of registers that are used for programming
			 * the window mapping and the offset used to select
			 * the correct set of registers to access the
			 * specified socket.  This allows basing everything
			 * off the _0 window
			 */

			/* map the I/O base in */
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STARTLOW + select,
					LOW_BYTE((uint32_t)winp->pcw_base));
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STARTHI + select,
					HIGH_BYTE((uint32_t)winp->pcw_base));

			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STOPLOW + select,
					LOW_BYTE((uint32_t)winp->pcw_base +
						window->WindowSize - 1));
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STOPHI + select,
					HIGH_BYTE((uint32_t)winp->pcw_base +
						window->WindowSize - 1));

			/*
			 * We've got the requested IO space, now see if we
			 *	need to adjust the IO window offset registers
			 *	so that the correct IO address is generated
			 *	at the socket. If this window doesn't have
			 *	this capability, then we're all done setting
			 *	up the IO resources.
			 */
			if (pcic->pc_flags & PCF_IO_REMAP) {

				winp->pcw_offset =
					((base - winp->pcw_base) & 0x0ffff);
				pcic_putb(pcic, socket,
					PCIC_IO_OFFSET_LOW +
					(win * PCIC_IO_OFFSET_OFFSET),
					winp->pcw_offset & 0x0ff);
				pcic_putb(pcic, socket,
					PCIC_IO_OFFSET_HI +
					(win * PCIC_IO_OFFSET_OFFSET),
					(winp->pcw_offset >> 8) & 0x0ff);

			} /* PCF_IO_REMAP */

			/* now get the other details (size, etc) right */

			/*
			 * Set the data size control bits here. Most of the
			 *	adapters will ignore IOMEM_16BIT when
			 *	IOMEM_IOCS16 is set, except for the Intel
			 *	82092, which only pays attention to the
			 *	IOMEM_16BIT bit. Sigh... Intel can't even
			 *	make a proper clone of their own chip.
			 * The 82092 also apparently can't set the timing
			 *	of I/O windows.
			 */
			which = (window->state & WS_16BIT) ?
					(IOMEM_16BIT | IOMEM_IOCS16) : 0;

			switch (pcic->pc_type) {
			case PCIC_CL_PD6729:
			case PCIC_CL_PD6730:
			case PCIC_CL_PD6710:
			case PCIC_CL_PD6722:

				/*
				 * Select Timer Set 1 - this will take
				 *	effect when the PCIC_IO_CONTROL
				 *	register is written to later on;
				 *	the call to pcic_set_cdtimers
				 *	just sets up the timer itself.
				 */
				which |= IOMEM_CLTIMER_SET_1;
				pcic_set_cdtimers(pcic, socket,
							window->speed,
							IOMEM_CLTIMER_SET_1);
				which |= IOMEM_IOCS16;
				break;
			case PCIC_TI_PCI1031:

				if (window->state & WS_16BIT)
				    which |= IOMEM_WAIT16;

				break;
			case PCIC_TI_PCI1130:

				if (window->state & WS_16BIT)
				    which |= IOMEM_WAIT16;

				break;
			case PCIC_INTEL_i82092:
				break;
			default:
				if (window->speed >
						mhztons(pcic->bus_speed) * 3)
				    which |= IOMEM_WAIT16;
#ifdef notdef
				if (window->speed <
						mhztons(pcic->bus_speed) * 6)
				    which |= IOMEM_ZERO_WAIT;
#endif
				break;
			} /* switch (pc_type) */

			/*
			 * Setup the data width and timing
			 */
			select = pcic_getb(pcic, socket, PCIC_IO_CONTROL);
			select &= ~(PCIC_IO_WIN_MASK << (win * 4));
			select |= IOMEM_SETWIN(win, which);
			pcic_putb(pcic, socket, PCIC_IO_CONTROL, select);

			/*
			 * Enable the IO window
			 */
			select = pcic_getb(pcic, socket, PCIC_MAPPING_ENABLE);
			pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE,
						select | IOMEM_WINDOW(win));

			winp->pcw_status |= PCW_ENABLED;

#if defined(PCIC_DEBUG)
			if (pcic_debug) {
				cmn_err(CE_CONT,
					"\twhich = %x, select = %x (%x)\n",
					which, select,
					IOMEM_SETWIN(win, which));
				xxdmp_all_regs(pcic, window->socket * 0x40, 24);
			}
#endif
		} else {
			/*
			 * not only do we unmap the IO space, the
			 * window has been turned off.
			 */
			if (winp->pcw_status & PCW_MAPPED) {
				ddi_regs_map_free(&winp->pcw_handle);
				res.ra_addr_lo = winp->pcw_base;
				res.ra_len = winp->pcw_len;
				(void) pcmcia_free_io(pcic->dip, &res);
				winp->pcw_status &= ~(PCW_MAPPED|PCW_ENABLED);
			}

			/* disable current mapping */
			select = pcic_getb(pcic, socket,
						PCIC_MAPPING_ENABLE);
			pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE,
					select &= ~IOMEM_WINDOW(win));
			winp->pcw_base = 0;
			winp->pcw_len = 0;
			winp->pcw_offset = 0;
			window->base = 0;
			/* now make sure we don't accidentally re-enable */
			/* find the register set offset */
			select = win * PCIC_IO_OFFSET;
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STARTLOW + select, 0);
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STARTHI + select, 0);
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STOPLOW + select, 0);
			pcic_putb(pcic, socket,
					PCIC_IO_ADDR_0_STOPHI + select, 0);
		}
	}
	mutex_exit(&pcic->pc_lock);

	return (SUCCESS);
}

/*
 * pcic_card_state()
 *	compute the instantaneous Card State information
 */
static int
pcic_card_state(pcicdev_t *pcic, pcic_socket_t *sockp)
{
	int value, result;

	mutex_enter(&pcic->pc_lock); /* protect the registers */

	value = pcic_getb(pcic, sockp->pcs_socket, PCIC_INTERFACE_STATUS);

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "pcic_card_state(%p) if status = %b for %d\n",
			(void *)sockp,
			(long)value,
			"\020\1BVD1\2BVD2\3CD1\4CD2\5WP\6RDY\7PWR\10~GPI",
			sockp->pcs_socket);
#endif

	if (value & PCIC_WRITE_PROTECT)
		result = SBM_WP;
	else
		result = 0;
	if (value & PCIC_READY)
		result |= SBM_RDYBSY;
	if (value & PCIC_ISTAT_CD_MASK)
		result |= SBM_CD;
	value =  (~value) & (PCIC_BVD1 | PCIC_BVD2);
	if (value & PCIC_BVD1)
		result |= SBM_BVD1;
	if (value & PCIC_BVD2)
		result |= SBM_BVD2;

	mutex_exit(&pcic->pc_lock);


	return (result);
}

/*
 * pcic_set_page()
 *	SocketServices SetPage function
 *	set the page of PC Card memory that should be in the mapped
 *	window
 */

pcic_set_page(dev_info_t *dip, set_page_t *page)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int select;
	int which, socket, window;
	uint32_t base;
	register pcs_memwin_t *memp;

#ifdef lint
	dip = dip;
#endif
	/* get real socket/window numbers */
	window = page->window % PCIC_NUMWINSOCK;
	socket = page->window / PCIC_NUMWINSOCK;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_set_page: window=%d, socket=%d, page=%d\n",
			window, socket, page->page);
	}
#endif
	/* only windows 2-6 work on memory */
	if (window < PCIC_IOWINDOWS)
		return (BAD_WINDOW);

	/* only one page supported (but any size) */
	if (page->page != 0)
		return (BAD_PAGE);

	mutex_enter(&pcic->pc_lock); /* protect the registers */

	memp = &pcic->pc_sockets[socket].pcs_windows[window].mem;
	window -= PCIC_IOWINDOWS;

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "\tpcw_base=%x, pcw_hostmem=%p, pcw_len=%x\n",
			(uint32_t)memp->pcw_base,
			(void *)memp->pcw_hostmem, memp->pcw_len);
#endif

	/* window must be enabled */
	if (!(memp->pcw_status & PCW_ENABLED))
		return (BAD_ATTRIBUTE);

	/* find the register set offset */
	select = window * PCIC_MEM_1_OFFSET;
#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "\tselect=%x\n", select);
#endif

	/*
	 * now map the card's memory pages - we start with page 0
	 */

	which = 0;		/* assume simple case */
	if (page->state & PS_ATTRIBUTE) {
		which |= CARDMEM_REG_ACTIVE;
		memp->pcw_status |= PCW_ATTRIBUTE;
	} else {
		memp->pcw_status &= ~PCW_ATTRIBUTE;
	}

	/*
	 * if caller says Write Protect, enforce it.
	 * should we check to see if the card says it
	 * wants to be WP and enforce that?
	 */
	if (page->state & PS_WP) {
		which |= CARDMEM_WRITE_PROTECT;
		memp->pcw_status |= PCW_WP;
	} else {
		memp->pcw_status &= ~PCW_WP;
	}
#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "\tmemory type = %s\n",
			(which & CARDMEM_REG_ACTIVE) ? "attribute" : "common");
		if (which & CARDMEM_WRITE_PROTECT)
			cmn_err(CE_CONT, "\twrite protect\n");
		cmn_err(CE_CONT, "\tpage offset=%x pcw_base=%x (%x)\n",
			(unsigned)page->offset,
			(unsigned)memp->pcw_base,
			(int)page->offset - (int)memp->pcw_base & 0xffffff);
	}
#endif
	/* address computation based on 16MB range and not larger */
	base = (uint32_t)memp->pcw_base & 0xffffff;
	pcic_putb(pcic, socket, PCIC_CARDMEM_0_LOW + select,
	    CARDMEM_LOW((int)page->offset - (int)base));
	(void) pcic_getb(pcic, socket, PCIC_CARDMEM_0_LOW + select);
	pcic_putb(pcic, socket, PCIC_CARDMEM_0_HI + select,
	    CARDMEM_HIGH((int)page->offset - base) | which);
	(void) pcic_getb(pcic, socket, PCIC_CARDMEM_0_HI + select);

	/*
	 * while not really necessary, this just makes sure
	 * nothing turned the window off behind our backs
	 */
	which = pcic_getb(pcic, socket, PCIC_MAPPING_ENABLE);
	which |= SYSMEM_WINDOW(window);
	pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE, which);
	(void) pcic_getb(pcic, socket, PCIC_MAPPING_ENABLE);


	if (which & PCW_ATTRIBUTE)
		pcic_mswait(2);

	memp->pcw_offset = (off_t)page->offset;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "\tbase=%p, *base=%x\n",
			(void *)memp->pcw_hostmem,
			(uint32_t)*memp->pcw_hostmem);

		xxdmp_all_regs(pcic, socket, -1);

		cmn_err(CE_CONT, "\tbase=%p, *base=%x\n",
			(void *)memp->pcw_hostmem,
			(uint32_t)*memp->pcw_hostmem);
	}
#endif

	mutex_exit(&pcic->pc_lock);

	return (SUCCESS);
}

/*
 * pcic_set_vcc_level()
 *
 *	set voltage based on adapter information
 *
 *	this routine implements a limited solution for support of 3.3v cards.
 *	the general solution, which would fully support the pcmcia spec
 *	as far as allowing client drivers to request which voltage levels
 *	to be set, requires more framework support and driver changes - ess
 */
static
pcic_set_vcc_level(pcicdev_t *pcic, set_socket_t *socket)
{
	uint32_t socket_present_state;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_set_vcc_level(pcic=%p, VccLevel=%d)\n",
			(void *)pcic, socket->VccLevel);
	}
#endif

	/*
	 * check VccLevel
	 * if this is zero, power is being turned off
	 * if it is non-zero, power is being turned on.
	 */
	if (socket->VccLevel == 0) {
		return (0);
	}

	/*
	 * range checking for sanity's sake
	 */
	if (socket->VccLevel >= pcic->pc_numpower) {
		return (BAD_VCC);
	}

	switch (pcic->pc_io_type) {
	/*
	 * Yenta-compliant adapters have vcc info in the extended registers
	 * Other adapters can be added as needed, but the 'default' case
	 * has been left as it was previously so as not to break existing
	 * adapters.
	 */
	case PCIC_IO_TYPE_YENTA:
		/*
		 * Here we ignore the VccLevel passed in and read the
		 * card type from the adapter socket present state register
		 */
		socket_present_state =
			ddi_get32(pcic->handle, (uint32_t *)(pcic->ioaddr +
				PCIC_PRESENT_STATE_REG));
#if defined(PCIC_DEBUG)
		if (pcic_debug) {
			cmn_err(CE_CONT,
				"socket present state = 0x%x\n",
				socket_present_state);
		}
#endif
		switch (socket_present_state & PCIC_VCC_MASK) {
			case PCIC_VCC_3VCARD:
				/* fall through */
			case PCIC_VCC_3VCARD|PCIC_VCC_5VCARD:
				return
				    (POWER_3VCARD_ENABLE|POWER_OUTPUT_ENABLE);
			case PCIC_VCC_5VCARD:
				return
				    (POWER_CARD_ENABLE|POWER_OUTPUT_ENABLE);
			default:
				/*
				 * if no card is present, this can be the
				 * case of a client making a SetSocket call
				 * after card removal. In this case we return
				 * the current power level
				 */
				return ((unsigned)ddi_get8(pcic->handle,
				    pcic->ioaddr + CB_R2_OFFSET +
					PCIC_POWER_CONTROL));
		}

	default:

		/* this appears to be very implementation specific */

		/* valid Vcc power level? */
		switch (socket->VccLevel) {
		case PCIC_VCC_3VLEVEL:
			return (BAD_VCC);
		case PCIC_VCC_5VLEVEL:
			/* enable Vcc */
			return (POWER_CARD_ENABLE|POWER_OUTPUT_ENABLE);
		default:
			return (BAD_VCC);
		}
	}
}


/*
 * pcic_set_socket()
 *	Socket Services SetSocket call
 *	sets basic socket configuration
 */
static
pcic_set_socket(dev_info_t *dip, set_socket_t *socket)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	pcic_socket_t *sockp = &pcic->pc_sockets[socket->socket];
	int irq, interrupt;
	int powerlevel = 0;
	int ind, value;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_set_socket(dip=%x, socket=%d)\n", (unsigned)dip,
			socket->socket);
	}
#endif
	/* set VccLevel based on adapter type */
	if ((powerlevel = pcic_set_vcc_level(pcic, socket)) == BAD_VCC) {
		return (BAD_VCC);
	}
	sockp->pcs_vcc = socket->VccLevel;
#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "\tVcc=%d Vpp1Level=%d, Vpp2Level=%d\n",
			socket->VccLevel,
			socket->Vpp1Level, socket->Vpp2Level);
	}
#endif
	ind = 0;		/* default index to 0 power */
	if ((int)socket->Vpp1Level >= 0 &&
	    socket->Vpp1Level < pcic->pc_numpower) {
		if (!(pcic_power[socket->Vpp1Level].ValidSignals & VPP1)) {
			return (BAD_VPP);
		}
		ind = pcic_power[socket->Vpp1Level].PowerLevel/10;
		powerlevel |= pcic_vpp_levels[ind];
		sockp->pcs_vpp1 = socket->Vpp1Level;
	}
	if ((int)socket->Vpp2Level >= 0 &&
	    socket->Vpp2Level < pcic->pc_numpower) {
		if (!(pcic_power[socket->Vpp2Level].ValidSignals & VPP2)) {
			return (BAD_VPP);
		}
		ind = pcic_power[socket->Vpp2Level].PowerLevel/10;
		powerlevel |= (pcic_vpp_levels[ind] << 2);
		sockp->pcs_vpp2 = socket->Vpp2Level;
	}

	if (pcic->pc_flags & PCF_VPPX) {
		/*
		 * this adapter doesn't allow separate Vpp1/Vpp2
		 * if one is turned on, both are turned on and only
		 * the Vpp1 bits should be set
		 */
		if (sockp->pcs_vpp2 != sockp->pcs_vpp1) {
			/* must be the same if one not zero */
			if (sockp->pcs_vpp1 != 0 && sockp->pcs_vpp2 != 0)
				return (BAD_VPP);
			if (sockp->pcs_vpp2 != 0) {
				powerlevel = (powerlevel &
					(3 | POWER_CARD_ENABLE)) |
						((powerlevel >> 2) & 0x3);
			}
		}
	}

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "\tpowerlevel=%x, ind=%x\n", powerlevel, ind);
	}
#endif
	mutex_enter(&pcic->pc_lock); /* protect the registers */
	/*
	 * should check to see if a BIOS version exists
	 * and use it if present. This would be a "different"
	 * driver but would be more reliable.
	 */

	if (sockp->pcs_flags & PCS_READY) {
		/*
		 * card just came ready.
		 * make sure enough time elapses
		 * before touching it.
		 */
		sockp->pcs_flags &= ~PCS_READY;
		pcic_mswait(10);
	}

	/* turn socket->IREQRouting off while programming */
	interrupt = pcic_getb(pcic, socket->socket, PCIC_INTERRUPT);
	interrupt &= ~PCIC_INTR_MASK;
	if (pcic->pc_flags & PCF_USE_SMI)
		interrupt |= PCIC_INTR_ENABLE;
	pcic_putb(pcic, socket->socket, PCIC_INTERRUPT, PCIC_RESET|interrupt);

	switch (pcic->pc_type) {
	    case PCIC_INTEL_i82092:
		pcic_82092_smiirq_ctl(pcic, socket->socket, PCIC_82092_CTL_IRQ,
						PCIC_82092_INT_DISABLE);
		break;
	    default:
		break;
	} /* switch */

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT,
			"\tSCIntMask=%x, interrupt=%x\n", socket->SCIntMask,
			interrupt);
#endif
	/* the SCIntMask specifies events to detect */
	irq = pcic_getb(pcic, socket->socket, PCIC_MANAGEMENT_INT);
	irq &= ~PCIC_CHANGE_MASK;

	/* save the mask we want to use */
	sockp->pcs_intmask = socket->SCIntMask;

	/* now update the hardware to reflect events desired */
	if (sockp->pcs_intmask & SBM_BVD1 || socket->IFType == IF_IO)
		irq |= PCIC_BD_DETECT;

	if (sockp->pcs_intmask & SBM_BVD2)
		irq |= PCIC_BW_DETECT;

	if (sockp->pcs_intmask & SBM_RDYBSY)
		irq |= PCIC_RD_DETECT;

	if (sockp->pcs_intmask & SBM_CD)
		irq |= PCIC_CD_DETECT;

	pcic_putb(pcic, socket->socket, PCIC_MANAGEMENT_INT, irq);

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "\tstatus change set to %x\n", irq);
	}
#endif

	switch (pcic->pc_type) {
	    case PCIC_I82365SL:
	    case PCIC_VADEM:
	    case PCIC_VADEM_VG469:
		/*
		 * The Intel version has different options. This is a
		 * special case of GPI which might be used for eject
		 */

		irq = pcic_getb(pcic, socket->socket, PCIC_CARD_DETECT);
		if (sockp->pcs_intmask & (SBM_EJECT|SBM_INSERT) &&
		    pcic->pc_flags & PCF_GPI_EJECT) {
			irq |= PCIC_GPI_ENABLE;
		} else {
			irq &= ~PCIC_GPI_ENABLE;
		}
		pcic_putb(pcic, socket->socket, PCIC_CARD_DETECT, irq);
		break;
	    case PCIC_CL_PD6710:
	    case PCIC_CL_PD6722:
		if (socket->IFType == IF_IO) {
			/* XXX */
			pcic_putb(pcic, socket->socket, PCIC_MISC_CTL_2, 0x0);
			value = pcic_getb(pcic, socket->socket,
						PCIC_MISC_CTL_1);
			if (pcic->pc_flags & PCF_AUDIO)
				value |= PCIC_MC_SPEAKER_ENB;
			pcic_putb(pcic, socket->socket, PCIC_MISC_CTL_1,
					value);
		} else {
			value = pcic_getb(pcic, socket->socket,
						PCIC_MISC_CTL_1);
			value &= ~PCIC_MC_SPEAKER_ENB;
			pcic_putb(pcic, socket->socket, PCIC_MISC_CTL_1,
					value);
		}
		break;
	    case PCIC_CL_PD6729:
	    case PCIC_CL_PD6730:
		value = pcic_getb(pcic, socket->socket, PCIC_MISC_CTL_1);
		if ((socket->IFType == IF_IO) && (pcic->pc_flags & PCF_AUDIO)) {
		    value |= PCIC_MC_SPEAKER_ENB;
		} else {
		    value &= ~PCIC_MC_SPEAKER_ENB;
		}
		pcic_putb(pcic, socket->socket, PCIC_MISC_CTL_1, value);
		break;
	}

	/*
	 * ctlind processing -- we can ignore this
	 * there aren't any outputs on the chip for this and
	 * the GUI will display what it thinks is correct
	 */

	/* power setup -- if necessary */
	value = pcic_getb(pcic, socket->socket, PCIC_POWER_CONTROL);
	if (powerlevel && powerlevel != value) {
		/*
		 * set power to socket
		 * note that the powerlevel was calculated earlier
		 */
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "\tpowerlevel = %x\n", powerlevel);
#endif

		if (!((pcic_getb(pcic, socket->socket, PCIC_POWER_CONTROL) &
			POWER_OUTPUT_ENABLE)))
			pcic_putb(pcic, socket->socket, PCIC_POWER_CONTROL,
					powerlevel & ~POWER_OUTPUT_ENABLE);
		pcic_putb(pcic, socket->socket, PCIC_POWER_CONTROL, powerlevel);

		/*
		 * this second write to the power control register is needed
		 * to resolve a problem on the IBM ThinkPad 750 where the
		 * first write doesn't latch.  The second write appears to
		 * always work and doesn't hurt the operation of other chips
		 * so we can just use it -- this is good since we can't
		 * determine what chip the 750 actually uses (I suspect an
		 * early Ricoh).
		 */
		pcic_putb(pcic, socket->socket, PCIC_POWER_CONTROL, powerlevel);

		powerlevel = pcic_getb(pcic, socket->socket,
					PCIC_POWER_CONTROL);
#if defined(PCIC_DEBUG)
		if (pcic_debug)
			cmn_err(CE_CONT, "\tpowerlevel reg = %x\n", powerlevel);
#endif
		/*
		 * since power was touched, make sure it says it
		 * is on.  This lets it become stable.
		 */
		pcic_mswait(20);
		for (ind = 0; ind < 1000; ind++) {
			powerlevel = pcic_getb(pcic, socket->socket,
						PCIC_INTERFACE_STATUS);
			if (powerlevel & PCIC_POWER_ON) {
				break;
			}
		}
	}
	if (powerlevel == 0) {
		/* explicitly turned off the power */
		pcic_putb(pcic, socket->socket, PCIC_POWER_CONTROL, 0);
		pcic_putb(pcic, socket->socket, PCIC_POWER_CONTROL, 0);
	}

	/* irq processing */
	if (socket->IFType == IF_IO) {
		inthandler_t *tmp = pcic->irq_top;
		/* IRQ only for I/O */
		irq = socket->IREQRouting & PCIC_INTR_MASK;
		value = pcic_getb(pcic, socket->socket, PCIC_INTERRUPT);
		value &= ~PCIC_INTR_MASK;

		/* to enable I/O operation */
		value |= PCIC_IO_CARD | PCIC_RESET;
		sockp->pcs_flags |= PCS_CARD_IO;
		if (irq != sockp->pcs_irq) {
			if (sockp->pcs_irq != 0)
				cmn_err(CE_CONT,
					"SetSocket: IRQ mismatch %x != %x!\n",
					irq, sockp->pcs_irq);
			else
				sockp->pcs_irq = irq;
		}
		irq = sockp->pcs_irq;

		switch (pcic->pc_intr_mode) {
		case PCIC_INTR_MODE_PCI_1:
			while (tmp) {
			    if (tmp->socket == socket->socket) {
				if (socket->IREQRouting & IRQ_ENABLE)
				    tmp->flags |= PCIRQH_IRQ_ENABLED;
				else
				    tmp->flags &= ~PCIRQH_IRQ_ENABLED;
			    }
			    tmp = tmp->next;
			} /* while (tmp) */

			break;
		default:
			break;
		}

		if (socket->IREQRouting & IRQ_ENABLE) {
			pcic_putb(pcic, socket->socket, PCIC_INTERRUPT,
					value | irq);
			switch (pcic->pc_type) {
			    case PCIC_INTEL_i82092:
				pcic_82092_smiirq_ctl(pcic, socket->socket,
							PCIC_82092_CTL_IRQ,
							PCIC_82092_INT_ENABLE);
				break;
			    default:
				break;
			} /* switch */
			sockp->pcs_flags |= PCS_IRQ_ENABLED;
		} else {
			pcic_putb(pcic, socket->socket, PCIC_INTERRUPT,
					value & ~0xF);
			switch (pcic->pc_type) {
			    case PCIC_INTEL_i82092:
				pcic_82092_smiirq_ctl(pcic, socket->socket,
							PCIC_82092_CTL_IRQ,
							PCIC_82092_INT_DISABLE);
				break;
			    default:
				break;
			} /* switch */
			sockp->pcs_flags &= ~PCS_IRQ_ENABLED;
		}
#if defined(PCIC_DEBUG)
		if (pcic_debug) {
			cmn_err(CE_CONT,
				"\tsocket type is I/O and irq %x is %s\n", irq,
				(socket->IREQRouting & IRQ_ENABLE) ?
				"enabled" : "not enabled");
			xxdmp_all_regs(pcic, socket->socket, 20);
		}
#endif
	} else {
		/* make sure I/O mode is off */

		sockp->pcs_irq = 0;

		value = pcic_getb(pcic, socket->socket, PCIC_INTERRUPT);
		value &= ~(PCIC_INTR_MASK|PCIC_IO_CARD);
		pcic_putb(pcic, socket->socket, PCIC_INTERRUPT,
				value | PCIC_RESET);

		switch (pcic->pc_type) {
		    case PCIC_INTEL_i82092:
			pcic_82092_smiirq_ctl(pcic, socket->socket,
						PCIC_82092_CTL_IRQ,
						PCIC_82092_INT_DISABLE);
			break;
		    default:
			break;
		} /* switch */

		sockp->pcs_flags &= ~(PCS_CARD_IO|PCS_IRQ_ENABLED);
	}

	sockp->pcs_state &= ~socket->State;

	mutex_exit(&pcic->pc_lock);
	return (SUCCESS);
}

/*
 * pcic_inquire_socket()
 *	SocketServices InquireSocket function
 *	returns basic characteristics of the socket
 */
static
pcic_inquire_socket(dev_info_t *dip, inquire_socket_t *socket)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int value;
#ifdef lint
	dip = dip;
#endif

	socket->SCIntCaps = PCIC_DEFAULT_INT_CAPS;
	socket->SCRptCaps = PCIC_DEFAULT_RPT_CAPS;
	socket->CtlIndCaps = PCIC_DEFAULT_CTL_CAPS;
	value = pcic->pc_sockets[socket->socket].pcs_flags;
	socket->SocketCaps = (value & PCS_SOCKET_IO) ? IF_IO : IF_MEMORY;
	socket->ActiveHigh = 0;
	/* these are the usable IRQs */
	socket->ActiveLow = 0xfff0;
	return (SUCCESS);
}

/*
 * pcic_inquire_window()
 *	SocketServices InquireWindow function
 *	returns detailed characteristics of the window
 *	this is where windows get tied to sockets
 */
static
pcic_inquire_window(dev_info_t *dip, inquire_window_t *window)
{
	int type, socket;

#ifdef lint
	dip = dip;
#endif
	type = window->window % PCIC_NUMWINSOCK;
	socket = window->window / PCIC_NUMWINSOCK;

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT,
			"pcic_inquire_window: window = %d/%d socket=%d\n",
			window->window, type, socket);
#endif
	if (type < PCIC_IOWINDOWS) {
		window->WndCaps = WC_IO|WC_WAIT;
		type = IF_IO;
	} else {
		window->WndCaps = WC_COMMON|WC_ATTRIBUTE|WC_WAIT;
		type = IF_MEMORY;
	}

	/* initialize the socket map - one socket per window */
	PR_ZERO(window->Sockets);
	PR_SET(window->Sockets, socket);

	if (type == IF_IO) {
		iowin_char_t *io;
		io = &window->iowin_char;
		io->IOWndCaps = WC_BASE|WC_SIZE|WC_WENABLE|WC_8BIT|
			WC_16BIT;
		io->FirstByte = (baseaddr_t)IOMEM_FIRST;
		io->LastByte = (baseaddr_t)IOMEM_LAST;
		io->MinSize = IOMEM_MIN;
		io->MaxSize = IOMEM_MAX;
		io->ReqGran = IOMEM_GRAN;
		io->AddrLines = IOMEM_DECODE;
		io->EISASlot = 0;
	} else {
		mem_win_char_t *mem;
		mem = &window->mem_win_char;
		mem->MemWndCaps = WC_BASE|WC_SIZE|WC_WENABLE|WC_8BIT|
			WC_16BIT|WC_WP;

		mem->FirstByte = (baseaddr_t)MEM_FIRST;
		mem->LastByte = (baseaddr_t)MEM_LAST;

		mem->MinSize = MEM_MIN;
		mem->MaxSize = MEM_MAX;
		mem->ReqGran = PCIC_PAGE;
		mem->ReqBase = 0;
		mem->ReqOffset = PCIC_PAGE;
		mem->Slowest = MEM_SPEED_MAX;
		mem->Fastest = MEM_SPEED_MIN;
	}
	return (SUCCESS);
}

/*
 * pcic_get_adapter()
 *	SocketServices GetAdapter function
 *	this is nearly a no-op.
 */
static
pcic_get_adapter(dev_info_t *dip, get_adapter_t *adapt)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;

#ifdef lint
	dip = dip;
#endif
	if (pcic->pc_flags & PCF_INTRENAB)
		adapt->SCRouting = IRQ_ENABLE;
	adapt->state = 0;
	return (SUCCESS);
}

/*
 * pcic_get_page()
 *	SocketServices GetPage function
 *	returns info about the window
 */
static
pcic_get_page(dev_info_t *dip, get_page_t *page)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int socket, window;
	pcs_memwin_t *winp;

#ifdef lint
	dip = dip;
#endif
	socket = page->window / PCIC_NUMWINSOCK;
	window = page->window % PCIC_NUMWINSOCK;

	/* I/O windows are the first two */
	if (window < PCIC_IOWINDOWS || socket >= pcic->pc_numsockets) {
		return (BAD_WINDOW);
	}

	winp = &pcic->pc_sockets[socket].pcs_windows[window].mem;

	if (page->page != 0)
		return (BAD_PAGE);

	page->state = 0;
	if (winp->pcw_status & PCW_ENABLED)
		page->state |= PS_ENABLED;
	if (winp->pcw_status & PCW_ATTRIBUTE)
		page->state |= PS_ATTRIBUTE;
	if (winp->pcw_status & PCW_WP)
		page->state |= PS_WP;

	page->offset = (off_t)winp->pcw_offset;

	return (SUCCESS);
}

/*
 * pcic_get_socket()
 *	SocketServices GetSocket
 *	returns information about the current socket setting
 */
static
pcic_get_socket(dev_info_t *dip, get_socket_t *socket)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int socknum, irq_enabled;
	pcic_socket_t *sockp;
#ifdef lint
	dip = dip;
#endif

	socknum = socket->socket;
	sockp = &pcic->pc_sockets[socknum];

	socket->SCIntMask = sockp->pcs_intmask;
	sockp->pcs_state = pcic_card_state(pcic, sockp);

	socket->state = sockp->pcs_state;
	if (sockp->pcs_flags & PCS_CARD_PRESENT)
		socket->state |= SBM_CD;

	socket->VccLevel = sockp->pcs_vcc;
	socket->Vpp1Level = sockp->pcs_vpp1;
	socket->Vpp2Level = sockp->pcs_vpp2;
	socket->CtlInd = 0;	/* no indicators */
	irq_enabled = (sockp->pcs_flags & PCS_IRQ_ENABLED) ? IRQ_ENABLE : 0;
	socket->IRQRouting = sockp->pcs_irq | irq_enabled;
	socket->IFType = (sockp->pcs_flags & PCS_CARD_IO) ? IF_IO : IF_MEMORY;

	return (SUCCESS);
}

/*
 * pcic_get_status()
 *	SocketServices GetStatus
 *	returns status information about the PC Card in
 *	the selected socket
 */
static
pcic_get_status(dev_info_t *dip, get_ss_status_t *status)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int socknum, irq_enabled;
	pcic_socket_t *sockp;
#ifdef lint
	dip = dip;
#endif

	socknum = status->socket;
	sockp = &pcic->pc_sockets[socknum];

	status->CardState = pcic_card_state(pcic, sockp);

	status->SocketState = sockp->pcs_state;
	if (sockp->pcs_flags & PCS_CARD_PRESENT)
		status->SocketState |= SBM_CD;
	status->CtlInd = 0;	/* no indicators */
	irq_enabled = (sockp->pcs_flags & PCS_CARD_ENABLED) ? IRQ_ENABLE : 0;
	status->IRQRouting = sockp->pcs_irq | irq_enabled;
	status->IFType = (sockp->pcs_flags & PCS_CARD_IO) ? IF_IO : IF_MEMORY;

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "pcic_get_status: socket=%d, CardState=%x,"
			"SocketState=%x\n",
			socknum, status->CardState, status->SocketState);
#endif
	return (SUCCESS);
}

/*
 * pcic_get_window()
 *	SocketServices GetWindow function
 *	returns state information about the specified window
 */
static
pcic_get_window(dev_info_t *dip, get_window_t *window)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int socket, win;
	pcic_socket_t *sockp;
	pcs_memwin_t *winp;
#ifdef lint
	dip = dip;
#endif

	socket = window->window / PCIC_NUMWINSOCK;
	win = window->window % PCIC_NUMWINSOCK;
#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_get_window(socket=%d, window=%d)\n",
			socket, win);
	}
#endif

	if (socket > pcic->pc_numsockets)
		return (BAD_WINDOW);

	sockp = &pcic->pc_sockets[socket];
	winp = &sockp->pcs_windows[win].mem;

	window->socket = socket;
	window->size = winp->pcw_len;
	window->speed = winp->pcw_speed;
	window->handle = (baseaddr_t)winp->pcw_handle;
	window->base = (uint32_t)winp->pcw_base;

	if (win >= PCIC_IOWINDOWS) {
		window->state = 0;
	} else {
		window->state = WS_IO;
	}
	if (winp->pcw_status & PCW_ENABLED)
		window->state |= WS_ENABLED;

	if (winp->pcw_status & PCS_CARD_16BIT)
		window->state |= WS_16BIT;
#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "\tsize=%d, speed=%d, base=%x, state=%x\n",
			window->size, (unsigned)window->speed,
			(unsigned)window->handle,
			window->state);
#endif

	return (SUCCESS);
}

/*
 * pcic_ll_reset
 *	low level reset
 *	separated out so it can be called when already locked
 *
 *	There are two variables that control the RESET timing:
 *		pcic_prereset_time - time in mS before asserting RESET
 *		pcic_reset_time - time in mS to assert RESET
 *
 * XXX - need to rethink RESET timing delays to avoid using drv_usecwait
 */
int pcic_prereset_time = 1;
int pcic_reset_time = 5;

static int
pcic_ll_reset(pcicdev_t *pcic, int socket)
{
	int windowbits, iobits;

	/* save windows that were on */
	windowbits = pcic_getb(pcic, socket, PCIC_MAPPING_ENABLE);
	/* turn all windows off */
	pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE, 0);

	if (pcic_prereset_time > 0)
		pcic_mswait(pcic_prereset_time);

	/* turn interrupts off and start a reset */
	iobits = pcic_getb(pcic, socket, PCIC_INTERRUPT);
	iobits &= ~(PCIC_INTR_MASK | PCIC_RESET);
	pcic_putb(pcic, socket, PCIC_INTERRUPT, iobits);

	switch (pcic->pc_type) {
	    case PCIC_INTEL_i82092:
		pcic_82092_smiirq_ctl(pcic, socket, PCIC_82092_CTL_IRQ,
						PCIC_82092_INT_DISABLE);
		break;
	    default:
		break;
	} /* switch */

	pcic->pc_sockets[socket].pcs_state = 0;

	if (pcic_reset_time > 0)
		pcic_mswait(pcic_reset_time);

	/* take it out of RESET now */
	pcic_putb(pcic, socket, PCIC_INTERRUPT, PCIC_RESET | iobits);

	/*
	 * can't access the card for 20ms, but we really don't
	 * want to sit around that long. The pcic is still usable.
	 * memory accesses must wait for RDY to come up.
	 */
	pcic_mswait(2);
	return (windowbits);
}

/*
 * pcic_reset_socket()
 *	SocketServices ResetSocket function
 *	puts the PC Card in the socket into the RESET state
 *	and then takes it out after the the cycle time
 *	The socket is back to initial state when done
 */
static
pcic_reset_socket(dev_info_t *dip, int socket, int mode)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	register int value;
	int i;
	pcic_socket_t *sockp;

#if defined(PCIC_DEBUG)
	if (pcic_debug)
		cmn_err(CE_CONT, "pcic_reset_socket(%x, %d, %d/%s)\n",
			(unsigned)dip, socket, mode,
			mode == RESET_MODE_FULL ? "full" : "partial");
#endif

	mutex_enter(&pcic->pc_lock); /* protect the registers */

	/* XXX CHECK ALL OF THIS Re: WINDOW ENABLE/DISABLE!!! XXX */

	sockp = &pcic->pc_sockets[socket];

	value = pcic_ll_reset(pcic, socket);
	if (mode == RESET_MODE_FULL) {
		/* disable and unmap all mapped windows */
		for (i = 0; i < PCIC_NUMWINSOCK; i++) {
			if (i < PCIC_IOWINDOWS) {
				if (sockp->pcs_windows[i].io.pcw_status &
				    PCW_MAPPED) {
					pcs_iowin_t *io;
					io = &sockp->pcs_windows[i].io;
					io->pcw_status &= ~PCW_ENABLED;
				}
			} else {
				if (sockp->pcs_windows[i].mem.pcw_status &
				    PCW_MAPPED) {
					pcs_memwin_t *mem;
					mem = &sockp->pcs_windows[i].mem;
					mem->pcw_status &= ~PCW_ENABLED;
				}
			}
		}
	} else {
				/* turn windows back on */
		pcic_putb(pcic, socket, PCIC_MAPPING_ENABLE, value);
		/* wait the rest of the time here */
		pcic_mswait(10);
	}
	mutex_exit(&pcic->pc_lock);
	return (SUCCESS);
}

/*
 * pcic_set_interrupt()
 *	SocketServices SetInterrupt function
 */
static
pcic_set_interrupt(dev_info_t *dip, set_irq_handler_t *handler)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	int value = DDI_SUCCESS;
	inthandler_t *intr;

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_set_interrupt: entered pc_intr_mode=0X%x\n",
			pcic->pc_intr_mode);
		cmn_err(CE_CONT,
			"\t irq_top=%p handler=%p handler_id=%x\n",
			(void *)pcic->irq_top, (void *)handler->handler,
			handler->handler_id);
	}
#endif

	/*
	 * If we're on a PCI bus, we route all IO IRQs through a single
	 *	PCI interrupt (typically INT A#) so we don't have to do
	 *	much other than add the caller to general interrupt handler
	 *	and set some state.
	 */

	intr = (inthandler_t *)kmem_zalloc(sizeof (inthandler_t), KM_NOSLEEP);
	if (intr == NULL) {
		return (NO_RESOURCE);
	}


	mutex_enter(&pcic->pc_lock);

	switch (pcic->pc_intr_mode) {
	case PCIC_INTR_MODE_PCI_1:
		/*
		 * We only allow above-lock-level IO IRQ handlers
		 *	in the PCI bus case.
		 */

		if (pcic->irq_top == NULL) {
		    pcic->irq_top = intr;
		    pcic->irq_current = pcic->irq_top;
		} else {
		    while (pcic->irq_current->next != NULL)
			pcic->irq_current = pcic->irq_current->next;
		    pcic->irq_current->next = intr;
		    pcic->irq_current = pcic->irq_current->next;
		}

		pcic->irq_current->intr =
		    (uint32_t(*)(caddr_t))handler->handler;
		pcic->irq_current->handler_id = handler->handler_id;
		pcic->irq_current->arg = handler->arg;
		pcic->irq_current->socket = handler->socket;

		handler->iblk_cookie = &pcic->pc_icookie;
		handler->idev_cookie = &pcic->pc_dcookie;
		break;

	default:
		intr->intr = (uint32_t(*)(caddr_t))handler->handler;
		intr->handler_id = handler->handler_id;
		intr->arg = handler->arg;
		intr->socket = handler->socket;
		intr->irq = handler->irq;

		/*
		 * need to revisit this to see if interrupts can be
		 * shared someday. Note that IRQ is set in the common
		 * code.
		 */

		if (pcic->pc_handlers == NULL) {
			pcic->pc_handlers = intr;
			intr->next = intr->prev = intr;
		} else {
			insque(intr, pcic->pc_handlers);
		}

#if defined(PCIC_DEBUG)
		if (pcic_debug) {
			cmn_err(CE_CONT,
				"pcic: ddi_add_intr(%x, %x, %x, %x, %x, %x)\n",
				(unsigned)dip,
				pcic_irq_map[intr->irq &
					    PCIC_INTR_MASK].irq,
				(unsigned)&intr->iblk_cookie,
				(unsigned)&intr->idev_cookie,
				(unsigned)intr->intr, (unsigned)intr->irq);
			cmn_err(CE_CONT, "\tIRQ=%x\n", intr->irq);
		}
#endif
		break;
	}
	mutex_exit(&pcic->pc_lock);

	/*
	 * need to fill in cookies in event of multiple high priority
	 * interrupt handlers on same IRQ
	 * XXX - What does this mean?
	 */

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_set_interrupt: exit irq_top=%p value=%d\n",
			(void *)pcic->irq_top, value);
	}
#endif

	if (value == DDI_SUCCESS) {
		return (SUCCESS);
	} else {
		return (BAD_IRQ);
	}
}

/*
 * pcic_clear_interrupt()
 *	SocketServices ClearInterrupt function
 *
 *	Interrupts for PCIC are complicated by the fact that we must
 *	follow several different models for interrupts.
 *	ISA: there is an interrupt per adapter and per socket and
 *		they can't be shared.
 *	PCI: some adapters have one PCI interrupt available while others
 *		have up to 4.  Solaris may or may not allow us to use more
 *		than 1 so we essentially share them all at this point.
 *	Hybrid: PCI bridge but interrupts wired to host interrupt controller.
 *		This is like ISA but we have to fudge and create an intrspec
 *		that PCI's parent understands and bypass the PCI nexus.
 *	multifunction: this requires sharing the interrupts on a per-socket
 *		basis.
 */
static
pcic_clear_interrupt(dev_info_t *dip, clear_irq_handler_t *handler)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private(dip);
	pcicdev_t *pcic = anp->an_private;
	inthandler_t *intr, *prev;
	int i;

	/*
	 * If we're on a PCI bus, we route all IO IRQs through a single
	 *	PCI interrupt (typically INT A#) so we don't have to do
	 *	much other than remove the caller from the general
	 *	interrupt handler callout list.
	 */

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
			"pcic_clear_interrupt: entered pc_intr_mode=0X%x\n",
			pcic->pc_intr_mode);
		cmn_err(CE_CONT,
			"\t irq_top=%p handler=%p handler_id=%x\n",
			(void *)pcic->irq_top, (void *)handler->handler,
			handler->handler_id);
	}
#endif

	mutex_enter(&pcic->pc_lock);
	switch (pcic->pc_intr_mode) {
	case PCIC_INTR_MODE_PCI_1:
		if (pcic->irq_top == NULL) {
			mutex_exit(&pcic->pc_lock);
			return (BAD_IRQ);
		}

		intr = NULL;
		pcic->irq_current = pcic->irq_top;

		while ((pcic->irq_current != NULL) &&
				(pcic->irq_current->handler_id !=
						handler->handler_id)) {
			intr = pcic->irq_current;
			pcic->irq_current = pcic->irq_current->next;
		}

		if (pcic->irq_current == NULL) {
			mutex_exit(&pcic->pc_lock);
			return (BAD_IRQ);
		}

		if (intr != NULL) {
			intr->next = pcic->irq_current->next;
		} else {
			pcic->irq_top = pcic->irq_current->next;
		}

		kmem_free((caddr_t)pcic->irq_current, sizeof (inthandler_t));
		pcic->irq_current = pcic->irq_top;

		break;

	default:

		intr = pcic_handlers;
		prev = (inthandler_t *)&pcic_handlers;

		while (intr != NULL) {
		    if (intr->handler_id == handler->handler_id) {
			i = intr->irq & PCIC_INTR_MASK;
			if (--pcic_irq_map[i].count == 0) {
				/* multi-handler form */
				ddi_remove_intr(dip,
						0,
						intr->iblk_cookie);
				(void) pcmcia_return_intr(pcic->dip, i);
#if defined(PCIC_DEBUG)
				if (pcic_debug) {
					cmn_err(CE_CONT,
						"removing interrupt %d at %s "
						"priority\n",
						i,
						"high");
					cmn_err(CE_CONT,
						"ddi_remove_intr(%x, %x, %x)\n",
						(unsigned)dip,
						0,
						(unsigned)intr->iblk_cookie);
				}
#endif
			}
			prev->next = intr->next;
			kmem_free((caddr_t)intr, sizeof (inthandler_t));
			intr = prev->next;
		    } else {
			prev = intr;
			intr = intr->next;
		    } /* if (handler_id) */
		} /* while */
	}
	mutex_exit(&pcic->pc_lock);

#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT,
		"pcic_clear_interrupt: exit irq_top=%p\n",
		(void *)pcic->irq_top);
	}
#endif


	return (SUCCESS);
}

struct intel_regs {
	char *name;
	int   off;
	char *fmt;
} iregs[] = {
	{"ident     ", 0},
	{"if-status ", 1, "\020\1BVD1\2BVD2\3CD1\4CD2\5WP\6RDY\7PWR\10~GPI"},
	{"power     ", 2, "\020\1Vpp1c0\2Vpp1c1\3Vpp2c0\4Vpp2c1\5PE\6AUTO"
		"\7DRD\10OE"},
	{"cardstatus", 4, "\020\1BD\2BW\3RC\4CD\5GPI\6R1\7R2\010R3"},
	{"enable    ", 6, "\020\1MW0\2MW1\3MW2\4MW3\5MW4\6MEM16\7IO0\10IO1"},
	{"cd-gcr    ", 0x16, "\020\1MDI16\2CRE\3GPIE\4GPIT\5CDR\6S/W"},
	{"GCR       ", 0x1e, "\020\1PD\2LEVEL\3WCSC\4PLS14"},
	{"int-gcr   ", 3, "\020\5INTR\6IO\7~RST\10RI"},
	{"management", 5, "\020\1BDE\2BWE\3RE\4CDE"},
	{"volt-sense", 0x1f, "\020\1A_VS1\2A_VS2\3B_VS1\4B_VS2"},
	{"volt-sel  ", 0x2f, "\020\5EXTCONF\6BUSSELECT\7MIXEDV\10ISAV"},
	{"VG ext A  ", 0x3c, "\20\3IVS\4CABLE\5CSTEP\6TEST\7RIO"},
	{"io-ctrl   ", 7, "\020\1DS0\2IOCS0\3ZWS0\4WS0\5DS1\6IOS1\7ZWS1\10WS1"},
	{"io0-slow  ", 8},
	{"io0-shi   ", 9},
	{"io0-elow  ", 0xa},
	{"io0-ehi   ", 0xb},
	{"io1-slow  ", 0xc},
	{"io1-shi   ", 0xd},
	{"io1-elow  ", 0xe},
	{"io1-ehi   ", 0xf},
	{"mem0-slow ", 0x10},
	{"mem0-shi  ", 0x11, "\020\7ZW\10DS"},
	{"mem0-elow ", 0x12},
	{"mem0-ehi  ", 0x13, "\020\7WS0\10WS1"},
	{"card0-low ", 0x14},
	{"card0-hi  ", 0x15, "\020\7AM\10WP"},
	{"mem1-slow ", 0x18},
	{"mem1-shi  ", 0x19, "\020\7ZW\10DS"},
	{"mem1-elow ", 0x1a},
	{"mem1-ehi  ", 0x1b, "\020\7WS0\10WS1"},
	{"card1-low ", 0x1c},
	{"card1-hi  ", 0x1d, "\020\7AM\10WP"},
	{"mem2-slow ", 0x20},
	{"mem2-shi  ", 0x21, "\020\7ZW\10DS"},
	{"mem2-elow ", 0x22},
	{"mem2-ehi  ", 0x23, "\020\7WS0\10WS1"},
	{"card2-low ", 0x24},
	{"card2-hi  ", 0x25, "\020\7AM\10WP"},
	{"mem3-slow ", 0x28},
	{"mem3-shi  ", 0x29, "\020\7ZW\10DS"},
	{"mem3-elow ", 0x2a},
	{"mem3-ehi  ", 0x2b, "\020\7WS0\10WS1"},
	{"card3-low ", 0x2c},
	{"card3-hi  ", 0x2d, "\020\7AM\10WP"},

	{"mem4-slow ", 0x30},
	{"mem4-shi  ", 0x31, "\020\7ZW\10DS"},
	{"mem4-elow ", 0x32},
	{"mem4-ehi  ", 0x33, "\020\7WS0\10WS1"},
	{"card4-low ", 0x34},
	{"card4-hi  ", 0x35, "\020\7AM\10WP"},
	{NULL},
};

static struct intel_regs cregs[] = {
	{"misc-ctl1 ", 0x16, "\20\2VCC3\3PMI\4PSI\5SPKR\10INPACK"},
	{"fifo      ", 0x17, "\20\10EMPTY"},
	{"misc-ctl2 ", 0x1e, "\20\1XCLK\2LOW\3SUSP\4CORE5V\5TCD\10RIOUT"},
	{"chip-info ", 0x1f, "\20\6DUAL"},
	{"IO-offlow0", 0x36},
	{"IO-offhi0 ", 0x37},
	{"IO-offlow1", 0x38},
	{"IO-offhi1 ", 0x39},
	NULL,
};

static struct intel_regs cxregs[] = {
	{"ext-ctl-1 ", 0x03,
		"\20\1VCCLCK\2AUTOCLR\3LED\4INVIRQC\5INVIRQM\6PUC"},
	{"mem0-up   ", 0x05},
	{"mem1-up   ", 0x06},
	{"mem2-up   ", 0x07},
	{"mem3-up   ", 0x08},
	{"mem4-up   ", 0x09},
	{NULL}
};
#ifdef	XXX
void
xxoutb(int reg, int value)
{
	int i;
	if (reg & 1) {
		cmn_err(CE_CONT, "outb(%x, %x)\n", reg, value);
	} else {
		cmn_err(CE_CONT, "outb(%x,", reg);
		for (i = 0; iregs[i].name != NULL; i++)
			if (iregs[i].off == (value&0x1F)) {
				cmn_err(CE_CONT, "%s", iregs[i].name);
				break;
			}
		cmn_err(CE_CONT, "<%x>)\n", value);
	}
	outb(reg, value);
}
#endif	XXX

void
xxdmp_cl_regs(pcicdev_t *pcic, int socket, uint32_t len)
{
	int i, value, j;
	char buff[256];
	char *fmt;

	cmn_err(CE_CONT, "--------- Cirrus Logic Registers --------\n");
	for (buff[0] = '\0', i = 0; cregs[i].name != NULL && len-- != 0; i++) {
		int sval;
		if (cregs[i].off == PCIC_MISC_CTL_2)
			sval = 0;
		else
			sval = socket;
		value = pcic_getb(pcic, sval, cregs[i].off);
		if (i & 1) {
			if (cregs[i].fmt)
				fmt = "%s\t%s\t%b\n";
			else
				fmt = "%s\t%s\t%x\n";
			cmn_err(CE_CONT, fmt, buff,
				cregs[i].name, value, cregs[i].fmt);
			buff[0] = '\0';
		} else {
			if (cregs[i].fmt)
				fmt = "\t%s\t%b";
			else
				fmt = "\t%s\t%x";
			(void) sprintf(buff, fmt,
				cregs[i].name, value, cregs[i].fmt);
			for (j = strlen(buff); j < 40; j++)
				buff[j] = ' ';
			buff[40] = '\0';
		}
	}
	cmn_err(CE_CONT, "%s\n", buff);

	i = pcic_getb(pcic, socket, PCIC_TIME_SETUP_0);
	j = pcic_getb(pcic, socket, PCIC_TIME_SETUP_1);
	cmn_err(CE_CONT, "\tsetup-tim0\t%x\tsetup-tim1\t%x\n", i, j);

	i = pcic_getb(pcic, socket, PCIC_TIME_COMMAND_0);
	j = pcic_getb(pcic, socket, PCIC_TIME_COMMAND_1);
	cmn_err(CE_CONT, "\tcmd-tim0  \t%x\tcmd-tim1  \t%x\n", i, j);

	i = pcic_getb(pcic, socket, PCIC_TIME_RECOVER_0);
	j = pcic_getb(pcic, socket, PCIC_TIME_RECOVER_1);
	cmn_err(CE_CONT, "\trcvr-tim0 \t%x\trcvr-tim1 \t%x\n", i, j);

	cmn_err(CE_CONT, "--------- Extended Registers  --------\n");

	for (buff[0] = '\0', i = 0; cxregs[i].name != NULL && len-- != 0; i++) {
		value = clext_reg_read(pcic, socket, cxregs[i].off);
		if (i & 1) {
			if (cxregs[i].fmt)
				fmt = "%s\t%s\t%b\n";
			else
				fmt = "%s\t%s\t%x\n";
			cmn_err(CE_CONT, fmt, buff,
				cxregs[i].name, value, cxregs[i].fmt);
			buff[0] = '\0';
		} else {
			if (cxregs[i].fmt)
				fmt = "\t%s\t%b";
			else
				fmt = "\t%s\t%x";
			(void) sprintf(buff, fmt,
				cxregs[i].name, value, cxregs[i].fmt);
			for (j = strlen(buff); j < 40; j++)
				buff[j] = ' ';
			buff[40] = '\0';
		}
	}
}

#if defined(PCIC_DEBUG)
static void
xxdmp_all_regs(pcicdev_t *pcic, int socket, uint32_t len)
{
	int i, value, j;
	char buff[256];
	char *fmt;

#if defined(PCIC_DEBUG)
	if (pcic_debug < 2)
		return;
#endif
	cmn_err(CE_CONT,
		"----------- PCIC Registers for socket %d---------\n",
		socket);
	cmn_err(CE_CONT,
		"\tname       value                        name       value\n");

	for (buff[0] = '\0', i = 0; iregs[i].name != NULL && len-- != 0; i++) {
		value = pcic_getb(pcic, socket, iregs[i].off);
		if (i & 1) {
			if (iregs[i].fmt)
				fmt = "%s\t%s\t%b\n";
			else
				fmt = "%s\t%s\t%x\n";
			cmn_err(CE_CONT, fmt, buff,
				iregs[i].name, value, iregs[i].fmt);
			buff[0] = '\0';
		} else {
			if (iregs[i].fmt)
				fmt = "\t%s\t%b";
			else
				fmt = "\t%s\t%x";
			(void) sprintf(buff, fmt,
				iregs[i].name, value, iregs[i].fmt);
			for (j = strlen(buff); j < 40; j++)
				buff[j] = ' ';
			buff[40] = '\0';
		}
	}
	switch (pcic->pc_type) {
	case PCIC_CL_PD6710:
	case PCIC_CL_PD6722:
	case PCIC_CL_PD6729:
		(void) xxdmp_cl_regs(pcic, socket, 0xFFFF);
		break;
	}
	cmn_err(CE_CONT, "%s\n", buff);
}
#endif

#if	defined(sparc) || defined(__sparc)
/*
 * pcmcia_SPARC_specific
 *	we need to find out what resources are available for
 *	PC Card drivers.  For now, they are provided via
 *	several special tuples in the "reg" property.
 *
 *	Note that this goes away in the future and we use
 *	"available" property on the parent.
 */
static int
pcic_system(dev_info_t *dip)
{
	int nregs, proplen;
	pci_regspec_t *regspec;

	if (ddi_dev_nregs(dip, &nregs) != DDI_SUCCESS) {
	    cmn_err(CE_CONT, "pcic%d: no register tuples found\n",
						ddi_get_instance(dip));
	    return (DDI_FAILURE);
	}

	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS, "reg",
				(caddr_t)&regspec, &proplen) != DDI_SUCCESS) {
	    cmn_err(CE_CONT, "pcic%d: unable to get reg property value\n",
						ddi_get_instance(dip));

	    return (DDI_FAILURE);
	}

	/*
	 * Check to be sure that we have at least two register tuples
	 *	defined:
	 *		reg num 0: config space
	 *		reg num 1: PCIC IO control registers
	 */
	if (nregs < 2) {
	    cmn_err(CE_CONT, "pcic%d: invalid number of register tuples\n",
							ddi_get_instance(dip));
	    kmem_free((caddr_t)regspec, proplen);
	    return (DDI_FAILURE);
	}

	/*
	 * Get Memory space resources
	 */

	kmem_free((caddr_t)regspec, proplen);
	return (DDI_SUCCESS);
}

#else
/*
 * PCMCIA x86 specific
 *	we need to find out what resources are available for
 *	PC Card drivers.  For now, they are provided via
 *	several properties.
 */
static int
pcic_system(dev_info_t *dip)
{
#ifdef lint
	dip = dip;
#endif
	return (DDI_SUCCESS);
}
#endif

/*
 * pcic_mswait(ms)
 *	sleep ms milliseconds
 *	call drv_usecwait once for each ms
 */
static void
pcic_mswait(int ms)
{
	while (ms-- > 0)
		drv_usecwait(1000);
}
/*
 * pcic_ready_wait(pcic, index, off)
 *	Wait for card to come ready
 *	We only wait if the card is NOT in RESET
 *	and power is on.
 */
static void
pcic_ready_wait(pcicdev_t *pcic, int socket)
{
	int ifstate, intstate, limit;

	intstate = pcic_getb(pcic, socket, PCIC_INTERRUPT);

	if (intstate & PCIC_RESET) {
				/* not in reset here */
		for (limit = PCIC_READY_WAIT_LOOPS; limit > 0; limit--) {
			ifstate = pcic_getb(pcic, socket,
						PCIC_INTERFACE_STATUS);
			if (((ifstate & (PCIC_READY|PCIC_POWER_ON)) ==
			    (PCIC_READY|PCIC_POWER_ON)) ||
			    (ifstate & PCIC_ISTAT_CD_MASK) !=
			    PCIC_CD_PRESENT_OK)
				return;
			pcic_mswait(PCIC_READY_WAIT_TIME);
		}
	}

#ifdef	PCIC_DEBUG
	cmn_err(CE_CONT, "pcic_ready_wait: Card in RESET, intstate = 0x%x, "
							"ifstate = 0x%x\n",
							intstate, ifstate);
	if (pcic_debug) {
		pcic_debug += 4;
		xxdmp_all_regs(pcic, socket, -1);
		pcic_debug -= 4;
	}
#endif
}

/*
 * timeout wrappers to sequence events appropriately for fake
 * power management.
 */
void
pcic_do_suspend(void *dip)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private((dev_info_t *)dip);
	pcicdev_t *pcic = anp->an_private;

	(void) pcic_detach((dev_info_t *)dip, DDI_SUSPEND);
	cv_broadcast(&pcic->pm_cv);
}
void
pcic_do_resume(void *dip)
{
	anp_t *anp = (anp_t *)ddi_get_driver_private((dev_info_t *)dip);
	pcicdev_t *pcic = anp->an_private;

	(void) pcic_attach((dev_info_t *)dip, DDI_RESUME);
	cv_broadcast(&pcic->pm_cv);
}

/*
 * pcic_pm_detection(pcicdev_t *)
 *	this function is called by a recurring timeout.
 *	Its purpose is to detect the case where a notebook system
 *	has been suspended and then resumed.  Since there is no
 *	power management framework to do this, we fake it here.
 *	The reason is that on suspend, the PCIC chip loses power
 *	and all programming
 */
static void
pcic_pm_detection(void *arg)
{
	pcicdev_t *pcic = arg;
	pcic_pm_t *pmt = &pcic->pmt;
	int value;
	int suspended = 0;	/* assume not */
	time_t ttime;

	if (drv_getparm(TIME, &ttime) == -1) {
	    cmn_err(CE_CONT, "pcic_pm_detection: error getting TIME\n");
	    return;
	}

	mutex_enter(&pcic->pc_lock);

	if (pmt->state == PCIC_PM_RUN) {
	    if (pcic_pm_methods & PCIC_PM_METHOD_TIME) {
		if ((ttime - pmt->ptime) > pcic_pm_detwin) {
		    suspended |= 1;
		}
	    } /* PCIC_PM_METHOD_TIME */

	    if (pcic_pm_methods & PCIC_PM_METHOD_REG) {
		value = pcic_getb(pcic, 0, PCIC_MANAGEMENT_INT);

		/*
		 * easiest thing to check is the management interrupt
		 * since we should always have one and powerup sets to
		 * zero.
		 */
		if ((value & 0xF0) == 0)
		    suspended |= 2;
	    } /* PCIC_PM_METHOD_REG */
	} /* PCIC_PM_RUN */

	pmt->ptime = ttime;
	pmt->state = PCIC_PM_RUN;

	if (suspended) {
#if defined(PCIC_DEBUG)
	    if (pcic_debug)
		cmn_err(CE_CONT, "?pcic: doing suspend/resume "
					"notifications %s %s : ",
					(suspended & 1)?"TIMEOUT":"",
					(suspended & 2)?"REGISTER":"");
#endif

		/* first fake a suspend event */
		(void) timeout(pcic_do_suspend, pmt->dip,
		    drv_usectohz(50 * 1000));

		/* wait for it to finish */
		cv_wait(&pcic->pm_cv, &pcic->pc_lock);

#if defined(PCIC_DEBUG)
		if (pcic_debug)
		    cmn_err(CE_CONT, "?DDI_SUSPEND ");
#endif

		/* then fake an resume event quarter second later */
		(void) timeout(pcic_do_resume, pmt->dip,
		    drv_usectohz(500 * 1000));

		/* wait for it to finish */
		cv_wait(&pcic->pm_cv, &pcic->pc_lock);

#if defined(PCIC_DEBUG)
		if (pcic_debug)
		    cmn_err(CE_CONT, "?DDI_RESUME\n");
#endif

		pmt->state = PCIC_PM_INIT;

	} /* suspended */

	pcic->pc_pmtimer = timeout(pcic_pm_detection, pcic,
	    drv_usectohz(pcic_pm_time * 1000 * 1000));

	mutex_exit(&pcic->pc_lock);
}

/*
 * Cirrus Logic extended register read/write routines
 */
static int
clext_reg_read(pcicdev_t *pcic, int sn, uchar_t ext_reg)
{
	int val;

	pcic_putb(pcic, sn, PCIC_CL_EXINDEX, ext_reg);
	val = pcic_getb(pcic, sn, PCIC_CL_EXINDEX + 1);

	return (val);
}

static void
clext_reg_write(pcicdev_t *pcic, int sn, uchar_t ext_reg, uchar_t value)
{
	pcic_putb(pcic, sn, PCIC_CL_EXINDEX, ext_reg);
	pcic_putb(pcic, sn, PCIC_CL_EXINDEX + 1, value);
}

/*
 * Misc PCI functions
 */
static void
pcic_iomem_pci_ctl(ddi_acc_handle_t handle, uchar_t *cfgaddr, unsigned flags)
{
	unsigned cmd;

	if (flags & (PCIC_ENABLE_IO | PCIC_ENABLE_MEM)) {
		cmd = ddi_get16(handle, (ushort_t *)(cfgaddr + 4));
		if (flags & PCIC_ENABLE_IO)
		    cmd |= PCI_COMM_IO;

		if (flags & PCIC_ENABLE_MEM)
		    cmd |= PCI_COMM_MAE;

		ddi_put16(handle, (ushort_t *)(cfgaddr + 4), cmd);
	} /* if (PCIC_ENABLE_IO | PCIC_ENABLE_MEM) */
}

/*
 * pcic_find_pci_type - Find and return PCI-PCMCIA adapter type
 */
pcic_find_pci_type(pcicdev_t *pcic)
{
	uint32_t vend, device;

	vend = ddi_getprop(DDI_DEV_T_ANY, pcic->dip,
				DDI_PROP_CANSLEEP|DDI_PROP_DONTPASS,
				"vendor-id", -1);
	device = ddi_getprop(DDI_DEV_T_ANY, pcic->dip,
				DDI_PROP_CANSLEEP|DDI_PROP_DONTPASS,
				"device-id", -1);

	device = PCI_ID(vend, device);
	pcic->pc_type = device;
	pcic->pc_chipname = "PCI:unknown";

	switch (device) {
	case PCIC_INTEL_i82092:
		pcic->pc_chipname = PCIC_TYPE_i82092;
		break;
	case PCIC_CL_PD6729:
		pcic->pc_chipname = PCIC_TYPE_PD6729;
		/*
		 * Some 6730's incorrectly identify themselves
		 *	as a 6729, so we need to do some more tests
		 *	here to see if the device that's claiming
		 *	to be a 6729 is really a 6730.
		 */
		if ((clext_reg_read(pcic, 0, PCIC_CLEXT_MISC_CTL_3) &
			PCIC_CLEXT_MISC_CTL_3_REV_MASK) ==
				0) {
			pcic->pc_chipname = PCIC_TYPE_PD6730;
			pcic->pc_type = PCIC_CL_PD6730;
		}
		break;
	case PCIC_CL_PD6730:
		pcic->pc_chipname = PCIC_TYPE_PD6730;
		break;
	case PCIC_SMC_34C90:
		pcic->pc_chipname = PCIC_TYPE_34C90;
		break;
	case PCIC_TOSHIBA_TOPIC95:
		pcic->pc_chipname = PCIC_TYPE_TOPIC95;
		break;
	case PCIC_TI_PCI1031:
		pcic->pc_chipname = PCIC_TYPE_PCI1031;
		break;
	case PCIC_TI_PCI1130:
		pcic->pc_chipname = PCIC_TYPE_PCI1130;
		break;
	case PCIC_TI_PCI1131:
		pcic->pc_chipname = PCIC_TYPE_PCI1131;
		break;
	case PCIC_TI_PCI1250:
		pcic->pc_chipname = PCIC_TYPE_PCI1250;
		break;
	case PCIC_TI_PCI1221:
		pcic->pc_chipname = PCIC_TYPE_PCI1221;
		break;
	case PCIC_TI_PCI1050:
		pcic->pc_chipname = PCIC_TYPE_PCI1050;
		break;
	case PCIC_RICOH_RL5C466:
		pcic->pc_chipname = PCIC_TYPE_RL5C466;
		break;
	default:
		if (!(pcic->pc_flags & PCF_CARDBUS))
			return (DDI_FAILURE);
		pcic->pc_chipname = PCIC_TYPE_YENTA;
		break;
	}
	return (DDI_SUCCESS);
}

static void
pcic_82092_smiirq_ctl(pcicdev_t *pcic, int socket, int intr, int state)
{
	uchar_t ppirr = ddi_get8(pcic->cfg_handle,
					pcic->cfgaddr + PCIC_82092_PPIRR);
	uchar_t val;

	if (intr == PCIC_82092_CTL_SMI) {
		val = PCIC_82092_SMI_CTL(socket,
						PCIC_82092_INT_DISABLE);
		ppirr &= ~val;
		val = PCIC_82092_SMI_CTL(socket, state);
		ppirr |= val;
	} else {
		val = PCIC_82092_IRQ_CTL(socket,
						PCIC_82092_INT_DISABLE);
		ppirr &= ~val;
		val = PCIC_82092_IRQ_CTL(socket, state);
		ppirr |= val;
	}
	ddi_put8(pcic->cfg_handle, pcic->cfgaddr + PCIC_82092_PPIRR,
			ppirr);
}

/* XXX figure this out and set it once XXX */
int pcic_debounce_count = PCIC_REM_DEBOUNCE_CNT;
int pcic_debounce_intr_time = PCIC_REM_DEBOUNCE_TIME;
int pcic_debounce_count_ok = PCIC_DEBOUNCE_OK_CNT;
int pcic_do_insertion = 1;
int pcic_do_removal = 1;
/* XXX figure this out and set it once XXX */

static void
pcic_handle_cd_change(pcicdev_t *pcic, pcic_socket_t *sockp, int sn)
{
	int status, insert_cnt = 0, remove_cnt = 0, uncertain_cnt = 0;
	int debounce_time = pcic_debounce_intr_time;
	int debounce_cnt = pcic_debounce_count;
	int prev_status = 0;

	/*
	 * Check to see whether a card is present or not. There are
	 *	only two states that we are concerned with - the state
	 *	where both CD pins are asserted, which means that the
	 *	card is fully seated, and the state where neither CD
	 *	pin is asserted, which means that the card is not
	 *	present.
	 * The CD signals are generally very noisy and cause a lot of
	 *	contact bounce as the card is being inserted and
	 *	removed, so we need to do some software debouncing.
	 *	Card insertion debouncing is handled by a low-priority
	 *	soft interrupt handler, since it's not critical when
	 *	we get the card insertion event. Card removal debouncing
	 *	is handled here.
	 */

	do {
		status = pcic_getb(pcic, sn, PCIC_INTERFACE_STATUS);
		status &= PCIC_ISTAT_CD_MASK;

#if defined(PCIC_DEBUG)
		if (pcic_debug) {
			cmn_err(CE_CONT, "pcic_handle_cd_change: status=0x%x"
			    " prev_status=0x%x\n", status, prev_status);
		}
#endif

		if (status == prev_status) {
			switch (status) {
				case PCIC_CD_PRESENT_OK:
					insert_cnt++;
					break;
				case PCIC_CD_NOTPRESENT:
					remove_cnt++;
					break;
				case PCIC_CD_NOTSEATED_1:
				case PCIC_CD_NOTSEATED_2:
				default:
					uncertain_cnt++;
					break;
			} /* switch */
		} else {
			insert_cnt = 0;
			remove_cnt = 0;
			uncertain_cnt = 0;
		}

		prev_status = status;

		drv_usecwait(debounce_time);

	} while ((insert_cnt < pcic_debounce_count_ok) &&
	    (remove_cnt < pcic_debounce_count_ok) &&
	    (debounce_cnt-- > 0));


#if defined(PCIC_DEBUG)
	if (pcic_debug) {
		cmn_err(CE_CONT, "pcic_handle_cd_change: ins_cnt=%d"
		    " rem_cnt=%d unc_cnt=%d deb_cnt=%d \n",
		    insert_cnt, remove_cnt, uncertain_cnt,
		    debounce_cnt);
	}
#endif

	if (insert_cnt >= pcic_debounce_count_ok) {
		if (!(sockp->pcs_flags & PCS_CARD_PRESENT)) {
			sockp->pcs_flags |= PCS_CARD_PRESENT;
			/* XXX */
			if (pcic_do_insertion) {
				PC_CALLBACK(pcic->dip, pcic->pc_cb_arg,
						PCE_CARD_INSERT, sn);
			}
		} /* PCS_CARD_PRESENT */
	} /* insert_cnt */


	/*
	 * It is possible to unseat the card from its slot in
	 * which case the uncertain_count is greater than zero.
	 * It is best to remove the card in this situation.
	 */

	else if ((remove_cnt >= pcic_debounce_count_ok) ||
	    (uncertain_cnt > 0)) {
		if (uncertain_cnt) {
			cmn_err(CE_CONT, "pcic%d: PC Card Unseated\n",
			    ddi_get_instance(pcic->dip));
		}

		if (sockp->pcs_flags & PCS_CARD_PRESENT) {
			sockp->pcs_flags &= ~PCS_CARD_PRESENT;
			/* XXX */
			if (pcic_do_removal) {
				PC_CALLBACK(pcic->dip, pcic->pc_cb_arg,
						PCE_CARD_REMOVAL, sn);
			}
		} /* PCS_CARD_PRESENT */
	} /* remove_cnt */

}

/*
 * pcic_getb()
 *	get an I/O byte based on the yardware decode method
 */
uint8_t
pcic_getb(pcicdev_t *pcic, int socket, int reg)
{
	register int work;

#if defined(PCIC_DEBUG)
	if (pcic_debug == 0x7fff) {
		cmn_err(CE_CONT, "pcic_getb0: pcic=%p socket=%d reg=%d\n",
			(void *)pcic, socket, reg);
		cmn_err(CE_CONT, "pcic_getb1: type=%d handle=%p ioaddr=%p \n",
			pcic->pc_io_type, (void *)pcic->handle,
			(void *)pcic->ioaddr);
	}
#endif

	switch (pcic->pc_io_type) {
	case PCIC_IO_TYPE_YENTA:
		return (ddi_get8(pcic->handle,
					pcic->ioaddr + CB_R2_OFFSET + reg));
	default:
		work = (socket * PCIC_SOCKET_1) | reg;
		ddi_put8(pcic->handle, pcic->ioaddr, work);
		return (ddi_get8(pcic->handle, pcic->ioaddr + 1));
	}
}

void
pcic_putb(pcicdev_t *pcic, int socket, int reg, int8_t value)
{
	register int work;

#if defined(PCIC_DEBUG)
	if (pcic_debug == 0x7fff) {
		cmn_err(CE_CONT,
			"pcic_putb0: pcic=%p socket=%d reg=%d value=%x \n",
			(void *)pcic, socket, reg, value);
		cmn_err(CE_CONT,
			"pcic_putb1: type=%d handle=%p ioaddr=%p \n",
			pcic->pc_io_type, (void *)pcic->handle,
			(void *)pcic->ioaddr);
	}
#endif


	switch (pcic->pc_io_type) {
	case PCIC_IO_TYPE_YENTA:
		ddi_put8(pcic->handle, pcic->ioaddr + CB_R2_OFFSET + reg,
				value);
		break;
	default:
		work = (socket * PCIC_SOCKET_1) | reg;
		ddi_put8(pcic->handle, pcic->ioaddr, work);
		ddi_put8(pcic->handle, pcic->ioaddr + 1, value);
		break;
	}
}

/*
 * chip identification functions
 */

/*
 * chip identification: Cirrus Logic PD6710/6720/6722
 */
int
pcic_ci_cirrus(pcicdev_t *pcic)
{
	int value1, value2;

	/* Init the CL id mode */
	value1 = pcic_getb(pcic, 0, PCIC_CHIP_INFO);
	pcic_putb(pcic, 0, PCIC_CHIP_INFO, 0);
	value1 = pcic_getb(pcic, 0, PCIC_CHIP_INFO);
	value2 = pcic_getb(pcic, 0, PCIC_CHIP_INFO);

	if ((value1 & PCIC_CI_ID) == PCIC_CI_ID &&
	    (value2 & PCIC_CI_ID) == 0) {
		/* chip is a Cirrus Logic and not Intel */
		pcic->pc_type = PCIC_CL_PD6710;
		if (value1 & PCIC_CI_SLOTS)
			pcic->pc_chipname = PCIC_TYPE_PD6720;
		else
			pcic->pc_chipname = PCIC_TYPE_PD6710;
		/* now fine tune things just in case a 6722 */
		value1 = clext_reg_read(pcic, 0, PCIC_CLEXT_DMASK_0);
		if (value1 == 0) {
			clext_reg_write(pcic, 0, PCIC_CLEXT_SCRATCH, 0x55);
			value1 = clext_reg_read(pcic, 0, PCIC_CLEXT_SCRATCH);
			if (value1 == 0x55) {
				pcic->pc_chipname = PCIC_TYPE_PD6722;
				pcic->pc_type = PCIC_CL_PD6722;
				clext_reg_write(pcic, 0, PCIC_CLEXT_SCRATCH, 0);
			}
		}
		return (1);
	}
	return (0);
}

/*
 * chip identification: Vadem (VG365/465/468/469)
 */

void
pcic_vadem_enable(pcicdev_t *pcic)
{
	ddi_put8(pcic->handle, pcic->ioaddr, PCIC_VADEM_P1);
	ddi_put8(pcic->handle, pcic->ioaddr, PCIC_VADEM_P2);
	ddi_put8(pcic->handle, pcic->ioaddr, pcic->pc_lastreg);
}

int
pcic_ci_vadem(pcicdev_t *pcic)
{
	int value;

	pcic_vadem_enable(pcic);
	value = pcic_getb(pcic, 0, PCIC_CHIP_REVISION);
	pcic_putb(pcic, 0, PCIC_CHIP_REVISION, 0xFF);
	if (pcic_getb(pcic, 0, PCIC_CHIP_REVISION) ==
	    (value | PCIC_VADEM_D3) ||
	    (pcic_getb(pcic, 0, PCIC_CHIP_REVISION) & PCIC_REV_MASK) ==
	    PCIC_VADEM_469) {
		int vadem, new;
		pcic_vadem_enable(pcic);
		vadem = pcic_getb(pcic, 0, PCIC_VG_DMA) &
			~(PCIC_V_UNLOCK | PCIC_V_VADEMREV);
		new = vadem | (PCIC_V_VADEMREV|PCIC_V_UNLOCK);
		pcic_putb(pcic, 0, PCIC_VG_DMA, new);
		value = pcic_getb(pcic, 0, PCIC_CHIP_REVISION);

		/* want to lock but leave mouse or other on */
		pcic_putb(pcic, 0, PCIC_VG_DMA, vadem);
		switch (value & PCIC_REV_MASK) {
		case PCIC_VADEM_365:
			pcic->pc_chipname = PCIC_VG_365;
			pcic->pc_type = PCIC_VADEM;
			break;
		case PCIC_VADEM_465:
			pcic->pc_chipname = PCIC_VG_465;
			pcic->pc_type = PCIC_VADEM;
			pcic->pc_flags |= PCF_1SOCKET;
			break;
		case PCIC_VADEM_468:
			pcic->pc_chipname = PCIC_VG_468;
			pcic->pc_type = PCIC_VADEM;
			break;
		case PCIC_VADEM_469:
			pcic->pc_chipname = PCIC_VG_469;
			pcic->pc_type = PCIC_VADEM_VG469;
			break;
		}
		return (1);
	}
	return (0);
}

/*
 * chip identification: Ricoh
 */
int
pcic_ci_ricoh(pcicdev_t *pcic)
{
	int value;

	value = pcic_getb(pcic, 0, PCIC_RF_CHIP_IDENT);
	switch (value) {
	case PCIC_RF_296:
		pcic->pc_type = PCIC_RICOH;
		pcic->pc_chipname = PCIC_TYPE_RF5C296;
		return (1);
	case PCIC_RF_396:
		pcic->pc_type = PCIC_RICOH;
		pcic->pc_chipname = PCIC_TYPE_RF5C396;
		return (1);
	}
	return (0);
}

void
pcic_init_assigned(dev_info_t *dip)
{
	dev_info_t *par;
	pci_regspec_t *regs;
	int rlen;
	/*
	 * we need to see if this is a node that has a parent
	 * with "available" property.  If so, then see if that
	 * parent has been processed yet.  If so, return, if not
	 * then do it.  We only deal with PCI nexi at this point.
	 */
	for (par = dip; par != NULL; par = ddi_get_parent(par)) {
		if (ddi_getlongprop(DDI_DEV_T_NONE, par, DDI_PROP_DONTPASS,
					"available", (caddr_t)&regs,
					&rlen) == DDI_SUCCESS)
			break;
	}
	if (par != NULL) {
		/* have a valid parent */
		(void) pci_resource_setup(par);
		kmem_free((caddr_t)regs, rlen);
	}
}
