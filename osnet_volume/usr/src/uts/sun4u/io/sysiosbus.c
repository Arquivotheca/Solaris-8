/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysiosbus.c	1.96	99/08/28 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/autoconf.h>
#include <sys/spl.h>
#include <sys/intr.h>
#include <sys/iommu.h>
#include <sys/sysiosbus.h>
#include <sys/sysioerr.h>
#include <sys/iocache.h>
#include <sys/machsystm.h>
#include <sys/intreg.h>
#include <sys/ddi_subrdefs.h>
#ifdef _STARFIRE
#include <sys/starfire.h>
#endif _STARFIRE

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>
/* Bitfield debugging definitions for this file */
#define	SBUS_ATTACH_DEBUG	0x1
#define	SBUS_SBUSMEM_DEBUG	0x2
#define	SBUS_INTERRUPT_DEBUG	0x4
#define	SBUS_REGISTERS_DEBUG	0x8

/*
 * Interrupt registers table.
 * This table is necessary due to inconsistencies in the sysio register
 * layout.  If this gets fixed in the chip, we can get rid of this stupid
 * table.
 */
static struct sbus_slot_entry ino_1 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L1_CLEAR, NULL};
static struct sbus_slot_entry ino_2 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L2_CLEAR, NULL};
static struct sbus_slot_entry ino_3 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L3_CLEAR, NULL};
static struct sbus_slot_entry ino_4 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L4_CLEAR, NULL};
static struct sbus_slot_entry ino_5 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L5_CLEAR, NULL};
static struct sbus_slot_entry ino_6 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L6_CLEAR, NULL};
static struct sbus_slot_entry ino_7 = {SBUS_SLOT0_CONFIG, SBUS_SLOT0_MAPREG,
					SBUS_SLOT0_L7_CLEAR, NULL};
static struct sbus_slot_entry ino_9 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L1_CLEAR, NULL};
static struct sbus_slot_entry ino_10 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L2_CLEAR, NULL};
static struct sbus_slot_entry ino_11 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L3_CLEAR, NULL};
static struct sbus_slot_entry ino_12 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L4_CLEAR, NULL};
static struct sbus_slot_entry ino_13 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L5_CLEAR, NULL};
static struct sbus_slot_entry ino_14 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L6_CLEAR, NULL};
static struct sbus_slot_entry ino_15 = {SBUS_SLOT1_CONFIG, SBUS_SLOT1_MAPREG,
					SBUS_SLOT1_L7_CLEAR, NULL};
static struct sbus_slot_entry ino_17 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L1_CLEAR, NULL};
static struct sbus_slot_entry ino_18 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L2_CLEAR, NULL};
static struct sbus_slot_entry ino_19 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L3_CLEAR, NULL};
static struct sbus_slot_entry ino_20 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L4_CLEAR, NULL};
static struct sbus_slot_entry ino_21 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L5_CLEAR, NULL};
static struct sbus_slot_entry ino_22 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L6_CLEAR, NULL};
static struct sbus_slot_entry ino_23 = {SBUS_SLOT2_CONFIG, SBUS_SLOT2_MAPREG,
					SBUS_SLOT2_L7_CLEAR, NULL};
static struct sbus_slot_entry ino_25 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L1_CLEAR, NULL};
static struct sbus_slot_entry ino_26 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L2_CLEAR, NULL};
static struct sbus_slot_entry ino_27 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L3_CLEAR, NULL};
static struct sbus_slot_entry ino_28 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L4_CLEAR, NULL};
static struct sbus_slot_entry ino_29 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L5_CLEAR, NULL};
static struct sbus_slot_entry ino_30 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L6_CLEAR, NULL};
static struct sbus_slot_entry ino_31 = {SBUS_SLOT3_CONFIG, SBUS_SLOT3_MAPREG,
					SBUS_SLOT3_L7_CLEAR, NULL};
