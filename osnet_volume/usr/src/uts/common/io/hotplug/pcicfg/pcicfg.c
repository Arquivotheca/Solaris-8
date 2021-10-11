/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)pcicfg.c	1.31	99/06/18 SMI"

/*
 *     PCI configurator (pcicfg_)
 */

#include <sys/isa_defs.h>

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>

#include <sys/hwconf.h>
#include <sys/ddi_impldefs.h>

#include <sys/pci.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/hotplug/pci/pcicfg.h>

#include <sys/ndi_impldefs.h>

/*
 * ************************************************************************
 * *** Implementation specific local data structures/definitions.	***
 * ************************************************************************
 */

#define	PCICFG_MAX_DEVICE 32
#define	PCICFG_MAX_FUNCTION 8

#define	PCICFG_NODEVICE 42
#define	PCICFG_NOMEMORY 43
#define	PCICFG_NOMULTI	44

#define	PCICFG_HIADDR(n) ((uint32_t)(((uint64_t)(n) & 0xFFFFFFFF00000000)>> 32))
#define	PCICFG_LOADDR(n) ((uint32_t)((uint64_t)(n) & 0x00000000FFFFFFFF))
#define	PCICFG_LADDR(lo, hi)	(((uint64_t)(hi) << 32) | (uint32_t)(lo))

#define	PCICFG_HIWORD(n) ((uint16_t)(((uint32_t)(n) & 0xFFFF0000)>> 16))
#define	PCICFG_LOWORD(n) ((uint16_t)((uint32_t)(n) & 0x0000FFFF))
#define	PCICFG_HIBYTE(n) ((uint8_t)(((uint16_t)(n) & 0xFF00)>> 8))
#define	PCICFG_LOBYTE(n) ((uint8_t)((uint16_t)(n) & 0x00FF))

#define	PCICFG_ROUND_UP(addr, gran) ((uintptr_t)((gran+addr-1)&(~(gran-1))))
#define	PCICFG_ROUND_DOWN(addr, gran) ((uintptr_t)((addr) & ~(gran-1)))

#define	PCICFG_MEMGRAN 0x100000
#define	PCICFG_IOGRAN 0x1000
#define	PCICFG_4GIG_LIMIT 0xFFFFFFFFUL

#define	PCICFG_MEM_MULT 4
#define	PCICFG_IO_MULT 4
#define	PCICFG_RANGE_LEN 2 /* Number of range entries */

/*
 * The following typedef is used to represent a
 * 1275 "bus-range" property of a PCI Bus node.
 * DAF - should be in generic include file...
 */

typedef struct pcicfg_bus_range {
	uint32_t lo;
	uint32_t hi;
} pcicfg_bus_range_t;

typedef struct pcicfg_range {

	uint32_t child_hi;
	uint32_t child_mid;
	uint32_t child_lo;
	uint32_t parent_hi;
	uint32_t parent_mid;
	uint32_t parent_lo;
	uint32_t size_hi;
	uint32_t size_lo;

} pcicfg_range_t;

typedef struct pcicfg_phdl pcicfg_phdl_t;

struct pcicfg_phdl {

	dev_info_t	*dip;		/* Associated with the attach point */
	pcicfg_phdl_t	*next;

	uint64_t	memory_base;	/* Memory base for this attach point */
	uint64_t	memory_last;
	uint64_t	memory_len;
	uint32_t	io_base;	/* I/O base for this attach point */
	uint32_t	io_last;
	uint32_t	io_len;

	int		error;
	uint_t		highest_bus;	/* Highest bus seen on the probe */
	ndi_ra_request_t mem_req;	/* allocator request for memory */
	ndi_ra_request_t io_req;	/* allocator request for I/O */
};

struct pcicfg_standard_prop_entry {
    uchar_t *name;
    uint_t  config_offset;
    uint_t  size;
};


struct pcicfg_name_entry {
    uint32_t class_code;
    char  *name;
};

struct pcicfg_find_ctrl {
	uint_t		device;
	uint_t		function;
	dev_info_t	*dip;
};

#define	PCICFG_MAKE_REG_HIGH(busnum, devnum, funcnum, register)\
	(\
	((ulong_t)(busnum & 0xff) << 16)    |\
	((ulong_t)(devnum & 0x1f) << 11)    |\
	((ulong_t)(funcnum & 0x7) <<  8)    |\
	((ulong_t)(register & 0x3f)))

/*
 * debug macros:
 */
#if defined(DEBUG)
extern void prom_printf(const char *, ...);

int pcicfg_debug = 0;

static void debug(char *, uintptr_t, uintptr_t,
	uintptr_t, uintptr_t, uintptr_t);

#define	DEBUG0(fmt)\
	debug(fmt, 0, 0, 0, 0, 0);
#define	DEBUG1(fmt, a1)\
	debug(fmt, (uintptr_t)(a1), 0, 0, 0, 0);
#define	DEBUG2(fmt, a1, a2)\
	debug(fmt, (uintptr_t)(a1), (uintptr_t)(a2), 0, 0, 0);
#define	DEBUG3(fmt, a1, a2, a3)\
	debug(fmt, (uintptr_t)(a1), (uintptr_t)(a2),\
		(uintptr_t)(a3), 0, 0);
#define	DEBUG4(fmt, a1, a2, a3, a4)\
	debug(fmt, (uintptr_t)(a1), (uintptr_t)(a2),\
		(uintptr_t)(a3), (uintptr_t)(a4), 0);
#else
#define	DEBUG0(fmt)
#define	DEBUG1(fmt, a1)
#define	DEBUG2(fmt, a1, a2)
#define	DEBUG3(fmt, a1, a2, a3)
#define	DEBUG4(fmt, a1, a2, a3, a4)
#endif

/*
 * forward declarations for routines defined in this module (called here)
 */

static int pcicfg_add_config_reg(dev_info_t *,
	uint_t, uint_t, uint_t);
static int pcicfg_probe_children(dev_info_t *,
	uint_t, uint_t, uint_t);
static int pcicfg_match_dev(dev_info_t *, void *);
static dev_info_t *pcicfg_devi_find(dev_info_t *, uint_t, uint_t);
static pcicfg_phdl_t *pcicfg_find_phdl(dev_info_t *);
static pcicfg_phdl_t *pcicfg_create_phdl(dev_info_t *);
static int pcicfg_destroy_phdl(dev_info_t *);
static int pcicfg_sum_resources(dev_info_t *, void *);
static int pcicfg_allocate_chunk(dev_info_t *);
static int pcicfg_program_ap(dev_info_t *);
static int pcicfg_device_assign(dev_info_t *);
static int pcicfg_bridge_assign(dev_info_t *, void *);
static int pcicfg_free_resources(dev_info_t *);
static void pcicfg_setup_bridge(pcicfg_phdl_t *, ddi_acc_handle_t);
static void pcicfg_update_bridge(pcicfg_phdl_t *, ddi_acc_handle_t);
static int pcicfg_update_assigned_prop(dev_info_t *, pci_regspec_t *);
static void pcicfg_device_on(ddi_acc_handle_t);
static void pcicfg_device_off(ddi_acc_handle_t);
static int pcicfg_set_busnode_props(dev_info_t *);
static int pcicfg_free_bridge_resources(dev_info_t *);
static int pcicfg_free_device_resources(dev_info_t *);
static int pcicfg_teardown_device(dev_info_t *);
static void pcicfg_devi_attach_to_parent(dev_info_t *);
static void pcicfg_reparent_node(dev_info_t *, dev_info_t *);
static int pcicfg_config_setup(dev_info_t *, ddi_acc_handle_t *);
static void pcicfg_config_teardown(ddi_acc_handle_t *);
static void pcicfg_get_mem(pcicfg_phdl_t *, uint32_t, uint64_t *);
static void pcicfg_get_io(pcicfg_phdl_t *, uint32_t, uint32_t *);
static int pcicfg_update_ranges_prop(dev_info_t *, pcicfg_range_t *);

#ifdef DEBUG
static void pcicfg_dump_device_config(ddi_acc_handle_t);
#endif

static kmutex_t pcicfg_list_mutex; /* Protects the probe handle list */
static pcicfg_phdl_t *pcicfg_phdl_list = NULL;

#ifndef _DONT_USE_1275_GENERIC_NAMES
/*
 * Class code table
 */
static struct pcicfg_name_entry pcicfg_class_lookup [] = {

	{ 0x001, "display" },
	{ 0x100, "scsi" },
	{ 0x101, "ide" },
	{ 0x102, "fdc" },
	{ 0x103, "ipi" },
	{ 0x104, "raid" },
	{ 0x200, "ethernet" },
	{ 0x201, "token-ring" },
	{ 0x202, "fddi" },
	{ 0x203, "atm" },
	{ 0x300, "display" },
	{ 0x400, "video" },
	{ 0x401, "sound" },
	{ 0x500, "memory" },
	{ 0x501, "flash" },
	{ 0x600, "host" },
	{ 0x601, "isa" },
	{ 0x602, "eisa" },
	{ 0x603, "mca" },
	{ 0x604, "pci" },
	{ 0x605, "pcmcia" },
	{ 0x606, "nubus" },
	{ 0x607, "cardbus" },
	{ 0x700, "serial" },
	{ 0x701, "parallel" },
	{ 0x800, "interrupt-controller" },
	{ 0x801, "dma-controller" },
	{ 0x802, "timer" },
	{ 0x803, "rtc" },
	{ 0x900, "keyboard" },
	{ 0x901, "pen" },
	{ 0x902, "mouse" },
	{ 0xa00, "dock" },
	{ 0xb00, "cpu" },
	{ 0xc00, "firewire" },
	{ 0xc01, "access-bus" },
	{ 0xc02, "ssa" },
	{ 0xc03, "usb" },
	{ 0xc04, "fibre-channel" },
	{ 0, 0 }
};
#endif /* _DONT_USE_1275_GENERIC_NAMES */

/*
 * Module control operations
 */

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, /* Type of module */
	"PCI configurator"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};


#ifdef DEBUG

