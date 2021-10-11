/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)simba.c	1.20	99/04/26 SMI"

/*
 *	PCI to PCI bus bridge nexus driver
 */

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/pci.h>
#include <sys/pci/pci_nexus.h>
#include <sys/pci/pci_regs.h>
#include <sys/pci/pci_simba.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/promif.h>		/* prom_printf */

#if defined(DEBUG) && !defined(lint)
static uint_t simba_debug_flags = 0;
#define	D_IDENTIFY	0x00000001
#define	D_ATTACH	0x00000002
#define	D_DETACH	0x00000004
#define	D_MAP		0x00000008
#define	D_CTLOPS	0x00000010
#define	D_G_ISPEC	0x00000020
#define	D_A_ISPEC	0x00000040
#define	D_INIT_CLD	0x00400000
#define	D_FAULT		0x00000080

#define	DEBUG0(f, s) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n")

#define	DEBUG1(f, s, a) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a)

#define	DEBUG2(f, s, a, b) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a, b)

#define	DEBUG3(f, s, a, b, c) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a, b, c)

#define	DEBUG4(f, s, a, b, c, d) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a, b, c, d)

#define	DEBUG5(f, s, a, b, c, d, e) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a, b, c, d, e)

#define	DEBUG6(f, s, a, b, c, d, e, ff) if ((f)& simba_debug_flags) \
	prom_printf("simba: " s "\n", a, b, c, d, e, ff)

#else

#define	DEBUG0(f, s)
#define	DEBUG1(f, s, a)
#define	DEBUG2(f, s, a, b)
#define	DEBUG3(f, s, a, b, c)
#define	DEBUG4(f, s, a, b, c, d)
#define	DEBUG5(f, s, a, b, c, d, e)
#define	DEBUG6(f, s, a, b, c, d, e, ff)

#endif

/*
 * The variable controls the default setting of the command register
 * for pci devices.  See simba_initchild() for details.
 */
static ushort_t simba_command_default = PCI_COMM_SERR_ENABLE |
					PCI_COMM_WAIT_CYC_ENAB |
					PCI_COMM_PARITY_DETECT |
					PCI_COMM_ME |
					PCI_COMM_MAE |
					PCI_COMM_IO;

static int simba_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
	off_t, off_t, caddr_t *);
static int simba_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
	void *, void *);
static int simba_fault(enum pci_fault_ops op, void *arg);

struct bus_ops simba_bus_ops = {
	BUSO_REV,
	simba_bus_map,
	0,
	0,
	0,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	simba_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0,	/* (*bus_post_event)();		*/
	i_ddi_intr_ctlops
};


static struct cb_ops simba_cb_ops = {
	nulldev,			/* open */
	nulldev,			/* close */
	nulldev,			/* strategy */
	nulldev,			/* print */
	nulldev,			/* dump */
	nulldev,			/* read */
	nulldev,			/* write */
	nulldev,			/* ioctl */
	nodev,				/* devmap */
	nodev,				/* mmap */
	nodev,				/* segmap */
	nochpoll,			/* poll */
	ddi_prop_op,			/* cb_prop_op */
	NULL,				/* streamtab */
	D_NEW | D_MP | D_HOTPLUG,	/* Driver compatibility flag */
	CB_REV,				/* rev */
	nodev,				/* int (*cb_aread)() */
	nodev				/* int (*cb_awrite)() */
};

static int simba_identify(dev_info_t *devi);
static int simba_probe(dev_info_t *);
static int simba_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int simba_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int simba_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

struct dev_ops simba_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	simba_info,		/* info */
	simba_identify,		/* identify */
	simba_probe,		/* probe */
	simba_attach,		/* attach */
	simba_detach,		/* detach */
	nulldev,		/* reset */
	&simba_cb_ops,		/* driver operations */
	&simba_bus_ops		/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"SIMBA PCI to PCI bridge nexus driver",
	&simba_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

static struct simba_cfg_state {
	dev_info_t *dip;
	ushort_t command;
	uchar_t cache_line_size;
	uchar_t latency_timer;
	uchar_t header_type;
	uchar_t bus_number;
	uchar_t sec_bus_number;
	uchar_t sub_bus_number;
	uchar_t sec_latency_timer;
	ushort_t bridge_control;
};