static struct sbus_slot_entry ino_32 = {SBUS_SLOT5_CONFIG, ESP_MAPREG,
					ESP_CLEAR, ESP_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_33 = {SBUS_SLOT5_CONFIG, ETHER_MAPREG,
					ETHER_CLEAR, ETHER_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_34 = {SBUS_SLOT5_CONFIG, PP_MAPREG,
					PP_CLEAR, PP_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_36 = {SBUS_SLOT4_CONFIG, AUDIO_MAPREG,
					AUDIO_CLEAR, AUDIO_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_40 = {SBUS_SLOT6_CONFIG, KBDMOUSE_MAPREG,
					KBDMOUSE_CLEAR,
					KBDMOUSE_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_41 = {SBUS_SLOT6_CONFIG, FLOPPY_MAPREG,
					FLOPPY_CLEAR, FLOPPY_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_42 = {SBUS_SLOT6_CONFIG, THERMAL_MAPREG,
					THERMAL_CLEAR,
					THERMAL_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_48 = {SBUS_SLOT6_CONFIG, TIMER0_MAPREG,
					TIMER0_CLEAR, TIMER0_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_49 = {SBUS_SLOT6_CONFIG, TIMER1_MAPREG,
					TIMER1_CLEAR, TIMER1_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_52 = {SBUS_SLOT6_CONFIG, UE_ECC_MAPREG,
					UE_ECC_CLEAR, UE_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_53 = {SBUS_SLOT6_CONFIG, CE_ECC_MAPREG,
					CE_ECC_CLEAR, CE_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_54 = {SBUS_SLOT6_CONFIG, SBUS_ERR_MAPREG,
					SBUS_ERR_CLEAR, SERR_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_55 = {SBUS_SLOT6_CONFIG, PM_WAKEUP_MAPREG,
					PM_WAKEUP_CLEAR, PM_INTR_STATE_SHIFT};
static struct sbus_slot_entry ino_ffb = {NULL, FFB_MAPPING_REG, NULL, NULL};
static struct sbus_slot_entry ino_exp = {NULL, EXP_MAPPING_REG, NULL, NULL};

/* Construct the interrupt number array */
struct sbus_slot_entry *ino_table[] = {
	NULL, &ino_1, &ino_2, &ino_3, &ino_4, &ino_5, &ino_6, &ino_7,
	NULL, &ino_9, &ino_10, &ino_11, &ino_12, &ino_13, &ino_14, &ino_15,
	NULL, &ino_17, &ino_18, &ino_19, &ino_20, &ino_21, &ino_22, &ino_23,
	NULL, &ino_25, &ino_26, &ino_27, &ino_28, &ino_29, &ino_30, &ino_31,
	&ino_32, &ino_33, &ino_34, NULL, &ino_36, NULL, NULL, NULL,
	&ino_40, &ino_41, &ino_42, NULL, NULL, NULL, NULL, NULL, &ino_48,
	&ino_49, NULL, NULL, &ino_52, &ino_53, &ino_54, &ino_55, &ino_ffb,
	&ino_exp
};

/*
 * This table represents the Fusion interrupt priorities.  They range
 * from 1 - 15, so we'll pattern the priorities after the 4M.  We map Fusion
 * interrupt number to system priority.  The mondo number is used as an
 * index into this table.
 */
int interrupt_priorities[] = {
	-1, 2, 3, 5, 7, 9, 11, 13,	/* Slot 0 sbus level 1 - 7 */
	-1, 2, 3, 5, 7, 9, 11, 13,	/* Slot 1 sbus level 1 - 7 */
	-1, 2, 3, 5, 7, 9, 11, 13,	/* Slot 2 sbus level 1 - 7 */
	-1, 2, 3, 5, 7, 9, 11, 13,	/* Slot 3 sbus level 1 - 7 */
	4,				/* Onboard SCSI */
	6,				/* Onboard Ethernet */
	3,				/* Onboard Parallel port */
	-1,				/* Not in use */
	9,				/* Onboard Audio */
	-1, -1, -1,			/* Not in use */
	12,				/* Onboard keyboard/serial ports */
	11,				/* Onboard Floppy */
	9,				/* Thermal interrupt */
	-1, -1, -1,			/* Not is use */
	10,				/* Timer 0 (tick timer) */
	14,				/* Timer 1 (not used) */
	15,				/* Sysio UE ECC error */
	10,				/* Sysio CE ECC error */
	10,				/* Sysio Sbus error */
	10,				/* PM Wakeup */
};

/* Interrupt counter flag.  To enable/disable spurious interrupt counter. */
static int intr_cntr_on;

/*
 * Function prototypes.
 */
static int
sbus_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static int
sbus_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);

static void
sbus_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);

static int
sbus_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t ctlop,
    void *arg, void *result);

static int
sbus_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, ddi_ispec_t *ispecp,
    int32_t ign);

static int
sbus_identify(dev_info_t *devi);

static int
sbus_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static int
sbus_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static int
sbus_do_detach(dev_info_t *devi);

static	void
sbus_add_picN_kstats(dev_info_t *dip);

static	void
sbus_add_kstats(struct sbus_soft_state *);

static	int
sbus_counters_kstat_update(kstat_t *, int);

extern int
sysio_err_uninit(struct sbus_soft_state *softsp);

extern int
iommu_uninit(struct sbus_soft_state *softsp);

extern int
stream_buf_uninit(struct sbus_soft_state *softsp);

static int
find_sbus_slot(dev_info_t *dip, dev_info_t *rdip);

static int
make_sbus_ppd(dev_info_t *child);

static int
sbusmem_initchild(dev_info_t *dip, dev_info_t *child);

static int
sbus_initchild(dev_info_t *dip, dev_info_t *child);

static int
sbus_uninitchild(dev_info_t *dip);

static int
sbus_init(struct sbus_soft_state *softsp, caddr_t address);

static int
sbus_resume_init(struct sbus_soft_state *softsp, int resume);

static void
sbus_cpr_handle_intr_map_reg(uint64_t *cpr_softsp, volatile uint64_t *baddr,
    int flag);

void sbus_intrdist(void *, int, uint_t);

#ifdef	_STARFIRE
void
pc_ittrans_init(int, caddr_t *);

void
pc_ittrans_uninit(caddr_t);

int
pc_translate_tgtid(caddr_t, int, volatile uint64_t *);

void
pc_ittrans_cleanup(caddr_t, volatile uint64_t *);
#endif	/* _STARFIRE */

/*
 * Configuration data structures
 */
static struct bus_ops sbus_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	0,
	0,
	0,
	i_ddi_map_fault,
	iommu_dma_map,
	iommu_dma_allochdl,
	iommu_dma_freehdl,
	iommu_dma_bindhdl,
	iommu_dma_unbindhdl,
	iommu_dma_flush,
	iommu_dma_win,
	iommu_dma_mctl,
	sbus_ctlops,
	ddi_bus_prop_op,
	0,			/* (*bus_get_eventcookie)();	*/
	0,			/* (*bus_add_eventcall)();	*/
	0,			/* (*bus_remove_eventcall)();	*/
	0,			/* (*bus_post_event)();		*/
	sbus_intr_ctlops
};

static struct cb_ops sbus_cb_ops = {
	nodev,			/* open */
	nodev,			/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,
	D_NEW | D_MP | D_HOTPLUG,
	CB_REV,				/* rev */
	nodev,				/* int (*cb_aread)() */
	nodev				/* int (*cb_awrite)() */
};

static struct dev_ops sbus_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	sbus_identify,		/* identify */
	0,			/* probe */
	sbus_attach,		/* attach */
	sbus_detach,		/* detach */
	nodev,			/* reset */
	&sbus_cb_ops,		/* driver operations */
	&sbus_bus_ops,		/* bus operations */
	nulldev			/* power */
};

/* global data */
void *sbusp;		/* sbus soft state hook */
void *sbus_cprp;	/* subs suspend/resume soft state hook */
static kstat_t *sbus_picN_ksp[SBUS_NUM_PICS]; /* performance picN kstats */
static int	sbus_attachcnt = 0;   /* number of instances attached */
static kmutex_t	sbus_attachcnt_mutex; /* sbus_attachcnt lock - attach/detach */

#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module.  This one is a driver */
	"SBus (sysio) nexus driver 1.96",	/* Name of module. */
	&sbus_ops,		/* driver ops */
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
	int error;

	if ((error = ddi_soft_state_init(&sbusp,
	    sizeof (struct sbus_soft_state), 1)) != 0)
		return (error);

	/*
	 * Initialize cpr soft state structure
	 */
	if ((error = ddi_soft_state_init(&sbus_cprp,
	    sizeof (uint64_t) * MAX_INO_TABLE_SIZE, 0)) != 0)
		return (error);

	/* Initialize global mutex */
	mutex_init(&sbus_attachcnt_mutex, NULL, MUTEX_DRIVER, NULL);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	mutex_destroy(&sbus_attachcnt_mutex);
	ddi_soft_state_fini(&sbusp);
	ddi_soft_state_fini(&sbus_cprp);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
sbus_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if (strcmp(name, "sbus") == 0) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

/*ARGSUSED*/
static int
sbus_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct sbus_soft_state *softsp;
	int instance, error;
	uint64_t *cpr_softsp;
	ddi_device_acc_attr_t attr;


#ifdef	DEBUG
	debug_info = 1;
	debug_print_level = 0;
#endif

	instance = ddi_get_instance(devi);

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		softsp = ddi_get_soft_state(sbusp, instance);

		if ((error = iommu_resume_init(softsp)) != DDI_SUCCESS)
			return (error);

		if ((error = sbus_resume_init(softsp, 1)) != DDI_SUCCESS)
			return (error);

		if ((error = stream_buf_resume_init(softsp)) != DDI_SUCCESS)
			return (error);

		/*
		 * Restore Interrupt Mapping registers
		 */
		cpr_softsp = ddi_get_soft_state(sbus_cprp, instance);

		if (cpr_softsp != NULL) {
			sbus_cpr_handle_intr_map_reg(cpr_softsp,
			    softsp->intr_mapping_reg, 0);
			ddi_soft_state_free(sbus_cprp, instance);
		}

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(sbusp, instance) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softsp = ddi_get_soft_state(sbusp, instance);

	/* Set the dip in the soft state */
	softsp->dip = devi;

	/* Initialize to not using any spare iommu tsb */

	softsp->spare_slot = (uint_t)-1;


	if ((softsp->upa_id = (int)ddi_getprop(DDI_DEV_T_ANY, softsp->dip,
	    DDI_PROP_DONTPASS, "upa-portid", -1)) == -1) {
		cmn_err(CE_WARN, "Unable to retrieve sbus upa-portid"
		    "property.");
		error = DDI_FAILURE;
		goto bad;
	}

	/*
	 * The firmware maps in all 3 pages of the sysio chips device
	 * device registers and exports the mapping in the int-sized
	 * property "address".  Read in this address and pass it to
	 * the subsidiary *_init functions, so we don't create extra
	 * mappings to the same physical pages and we don't have to
	 * retrieve the more than once.
	 */
	/*
	 * Implement new policy to start ignoring the "address" property
	 * due to new requirements from DR.  The problem is that the contents
	 * of the "address" property contain vm mappings from OBP which needs
	 * to be recaptured into kernel vm.  Instead of relying on a blanket
	 * recapture during boot time, we map psycho registers each time during
	 * attach and unmap the during detach.  In some future point of time
	 * OBP will drop creating "address" property but this driver will
	 * will already not rely on this property any more.
	 */

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	if (ddi_regs_map_setup(softsp->dip, 0, &softsp->address, 0, 0,
		&attr, &softsp->ac) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "%s%d: unable to map reg set 0\n",
			ddi_get_name(softsp->dip),
			ddi_get_instance(softsp->dip));
		return (0);
	}
	if (softsp->address == (caddr_t)-1) {
		cmn_err(CE_CONT, "?sbus%d: No sysio <address> property\n",
		    ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	DPRINTF(SBUS_ATTACH_DEBUG, ("sbus: devi=0x%x, softsp=0x%x\n",
	    devi, softsp));

#ifdef	notdef
	/*
	 * This bit of code, plus the firmware, will tell us if
	 * the #size-cells infrastructure code works, to some degree.
	 * You should be able to use the firmware to determine if
	 * the address returned by ddi_map_regs maps the correct phys. pages.
	 */

	{
		caddr_t addr;
		int rv;

		cmn_err(CE_CONT, "?sbus: address property = 0x%x\n", address);

		if ((rv = ddi_map_regs(softsp->dip, 0, &addr,
		    (off_t)0, (off_t)0)) != DDI_SUCCESS)  {
			cmn_err(CE_CONT, "?sbus: ddi_map_regs failed: %d\n",
			    rv);
		} else {
			cmn_err(CE_CONT, "?sbus: ddi_map_regs returned "
			    " virtual address 0x%x\n", addr);
		}
	}
#endif	notdef

	if ((error = iommu_init(softsp, softsp->address)) != DDI_SUCCESS)
		goto bad;

	if ((error = sbus_init(softsp, softsp->address)) != DDI_SUCCESS)
		goto bad;

	if ((error = sysio_err_init(softsp, softsp->address)) != DDI_SUCCESS)
		goto bad;

	if ((error = stream_buf_init(softsp, softsp->address)) != DDI_SUCCESS)
		goto bad;

	/* Init the pokefault mutex for sbus devices */
	mutex_init(&softsp->pokefault_mutex, NULL, MUTEX_SPIN,
	    (void *)ipltospl(SBUS_ERR_PIL - 1));

	sbus_add_kstats(softsp);

	ddi_report_dev(devi);

	return (DDI_SUCCESS);

bad:
	ddi_soft_state_free(sbusp, instance);
	return (error);
}

/* ARGSUSED */
static int
sbus_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct sbus_soft_state *softsp;
	uint64_t *cpr_softsp;

	switch (cmd) {
	case DDI_SUSPEND:
		/*
		 * Allocate the cpr  soft data structure to save the current
		 * state of the interrupt mapping registers.
		 * This structure will be deallocated after the system
		 * is resumed.
		 */
		instance = ddi_get_instance(devi);

		if (ddi_soft_state_zalloc(sbus_cprp, instance)
			!= DDI_SUCCESS)
			return (DDI_FAILURE);

		cpr_softsp = ddi_get_soft_state(sbus_cprp, instance);

		softsp = ddi_get_soft_state(sbusp, instance);

		sbus_cpr_handle_intr_map_reg(cpr_softsp,
		    softsp->intr_mapping_reg, 1);
		return (DDI_SUCCESS);

	case DDI_DETACH:
		return (sbus_do_detach(devi));
	default:
		return (DDI_FAILURE);
	}
}

static int
sbus_do_detach(dev_info_t *devi)
{
	int instance, pic;
	struct sbus_soft_state *softsp;

	instance = ddi_get_instance(devi);
	softsp = ddi_get_soft_state(sbusp, instance);
	ASSERT(softsp != NULL);

	/* disable the streamming cache */
	if (stream_buf_uninit(softsp) == DDI_FAILURE) {
		goto err;
	}

	/* remove the interrupt handlers from the system */
	if (sysio_err_uninit(softsp) == DDI_FAILURE) {
		goto err;
	}

	/* disable the IOMMU */
	if (iommu_uninit(softsp)) {
		goto err;
	}

	/* unmap register space if we have a handle */
	if (softsp->ac) {
		ddi_regs_map_free(&softsp->ac);
		softsp->address = NULL;
	}

	/*
	 * remove counter kstats for this device
	 */
	if (softsp->sbus_counters_ksp != (kstat_t *)NULL)
		kstat_delete(softsp->sbus_counters_ksp);

	/*
	 * if we are the last instance to detach we need to
	 * remove the picN kstats. We use sbus_attachcnt as a
	 * count of how many instances are still attached. This
	 * is protected by a mutex.
	 */
	mutex_enter(&sbus_attachcnt_mutex);
	sbus_attachcnt --;
	if (sbus_attachcnt == 0) {
		for (pic = 0; pic < SBUS_NUM_PICS; pic++) {
			if (sbus_picN_ksp[pic] != (kstat_t *)NULL) {
				kstat_delete(sbus_picN_ksp[pic]);
				sbus_picN_ksp[pic] = NULL;
			}
		}
	}
	mutex_exit(&sbus_attachcnt_mutex);

#ifdef _STARFIRE
	/* free starfire specific soft intr mapping structure */
	pc_ittrans_uninit(softsp->ittrans_cookie);
#endif /* _STARFIRE */

	/* free the soft state structure */
	ddi_soft_state_free(sbusp, instance);

	return (DDI_SUCCESS);
err:
	return (DDI_FAILURE);
}

static int
sbus_init(struct sbus_soft_state *softsp, caddr_t address)
{
	int i;
	extern void set_intr_mapping_reg(int, uint64_t *, int);
	int numproxy;

	/*
	 * Simply add each registers offset to the base address
	 * to calculate the already mapped virtual address of
	 * the device register...
	 *
	 * define a macro for the pointer arithmetic; all registers
	 * are 64 bits wide and are defined as uint64_t's.
	 */

#define	REG_ADDR(b, o)	(uint64_t *)((caddr_t)(b) + (o))

	softsp->sysio_ctrl_reg = REG_ADDR(address, OFF_SYSIO_CTRL_REG);
	softsp->sbus_ctrl_reg = REG_ADDR(address, OFF_SBUS_CTRL_REG);
	softsp->sbus_slot_config_reg = REG_ADDR(address, OFF_SBUS_SLOT_CONFIG);
	softsp->intr_mapping_reg = REG_ADDR(address, OFF_INTR_MAPPING_REG);
	softsp->clr_intr_reg = REG_ADDR(address, OFF_CLR_INTR_REG);
	softsp->intr_retry_reg = REG_ADDR(address, OFF_INTR_RETRY_REG);
	softsp->sbus_intr_state = REG_ADDR(address, OFF_SBUS_INTR_STATE_REG);
	softsp->sbus_pcr = REG_ADDR(address, OFF_SBUS_PCR);
	softsp->sbus_pic = REG_ADDR(address, OFF_SBUS_PIC);

#undef	REG_ADDR

	DPRINTF(SBUS_REGISTERS_DEBUG, ("SYSIO Control reg: 0x%x\n"
	    "SBUS Control reg: 0x%x", softsp->sysio_ctrl_reg,
	    softsp->sbus_ctrl_reg));

#ifdef _STARFIRE
	/* Setup interrupt target translation for starfire */
	pc_ittrans_init(softsp->upa_id, &softsp->ittrans_cookie);
#endif _STARFIRE

	softsp->intr_mapping_ign =
	    UPAID_TO_IGN(softsp->upa_id) << IMR_IGN_SHIFT;

	/* Diag reg 2 is the next 64 bit word after diag reg 1 */
	softsp->obio_intr_state = softsp->sbus_intr_state + 1;

	(void) sbus_resume_init(softsp, 0);

	/*
	 * Set the initial burstsizes for each slot to all 1's.  This will
	 * get changed at initchild time.
	 */
	for (i = 0; i < MAX_SBUS_SLOTS; i++)
		softsp->sbus_slave_burstsizes[i] = 0xffffffffu;

	/*
	 * Since SYSIO is used as an interrupt mastering device for slave
	 * only UPA devices, we call a dedicated kernel function to register
	 * The address of the interrupt mapping register for the slave device.
	 *
	 * If RISC/sysio is wired to support 2 upa slave interrupt
	 * devices then register 2nd mapping register with system.
	 * The slave/proxy portid algorithm (decribed in Fusion Desktop Spec)
	 * allows for upto 3 slaves per proxy but Psycho/SYSIO only support 2.
	 *
	 * #upa-interrupt-proxies property defines how many UPA interrupt
	 * slaves a bridge is wired to support. Older systems that lack
	 * this property will default to 1.
	 */
	numproxy = ddi_prop_get_int(DDI_DEV_T_ANY, softsp->dip,
	    DDI_PROP_DONTPASS, "#upa-interrupt-proxies", 1);

	if (numproxy > 0)
		set_intr_mapping_reg(softsp->upa_id,
		    (uint64_t *)(softsp->intr_mapping_reg +
		    FFB_MAPPING_REG), 1);

	if (numproxy > 1)
		set_intr_mapping_reg(softsp->upa_id,
		    (uint64_t *)(softsp->intr_mapping_reg +
		    EXP_MAPPING_REG), 2);

	/* support for a 3 interrupt proxy would go here */

	/* Turn on spurious interrupt counter if we're not a DEBUG kernel. */
#ifndef DEBUG
	intr_cntr_on = 1;
#else
	intr_cntr_on = 0;
#endif


	return (DDI_SUCCESS);
}

/*
 * This procedure is part of sbus initialization. It is called by
 * sbus_init() and is invoked when the system is being resumed.
 */
static int
sbus_resume_init(struct sbus_soft_state *softsp, int resume)
{
	int i;
	uint_t sbus_burst_sizes;

	/*
	 * This shouldn't be needed when we have a real OBP PROM.
	 * (RAZ) Get rid of this later!!!
	 */

#ifdef _STARFIRE
	/*
	 * For Starfire, we need to program a
	 * constant odd value.
	 * Zero out the MID field before ORing
	 * We leave the LSB of the MID field intact since
	 * we cannot have a zero(even) MID value
	 */
	uint64_t tmpconst = 0x1DULL;
	*softsp->sysio_ctrl_reg &= 0xFF0FFFFFFFFFFFFFULL;
	*softsp->sysio_ctrl_reg |= tmpconst << 51;

	/*
	 * Program in the interrupt group number
	 * Here we have to convert the starfire
	 * 7 bit upaid into a 5bit value.
	 */
	*softsp->sysio_ctrl_reg |=
		(uint64_t)STARFIRE_UPAID2HWIGN(softsp->upa_id)
				<< SYSIO_IGN;
#else
	/* for the rest of sun4u's */
	*softsp->sysio_ctrl_reg |=
		(uint64_t)softsp->upa_id << 51;

	/* Program in the interrupt group number */
	*softsp->sysio_ctrl_reg |=
		(uint64_t)softsp->upa_id << SYSIO_IGN;
#endif /* _STARFIRE */

	/*
	 * Set appropriate fields of sbus control register.
	 * Set DVMA arbitration enable for all devices.
	 */
	*softsp->sbus_ctrl_reg |= SBUS_ARBIT_ALL;

	/* Calculate our burstsizes now so we don't have to do it later */
	sbus_burst_sizes = (SYSIO64_BURST_RANGE << SYSIO64_BURST_SHIFT)
		| SYSIO_BURST_RANGE;

	sbus_burst_sizes = ddi_getprop(DDI_DEV_T_ANY, softsp->dip,
		DDI_PROP_DONTPASS, "up-burst-sizes", sbus_burst_sizes);

	softsp->sbus_burst_sizes = sbus_burst_sizes & SYSIO_BURST_MASK;
	softsp->sbus64_burst_sizes = sbus_burst_sizes & SYSIO64_BURST_MASK;

	if (!resume) {
		/* Set burstsizes to smallest value */
		for (i = 0; i < MAX_SBUS_SLOTS; i++) {
			volatile uint64_t *config;
			uint64_t tmpreg;

			config = softsp->sbus_slot_config_reg + i;

			/* Write out the burst size */
			tmpreg = (uint64_t)0;
			*config = tmpreg;

			/* Flush any write buffers */
			tmpreg = *softsp->sbus_ctrl_reg;

			DPRINTF(SBUS_REGISTERS_DEBUG, ("Sbus slot 0x%x slot "
			    "configuration reg: 0x%x", (i > 3) ? i + 9 : i,
			    config));
		}
	} else {
		/* Program the slot configuration registers */
		for (i = 0; i < MAX_SBUS_SLOTS; i++) {
			volatile uint64_t *config;
#ifndef lint
			uint64_t tmpreg;
#endif /* !lint */
			uint_t slave_burstsizes;

			slave_burstsizes = 0;
			if (softsp->sbus_slave_burstsizes[i] != 0xffffffffu) {
				config = softsp->sbus_slot_config_reg + i;

				if (softsp->sbus_slave_burstsizes[i] &
				    SYSIO64_BURST_MASK) {
					/* get the 64 bit burstsizes */
					slave_burstsizes =
					    softsp->sbus_slave_burstsizes[i] >>
					    SYSIO64_BURST_SHIFT;

					/* Turn on 64 bit PIO's on the sbus */
					*config |= SBUS_ETM;
				} else {
					slave_burstsizes =
					    softsp->sbus_slave_burstsizes[i] &
					    SYSIO_BURST_MASK;
				}

				/* Get burstsizes into sysio register format */
				slave_burstsizes >>= SYSIO_SLAVEBURST_REGSHIFT;

				/* Program the burstsizes */
				*config |= (uint64_t)slave_burstsizes;

				/* Flush any write buffers */
#ifndef lint
				tmpreg = *softsp->sbus_ctrl_reg;
#endif /* !lint */
			}
		}
	}

	return (DDI_SUCCESS);
}

#define	get_prop(di, pname, flag, pval, plen)	\
	(ddi_prop_op(DDI_DEV_T_NONE, di, PROP_LEN_AND_VAL_ALLOC, \
	flag | DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, \
	pname, (caddr_t)pval, plen))

struct prop_ispec {
	uint_t	pri, vec;
};

/*
 * Create a sysio_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and either an "intr" or "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is assumed to be a list of at least one triple
 *
 *	<bustype, address, size>*1
 *
 * On pre-fusion machines, the "intr" property was the IPL for the system.
 * Most new sbus devices post an "interrupts" property that corresponds to
 * a particular bus level.  All devices on fusion using an "intr" property
 * will have it's contents translated into a bus level.  Hence, "intr" and
 * "interrupts on the fusion platform can be treated the same.
 *
 * The "interrupts" property is assumed to be a list of at least one
 * n-tuples that describes the interrupt capabilities of the bus the device
 * is connected to.  For SBus, this looks like
 *
 *	<SBus-level>*1
 *
 * (This property obsoletes the 'intr' property).
 *
 * The OBP_RANGES property is optional.
 */
static int
make_sbus_ppd(dev_info_t *child)
{
	struct sysio_parent_private_data *pdptr;
	int n;
	int *reg_prop, *rgstr_prop, *rng_prop;
	int reg_len, rgstr_len, rng_len;
	int has_registers = 0;

	pdptr = (struct sysio_parent_private_data *)
			kmem_zalloc(sizeof (*pdptr), KM_SLEEP);
	ddi_set_parent_data(child, (caddr_t)pdptr);

	/*
	 * Handle the 'reg'/'registers' properties.
	 * "registers" overrides "reg", but requires that "reg" be exported,
	 * so we can handle wildcard specifiers.  "registers" implies an
	 * sbus style device.  "registers" implies that we insert the
	 * correct value in the regspec_bustype field of each spec for a real
	 * (non-pseudo) device node.  "registers" is a s/w only property, so
	 * we inhibit the prom search for this property.
	 */
	if (get_prop(child, OBP_REG, 0, &reg_prop, &reg_len) != DDI_SUCCESS)
		reg_len = 0;

	/*
	 * Save the underlying slot number and slot offset.
	 * Among other things, we use these to name the child node.
	 */
	pdptr->slot = (uint_t)-1;
	if (reg_len != 0) {
		pdptr->slot = ((struct regspec *)reg_prop)->regspec_bustype;
		pdptr->offset = ((struct regspec *)reg_prop)->regspec_addr;
	}

	rgstr_len = 0;
	(void) get_prop(child, "registers", DDI_PROP_NOTPROM,
	    &rgstr_prop, &rgstr_len);

	if (rgstr_len != 0)  {

		if (ndi_dev_is_persistent_node(child) && (reg_len != 0))  {

			/*
			 * Convert wildcard "registers" for a real node...
			 * (Else, this is the wildcard prototype node)
			 */
			struct regspec *rp = (struct regspec *)reg_prop;
			uint_t slot = rp->regspec_bustype;
			int i;

			rp = (struct regspec *)rgstr_prop;
			n = rgstr_len / sizeof (struct regspec);
			for (i = 0; i < n; ++i, ++rp)
				rp->regspec_bustype = slot;
		}

		if (reg_len != 0)
			kmem_free(reg_prop, reg_len);

		reg_prop = rgstr_prop;
		reg_len = rgstr_len;
		++has_registers;
	}
	if ((n = reg_len) != 0)  {
		pdptr->par_nreg = n / (int)sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * See if I have ranges.
	 */
	if (get_prop(child, OBP_RANGES, 0, &rng_prop, &rng_len) ==
	    DDI_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}

	return (has_registers);
}

/*
 * Special handling for "sbusmem" pseudo device nodes.
 * The special handling automatically creates the "reg"
 * property in the sbusmem nodes, based on the parent's
 * property so that each slot will automtically have a
 * correctly sized "reg" property, once created,
 * sbus_initchild does the rest of the work to init
 * the child node.
 */
static int
sbusmem_initchild(dev_info_t *dip, dev_info_t *child)
{
	int i, n;
	int slot, size;
	char ident[10];

	slot = ddi_getprop(DDI_DEV_T_NONE, child,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "slot", -1);
	if (slot == -1) {
		DPRINTF(SBUS_SBUSMEM_DEBUG, ("can't get slot property\n"));
		return (DDI_FAILURE);
	}

	/*
	 * Find the parent range corresponding to this "slot",
	 * so we can set the size of the child's "reg" property.
	 */
	for (i = 0, n = sparc_pd_getnrng(dip); i < n; i++) {
		struct rangespec *rp = sparc_pd_getrng(dip, i);

		if (rp->rng_cbustype == (uint_t)slot) {
			struct regspec r;

			/* create reg property */

			r.regspec_bustype = (uint_t)slot;
			r.regspec_addr = 0;
			r.regspec_size = rp->rng_size;
			(void) ddi_prop_create(DDI_DEV_T_NONE,
			    child, DDI_PROP_CANSLEEP, "reg",
			    (caddr_t)&r,
			    sizeof (struct regspec));

			/* create size property for slot */

			size = rp->rng_size;
			(void) ddi_prop_create(DDI_DEV_T_NONE,
			    child, DDI_PROP_CANSLEEP, "size",
			    (caddr_t)&size, sizeof (int));

			(void) sprintf(ident, "slot%x", slot);
			(void) ddi_prop_create(DDI_DEV_T_NONE,
			    child, DDI_PROP_CANSLEEP, "ident",
			    ident, sizeof (ident));

			return (DDI_SUCCESS);
		}
	}
	return (DDI_FAILURE);
}

/*
 * Called from the bus_ctl op of sysio sbus nexus driver
 * to implement the DDI_CTLOPS_INITCHILD operation.  That is, it names
 * the children of sysio sbusses based on the reg spec.
 *
 * Handles the following properties:
 *
 *	Property		value
 *	  Name			type
 *
 *	reg		register spec
 *	registers	wildcard s/w sbus register spec (.conf file property)
 *	intr		old-form interrupt spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 */
static int
sbus_initchild(dev_info_t *dip, dev_info_t *child)
{
	int rv, has_registers;
	char name[MAXNAMELEN];
	ulong_t slave_burstsizes;
	int slot;
	volatile uint64_t *slot_reg;
#ifndef lint
	uint64_t tmp;
#endif /* !lint */
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    ddi_get_soft_state(sbusp, ddi_get_instance(dip));
	extern int impl_ddi_merge_child(dev_info_t *child);
	extern int impl_ddi_merge_wildcard(dev_info_t *child);

	if (strcmp(ddi_get_name(child), "sbusmem") == 0) {
		if (sbusmem_initchild(dip, child) != DDI_SUCCESS)
			return (DDI_FAILURE);
	}

	/*
	 * Fill in parent-private data and note an indication if the
	 * "registers" property was used to fill in the data.
	 */
	has_registers = make_sbus_ppd(child);

	/*
	 * If this is a s/w node defined with the "registers" property,
	 * this means that this is a wildcard specifier, whose properties
	 * get applied to all previously defined h/w nodes with the same
	 * name and same parent.
	 */
	if ((has_registers) && (ndi_dev_is_persistent_node(child) == 0))
		return (impl_ddi_merge_wildcard(child));

	/*
	 * Name the device node using the underlying (prom) values
	 * of the first entry in the "reg" property.  For SBus devices,
	 * the textual form of the name is <name>@<slot#>,<offset>.
	 * This must match the prom's pathname or mountroot, etc, won't
	 * work.
	 */
	name[0] = '\0';
	if (sysio_pd_getslot(child) != (uint_t)-1)
		(void) sprintf(name, "%x,%x", sysio_pd_getslot(child),
		    sysio_pd_getoffset(child));
	ddi_set_name_addr(child, name);

	/*
	 * If a pseudo node, attempt to merge it into a hw node,
	 * if merged, returns an indication that this node should
	 * be removed (after the caller uninitializes it).
	 */
	if ((rv = impl_ddi_merge_child(child)) != DDI_SUCCESS)
		return (rv);

	/* Figure out the child devices slot number */
	slot = sysio_pd_getslot(child);

	/* If we don't have a reg property, bypass slot specific programming */
	if (slot < 0 || slot >= MAX_SBUS_SLOT_ADDR) {
#ifdef DEBUG
		cmn_err(CE_WARN, "?Invalid sbus slot address 0x%x for %s "
		    "device\n", slot, ddi_get_name(child));
#endif /* DEBUG */
		goto done;
	}

	/* Modify the onboard slot numbers if applicable. */
	slot = (slot > 3) ? slot - 9 : slot;

	/* Get the slot configuration register for the child device. */
	slot_reg = softsp->sbus_slot_config_reg + slot;

	/*
	 * Program the devices slot configuration register for the
	 * appropriate slave burstsizes.
	 * The upper 16 bits of the slave-burst-sizes are for 64 bit sbus
	 * and the lower 16 bits are the burst sizes for 32 bit sbus. If
	 * we see that a device supports both 64 bit and 32 bit slave accesses,
	 * we default to 64 bit and turn it on in the slot config reg.
	 *
	 * For older devices, make sure we check the "burst-sizes" property
	 * too.
	 */
	if ((slave_burstsizes = (ulong_t)ddi_getprop(DDI_DEV_T_ANY, child,
	    DDI_PROP_DONTPASS, "slave-burst-sizes", 0)) != 0 ||
	    (slave_burstsizes = (ulong_t)ddi_getprop(DDI_DEV_T_ANY, child,
	    DDI_PROP_DONTPASS, "burst-sizes", 0)) != 0) {
		uint_t burstsizes = 0;

		/*
		 * If we only have 32 bit burst sizes from a previous device,
		 * mask out any burstsizes for 64 bit mode.
		 */
		if (((softsp->sbus_slave_burstsizes[slot] &
		    0xffff0000u) == 0) &&
		    ((softsp->sbus_slave_burstsizes[slot] & 0xffff) != 0)) {
			slave_burstsizes &= 0xffff;
		}

		/*
		 * If "slave-burst-sizes was defined but we have 0 at this
		 * point, we must have had 64 bit burstsizes, however a prior
		 * device can only burst in 32 bit mode.  Therefore, we leave
		 * the burstsizes in the 32 bit mode and disregard the 64 bit.
		 */
		if (slave_burstsizes == 0)
			goto done;

		/*
		 * We and in the new burst sizes with that of prior devices.
		 * This ensures that we always take the least common
		 * denominator of the burst sizes.
		 */
		softsp->sbus_slave_burstsizes[slot] &=
		    (slave_burstsizes &
		    ((SYSIO64_SLAVEBURST_RANGE <<
		    SYSIO64_BURST_SHIFT) |
		    SYSIO_SLAVEBURST_RANGE));

		/* Get the 64 bit burstsizes. */
		if (softsp->sbus_slave_burstsizes[slot] &
		    SYSIO64_BURST_MASK) {
			/* get the 64 bit burstsizes */
			burstsizes = softsp->sbus_slave_burstsizes[slot] >>
			    SYSIO64_BURST_SHIFT;

			/* Turn on 64 bit PIO's on the sbus */
			*slot_reg |= SBUS_ETM;
		} else {
			/* Turn off 64 bit PIO's on the sbus */
			*slot_reg &= ~SBUS_ETM;

			/* Get the 32 bit burstsizes if we don't have 64 bit. */
			if (softsp->sbus_slave_burstsizes[slot] &
			    SYSIO_BURST_MASK) {
				burstsizes =
				    softsp->sbus_slave_burstsizes[slot] &
				    SYSIO_BURST_MASK;
			}
		}

		/* Get the burstsizes into sysio register format */
		burstsizes >>= SYSIO_SLAVEBURST_REGSHIFT;

		/* Reset reg in case we're scaling back */
		*slot_reg &= (uint64_t)~SYSIO_SLAVEBURST_MASK;

		/* Program the burstsizes */
		*slot_reg |= (uint64_t)burstsizes;

		/* Flush system load/store buffers */
#ifndef lint
		tmp = *slot_reg;
#endif /* !lint */
	}

done:
	return (DDI_SUCCESS);
}

static int
sbus_uninitchild(dev_info_t *dip)
{
	struct sysio_parent_private_data *pdptr;
	size_t n;

	if ((pdptr = (struct sysio_parent_private_data *)
	    ddi_get_parent_data(dip)) != NULL)  {
		if ((n = (size_t)pdptr->par_nrng) != 0)
			kmem_free(pdptr->par_rng, n *
			    sizeof (struct rangespec));

		if ((n = pdptr->par_nreg) != 0)
			kmem_free(pdptr->par_reg, n * sizeof (struct regspec));

		kmem_free(pdptr, sizeof (*pdptr));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);
	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);
	return (DDI_SUCCESS);
}

static int
sbus_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t op, void *arg, void *result)
{
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
		ddi_get_soft_state(sbusp, ddi_get_instance(dip));

	switch (op) {

	case DDI_CTLOPS_INITCHILD:
		return (sbus_initchild(dip, (dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		return (sbus_uninitchild(arg));

	case DDI_CTLOPS_IOMIN: {
		int val = *((int *)result);

		/*
		 * The 'arg' value of nonzero indicates 'streaming' mode.
		 * If in streaming mode, pick the largest of our burstsizes
		 * available and say that that is our minimum value (modulo
		 * what mincycle is).
		 */
		if ((int)arg)
			val = maxbit(val,
			    (1 << (ddi_fls(softsp->sbus_burst_sizes) - 1)));
		else
			val = maxbit(val,
			    (1 << (ddi_ffs(softsp->sbus_burst_sizes) - 1)));

		*((int *)result) = val;
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}

	case DDI_CTLOPS_REPORTDEV: {
		dev_info_t *pdev;
		int i, n, len, f_len;
		char *msgbuf;

	/*
	 * So we can do one atomic cmn_err call, we allocate a 4k
	 * buffer, and format the reportdev message into that buffer,
	 * send it to cmn_err, and then free the allocated buffer.
	 * If message is longer than 1k, the message is truncated and
	 * an error message is emitted (debug kernel only).
	 */
#define	REPORTDEV_BUFSIZE	1024

		int sbusid = ddi_get_instance(dip);

		if (ddi_get_parent_data(rdip) == NULL)
			return (DDI_FAILURE);

		msgbuf = kmem_zalloc(REPORTDEV_BUFSIZE, KM_SLEEP);

		pdev = ddi_get_parent(rdip);
		f_len = snprintf(msgbuf, REPORTDEV_BUFSIZE,
		    "%s%d at %s%d: SBus%d ",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    ddi_driver_name(pdev), ddi_get_instance(pdev), sbusid);
		len = strlen(msgbuf);

		for (i = 0, n = sysio_pd_getnreg(rdip); i < n; i++) {
			struct regspec *rp;

			rp = sysio_pd_getreg(rdip, i);
			if (i != 0) {
				f_len += snprintf(msgbuf + len,
				    REPORTDEV_BUFSIZE - len, " and ");
				len = strlen(msgbuf);
			}

			f_len += snprintf(msgbuf + len, REPORTDEV_BUFSIZE - len,
			    "slot 0x%x offset 0x%x",
			    rp->regspec_bustype, rp->regspec_addr);
			len = strlen(msgbuf);
		}

		for (i = 0, n = i_ddi_get_nintrs(rdip); i < n; i++) {
			ddi_ispec_t *ispecp;
			uint32_t sbuslevel, pri;

			if (i != 0) {
				f_len += snprintf(msgbuf + len,
				    REPORTDEV_BUFSIZE - len, ",");
				len = strlen(msgbuf);
			}

			i_ddi_alloc_ispec(rdip, i,
			    (ddi_intrspec_t *)&ispecp);

			sbuslevel = *ispecp->is_intr;

			(void) sbus_xlate_intrs(dip, rdip, ispecp,
			    softsp->intr_mapping_ign);

			pri = ispecp->is_pil;

			if (sbuslevel > MAX_SBUS_LEVEL)
				f_len += snprintf(msgbuf + len,
				    REPORTDEV_BUFSIZE - len,
				    " Onboard device ");
			else
				f_len += snprintf(msgbuf + len,
				    REPORTDEV_BUFSIZE - len, " SBus level %d ",
				    sbuslevel);
			len = strlen(msgbuf);

			f_len += snprintf(msgbuf + len, REPORTDEV_BUFSIZE - len,
			    "sparc9 ipl %d", pri);
			len = strlen(msgbuf);

			i_ddi_free_ispec((ddi_intrspec_t)ispecp);
		}
#ifdef DEBUG
	if (f_len + 1 >= REPORTDEV_BUFSIZE) {
		cmn_err(CE_NOTE, "next message is truncated: "
		    "printed length 1024, real length %d", f_len);
	}
#endif DEBUG

		cmn_err(CE_CONT, "?%s\n", msgbuf);
		kmem_free(msgbuf, REPORTDEV_BUFSIZE);
		return (DDI_SUCCESS);

#undef	REPORTDEV_BUFSIZE
	}

	case DDI_CTLOPS_SLAVEONLY:
		return (DDI_FAILURE);

	case DDI_CTLOPS_AFFINITY: {
		dev_info_t *dipb = (dev_info_t *)arg;
		int r_slot, b_slot;

		if ((b_slot = find_sbus_slot(dip, dipb)) < 0)
			return (DDI_FAILURE);

		if ((r_slot = find_sbus_slot(dip, rdip)) < 0)
			return (DDI_FAILURE);

		return ((b_slot == r_slot)? DDI_SUCCESS : DDI_FAILURE);

	}
	case DDI_CTLOPS_DMAPMAPC:
		cmn_err(CE_CONT, "?DDI_DMAPMAPC called!!\n");
		return (DDI_FAILURE);

	case DDI_CTLOPS_POKE_INIT: {
		ddi_nofault_data_t *nofault_data = arg;

		mutex_enter(&softsp->pokefault_mutex);
		if (nofault_data && nofault_data->op_type == POKE_START) {
			softsp->nofault_data = nofault_data;
			return (DDI_SUCCESS);
		}

		mutex_exit(&softsp->pokefault_mutex);
		return (DDI_FAILURE);

	}

	case DDI_CTLOPS_POKE_FLUSH: {
		volatile uint64_t tmpreg;
		ddi_nofault_data_t *nofault_data = softsp->nofault_data;

		/* Flush any sbus store buffers. */
		tmpreg = *softsp->sbus_ctrl_reg;

		/*
		 * Read the sbus error reg and see if a fault occured.  If
		 * one has, give the SYSIO time to packetize the interrupt
		 * for the fault and send it out.  The sbus error handler will
		 * 0 these fields when it's called to service the fault.
		 */
		tmpreg = *softsp->sbus_err_reg;
		while (tmpreg & SB_AFSR_P_TO || tmpreg & SB_AFSR_P_BERR)
			tmpreg = *softsp->sbus_err_reg;

		ASSERT(nofault_data != 0);
		return (nofault_data->op_type & POKE_FAULT
		    ? DDI_FAILURE : DDI_SUCCESS);
	}
	case DDI_CTLOPS_POKE_FINI: {

		softsp->nofault_data = (ddi_nofault_data_t *)0;
		mutex_exit(&softsp->pokefault_mutex);
		return (DDI_SUCCESS);
	}
	case DDI_CTLOPS_DVMAPAGESIZE:
		*(ulong_t *)result = IOMMU_PAGESIZE;
		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}

static int
find_sbus_slot(dev_info_t *dip, dev_info_t *rdip)
{
	dev_info_t *child;
	int slot = -1;

	/*
	 * look for the node that's a direct child of this Sbus node.
	 */
	while (rdip && (child = ddi_get_parent(rdip)) != dip) {
		rdip = child;
	}

	/*
	 * If there is one, get the slot number of *my* child
	 */
	if (child == dip)
		slot = sysio_pd_getslot(rdip);

	return (slot);
}

/*
 * This is the sbus interrupt routine wrapper function.  This function
 * installs itself as a child devices interrupt handler.  It's function is
 * to dispatch a child devices interrupt handler, and then
 * reset the interrupt clear register for the child device.
 *
 * Warning: This routine may need to be implemented as an assembly level
 * routine to improve performance.
 */

#define	MAX_INTR_CNT 10

static uint_t
sbus_intr_wrapper(caddr_t arg)
{
	uint_t intr_return = DDI_INTR_UNCLAIMED;
	volatile uint64_t tmpreg;
	struct sbus_wrapper_arg *intr_info;
	struct sbus_intr_handler *intr_handler;
	uchar_t *spurious_cntr;

	intr_info = (struct sbus_wrapper_arg *)arg;
	spurious_cntr = &intr_info->softsp->spurious_cntrs[intr_info->pil];
	intr_handler = intr_info->handler_list;

	while (intr_handler) {
		intr_return |= (*intr_handler->funcp)(intr_handler->arg);
		intr_handler = intr_handler->next;
	}

	/* Set the interrupt state machine to idle */
	tmpreg = *intr_info->softsp->sbus_ctrl_reg;
	tmpreg = SBUS_INTR_IDLE;
	*intr_info->clear_reg = tmpreg;
	tmpreg = *intr_info->softsp->sbus_ctrl_reg;

	if (intr_return == DDI_INTR_UNCLAIMED) {
		(*spurious_cntr)++;

		if (*spurious_cntr < MAX_INTR_CNT) {
			if (intr_cntr_on)
				return (DDI_INTR_CLAIMED);
		}
#ifdef DEBUG
		else if (intr_info->pil >= LOCK_LEVEL) {
			cmn_err(CE_PANIC, "%d unclaimed interrupts at "
			    "interrupt level %d\n", MAX_INTR_CNT,
			    intr_info->pil);
		}
#endif

		/*
		 * Reset spurious counter once we acknowledge
		 * it to the system level.
		 */
		*spurious_cntr = (uchar_t)0;
	} else {
		*spurious_cntr = (uchar_t)0;
	}

	return (intr_return);
}

/*
 * add_intrspec - Add an interrupt specification.
 */
static int
sbus_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    ddi_get_soft_state(sbusp, ddi_get_instance(dip));
	ddi_ispec_t *ispecp = (ddi_ispec_t *)intr_info->ii_ispec;
	volatile uint64_t *mondo_vec_reg;
	volatile uint64_t tmp_mondo_vec;
	volatile uint64_t *intr_state_reg;
	volatile uint64_t	tmpreg;	/* HW flush reg */
	uint_t start_bit;
	int ino;
	uint_t cpu_id;
	struct sbus_wrapper_arg *sbus_arg;
	struct sbus_intr_handler *intr_handler;
	uint32_t slot, pil;
	/* Interrupt state machine reset flag */
	int reset_ism_register = 1;
	int ret = DDI_SUCCESS;

	/* Check if we have a valid sbus slot address */
	if (((slot = (uint_t)find_sbus_slot(dip, rdip)) >=
	    MAX_SBUS_SLOT_ADDR) || (slot < (uint_t)0)) {
		cmn_err(CE_WARN, "Invalid sbus slot 0x%x during add intr\n",
		    slot);
		return (DDI_FAILURE);
	}

	DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr: sbus interrupt %d for "
	    "device %s%d\n", *ispecp->is_intr, ddi_get_name(rdip),
	    ddi_get_instance(rdip)));

	/* Xlate the interrupt */
	if (sbus_xlate_intrs(dip, rdip, ispecp,
	    softsp->intr_mapping_ign) == DDI_FAILURE) {
		cmn_err(CE_WARN, "Can't xlate SBUS devices %s intyerrupt.\n",
		    ddi_get_name(rdip));
		return (DDI_FAILURE);
	}

	/* get the ino number */
	ino = (*ispecp->is_intr) & SBUS_MAX_INO;
	mondo_vec_reg = (softsp->intr_mapping_reg +
	    ino_table[ino]->mapping_reg);

	/*
	 * This is an intermediate step in identifying
	 * the exact bits which represent the device in the interrupt
	 * state diagnostic register.
	 */

	if (ino > MAX_MONDO_EXTERNAL) {
		start_bit = ino_table[ino]->diagreg_shift;
		intr_state_reg = softsp->obio_intr_state;
	} else {
		start_bit = 16 * (ino >> 3) + 2 * (ino & 0x7);
		intr_state_reg = softsp->sbus_intr_state;
	}


	/* Allocate a nexus interrupt data structure */
	intr_handler = kmem_zalloc(sizeof (struct sbus_intr_handler),
	    KM_SLEEP);
	intr_handler->dip = rdip;
	intr_handler->funcp = intr_info->ii_int_handler;
	intr_handler->arg = intr_info->ii_int_handler_arg;

	DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr: xlated interrupt 0x%x "
	    "intr_handler 0x%x\n", *ispecp->is_intr, intr_handler));
	/*
	 * Grab the system global interrupt distribution lock.
	 * It must always be held around the poll list lock.
	 */
	mutex_enter(&intr_dist_lock);

	/*
	 * Grab this lock here. So it will protect the poll list and
	 */
	mutex_enter(&softsp->intr_poll_list_lock);


	sbus_arg = softsp->intr_list[ino];
	/* Check if we have a poll list to deal with */
	if (sbus_arg) {
		tmp_mondo_vec = *mondo_vec_reg;
		tmp_mondo_vec &= ~INTERRUPT_VALID;
		*mondo_vec_reg = tmp_mondo_vec;

		tmpreg = *softsp->sbus_ctrl_reg;
#ifdef	lint
		tmpreg = tmpreg;
#endif

		DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr:sbus_arg exists "
		    "0x%x\n", sbus_arg));
		/*
		 * Two bits per ino in the diagnostic register
		 * indicate the status of its interrupt.
		 * 0 - idle, 1 - transmit, 3 - pending.
		 */
		while (((*intr_state_reg >>
		    start_bit) & 0x3) == INT_PENDING);

		/* Extract the PIL */
		pil = sbus_arg->pil;

		intr_handler->next = sbus_arg->handler_list;
		sbus_arg->handler_list = intr_handler;

		reset_ism_register = 0;

	} else {
		int mask_flag;

		sbus_arg = kmem_zalloc(sizeof (struct sbus_wrapper_arg),
		    KM_SLEEP);

		softsp->intr_list[ino] = sbus_arg;
		sbus_arg->clear_reg = (softsp->clr_intr_reg +
		    ino_table[ino]->clear_reg);
		DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr:Ino 0x%x Interrupt "
		    "clear reg: 0x%x\n", ino, sbus_arg->clear_reg));
		sbus_arg->softsp = softsp;
		sbus_arg->handler_list = intr_handler;