void
pcicfg_dump_common_config(ddi_acc_handle_t config_handle)
{
	prom_printf(" Vendor ID   = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_VENID));
	prom_printf(" Device ID   = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_DEVID));
	prom_printf(" Command REG = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_COMM));
	prom_printf(" Status  REG = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_STAT));
	prom_printf(" Revision ID = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_REVID));
	prom_printf(" Prog Class  = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_PROGCLASS));
	prom_printf(" Dev Class   = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_SUBCLASS));
	prom_printf(" Base Class  = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_BASCLASS));
	prom_printf(" Device ID   = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ));
	prom_printf(" Header Type = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_HEADER));
	prom_printf(" BIST        = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_BIST));
	prom_printf(" BASE 0      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE0));
	prom_printf(" BASE 1      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE1));

}
void
pcicfg_dump_device_config(ddi_acc_handle_t config_handle)
{
	pcicfg_dump_common_config(config_handle);

	prom_printf(" BASE 2      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE2));
	prom_printf(" BASE 3      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE3));
	prom_printf(" BASE 4      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE4));
	prom_printf(" BASE 5      = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_BASE5));
	prom_printf(" Cardbus CIS = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_CIS));
	prom_printf(" Sub VID     = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_SUBVENID));
	prom_printf(" Sub SID     = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_SUBSYSID));
	prom_printf(" ROM         = [0x%x]\n",
		pci_config_get32(config_handle, PCI_CONF_ROM));
	prom_printf(" I Line      = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_ILINE));
	prom_printf(" I Pin       = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_IPIN));
	prom_printf(" Max Grant   = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_MIN_G));
	prom_printf(" Max Latent  = [0x%x]\n",
		pci_config_get8(config_handle, PCI_CONF_MAX_L));
}
void
pcicfg_dump_bridge_config(ddi_acc_handle_t config_handle)
{
	pcicfg_dump_common_config(config_handle);

	prom_printf("........................................\n");

	prom_printf(" Pri Bus     = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_PRIBUS));
	prom_printf(" Sec Bus     = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_SECBUS));
	prom_printf(" Sub Bus     = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_SUBBUS));
	prom_printf(" Latency     = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_LATENCY_TIMER));
	prom_printf(" I/O Base LO = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_IO_BASE_LOW));
	prom_printf(" I/O Lim LO  = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_IO_LIMIT_LOW));
	prom_printf(" Sec. Status = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_SEC_STATUS));
	prom_printf(" Mem Base    = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_MEM_BASE));
	prom_printf(" Mem Limit   = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_MEM_LIMIT));
	prom_printf(" PF Mem Base = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_PF_BASE_LOW));
	prom_printf(" PF Mem Lim  = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_PF_LIMIT_LOW));
	prom_printf(" PF Base HI  = [0x%x]\n",
		pci_config_get32(config_handle, PCI_BCNF_PF_BASE_HIGH));
	prom_printf(" PF Lim  HI  = [0x%x]\n",
		pci_config_get32(config_handle, PCI_BCNF_PF_LIMIT_HIGH));
	prom_printf(" I/O Base HI = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_IO_BASE_HI));
	prom_printf(" I/O Lim HI  = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_IO_LIMIT_HI));
	prom_printf(" ROM addr    = [0x%x]\n",
		pci_config_get32(config_handle, PCI_BCNF_ROM));
	prom_printf(" Intr Line   = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_ILINE));
	prom_printf(" Intr Pin    = [0x%x]\n",
		pci_config_get8(config_handle, PCI_BCNF_IPIN));
	prom_printf(" Bridge Ctrl = [0x%x]\n",
		pci_config_get16(config_handle, PCI_BCNF_BCNTRL));
}

#endif


_init()
{
	DEBUG0(" PCI configurator installed\n");
	mutex_init(&pcicfg_list_mutex, NULL, MUTEX_DRIVER, NULL);
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	error = mod_remove(&modlinkage);
	if (error != 0) {
		return (error);
	}
	mutex_destroy(&pcicfg_list_mutex);
	return (0);
}

_info(modinfop)
struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * This entry point is called to configure a device (and
 * all its children) on the given bus. It is called when
 * a new device is added to the PCI domain.  This routine
 * will create the device tree and program the devices
 * registers.
 */

int
pcicfg_configure(dev_info_t *devi, uint_t device)
{
	uint_t bus;
	int len;
	int func;
	dev_info_t *new_device;
	dev_info_t *attach_point;
	pcicfg_bus_range_t pci_bus_range;
	int rv;

	/*
	 * Start probing at the device specified in "device" on the
	 * "bus" specified.
	 */
	len = sizeof (pcicfg_bus_range_t);
	if (ddi_getlongprop_buf(DDI_DEV_T_NONE, devi, DDI_PROP_DONTPASS,
		"bus-range", (caddr_t)&pci_bus_range, &len) != DDI_SUCCESS) {
		DEBUG0("no bus-range property\n");
		return (PCICFG_FAILURE);
	}

	bus = pci_bus_range.lo; /* primary bus number of this bus node */

	if (ndi_devi_alloc(devi, DEVI_PSEUDO_NEXNAME,
		(dnode_t)DEVI_SID_NODEID, &attach_point) != NDI_SUCCESS) {
		DEBUG0("pcicfg_configure(): Failed to alloc probe node\n");
		return (PCICFG_FAILURE);
	}

	/*
	 * Node name marks this node as the "attachment point".
	 */
	if (ndi_devi_set_nodename(attach_point,
		"hp_attachment", 0) != NDI_SUCCESS) {
		DEBUG0("Failed to set nodename for attachment node\n");
		(void) ndi_devi_free(attach_point);
		return (PCICFG_FAILURE);
	}

	for (func = 0; func < PCICFG_MAX_FUNCTION; func++) {

		DEBUG3("Configuring [0x%x][0x%x][0x%x]\n", bus, device, func);

		switch (rv = pcicfg_probe_children(attach_point,
			bus, device, func)) {
			case PCICFG_FAILURE:
				DEBUG2("configure failed: "
				"bus [0x%x] device [0x%x]\n",
					bus, device);
				goto cleanup;
			case PCICFG_NODEVICE:
				DEBUG3("no device : bus "
				"[0x%x] slot [0x%x] func [0x%x]\n",
					bus, device, func);
				break;
			default:
				DEBUG3("configure: bus => [%d] "
				"slot => [%d] func => [%d]\n",
					bus, device, func);
			break;
		}

		if (rv != PCICFG_SUCCESS)
			break;

		if ((new_device = pcicfg_devi_find(attach_point,
			device, func)) == NULL) {
			DEBUG0("Did'nt find device node just created\n");
			goto cleanup;
		}

		if (pcicfg_program_ap(new_device) == PCICFG_FAILURE) {
			DEBUG0("Failed to program devices\n");
			goto cleanup;
		}

		(void) pcicfg_reparent_node(new_device, devi);

		(void) pcicfg_devi_attach_to_parent(new_device);
	}

	(void) ndi_devi_free(attach_point);

	if (func == 0)
		return (PCICFG_FAILURE);	/* probe failed */
	else
		return (PCICFG_SUCCESS);

cleanup:
	/*
	 * Clean up a partially created "probe state" tree.
	 * There are no resources allocated to the in the
	 * probe state.
	 */

	for (func = 0; func < PCICFG_MAX_FUNCTION; func++) {
		if ((new_device = pcicfg_devi_find(devi,
			device, func)) == NULL) {
			DEBUG0("No more devices to clean up\n");
			break;
		}

		DEBUG2("Cleaning up device [0x%x] function [0x%x]\n",
			device, func);
		/*
		 * If this was a bridge device it will have a
		 * probe handle - if not, no harm in calling this.
		 */
		(void) pcicfg_destroy_phdl(new_device);
		/*
		 * This will free up the node
		 */
		(void) ndi_devi_offline(new_device, NDI_DEVI_REMOVE);
	}

	(void) ndi_devi_free(attach_point);

	return (PCICFG_FAILURE);
}

/*
 * This will turn  resources allocated by pcicfg_configure()
 * and remove the device tree from the attachment point
 * and below.  The routine assumes the devices have their
 * drivers detached.
 */
int
pcicfg_unconfigure(dev_info_t *devi, uint_t device)
{
	dev_info_t *child_dip;
	int func;
	int i;

	/*
	 * Cycle through devices to make sure none are busy.
	 * If a single device is busy fail the whole unconfigure.
	 */
	for (func = 0; func < PCICFG_MAX_FUNCTION; func++) {
		if ((child_dip = pcicfg_devi_find(devi, device, func)) == NULL)
			break;
		if (ndi_devi_offline(child_dip, NDI_UNCONFIG) == NDI_SUCCESS)
			continue;
		/*
		 * Device function is busy. Before returning we have to
		 * put all functions back online which were taken
		 * offline during the process.
		 */
		DEBUG2("Device [0x%x] function [%x] is busy\n", device, func);
		for (i = 0; i < func; i++) {
		    if ((child_dip = pcicfg_devi_find(devi, device, i))
			== NULL) {
			DEBUG0("No more devices to put back on line!!\n");
			/*
			 * Made it through all functions
			 */
			break;
		    }
		    if (ndi_devi_online(child_dip, NDI_CONFIG) != NDI_SUCCESS) {
			DEBUG0("Failed to put back devices state\n");
			return (PCICFG_FAILURE);
		    }
		}
		return (PCICFG_FAILURE);
	}

	/*
	 * Now, tear down all devinfo nodes for this AP.
	 */
	for (func = 0; func < PCICFG_MAX_FUNCTION; func++) {
		if ((child_dip = pcicfg_devi_find(devi,
			device, func)) == NULL) {
			DEBUG0("No more devices to tear down!\n");
			break;
		}

		DEBUG2("Tearing down device [0x%x] function [0x%x]\n",
			device, func);

		if (pcicfg_teardown_device(child_dip) != PCICFG_SUCCESS) {
			DEBUG2("Failed to tear down device [0x%x]"
			"function [0x%x]\n",
				device, func);
			return (PCICFG_FAILURE);
		}
	}

	return (PCICFG_SUCCESS);
}

static int
pcicfg_teardown_device(dev_info_t *dip)
{
	/*
	 * Free up resources associated with 'dip'
	 */

	if (pcicfg_free_resources(dip) != PCICFG_SUCCESS) {
		DEBUG0("Failed to free resources\n");
		return (PCICFG_FAILURE);
	}

	/*
	 * The framework provides this routine which can
	 * tear down a sub-tree.
	 */
	if (ndi_devi_offline(dip, NDI_DEVI_REMOVE) != NDI_SUCCESS) {
		DEBUG0("Failed to offline and remove node\n");
		return (PCICFG_FAILURE);
	}

	return (PCICFG_SUCCESS);
}

/*
 * BEGIN GENERIC SUPPORT ROUTINES
 */
static pcicfg_phdl_t *
pcicfg_find_phdl(dev_info_t *dip)
{
	pcicfg_phdl_t *entry;
	mutex_enter(&pcicfg_list_mutex);
	for (entry = pcicfg_phdl_list; entry != NULL; entry = entry->next) {
		if (entry->dip == dip) {
			mutex_exit(&pcicfg_list_mutex);
			return (entry);
		}
	}
	mutex_exit(&pcicfg_list_mutex);

	/*
	 * Did'nt find entry - create one
	 */
	return (pcicfg_create_phdl(dip));
}

static pcicfg_phdl_t *
pcicfg_create_phdl(dev_info_t *dip)
{
	pcicfg_phdl_t *new;

	new = (pcicfg_phdl_t *)kmem_zalloc(sizeof (pcicfg_phdl_t),
		KM_SLEEP);

	new->dip = dip;
	mutex_enter(&pcicfg_list_mutex);
	new->next = pcicfg_phdl_list;
	pcicfg_phdl_list = new;
	mutex_exit(&pcicfg_list_mutex);

	return (new);
}

static int
pcicfg_destroy_phdl(dev_info_t *dip)
{
	pcicfg_phdl_t *entry;
	pcicfg_phdl_t *follow = NULL;

	mutex_enter(&pcicfg_list_mutex);
	for (entry = pcicfg_phdl_list; entry != NULL; follow = entry,
		entry = entry->next) {
		if (entry->dip == dip) {
			if (entry == pcicfg_phdl_list) {
				pcicfg_phdl_list = entry->next;
			} else {
				follow->next = entry->next;
			}
			/*
			 * If this entry has any allocated memory
			 * or IO space associated with it, that
			 * must be freed up.
			 */
			if (entry->memory_len > 0) {
				(void) ndi_ra_free(ddi_get_parent(dip),
					entry->memory_base,
					entry->memory_len,
					NDI_RA_TYPE_MEM, NDI_RA_PASS);
			}
			if (entry->io_len > 0) {
				(void) ndi_ra_free(ddi_get_parent(dip),
					entry->io_base,
					entry->io_len,
					NDI_RA_TYPE_IO, NDI_RA_PASS);
			}
			/*
			 * Destroy this entry
			 */
			kmem_free((caddr_t)entry, sizeof (pcicfg_phdl_t));
			mutex_exit(&pcicfg_list_mutex);
			return (PCICFG_SUCCESS);
		}
	}
	mutex_exit(&pcicfg_list_mutex);
	/*
	 * Did'nt find the entry
	 */
	return (PCICFG_FAILURE);
}

static int
pcicfg_program_ap(dev_info_t *dip)
{
	pcicfg_phdl_t *phdl;
	uint8_t header_type;
	ddi_acc_handle_t handle;
	pcicfg_phdl_t *entry;

	if (pcicfg_config_setup(dip, &handle) != DDI_SUCCESS) {
		DEBUG0("Failed to map config space!\n");
		return (PCICFG_FAILURE);

	}

	header_type = pci_config_get8(handle, PCI_CONF_HEADER);

	(void) pcicfg_config_teardown(&handle);

	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_PPB) {

		if (pcicfg_allocate_chunk(dip) != PCICFG_SUCCESS) {
			DEBUG0("Not enough memory to hotplug\n");
			(void) pcicfg_destroy_phdl(dip);
			return (PCICFG_FAILURE);
		}

		phdl = pcicfg_find_phdl(dip);
		ASSERT(phdl);

		(void) pcicfg_bridge_assign(dip, (void *)phdl);

		if (phdl->error != PCICFG_SUCCESS) {
			DEBUG0("Problem assigning bridge\n");
			(void) pcicfg_destroy_phdl(dip);
			return (phdl->error);
		}

		/*
		 * Successfully allocated and assigned
		 * memory.  Set the memory and IO length
		 * to zero so when the handle is freed up
		 * it will not de-allocate assigned resources.
		 */
		entry = (pcicfg_phdl_t *)phdl;

		entry->memory_len = entry->io_len = 0;

		/*
		 * Free up the "entry" structure.
		 */
		(void) pcicfg_destroy_phdl(dip);
	} else {
		if (pcicfg_device_assign(dip) != PCICFG_SUCCESS) {
			return (PCICFG_FAILURE);
		}
	}
	return (PCICFG_SUCCESS);
}
static int
pcicfg_bridge_assign(dev_info_t *dip, void *hdl)
{
	ddi_acc_handle_t handle;
	pci_regspec_t *reg;
	int length;
	int rcount;
	int i;
	int offset;
	uint64_t mem_answer;
	uint32_t io_answer;
	uint_t count;
	uint8_t header_type;
	pcicfg_range_t range[PCICFG_RANGE_LEN];
	int bus_range[2];

	pcicfg_phdl_t *entry = (pcicfg_phdl_t *)hdl;

	entry->error = PCICFG_SUCCESS;

	if (entry == NULL) {
		DEBUG0("Failed to get entry\n");
		entry->error = PCICFG_FAILURE;
		return (DDI_WALK_TERMINATE);
	}

	if (pcicfg_config_setup(dip, &handle) != DDI_SUCCESS) {
		DEBUG0("Failed to map config space!\n");
		entry->error = PCICFG_FAILURE;
		return (DDI_WALK_TERMINATE);
	}

	header_type = pci_config_get8(handle, PCI_CONF_HEADER);

	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_PPB) {

		bzero((caddr_t)range,
			sizeof (pcicfg_range_t) * PCICFG_RANGE_LEN);

		(void) pcicfg_setup_bridge(entry, handle);

		range[0].child_hi = range[0].parent_hi |=
			(PCI_REG_REL_M | PCI_ADDR_IO);
		range[0].child_lo = range[0].parent_lo =
			entry->io_last;
		range[1].child_hi = range[1].parent_hi |=
			(PCI_REG_REL_M | PCI_ADDR_MEM32);
		range[1].child_lo = range[1].parent_lo =
			entry->memory_last;

		i_ndi_block_device_tree_changes(&count);
		ddi_walk_devs(ddi_get_child(dip),
			pcicfg_bridge_assign, (void *)entry);
		i_ndi_allow_device_tree_changes(count);

		(void) pcicfg_update_bridge(entry, handle);

		bus_range[0] = pci_config_get8(handle, PCI_BCNF_SECBUS);
		bus_range[1] = pci_config_get8(handle, PCI_BCNF_SUBBUS);

		if (ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
				"bus-range", bus_range, 2) != DDI_SUCCESS) {
			DEBUG0("Failed to set bus-range property");
			entry->error = PCICFG_FAILURE;
			return (DDI_WALK_TERMINATE);
		}

		if (entry->io_len > 0) {
			range[0].size_lo = entry->io_last - entry->io_base;
			if (pcicfg_update_ranges_prop(dip, &range[0])) {
				DEBUG0("Failed to update ranges (i/o)\n");
				entry->error = PCICFG_FAILURE;
				return (DDI_WALK_TERMINATE);
			}
		}
		if (entry->memory_len > 0) {
			range[1].size_lo =
				entry->memory_last - entry->memory_base;
			if (pcicfg_update_ranges_prop(dip, &range[1])) {
				DEBUG0("Failed to update ranges (memory)\n");
				entry->error = PCICFG_FAILURE;
				return (DDI_WALK_TERMINATE);
			}
		}

		(void) pcicfg_device_on(handle);
#if 0
		pcicfg_dump_bridge_config(handle);
#endif
		return (DDI_WALK_PRUNECHILD);
	}

	/*
	 * If there is an interrupt pin set program
	 * interrupt line with default values.
	 */
	if (pci_config_get8(handle, PCI_CONF_IPIN)) {
		pci_config_put8(handle, PCI_CONF_ILINE, 0xf);
	}

	/*
	 * A single device (under a bridge).
	 * For each "reg" property with a length, allocate memory
	 * and program the base registers.
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "reg", (caddr_t)&reg,
		&length) != DDI_PROP_SUCCESS) {
		DEBUG0("Failed to read reg property\n");
		entry->error = PCICFG_FAILURE;
		return (DDI_WALK_TERMINATE);
	}

	rcount = length / sizeof (pci_regspec_t);
	offset = PCI_CONF_BASE0;
	for (i = 0; i < rcount; i++) {
		if ((reg[i].pci_size_low != 0)||
			(reg[i].pci_size_hi != 0)) {

			switch (PCI_REG_ADDR_G(reg[i].pci_phys_hi)) {
			case PCI_REG_ADDR_G(PCI_ADDR_MEM64):

				(void) pcicfg_get_mem(entry,
				reg[i].pci_size_low, &mem_answer);
				pci_config_put64(handle, offset, mem_answer);
				DEBUG1("REGISTER (64)LO ----> [0x%x]\n",
					pci_config_get32(handle, offset));
				DEBUG1("REGISTER (64)HI ----> [0x%x]\n",
					pci_config_get32(handle, offset + 4));

				reg[i].pci_phys_low = PCICFG_HIADDR(mem_answer);
				reg[i].pci_phys_mid  =
					PCICFG_LOADDR(mem_answer);

				offset += 8;
				break;

			case PCI_REG_ADDR_G(PCI_ADDR_MEM32):
				/* allocate memory space from the allocator */

				(void) pcicfg_get_mem(entry,
					reg[i].pci_size_low, &mem_answer);
				pci_config_put32(handle,
					offset, (uint32_t)mem_answer);

				DEBUG1("REGISTER (32)LO ----> [0x%x]\n",
					pci_config_get32(handle, offset));

				reg[i].pci_phys_low = (uint32_t)mem_answer;

				offset += 4;
				break;
			case PCI_REG_ADDR_G(PCI_ADDR_IO):
				/* allocate I/O space from the allocator */

				(void) pcicfg_get_io(entry,
					reg[i].pci_size_low, &io_answer);
				pci_config_put32(handle, offset, io_answer);

				DEBUG1("REGISTER (I/O)LO ----> [0x%x]\n",
					pci_config_get32(handle, offset));

				reg[i].pci_phys_low = io_answer;

				offset += 4;
				break;
			default:
				DEBUG0("Unknown register type\n");
				kmem_free(reg, length);
				(void) pcicfg_config_teardown(&handle);
				entry->error = PCICFG_FAILURE;
				return (DDI_WALK_TERMINATE);
			} /* switch */

			/*
			 * Now that memory locations are assigned,
			 * update the assigned address property.
			 */
			if (pcicfg_update_assigned_prop(dip,
				&reg[i]) != PCICFG_SUCCESS) {
				kmem_free(reg, length);
				(void) pcicfg_config_teardown(&handle);
				entry->error = PCICFG_FAILURE;
				return (DDI_WALK_TERMINATE);
			}
		}
	}
	(void) pcicfg_device_on(handle);
#if DEBUG
	(void) pcicfg_dump_device_config(handle);
#endif
	(void) pcicfg_config_teardown(&handle);
	return (DDI_WALK_CONTINUE);
}

static int
pcicfg_device_assign(dev_info_t *dip)
{
	ddi_acc_handle_t	handle;
	pci_regspec_t		*reg;
	int			length;
	int			rcount;
	int			i;
	int			offset;
	ndi_ra_request_t	request;
	uint64_t		answer;
	uint64_t		alen;


	/*
	 * XXX Failure here should be noted
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "reg", (caddr_t)&reg,
		&length) != DDI_PROP_SUCCESS) {
		DEBUG0("Failed to read reg property\n");
		return (PCICFG_FAILURE);
	}

	if (pcicfg_config_setup(dip, &handle) != DDI_SUCCESS) {
		DEBUG0("Failed to map config space!\n");
		return (PCICFG_FAILURE);
	}

	/*
	 * A single device
	 *
	 * For each "reg" property with a length, allocate memory
	 * and program the base registers.
	 */

	/*
	 * If there is an interrupt pin set program
	 * interrupt line with default values.
	 */
	if (pci_config_get8(handle, PCI_CONF_IPIN)) {
		pci_config_put8(handle, PCI_CONF_ILINE, 0xf);
	}

	bzero((caddr_t)&request, sizeof (ndi_ra_request_t));

	request.ra_flags |= NDI_RA_ALIGN_SIZE;
	request.ra_boundbase = 0;
	request.ra_boundlen = PCICFG_4GIG_LIMIT;
	rcount = length / sizeof (pci_regspec_t);
	offset = PCI_CONF_BASE0;
	for (i = 0; i < rcount; i++) {
		if ((reg[i].pci_size_low != 0)||
			(reg[i].pci_size_hi != 0)) {

			if (PCI_REG_REG_G(reg[i].pci_phys_hi) == PCI_CONF_ROM) {
				break; /* if */
			}
			request.ra_len = reg[i].pci_size_low;

			switch (PCI_REG_ADDR_G(reg[i].pci_phys_hi)) {
			case PCI_REG_ADDR_G(PCI_ADDR_MEM64):
				request.ra_flags ^= NDI_RA_ALLOC_BOUNDED;
				/* allocate memory space from the allocator */
				if (ndi_ra_alloc(ddi_get_parent(dip),
					&request, &answer, &alen,
					NDI_RA_TYPE_MEM, NDI_RA_PASS)
							!= NDI_SUCCESS) {
					DEBUG0("Failed to allocate 64b mem\n");
					kmem_free(reg, length);
					(void) pcicfg_config_teardown(&handle);
					return (PCICFG_FAILURE);
				}
				DEBUG3("64 addr = [0x%x.%x] len [0x%x]\n",
					PCICFG_HIADDR(answer),
					PCICFG_LOADDR(answer),
					alen);
				/* program the low word */
				pci_config_put32(handle,
					offset, PCICFG_LOADDR(answer));

				/* program the high word with value zero */
				pci_config_put32(handle, offset + 4,
					PCICFG_HIADDR(answer));

				reg[i].pci_phys_low = PCICFG_LOADDR(answer);
				reg[i].pci_phys_mid = PCICFG_HIADDR(answer);

				offset += 8;
				break;

			case PCI_REG_ADDR_G(PCI_ADDR_MEM32):
				request.ra_flags |= NDI_RA_ALLOC_BOUNDED;
				/* allocate memory space from the allocator */
				if (ndi_ra_alloc(ddi_get_parent(dip),
					&request, &answer, &alen,
					NDI_RA_TYPE_MEM, NDI_RA_PASS)
							!= NDI_SUCCESS) {
					DEBUG0("Failed to allocate 32b mem\n");
					kmem_free(reg, length);
					(void) pcicfg_config_teardown(&handle);
					return (PCICFG_FAILURE);
				}
				DEBUG3("32 addr = [0x%x.%x] len [0x%x]\n",
					PCICFG_HIADDR(answer),
					PCICFG_LOADDR(answer),
					alen);
				/* program the low word */
				pci_config_put32(handle,
					offset, PCICFG_LOADDR(answer));

				reg[i].pci_phys_low = PCICFG_LOADDR(answer);

				offset += 4;
				break;
			case PCI_REG_ADDR_G(PCI_ADDR_IO):
				/* allocate I/O space from the allocator */
				request.ra_flags |= NDI_RA_ALLOC_BOUNDED;
				if (ndi_ra_alloc(ddi_get_parent(dip),
					&request, &answer, &alen,
					NDI_RA_TYPE_IO, NDI_RA_PASS)
							!= NDI_SUCCESS) {
					DEBUG0("Failed to allocate I/O\n");
					kmem_free(reg, length);
					(void) pcicfg_config_teardown(&handle);
					return (PCICFG_FAILURE);
				}
				DEBUG3("I/O addr = [0x%x.%x] len [0x%x]\n",
					PCICFG_HIADDR(answer),
					PCICFG_LOADDR(answer),
					alen);
				pci_config_put32(handle,
					offset, PCICFG_LOADDR(answer));

				reg[i].pci_phys_low = PCICFG_LOADDR(answer);

				offset += 4;
				break;
			default:
				DEBUG0("Unknown register type\n");
				kmem_free(reg, length);
				(void) pcicfg_config_teardown(&handle);
				return (PCICFG_FAILURE);
			} /* switch */

			/*
			 * Now that memory locations are assigned,
			 * update the assigned address property.
			 */

			if (pcicfg_update_assigned_prop(dip,
				&reg[i]) != PCICFG_SUCCESS) {
				kmem_free(reg, length);
				(void) pcicfg_config_teardown(&handle);
				return (PCICFG_FAILURE);
			}
		}
	}

	(void) pcicfg_device_on(handle);
	kmem_free(reg, length);
#if DEBUG
	(void) pcicfg_dump_device_config(handle);
#endif
	(void) pcicfg_config_teardown(&handle);
	return (PCICFG_SUCCESS);
}

/*
 * The "dip" passed to this routine is assumed to be
 * the device at the attachment point. Currently it is
 * assumed to be a bridge.
 */
static int
pcicfg_allocate_chunk(dev_info_t *dip)
{
	pcicfg_phdl_t		*phdl;
	ndi_ra_request_t	*mem_request;
	ndi_ra_request_t	*io_request;
	uint64_t		mem_answer;
	uint64_t		io_answer;
	uint_t			count;
	uint64_t		alen;

	/*
	 * This should not find an existing entry - so
	 * it will create a new one.
	 */
	phdl = pcicfg_find_phdl(dip);
	ASSERT(phdl);

	mem_request = &phdl->mem_req;
	io_request  = &phdl->io_req;

	/*
	 * From this point in the tree - walk the devices,
	 * The function passed in will read and "sum" up
	 * the memory and I/O requirements and put them in
	 * structure "phdl".
	 */
	i_ndi_block_device_tree_changes(&count);
	ddi_walk_devs(dip, pcicfg_sum_resources, (void *)phdl);
	i_ndi_allow_device_tree_changes(count);

	if (phdl->error != PCICFG_SUCCESS) {
		DEBUG0("Failure summing resources\n");
		return (phdl->error);
	}

	/*
	 * Call into the memory allocator with the request.
	 * Record the addresses returned in the phdl
	 */
	DEBUG1("AP requires [0x%x] bytes of memory space\n",
		mem_request->ra_len);
	DEBUG1("AP requires [0x%x] bytes of I/O    space\n",
		io_request->ra_len);

	mem_request->ra_align_mask =
		PCICFG_MEMGRAN - 1; /* 1M alignment on memory space */
	io_request->ra_align_mask =
		PCICFG_IOGRAN - 1;   /* 4K alignment on I/O space */
	io_request->ra_boundbase = 0;
	io_request->ra_boundlen = PCICFG_4GIG_LIMIT;
	io_request->ra_flags |= NDI_RA_ALLOC_BOUNDED;

	mem_request->ra_len =
		PCICFG_ROUND_UP(mem_request->ra_len, PCICFG_MEMGRAN);

	io_request->ra_len =
		PCICFG_ROUND_UP(io_request->ra_len, PCICFG_IOGRAN);

	if (ndi_ra_alloc(ddi_get_parent(dip),
	    mem_request, &mem_answer, &alen,
		NDI_RA_TYPE_MEM, NDI_RA_PASS) != NDI_SUCCESS) {
		DEBUG0("Failed to allocate memory\n");
		return (PCICFG_FAILURE);
	}

	phdl->memory_base = phdl->memory_last = mem_answer;
	phdl->memory_len  = alen;

	if (ndi_ra_alloc(ddi_get_parent(dip), io_request, &io_answer,
		&alen, NDI_RA_TYPE_IO, NDI_RA_PASS) != NDI_SUCCESS) {
		DEBUG0("Failed to allocate I/O space\n");
		(void) ndi_ra_free(ddi_get_parent(dip), mem_answer,
			alen, NDI_RA_TYPE_MEM, NDI_RA_PASS);
		phdl->memory_len = phdl->io_len = 0;
		return (PCICFG_FAILURE);
	}

	phdl->io_base = phdl->io_last = (uint32_t)io_answer;
	phdl->io_len  = (uint32_t)alen;

	DEBUG2("MEMORY BASE = [0x%x] length [0x%x]\n",
		phdl->memory_base, phdl->memory_len);
	DEBUG2("IO     BASE = [0x%x] length [0x%x]\n",
		phdl->io_base, phdl->io_len);

	return (PCICFG_SUCCESS);
}

static void
pcicfg_get_mem(pcicfg_phdl_t *entry,
	uint32_t length, uint64_t *ans)
{
	/*
	 * Round up the request to the "size" boundary
	 */
	entry->memory_last =
		PCICFG_ROUND_UP(entry->memory_last, length);

	/*
	 * These routines should parcel out the memory
	 * completely.  There should never be a case of
	 * over running the bounds.
	 */
	ASSERT((entry->memory_last + length) <=
		(entry->memory_base + entry->memory_len));

	/*
	 * If ans is NULL don't return anything,
	 * they are just asking to reserve the memory.
	 */
	if (ans != NULL)
		*ans = entry->memory_last;

	/*
	 * Increment to the next location
	 */
	entry->memory_last += length;
}

static void
pcicfg_get_io(pcicfg_phdl_t *entry,
	uint32_t length, uint32_t *ans)
{
	/*
	 * Round up the request to the "size" boundary
	 */
	entry->io_last =
		PCICFG_ROUND_UP(entry->io_last, length);

	/*
	 * These routines should parcel out the memory
	 * completely.  There should never be a case of
	 * over running the bounds.
	 */
	ASSERT((entry->io_last + length) <=
		(entry->io_base + entry->io_len));

	/*
	 * If ans is NULL don't return anything,
	 * they are just asking to reserve the memory.
	 */
	if (ans != NULL)
		*ans = entry->io_last;

	/*
	 * Increment to the next location
	 */
	entry->io_last += length;
}

static int
pcicfg_sum_resources(dev_info_t *dip, void *hdl)
{
	pcicfg_phdl_t *entry = (pcicfg_phdl_t *)hdl;
	pci_regspec_t *pci_rp;
	int length;
	int rcount;
	int i;
	ndi_ra_request_t *mem_request;
	ndi_ra_request_t *io_request;
	uint8_t header_type;
	ddi_acc_handle_t handle;

	entry->error = PCICFG_SUCCESS;

	mem_request = &entry->mem_req;
	io_request =  &entry->io_req;

	if (pcicfg_config_setup(dip, &handle) != DDI_SUCCESS) {
		DEBUG0("Failed to map config space!\n");
		entry->error = PCICFG_FAILURE;
		return (DDI_WALK_TERMINATE);
	}

	header_type = pci_config_get8(handle, PCI_CONF_HEADER);

	/*
	 * If its a bridge - just record the highest bus seen
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_PPB) {

		if (entry->highest_bus < pci_config_get8(handle,
			PCI_BCNF_SECBUS)) {
			entry->highest_bus =
				pci_config_get8(handle, PCI_BCNF_SECBUS);
		}

		(void) pcicfg_config_teardown(&handle);
		entry->error = PCICFG_FAILURE;
		return (DDI_WALK_CONTINUE);
	} else {
		if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "reg", (caddr_t)&pci_rp,
			&length) != DDI_PROP_SUCCESS) {
			/*
			 * If one node in (the subtree of nodes)
			 * does'nt have a "reg" property fail the
			 * allocation.
			 */
			entry->memory_len = 0;
			entry->io_len = 0;
			entry->error = PCICFG_FAILURE;
			return (DDI_WALK_TERMINATE);
		}
		/*
		 * For each "reg" property with a length, add that to the
		 * total memory (or I/O) to allocate.
		 */
		rcount = length / sizeof (pci_regspec_t);

		for (i = 0; i < rcount; i++) {

			switch (PCI_REG_ADDR_G(pci_rp[i].pci_phys_hi)) {

			case PCI_REG_ADDR_G(PCI_ADDR_MEM32):
				mem_request->ra_len =
				pci_rp[i].pci_size_low +
				PCICFG_ROUND_UP(mem_request->ra_len,
				pci_rp[i].pci_size_low);
				DEBUG1("ADDING 32 --->0x%x\n",
					pci_rp[i].pci_size_low);

			break;
			case PCI_REG_ADDR_G(PCI_ADDR_MEM64):
				mem_request->ra_len =
				pci_rp[i].pci_size_low +
				PCICFG_ROUND_UP(mem_request->ra_len,
				pci_rp[i].pci_size_low);
				DEBUG1("ADDING 64 --->0x%x\n",
					pci_rp[i].pci_size_low);

			break;
			case PCI_REG_ADDR_G(PCI_ADDR_IO):
				io_request->ra_len =
				pci_rp[i].pci_size_low +
				PCICFG_ROUND_UP(io_request->ra_len,
				pci_rp[i].pci_size_low);
				DEBUG1("ADDING I/O --->0x%x\n",
					pci_rp[i].pci_size_low);
			break;
			default:
			    /* Config space register - not included */
			break;
			}
		}

		/*
		 * free the memory allocated by ddi_getlongprop
		 */
		kmem_free(pci_rp, length);

		/*
		 * continue the walk to the next sibling to sum memory
		 */

		(void) pcicfg_config_teardown(&handle);

		return (DDI_WALK_CONTINUE);
	}
}

static int
pcicfg_free_bridge_resources(dev_info_t *dip)
{
	pcicfg_range_t		*ranges;
	uint_t			*bus;
	int			k;
	int			length;
	int			i;


	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "ranges", (caddr_t)&ranges,
		&length) != DDI_PROP_SUCCESS) {
		DEBUG0("Failed to read ranges property\n");
		return (PCICFG_FAILURE);
	}

	for (i = 0; i < length / sizeof (pcicfg_range_t); i++) {
		if (ranges[i].size_lo != 0 ||
			ranges[i].size_hi != 0) {
			switch (ranges[i].parent_hi & PCI_REG_ADDR_M) {
				case PCI_ADDR_IO:
					DEBUG2("Free I/O    "
					"base/length = [0x%x]/[0x%x]\n",
						ranges[i].child_lo,
						ranges[i].size_lo);
					if (ndi_ra_free(ddi_get_parent(dip),
						(uint64_t)ranges[i].child_lo,
						(uint64_t)ranges[i].size_lo,
						NDI_RA_TYPE_IO, NDI_RA_PASS)
						!= NDI_SUCCESS) {
						DEBUG0("Trouble freeing "
						"PCI i/o space\n");
						kmem_free(ranges, length);
						return (PCICFG_FAILURE);
					}
				break;
				case PCI_ADDR_MEM32:
				case PCI_ADDR_MEM64:
					DEBUG3("Free Memory base/length = "
					"[0x%x.%x]/[0x%x]\n",
						ranges[i].child_mid,
						ranges[i].child_lo,
						ranges[i].size_lo)
					if (ndi_ra_free(ddi_get_parent(dip),
						PCICFG_LADDR(ranges[i].child_lo,
						ranges[i].child_mid),
						(uint64_t)ranges[i].size_lo,
						NDI_RA_TYPE_MEM, NDI_RA_PASS)
						!= NDI_SUCCESS) {
						DEBUG0("Trouble freeing "
						"PCI memory space\n");
						kmem_free(ranges, length);
						return (PCICFG_FAILURE);
					}
				break;
				default:
					DEBUG0("Unknown memory space\n");
				break;
			}
		}
	}

	kmem_free(ranges, length);

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "bus-range", (caddr_t)&bus,
		&k) != DDI_PROP_SUCCESS) {
		DEBUG0("Failed to read bus-range property\n");
		return (PCICFG_FAILURE);
	}

	DEBUG2("Need to free bus [%d] range [%d]\n",
		bus[0], bus[1] - bus[0] + 1);

	if (ndi_ra_free(ddi_get_parent(dip),
		(uint64_t)bus[0], (uint64_t)(bus[1] - bus[0] + 1),
		NDI_RA_TYPE_PCI_BUSNUM, NDI_RA_PASS) != NDI_SUCCESS) {
		DEBUG0("Failed to free a bus number\n");
		return (PCICFG_FAILURE);
	}
	return (PCICFG_SUCCESS);
}

static int
pcicfg_free_device_resources(dev_info_t *dip)
{
	pci_regspec_t *assigned;

	int length;
	int acount;
	int i;

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "assigned-addresses", (caddr_t)&assigned,
		&length) != DDI_PROP_SUCCESS) {
		DEBUG0("Failed to read assigned-addresses property\n");
		return (PCICFG_FAILURE);
	}

	/*
	 * For each "assigned-addresses" property entry with a length,
	 * call the memory allocation routines to return the
	 * resource.
	 */
	acount = length / sizeof (pci_regspec_t);
	for (i = 0; i < acount; i++) {
		/*
		 * Workaround for Devconf (x86) bug to skip extra entries
		 * beyond the PCI_CONF_BASE5 offset.
		 */
		if (PCI_REG_REG_G(assigned[i].pci_phys_hi) > PCI_CONF_BASE5)
			break;

		/*
		 * Free the resource if the size of it is not zero.
		 */
		if ((assigned[i].pci_size_low != 0)||
			(assigned[i].pci_size_hi != 0)) {
			switch (PCI_REG_ADDR_G(assigned[i].pci_phys_hi)) {
			case PCI_REG_ADDR_G(PCI_ADDR_MEM32):
				if (ndi_ra_free(ddi_get_parent(dip),
				(uint64_t)assigned[i].pci_phys_low,
				(uint64_t)assigned[i].pci_size_low,
				NDI_RA_TYPE_MEM, NDI_RA_PASS) != NDI_SUCCESS) {
				DEBUG0("Trouble freeing "
				"PCI memory space\n");
				return (PCICFG_FAILURE);
				}

				DEBUG3("Returned 0x%x of 32 bit MEM space"
				" @ 0x%x from register 0x%x\n",
					assigned[i].pci_size_low,
					assigned[i].pci_phys_low,
					PCI_REG_REG_G(assigned[i].pci_phys_hi));

			break;
			case PCI_REG_ADDR_G(PCI_ADDR_MEM64):
				if (ndi_ra_free(ddi_get_parent(dip),
				PCICFG_LADDR(assigned[i].pci_phys_low,
				assigned[i].pci_phys_mid),
				(uint64_t)assigned[i].pci_size_low,
				NDI_RA_TYPE_MEM, NDI_RA_PASS) != NDI_SUCCESS) {
				DEBUG0("Trouble freeing "
				"PCI memory space\n");
				return (PCICFG_FAILURE);
				}

				DEBUG4("Returned 0x%x of 64 bit MEM space"
				" @ 0x%x.%x from register 0x%x\n",
					assigned[i].pci_size_low,
					assigned[i].pci_phys_mid,
					assigned[i].pci_phys_low,
					PCI_REG_REG_G(assigned[i].pci_phys_hi));

			break;
			case PCI_REG_ADDR_G(PCI_ADDR_IO):
				if (ndi_ra_free(ddi_get_parent(dip),
				(uint64_t)assigned[i].pci_phys_low,
				(uint64_t)assigned[i].pci_size_low,
				NDI_RA_TYPE_IO, NDI_RA_PASS) != NDI_SUCCESS) {
				DEBUG0("Trouble freeing "
				"PCI IO space\n");
				return (PCICFG_FAILURE);
				}
				DEBUG3("Returned 0x%x of IO space @ 0x%x"
				" from register 0x%x\n",
					assigned[i].pci_size_low,
					assigned[i].pci_phys_low,
					PCI_REG_REG_G(assigned[i].pci_phys_hi));
			break;
			default:
				DEBUG0("Unknown register type\n");
				kmem_free(assigned, length);
				return (PCICFG_FAILURE);
			} /* switch */
		}
	}
	kmem_free(assigned, length);
	return (PCICFG_SUCCESS);
}

static int
pcicfg_free_resources(dev_info_t *dip)
{
	ddi_acc_handle_t handle;
	uint8_t header_type;

	if (pci_config_setup(dip, &handle) != DDI_SUCCESS) {
		DEBUG0("Failed to map config space!\n");
		return (PCICFG_FAILURE);
	}

	header_type = pci_config_get8(handle, PCI_CONF_HEADER);

	(void) pci_config_teardown(&handle);

	/*
	 * A different algorithim is used for bridges and leaf devices.
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_PPB) {
		if (pcicfg_free_bridge_resources(dip) != PCICFG_SUCCESS) {
			DEBUG0("Failed freeing up bridge resources\n");
			return (PCICFG_FAILURE);
		}
	} else {
		if (pcicfg_free_device_resources(dip) != PCICFG_SUCCESS) {
			DEBUG0("Failed freeing up device resources\n");
			return (PCICFG_FAILURE);
		}
	}
	return (PCICFG_SUCCESS);
}

#ifndef _DONT_USE_1275_GENERIC_NAMES
static char *
pcicfg_get_class_name(uint32_t classcode)
{
	struct pcicfg_name_entry *ptr;

	for (ptr = &pcicfg_class_lookup[0]; ptr->name != NULL; ptr++) {
		if (ptr->class_code == classcode) {
			return (ptr->name);
		}
	}
	return (NULL);
}
#endif /* _DONT_USE_1275_GENERIC_NAMES */

static dev_info_t *
pcicfg_devi_find(dev_info_t *dip, uint_t device, uint_t function)
{
	struct pcicfg_find_ctrl ctrl;
	uint_t count;

	ctrl.device = device;
	ctrl.function = function;
	ctrl.dip = NULL;

	i_ndi_block_device_tree_changes(&count);
	ddi_walk_devs(ddi_get_child(dip), pcicfg_match_dev, (void *)&ctrl);
	i_ndi_allow_device_tree_changes(count);

	return (ctrl.dip);
}

static int
pcicfg_match_dev(dev_info_t *dip, void *hdl)
{
	struct pcicfg_find_ctrl *ctrl = (struct pcicfg_find_ctrl *)hdl;
	pci_regspec_t *pci_rp;
	int length;
	int pci_dev;
	int pci_func;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		(uint_t *)&length) != DDI_PROP_SUCCESS) {
		ctrl->dip = NULL;
		return (DDI_WALK_TERMINATE);
	}

	/* get the PCI device address info */
	pci_dev = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	pci_func = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	ddi_prop_free(pci_rp);


	if ((pci_dev == ctrl->device) && (pci_func == ctrl->function)) {
		/* found the match for the specified device address */
		ctrl->dip = dip;
		return (DDI_WALK_TERMINATE);
	}

	/*
	 * continue the walk to the next sibling to look for a match.
	 */
	return (DDI_WALK_PRUNECHILD);
}

static int
pcicfg_update_assigned_prop(dev_info_t *dip, pci_regspec_t *newone)
{
	int		alen;
	pci_regspec_t	*assigned;
	caddr_t		newreg;
	uint_t		status;

	status = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
		"assigned-addresses", (caddr_t)&assigned, &alen);
	switch (status) {
		case DDI_PROP_SUCCESS:
		break;
		case DDI_PROP_NO_MEMORY:
			DEBUG0("no memory for assigned-addresses property\n");
			return (PCICFG_FAILURE);
		default:
			(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
			"assigned-addresses", (int *)newone,
				sizeof (*newone)/sizeof (int));
			return (PCICFG_SUCCESS);
	}

	/*
	 * Allocate memory for the existing
	 * assigned-addresses(s) plus one and then
	 * build it.
	 */

	newreg = kmem_zalloc(alen+sizeof (*newone), KM_SLEEP);

	bcopy(assigned, newreg, alen);
	bcopy(newone, newreg + alen, sizeof (*newone));

	/*
	 * Write out the new "assinged-addresses" spec
	 */
	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
		"assigned-addresses", (int *)newreg,
		(alen + sizeof (*newone))/sizeof (int));

	kmem_free((caddr_t)newreg, alen+sizeof (*newone));

	return (PCICFG_SUCCESS);
}
static int
pcicfg_update_ranges_prop(dev_info_t *dip, pcicfg_range_t *addition)
{
	int		rlen;
	pcicfg_range_t	*ranges;
	caddr_t		newreg;
	uint_t		status;

	status = ddi_getlongprop(DDI_DEV_T_NONE,
		dip, DDI_PROP_DONTPASS, "ranges", (caddr_t)&ranges, &rlen);


	switch (status) {
		case DDI_PROP_SUCCESS:
		break;
		case DDI_PROP_NO_MEMORY:
			DEBUG0("ranges present, but unable to get memory\n");
			return (PCICFG_FAILURE);
		default:
			DEBUG0("no ranges property - creating one\n");
			if (ndi_prop_update_int_array(DDI_DEV_T_NONE,
				dip, "ranges", (int *)addition,
				sizeof (pcicfg_range_t)/sizeof (int))
				!= DDI_SUCCESS) {
				DEBUG0("Did'nt create ranges property\n");
				return (PCICFG_FAILURE);
			}
			return (PCICFG_SUCCESS);
	}

	/*
	 * Allocate memory for the existing reg(s) plus one and then
	 * build it.
	 */
	newreg = kmem_zalloc(rlen+sizeof (pcicfg_range_t), KM_SLEEP);

	bcopy(ranges, newreg, rlen);
	bcopy(addition, newreg + rlen, sizeof (pcicfg_range_t));

	/*
	 * Write out the new "ranges" property
	 */
	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE,
		dip, "ranges", (int *)newreg,
		(rlen + sizeof (pcicfg_range_t))/sizeof (int));

	kmem_free((caddr_t)newreg, rlen+sizeof (pcicfg_range_t));

	kmem_free((caddr_t)ranges, rlen);

	return (PCICFG_SUCCESS);
}

static int
pcicfg_update_reg_prop(dev_info_t *dip, uint32_t regvalue, uint_t reg_offset)
{
	int		rlen;
	pci_regspec_t	*reg;
	caddr_t		newreg;
	uint32_t	hiword;
	pci_regspec_t	addition;
	uint32_t	size;
	uint_t		status;

	status = ddi_getlongprop(DDI_DEV_T_NONE,
		dip, DDI_PROP_DONTPASS, "reg", (caddr_t)&reg, &rlen);

	switch (status) {
		case DDI_PROP_SUCCESS:
		break;
		case DDI_PROP_NO_MEMORY:
			DEBUG0("reg present, but unable to get memory\n");
			return (PCICFG_FAILURE);
		default:
			DEBUG0("no reg property\n");
			return (PCICFG_FAILURE);
	}

	/*
	 * Allocate memory for the existing reg(s) plus one and then
	 * build it.
	 */
	newreg = kmem_zalloc(rlen+sizeof (pci_regspec_t), KM_SLEEP);

	/*
	 * Build the regspec, then add it to the existing one(s)
	 */

	hiword = PCICFG_MAKE_REG_HIGH(PCI_REG_BUS_G(reg->pci_phys_hi),
	    PCI_REG_DEV_G(reg->pci_phys_hi),
	    PCI_REG_FUNC_G(reg->pci_phys_hi), reg_offset);

	if (reg_offset == PCI_CONF_ROM) {
		size = (~(PCI_BASE_ROM_ADDR_M & regvalue))+1;
		hiword |= PCI_ADDR_MEM32;
	} else {
		size = (~(PCI_BASE_M_ADDR_M & regvalue))+1;

		if ((PCI_BASE_SPACE_M & regvalue) == PCI_BASE_SPACE_MEM) {
			if ((PCI_BASE_TYPE_M & regvalue) == PCI_BASE_TYPE_MEM) {
				hiword |= PCI_ADDR_MEM32;
			} else if ((PCI_BASE_TYPE_M & regvalue)
				== PCI_BASE_TYPE_ALL) {
				hiword |= PCI_ADDR_MEM64;
			}
		} else {
			hiword |= PCI_ADDR_IO;
		}
	}

	addition.pci_phys_hi = hiword;
	addition.pci_phys_mid = 0;
	addition.pci_phys_low = 0;
	addition.pci_size_hi = 0;
	addition.pci_size_low = size;

	bcopy(reg, newreg, rlen);
	bcopy(&addition, newreg + rlen, sizeof (pci_regspec_t));

	/*
	 * Write out the new "reg" property
	 */
	(void) ndi_prop_update_int_array(DDI_DEV_T_NONE,
		dip, "reg", (int *)newreg,
		(rlen + sizeof (pci_regspec_t))/sizeof (int));

	kmem_free((caddr_t)newreg, rlen+sizeof (pci_regspec_t));
	kmem_free((caddr_t)reg, rlen);

	return (PCICFG_SUCCESS);
}

static void
pcicfg_device_on(ddi_acc_handle_t config_handle)
{
	/*
	 * Enable memory, IO, and bus mastership
	 * XXX should we enable parity, SERR#,
	 * fast back-to-back, and addr. stepping?
	 */
	pci_config_put16(config_handle, PCI_CONF_COMM,
		pci_config_get16(config_handle, PCI_CONF_COMM) | 0x7);
}

static void
pcicfg_device_off(ddi_acc_handle_t config_handle)
{
	/*
	 * Disable I/O and memory traffic through the bridge
	 */
	pci_config_put16(config_handle, PCI_CONF_COMM, 0x0);
}

/*
 * Setup the basic 1275 properties based on information found in the config
 * header of the PCI device
 */
static int
pcicfg_set_standard_props(dev_info_t *dip, ddi_acc_handle_t config_handle)
{
	int ret;
	uint16_t val;
	uint32_t wordval;
	uint8_t byteval;

	/* These two exists only for non-bridges */
	if ((pci_config_get8(config_handle,
		PCI_CONF_HEADER) & PCI_HEADER_TYPE_M) == PCI_HEADER_ZERO) {
		byteval = pci_config_get8(config_handle, PCI_CONF_MIN_G);
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
			"min-grant", byteval)) != DDI_SUCCESS) {
			return (ret);
		}

		byteval = pci_config_get8(config_handle, PCI_CONF_MAX_L);
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
			"max-latency", byteval)) != DDI_SUCCESS) {
			return (ret);
		}
	}

	/*
	 *These should always exist and have the value of the
	 * corresponding register value
	 */
	val = pci_config_get16(config_handle, PCI_CONF_VENID);

	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"vendor-id", val)) != DDI_SUCCESS) {
		return (ret);
	}
	val = pci_config_get16(config_handle, PCI_CONF_DEVID);
	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"device-id", val)) != DDI_SUCCESS) {
		return (ret);
	}
	byteval = pci_config_get8(config_handle, PCI_CONF_REVID);
	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"revision-id", byteval)) != DDI_SUCCESS) {
		return (ret);
	}

	wordval = (pci_config_get16(config_handle, PCI_CONF_SUBCLASS)<< 8) |
		(pci_config_get8(config_handle, PCI_CONF_PROGCLASS));

	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"class-code", wordval)) != DDI_SUCCESS) {
		return (ret);
	}
	val = (pci_config_get16(config_handle,
		PCI_CONF_STAT) & PCI_STAT_DEVSELT);
	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"devsel-speed", val)) != DDI_SUCCESS) {
		return (ret);
	}

	/*
	 * The next three are bits set in the status register.  The property is
	 * present (but with no value other than its own existence) if the bit
	 * is set, non-existent otherwise
	 */
	if (pci_config_get16(config_handle, PCI_CONF_STAT) & PCI_STAT_FBBC) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"fast-back-to-back", 0)) != DDI_SUCCESS) {
			return (ret);
		}
	}
	if (pci_config_get16(config_handle, PCI_CONF_STAT) & PCI_STAT_66MHZ) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"66mhz-capable", 0)) != DDI_SUCCESS) {
			return (ret);
		}
	}
	if (pci_config_get16(config_handle, PCI_CONF_STAT) & PCI_STAT_UDF) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"udf-supported", 0)) != DDI_SUCCESS) {
			return (ret);
		}
	}

	/*
	 * These next three are optional and are not present
	 * if the corresponding register is zero.  If the value
	 * is non-zero then the property exists with the value
	 * of the register.
	 */
	if ((val = pci_config_get16(config_handle,
		PCI_CONF_SUBVENID)) != 0) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"subsystem-vendor-id", val)) != DDI_SUCCESS) {
			return (ret);
		}
	}
	if ((val = pci_config_get16(config_handle,
		PCI_CONF_SUBSYSID)) != 0) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"subsystem-id", val)) != DDI_SUCCESS) {
			return (ret);
		}
	}
	if ((val = pci_config_get16(config_handle,
		PCI_CONF_CACHE_LINESZ)) != 0) {
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"cache-line-size", val)) != DDI_SUCCESS) {
			return (ret);
		}
	}

	/*
	 * If the Interrupt Pin register is non-zero then the
	 * interrupts property exists
	 */
	if ((byteval = pci_config_get8(config_handle, PCI_CONF_IPIN)) != 0) {
		/*
		 * If interrupt pin is non-zero,
		 * record the interrupt line used
		 */
		if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"interrupts", byteval)) != DDI_SUCCESS) {
			return (ret);
		}
	}
	return (PCICFG_SUCCESS);
}
static int
pcicfg_set_busnode_props(dev_info_t *dip)
{
	int ret;

	if ((ret = ndi_prop_update_string(DDI_DEV_T_NONE, dip,
				"device_type", "pci")) != DDI_SUCCESS) {
		return (ret);
	}
	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"#address-cells", 3)) != DDI_SUCCESS) {
		return (ret);
	}
	if ((ret = ndi_prop_update_int(DDI_DEV_T_NONE, dip,
				"#size-cells", 2)) != DDI_SUCCESS) {
		return (ret);
	}
	return (PCICFG_SUCCESS);
}