/*
 * soft state pointer and structure template:
 */
static void *simba_state;

typedef struct {

	dev_info_t *dip;

	/*
	 * configuration register state for the bus:
	 */
	ddi_acc_handle_t config_handle;
	uchar_t simba_cache_line_size;
	uchar_t simba_latency_timer;

	/*
	 * cpr support:
	 */
	uint_t config_state_index;
	struct simba_cfg_state *simba_config_state_p;
} simba_devstate_t;

/*
 * The following variable enables a workaround for the following obp bug:
 *
 *	1234181 - obp should set latency timer registers in pci
 *		configuration header
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
static uint_t simba_set_latency_timer_register = 1;

/*
 * The following variable enables a workaround for an obp bug to be
 * submitted.  A bug requesting a workaround fof this problem has
 * been filed:
 *
 *	1235094 - need workarounds on positron nexus drivers to set cache
 *		line size registers
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
static uint_t simba_set_cache_line_size_register = 1;


/*
 * forward function declarations:
 */
static void simba_removechild(dev_info_t *);
static int simba_initchild(dev_info_t *child);
static int simba_create_pci_prop(dev_info_t *child, uint_t *, uint_t *);
static void simba_save_config_regs(simba_devstate_t *simba_p);
static void simba_restore_config_regs(simba_devstate_t *simba_p);

extern int impl_ddi_merge_child(dev_info_t *child);