		/*
		 * No handler added yet in the interrupt vector
		 * table for this ino.
		 * Install the nexus interrupt wrapper in the
		 * system. The wrapper will call the device
		 * interrupt handler.
		 */

		intr_info->ii_int_handler = sbus_intr_wrapper;
		intr_info->ii_int_handler_arg = (caddr_t)sbus_arg;

		if ((ret = i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_ADD,
		    (void *)intr_info, (void *) NULL)) != DDI_SUCCESS) {
			kmem_free(sbus_arg, sizeof (struct sbus_wrapper_arg));
			kmem_free(intr_handler,
			    sizeof (struct sbus_intr_handler));
			mutex_exit(&softsp->intr_poll_list_lock);
			goto done;
		}

		if (slot < EXT_SBUS_SLOTS) {
			mask_flag = 1;
		} else {
			mask_flag = 0;
		}

		if ((slot >= EXT_SBUS_SLOTS) ||
		    (softsp->intr_hndlr_cnt[slot] == 0)) {
			cpu_id = intr_add_cpu(sbus_intrdist, (void *)dip,
			    (int)*ispecp->is_intr, mask_flag);
#ifdef	_STARFIRE
			tmp_mondo_vec = pc_translate_tgtid(
			    softsp->ittrans_cookie, cpu_id,
				mondo_vec_reg) << IMR_TID_SHIFT;
#else
			tmp_mondo_vec =
			    cpu_id << IMR_TID_SHIFT;
			DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr: initial "
			    "mapping reg 0x%llx\n", tmp_mondo_vec));