static int
pcicfg_set_childnode_props(dev_info_t *dip, ddi_acc_handle_t config_handle)
{

	int		ret;
#ifndef _DONT_USE_1275_GENERIC_NAMES
	uint32_t	wordval;
#endif
	char		*name;
	char		buffer[64];
	uint32_t	classcode;
	char		*compat[8];
	int		i;
	int		n;
	/*
	 * NOTE: These are for both a child and PCI-PCI bridge node
	 */
#ifndef _DONT_USE_1275_GENERIC_NAMES
	wordval = (pci_config_get16(config_handle, PCI_CONF_SUBCLASS)<< 8) |
		(pci_config_get8(config_handle, PCI_CONF_PROGCLASS));
#endif

	if (pci_config_get16(config_handle, PCI_CONF_SUBSYSID) != 0) {
		(void) sprintf(buffer, "pci%x,%x",
			pci_config_get16(config_handle, PCI_CONF_SUBVENID),
			pci_config_get16(config_handle, PCI_CONF_SUBSYSID));
	} else {
		(void) sprintf(buffer, "pci%x,%x",
			pci_config_get16(config_handle, PCI_CONF_VENID),
			pci_config_get16(config_handle, PCI_CONF_DEVID));
	}

	/*
	 * In some environments, trying to use "generic" 1275 names is
	 * not the convention.  In those cases use the name as created
	 * above.  In all the rest of the cases, check to see if there
	 * is a generic name first.
	 */
#ifdef _DONT_USE_1275_GENERIC_NAMES
	name = buffer;
#else
	if ((name = pcicfg_get_class_name(wordval>>8)) == NULL) {
		/*
		 * Set name to the above fabricated name
		 */
		name = buffer;
	}
#endif

	/*
	 * The node name field needs to be filled in with the name
	 */
	if (ndi_devi_set_nodename(dip, name, 0) != NDI_SUCCESS) {
		DEBUG0("Failed to set nodename for node\n");
		return (PCICFG_FAILURE);
	}

	/*
	 * Create the compatible property as an array of pointers
	 * to strings.  Start with the buffer created above.
	 */
	n = 0;
	compat[n] = kmem_alloc(strlen(buffer) + 1, KM_SLEEP);
	(void) strcpy(compat[n++], buffer);

	/*
	 * Add in the VendorID/DeviceID compatible name.
	 */
	(void) sprintf(buffer, "pci%x,%x",
		pci_config_get16(config_handle, PCI_CONF_VENID),
		pci_config_get16(config_handle, PCI_CONF_DEVID));

	compat[n] = kmem_alloc(strlen(buffer) + 1, KM_SLEEP);
	(void) strcpy(compat[n++], buffer);

	classcode = (pci_config_get16(config_handle, PCI_CONF_SUBCLASS)<< 8) |
		(pci_config_get8(config_handle, PCI_CONF_PROGCLASS));

	/*
	 * Add in the Classcode
	 */
	(void) sprintf(buffer, "pciclass,%06x", classcode);

	compat[n] = kmem_alloc(strlen(buffer) + 1, KM_SLEEP);
	(void) strcpy(compat[n++], buffer);

	if ((ret = ndi_prop_update_string_array(DDI_DEV_T_NONE, dip,
		"compatible", (char **)compat, n)) != DDI_SUCCESS) {
		return (ret);
	}

	for (i = 0; i < n; i++) {
		kmem_free(compat[i], strlen(compat[i]) + 1);
	}

	return (PCICFG_SUCCESS);
}