int
_init(void)
{
	int e;

	DEBUG0(D_ATTACH, "_init() installing module...\n");
	if ((e = ddi_soft_state_init(&simba_state, sizeof (simba_devstate_t),
	    1)) == 0 && (e = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&simba_state);

	DEBUG0(D_ATTACH, "_init() module installed\n");
	return (e);
}

int
_fini(void)
{
	int e;
	DEBUG0(D_ATTACH, "_fini() removing module...\n");
	if ((e = mod_remove(&modlinkage)) == 0)
		ddi_soft_state_fini(&simba_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	DEBUG0(D_ATTACH, "_info() called.\n");
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
simba_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	DEBUG0(D_ATTACH, "simba_info() called.\n");
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
simba_identify(dev_info_t *dip)
{
	DEBUG0(D_IDENTIFY, "simba_identify() called.\n");
	return (DDI_IDENTIFIED);
}

/*ARGSUSED*/
static int
simba_probe(register dev_info_t *devi)
{
	DEBUG0(D_ATTACH, "simba_probe() called.\n");
	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
simba_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	simba_devstate_t *simba;

	switch (cmd) {
	case DDI_ATTACH:

		DEBUG1(D_ATTACH, "attach(%x) ATTACH\n", (int)devi);

		/*
		 * Make sure the "device_type" property exists.
		 */
		(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
			"device_type", (caddr_t)"pci", 4);

		/*
		 * Allocate and get soft state structure.
		 */
		instance = ddi_get_instance(devi);
		if (ddi_soft_state_zalloc(simba_state, instance) != DDI_SUCCESS)
			return (DDI_FAILURE);
		simba = (simba_devstate_t *)ddi_get_soft_state(simba_state,
		    instance);
		simba->dip = devi;
		if (pci_config_setup(devi, &simba->config_handle) !=
		    DDI_SUCCESS) {
			ddi_soft_state_free(simba_state, instance);
			return (DDI_FAILURE);
		}

		/*
		 * Simba cache line size is 64 bytes and hardwired.
		 */
		simba->simba_cache_line_size =
		    pci_config_get8(simba->config_handle,
			PCI_CONF_CACHE_LINESZ);
		simba->simba_latency_timer =
		    pci_config_get8(simba->config_handle,
			PCI_CONF_LATENCY_TIMER);

		/* simba specific, clears up the pri/sec status registers */
		pci_config_put16(simba->config_handle, 0x6, 0xffff);
		pci_config_put16(simba->config_handle, 0x1e, 0xffff);

		DEBUG2(D_ATTACH, "simba_attach(): clsz=%x, lt=%x\n",
			simba->simba_cache_line_size,
			simba->simba_latency_timer);

		(void) ddi_ctlops(devi, devi, DDI_CTLOPS_AFFINITY,
			(void *)simba, (void *)simba_fault);

		ddi_report_dev(devi);
		DEBUG0(D_ATTACH, "attach(): ATTACH done\n");
		return (DDI_SUCCESS);

	case DDI_RESUME:

		/*
		 * Get the soft state structure for the bridge.
		 */
		simba = (simba_devstate_t *)
			ddi_get_soft_state(simba_state, ddi_get_instance(devi));
		simba_restore_config_regs(simba);
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
simba_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	simba_devstate_t *simba;
	simba = (simba_devstate_t *)
		ddi_get_soft_state(simba_state, ddi_get_instance(devi));

	switch (cmd) {
	case DDI_DETACH:
		DEBUG0(D_DETACH, "detach() called\n");
		pci_config_teardown(&simba->config_handle);
		(void) ddi_prop_remove(DDI_DEV_T_NONE, devi, "device_type");
		ddi_soft_state_free(simba_state, ddi_get_instance(devi));
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		simba_save_config_regs(simba);
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
simba_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	register dev_info_t *pdip;

	DEBUG3(D_MAP, "simba_bus_map(): dip=%x, rdip=%x, mp=%x", dip, rdip, mp);
	DEBUG3(D_MAP, "simba_bus_map(): offset=%x, len=%x, vaddrp=%x",
	    offset, len, vaddrp);

	pdip = (dev_info_t *)DEVI(dip)->devi_parent;
	return ((DEVI(pdip)->devi_ops->devo_bus_ops->bus_map)
	    (pdip, rdip, mp, offset, len, vaddrp));
}

/* XXX - dup of same func in pci_util.c */
static int
log_pci_cfg_err(ushort_t e, int bridge_secondary)
{
	int nerr = 0;
	if (e & PCI_STAT_PERROR) {
		nerr++;
		cmn_err(CE_CONT, "detected parity error.\n");
	}
	if (e & PCI_STAT_S_SYSERR) {
		nerr++;
		if (bridge_secondary)
			cmn_err(CE_CONT, "received system error.\n");
		else
			cmn_err(CE_CONT, "signalled system error.\n");
	}
	if (e & PCI_STAT_R_MAST_AB) {
		nerr++;
		cmn_err(CE_CONT, "received master abort.\n");
	}
	if (e & PCI_STAT_R_TARG_AB)
		cmn_err(CE_CONT, "received target abort.\n");
	if (e & PCI_STAT_S_TARG_AB)
		cmn_err(CE_CONT, "signalled target abort\n");
	if (e & PCI_STAT_S_PERROR) {
		nerr++;
		cmn_err(CE_CONT, "signalled parity error\n");
	}
	return (nerr);
}

static int
simba_fault(enum pci_fault_ops op, void *arg)
{
	simba_devstate_t *simba = (simba_devstate_t *)arg;
	ushort_t pci_cfg_stat	=
		pci_config_get16(simba->config_handle, PCI_CONF_STAT);
	ushort_t pci_cfg_sec_stat =
		pci_config_get16(simba->config_handle, 0x1e);
	uint64_t afsr = pci_config_get64(simba->config_handle, 0xe8);
	uint64_t afar = pci_config_get64(simba->config_handle, 0xf0);
	char nm[24];
	int nerr = 0;

#ifdef lint
	afar = afar;
#endif
	DEBUG3(D_FAULT, "simba_fault(): %s-%d: op=%x",
		ddi_driver_name(simba->dip), ddi_get_instance(simba->dip), op);
	DEBUG6(D_FAULT, "simba_fault(): config status 0x%x "
		"secondary status 0x%x\nafsr 0x%x.%8x afar 0x%x.%8x",
			pci_cfg_stat, pci_cfg_sec_stat, (uint_t)(afsr >> 32),
			(uint_t)afsr, (uint_t)(afar >> 32), (uint_t)afar);

	switch (op) {
	case FAULT_LOG:
		(void) sprintf(nm, "%s-%d", ddi_driver_name(simba->dip),
			ddi_get_instance(simba->dip));

		cmn_err(CE_WARN, "%s: Simba fault log start:\n", nm);
		cmn_err(CE_WARN, "%s: primary err (%x):\n", nm, pci_cfg_stat);
		nerr += log_pci_cfg_err(pci_cfg_stat, 0);
		cmn_err(CE_WARN, "%s: sec err (%x):\n", nm, pci_cfg_sec_stat);
		nerr += log_pci_cfg_err(pci_cfg_sec_stat, 1);
		cmn_err(CE_CONT, "%s: PCI fault log end.\n", nm);
		break;
	case FAULT_POKEFINI:
	case FAULT_RESET:
		DEBUG6(D_FAULT, "%s-%d: cleaning up fault bits %x %x %x.%8x\n",
			ddi_driver_name(simba->dip),
			ddi_get_instance(simba->dip), pci_cfg_stat,
			pci_cfg_sec_stat, (uint_t)(afsr >> 32), (uint_t)afsr);
		pci_config_put16(simba->config_handle,
			PCI_CONF_STAT, pci_cfg_stat);
		pci_config_put16(simba->config_handle, 0x1e, pci_cfg_sec_stat);
		pci_config_put64(simba->config_handle, 0xe8, afsr);
		break;
	case FAULT_POKEFLT:
		if (!(pci_cfg_stat & PCI_STAT_S_SYSERR))
			return (1);
		if (!(pci_cfg_sec_stat & PCI_STAT_R_MAST_AB))
			return (1);
		break;
	default:
		break;
	}
#ifdef DEBUG
	if (op != 0xbad)
		(void) simba_fault(0xbad, simba);
#endif
	return (DDI_SUCCESS);
}

#if defined(DEBUG) && !defined(lint)
static char *ops[] =
{
	"DDI_CTLOPS_DMAPMAPC",
	"DDI_CTLOPS_INITCHILD",
	"DDI_CTLOPS_UNINITCHILD",
	"DDI_CTLOPS_REPORTDEV",
	"DDI_CTLOPS_REPORTINT",
	"DDI_CTLOPS_REGSIZE",
	"DDI_CTLOPS_NREGS",
	"DDI_CTLOPS_NINTRS",
	"DDI_CTLOPS_SIDDEV",
	"DDI_CTLOPS_SLAVEONLY",
	"DDI_CTLOPS_AFFINITY",
	"DDI_CTLOPS_IOMIN",
	"DDI_CTLOPS_PTOB",
	"DDI_CTLOPS_BTOP",
	"DDI_CTLOPS_BTOPR",
	"DDI_CTLOPS_POKE_INIT",
	"DDI_CTLOPS_POKE_FLUSH",
	"DDI_CTLOPS_POKE_FINI",
	"DDI_CTLOPS_INTR_HILEVEL",
	"DDI_CTLOPS_XLATE_INTRS",
	"DDI_CTLOPS_DVMAPAGESIZE",
	"DDI_CTLOPS_POWER"
};
#endif

/*ARGSUSED*/
static int
simba_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t ctlop,
	void *arg, void *result)
{
	int reglen;
	int rn;
	int totreg;
	pci_regspec_t *drv_regp;

	DEBUG6(D_CTLOPS, "simba_ctlops(): dip=%x, rdip=%x, ctlop=%x-%s,"
	    " arg=%x, result=%x", dip, rdip, ctlop, ops[ctlop], arg, result);

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?PCI-device: %s@%s, %s%d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip),
		    ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (simba_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		simba_removechild((dev_info_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_SIDDEV:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		break;

	default:
		DEBUG0(D_CTLOPS, "simba_ctlops(): calling ddi_ctlops()");
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}

	*(int *)result = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, rdip,
		DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "reg",
		(caddr_t)&drv_regp, &reglen) != DDI_SUCCESS)
		return (DDI_FAILURE);

	totreg = reglen / sizeof (pci_regspec_t);
	if (ctlop == DDI_CTLOPS_NREGS)
		*(int *)result = totreg;
	else if (ctlop == DDI_CTLOPS_REGSIZE) {
		rn = *(int *)arg;
		if (rn > totreg)
			return (DDI_FAILURE);
		*(off_t *)result = drv_regp[rn].pci_size_low;
	}

	kmem_free(drv_regp, reglen);
	DEBUG1(D_CTLOPS, "simba_ctlops(): *result=%x\n", *(off_t *)result);
	return (DDI_SUCCESS);
}

static int
simba_initchild(dev_info_t *child)
{
	char name[MAXNAMELEN];
	int ret, i;
	uint_t slot, func;
	ddi_acc_handle_t config_handle;
	ushort_t command_preserve, command;
	uchar_t header_type;
	uchar_t min_gnt, latency_timer;
	simba_devstate_t *simba;
	uint_t n;

	DEBUG1(D_INIT_CLD, "simba_initchild(): child=%x\n", child);

	/*
	 * Pseudo nodes indicate a prototype node with per-instance
	 * properties to be merged into the real h/w device node.
	 * The interpretation of the unit-address is DD[,F]
	 * where DD is the device id and F is the function.
	 */
	if (ndi_dev_is_persistent_node(child) == 0) {
		pci_regspec_t *pci_rp;
		char **unit_addr;

		if (ddi_getlongprop(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "reg", (caddr_t)&pci_rp, &i) ==
		    DDI_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_driver_name(child));
			kmem_free(pci_rp, i);
			return (DDI_NOT_WELL_FORMED);
		}

		if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "unit-address", &unit_addr, &n) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_driver_name(child));
			return (DDI_NOT_WELL_FORMED);
		}
		if (n != 1 || *unit_addr == NULL || **unit_addr == 0) {
			cmn_err(CE_WARN, "unit-address property in %s.conf"
			    " not well-formed", ddi_driver_name(child));
			ddi_prop_free(unit_addr);
			return (DDI_NOT_WELL_FORMED);
		}
		ddi_set_name_addr(child, *unit_addr);
		ddi_set_parent_data(child, NULL);
		ddi_prop_free(unit_addr);

		/*
		 * Try to merge the properties from this prototype
		 * node into real h/w nodes.
		 */
		if (impl_ddi_merge_child(child) != DDI_SUCCESS) {
			/*
			 * Merged ok - return failure to remove the node.
			 */
			return (DDI_FAILURE);
		}
		/*
		 * The child was not merged into a h/w node,
		 * but there's not much we can do with it other
		 * than return failure to cause the node to be removed.
		 */
		cmn_err(CE_WARN, "!%s@%s: %s.conf properties not merged",
		    ddi_driver_name(child), ddi_get_name_addr(child),
		    ddi_driver_name(child));
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * Initialize real h/w nodes
	 */
	if ((ret = simba_create_pci_prop(child, &slot, &func)) != DDI_SUCCESS)
		return (ret);
	if (func != 0)
		(void) sprintf(name, "%x,%x", slot, func);
	else
		(void) sprintf(name, "%x", slot);

	ddi_set_name_addr(child, name);

	ddi_set_parent_data(child, NULL);

	if (pci_config_setup(child, &config_handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

	DEBUG0(D_INIT_CLD, "simba_initchild(): pci_config_setup success!\n");

	/*
	 * Determine the configuration header type.
	 */
	header_type = pci_config_get8(config_handle, PCI_CONF_HEADER);

	/*
	 * Support for the "command-preserve" property.
	 */
	command_preserve = ddi_prop_get_int(DDI_DEV_T_ANY, child,
	    DDI_PROP_DONTPASS, "command-preserve", 0);
	command = pci_config_get16(config_handle, PCI_CONF_COMM);
	command &= (command_preserve | PCI_COMM_BACK2BACK_ENAB);
	command |= (simba_command_default & ~command_preserve);
	pci_config_put16(config_handle, PCI_CONF_COMM, command);

	/* clean up all PCI child devices status register */
	pci_config_put16(config_handle, PCI_CONF_STAT, 0xffff);

	/*
	 * If the device has a primary bus control register then program it
	 * based on the settings in the command register.
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
		ushort_t bcr =
		    pci_config_get16(config_handle, PCI_BCNF_BCNTRL);
		if (simba_command_default & PCI_COMM_PARITY_DETECT)
			bcr |= PCI_BCNF_BCNTRL_PARITY_ENABLE;
		if (simba_command_default & PCI_COMM_SERR_ENABLE)
			bcr |= PCI_BCNF_BCNTRL_SERR_ENABLE;
		bcr |= PCI_BCNF_BCNTRL_MAST_AB_MODE;
		pci_config_put8(config_handle, PCI_BCNF_BCNTRL, bcr);
	}

	simba = (simba_devstate_t *)ddi_get_soft_state(simba_state,
		ddi_get_instance(ddi_get_parent(child)));
	/*
	 * Initialize cache-line-size configuration register if needed.
	 */
	if (simba_set_cache_line_size_register &&
	    ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"cache-line-size", 0) == 0) {
		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
		    simba->simba_cache_line_size);
		n = pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		if (n != 0)
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
				"cache-line-size", n);
	}

	/*
	 * Initialize latency timer configuration registers if needed.
	 */
	if (simba_set_latency_timer_register &&
	    ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"latency-timer", 0) == 0) {

		if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
			latency_timer = simba->simba_latency_timer;
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
			    simba->simba_latency_timer);
		} else {
			min_gnt = pci_config_get8(config_handle,
			    PCI_CONF_MIN_G);
			latency_timer = min_gnt * 8;
		}
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
		    latency_timer);
		n = pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if (n != 0)
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			    "latency-timer", n);
	}

	pci_config_teardown(&config_handle);
	DEBUG0(D_INIT_CLD, "simba_initchild(): pci_config_teardown called\n");
	return (DDI_SUCCESS);
}