#endif	/* _STARFIRE */
		} else {
			/*
			 * There is already a different
			 * ino programmed at this IMR.
			 * Just read the IMR out to get the
			 * correct MID target.
			 */
			tmp_mondo_vec = *mondo_vec_reg;
			tmp_mondo_vec &= ~INTERRUPT_VALID;
			*mondo_vec_reg = tmp_mondo_vec;
			DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr: existing "
			    "mapping reg 0x%llx\n", tmp_mondo_vec));
		}

		pil = sbus_arg->pil = ispecp->is_pil;

		DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr:Alloc sbus_arg "
		    "0x%x\n", sbus_arg));
	}

	softsp->intr_hndlr_cnt[slot]++;

	mutex_exit(&softsp->intr_poll_list_lock);

	/* Program the iblock cookie */
	if (intr_info->ii_iblock_cookiep) {
		*intr_info->ii_iblock_cookiep = (ddi_iblock_cookie_t)pil;
	}

	/* Program the device cookie */
	if (intr_info->ii_idevice_cookiep) {
		intr_info->ii_idevice_cookiep->idev_vector = 0;
		/*
		 * The idevice cookie contains the priority as
		 * understood by the device itself on the bus it
		 * lives on.  Let the nexi beneath sort out the
		 * translation (if any) that's needed.
		 */
		intr_info->ii_idevice_cookiep->idev_priority =
		    (ushort_t)*ispecp->is_intr;
	}

	/*
	 * Program the ino vector accordingly.  This MUST be the
	 * last thing we do.  Once we program the ino, the device
	 * may begin to interrupt. Add this hardware interrupt to
	 * the interrupt lists, and get the CPU to target it at.
	 */

	tmp_mondo_vec |= INTERRUPT_VALID;

	DPRINTF(SBUS_INTERRUPT_DEBUG, ("Add intr: Ino 0x%x mapping reg: 0x%x "
	    "Intr cntr %d\n", ino, mondo_vec_reg,
	    softsp->intr_hndlr_cnt[slot]));

	/* Force the interrupt state machine to idle. */
	if (reset_ism_register) {
		tmpreg = SBUS_INTR_IDLE;
		*sbus_arg->clear_reg = tmpreg;
	}

	/* Store it in the hardware reg. */
	*mondo_vec_reg = tmp_mondo_vec;

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;