/*
 * Program the bus numbers into the bridge
 */

static void
pcicfg_set_bus_numbers(ddi_acc_handle_t config_handle,
uint_t primary, uint_t secondary)
{
	/*
	 * Primary bus#
	 */
	pci_config_put8(config_handle, PCI_BCNF_PRIBUS, primary);

	/*
	 * Secondary bus#
	 */
	pci_config_put8(config_handle, PCI_BCNF_SECBUS, secondary);

	/*
	 * Set the subordinate bus number to ff in order to pass through any
	 * type 1 cycle with a bus number higher than the secondary bus#
	 */
	pci_config_put8(config_handle, PCI_BCNF_SUBBUS, 0xFF);
}

/*
 * Put bridge registers into initial state
 */
static void
pcicfg_setup_bridge(pcicfg_phdl_t *entry,
	ddi_acc_handle_t handle)
{
	/*
	 * The highest bus seen during probing is the max-subordinate bus
	 */
	pci_config_put8(handle, PCI_BCNF_SUBBUS, entry->highest_bus);

	/*
	 * Reset the secondary bus
	 */
	pci_config_put16(handle, PCI_BCNF_BCNTRL,
		pci_config_get16(handle, PCI_BCNF_BCNTRL) | 0x40);
	pci_config_put16(handle, PCI_BCNF_BCNTRL,
		pci_config_get16(handle, PCI_BCNF_BCNTRL) & ~0x40);

	/*
	 * Program the memory base register with the
	 * start of the memory range
	 */
	pci_config_put16(handle, PCI_BCNF_MEM_BASE,
		PCICFG_HIWORD(PCICFG_LOADDR(entry->memory_last)));

	/*
	 * Program the I/O base register with the start of the I/O range
	 */
	pci_config_put8(handle, PCI_BCNF_IO_BASE_LOW,
		PCICFG_HIBYTE(PCICFG_LOWORD(PCICFG_LOADDR(entry->io_last))));
	pci_config_put16(handle, PCI_BCNF_IO_BASE_HI,
		PCICFG_HIWORD(PCICFG_LOADDR(entry->io_last)));

	/*
	 * Clear status bits
	 */
	pci_config_put16(handle, PCI_BCNF_SEC_STATUS, 0xffff);

	/*
	 * Turn off prefetchable range
	 */
	pci_config_put32(handle, PCI_BCNF_PF_BASE_LOW, 0x0000ffff);
	pci_config_put32(handle, PCI_BCNF_PF_BASE_HIGH, 0xffffffff);
	pci_config_put32(handle, PCI_BCNF_PF_LIMIT_HIGH, 0x0);

	/*
	 * Needs to be set to this value
	 */
	pci_config_put8(handle, PCI_CONF_ILINE, 0xf);
}