static void
simba_removechild(dev_info_t *dip)
{
	ddi_set_name_addr(dip, NULL);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);

	impl_rem_dev_props(dip);
}

static int
simba_create_pci_prop(dev_info_t *child, uint_t *foundslot, uint_t *foundfunc)
{
	pci_regspec_t *pci_rp;
	int	length;
	int	value;

	/* get child "reg" property */
	value = ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_CANSLEEP,
		"reg", (caddr_t)&pci_rp, &length);
	if (value != DDI_SUCCESS)
		return (value);

	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, child, "reg",
		(int *)pci_rp, length / sizeof (int));

	/* copy the device identifications */
	*foundslot = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	*foundfunc = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_getlongprop ().
	 */
	kmem_free(pci_rp, length);

	/* assign the basic PCI Properties */

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"vendor-id", -1);
	if (value != -1) {
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			"vendor-id", value);
	}

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"device-id", -1);
	if (value != -1) {
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			"device-id", value);
	}

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"interrupts", -1);
	if (value != -1) {
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			"interrupts", value);
	}
	return (DDI_SUCCESS);
}


/*
 * simba_save_config_regs
 *
 * This routine saves the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: simba_detach() on suspends
 *
 * return value: none
 */
static void
simba_save_config_regs(simba_devstate_t *simba_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t ch;
	struct simba_cfg_state *statep;

	for (i = 0, dip = ddi_get_child(simba_p->dip); dip != NULL;
		dip = ddi_get_next_sibling(dip)) {
		if (DDI_CF2(dip))
			i++;
	}
	if (!i)
		return;
	simba_p->simba_config_state_p =
		kmem_zalloc(i * sizeof (struct simba_cfg_state), KM_NOSLEEP);
	if (!simba_p->simba_config_state_p) {
		cmn_err(CE_WARN, "not enough memrory to save simba child\n");
		return;
	}
	simba_p->config_state_index = i;

	for (statep = simba_p->simba_config_state_p,
		dip = ddi_get_child(simba_p->dip);
		dip != NULL;
		dip = ddi_get_next_sibling(dip)) {

		if (!DDI_CF2(dip)) {
			DEBUG4(D_DETACH, "%s%d: skipping unattached %s%d\n",
				ddi_driver_name(simba_p->dip),
				ddi_get_instance(simba_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}

		DEBUG4(D_DETACH, "%s%d: saving regs for %s%d\n",
			ddi_driver_name(simba_p->dip),
			ddi_get_instance(simba_p->dip),
			ddi_driver_name(dip),
			ddi_get_instance(dip));

		if (pci_config_setup(dip, &ch) != DDI_SUCCESS) {
			DEBUG4(D_DETACH, "%s%d: can't config space for %s%d\n",
				ddi_driver_name(simba_p->dip),
				ddi_get_instance(simba_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}

		DEBUG3(D_DETACH, "%s%d: saving child dip=%x\n",
			ddi_driver_name(simba_p->dip),
			ddi_get_instance(simba_p->dip),
			dip);

		statep->dip = dip;
		statep->command = pci_config_get16(ch, PCI_CONF_COMM);
		statep->header_type = pci_config_get8(ch, PCI_CONF_HEADER);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			statep->bridge_control =
			    pci_config_get16(ch, PCI_BCNF_BCNTRL);
		statep->cache_line_size =
			pci_config_get8(ch, PCI_CONF_CACHE_LINESZ);
		statep->latency_timer =
			pci_config_get8(ch, PCI_CONF_LATENCY_TIMER);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			statep->sec_latency_timer =
				pci_config_get8(ch, PCI_BCNF_LATENCY_TIMER);
		/*
		 * Simba specific.
		 */
		if (pci_config_get16(ch, PCI_CONF_VENID) == PCI_SIMBA_VENID &&
		    pci_config_get16(ch, PCI_CONF_DEVID) == PCI_SIMBA_DEVID) {

			statep->bus_number =
				pci_config_get8(ch, PCI_BCNF_PRIBUS);
			statep->sec_bus_number =
				pci_config_get8(ch, PCI_BCNF_SECBUS);
			statep->sub_bus_number =
				pci_config_get8(ch, PCI_BCNF_SUBBUS);
			statep->bridge_control =
				pci_config_get16(ch, PCI_BCNF_BCNTRL);
		}
		pci_config_teardown(&ch);
		statep++;
	}
}


/*
 * simba_restore_config_regs
 *
 * This routine restores the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: simba_attach() on resume
 *
 * return value: none
 */
static void
simba_restore_config_regs(simba_devstate_t *simba_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t ch;
	struct simba_cfg_state *statep = simba_p->simba_config_state_p;
	if (!simba_p->config_state_index)
		return;

	for (i = 0; i < simba_p->config_state_index; i++, statep++) {
		dip = statep->dip;
		if (!dip) {
			cmn_err(CE_WARN,
				"%s%d: skipping bad dev info (%d)\n",
				ddi_driver_name(simba_p->dip),
				ddi_get_instance(simba_p->dip),
				i);
			continue;
		}

		DEBUG5(D_ATTACH, "%s%d: restoring regs for %x-%s%d\n",
			ddi_driver_name(simba_p->dip),
			ddi_get_instance(simba_p->dip),
			dip,
			ddi_driver_name(dip),
			ddi_get_instance(dip));

		if (pci_config_setup(dip, &ch) != DDI_SUCCESS) {
			DEBUG4(D_ATTACH, "%s%d: can't config space for %s%d\n",
				ddi_driver_name(simba_p->dip),
				ddi_get_instance(simba_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}
		pci_config_put16(ch, PCI_CONF_COMM, statep->command);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			pci_config_put16(ch, PCI_BCNF_BCNTRL,
				statep->bridge_control);
		/*
		 * Simba specific.
		 */
		if (pci_config_get16(ch, PCI_CONF_VENID) == PCI_SIMBA_VENID &&
		    pci_config_get16(ch, PCI_CONF_DEVID) == PCI_SIMBA_DEVID) {
			pci_config_put8(ch, PCI_BCNF_PRIBUS,
				statep->bus_number);
			pci_config_put8(ch, PCI_BCNF_SECBUS,
				statep->sec_bus_number);
			pci_config_put8(ch, PCI_BCNF_SUBBUS,
				statep->sub_bus_number);
			pci_config_put16(ch, PCI_BCNF_BCNTRL,
				statep->bridge_control);
		}

		pci_config_put8(ch, PCI_CONF_CACHE_LINESZ,
			statep->cache_line_size);
		pci_config_put8(ch, PCI_CONF_LATENCY_TIMER,
			statep->latency_timer);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			pci_config_put8(ch, PCI_BCNF_LATENCY_TIMER,
				statep->sec_latency_timer);
		pci_config_teardown(&ch);
	}

	kmem_free(simba_p->simba_config_state_p,
		simba_p->config_state_index * sizeof (struct simba_cfg_state));
	simba_p->simba_config_state_p = NULL;
	simba_p->config_state_index = 0;
}