done:
	/*
	 * Unlock the system global interrupt distribution list
	 * lock.
	 */
	mutex_exit(&intr_dist_lock);

	return (ret);
}

static void
sbus_free_handler(dev_info_t *dip, uint32_t inum,
    struct sbus_wrapper_arg *sbus_arg)
{
	struct sbus_intr_handler *listp, *prevp;

	if (sbus_arg) {
		prevp = 0;
		listp = sbus_arg->handler_list;

		while (listp) {
			if (listp->dip == dip && listp->inum == inum) {
				if (prevp)
					prevp->next = listp->next;
				else {
					prevp = listp->next;
					sbus_arg->handler_list = prevp;
				}

				kmem_free(listp,
				    sizeof (struct sbus_intr_handler));
				break;
			}
			prevp = listp;
			listp = listp->next;
		}
	}
}

/*
 * remove_intrspec - Remove an interrupt specification.
 */
/*ARGSUSED*/
static void
sbus_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	volatile uint64_t *mondo_vec_reg;
	volatile uint64_t *intr_state_reg;
#ifndef lint
	volatile uint64_t tmpreg;
#endif /* !lint */
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    ddi_get_soft_state(sbusp, ddi_get_instance(dip));
	ddi_ispec_t *ispecp = (ddi_ispec_t *)intr_info->ii_ispec;
	int start_bit, ino, slot;
	struct sbus_wrapper_arg *sbus_arg;

	/*
	 * Grab the mutex protecting the system interrupt distribution
	 * lists.
	 */
	mutex_enter(&intr_dist_lock);

	/* Grab the mutex protecting the poll list */
	mutex_enter(&softsp->intr_poll_list_lock);

	/* Xlate the interrupt */
	if (sbus_xlate_intrs(dip, rdip, ispecp,
	    softsp->intr_mapping_ign) == DDI_FAILURE) {
		cmn_err(CE_WARN, "Can't xlate SBUS devices %s intyerrupt.\n",
		    ddi_get_name(rdip));
		goto done;
	}

	ino = ((int32_t)*ispecp->is_intr) & SBUS_MAX_INO;

	mondo_vec_reg = (softsp->intr_mapping_reg +
	    ino_table[ino]->mapping_reg);

	/* Turn off the valid bit in the mapping register. */
	*mondo_vec_reg &= ~INTERRUPT_VALID;