static void
pcicfg_update_bridge(pcicfg_phdl_t *entry,
	ddi_acc_handle_t handle)
{
	uint_t length;

	/*
	 * Program the memory limit register with the end of the memory range
	 */

	DEBUG1("DOWN ROUNDED ===>[0x%x]\n",
		PCICFG_ROUND_DOWN(entry->memory_last,
		PCICFG_MEMGRAN));

	pci_config_put16(handle, PCI_BCNF_MEM_LIMIT,
		PCICFG_HIWORD(PCICFG_LOADDR(
		PCICFG_ROUND_DOWN(entry->memory_last,
			PCICFG_MEMGRAN))));
	/*
	 * Since this is a bridge, the rest of this range will
	 * be responded to by the bridge.  We have to round up
	 * so no other device claims it.
	 */
	if ((length = (PCICFG_ROUND_UP(entry->memory_last,
		PCICFG_MEMGRAN) - entry->memory_last)) > 0) {
		(void) pcicfg_get_mem(entry, length, NULL);
		DEBUG1("Added [0x%x]at the top of "
		"the bridge (mem)\n", length);
	}

	/*
	 * Program the I/O limit register with the end of the I/O range
	 */
	pci_config_put8(handle, PCI_BCNF_IO_LIMIT_LOW,
		PCICFG_HIBYTE(PCICFG_LOWORD(
		PCICFG_LOADDR(PCICFG_ROUND_DOWN(entry->io_last,
			PCICFG_IOGRAN)))));

	pci_config_put16(handle, PCI_BCNF_IO_LIMIT_HI,
		PCICFG_HIWORD(PCICFG_LOADDR(PCICFG_ROUND_DOWN(entry->io_last,
		PCICFG_IOGRAN))));

	/*
	 * Same as above for I/O space. Since this is a
	 * bridge, the rest of this range will be responded
	 * to by the bridge.  We have to round up so no
	 * other device claims it.
	 */
	if ((length = (PCICFG_ROUND_UP(entry->io_last,
		PCICFG_IOGRAN) - entry->io_last)) > 0) {
		(void) pcicfg_get_io(entry, length, NULL);
		DEBUG1("Added [0x%x]at the top of "
		"the bridge (I/O)\n",  length);
	}
}

static int
pcicfg_probe_children(dev_info_t *parent, uint_t bus,
	uint_t device, uint_t func)
{
	dev_info_t		*new_child;
	ddi_acc_handle_t	config_handle;
	uint8_t			header_type;

	int			i, j;
	ndi_ra_request_t	req;
	uint64_t		next_bus;
	uint64_t		blen;
	uint32_t		request;
	uint_t			new_bus;
	int			ret;

	/*
	 * This node will be put immediately below
	 * "parent". Allocate a blank device node.  It will either
	 * be filled in or freed up based on further probing.
	 */

	if (ndi_devi_alloc(parent, DEVI_PSEUDO_NEXNAME,
		(dnode_t)DEVI_SID_NODEID, &new_child)
		!= NDI_SUCCESS) {
		DEBUG0("pcicfg_probe_children(): Failed to alloc child node\n");
		return (PCICFG_FAILURE);
	}

	if (pcicfg_add_config_reg(new_child, bus,
		device, func) != DDI_SUCCESS) {
		DEBUG0("pcicfg_probe_children():"
		"Failed to add candidate REG\n");
		goto failedchild;
	}

	if ((ret = pcicfg_config_setup(new_child, &config_handle))
		!= PCICFG_SUCCESS) {
		if (ret == PCICFG_NODEVICE) {
			(void) ndi_devi_free(new_child);
			return (ret);
		}
		DEBUG0("pcicfg_probe_children():"
		"Failed to setup config space\n");
		goto failedconfig;
	}

	/*
	 * As soon as we have access to config space,
	 * turn off device. It will get turned on
	 * later (after memory is assigned).
	 */
	(void) pcicfg_device_off(config_handle);


	/*
	 * Set 1275 properties common to all devices
	 */
	if (pcicfg_set_standard_props(new_child,
		config_handle) != PCICFG_SUCCESS) {
		DEBUG0("Failed to set standard properties\n");
		goto failedchild;
	}

	/*
	 * Child node properties  NOTE: Both for PCI-PCI bridge and child node
	 */
	if (pcicfg_set_childnode_props(new_child,
		config_handle) != PCICFG_SUCCESS) {
		goto failedchild;
	}

	header_type = pci_config_get8(config_handle, PCI_CONF_HEADER);


	/*
	 * If this is not a multi-function card only probe function zero.
	 */
	if (!(header_type & PCI_HEADER_MULTI) && (func != 0)) {

		(void) pcicfg_config_teardown(&config_handle);
		(void) ndi_devi_free(new_child);
		return (PCICFG_NODEVICE);
	}

	DEBUG1("---Vendor ID = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_VENID));
	DEBUG1("---Device ID = [0x%x]\n",
		pci_config_get16(config_handle, PCI_CONF_DEVID));

	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_PPB) {

		DEBUG3("--Bridge found bus [0x%x] device"
			"[0x%x] func [0x%x]\n", bus, device, func);

		/*
		 * Get next bus in sequence and program device.
		 * XXX There might have to be slot specific
		 * ranges taken care of here.
		 */
		bzero((caddr_t)&req, sizeof (ndi_ra_request_t));
		req.ra_len = 1;
		if (ndi_ra_alloc(ddi_get_parent(new_child), &req,
			&next_bus, &blen, NDI_RA_TYPE_PCI_BUSNUM,
			NDI_RA_PASS) != NDI_SUCCESS) {
			DEBUG0("Failed to get a bus number\n");
			goto failedchild;
		}
		new_bus = next_bus;

		DEBUG1("NEW bus found  ->[%d]\n", new_bus);

		(void) pcicfg_set_bus_numbers(config_handle,
			bus, new_bus);
		/*
		 * Set bus properties
		 */
		if (pcicfg_set_busnode_props(new_child) != PCICFG_SUCCESS) {
			DEBUG0("Failed to set busnode props\n");
			goto failedchild;
		}

		/*
		 * Probe all children devices
		 */
		for (i = 0; i < PCICFG_MAX_DEVICE; i++) {
			for (j = 0; j < PCICFG_MAX_FUNCTION; j++) {
				if (pcicfg_probe_children(new_child,
					new_bus, i, j) == PCICFG_FAILURE) {
					DEBUG3("Failed to configure bus "
					"[0x%x] device [0x%x] func [0x%x]\n",
						new_bus, i, j);
					goto failedchild;
				}
			}
		}

	} else {

		DEBUG3("--Leaf device found bus [0x%x] device"
			"[0x%x] func [0x%x]\n",
				bus, device, func);

		i = PCI_CONF_BASE0;

		while (i <= PCI_CONF_BASE5) {

			pci_config_put32(config_handle, i, 0xffffffff);

			request = pci_config_get32(config_handle, i);
			/*
			 * If its a zero length, don't do
			 * any programming.
			 */
			if (request != 0) {
				/*
				 * Add to the "reg" property
				 */
				if (pcicfg_update_reg_prop(new_child,
					request, i) != PCICFG_SUCCESS) {
					goto failedchild;
				}
			} else {
				break;
			}

			/*
			 * Increment by eight if it is 64 bit address space
			 */
			if ((PCI_BASE_TYPE_M & request) == PCI_BASE_TYPE_ALL) {
				DEBUG3("BASE register [0x%x] asks for "
				"[0x%x]=[0x%x] (64)\n",
					i, request,
					(~(PCI_BASE_M_ADDR_M & request))+1)
				i += 8;
			} else {
				DEBUG3("BASE register [0x%x] asks for "
				"[0x%x]=[0x%x](32)\n",
					i, request,
					(~(PCI_BASE_M_ADDR_M & request))+1)
				i += 4;
			}
		}

		/*
		 * Get the ROM size and create register for it
		 */
		pci_config_put32(config_handle, PCI_CONF_ROM, 0xfffffffe);

		request = pci_config_get32(config_handle, PCI_CONF_ROM);
		/*
		 * If its a zero length, don't do
		 * any programming.
		 */

		if (request != 0) {
			DEBUG3("BASE register [0x%x] asks for [0x%x]=[0x%x]\n",
				PCI_CONF_ROM, request,
				(~(PCI_BASE_ROM_ADDR_M & request))+1);
			/*
			 * Add to the "reg" property
			 */
			if (pcicfg_update_reg_prop(new_child,
				request, PCI_CONF_ROM) != PCICFG_SUCCESS) {
				goto failedchild;
			}
		}
	}

	(void) pcicfg_config_teardown(&config_handle);

	/*
	 * Attach the child to its parent
	 */
	(void) pcicfg_devi_attach_to_parent(new_child);

	return (PCICFG_SUCCESS);

failedchild:
	/*
	 * XXX check if it should be taken offline (if online)
	 */
	(void) pcicfg_config_teardown(&config_handle);

failedconfig:

	(void) ndi_devi_free(new_child);

	return (PCICFG_FAILURE);
}