#ifndef lint
	tmpreg = *softsp->sbus_ctrl_reg;
#endif /* !lint */

	/* Get our bit position for checking intr pending */
	if (ino > MAX_MONDO_EXTERNAL) {
		start_bit = ino_table[ino]->diagreg_shift;
		intr_state_reg = softsp->obio_intr_state;
	} else {
		start_bit = 16 * (ino >> 3) + 2 * (ino & 0x7);
		intr_state_reg = softsp->sbus_intr_state;
	}

	while (((*intr_state_reg >> start_bit) & 0x3) == INT_PENDING);

	slot = find_sbus_slot(dip, rdip);

	/* Return if the slot is invalid */
	if (slot >= MAX_SBUS_SLOT_ADDR || slot < 0) {
		goto done;
	}

	sbus_arg = softsp->intr_list[ino];

	/* Decrement the intr handler count on this slot */
	softsp->intr_hndlr_cnt[slot]--;

	DPRINTF(SBUS_INTERRUPT_DEBUG, ("Rem intr: Softsp 0x%x, Mondo 0x%x, "
	    "ino 0x%x, sbus_arg 0x%x intr cntr %d\n", softsp,
	    *ispecp->is_intr, ino, sbus_arg, softsp->intr_hndlr_cnt[slot]));

	ASSERT(sbus_arg != 0);
	sbus_free_handler(rdip, intr_info->ii_inum, sbus_arg);

	/* If we still have a list, we're done. */
	if (sbus_arg->handler_list == 0) {
		(void) i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_REMOVE,
		    (void *)intr_info, (void *) NULL);
	}

	/*
	 * If other devices are still installed for this slot, we need to
	 * turn the valid bit back on.
	 */
	if (softsp->intr_hndlr_cnt[slot] > 0) {
		*mondo_vec_reg |= INTERRUPT_VALID;
#ifndef lint
		tmpreg = *softsp->sbus_ctrl_reg;
#endif /* !lint */
	}

	if ((softsp->intr_hndlr_cnt[slot] == 0) || (slot >= EXT_SBUS_SLOTS)) {
		intr_rem_cpu(softsp->intr_mapping_ign | ino);

#ifdef	_STARFIRE
		/* Do cleanup for interrupt target translation */
		pc_ittrans_cleanup(softsp->ittrans_cookie, mondo_vec_reg);
#endif	/* _STARFIRE */
	}


	/* Free up the memory used for the sbus interrupt handler */
	if (sbus_arg->handler_list == 0) {
		DPRINTF(SBUS_INTERRUPT_DEBUG, ("Rem intr: Freeing sbus arg "
		    "0x%x\n", sbus_arg));
		kmem_free(sbus_arg, sizeof (struct sbus_wrapper_arg));
		softsp->intr_list[ino] = NULL;
	}