/*
 * Make "parent" be the parent of the "child" dip
 */
static void
pcicfg_reparent_node(dev_info_t *child, dev_info_t *parent)
{
	uint_t count;

	i_ndi_block_device_tree_changes(&count);
	DEVI(child)->devi_parent = DEVI(parent);
	DEVI(child)->devi_bus_ctl = DEVI(parent);
	i_ndi_allow_device_tree_changes(count);
}

int
pcicfg_config_setup(dev_info_t *dip, ddi_acc_handle_t *handle)
{
	caddr_t	cfgaddr;
	ddi_device_acc_attr_t attr;
	dev_info_t *anode;
	int status;
	int		rlen;
	pci_regspec_t	*reg;
	int		ret;

	/*
	 * Get the pci register spec from the node
	 */
	status = ddi_getlongprop(DDI_DEV_T_NONE,
		dip, DDI_PROP_DONTPASS, "reg", (caddr_t)&reg, &rlen);

	switch (status) {
		case DDI_PROP_SUCCESS:
		break;
		case DDI_PROP_NO_MEMORY:
			DEBUG0("reg present, but unable to get memory\n");
			return (PCICFG_FAILURE);
		default:
			DEBUG0("no reg property\n");
			return (PCICFG_FAILURE);
	}

	anode = dip;

	/*
	 * Find the attachment point node
	 */
	while ((anode != NULL) && (strcmp(ddi_binding_name(anode),
		"hp_attachment") != 0)) {
		anode = ddi_get_parent(anode);
	}

	if (anode == NULL) {
		DEBUG0("Tree not in PROBE state\n");
		kmem_free((caddr_t)reg, rlen);
		return (PCICFG_FAILURE);
	}

	if (ndi_prop_update_int_array(DDI_DEV_T_NONE, anode,
				    "reg", (int *)reg, 5)) {
		DEBUG0("Failed to update reg property...\n");
		kmem_free((caddr_t)reg, rlen);
		return (PCICFG_FAILURE);
	}

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
#if 0
	DEBUG3("PROBING =>->->->->->-> [0x%x][0x%x][0x%x]\n",
		PCI_REG_BUS_G(reg->pci_phys_hi),
		PCI_REG_DEV_G(reg->pci_phys_hi),
		PCI_REG_FUNC_G(reg->pci_phys_hi));
#endif
	if (ddi_regs_map_setup(anode, 0, &cfgaddr,
		0, 0, &attr, handle) != DDI_SUCCESS) {
		DEBUG0("Failed to setup registers\n");
		return (PCICFG_FAILURE);
	}


	if (ddi_get16(*handle, (uint16_t *)cfgaddr) == 0xffff) {
		ret = PCICFG_NODEVICE;
	} else {
		ret = PCICFG_SUCCESS;

	}

	kmem_free((caddr_t)reg, rlen);

	return (ret);

}

static void
pcicfg_config_teardown(ddi_acc_handle_t *handle)
{
	(void) ddi_regs_map_free(handle);
}

static void
pcicfg_devi_attach_to_parent(dev_info_t *dip)
{
	struct dev_info *devi = DEVI(dip);
	struct dev_info *parent = devi->devi_parent;

	dev_info_t **pdip, *list;

	/*
	 * attach the node to its parent.
	 */
	rw_enter(&(devinfo_tree_lock), RW_WRITER);
	pdip = (dev_info_t **)(&DEVI(parent)->devi_child);
	list = *pdip;

	while (list && (list != dip)) {
		pdip = (dev_info_t **)(&DEVI(list)->devi_sibling);
		list = (dev_info_t *)DEVI(list)->devi_sibling;
	}
	if (list == NULL) {
		*pdip = dip;
		DEVI(dip)->devi_sibling = NULL;
	}
	rw_exit(&(devinfo_tree_lock));
}

static int
pcicfg_add_config_reg(dev_info_t *dip,
	uint_t bus, uint_t device, uint_t func)
{
	int reg[10] = { PCI_ADDR_CONFIG, 0, 0, 0, 0};

	reg[0] = PCICFG_MAKE_REG_HIGH(bus, device, func, 0);

	return (ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
		"reg", reg, 5));
}

#ifdef DEBUG
static void
debug(char *fmt, uintptr_t a1, uintptr_t a2, uintptr_t a3,
	uintptr_t a4, uintptr_t a5)
{
	if (pcicfg_debug != 0) {
		prom_printf("pcicfg: ");
		prom_printf(fmt, a1, a2, a3, a4, a5);
	}
}
#endif