done:
	mutex_exit(&softsp->intr_poll_list_lock);
	mutex_exit(&intr_dist_lock);
}

/*
 * We're prepared to claim that the interrupt string is in
 * the form of a list of <SBusintr> specifications, or we're dealing
 * with on-board devices and we have an interrupt_number property which
 * gives us our mondo number.
 * Translate the sbus levels or mondos into sysiointrspecs.
 */
static int
sbus_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, ddi_ispec_t *ispecp,
    int32_t ign)
{
	uint32_t ino, slot, level = *ispecp->is_intr;
	int ret = DDI_SUCCESS;

	/*
	 * Create the sysio ino number.  onboard devices will have
	 * an "interrupts" property, that is equal to the ino number.
	 * If the devices are from the
	 * expansion slots, we construct the ino number by putting
	 * the slot number in the upper three bits, and the sbus
	 * interrupt level in the lower three bits.
	 */
	if (level > MAX_SBUS_LEVEL) {
		ino = level;
	} else {
		/* Construct ino from slot and interrupts */
		if ((slot = find_sbus_slot(dip, rdip)) == -1) {
			cmn_err(CE_WARN, "Can't determine sbus slot "
			    "of %s device\n", ddi_get_name(rdip));
			ret = DDI_FAILURE;
			goto done;
		}

		if (slot >= MAX_SBUS_SLOT_ADDR) {
			cmn_err(CE_WARN, "Invalid sbus slot 0x%x"
			    "in %s device\n", slot, ddi_get_name(rdip));
			ret = DDI_FAILURE;
			goto done;
		}

		ino = slot << 3;
		ino |= level;
	}

	/* Sanity check the inos range */
	if (ino >= MAX_INO_TABLE_SIZE) {
		cmn_err(CE_WARN, "Ino vector 0x%x out of range", ino);
		ret = DDI_FAILURE;
		goto done;
	}
	/* Sanity check the inos value */
	if (!ino_table[ino]) {
		cmn_err(CE_WARN, "Ino vector 0x%x is invalid", ino);
		ret = DDI_FAILURE;
		goto done;
	}

	if (ispecp->is_pil == 0) {
#define	SOC_PRIORITY 5
		/* The sunfire i/o board has a soc in the printer slot */
		if ((ino_table[ino]->clear_reg == PP_CLEAR) &&
		    ((strcmp(ddi_get_name(rdip), "soc") == 0) ||
			(strcmp(ddi_get_name(rdip), "SUNW,soc") == 0))) {
			ispecp->is_pil = SOC_PRIORITY;
		} else {
			/* Figure out the pil associated with this interrupt */
			ispecp->is_pil = interrupt_priorities[ino];
		}
	}

	/* Or in the upa_id into the interrupt group number field */
	*ispecp->is_intr = (uint32_t)(ino | ign);

	DPRINTF(SBUS_INTERRUPT_DEBUG, ("Xlate intr: Interrupt info for "
	    "device %s Mondo: 0x%x, ino: 0x%x, Pil: 0x%x, sbus level: 0x%x\n",
	    ddi_get_name(rdip), *ispecp->is_intr, ino, ispecp->is_pil, level));

done:
	return (ret);

}

/* Consolidated interrupt processing interface */
int
sbus_intr_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_intr_ctlop_t op, void *arg, void *result)
{
	ddi_intr_info_t *intr_info;

	switch (op) {
	case DDI_INTR_CTLOPS_ALLOC_ISPEC: {
		uint32_t inumber = (uint32_t)arg;
		ddi_ispec_t **ispecp = result;

		i_ddi_alloc_ispec(rdip, inumber, (ddi_intrspec_t *)ispecp);

		return (DDI_SUCCESS);
	}

	case DDI_INTR_CTLOPS_FREE_ISPEC:
		i_ddi_free_ispec((ddi_intrspec_t)arg);
		return (DDI_SUCCESS);

	case DDI_INTR_CTLOPS_NINTRS:
		*(int *)result = i_ddi_get_nintrs(rdip);
		return (DDI_SUCCESS);

	default:
		break;
	}

	intr_info = (ddi_intr_info_t *)arg;

	switch (intr_info->ii_kind) {
	case IDDI_INTR_TYPE_NORMAL:
		switch (op) {
		case DDI_INTR_CTLOPS_ADD:
			return (sbus_add_intr_impl(dip, rdip, intr_info));

		case DDI_INTR_CTLOPS_REMOVE:
			sbus_remove_intr_impl(dip, rdip, intr_info);
			return (DDI_SUCCESS);

		case DDI_INTR_CTLOPS_HILEVEL: {
			struct sbus_soft_state *softsp =
			    (struct sbus_soft_state *)
			    ddi_get_soft_state(sbusp,
				ddi_get_instance(dip));
			ddi_ispec_t *ispecp =
			    (ddi_ispec_t *)intr_info->ii_ispec;

				/* Xlate the interrupt */
			if (sbus_xlate_intrs(dip, rdip, ispecp,
			    softsp->intr_mapping_ign) == DDI_FAILURE) {
				return (DDI_FAILURE);
			}

			break;
		}

		default:
			break;
		}

		break;

	default:
		/* Only support normal interrupts */
		return (DDI_FAILURE);
	}

	/* Pass up ctlops */
	return (i_ddi_intr_ctlops(dip, rdip, op, arg, result));
}


/*
 * Called by suspend/resume to save/restore the interrupt status (valid bit)
 * of the interrupt mapping registers.
 */
static void
sbus_cpr_handle_intr_map_reg(uint64_t *cpr_softsp, volatile uint64_t *baddr,
    int save)
{
	int i;
	volatile uint64_t *mondo_vec_reg;

	for (i = 0; i < MAX_INO_TABLE_SIZE; i++) {
		if (ino_table[i] != NULL) {
			mondo_vec_reg = baddr + ino_table[i]->mapping_reg;
			if (save) {
				if (*mondo_vec_reg & INTERRUPT_VALID) {
					cpr_softsp[i] = *mondo_vec_reg;
				}
			} else {
				if (cpr_softsp[i]) {
					*mondo_vec_reg = cpr_softsp[i];
				}
			}
		}
	}
}

/*
 * sbus_intrdist
 *
 * This function retargets active interrupts by reprogramming the mondo
 * vec register. If the CPU ID of the target has not changed, then
 * the mondo is not reprogrammed. The routine must hold the mondo
 * lock for this instance of the sbus.
 */
void
sbus_intrdist(void *arg, int mondo, uint_t cpu_id)
{
	struct sbus_soft_state *softsp;
	dev_info_t *dip = (dev_info_t *)arg;
	volatile uint64_t *mondo_vec_reg;
	uint64_t mondo_vec;
	volatile uint64_t *intr_state_reg;
	uint_t start_bit;
	volatile uint64_t tmpreg; /* HW flush reg */

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	/* mask off the mondo so that UPA_ID of sysio is gone */
	mondo &= IMR_INO;

	/* extract the soft state pointer */
	softsp = ddi_get_soft_state(sbusp, ddi_get_instance(dip));

	mondo_vec_reg = (softsp->intr_mapping_reg +
		ino_table[mondo]->mapping_reg);

#ifdef _STARFIRE
	/*
	 * For Starfire it is a pain to check the current target for
	 * the mondo since we have to read the PC asics ITTR slot
	 * assigned to this mondo. It will be much easier to assume
	 * the current target is always different and do the target
	 * reprogram all the time.
	 */
#else
	if (((*mondo_vec_reg & IMR_TID) >> IMR_TID_SHIFT) == cpu_id) {
		/* It is the same, don't reprogram */
		return;
	}
#endif	/* _STARFIRE */

	/* So it's OK to reprogram the CPU target */

	/* turn off the valid bit and wait for the state machine to idle */
	*mondo_vec_reg &= ~INTERRUPT_VALID;

	tmpreg = *softsp->sbus_ctrl_reg;

#ifdef	lint
	tmpreg = tmpreg;
#endif	/* lint */

	if (mondo > MAX_MONDO_EXTERNAL) {
		start_bit = ino_table[mondo]->diagreg_shift;
		intr_state_reg = softsp->obio_intr_state;

		/*
		 * Loop waiting for state machine to idle. Do not keep
		 * looping on a panic so that the system does not hang.
		 */
		while ((((*intr_state_reg >> start_bit) & 0x3) ==
		    INT_PENDING) && !panicstr);
	} else {
		int int_pending = 0;	/* interrupts pending */

		/*
		 * Shift over to first bit for this Sbus slot, 16
		 * bits per slot, but bits 0-1 of each slot are reserved.
		 */
		start_bit = 16 * (mondo >> 3) + 2;
		intr_state_reg = softsp->sbus_intr_state;

		/*
		 * Make sure interrupts for levels 1-7 of this slot
		 * are not pending.
		 */
		do {
			int level;		/* Sbus interrupt level */
			int shift;		/* # of bits to shift */
			uint64_t state_reg = *intr_state_reg;

			int_pending = 0;

			for (shift = start_bit, level = 1; level < 8;
			    level++, shift += 2) {
				if (((state_reg >> shift) &
				    0x3) == INT_PENDING) {
					int_pending = 1;
					break;
				}
			}
		} while (int_pending && !panicstr);
	}

	/* re-target the mondo and turn it on */
#ifdef _STARFIRE
	mondo_vec = (pc_translate_tgtid(softsp->ittrans_cookie,
	    cpu_id, mondo_vec_reg) <<
	    INTERRUPT_CPU_FIELD) |
	    INTERRUPT_VALID;
#else
	mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
#endif	/* _STARFIRE */

	/* write it back to the hardware. */
	*mondo_vec_reg = mondo_vec;

	/* flush the hardware buffers. */
	tmpreg = *mondo_vec_reg;

#ifdef	lint
	tmpreg = tmpreg;
#endif	/* lint */
}

/*
 * called from sbus_add_kstats() to create a kstat for each %pic
 * that the SBUS supports. These (read-only) kstats export the
 * event names that each %pic supports.
 *
 * if we fail to create any of these kstats we must remove any
 * that we have already created and return;
 *
 * NOTE: because all sbus devices use the same events we only
 *	 need to create the picN kstats once. All instances can
 *	 use the same picN kstats.
 *
 *       The flexibility exists to allow each device specify it's
 *       own events by creating picN kstats with the instance number
 *       set to ddi_get_instance(softsp->dip).
 *
 *       When searching for a picN kstat for a device you should
 *       first search for a picN kstat using the instance number
 *       of the device you are interested in. If that fails you
 *       should use the first picN kstat found for that device.
 */
static	void
sbus_add_picN_kstats(dev_info_t *dip)
{
	/*
	 * SBUS Performance Events.
	 *
	 * We declare an array of event-names and event-masks.
	 * The num of events in this array is AC_NUM_EVENTS.
	 */
	sbus_event_mask_t sbus_events_arr[SBUS_NUM_EVENTS] = {
		{"dvma_stream_rd", 0x0}, {"dvma_stream_wr", 0x1},
		{"dvma_const_rd", 0x2}, {"dvma_const_wr", 0x3},
		{"dvma_tlb_misses", 0x4}, {"dvma_stream_buf_mis", 0x5},
		{"dvma_cycles", 0x6}, {"dvma_bytes_xfr", 0x7},
		{"interrupts", 0x8}, {"upa_inter_nack", 0x9},
		{"pio_reads", 0xA}, {"pio_writes", 0xB},
		{"sbus_reruns", 0xC}, {"pio_cycles", 0xD}
	};

	/*
	* We declare an array of clear masks for each pic.
	* These masks are used to clear the %pcr bits for
	* each pic.
	*/
	sbus_event_mask_t sbus_clear_pic[SBUS_NUM_PICS] = {
		/* pic0 */
		{"clear_pic", (uint64_t)~(0xf)},
		/* pic1 */
		{"clear_pic", (uint64_t)~(0xf << 8)}
	};

	struct kstat_named *sbus_pic_named_data;
	int  		event, pic;
	char 		pic_name[30];
	int		instance = ddi_get_instance(dip);
	int		pic_shift = 0;

	for (pic = 0; pic < SBUS_NUM_PICS; pic++) {
		/*
		 * create the picN kstat. The size of this kstat is
		 * SBUS_NUM_EVENTS + 1 for the clear_event_mask
		 */
		(void) sprintf(pic_name, "pic%d", pic);	/* pic0, pic1 ... */
		if ((sbus_picN_ksp[pic] = kstat_create("sbus",
			instance, pic_name, "bus", KSTAT_TYPE_NAMED,
			SBUS_NUM_EVENTS + 1, NULL)) == NULL) {
				cmn_err(CE_WARN, "sbus %s: kstat_create failed",
					pic_name);

			/* remove pic0 kstat if pic1 create fails */
			if (pic == 1) {
				kstat_delete(sbus_picN_ksp[0]);
				sbus_picN_ksp[0] = NULL;
			}
			return;
		}

		sbus_pic_named_data =
			(struct kstat_named *)(sbus_picN_ksp[pic]->ks_data);

		/*
		 * when we are writing pcr_masks to the kstat we need to
		 * shift bits left by 8 for pic1 events.
		 */
		if (pic == 1)
			pic_shift = 8;

		/*
		 * for each picN event we need to write a kstat record
		 * (name = EVENT, value.ui64 = PCR_MASK)
		 */
		for (event = 0; event < SBUS_NUM_EVENTS; event ++) {

			/* pcr_mask */
			sbus_pic_named_data[event].value.ui64 =
				sbus_events_arr[event].pcr_mask << pic_shift;

			/* event-name */
			kstat_named_init(&sbus_pic_named_data[event],
				sbus_events_arr[event].event_name,
				KSTAT_DATA_UINT64);
		}

		/*
		 * we add the clear_pic event and mask as the last
		 * record in the kstat
		 */
		/* pcr mask */
		sbus_pic_named_data[SBUS_NUM_EVENTS].value.ui64 =
			sbus_clear_pic[pic].pcr_mask;

		/* event-name */
		kstat_named_init(&sbus_pic_named_data[SBUS_NUM_EVENTS],
			sbus_clear_pic[pic].event_name,
			KSTAT_DATA_UINT64);

		kstat_install(sbus_picN_ksp[pic]);
	}
}

static	void
sbus_add_kstats(struct sbus_soft_state *softsp)
{
	struct kstat *sbus_counters_ksp;
	struct kstat_named *sbus_counters_named_data;

	/*
	 * Create the picN kstats if we are the first instance
	 * to attach. We use sbus_attachcnt as a count of how
	 * many instances have attached. This is protected by
	 * a mutex.
	 */
	mutex_enter(&sbus_attachcnt_mutex);
	if (sbus_attachcnt == 0)
		sbus_add_picN_kstats(softsp->dip);

	sbus_attachcnt ++;
	mutex_exit(&sbus_attachcnt_mutex);

	/*
	 * A "counter" kstat is created for each sbus
	 * instance that provides access to the %pcr and %pic
	 * registers for that instance.
	 *
	 * The size of this kstat is SBUS_NUM_PICS + 1 for %pcr
	 */
	if ((sbus_counters_ksp = kstat_create("sbus",
		ddi_get_instance(softsp->dip), "counters",
		"bus", KSTAT_TYPE_NAMED, SBUS_NUM_PICS + 1,
		KSTAT_FLAG_WRITABLE)) == NULL) {

			cmn_err(CE_WARN, "sbus%d counters: kstat_create"
				" failed", ddi_get_instance(softsp->dip));
		return;
	}

	sbus_counters_named_data =
		(struct kstat_named *)(sbus_counters_ksp->ks_data);

	/* initialize the named kstats */
	kstat_named_init(&sbus_counters_named_data[0],
		"pcr", KSTAT_DATA_UINT64);

	kstat_named_init(&sbus_counters_named_data[1],
		"pic0", KSTAT_DATA_UINT64);

	kstat_named_init(&sbus_counters_named_data[2],
		"pic1", KSTAT_DATA_UINT64);

	sbus_counters_ksp->ks_update = sbus_counters_kstat_update;
	sbus_counters_ksp->ks_private = (void *)softsp;

	kstat_install(sbus_counters_ksp);

	/* update the sofstate */
	softsp->sbus_counters_ksp = sbus_counters_ksp;
}

static	int
sbus_counters_kstat_update(kstat_t *ksp, int rw)
{
	struct kstat_named *sbus_counters_data;
	struct sbus_soft_state *softsp;
	uint64_t pic_register;

	sbus_counters_data = (struct kstat_named *)ksp->ks_data;
	softsp = (struct sbus_soft_state *)ksp->ks_private;

	if (rw == KSTAT_WRITE) {

		/*
		 * Write the pcr value to the softsp->sbus_pcr.
		 * The pic register is read-only so we don't
		 * attempt to write to it.
		 */

		*softsp->sbus_pcr =
			(uint32_t)sbus_counters_data[0].value.ui64;

	} else {
		/*
		 * Read %pcr and %pic register values and write them
		 * into counters kstat.
		 *
		 * Due to a hardware bug we need to right shift the %pcr
		 * by 4 bits. This is only done when reading the %pcr.
		 *
		 */
		/* pcr */
		sbus_counters_data[0].value.ui64 = *softsp->sbus_pcr >> 4;

		pic_register = *softsp->sbus_pic;
		/*
		 * sbus pic register:
		 *  (63:32) = pic0
		 *  (31:00) = pic1
		 */

		/* pic0 */
		sbus_counters_data[1].value.ui64 = pic_register >> 32;
		/* pic1 */
		sbus_counters_data[2].value.ui64 =
			pic_register & SBUS_PIC0_MASK;

	}
	return (0);
}
