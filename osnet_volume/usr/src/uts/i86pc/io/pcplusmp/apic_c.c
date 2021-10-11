/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)apic_c.c	1.52	99/11/20 SMI"

/*
 * PSMI 1.1 extensions are supported only in 2.6 and later versions.
 * PSMI 1.2 extensions are supported only in 2.7 and later versions.
 */
#define	PSMI_1_2

#include <sys/types.h>
#include <sys/param.h>
#include <sys/processor.h>
#include <sys/time.h>
#include <sys/psm.h>
#include <sys/cmn_err.h>
#include <sys/smp_impldefs.h>
#include <sys/cram.h>
#include "apic.h"
#include <sys/pit.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci.h>
#include <sys/promif.h>
#include <sys/x86_archext.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>
#include <sys/cpc_impl.h>
#include <sys/trap.h>

/*
 *	Local Function Prototypes
 */
static void apic_init_intr();
static void apic_ret();
static int apic_handle_defconf();
static int apic_parse_mpct(caddr_t mpct);
static struct apic_mpfps_hdr *apic_find_fps_sig(caddr_t fptr, int size);
static int apic_checksum(caddr_t bptr, int len);
static int get_apic_cmd1();
static int get_apic_pri();
static int apic_find_bus_type(char *bus);
static int apic_find_bus(int busid);
static int apic_find_bus_id(int bustype);
static struct apic_io_intr *apic_find_io_intr(int irqno);
static int apic_allocate_irq(int irq);
static int apic_find_free_irq(int start, int end);
static uchar_t apic_allocate_vector(int ipl, int irq);
static void apic_modify_vector(uchar_t vector, int irq);
static void apic_mark_vector(uchar_t oldvector, uchar_t newvector);
static uchar_t apic_xlate_vector(uchar_t oldvector);
static void apic_free_vector(uchar_t vector);
static void apic_setup_io_intr(int irq);
static struct apic_io_intr *apic_find_io_intr_w_busid(int irqno, int busid);
static int apic_find_intin(uchar_t ioapic, uchar_t intin);
static int apic_handle_pci_pci_bridge(dev_info_t *dip, int child_devno,
	int child_ipin, struct apic_io_intr **intrp, iflag_t *intr_flagp);
static int apic_setup_irq_table(dev_info_t *dip, int irqno,
	struct apic_io_intr *intrp, int origirq, iflag_t *intr_flagp);
static void apic_nmi_intr(caddr_t arg);
static uchar_t apic_bind_intr(dev_info_t *dip, int irq, uchar_t ioapicid,
	uchar_t intin);
static int apic_rebind(apic_irq_t *irq_ptr, int bind_cpu, int safe);
static void apic_intr_redistribute();
static void apic_cleanup_busy();

/* ACPI support routines */
static int acpi_probe(void);
static int acpi_translate_pci_irq(dev_info_t *dip, int busid, int devid,
	int ipin, iflag_t *intr_flagp);
static acpi_obj acpi_find_pcibus(int busno);
static int acpi_eval_lnk(acpi_obj lnkobj, iflag_t *intr_flagp);
static int acpi_get_gsiv(acpi_obj pciobj, int devno, int ipin,
	iflag_t *intr_flagp);
static uchar_t acpi_find_ioapic(int irq);
static int acpi_eval_int(acpi_obj dev, char *method, int *rint);
static int acpi_intr_compatible(iflag_t *iflagp1, iflag_t *iflagp2);

/*
 *	standard MP entries
 */
static int 	apic_probe();
static void	apic_clkinit();
static int	apic_getclkirq(int ipl);
static hrtime_t apic_gettime();
static hrtime_t apic_gethrtime();
static void	apic_init();
static void 	apic_picinit(void);
static void 	apic_cpu_start(processorid_t cpun, caddr_t rm_code);
static int	apic_post_cpu_start(void);
static void	apic_send_ipi(int cpun, int ipl);
static void	apic_set_softintr(int softintr);
static void	apic_set_idlecpu(processorid_t cpun);
static void	apic_unset_idlecpu(processorid_t cpun);
static int	apic_softlvl_to_irq(int ipl);
static int	apic_intr_enter(int ipl, int *vect);
static void	apic_intr_exit(int ipl, int vect);
static void	apic_setspl(int ipl);
static int 	apic_addspl(int ipl, int vector, int min_ipl, int max_ipl);
static int 	apic_delspl(int ipl, int vector, int min_ipl, int max_ipl);
static void	apic_shutdown();
static int	apic_disable_intr(processorid_t cpun);
static void	apic_enable_intr(processorid_t cpun);
static processorid_t	apic_get_next_processorid(processorid_t cpun);
static int		apic_get_ipivect(int ipl, int type);
static int	apic_translate_irq(dev_info_t *dip, int irqno);

/*
 * These variables are frequently accessed in apic_intr_enter(),
 * apic_intr_exit and apic_setspl, so group them together
 */
volatile uint32_t *apicadr = NULL;	/* virtual addr of local APIC 	*/
int apic_setspl_delay = 1;		/* apic_setspl - delay enable	*/
int apic_clkvect;

/* vector at which error interrupts come in */
int apic_errvect;
int apic_enable_error_intr = 1;
int apic_error_display_delay = 100;

/* vector at which performance counter overflow interrupts come in */
int apic_cpcovf_vect;
int apic_enable_cpcovf_intr = 1;

/*
 * number of bits per byte, from <sys/param.h>
 */
#define	UCHAR_MAX	((1 << NBBY) - 1)

/*
 * The following vector assignments influence the value of ipltopri and
 * vectortoipl. Note that vectors 0 - 0x1f are not used. We can program
 * idle to 0 and IPL 0 to 0x10 to differentiate idle in case
 * we care to do so in future. Note some IPLs which are rarely used
 * will share the vector ranges and heavily used IPLs (5 and 6) have
 * a wide range.
 *	IPL		Vector range.		as passed to intr_enter
 *	0		none.
 *	1,2,3		0x20-0x2f		0x0-0xf
 *	4		0x30-0x3f		0x10-0x1f
 *	5		0x40-0x5f		0x20-0x3f
 *	6		0x60-0x7f		0x40-0x5f
 *	7,8,9		0x80-0x8f		0x60-0x6f
 *	10		0x90-0x9f		0x70-0x7f
 *	11		0xa0-0xaf		0x80-0x8f
 *	...		...
 *	16		0xf0-0xff		0xd0-0xdf
 */
uchar_t apic_vectortoipl[APIC_AVAIL_VECTOR/APIC_VECTOR_PER_IPL] =
	{3, 4, 5, 5, 6, 6, 9, 10, 11, 12, 13, 14, 15, 16};
	/*
	 * The ipl of an ISR at vector X is apic_vectortoipl[X<<4]
	 * NOTE that this is vector as passed into intr_enter which is
	 * programmed vector - 0x20 (APIC_BASE_VECT)
	 */

uchar_t apic_ipltopri[MAXIPL+1];	/* unix ipl to apic pri		*/
	/* The taskpri to be programmed into apic to mask given ipl */


/*
 * Patchable global variables.
 */
int	apic_forceload = 0;

#define	INTR_ROUND_ROBIN_WITH_AFFINITY	0
#define	INTR_ROUND_ROBIN		1
#define	INTR_LOWEST_PRIORITY		2

int	apic_intr_policy = INTR_ROUND_ROBIN_WITH_AFFINITY;

static	int	apic_next_bind_cpu = 2; /* For round robin assignment */
					/* start with cpu 1 */

int	apic_coarse_hrtime = 1;		/* 0 - use accurate slow gethrtime() */
					/* 1 - use gettime() for performance */
int	apic_flat_model = 0;		/* 0 - clustered. 1 - flat */
int	apic_enable_hwsoftint = 0;	/* 0 - disable, 1 - enable	*/
int	apic_enable_bind_log = 1;	/* 1 - display interrupt binding log */
int	apic_verbose = 0;		/* 1 - enable more chatty operation */
int	apic_panic_on_nmi = 0;
int	apic_panic_on_apic_error = 0;

/* Now the ones for Dynamic Interrupt distribution */
int	apic_enable_dynamic_migration = 1;
/*
 * If enabled, the distribution works as follows:
 * On every interrupt entry, the current ipl for the CPU is set in cpu_info
 * and the irq corresponding to the ipl is also set in the aci_current array.
 * interrupt exit and setspl (due to soft interrupts) will cause the current
 * ipl to be be changed. This is cache friendly as these frequently used
 * paths write into a per cpu structure.
 * Every clock interrupt, we go and check the structures for all CPUs
 * and increment the busy field of the irq (if any) executing on each CPU and
 * the busy field of the corresponding CPU. Every apic_ticks_for_redistribution
 * clock interrupts, we do computations to decide which interrupt needs to
 * be migrated (see comments before apic_intr_redistribute().
 *
 * Following 3 variables start as % and can be patched or set using an
 * API to be defined in future. They will be scaled to ticks_for_redistribution
 * which is in turn set to hertz+1 to stagger it away from one sec processing
 */
int	apic_int_busy_mark = 60;
int	apic_int_free_mark = 20;
int	apic_diff_for_redistribution = 10;
int	apic_ticks_for_redistribution;
int	apic_redist_cpu_skip = 0;
int	apic_num_imbalance = 0;
int	apic_num_rebind = 0;

int	apic_nproc = 0;
int	apic_defconf = 0;
int	apic_irq_translate = 0;
int	apic_spec_rev = 0;
int	apic_imcrp = 0;

int	apic_use_acpi = 1;	/* 1 = use ACPI, 0 = don't use ACPI */

/*
 *	Local static data
 */
static struct	psm_ops apic_ops = {
	apic_probe,

	apic_init,
	apic_picinit,
	apic_intr_enter,
	apic_intr_exit,
	apic_setspl,
	apic_addspl,
	apic_delspl,
	apic_disable_intr,
	apic_enable_intr,
	apic_softlvl_to_irq,
	apic_set_softintr,

	apic_set_idlecpu,
	apic_unset_idlecpu,

	apic_clkinit,
	apic_getclkirq,
	(void (*)(void))NULL,		/* psm_hrtimeinit */
	apic_gethrtime,

	apic_get_next_processorid,
	apic_cpu_start,
	apic_post_cpu_start,
	apic_shutdown,
	apic_get_ipivect,
	apic_send_ipi,

	apic_translate_irq,
	(int (*)(todinfo_t *))NULL,	/* psm_tod_get */
	(int (*)(todinfo_t *))NULL,	/* psm_tod_set */
	(void (*)(int, char *))NULL,	/* psm_notify_error */
	(void (*)(int))NULL		/* psm_notify_func */
};


static struct	psm_info apic_psm_info = {
	PSM_INFO_VER01_1,			/* version */
	PSM_OWN_EXCLUSIVE,			/* ownership */
	(struct psm_ops *)&apic_ops,		/* operation */
	"pcplusmp",				/* machine name */
	"pcplusmp v1.4 compatible 1.52",
};

static void *apic_hdlp;

#ifdef DEBUG
#define	DENT	0x0001
int	apic_debug  = 0;

#define	APIC_DEBUG_MSGBUFSIZE	2048
int	apic_debug_msgbuf[APIC_DEBUG_MSGBUFSIZE];
int	apic_debug_msgbufindex = 0;

/*
 * Put "int" info into debug buffer. No MP consistency, but light weight.
 * Good enough for most debugging.
 */
#define	APIC_DEBUG_BUF_PUT(x) \
	apic_debug_msgbuf[apic_debug_msgbufindex++] = x; \
	if (apic_debug_msgbufindex >= (APIC_DEBUG_MSGBUFSIZE - NCPU)) \
		apic_debug_msgbufindex = 0;

#endif

static apic_cpus_info_t	*apic_cpus;

static uint_t 	apic_cpumask = 0;
static uint_t	apic_flag;

/* Flag to indicate that we need to shut down all processors */
static uint_t	apic_shutdown_processors;

uint_t apic_nsec_per_intr = 0;
uint_t pit_ticks_adj = 0;

/*
 * apic_let_idle_redistribute can have the following values:
 * <0 - Some idle cpu is currently redistributing
 * 2 - clock has determined it is time for an idle cpu to look at intpt load.
 * 1 - 1 clock tick has expired since clock set it to 2 and no CPU went idle
 *	in the meantime. Time to poke some cpu if it is in idle.
 * 0 - If clock decremented it from 1 to 0, clock has to call redistribute.
 *	Else, it is the normal state - Do nothing.
 * apic_redistribute_lock prevents multiple idle cpus from redistributing
 */
int	apic_num_idle_redistributions = 0;
static	int apic_let_idle_redistribute = 0;
static	uint_t apic_nticks = 0;
static	uint_t apic_skipped_redistribute = 0;

static	uint_t last_count_read = 0;
static	lock_t	apic_gethrtime_lock;
volatile int	apic_hrtime_stamp = 0;
volatile hrtime_t apic_nsec_since_boot = 0;
static uint_t apic_hertz_count, apic_nsec_per_tick;

static	hrtime_t	apic_last_hrtime = 0;
int		apic_hrtime_error = 0;
int		apic_remote_hrterr = 0;
int		apic_num_nmis = 0;
int		apic_apic_error = 0;
int		apic_num_apic_errors = 0;

static	uchar_t	apic_io_id[MAX_IO_APIC];
static	uchar_t	apic_io_ver[MAX_IO_APIC];
static	uchar_t	apic_io_vectbase[MAX_IO_APIC];
static	uchar_t	apic_io_vectend[MAX_IO_APIC];
static	volatile int32_t *apicioadr[MAX_IO_APIC];
/*
 * apic_ioapic_lock protects the ioapics (reg select), the status, temp_bound
 * and bound elements of cpus_info and the temp_cpu element of irq_struct
 */
static	lock_t	apic_ioapic_lock;
static	int	apic_io_max = 0;	/* no. of i/o apics enabled */

static	struct apic_io_intr *apic_io_intrp = 0;
static	struct apic_bus	*apic_busp;

static	uchar_t	apic_vector_to_irq[APIC_AVAIL_VECTOR];
static	uchar_t	apic_resv_vector[MAXIPL+1];

static	char	apic_level_intr[APIC_MAX_VECTOR+1];
static	int	apic_error = 0;
/* values which apic_error can take. Not catastrophic, but may help debug */
#define	APIC_ERR_BOOT_EOI	  0x1
#define	APIC_ERR_GET_IPIVECT_FAIL 0x2
#define	APIC_ERR_INVALID_INDEX	  0x4
#define	APIC_ERR_MARK_VECTOR_FAIL 0x8
#define	APIC_ERR_APIC_ERROR	  0x40000000
#define	APIC_ERR_NMI		  0x80000000

static	int	apic_cmos_ssb_set = 0;

static	uint32_t	eisa_level_intr_mask = 0;
	/* At least MSB will be set if EISA bus */

static	int	apic_pci_bus_total = 0;
static	uchar_t	apic_single_pci_busid = 0;


static	apic_irq_t *apic_irq_table[APIC_MAX_VECTOR+1];
static	int	apic_max_device_irq = 0;
static	int	apic_min_device_irq = APIC_MAX_VECTOR;

/* use to make sure only one cpu handles the nmi */
static	lock_t	apic_nmi_lock;

/*
 * Following declarations are for revectoring; used when ISRs at different
 * IPLs share an irq.
 */
static	lock_t	apic_revector_lock;
static	int	apic_revector_pending = 0;
static	uchar_t	*apic_oldvec_to_newvec;
static	uchar_t	*apic_newvec_to_oldvec;
static	hrtime_t apic_last_revector = 0;

/*
 * ACPI definitions
 */
/* _PIC method arguments */
#define	ACPI_PIC_MODE	0
#define	ACPI_APIC_MODE	1

/* _HID for PCI bus object */
#define	HID_PCI_BUS	0x30AD041

/* _PRT package indexes */
#define	ACPI_PRT_ADDR_IDX	0
#define	ACPI_PRT_PIN_IDX	1
#define	ACPI_PRT_SRC_IDX	2
#define	ACPI_PRT_SRCIND_IDX	3

/* ACPI resource type */
#define	ACPI_REG_IRQ_TYPE1	0x22
#define	ACPI_REG_IRQ_TYPE2	0x23
#define	ACPI_EXT_IRQ_TYPE	0x89

/* resource type index */
#define	RES_TYPE		0x00

/* indexes for extended IRQ resource descriptor */
#define	EXT_IRQ_RES_LEN		0x01
#define	EXT_IRQ_FLAG		0x03
#define	EXT_IRQ_TBL_LEN		0x04
#define	EXT_IRQ_NO		0x05

/* extended IRQ flag */
#define	EXT_IFLAG_LL		0x04
#define	EXT_IFLAG_HE		0x02

/* indexes for small resource IRQ descriptor */
#define	REG_IRQ_MASK	0x01
#define	REG_IRQ_FLAG	0x03

/* regular IRQ flag */
#define	REG_IFLAG_HE	0x01
#define	REG_IFLAG_LL	0x04

/*
 * ACPI variables
 */
/* 1 = acpi is enabled & working, 0 = acpi is not enabled or not there */
static	int apic_enable_acpi = 0;

/* ACPI Multiple APIC Description Table ptr */
static	acpi_apic_t *acpi_mapic_dtp = NULL;

/* ACPI Interrupt Source Override Structure ptr */
static	acpi_iso_t *acpi_isop = NULL;
static	int acpi_iso_cnt = 0;

/* ACPI Non-maskable Interrupt Sources ptr */
static	acpi_apic_nmi_src_t *acpi_nmi_sp = NULL;
static	int acpi_nmi_scnt = 0;
static	acpi_apic_nmi_cnct_t *acpi_nmi_cp = NULL;
static	int acpi_nmi_ccnt = 0;

/* ACPI _SB object */
static	acpi_obj acpi_root = NULL;
static	acpi_obj acpi_sbobj = NULL;

/*
 * extern declarations
 */
extern	void	lock_init(lock_t *);
extern	void	lock_set(lock_t *);
extern	int	lock_try(lock_t *);
extern	void	lock_clear(lock_t *);
extern	uint_t	apic_calibrate(volatile uint32_t *);
extern	void	lock_add_hrt(void);
extern	void	tenmicrosec(void);
extern	int	cr0(void);
extern	void	setcr0(int);
extern	int	intr_clear(void);
extern	void	intr_restore(uint_t);
extern	int	acpi_init1(void);

/*
 *	This is the loadable module wrapper
 */

int
_init(void)
{
	if (apic_coarse_hrtime)
		apic_ops.psm_gethrtime = &apic_gettime;
	return (psm_mod_init(&apic_hdlp, &apic_psm_info));
}

int
_fini(void)
{
	return (psm_mod_fini(&apic_hdlp, &apic_psm_info));
}

int
_info(struct modinfo *modinfop)
{
	return (psm_mod_info(&apic_hdlp, &apic_psm_info, modinfop));
}

/*
 * Auto-configuration routines
 */

/*
 * Look at MPSpec 1.4 (Intel Order # 242016-005) for details of what we do here
 * May work with 1.1 - but not guaranteed.
 * According to the MP Spec, the MP floating pointer structure
 * will be searched in the order described below:
 * 1. In the first kilobyte of Extended BIOS Data Area (EBDA)
 * 2. Within the last kilobyte of system base memory
 * 3. In the BIOS ROM address space between 0F0000h and 0FFFFh
 * Once we find the right signature with proper checksum, we call
 * either handle_defconf or parse_mpct to get all info necessary for
 * subsequent operations.
 */
static int
apic_probe()
{
	paddr_t	mpct_addr, ebda_start = 0, base_mem_end;
	uchar_t	*biosdatap;
	caddr_t	mpct;
	caddr_t	fptr;
	int	i, mpct_size, mapsize;
	ushort_t	ebda_seg, base_mem_size;
	struct	apic_mpfps_hdr	*fpsp;
	struct	apic_mp_cnf_hdr	*hdrp;

	if (apic_forceload < 0)
		return (PSM_FAILURE);

	if (acpi_probe() == PSM_SUCCESS)
		return (PSM_SUCCESS);

	/*
	 * mapin the bios data area 40:0
	 * 40:13h - two-byte location reports the base memory size
	 * 40:0Eh - two-byte location for the exact starting address of
	 *	    the EBDA segment for EISA
	 */
	biosdatap = (uchar_t *)psm_map_phys((paddr_t)0x400, 0x20, PROT_READ);
	if (! biosdatap)
		return (PSM_FAILURE);
	fpsp = (struct apic_mpfps_hdr *)NULL;
	mapsize = MPFPS_RAM_WIN_LEN;
	/*LINTED: pointer cast may result in improper alignment */
	ebda_seg = *((ushort_t *)(biosdatap+0xe));
	/* check the 1k of EBDA */
	if (ebda_seg) {
	    ebda_start = ((paddr_t)ebda_seg) << 4;
	    fptr = psm_map_phys(ebda_start, MPFPS_RAM_WIN_LEN, PROT_READ);
	    if (fptr) {
		if (!(fpsp = apic_find_fps_sig(fptr, MPFPS_RAM_WIN_LEN)))
			psm_unmap_phys(fptr, MPFPS_RAM_WIN_LEN);
	    }
	}
	/* If not in EBDA, check the last k of system base memory */
	if (! fpsp) {
	    /*LINTED: pointer cast may result in improper alignment */
	    base_mem_size = * ((ushort_t *)(biosdatap+0x13));
	    if (base_mem_size > 512)
			base_mem_end = 639*1024;
	    else
			base_mem_end = 511*1024;
	    /* if ebda == last k of base mem, skip to check BIOS ROM */
	    if (base_mem_end != ebda_start) {
		fptr = psm_map_phys(base_mem_end, MPFPS_RAM_WIN_LEN, PROT_READ);
		if (fptr) {
		    if (!(fpsp = apic_find_fps_sig(fptr, MPFPS_RAM_WIN_LEN)))
			    psm_unmap_phys(fptr, MPFPS_RAM_WIN_LEN);
		}
	    }
	}
	psm_unmap_phys((caddr_t)biosdatap, 0x20);

	/* If still cannot find it, check the BIOS ROM space */
	if (! fpsp) {
		mapsize = MPFPS_ROM_WIN_LEN;
		fptr = psm_map_phys(MPFPS_ROM_WIN_START,
				MPFPS_ROM_WIN_LEN, PROT_READ);
		if (fptr) {
		    if (!(fpsp = apic_find_fps_sig(fptr, MPFPS_ROM_WIN_LEN))) {
			psm_unmap_phys(fptr, MPFPS_ROM_WIN_LEN);
			return (PSM_FAILURE);
		    }
		}
	}

	if (apic_checksum((caddr_t)fpsp, fpsp->mpfps_length * 16) != 0) {
		psm_unmap_phys(fptr, MPFPS_ROM_WIN_LEN);
		return (PSM_FAILURE);
	}

	apic_spec_rev = fpsp->mpfps_spec_rev;
	if ((apic_spec_rev != 04) && (apic_spec_rev != 01)) {
		psm_unmap_phys(fptr, MPFPS_ROM_WIN_LEN);
		return (PSM_FAILURE);
	}

	/* check IMCR is present or not */
	apic_imcrp = fpsp->mpfps_featinfo2 & MPFPS_FEATINFO2_IMCRP;

	/* check default configuration (dual CPUs) */
	if ((apic_defconf = fpsp->mpfps_featinfo1) != 0) {
	    psm_unmap_phys(fptr, mapsize);
	    return (apic_handle_defconf());
	}

	/* MP Configuration Table */
	mpct_addr = (paddr_t)(fpsp->mpfps_mpct_paddr);

	psm_unmap_phys(fptr, mapsize); /* unmap floating ptr struct */

	/*
	 * Map in enough memory for the MP Configuration Table
	 * Header.  Use this table to read the total
	 * length of the BIOS data and map in all the info
	 */
	/*LINTED: pointer cast may result in improper alignment */
	hdrp = (struct apic_mp_cnf_hdr *)psm_map_phys(mpct_addr,
			sizeof (struct apic_mp_cnf_hdr), PROT_READ);
	if (! hdrp)
		return (PSM_FAILURE);

	/* check mp configuration table signature PCMP */
	if (hdrp->mpcnf_sig != 0x504d4350) {
		psm_unmap_phys((caddr_t)hdrp, sizeof (struct apic_mp_cnf_hdr));
		return (PSM_FAILURE);
	}
	mpct_size = (int)hdrp->mpcnf_tbl_length;
	psm_unmap_phys((caddr_t)hdrp, sizeof (struct apic_mp_cnf_hdr));

	/*
	 * Map in the entries for this machine, ie. Processor
	 * Entry Tables, Bus Entry Tables, etc.
	 * They are in fixed order following one another
	 */
	mpct = psm_map_phys(mpct_addr, mpct_size, PROT_READ);
	if (! mpct)
		return (PSM_FAILURE);

	if (apic_checksum(mpct, mpct_size) != 0)
		goto apic_fail1;


	/*LINTED: pointer cast may result in improper alignment */
	hdrp = (struct apic_mp_cnf_hdr *)mpct;
	/*LINTED: pointer cast may result in improper alignment */
	apicadr = (uint32_t *)psm_map_phys((paddr_t)hdrp->mpcnf_local_apic,
				APIC_LOCAL_MEMLEN, PROT_READ|PROT_WRITE);
	if (! apicadr)
		goto apic_fail1;

	/* Parse all information in the tables */
	if (apic_parse_mpct(mpct) == PSM_SUCCESS)
		return (PSM_SUCCESS);

	for (i = 0; i < apic_io_max; i++)
		psm_unmap_phys((caddr_t)apicioadr[i], APIC_IO_MEMLEN);
	if (apic_cpus)
		kmem_free(apic_cpus, sizeof (*apic_cpus) * apic_nproc);
	if (apicadr)
		psm_unmap_phys((caddr_t)apicadr, APIC_LOCAL_MEMLEN);
apic_fail1:
	psm_unmap_phys(mpct, mpct_size);
	return (PSM_FAILURE);
}

static int
acpi_probe(void)
{
	int i, id, intmax, ver, index, rv;
	caddr_t acpi_mapic_dtend, cptr;
	acpi_local_apic_t *local_apicp;
	acpi_io_apic_t *io_apicp;
	volatile int32_t *ioapic;
	char local_ids[NCPU];
	char proc_ids[NCPU];
	acpi_obj picobj;
	acpi_val_t *one, *argp;

	/* call acpi_init1() just to get the apic table */
	if (! apic_use_acpi || (acpi_init1() != ACPI_OK))
		return (PSM_FAILURE);
	if ((acpi_mapic_dtp = acpi_apic_get()) == NULL)
		return (PSM_FAILURE);
	if (apic_checksum((caddr_t)acpi_mapic_dtp,
		acpi_mapic_dtp->header.length) != 0)
		return (PSM_FAILURE);
	acpi_mapic_dtend = ((char *)acpi_mapic_dtp) +
	    acpi_mapic_dtp->header.length - 1;
	apicadr = (uint32_t *)psm_map_phys(
	    (paddr_t)acpi_mapic_dtp->local_apic_addr,
	    APIC_LOCAL_MEMLEN, PROT_READ|PROT_WRITE);
	if (!apicadr)
		return (PSM_FAILURE);

	id = apicadr[APIC_LID_REG];
	local_ids[0] = (uchar_t)(((uint_t)id) >> 24);
	apic_nproc = index = 1;
	apic_io_max = 0;
	cptr = ((caddr_t)acpi_mapic_dtp) + sizeof (struct acpi_apic);

	while (cptr <= acpi_mapic_dtend) {
		switch (*cptr) {
		case ACPI_APIC_LOCAL:
			local_apicp = (acpi_local_apic_t *)cptr;
			if (local_apicp->flags & ACPI_APIC_ENABLED) {
				if (local_apicp->apic_id == local_ids[0])
					proc_ids[0] = local_apicp->proc_id;
				else if (apic_nproc < NCPU) {
					local_ids[index] = local_apicp->apic_id;
					proc_ids[index] = local_apicp->proc_id;
					index++;
					apic_nproc++;
				} else
					cmn_err(CE_WARN, "pcplusmp: exceed "
					"maximum no. of CPUs (= %d)", NCPU);
			}
			cptr += sizeof (struct acpi_local_apic);
			break;
		case ACPI_APIC_IO:
			io_apicp = (acpi_io_apic_t *)cptr;
			if (apic_io_max < MAX_IO_APIC) {
				apic_io_id[apic_io_max] = io_apicp->apic_id;
				apic_io_vectbase[apic_io_max] =
							io_apicp->vector_base;
				ioapic = apicioadr[apic_io_max] =
					(int32_t *)psm_map_phys(
					(paddr_t)io_apicp->address,
					APIC_IO_MEMLEN, PROT_READ | PROT_WRITE);
				if (! ioapic)
					goto cleanup;
				apic_io_max++;
			}
			cptr += sizeof (struct acpi_io_apic);
			break;
		case ACPI_APIC_ISO:
			if (acpi_isop == NULL)
				acpi_isop = (acpi_iso_t *)cptr;
			acpi_iso_cnt++;
			cptr += sizeof (struct acpi_iso);
			break;
		case ACPI_APIC_NMI_SRC:
			if (acpi_nmi_sp == NULL)
				acpi_nmi_sp = (acpi_apic_nmi_src_t *)cptr;
			acpi_nmi_scnt++;
			cptr += sizeof (struct acpi_apic_nmi_src);
			break;
		case ACPI_APIC_NMI_CNCT:
			if (acpi_nmi_cp == NULL)
				acpi_nmi_cp = (acpi_apic_nmi_cnct_t *)cptr;
			acpi_nmi_ccnt++;
			cptr += sizeof (struct acpi_apic_nmi_cnct);
			break;
		default:
			cmn_err(CE_WARN, "pcplusmp: acpi_probe: illegal "
				"type in APIC DT");
			goto cleanup;
		}
	}

	if ((apic_cpus = kmem_zalloc(sizeof (*apic_cpus) * apic_nproc,
	    KM_NOSLEEP)) == NULL)
		goto cleanup;

	apic_cpumask = (1 << apic_nproc) - 1;

	/*
	 * ACPI doesn't provide the local apic ver, get it directly from the
	 * local apic
	 */
	ver = apicadr[APIC_VERS_REG];
	for (i = 0; i < apic_nproc; i++) {
		apic_cpus[i].aci_local_id = local_ids[i];
		apic_cpus[i].aci_local_ver = (uchar_t)(ver & 0xFF);
	}
	for (i = 0; i < apic_io_max; i++) {
		ioapic = apicioadr[i];
		/*
		 * On the Sitka, the ioapic's apic_id field isn't reporting
		 * the actual io apic id.  We have reported this problem
		 * to Intel.  Until they fix the problem, we will get the
		 * actual id directly from the ioapic.
		 */
		ioapic[APIC_IO_REG] = APIC_ID_CMD;
		id = ioapic[APIC_IO_DATA];
		apic_io_id[i] = (uchar_t)(((uint_t)id) >> 24);
		ioapic[APIC_IO_REG] = APIC_VERS_CMD;
		ver = ioapic[APIC_IO_DATA];
		apic_io_ver[i] = (uchar_t)(ver & 0xff);
		intmax = (ver >> 16) & 0xff;
		apic_io_vectend[i] = apic_io_vectbase[i] + intmax;
	}

	/* now call acpi_init() to generate namespaces */
	if (acpi_init() != ACPI_OK)
		goto cleanup;

	if (((acpi_root = acpi_rootobj()) == NULL) ||
		((acpi_sbobj = acpi_findobj(acpi_root, "_SB", 0)) == NULL))
		goto acpi_cleanup;

	/* Enable ACPI APIC interrupt routing */
	if ((picobj = acpi_findobj(acpi_root, "_PIC", 0)) == NULL)
		goto cleanup;

	if ((argp = acpi_package_new(1)) == NULL)
		goto acpi_cleanup;

	if ((one = acpi_integer_new(ACPI_APIC_MODE)) == NULL) {
		acpi_val_free(argp);
		goto acpi_cleanup;
	}
	argp = acpi_pkg_setn(argp, 0, one);
	rv = acpi_eval(picobj, argp, NULL);
	acpi_val_free(argp);
	acpi_val_free(one);
	if (rv == ACPI_OK) {
		acpi_client_status(ACPI_OS_PCPLUSMP, ACPI_CLIENT_ON);
		apic_enable_acpi = 1;
		return (PSM_SUCCESS);
	}
acpi_cleanup:
	/* something wrong happen with ACPI */
	acpi_disable();
cleanup:
	if (apicadr != NULL) {
		psm_unmap_phys((caddr_t)apicadr, APIC_LOCAL_MEMLEN);
		apicadr = NULL;
	}
	apic_nproc = 0;
	for (i = 0; i < apic_io_max; i++) {
		psm_unmap_phys((caddr_t)apicioadr[i], APIC_IO_MEMLEN);
		apicioadr[i] = NULL;
	}
	apic_io_max = 0;
	acpi_isop = NULL;
	acpi_iso_cnt = 0;
	acpi_nmi_sp = NULL;
	acpi_nmi_scnt = 0;
	acpi_nmi_cp = NULL;
	acpi_nmi_ccnt = 0;
	return (PSM_FAILURE);
}

/*
 * Handle default configuration. Fill in reqd global variables & tables
 * Fill all details as MP table does not give any more info
 */
static int
apic_handle_defconf()
{
	uint_t	lid;

	/*LINTED: pointer cast may result in improper alignment */
	apicioadr[0] = (int32_t *)psm_map_phys(APIC_IO_ADDR,
					APIC_IO_MEMLEN, PROT_READ|PROT_WRITE);
	/*LINTED: pointer cast may result in improper alignment */
	apicadr = (uint32_t *)psm_map_phys(APIC_LOCAL_ADDR,
				APIC_LOCAL_MEMLEN, PROT_READ|PROT_WRITE);
	apic_cpus = (apic_cpus_info_t *)
		    kmem_zalloc(sizeof (*apic_cpus) * 2, KM_NOSLEEP);
	if ((! apicadr) || (! apicioadr[0]) || (! apic_cpus)) {
			goto apic_handle_defconf_fail;
	}
	apic_cpumask = 3;
	apic_nproc = 2;
	lid = apicadr[APIC_LID_REG];
	apic_cpus[0].aci_local_id = (uchar_t)(lid >> APIC_ID_BIT_OFFSET);
	/*
	 * According to the PC+MP spec 1.1, the local ids
	 * for the default configuration has to be 0 or 1
	 */
	if (apic_cpus[0].aci_local_id == 1)
		apic_cpus[1].aci_local_id = 0;
	else if (apic_cpus[0].aci_local_id == 0)
		apic_cpus[1].aci_local_id = 1;
	else {
		goto apic_handle_defconf_fail;
	}
	apic_io_id[0] = 2;
	apic_io_max = 1;
	if (apic_defconf >= 5) {
		apic_cpus[0].aci_local_ver = APIC_INTEGRATED_VERS;
		apic_cpus[1].aci_local_ver = APIC_INTEGRATED_VERS;
		apic_io_ver[0] = APIC_INTEGRATED_VERS;
	} else {
		apic_cpus[0].aci_local_ver = 0;		/* 82489 DX */
		apic_cpus[1].aci_local_ver = 0;
		apic_io_ver[0] = 0;
	}
	if (apic_defconf == 2 || apic_defconf == 3 || apic_defconf == 6)
		eisa_level_intr_mask = (inb(EISA_LEVEL_CNTL+1) << 8) |
				inb(EISA_LEVEL_CNTL) | ((uint_t)INT32_MAX+1);
	return (PSM_SUCCESS);

apic_handle_defconf_fail:
	if (apic_cpus)
		kmem_free(apic_cpus, sizeof (*apic_cpus) * 2);
	if (apicadr)
		psm_unmap_phys((caddr_t)apicadr, APIC_LOCAL_MEMLEN);
	if (apicioadr[0])
		psm_unmap_phys((caddr_t)apicioadr[0], APIC_IO_MEMLEN);
	return (PSM_FAILURE);
}

/* Parse the entries in MP configuration table and collect info that we need */
static int
apic_parse_mpct(caddr_t mpct)
{
	struct	apic_procent	*procp;
	struct	apic_bus	*busp;
	struct	apic_io_entry	*ioapicp;
	struct	apic_io_intr	*intrp;
	uint_t	lid;

	/*LINTED: pointer cast may result in improper alignment */
	procp = (struct apic_procent *)(mpct + sizeof (struct apic_mp_cnf_hdr));

	/* Find max # of CPUS and allocate structure accordingly */
	apic_nproc = 0;
	while (procp->proc_entry == APIC_CPU_ENTRY) {
		if (procp->proc_cpuflags & CPUFLAGS_EN) {
			apic_nproc++;
		}
		procp++;
	}
	if (apic_nproc > NCPU)
		cmn_err(CE_WARN, "pcplusmp: exceed "
				"maximum no. of CPUs (= %d)", NCPU);
	if (!apic_nproc || !(apic_cpus = (apic_cpus_info_t *)
	    kmem_zalloc(sizeof (*apic_cpus)*apic_nproc, KM_NOSLEEP)))
		return (PSM_FAILURE);

	/*LINTED: pointer cast may result in improper alignment */
	procp = (struct apic_procent *)(mpct + sizeof (struct apic_mp_cnf_hdr));
	/* start with index 1 as 0 needs to be filled in with Boot CPU */
	apic_nproc = 1;
	while (procp->proc_entry == APIC_CPU_ENTRY) {
	    /* check whether the cpu exists or not */
	    if (procp->proc_cpuflags & CPUFLAGS_EN) {
		if (procp->proc_cpuflags & CPUFLAGS_BP) { /* Boot CPU */
		    lid = apicadr[APIC_LID_REG];
		    apic_cpus[0].aci_local_id = procp->proc_apicid;
		    if (apic_cpus[0].aci_local_id !=
				(uchar_t)(lid >> APIC_ID_BIT_OFFSET)) {
			return (PSM_FAILURE);
		    }
		    apic_cpus[0].aci_local_ver = procp->proc_version;
		} else {
		    apic_cpus[apic_nproc].aci_local_id = procp->proc_apicid;
		    apic_cpus[apic_nproc].aci_local_ver = procp->proc_version;
		    apic_nproc++;
		}
	    }
	    procp++;
	}

	/* convert the number of processors into a cpumask */
	apic_cpumask = (1 << apic_nproc) - 1;

	/*
	 * Save start of bus entries for later use.
	 * Get EISA level cntrl if EISA bus is present.
	 * Also get the CPI bus id for single CPI bus case
	 */
	apic_busp = busp = (struct apic_bus *)procp;
	while (busp->bus_entry == APIC_BUS_ENTRY) {
		lid = apic_find_bus_type((char *)&busp->bus_str1);
		if (lid	== BUS_EISA) {
		    eisa_level_intr_mask = (inb(EISA_LEVEL_CNTL+1) << 8) |
				inb(EISA_LEVEL_CNTL) | ((uint_t)INT32_MAX+1);
		} else if (lid == BUS_PCI) {
			/*
			 * apic_single_pci_busid will be used only if
			 * apic_pic_bus_total is equal to 1
			 */
			apic_pci_bus_total++;
			apic_single_pci_busid = busp->bus_id;
		}
		busp++;
	}

	ioapicp = (struct apic_io_entry *)busp;

	apic_io_max = 0;
	do {
	    if (apic_io_max < MAX_IO_APIC) {
		if (ioapicp->io_flags & IOAPIC_FLAGS_EN) {
		    apic_io_id[apic_io_max] = ioapicp->io_apicid;
		    apic_io_ver[apic_io_max] = ioapicp->io_version;
		    /*LINTED: pointer cast may result in improper alignment */
		    apicioadr[apic_io_max] = (int32_t *)psm_map_phys(
			(paddr_t)ioapicp->io_apic_addr,
			APIC_IO_MEMLEN, PROT_READ|PROT_WRITE);

		    if (! apicioadr[apic_io_max])
			return (PSM_FAILURE);

		    apic_io_max++;
		}
	    }
	    ioapicp++;
	} while (ioapicp->io_entry == APIC_IO_ENTRY);

	apic_io_intrp = (struct apic_io_intr *)ioapicp;

	intrp = apic_io_intrp;
	while (intrp->intr_entry == APIC_IO_INTR_ENTRY) {
		if ((intrp->intr_irq > APIC_MAX_ISA_IRQ) ||
			(apic_find_bus(intrp->intr_busid) == BUS_PCI)) {
				apic_irq_translate = 1;
				break;
		}
		intrp++;
	}

	return (PSM_SUCCESS);
}

static struct apic_mpfps_hdr *
apic_find_fps_sig(caddr_t cptr, int len)
{
	int	i;

	/* Look for the pattern "_MP_" */
	for (i = 0; i < len; i += 16) {
		if ((*(cptr+i) == '_') &&
			(*(cptr+i+1) == 'M') &&
			(*(cptr+i+2) == 'P') &&
			(*(cptr+i+3) == '_'))
		    /*LINTED: pointer cast may result in improper alignment */
			return ((struct apic_mpfps_hdr *)(cptr + i));
	}
	return (NULL);
}

static int
apic_checksum(caddr_t bptr, int len)
{
	int	i;
	uchar_t	cksum;

	cksum = 0;
	for (i = 0; i < len; i++)
		cksum += *bptr++;
	return ((int)cksum);
}


/*
 * Initialise vector->ipl and ipl->pri arrays. level_intr and irqtable
 * are also set to NULL. vector->irq is set to a value which cannot map
 * to a real irq to show that it is free.
 */
void
apic_init()
{
	int	i;
	int	*iptr;

	int	j = 1;
	apic_ipltopri[0] = APIC_VECTOR_PER_IPL; /* leave 0 for idle */
	for (i = 0; i < (APIC_AVAIL_VECTOR/APIC_VECTOR_PER_IPL); i++) {
		if ((i < ((APIC_AVAIL_VECTOR/APIC_VECTOR_PER_IPL) -1)) &&
			(apic_vectortoipl[i+1] == apic_vectortoipl[i]))
			/* get to highest vector at the same ipl */
				continue;
		for (; j <= apic_vectortoipl[i]; j++) {
			apic_ipltopri[j] = (i << APIC_IPL_SHIFT) +
				APIC_BASE_VECT;
		}
	}
	for (; j < MAXIPL+1; j++)
		/* fill up any empty ipltopri slots */
		apic_ipltopri[j] = (i << APIC_IPL_SHIFT) + APIC_BASE_VECT;
	/* cpu 0 is always up */
	apic_cpus[0].aci_status = APIC_CPU_ONLINE | APIC_CPU_INTR_ENABLE;

	iptr = (int *)&apic_irq_table[0];
	for (i = 0; i <= APIC_MAX_VECTOR; i++) {
		apic_level_intr[i] = 0;
		*iptr++ = NULL;
		if (i < APIC_AVAIL_VECTOR)
			apic_vector_to_irq[i] = APIC_RESV_IRQ;
	}

	/*
	 * Allocate a dummy irq table entry for the reserved entry.
	 * This takes care of the race between removing an irq and
	 * clock detecting a CPU in that irq during interrupt load
	 * sampling.
	 */
	apic_irq_table[APIC_RESV_IRQ] =
		kmem_zalloc(sizeof (apic_irq_t), KM_NOSLEEP);

}

/*
 * handler for APIC Error interrupt. Just print a warning and continue
 */
static int
apic_error_intr()
{
	uint_t	error;
	uint_t	i;

	/*
	 * We need to write before read as per 7.4.17 of system prog manual
	 * We do both and or the results to be safe
	 */
	error = apicadr[APIC_ERROR_STATUS];
	apicadr[APIC_ERROR_STATUS] = 0;
	error |= apicadr[APIC_ERROR_STATUS];
	if (error) {
#if	DEBUG
		debug_enter("pcplusmp: APIC Error interrupt received");
#endif
		if (apic_panic_on_apic_error)
			cmn_err(CE_PANIC,
				"APIC Error interrupt on CPU %d. Status = %x",
				psm_get_cpu_id(), error);
		else {
			/*
			 * prom_printf is the best shot we have of something
			 * which is problem free from high level/NMI type of
			 * interrupts
			 */
			prom_printf(
				"APIC Error interrupt on CPU %d. Status = %x",
				psm_get_cpu_id(), error);
			apic_error |= APIC_ERR_APIC_ERROR;
			apic_apic_error |= error;
			apic_num_apic_errors++;
			for (i = 0; i < apic_error_display_delay; i++) {
				tenmicrosec();
			}
			/*
			 * provide more delay next time limited to roughly
			 * 1 clock tick time
			 */
			if (apic_error_display_delay < 500)
				apic_error_display_delay *= 2;
		}
		return (DDI_INTR_CLAIMED);
	} else
		return (DDI_INTR_UNCLAIMED);
}

static void
apic_init_intr()
{
	processorid_t	cpun = psm_get_cpu_id();

	apicadr[APIC_TASK_REG] 	= APIC_MASK_ALL;

	if (apic_flat_model)
		apicadr[APIC_FORMAT_REG] = APIC_FLAT_MODEL;
	else
		apicadr[APIC_FORMAT_REG] = APIC_CLUSTER_MODEL;
	apicadr[APIC_DEST_REG] = AV_HIGH_ORDER >> cpun;

	/* need to enable APIC before unmasking NMI */
	apicadr[APIC_SPUR_INT_REG] = AV_UNIT_ENABLE|APIC_SPUR_INTR;

	apicadr[APIC_LOCAL_TIMER] 	= AV_MASK;
	apicadr[APIC_INT_VECT0] 	= AV_MASK;	/* local intr reg 0 */
	apicadr[APIC_INT_VECT1] 	= AV_NMI;	/* enable NMI */

	if (apic_cpus[cpun].aci_local_ver < APIC_INTEGRATED_VERS)
		return;

	/* Enable performance counter overflow interrupt */

	if ((x86_feature & X86_MSR) != X86_MSR)
		apic_enable_cpcovf_intr = 0;
	if (apic_enable_cpcovf_intr) {
		if (apic_cpcovf_vect == 0) {
			int ipl = APIC_PCINT_IPL;
			int irq = apic_get_ipivect(ipl, -1);

			ASSERT(irq != -1);
			apic_cpcovf_vect = apic_irq_table[irq]->airq_vector;
			ASSERT(apic_cpcovf_vect);
			(void) add_avintr(NULL, ipl, kcpc_hw_overflow_intr,
			    "apic pcint", irq, NULL);
			kcpc_hw_overflow_intr_installed = 1;
		}
		apicadr[APIC_PCINT_VECT] = apic_cpcovf_vect;
	}

	/* Enable error interrupt */

	if (apic_enable_error_intr) {
		if (apic_errvect == 0) {
			int ipl = 0xf;	/* get highest priority intr */
			int irq = apic_get_ipivect(ipl, -1);

			ASSERT(irq != -1);
			apic_errvect = apic_irq_table[irq]->airq_vector;
			ASSERT(apic_errvect);
			/*
			 * Not PSMI compliant, but we are going to merge
			 * with ON anyway
			 */
			(void) add_avintr((void *)NULL, ipl,
			    (avfunc)apic_error_intr, "apic error intr",
			    irq, NULL);
		}
		apicadr[APIC_ERR_VECT] = apic_errvect;
		apicadr[APIC_ERROR_STATUS] = 0;
		apicadr[APIC_ERROR_STATUS] = 0;
	}
}

static void
apic_disable_local_apic()
{
	apicadr[APIC_TASK_REG] = APIC_MASK_ALL;
	apicadr[APIC_LOCAL_TIMER] = AV_MASK;
	apicadr[APIC_INT_VECT0] = AV_MASK;	/* local intr reg 0 */
	apicadr[APIC_INT_VECT1] = AV_MASK;	/* disable NMI */
	apicadr[APIC_ERR_VECT] = AV_MASK;	/* and error interrupt */
	apicadr[APIC_PCINT_VECT] = AV_MASK;	/* and perf counter intr */
	apicadr[APIC_SPUR_INT_REG] = APIC_SPUR_INTR;
}

static void
apic_picinit(void)
{
	int i, j;
	uint_t isr;
	volatile int32_t *ioapic;

/*
	On UniSys Model 6520, the BIOS leaves vector 0x20 isr
	bit on without clearing it with EOI.  Since softint
	uses vector 0x20 to interrupt itself, so softint will
	not work on this machine.  In order to fix this problem
	a check is made to verify all the isr bits are clear.
	If not, EOIs are issued to clear the bits.
*/
	for (i = 7; i >= 1; i--) {
		if ((isr = apicadr[APIC_ISR_REG+(i*4)]) != 0)
			for (j = 0; ((j < 32) && (isr != 0)); j++)
				if (isr & (1 << j)) {
					apicadr[APIC_EOI_REG] = 0;
					isr &= ~(1 << j);
					apic_error |= APIC_ERR_BOOT_EOI;
				}
	}

	/* set a flag so we know we have run apic_picinit() */
	apic_flag = 1;
	lock_init(&apic_gethrtime_lock);
	lock_init(&apic_ioapic_lock);
	lock_init(&apic_revector_lock);
	picsetup();	 /* initialise the 8259 */

	/* add nmi handler - least priority nmi handler */
	lock_init(&apic_nmi_lock);
	if (! psm_add_nmintr(0, (avfunc) apic_nmi_intr,
		"pcplusmp NMI handler",  (caddr_t)NULL))
		cmn_err(CE_WARN, "pcplusmp: Unable to add nmi handler");

	apic_init_intr();

	/*	enable apic mode if imcr present	*/
	if (apic_imcrp) {
		outb(APIC_IMCR_P1, (uchar_t)APIC_IMCR_SELECT);
		outb(APIC_IMCR_P2, (uchar_t)APIC_IMCR_APIC);
	}

	/* mask interrupt vectors					*/
	for (j = 0; j < apic_io_max; j++) {
		int	intin_max;
		ioapic = apicioadr[j];
		ioapic[APIC_IO_REG] = APIC_VERS_CMD;
		/* Bits 23-16 define the maximum redirection entries */
		intin_max = (ioapic[APIC_IO_DATA] >> 16) & 0xff;
		for (i = 0; i < intin_max; i++) {
			ioapic[APIC_IO_REG] = APIC_RDT_CMD + 2*i;
			ioapic[APIC_IO_DATA] = AV_MASK;
		}
	}

	/*
	 * if apic_addspl() was called before apic_picinit(), the ioapic
	 * would not have been setup. DO it now.
	 */
	for (i = apic_min_device_irq; i <= apic_max_device_irq; i++) {
		if ((apic_irq_table[i]) &&
		    (apic_irq_table[i]->airq_mps_intr_index != FREE_INDEX)) {
			apic_setup_io_intr(i);
		}
	}
}


static void
apic_cpu_start(processorid_t cpun, caddr_t rm_code)
{
	int	loop_count;
	ulong_t	vector;
	uint_t cpu_id, iflag;

	cpu_id = apic_cpus[cpun].aci_local_id;

	apic_cmos_ssb_set = 1;

	iflag = intr_clear();
	outb(CMOS_ADDR, SSB);
	outb(CMOS_DATA, BIOS_SHUTDOWN);
	intr_restore(iflag);

	while (get_apic_cmd1() & AV_PENDING)
		apic_ret();

	/* for integrated - make sure there is one INIT IPI in buffer */
	/* for external - it will wake up the cpu */
	apicadr[APIC_INT_CMD2] = cpu_id << APIC_ICR_ID_BIT_OFFSET;
	apicadr[APIC_INT_CMD1] = AV_ASSERT|AV_RESET;

	/* If only 1 CPU is installed, PENDING bit will not go low */
	for (loop_count = 0x1000; loop_count; loop_count--)
		if (get_apic_cmd1() & AV_PENDING)
			apic_ret();
		else
			break;

	apicadr[APIC_INT_CMD2] = cpu_id << APIC_ICR_ID_BIT_OFFSET;
	apicadr[APIC_INT_CMD1] = AV_DEASSERT|AV_RESET;

	drv_usecwait(20000);		/* 20 milli sec */

	if (apic_cpus[cpun].aci_local_ver >= APIC_INTEGRATED_VERS) {
		/* integrated apic */

		rm_code = (caddr_t)rm_platter_pa;
		vector = ((int)rm_code & 0xff000)>>12;

		/* to offset the INIT IPI queue up in the buffer */
		apicadr[APIC_INT_CMD2] = cpu_id << APIC_ICR_ID_BIT_OFFSET;
		apicadr[APIC_INT_CMD1] = vector|AV_STARTUP;

		drv_usecwait(200);		/* 20 micro sec */

		apicadr[APIC_INT_CMD2] = cpu_id << APIC_ICR_ID_BIT_OFFSET;
		apicadr[APIC_INT_CMD1] = vector|AV_STARTUP;

		drv_usecwait(200);		/* 20 micro sec */
	}
}


#ifdef	DEBUG
int	apic_break_on_cpu = 9;
int	apic_stretch_interrupts = 1000;
int	apic_stretch_ISR = 1 << 3;	/* IPL of 3 matches nothing now */

void
apic_break()
{
}
#endif

/*
 * platform_intr_enter
 *
 *	Called at the beginning of the interrupt service routine to
 *	mask all level equal to and below the interrupt priority
 *	of the interrupting vector.  An EOI should be given to
 *	the interrupt controller to enable other HW interrupts.
 *
 *	Return -1 for spurious interrupts
 *
 */
/*ARGSUSED*/
static int
apic_intr_enter(int ipl, int *vectorp)
{
	uchar_t vector;
	int nipl;
	int	irq, i, j, max_busy;
	apic_cpus_info_t *cpu_infop;

	/*
	 * The real vector programmed in APIC is *vectorp + 0x20
	 * But, cmnint code subtracts 0x20 before pushing it.
	 * Hence APIC_BASE_VECT is 0x20.
	 */

	vector = (uchar_t)*vectorp;

	/* if interrupted by the clock, increment apic_nsec_since_boot */
	if (vector == apic_clkvect) {
	    lock_add_hrt();
	    last_count_read = apic_hertz_count;
	    if (apic_enable_dynamic_migration) {
#if	IDLE_REDISTRIBUTE
		if (apic_let_idle_redistribute > 0) {
		    lock_set(&apic_redistribute_lock);
		    if (apic_let_idle_redistribute > 0) {
			apic_let_idle_redistribute--;
			if (!apic_let_idle_redistribute) {
			    lock_clear(&apic_redistribute_lock);
			    apic_intr_redistribute();
			} else {
			    lock_clear(&apic_redistribute_lock);
				/* poke idle cpu */
			    for (i = 0; i < apic_nproc; i++) {
				if (apic_cpus[i].aci_idle)
				    apic_send_ipi(i, 1);
			    }
			}
		    }
		}
#endif

		if (apic_let_idle_redistribute == 0) {
		    if (++apic_nticks == apic_ticks_for_redistribution) {
			/*
			 * Time to call apic_intr_redistribute().
			 * reset apic_nticks. This will cause max_busy to
			 * be calculated below and if it is more than
			 * apic_int_busy, we will do the whole thing
			 */
			apic_nticks = 0;
		    }
		    max_busy = 0;
		    for (i = 0; i < apic_nproc; i++) {
		    /* Check if curipl is non zero & if ISR is in progress */
			if (((j = apic_cpus[i].aci_curipl) != 0) &&
			    (apic_cpus[i].aci_ISR_in_progress & (1 << j))) {

			    int	irq;
			    apic_cpus[i].aci_busy++;
			    irq = apic_cpus[i].aci_current[j];
			    apic_irq_table[irq]->airq_busy++;
			}
			if (!apic_nticks && (apic_cpus[i].aci_busy > max_busy))
			    max_busy = apic_cpus[i].aci_busy;
		    }
		    if (!apic_nticks) {
			if (max_busy > apic_int_busy_mark) {
			/*
			 * We could make the following check be skipped > 1
			 * in which case, we get a redistribution at half
			 * the busy mark (due to double interval). Need to
			 * be able to collect more empirical data to decide
			 * if that is a good strategy. Punt for now.
			 */
			    if (apic_skipped_redistribute)
				apic_cleanup_busy();
			    else
#if	IDLE_REDISTRIBUTE
				apic_let_idle_redistribute = 2;
#else
				apic_intr_redistribute();
#endif
			} else
			    apic_skipped_redistribute++;
		    }
		}
	    }
	    /* We will avoid all the book keeping overhead for clock */
	    nipl = apic_vectortoipl[vector >> APIC_IPL_SHIFT];
	    apicadr[APIC_TASK_REG] = apic_ipltopri[nipl];
	    *vectorp = apic_vector_to_irq[vector];
	    apicadr[APIC_EOI_REG] = 0;
	    return (nipl);
	}

	cpu_infop = &apic_cpus[psm_get_cpu_id()];

	if (vector == (APIC_SPUR_INTR-APIC_BASE_VECT)) {
		cpu_infop->aci_spur_cnt++;
		return (APIC_INT_SPURIOUS);
	}

	/* Check if the vector we got is really what we need */
	if (apic_revector_pending)
	    vector = apic_xlate_vector(vector+APIC_BASE_VECT)-APIC_BASE_VECT;

	nipl = apic_vectortoipl[vector >> APIC_IPL_SHIFT];
	*vectorp = irq = apic_vector_to_irq[vector];

	apicadr[APIC_TASK_REG] = apic_ipltopri[nipl];

	cpu_infop->aci_current[nipl] = (uchar_t)irq;
	cpu_infop->aci_curipl = (uchar_t)nipl;
	cpu_infop->aci_ISR_in_progress |= 1 << nipl;

	/*
	 * apic_level_intr could have been assimilated into the irq struct.
	 * but, having it as a character array is more efficient in terms of
	 * cache usage. So, we leave it as is.
	 */
	if (! apic_level_intr[irq])
		apicadr[APIC_EOI_REG] = 0;

#ifdef	DEBUG
	APIC_DEBUG_BUF_PUT(vector);
	APIC_DEBUG_BUF_PUT(irq);
	APIC_DEBUG_BUF_PUT(nipl);
	APIC_DEBUG_BUF_PUT(psm_get_cpu_id());
if ((apic_stretch_interrupts) && (apic_stretch_ISR & (1 << nipl)))
	drv_usecwait(apic_stretch_interrupts);

if (apic_break_on_cpu == psm_get_cpu_id())
	apic_break();
#endif
	return (nipl);
}

static void
apic_intr_exit(int prev_ipl, int irq)
{
	apic_cpus_info_t *cpu_infop;

	apicadr[APIC_TASK_REG] = apic_ipltopri[prev_ipl];

	cpu_infop = &apic_cpus[psm_get_cpu_id()];
	if (apic_level_intr[irq])
		apicadr[APIC_EOI_REG] = 0;

	cpu_infop->aci_curipl = (uchar_t)prev_ipl;
	/* ISR above current pri could not be in progress */
	cpu_infop->aci_ISR_in_progress &= (2 << prev_ipl) - 1;
}

/*
 * Mask all interrupts below or equal to the given IPL
 */
static void
apic_setspl(int ipl)
{

	apicadr[APIC_TASK_REG] = apic_ipltopri[ipl];

	/* interrupts at ipl above this cannot be in progress */
	apic_cpus[psm_get_cpu_id()].aci_ISR_in_progress &= (2 << ipl) - 1;
	/*
	 * this is a patch fix for the ALR QSMP P5 machine, so that interrupts
	 * have enough time to come in before the priority is raised again
	 * during the idle() loop.
	 */
	if (apic_setspl_delay)
		(void) get_apic_pri();
}

/*
 * trigger a software interrupt at the given IPL
 */
static void
apic_set_softintr(int ipl)
{
	int	vector;
	uint_t flag;

	vector = apic_resv_vector[ipl];

	flag = intr_clear();
	while (get_apic_cmd1() & AV_PENDING)
		apic_ret();


	/* generate interrupt at vector on itself only */
	apicadr[APIC_INT_CMD1]  = AV_SH_SELF|vector;

	intr_restore(flag);
}

/*
 * generates an interprocessor interrupt to another CPU
 */
static void
apic_send_ipi(int cpun, int ipl)
{
	int vector;
	uint_t	 flag;

	vector = apic_resv_vector[ipl];

	flag = intr_clear();
	while (get_apic_cmd1() & AV_PENDING)
		apic_ret();

	apicadr[APIC_INT_CMD2] =
		apic_cpus[cpun].aci_local_id << APIC_ICR_ID_BIT_OFFSET;
	apicadr[APIC_INT_CMD1]  = vector;
	intr_restore(flag);
}


/*ARGSUSED*/
static void
apic_set_idlecpu(processorid_t cpun)
{
#ifdef	IDLE_REDISTRIBUTE
	apic_cpus_info_t *cpu_infop;

	cpu_infop = &apic_cpus[cpun];
	cpu_infop->aci_idle = 1;


	if (cpu_infop->aci_redistribute) {
		/* do magic computations here */
	}
#endif
}

/*ARGSUSED*/
static void
apic_unset_idlecpu(processorid_t cpun)
{
#ifdef	IDLE_REDISTRIBUTE
	apic_cpus_info_t *cpu_infop;

	cpu_infop = &apic_cpus[cpun];
	cpu_infop->aci_idle = 0;
#endif
}


static void
apic_ret()
{
}

static int
get_apic_cmd1()
{
	return (apicadr[APIC_INT_CMD1]);
}

static int
get_apic_pri()
{
	return (apicadr[APIC_TASK_REG]);
}

/*
 * If apic_coarse_time == 1, then apic_gettime() is used instead of
 * apic_gethrtime().  This is used for performance instead of accuracy.
 */

static hrtime_t
apic_gettime()
{
	int	old_hrtime_stamp;
	hrtime_t temp;

gettime_again:
	while ((old_hrtime_stamp = apic_hrtime_stamp) & 1)
		apic_ret();

	temp = apic_nsec_since_boot;

	if (apic_hrtime_stamp != old_hrtime_stamp) {	/* got an interrupt */
		goto gettime_again;
	}
	return (temp);
}

/*
 * Here we return the number of nanoseconds since booting.  Note every
 * clock interrupt increments apic_nsec_since_boot by the appropriate
 * amount.
 */
static hrtime_t
apic_gethrtime()
{
	int	curr_timeval, countval, elapsed_ticks, oflags;
	int	old_hrtime_stamp, status;
	hrtime_t temp;
	uchar_t	cpun;

	oflags = intr_clear();	/* prevent migration */

	cpun = (uchar_t)((uint_t)apicadr[APIC_LID_REG] >> APIC_ID_BIT_OFFSET);

	lock_set(&apic_gethrtime_lock);

gethrtime_again:
	while ((old_hrtime_stamp = apic_hrtime_stamp) & 1)
		apic_ret();

	/*
	 * Check to see which CPU we are on.  Note the time is kept on
	 * the local APIC of CPU 0.  If on CPU 0, simply read the current
	 * counter.  If on another CPU, issue a remote read command to CPU 0.
	 */
	if (cpun == apic_cpus[0].aci_local_id) {
		countval = apicadr[APIC_CURR_COUNT];
	} else {
		while (get_apic_cmd1() & AV_PENDING)
			apic_ret();

		apicadr[APIC_INT_CMD2] =
			apic_cpus[0].aci_local_id << APIC_ICR_ID_BIT_OFFSET;
		apicadr[APIC_INT_CMD1] = APIC_CURR_ADD|AV_REMOTE;

		while ((status = get_apic_cmd1()) & AV_READ_PENDING)
			apic_ret();

		if (status & AV_REMOTE_STATUS)	/* 1 = valid */
			countval = apicadr[APIC_REMOTE_READ];
		else {	/* 0 = invalid */
			apic_remote_hrterr++;
			/*
			 * return last hrtime right now, will need more
			 * testing if change to retry
			 */
			temp = apic_last_hrtime;

			lock_clear(&apic_gethrtime_lock);
			intr_restore(oflags);

			return (temp);
		}
	}
	if (countval > last_count_read)
		countval = 0;
	else
		last_count_read = countval;

	elapsed_ticks = apic_hertz_count - countval;

	curr_timeval = elapsed_ticks * apic_nsec_per_tick;
	temp = apic_nsec_since_boot+curr_timeval;

	if (apic_hrtime_stamp != old_hrtime_stamp) {	/* got an interrupt */
		/* we might have clobbered last_count_read. Restore it */
		last_count_read = apic_hertz_count;
		goto gethrtime_again;
	}

	if (temp < apic_last_hrtime) {
		/* return last hrtime if error occurs */
		apic_hrtime_error++;
		temp = apic_last_hrtime;
	}
	else
		apic_last_hrtime = temp;

	lock_clear(&apic_gethrtime_lock);
	intr_restore(oflags);

	return (temp);
}

/* apic NMI handler */
/*ARGSUSED*/
static void
apic_nmi_intr(caddr_t arg)
{
	if (apic_shutdown_processors) {
		apic_disable_local_apic();
		return;
	}

	if (lock_try(&apic_nmi_lock)) {
#if	DEBUG
		debug_enter("pcplusmp: NMI received");
#else
		if (apic_panic_on_nmi)
			cmn_err(CE_PANIC, "pcplusmp: NMI received");
		else {
			/*
			 * prom_printf is the best shot we have of something
			 * which is problem free from high level/NMI type of
			 * interrupts
			 */
			prom_printf("pcplusmp: NMI received");
			apic_error |= APIC_ERR_NMI;
			apic_num_nmis++;
		}
#endif
		lock_clear(&apic_nmi_lock);
	}
}

/*
 * Add mask bits to disable interrupt vector from happening
 * at or above IPL.  In addition, it should remove mask bits
 * to enable interrupt vectors below the given IPL.
 *
 * Both add and delspl are complicated by the fact that different interrupts
 * may share IRQs. This can happen in two ways.
 * 1. The same H/W line is shared by more than 1 device
 * 1a. with interrupts at different IPLs
 * 1b. with interrupts at same IPL
 * 2. We ran out of vectors at a given IPL and started sharing vectors.
 * 1b and 2 should be handled gracefully, except for the fact some ISRs
 * will get called often when no interrupt is pending for the device.
 * For 1a, we just hope that the machine blows up with the person who
 * set it up that way!. In the meantime, we handle it at the higher IPL.
 */
/*ARGSUSED*/
static int
apic_addspl(int irqno, int ipl, int min_ipl, int max_ipl)
{
	uchar_t	vector, iflag;

	ASSERT(max_ipl <= UCHAR_MAX);
	if ((irqno == -1) || (!apic_irq_table[irqno]))
		return (PSM_FAILURE);
	/* return if it is not hardware interrupt */
	if (apic_irq_table[irqno]->airq_mps_intr_index == RESERVE_INDEX)
		return (PSM_SUCCESS);

	/* Or if there are more interupts at a higher IPL */
	if (ipl != max_ipl)
		return (PSM_SUCCESS);

	/*
	 * if apic_picinit() has not been called yet, just return.
	 * At the end of apic_picinit(), we will call setup_io_intr().
	 */

	if (!apic_flag)
		return (PSM_SUCCESS);

	iflag = intr_clear();

	/*
	 * Upgrade vector if max_ipl is not earlier ipl. If we cannot allocate,
	 * return failure. Not very elegant, but then we hope the
	 * machine will blow up with ...
	 */
	if (apic_irq_table[irqno]->airq_ipl != max_ipl) {
	    if ((vector = apic_allocate_vector(max_ipl, irqno)) == 0) {
		intr_restore(iflag);
		return (PSM_FAILURE);
	    } else {
		ASSERT(vector <= UCHAR_MAX);
		apic_mark_vector(apic_irq_table[irqno]->airq_vector, vector);
		apic_irq_table[irqno]->airq_vector = vector;
		apic_irq_table[irqno]->airq_ipl = (uchar_t)max_ipl;
	    }
	}

	apic_setup_io_intr(irqno);
	intr_restore(iflag);
	return (PSM_SUCCESS);
}

/*
 * Recompute mask bits for the given interrupt vector.
 * If there is no interrupt servicing routine for this
 * vector, this function should disable interrupt vector
 * from happening at all IPLs.  If there are still
 * handlers using the given vector, this function should
 * disable the given vector from happening below the lowest
 * IPL of the remaining hadlers.
 */
/*ARGSUSED*/
static int
apic_delspl(int irqno, int ipl, int min_ipl, int max_ipl)
{
	uchar_t vector, bind_cpu;
	int	iflag, intin;
	volatile int32_t *ioapic;

	apic_irq_table[irqno]->airq_share--;

	if (ipl < max_ipl)
	    return (PSM_SUCCESS);

	/* return if it is not hardware interrupt */
	if (apic_irq_table[irqno]->airq_mps_intr_index == RESERVE_INDEX)
		return (PSM_SUCCESS);

	if (! apic_flag) {
		/*
		 * Clear irq_struct. If two devices shared an intpt
		 * line & 1 unloaded before picinit, we are hosed. But, then
		 * we hope the machine will ...
		 */
		apic_irq_table[irqno]->airq_mps_intr_index = FREE_INDEX;
		apic_free_vector(apic_irq_table[irqno]->airq_vector);
		return (PSM_SUCCESS);
	}

	ioapic = apicioadr[apic_irq_table[irqno]->airq_ioapicindex];
	intin = apic_irq_table[irqno]->airq_intin_no;
	iflag = intr_clear();
	lock_set(&apic_ioapic_lock);
	ioapic[APIC_IO_REG] = APIC_RDT_CMD + 2 * intin;
	ioapic[APIC_IO_DATA] = AV_MASK;

	if (max_ipl == PSM_INVALID_IPL) {
		bind_cpu = apic_irq_table[irqno]->airq_temp_cpu;
		if ((uchar_t)bind_cpu != IRQ_UNBOUND) {
		    ASSERT(bind_cpu < apic_nproc);
		    if (bind_cpu & IRQ_USER_BOUND)
			/* If hardbound, temp_cpu == cpu */
			apic_cpus[bind_cpu & ~IRQ_USER_BOUND].aci_bound--;
		    else
			apic_cpus[bind_cpu].aci_temp_bound--;
		}
		lock_clear(&apic_ioapic_lock);
		intr_restore(iflag);
		ASSERT(apic_irq_table[irqno]->airq_share == 0);
		apic_irq_table[irqno]->airq_mps_intr_index = FREE_INDEX;
		apic_free_vector(apic_irq_table[irqno]->airq_vector);
		return (PSM_SUCCESS);
	}
	lock_clear(&apic_ioapic_lock);
	/*
	 * Downgrade vector to new max_ipl if needed.If we cannot allocate,
	 * use old IPL. Not very elegant, but then we hope ...
	 */
	if (apic_irq_table[irqno]->airq_ipl != max_ipl) {
	    if (vector = apic_allocate_vector(max_ipl, irqno)) {
		apic_mark_vector(apic_irq_table[irqno]->airq_vector, vector);
		apic_irq_table[irqno]->airq_vector = vector;
		apic_irq_table[irqno]->airq_ipl = (uchar_t)max_ipl;
		apic_setup_io_intr(irqno);
	    }
	}
	intr_restore(iflag);
	return (PSM_SUCCESS);
}

/*
 * Return HW interrupt number corresponding to the given IPL
 */
/*ARGSUSED*/
static int
apic_softlvl_to_irq(int ipl)
{
	/*
	 * Do not use apic to trigger soft interrupt.
	 * It will cause the system to hang when 2 hardware interrupts
	 * at the same priority with the softint are already accepted
	 * by the apic.  Cause the AV_PENDING bit will not be cleared
	 * until one of the hardware interrupt is eoi'ed.  If we need
	 * to send an ipi at this time, we will end up looping forever
	 * to wait for the AV_PENDING bit to clear.
	 */
	return (PSM_SV_SOFTWARE);
}

static int
apic_post_cpu_start()
{
	int i, cpun;
	apic_irq_t *irq_ptr;

	apic_init_intr();

	/*
	 * since some systems don't enable the internal cache on the non-boot
	 * cpus, so we have to enable them here
	 */
	setcr0(cr0() & ~(0x60000000));

	while (get_apic_cmd1() & AV_PENDING)
		apic_ret();

	cpun = psm_get_cpu_id();
	apic_cpus[cpun].aci_status = APIC_CPU_ONLINE | APIC_CPU_INTR_ENABLE;

	for (i = apic_min_device_irq; i <= apic_max_device_irq; i++)
	    if (((irq_ptr = apic_irq_table[i]) != NULL) &&
		((irq_ptr->airq_cpu & ~IRQ_USER_BOUND) == cpun) &&
		(irq_ptr->airq_mps_intr_index != FREE_INDEX))
		    (void) apic_rebind(irq_ptr, irq_ptr->airq_cpu, 1);

	return (PSM_SUCCESS);
}

processorid_t
apic_get_next_processorid(processorid_t cpu_id)
{

	int i;

	if (cpu_id == -1)
		return ((processorid_t)0);

	for (i = cpu_id + 1; i < NCPU; i++) {
		if (apic_cpumask & (1 << i))
			return (i);
	}

	return ((processorid_t)-1);
}


/*
 * type == -1 indicates it is an internal request. Do not change
 * resv_vector for these requests
 */
static int
apic_get_ipivect(int ipl, int type)
{
	uchar_t vector;
	int  irq;

	if (irq = apic_allocate_irq(APIC_VECTOR(ipl))) {
		if (vector = apic_allocate_vector(ipl, irq)) {
		    apic_irq_table[irq]->airq_mps_intr_index = RESERVE_INDEX;
		    apic_irq_table[irq]->airq_vector = vector;
		    if (type != -1) {
			apic_resv_vector[ipl] = vector;
		    }
		    return (irq);
		}
	}
	apic_error |= APIC_ERR_GET_IPIVECT_FAIL;
	return (-1);	/* shouldn't happen */
}

static int
apic_getclkirq(int ipl)
{
	int	irq;

	if ((irq = apic_get_ipivect(ipl, -1)) == -1)
		return (-1);
	/*
	 * Note the vector in apic_clkvect for per clock handling.
	 */
	apic_clkvect = apic_irq_table[irq]->airq_vector - APIC_BASE_VECT;
	if (apic_verbose)
		cmn_err(CE_NOTE, "get_clkirq: vector = %x\n", apic_clkvect);
	return (irq);
}

/*
 * Initialise the APIC timer on the local APIC of CPU 0 to the desired
 * frequency.  Note at this stage in the boot sequence, the boot processor
 * is the only active processor.
 */
static void
apic_clkinit(int hertz)
{

	uint_t apic_ticks = 0;
	uint_t	pit_time;

	apicadr[APIC_DIVIDE_REG] = 0x0;
	apicadr[APIC_INIT_COUNT] = APIC_MAXVAL;

	/* set periodic interrupt based on CLKIN */
	apicadr[APIC_LOCAL_TIMER] = (apic_clkvect + APIC_BASE_VECT) | AV_TIME;
	tenmicrosec();

	apic_ticks = apic_calibrate(apicadr);

	/*
	 * pit time is the amount of real time (in nanoseconds ) it took
	 * the 8254 to decrement APIC_TIME_COUNT ticks
	 */
	pit_time =  ((longlong_t)(APIC_TIME_COUNT +
			pit_ticks_adj) * NSEC_IN_SEC) / PIT_CLK_SPEED;

	/*
	 * Determine the number of nanoseconds per APIC clock tick
	 * and then determine how many APIC ticks to interrupt at the
	 * desired frequency
	 */
	apic_nsec_per_tick = pit_time / apic_ticks;
	apic_nsec_per_intr = NSEC_IN_SEC / hertz;
	apic_hertz_count =  ((longlong_t)apic_nsec_per_intr *
				apic_ticks) / pit_time;

	/* program the local APIC to interrupt at the given frequency */
	apicadr[APIC_INIT_COUNT] = apic_hertz_count;
	apicadr[APIC_LOCAL_TIMER] = (apic_clkvect + APIC_BASE_VECT) | AV_TIME;

	apic_ticks_for_redistribution = hertz + 1;
	apic_int_busy_mark = (apic_int_busy_mark *
				apic_ticks_for_redistribution)/100;
	apic_int_free_mark = (apic_int_free_mark *
				apic_ticks_for_redistribution)/100;
	apic_diff_for_redistribution = (apic_diff_for_redistribution *
					apic_ticks_for_redistribution)/100;
}

static void
apic_shutdown()
{
	int iflag;
	int i, j;
	volatile int32_t *ioapic;

	/* Send NMI to all CPUs except self to do per processor shutdown */
	iflag = intr_clear();
	while (get_apic_cmd1() & AV_PENDING)
		apic_ret();
	apic_shutdown_processors = 1;
	apicadr[APIC_INT_CMD1]  = AV_NMI | AV_LEVEL | AV_SH_ALL_EXCSELF;

	/*	restore cmos shutdown byte before reboot	*/
	if (apic_cmos_ssb_set) {
		outb(CMOS_ADDR, SSB);
		outb(CMOS_DATA, 0);
	}
	/* Disable the I/O APIC redirection entries */
	for (j = 0; j < apic_io_max; j++) {
		int	intin_max;
		ioapic = apicioadr[j];
		ioapic[APIC_IO_REG] = APIC_VERS_CMD;
		/* Bits 23-16 define the maximum redirection entries */
		intin_max = (ioapic[APIC_IO_DATA] >> 16) & 0xff;
		for (i = 0; i < intin_max; i++) {
			ioapic[APIC_IO_REG] = APIC_RDT_CMD + 2*i;
			ioapic[APIC_IO_DATA] = AV_MASK;
		}
	}

	/*	disable apic mode if imcr present	*/
	if (apic_imcrp) {
		outb(APIC_IMCR_P1, (uchar_t)APIC_IMCR_SELECT);
		outb(APIC_IMCR_P2, (uchar_t)APIC_IMCR_PIC);
	}
	apic_disable_local_apic();

	intr_restore(iflag);

}

/*
 * Try and disable all interrupts. We just assign interrupts to other
 * processors based on policy. If any were bound by user request, we
 * let them continue and return failure. We do not bother to check
 * for cache affinity while rebinding.
 */

static int
apic_disable_intr(processorid_t cpun)
{
	int bind_cpu = 0, i, hardbound = 0;
	apic_irq_t *irq_ptr;

	if (cpun == 0)
		return (PSM_FAILURE);

	lock_set(&apic_ioapic_lock);
	apic_cpus[cpun].aci_status &= ~APIC_CPU_INTR_ENABLE;
	lock_clear(&apic_ioapic_lock);
	apic_cpus[cpun].aci_curipl = 0;
	i = apic_min_device_irq;
	for (; i <= apic_max_device_irq; i++) {
	/*
	 * If there are bound interrupts on this cpu, then
	 * rebind them to other processors.
	 */
	    if ((irq_ptr = apic_irq_table[i]) != NULL) {
		ASSERT((irq_ptr->airq_temp_cpu == IRQ_UNBOUND) ||
			(irq_ptr->airq_temp_cpu < apic_nproc));
		if (irq_ptr->airq_temp_cpu == (cpun | IRQ_USER_BOUND)) {
		    hardbound = 1;
		    continue;
		}
		if (irq_ptr->airq_temp_cpu == cpun) {
		    do {
			apic_next_bind_cpu += 2;
			bind_cpu = apic_next_bind_cpu/2;
			if (bind_cpu >= apic_nproc) {
			    apic_next_bind_cpu = 1;
			    bind_cpu = 0;
			}
		    } while (apic_rebind(irq_ptr, bind_cpu, 1));
		}
	    }
	}
	if (hardbound) {
		cmn_err(CE_WARN, "Could not disable interrupts on %d"
			"due to user bound interrupts", cpun);
		return (PSM_FAILURE);
	}
	else
		return (PSM_SUCCESS);
}

static void
apic_enable_intr(processorid_t cpun)
{
	int	i;
	apic_irq_t *irq_ptr;

	lock_set(&apic_ioapic_lock);
	apic_cpus[cpun].aci_status |= APIC_CPU_INTR_ENABLE;
	lock_clear(&apic_ioapic_lock);

	i = apic_min_device_irq;
	for (i = apic_min_device_irq; i <= apic_max_device_irq; i++) {
	    if ((irq_ptr = apic_irq_table[i]) != NULL) {
		if ((irq_ptr->airq_cpu & ~IRQ_USER_BOUND) == cpun) {
		    (void) apic_rebind(irq_ptr, irq_ptr->airq_cpu, 1);
		}
	    }
	}
}


static int
apic_translate_irq(dev_info_t *dip, int irqno)
{
	char dev_type[16];
	int dev_len, pci_irq, len, rc, newirq, bustype, devid, busid, i;
	ddi_acc_handle_t cfg_handle;
	uchar_t ipin;
	struct apic_io_intr *intrp;
	pci_regspec_t *pci_rp;
	iflag_t intr_flag;
	acpi_apic_header_t *hp;
	acpi_iso_t *isop;

	bustype = 0;

	if (apic_defconf)
		goto defconf;

	if ((dip == NULL) || (!apic_irq_translate && !apic_enable_acpi))
		goto nonpci;

	dev_len = sizeof (dev_type);
	if (ddi_getlongprop_buf(DDI_DEV_T_NONE, ddi_get_parent(dip),
		DDI_PROP_DONTPASS, "device_type", (caddr_t)dev_type,
		&dev_len) != DDI_PROP_SUCCESS) {
		goto nonpci;
	}

	if (strcmp(dev_type, "pci") == 0) {
		/* pci device */
		rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
			(uint_t *)&len);
		if ((rc != DDI_SUCCESS) || (len <
			(sizeof (pci_regspec_t) / sizeof (int))))
			goto nonpci;
		devid = (int)PCI_REG_DEV_G(pci_rp->pci_phys_hi);
		busid = (int)PCI_REG_BUS_G(pci_rp->pci_phys_hi);
		if (busid == 0 && apic_pci_bus_total == 1)
			busid = (int)apic_single_pci_busid;
		ddi_prop_free(pci_rp);

		if (pci_config_setup(dip, &cfg_handle) != DDI_SUCCESS)
			goto nonpci;
		ipin = pci_config_getb(cfg_handle, PCI_CONF_IPIN) - PCI_INTA;
		pci_config_teardown(&cfg_handle);
		if (apic_enable_acpi) {
			pci_irq = acpi_translate_pci_irq(dip, busid, devid,
					ipin, &intr_flag);
			if (pci_irq == -1)
				goto nonpci;
			intr_flag.bustype = BUS_PCI;
			if ((newirq = apic_setup_irq_table(dip, pci_irq, NULL,
				irqno, &intr_flag)) == -1)
				goto nonpci;
			return (newirq);
		} else {
			pci_irq = ((devid & 0x1f) << 2) | (ipin & 0x3);
			if ((intrp = apic_find_io_intr_w_busid(pci_irq, busid))
				== NULL) {
				if ((pci_irq = apic_handle_pci_pci_bridge(dip,
					devid, ipin, &intrp, NULL)) == -1)
					goto nonpci;
			}
			if ((newirq = apic_setup_irq_table(dip, pci_irq, intrp,
				irqno, NULL)) == -1)
				goto nonpci;
			return (newirq);
		}
	} else if (strcmp(dev_type, "isa") == 0)
		bustype = BUS_ISA;
	else if (strcmp(dev_type, "eisa") == 0)
		bustype = BUS_EISA;

nonpci:
	if (apic_enable_acpi) {
		/* search iso entries first */
		if (acpi_iso_cnt != 0) {
			hp = (acpi_apic_header_t *)acpi_isop;
			i = 0;
			while (i < acpi_iso_cnt) {
			    if (hp->type == ACPI_APIC_ISO) {
				isop = (acpi_iso_t *)hp;
				if (isop->bus == 0 && isop->source == irqno) {
					newirq = isop->int_vector;
					intr_flag.intr_po = isop->flags & 0x3;
					intr_flag.intr_el =
					    (isop->flags & 0xc >> 2);
					intr_flag.bustype = BUS_ISA;
					return (apic_setup_irq_table(dip,
						newirq, NULL, irqno,
						&intr_flag));
				}
				i++;
			    }
			    hp = (acpi_apic_header_t *)(((char *)hp) +
				hp->length);
			}
		}
		intr_flag.intr_po = INTR_PO_ACTIVE_HIGH;
		intr_flag.intr_el = INTR_EL_EDGE;
		intr_flag.bustype = BUS_ISA;
		return (apic_setup_irq_table(dip, irqno, NULL, irqno,
						&intr_flag));
	} else {
		if (bustype == 0)
			bustype = eisa_level_intr_mask ? BUS_EISA : BUS_ISA;
		for (i = 0; i < 2; i++) {
			if (((busid = apic_find_bus_id(bustype)) != -1) &&
			    ((intrp = apic_find_io_intr_w_busid(irqno, busid))
				!= NULL)) {
				if ((newirq = apic_setup_irq_table(dip, irqno,
					intrp, irqno, NULL)) != -1) {
					return (newirq);
				}
				goto defconf;
			}
			bustype = (bustype == BUS_EISA) ? BUS_ISA : BUS_EISA;
		}
	}

/* MPS default configuration */
defconf:
	newirq = apic_setup_irq_table(dip, irqno, NULL, irqno, NULL);
	if (newirq == -1)
		return (newirq);
	ASSERT(newirq == irqno);
	ASSERT(apic_irq_table[irqno]);
	return (irqno);
}

/*
 * On Compaq machines (so far this only happens on Compaq MP), pci device
 * behind a pci to pci bridge may not have an interrupt entry defined in
 * the MP table.  Instead, interrupts entries for all 4 pins of the pci to
 * pci bridge are defined in the MP table.  And the rotating scheme that
 * Compaq's BIOS is using to setup the interrupts is documented in section 11
 * - Interrupt Support in the PCI to PCI Bridge Architecture Specification
 * Revision 1.0.
 */

static int
apic_handle_pci_pci_bridge(dev_info_t *dip, int child_devno, int child_ipin,
			struct apic_io_intr **intrp, iflag_t *intr_flagp)
{
	dev_info_t *dipp;
	int pci_irq, len, rc;
	ddi_acc_handle_t cfg_handle;
	int bridge_devno, bridge_bus;
	int ipin;
	pci_regspec_t *pci_rp;
	acpi_obj pciobj;

	/*CONSTCOND*/
	while (1) {
		if ((dipp = ddi_get_parent(dip)) == (dev_info_t *)NULL)
			return (-1);
		if ((pci_config_setup(dipp, &cfg_handle) == DDI_SUCCESS) &&
			(pci_config_getb(cfg_handle, PCI_CONF_BASCLASS) ==
			PCI_CLASS_BRIDGE) && (pci_config_getb(cfg_handle,
			PCI_CONF_SUBCLASS) == PCI_BRIDGE_PCI)) {
			rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY,
				dipp, DDI_PROP_DONTPASS, "reg",
				(int **)&pci_rp, (uint_t *)&len);
			if ((rc != DDI_SUCCESS) || (len <
				(sizeof (pci_regspec_t) / sizeof (int))))
					return (-1);
			bridge_devno = (int)PCI_REG_DEV_G(pci_rp->pci_phys_hi);
			bridge_bus = (int)PCI_REG_BUS_G(pci_rp->pci_phys_hi);
			ddi_prop_free(pci_rp);
			pci_config_teardown(&cfg_handle);
			/*
			 * This is the rotating scheme that Compaq is using
			 * and documented in the pci to pci spec.  Also, if
			 * the pci to pci bridge is behind another pci to
			 * pci bridge, then it need to keep transversing
			 * up until an interrupt entry is found or reach
			 * the top of the tree
			 */
			ipin = (child_devno + child_ipin) % PCI_INTD;
			if (apic_enable_acpi) {
				if ((pciobj = acpi_find_pcibus(bridge_bus))
					!= NULL) {
					pci_irq = acpi_get_gsiv(pciobj,
						bridge_devno, ipin, intr_flagp);
					return (pci_irq);
				}
			} else {
				if (bridge_bus == 0 && apic_pci_bus_total == 1)
					bridge_bus = (int)apic_single_pci_busid;
				pci_irq = ((bridge_devno & 0x1f) << 2) |
						(ipin & 0x3);
				if ((*intrp = apic_find_io_intr_w_busid(pci_irq,
					bridge_bus)) != NULL) {
					return (pci_irq);
				}
			}
			dip = dipp;
			child_devno = bridge_devno;
			child_ipin = ipin;
		} else
			return (-1);
	}
	/*LINTED: function will not fall off the bottom */
}

/*
 * See if two intrps are compatible for sharing a vector.
 * Currently we only support sharing of PCI devices.
 */
static int
intrp_compatible(struct apic_io_intr *intrp1, struct apic_io_intr *intrp2)
{
	uint_t	level1, po1;
	uint_t	level2, po2;

	/* Assume active high by default */
	po1 = 0;
	po2 = 0;

	if (apic_find_bus(intrp1->intr_busid) != BUS_PCI)
		return (0);
	if (apic_find_bus(intrp2->intr_busid) != BUS_PCI)
		return (0);

	if (intrp1->intr_el == INTR_EL_CONFORM) {
			level1 = AV_LEVEL;
	} else
		level1 = (intrp1->intr_el == INTR_EL_LEVEL) ? AV_LEVEL : 0;

	if (level1 && ((intrp1->intr_po == INTR_PO_ACTIVE_LOW) ||
		(intrp1->intr_po == INTR_PO_CONFORM)))
		po1 = AV_ACTIVE_LOW;

	if (intrp2->intr_el == INTR_EL_CONFORM) {
			level2 = AV_LEVEL;
	} else
		level2 = (intrp2->intr_el == INTR_EL_LEVEL) ? AV_LEVEL : 0;

	if (level2 && ((intrp2->intr_po == INTR_PO_ACTIVE_LOW) ||
		(intrp2->intr_po == INTR_PO_CONFORM)))
		po2 = AV_ACTIVE_LOW;

	if ((level1 == level2) && (po1 == po2))
		return (1);

	return (0);
}

/*
 * See if two irqs are compatible for sharing a vector.
 * Currently we only support sharing of PCI devices.
 */
static int
acpi_intr_compatible(iflag_t *iflagp1, iflag_t *iflagp2)
{
	uint_t	level1, po1;
	uint_t	level2, po2;

	/* Assume active high by default */
	po1 = 0;
	po2 = 0;

	if (iflagp1->bustype != iflagp2->bustype || iflagp1->bustype != BUS_PCI)
		return (0);

	if (iflagp1->intr_el == INTR_EL_CONFORM)
		level1 = AV_LEVEL;
	else
		level1 = (iflagp1->intr_el == INTR_EL_LEVEL) ? AV_LEVEL : 0;

	if (level1 && ((iflagp1->intr_po == INTR_PO_ACTIVE_LOW) ||
		(iflagp1->intr_po == INTR_PO_CONFORM)))
		po1 = AV_ACTIVE_LOW;

	if (iflagp2->intr_el == INTR_EL_CONFORM)
		level2 = AV_LEVEL;
	else
		level2 = (iflagp2->intr_el == INTR_EL_LEVEL) ? AV_LEVEL : 0;

	if (level2 && ((iflagp2->intr_po == INTR_PO_ACTIVE_LOW) ||
		(iflagp2->intr_po == INTR_PO_CONFORM)))
		po2 = AV_ACTIVE_LOW;

	if ((level1 == level2) && (po1 == po2))
		return (1);

	return (0);
}

static int
acpi_translate_pci_irq(dev_info_t *dip, int busid, int devid, int ipin,
	iflag_t *intr_flagp)
{
	acpi_obj pciobj;
	int pci_irq;

	if ((pciobj = acpi_find_pcibus(busid)) == NULL) {
		pci_irq = apic_handle_pci_pci_bridge(dip, devid, ipin, NULL,
				intr_flagp);
	} else
		pci_irq = acpi_get_gsiv(pciobj, devid, ipin, intr_flagp);
	return (pci_irq);
}

static acpi_obj
acpi_find_pcibus(int busno)
{
	acpi_obj busobj;
	int hid, bbn;

	busobj = acpi_childdev(acpi_sbobj);
	while (busobj != NULL) {
		if (acpi_eval_int(busobj, "_HID", &hid) == ACPI_OK) {
			if (hid == HID_PCI_BUS) {
				if (acpi_eval_int(busobj, "_BBN", &bbn) ==
					ACPI_OK) {
					if (bbn == busno)
						return (busobj);
				} else if (busno == 0)
					return (busobj);
			}
		}
		busobj = acpi_nextdev(busobj);
	}
	return (NULL);
}

static int
acpi_eval_int(acpi_obj dev, char *method, int *rint)
{
	acpi_val_t *rv;
	acpi_obj mobj;

	if ((mobj = acpi_findobj(dev, method, ACPI_EXACT)) != NULL) {
		if (acpi_eval(mobj, NULL, &rv) == ACPI_OK) {
			if (rv->type == ACPI_INTEGER) {
				*rint = rv->acpi_ival;
				acpi_val_free(rv);
				return (ACPI_OK);
			} else
				acpi_val_free(rv);
		}
	}
	return (ACPI_EOBJ);
}

static int
acpi_get_gsiv(acpi_obj pciobj, int devno, int ipin, iflag_t *intr_flagp)
{
	acpi_obj prtobj, srcobj;
	acpi_val_t *prtpkg, *pkgp, *addr_vp, *pin_vp, *src_vp, *idx_vp;
	int	dev_adr, irq, nopkg, i;

	if ((prtobj = acpi_findobj(pciobj, "_PRT", ACPI_EXACT)) == NULL)
		return (-1);
	if (acpi_eval(prtobj, NULL, &prtpkg) != ACPI_OK)
		return (-1);
	if (prtpkg->type != ACPI_PACKAGE)
		goto error;

	dev_adr = (devno << 16 | 0xffff);
	nopkg = prtpkg->length;
	for (i = 0; i < nopkg; i++) {
		pkgp = ACPI_PKG_N(prtpkg, i);
		if ((pkgp->type != ACPI_PACKAGE) || (pkgp->length != 4))
			break;
		addr_vp = ACPI_PKG_N(pkgp, ACPI_PRT_ADDR_IDX);
		pin_vp = ACPI_PKG_N(pkgp, ACPI_PRT_PIN_IDX);
		if (addr_vp->type != ACPI_INTEGER ||
			pin_vp->type != ACPI_INTEGER)
			break;
		if (addr_vp->acpi_ival == dev_adr &&
		    pin_vp->acpi_ival == ipin) {
			src_vp = ACPI_PKG_N(pkgp, ACPI_PRT_SRC_IDX);
			if (src_vp->type == ACPI_INTEGER &&
			    src_vp->acpi_ival == 0) {
				/* global interrupt vector from source index */
				idx_vp = ACPI_PKG_N(pkgp, ACPI_PRT_SRCIND_IDX);
				if (idx_vp->type == ACPI_INTEGER) {
					irq = idx_vp->acpi_ival;
					intr_flagp->intr_el = INTR_EL_LEVEL;
					intr_flagp->intr_po =
							INTR_PO_ACTIVE_LOW;
					acpi_val_free(prtpkg);
					return (irq);
				}
			} else if (src_vp->type == ACPI_STRING) {
				if ((srcobj = acpi_findobj(prtobj,
					src_vp->acpi_valp, 0)) == NULL)
					break;
				idx_vp = ACPI_PKG_N(pkgp, ACPI_PRT_SRCIND_IDX);
				if (idx_vp->type == ACPI_INTEGER) {
					irq = acpi_eval_lnk(srcobj, intr_flagp);
					acpi_val_free(prtpkg);
					return (irq);
				}
			}
			break;
		}
	}
error:
	acpi_val_free(prtpkg);
	return (-1);
}

static int
acpi_eval_lnk(acpi_obj lnkobj, iflag_t *intr_flagp)
{
	acpi_obj crs_obj;
	uchar_t *rp;
	ushort_t irq_mask;
	int irq;
	acpi_val_t *rv;

	if ((crs_obj = acpi_findobj(lnkobj, "_CRS", ACPI_EXACT)) == NULL)
		return (-1);
	if (! acpi_eval(crs_obj, NULL, &rv))
		return (-1);
	/* it has to be of type ACPI_BUFFER */
	if (rv->type != ACPI_BUFFER)
		goto error;

	rp = rv->acpi_valp;
	if (rp[RES_TYPE] == ACPI_EXT_IRQ_TYPE) {
		/* extended Interrupt Descriptor */
		if (rp[EXT_IRQ_TBL_LEN] == 1) {
			irq = *((int *)(rp + EXT_IRQ_NO));
			if (rp[EXT_IRQ_FLAG] & EXT_IFLAG_HE) {
				intr_flagp->intr_el = INTR_EL_EDGE;
				intr_flagp->intr_po = INTR_PO_ACTIVE_HIGH;
			} else {
				intr_flagp->intr_el = INTR_EL_LEVEL;
				intr_flagp->intr_po = INTR_PO_ACTIVE_LOW;
			}
			acpi_val_free(rv);
			return (irq);
		}
	} else if (rp[RES_TYPE] == ACPI_REG_IRQ_TYPE1 ||
			rp[RES_TYPE] == ACPI_REG_IRQ_TYPE2) {
		/* small resource irq Descriptor - shouldn't happen */
		irq_mask = *(ushort_t *)(rp + REG_IRQ_MASK);
		for (irq = 0; irq < 16; irq++) {
			if (irq_mask & (1 << irq)) {
				if (rp[RES_TYPE] == ACPI_REG_IRQ_TYPE1 ||
					rp[REG_IRQ_FLAG] & REG_IFLAG_HE) {
					intr_flagp->intr_el = INTR_EL_EDGE;
					intr_flagp->intr_po =
							INTR_PO_ACTIVE_HIGH;
				} else {
					intr_flagp->intr_el = INTR_EL_LEVEL;
					intr_flagp->intr_po =
							INTR_PO_ACTIVE_LOW;
				}
				acpi_val_free(rv);
				return (irq);
			}
		}
	}
error:
	acpi_val_free(rv);
	return (-1);
}

static uchar_t
acpi_find_ioapic(int irq)
{
	int i;

	for (i = 0; i < apic_io_max; i++) {
		if (irq >= apic_io_vectbase[i] && irq <= apic_io_vectend[i])
			return (i);
	}
	return (0xFF);	/* shouldn't happen */
}

static int
apic_setup_irq_table(dev_info_t *dip, int irqno, struct apic_io_intr *intrp,
	int origirq, iflag_t *intr_flagp)
{
	struct intrspec *ispec = NULL;
	int		newirq, intr_index, share = 127;
	uchar_t	ipin, ioapic, ioapicindex, vector, ipl = 0;
	apic_irq_t *irqptr;
	major_t	major;

	if (dip != NULL) {
	/*
	 * Change PSMI to pass in the IPL also.
	 * Till then we will use this hack to avoid dependency on rootnex
	 */
	    int	inum = 0;

	    /* get the named interrupt specification */
	    while ((ispec = (struct intrspec *)
		i_ddi_get_intrspec(dip, dip, inum)) != NULL) {
		if (ispec->intrspec_vec == origirq) {
		    ipl = ispec->intrspec_pri;
		    break;
		}
		inum++;
	    }
	    major = ddi_name_to_major(ddi_get_name(dip));
	} else
	    major = 0;
	if (intrp != NULL) {
	    intr_index = (int)(intrp -  apic_io_intrp);
	    ioapic = intrp->intr_destid;
	    ipin = intrp->intr_destintin;
	    /* Find ioapicindex. If destid was ALL, we will exit with 0. */
	    for (ioapicindex = apic_io_max - 1; ioapicindex; ioapicindex--)
		if (apic_io_id[ioapicindex] == ioapic)
			break;
	    ASSERT((ioapic == apic_io_id[ioapicindex]) ||
		(ioapic == INTR_ALL_APIC));

	    /* Now check whether this intin# has been used by another irqno */
	    if ((newirq = apic_find_intin(ioapicindex, ipin)) != -1) {
		apic_irq_table[newirq]->airq_share++;
		return (newirq);
	    }
	} else if (intr_flagp != NULL) {
		/* ACPI case */
		intr_index = ACPI_INDEX;
		ioapicindex = acpi_find_ioapic(irqno);
		ASSERT(ioapicindex != 0xFF);
		ioapic = apic_io_id[ioapicindex];
		ipin = irqno - apic_io_vectbase[ioapicindex];
		if (apic_irq_table[irqno] &&
		    apic_irq_table[irqno]->airq_mps_intr_index == ACPI_INDEX) {
			ASSERT(apic_irq_table[irqno]->airq_intin_no == ipin &&
				apic_irq_table[irqno]->airq_ioapicindex ==
					ioapicindex);
			return (irqno);
		}
	} else {
	    /* default configuration */
	    ioapicindex = 0;
	    ioapic = apic_io_id[ioapicindex];
	    ipin = (uchar_t)irqno;
	    intr_index = DEFAULT_INDEX;
	}
	if (ispec != NULL) {
	    vector = apic_allocate_vector(ipl, irqno);
	    if (vector == 0) {
		int chosen_irq;
		if (intrp == NULL && intr_flagp == NULL)
		    goto apic_setup_irq_table_error1;
		/* Run out of vector. Just share with some one else */
		newirq = apic_min_device_irq;
		for (; newirq <= apic_max_device_irq; newirq++) {
		    if ((apic_irq_table[newirq] != NULL) &&
			(apic_irq_table[newirq]->airq_ipl == ipl)) {

			irqptr = apic_irq_table[newirq];
			intr_index = irqptr->airq_mps_intr_index;
			if ((intr_index == DEFAULT_INDEX) ||
			    (intr_index == FREE_INDEX))
			    continue;

			if (intrp != NULL) {
				if (!intrp_compatible((apic_io_intrp +
					intr_index), intrp))
					continue;
			} else {
				if (!acpi_intr_compatible(&irqptr->airq_iflag,
					intr_flagp))
					continue;
			}
			if (irqptr->airq_share < share) {
			    share = irqptr->airq_share;
			    chosen_irq = newirq;
			}
		    }
		}
		if (chosen_irq) {
		    apic_irq_table[irqno]->airq_share++;
		    return (irqno);
		}
apic_setup_irq_table_error1:
		cmn_err(CE_WARN, "No vector for interrupt %d", origirq);
		return (-1);
	    }
	} else {
	    if (apic_verbose)
		cmn_err(CE_WARN, "No intrspec for irqno = %x", irqno);
	}
	if (apic_irq_table[irqno] == NULL) {
	    apic_irq_table[irqno] =
		kmem_zalloc(sizeof (apic_irq_t), KM_NOSLEEP);
	    if (apic_irq_table[irqno] == NULL) {
		cmn_err(CE_WARN, "pcplusmp: translate_irq "
			"No memory to allocate irq table");
		return (-1);
	    }
	} else {
	    /* won't happen with ACPI */
	    if ((intrp != NULL) &&
		(apic_irq_table[irqno]->airq_mps_intr_index != FREE_INDEX)) {
		/*
		 * The slot is used by another irqno, so allocate a free
		 * irqno for this interrupt
		 */
		    newirq = apic_allocate_irq(APIC_FIRST_FREE_IRQ);
		    if (newirq == 0)
			return (-1);
		    irqno = newirq;
		    apic_modify_vector(vector, newirq);
	    }
	}
	apic_irq_table[irqno]->airq_ioapicindex = ioapicindex;
	apic_irq_table[irqno]->airq_intin_no = ipin;
	apic_irq_table[irqno]->airq_ipl = ipl;
	apic_irq_table[irqno]->airq_vector = vector;
	apic_irq_table[irqno]->airq_share++;
	apic_irq_table[irqno]->airq_mps_intr_index = (short)intr_index;
	apic_irq_table[irqno]->airq_dip = dip;
	apic_irq_table[irqno]->airq_major = major;
	apic_irq_table[irqno]->airq_cpu =
			apic_bind_intr(dip, irqno, ioapic, ipin);
	apic_irq_table[irqno]->airq_temp_cpu = IRQ_UNBOUND;
	if (intr_flagp)
		apic_irq_table[irqno]->airq_iflag = *intr_flagp;
	apic_max_device_irq = max(irqno, apic_max_device_irq);
	apic_min_device_irq = min(irqno, apic_min_device_irq);
	return (irqno);
}

/*
 * return the cpu to which this intr should be bound.
 * Check properties or any other mechanism to see if user wants it
 * bound to a specific CPU. If so, return the cpu id with high bit set.
 * If not, use the policy to choose a cpu and return the id.
 */
static uchar_t
apic_bind_intr(dev_info_t *dip, int irq, uchar_t ioapicid, uchar_t intin)
{
	int	instance, instno, prop_len, bind_cpu, count;
	int	i, rc;
	uchar_t	cpu;
	major_t	major;
	char	*name, *drv_name, *prop_val, *cptr;
	char	prop_name[32];


	if (apic_intr_policy == INTR_LOWEST_PRIORITY)
		return (IRQ_UNBOUND);

	drv_name = NULL;
	rc = DDI_PROP_NOT_FOUND;
	major = (major_t)-1;
	if (dip != NULL) {
		name = ddi_get_name(dip);
		major = ddi_name_to_major(name);
		drv_name = ddi_major_to_name(major);
		instance = ddi_get_instance(dip);
		if (apic_intr_policy == INTR_ROUND_ROBIN_WITH_AFFINITY) {
		    i = apic_min_device_irq;
		    for (; i <= apic_max_device_irq; i++) {

			if ((i == irq) || (apic_irq_table[i] == NULL) ||
			    (apic_irq_table[i]->airq_mps_intr_index
				== FREE_INDEX))
				continue;

			if ((apic_irq_table[i]->airq_major == major) &&
			    (!(apic_irq_table[i]->airq_cpu & IRQ_USER_BOUND))) {
			    cpu = apic_irq_table[i]->airq_cpu;
			    cmn_err(CE_CONT, "!pcplusmp: %s (%s) instance #%d "
				"vector 0x%x ioapic 0x%x intin 0x%x is bound "
				"to cpu %d\n", name, drv_name, instance, irq,
				ioapicid, intin, cpu);
			    return (cpu);
			}
		    }
		}
		/*
		 * search for "drvname"_intpt_bind_cpus property first, the
		 * syntax of the property should be "a[,b,c,...]" where
		 * instance 0 binds to cpu a, instance 1 binds to cpu b,
		 * instance 3 binds to cpu c...
		 * ddi_getlongprop() will search /option first, then /
		 * if "drvname"_intpt_bind_cpus doesn't exist, then find
		 * intpt_bind_cpus property.  The syntax is the same, and
		 * it applies to all the devices if its "drvname" specific
		 * property doesn't exist
		 */
		(void) strcpy(prop_name, drv_name);
		(void) strcat(prop_name, "_intpt_bind_cpus");
		rc = ddi_getlongprop(DDI_DEV_T_ANY, dip, 0, prop_name,
			(caddr_t)&prop_val, &prop_len);
		if (rc != DDI_PROP_SUCCESS) {
			rc = ddi_getlongprop(DDI_DEV_T_ANY, dip, 0,
				"intpt_bind_cpus", (caddr_t)&prop_val,
				&prop_len);
		}
	}
	if (rc == DDI_PROP_SUCCESS) {
		for (i = count = 0; i < (prop_len - 1); i++)
			if (prop_val[i] == ',')
				count++;
		if (prop_val[i-1] != ',')
			count++;
		/*
		 * if somehow the binding instances defined in the
		 * property are not enough for this instno., then
		 * reuse the pattern for the next instance until
		 * it reaches the requested instno
		 */
		instno = instance % count;
		i = 0;
		cptr = prop_val;
		while (i < instno)
			if (*cptr++ == ',')
				i++;
		bind_cpu = stoi(&cptr);
		kmem_free(prop_val, prop_len);
		/* if specific cpu is bogus, then default to cpu 0 */
		if (bind_cpu >= apic_nproc) {
			cmn_err(CE_WARN, "pcplusmp: %s=%s: CPU %d not present",
				prop_name, prop_val, bind_cpu);
			bind_cpu = 0;
		} else {
			/* indicate that we are bound at user request */
			bind_cpu |= IRQ_USER_BOUND;
		}
		/*
		 * no need to check apic_cpus[].aci_status, if specific cpu is
		 * not up, then post_cpu_start will handle it.
		 */
	} else {
		/*
		 * We change bind_cpu only for every two calls
		 * as most drivers still do 2 add_intrs for every
		 * interrupt
		 */
		bind_cpu = (apic_next_bind_cpu++)/2;
		if (bind_cpu >= apic_nproc) {
			apic_next_bind_cpu = 1;
			bind_cpu = 0;
		}
	}
	if (drv_name != NULL)
		cmn_err(CE_CONT, "!pcplusmp: %s (%s) instance %d "
		    "vector 0x%x ioapic 0x%x intin 0x%x is bound to cpu %d\n",
		    name, drv_name, instance,
		    irq, ioapicid, intin, bind_cpu & ~IRQ_USER_BOUND);
	else
		cmn_err(CE_CONT, "!pcplusmp: "
		    "vector 0x%x ioapic 0x%x intin 0x%x is bound to cpu %d\n",
		    irq, ioapicid, intin, bind_cpu & ~IRQ_USER_BOUND);

	return ((uchar_t)bind_cpu);
}

static struct apic_io_intr *
apic_find_io_intr_w_busid(int irqno, int busid)
{
	struct	apic_io_intr	*intrp;

	/*
	 * It can have more than 1 entry with same source bus IRQ,
	 * but unique with the source bus id
	 */
	intrp = apic_io_intrp;
	if (intrp != NULL) {
		while (intrp->intr_entry == APIC_IO_INTR_ENTRY) {
			if (intrp->intr_irq == irqno &&
				intrp->intr_busid == busid &&
				intrp->intr_type == IO_INTR_INT)
				return (intrp);
			intrp++;
		}
	}
	if (apic_verbose)
	    cmn_err(CE_NOTE, "Did not find io intr for irqno:busid %x:%x",
		irqno, busid);
	return ((struct apic_io_intr *)NULL);
}


struct mps_bus_info {
	char	*bus_name;
	int	bus_id;
} bus_info_array[] = {"ISA ", BUS_ISA,
		    "PCI ", BUS_PCI,
		    "EISA ", BUS_EISA,
		    "XPRESS", BUS_XPRESS,
		    "PCMCIA", BUS_PCMCIA,
		    "VL ", BUS_VL,
		    "CBUS ", BUS_CBUS,
		    "CBUSII", BUS_CBUSII,
		    "FUTURE", BUS_FUTURE,
		    "INTERN", BUS_INTERN,
		    "MBI ", BUS_MBI,
		    "MBII ", BUS_MBII,
		    "MPI ", BUS_MPI,
		    "MPSA ", BUS_MPSA,
		    "NUBUS ", BUS_NUBUS,
		    "TC ", BUS_TC,
		    "VME ", BUS_VME};

static int
apic_find_bus_type(char *bus)
{
	int	i = 0;

	for (; i < sizeof (bus_info_array)/sizeof (struct mps_bus_info); i++)
		if (strncmp(bus, bus_info_array[i].bus_name,
			strlen(bus_info_array[i].bus_name)) == 0)
				return (bus_info_array[i].bus_id);
	if (apic_verbose)
	    cmn_err(CE_WARN, "Did not find bus type for bus %s", bus);
	return (0);
}

static int
apic_find_bus(int busid)
{
	struct	apic_bus	*busp;

	busp = apic_busp;
	while (busp->bus_entry == APIC_BUS_ENTRY) {
		if (busp->bus_id == busid)
			return (apic_find_bus_type((char *)&busp->bus_str1));
		busp++;
	}
	if (apic_verbose)
	    cmn_err(CE_WARN, "Did not find bus for bus id %x", busid);
	return (0);
}

static int
apic_find_bus_id(int bustype)
{
	struct	apic_bus	*busp;

	busp = apic_busp;
	while (busp->bus_entry == APIC_BUS_ENTRY) {
		if (apic_find_bus_type((char *)&busp->bus_str1) == bustype)
			return (busp->bus_id);
		busp++;
	}
	if (apic_verbose)
	    cmn_err(CE_WARN, "Did not find bus id for bustype %x", bustype);
	return (-1);
}

/*
 * Check if a particular irq need to be reserved for any io_intr
 */
static struct apic_io_intr *
apic_find_io_intr(int irqno)
{
	struct	apic_io_intr	*intrp;

	intrp = apic_io_intrp;
	if (intrp != NULL) {
		while (intrp->intr_entry == APIC_IO_INTR_ENTRY) {
			if (intrp->intr_irq == irqno &&
				intrp->intr_type == IO_INTR_INT)
				return (intrp);
			intrp++;
		}
	}
	return ((struct apic_io_intr *)NULL);
}

/*
 * Check if the given ioapicindex intin combination has already been assigned
 * an irq. If so return irqno. Else -1
 */
static int
apic_find_intin(uchar_t ioapic, uchar_t intin)
{
	int	i;

	/* find ioapic and intin in the apic_irq_table[] and return the index */
	for (i = 0; i <= APIC_MAX_VECTOR; i++) {
	    if (apic_irq_table[i] &&
		(apic_irq_table[i]->airq_mps_intr_index >= 0) &&
		(apic_irq_table[i]->airq_intin_no == intin) &&
		(apic_irq_table[i]->airq_ioapicindex == ioapic)) {
		if (apic_verbose)
		    cmn_err(CE_NOTE, "Found irq entry for ioapic:intin %x:%x "
			"shared interrupts ?", ioapic, intin);
		return (i);
	    }
	}
	return (-1);
}

static int
apic_allocate_irq(int irq)
{
	int	freeirq, i;

	if ((freeirq = apic_find_free_irq(irq, (APIC_RESV_IRQ - 1))) == -1)
	    if ((freeirq = apic_find_free_irq(APIC_FIRST_FREE_IRQ,
		(irq - 1))) == -1) {
		/*
		 * if BIOS really defines every single irq in the mps
		 * table, then don't worry about conflicting with
		 * them, just use any free slot in apic_irq_table
		 */
		for (i = APIC_FIRST_FREE_IRQ; i < APIC_RESV_IRQ; i++) {
		    if ((apic_irq_table[i] == NULL) ||
			apic_irq_table[i]->airq_mps_intr_index == FREE_INDEX) {
			freeirq = i;
			break;
		    }
		}
		if (freeirq == -1) {
		    /* This shouldn't happen, but just in case */
		    cmn_err(CE_WARN, "pcplusmp: NO available IRQ");
		    return (0);
		}
	    }
	if (apic_irq_table[freeirq] == NULL) {
	    apic_irq_table[freeirq] =
		kmem_zalloc(sizeof (apic_irq_t), KM_NOSLEEP);
	    if (apic_irq_table[freeirq] == NULL) {
		cmn_err(CE_WARN, "pcplusmp: NO memory to allocate IRQ");
		return (0);
	    }
	}
	return (freeirq);
}

static int
apic_find_free_irq(int start, int end)
{
	int	i;

	for (i = start; i <= end; i++)
	    /* Check if any I/O entry needs this IRQ */
	    if (apic_find_io_intr(i) == NULL) {
		/* Then see if it is free */
		if ((apic_irq_table[i] == NULL) ||
		    (apic_irq_table[i]->airq_mps_intr_index == FREE_INDEX)) {
			return (i);
		}
	    }
	return (-1);
}

static uchar_t
apic_allocate_vector(int ipl, int irq)
{
	int	lowest, highest, i;


	highest = apic_ipltopri[ipl] + APIC_VECTOR_MASK - APIC_BASE_VECT;
	lowest = apic_ipltopri[ipl-1] + APIC_VECTOR_PER_IPL - APIC_BASE_VECT;

	if (highest < lowest) /* Both ipl and ipl-1 map to same pri */
		lowest -= APIC_VECTOR_PER_IPL;

	for (i = lowest; i < highest; i++) {
		if (i + APIC_BASE_VECT == T_FASTTRAP)
			continue;

		if ((apic_vector_to_irq[i] == APIC_RESV_IRQ) &&
			(i != APIC_SPUR_INTR)) {
			apic_vector_to_irq[i] = (uchar_t)irq;
			return (i+APIC_BASE_VECT);
		}
	}

	return (0);
}

static void
apic_modify_vector(uchar_t vector, int irq)
{
	apic_vector_to_irq[vector - APIC_BASE_VECT] = (uchar_t)irq;
}

/*
 * Mark vector as being in the process of being deleted. Interrupts
 * may still come in on some CPU. The moment an interrupt comes with
 * the new vector, we know we can free the old one. Called only from
 * addspl and delspl with interrupts disabled. Because an interrupt
 * can be shared, but no interrupt from either device may come in,
 * we also use a timeout mechanism, which we arbitrarily set to 16
 * clock interrupts.
 */
static void
apic_mark_vector(uchar_t oldvector, uchar_t newvector)
{
	lock_set(&apic_revector_lock);
	if (!apic_oldvec_to_newvec) {
	    apic_oldvec_to_newvec =
		kmem_zalloc(sizeof (newvector) * APIC_MAX_VECTOR * 2,
			KM_NOSLEEP);
	    if (!apic_oldvec_to_newvec) {
		/*
		 * This failure is not catastrophic.
		 * But, the oldvec will never be freed.
		 */
		apic_error |= APIC_ERR_MARK_VECTOR_FAIL;
		lock_clear(&apic_revector_lock);
		return;
	    }
	    apic_newvec_to_oldvec = &apic_oldvec_to_newvec[APIC_MAX_VECTOR];
	}

	/* See if we already did this for drivers which do double addintrs */
	if (apic_oldvec_to_newvec[oldvector] != newvector) {
		apic_oldvec_to_newvec[oldvector] = newvector;
		apic_newvec_to_oldvec[newvector] = oldvector;
		apic_revector_pending++;
	}
	lock_clear(&apic_revector_lock);
	apic_last_revector = apic_gettime();
}

/*
 * xlate_vector is called from intr_enter if revector_pending is set.
 * It will xlate it if needed and mark the old vector as free.
 */
static uchar_t
apic_xlate_vector(uchar_t vector)
{
	int	i;
	uchar_t	newvector, oldvector = 0;

	lock_set(&apic_revector_lock);
	/* Do we really need to do this ? */
	if (!apic_revector_pending) {
		lock_clear(&apic_revector_lock);
		return (vector);
	}
	if ((newvector = apic_oldvec_to_newvec[vector]) != 0)
		oldvector = vector;
	else {
	    /* The incoming vector is new . See if a stale entry is remaining */
	    if ((oldvector = apic_newvec_to_oldvec[vector]) != 0)
		newvector = vector;
	}

	if (oldvector) {
		apic_revector_pending--;
		apic_oldvec_to_newvec[oldvector] = 0;
		apic_newvec_to_oldvec[newvector] = 0;
		apic_free_vector(oldvector);
		lock_clear(&apic_revector_lock);
		/* There could have been more than one reprogramming! */
		return (apic_xlate_vector(newvector));
	}
	if ((apic_gettime() -
		(apic_last_revector + (apic_nsec_per_intr * 16))) > 0) {
		for (i = 0; i < APIC_MAX_VECTOR; i++) {
		    if ((newvector = apic_oldvec_to_newvec[i]) != 0) {
			apic_free_vector(i);
			apic_oldvec_to_newvec[i] = 0;
			apic_newvec_to_oldvec[newvector] = 0;
		    }
		}
	}
	lock_clear(&apic_revector_lock);
	return (vector);
}

/* Mark vector as not being used by any irq */
static void
apic_free_vector(uchar_t vector)
{
	apic_vector_to_irq[vector - APIC_BASE_VECT] = APIC_RESV_IRQ;
}

/*
 * compute the polarity, trigger mode and vector for programming into
 * the I/O apic and record in airq_rdt_entry. Call rebind to do the
 * actual programming.
 */
static void
apic_setup_io_intr(int irq)
{
	int	ioapicindex, bus_type, vector;
	short	intr_index;
	uint_t	level, po, io_po;
	struct apic_io_intr *iointrp;
	apic_irq_t *irqptr;

	/* Assume edge triggered by default */
	level = 0;
	/* Assume active high by default */
	po = 0;

	if ((intr_index = apic_irq_table[irq]->airq_mps_intr_index)
		== RESERVE_INDEX) {
		apic_error |= APIC_ERR_INVALID_INDEX;
		return;
	}

	ioapicindex = apic_irq_table[irq]->airq_ioapicindex;
	vector = apic_irq_table[irq]->airq_vector;

	if (intr_index == DEFAULT_INDEX || intr_index == FREE_INDEX) {
		ASSERT(irq < 16);
		if (eisa_level_intr_mask & (1 << irq))
			level = AV_LEVEL;
		if (intr_index == FREE_INDEX && apic_defconf == 0)
			apic_error |= APIC_ERR_INVALID_INDEX;
	} else if (intr_index == ACPI_INDEX) {
		irqptr = apic_irq_table[irq];
		bus_type = irqptr->airq_iflag.bustype;
		if (irqptr->airq_iflag.intr_el == INTR_EL_CONFORM) {
			if (bus_type == BUS_PCI)
				level = AV_LEVEL;
		} else
			level = (irqptr->airq_iflag.intr_el == INTR_EL_LEVEL) ?
								AV_LEVEL : 0;
		if (level && ((irqptr->airq_iflag.intr_el ==
			INTR_PO_ACTIVE_LOW) || (irqptr->airq_iflag.intr_po ==
			INTR_PO_CONFORM && bus_type == BUS_PCI)))
			po = AV_ACTIVE_LOW;
	} else {
		iointrp = apic_io_intrp + intr_index;
		bus_type = apic_find_bus(iointrp->intr_busid);
		if (iointrp->intr_el == INTR_EL_CONFORM) {
			if ((irq < 16) && (eisa_level_intr_mask & (1 << irq)))
				level = AV_LEVEL;
			else if (bus_type == BUS_PCI)
				level = AV_LEVEL;
		} else
			level = (iointrp->intr_el == INTR_EL_LEVEL) ?
								AV_LEVEL : 0;
		if (level && ((iointrp->intr_po == INTR_PO_ACTIVE_LOW) ||
			(iointrp->intr_po == INTR_PO_CONFORM &&
				bus_type == BUS_PCI)))
			po = AV_ACTIVE_LOW;
	}
	if (level)
		apic_level_intr[irq] = 1;

	if (po && ((apic_io_ver[ioapicindex] & APIC_VERS_MASK) != 0))
		io_po = po;
	else
		io_po = 0;

	if (apic_verbose)
	    printf("setio: ioapic=%x intin=%x level=%x po=%x vector=%x\n",
		ioapicindex, apic_irq_table[irq]->airq_intin_no,
		level, io_po, vector);

	apic_irq_table[irq]->airq_rdt_entry = level|io_po|vector;

	if (apic_rebind(apic_irq_table[irq], apic_irq_table[irq]->airq_cpu, 1))
		/* CPU is not up or interrupt is disabled. Fall back to 0 */
		(void) apic_rebind(apic_irq_table[irq], 0, 1);
}

/*
 * Bind interrupt corresponding to irq_ptr to bind_cpu. safe
 * if true means we are not being called from an interrupt
 * context and hence it is safe to do a lock_set. If false
 * do only a lock_try and return failure (non 0) if we cannot get it
 */
static int
apic_rebind(apic_irq_t *irq_ptr, int bind_cpu, int safe)
{
	int	intin_no;
	volatile int32_t *ioapic;
	apic_cpus_info_t *cpu_infop;

	intin_no = irq_ptr->airq_intin_no;
	ioapic = apicioadr[irq_ptr->airq_ioapicindex];

	if (!safe) {
		if (lock_try(&apic_ioapic_lock) == 0)
			return (1);
	} else
		lock_set(&apic_ioapic_lock);

	if ((uchar_t)bind_cpu == IRQ_UNBOUND) {
	    ioapic[APIC_IO_REG] = APIC_RDT_CMD2 + 2 * intin_no;
	    ioapic[APIC_IO_DATA] = AV_TOALL;
	    if (irq_ptr->airq_temp_cpu != IRQ_UNBOUND) {
		apic_cpus[irq_ptr->airq_temp_cpu].aci_temp_bound--;
	    }
	    ioapic[APIC_IO_REG] = APIC_RDT_CMD + 2 * intin_no;
	    ioapic[APIC_IO_DATA] = AV_LDEST|AV_LOPRI|irq_ptr->airq_rdt_entry;
	    lock_clear(&apic_ioapic_lock);
	    irq_ptr->airq_temp_cpu = IRQ_UNBOUND;
	    return (0);
	}

	cpu_infop = &apic_cpus[bind_cpu & ~IRQ_USER_BOUND];
	if (!(cpu_infop->aci_status & APIC_CPU_INTR_ENABLE)) {
		lock_clear(&apic_ioapic_lock);
		return (1);
	}
	if (bind_cpu & IRQ_USER_BOUND) {
		cpu_infop->aci_bound++;
	} else {
		cpu_infop->aci_temp_bound++;
	}
	ASSERT((bind_cpu & ~IRQ_USER_BOUND) < apic_nproc);
	/* bind to the cpu */
	ioapic[APIC_IO_REG] = APIC_RDT_CMD2 + 2 * intin_no;
	ioapic[APIC_IO_DATA] = cpu_infop->aci_local_id << APIC_ID_BIT_OFFSET;
	if (irq_ptr->airq_temp_cpu != IRQ_UNBOUND) {
		apic_cpus[irq_ptr->airq_temp_cpu].aci_temp_bound--;
	}
	ioapic[APIC_IO_REG] = APIC_RDT_CMD + 2 * intin_no;
	ioapic[APIC_IO_DATA] = AV_PDEST|AV_FIXED|irq_ptr->airq_rdt_entry;

	lock_clear(&apic_ioapic_lock);
	irq_ptr->airq_temp_cpu = (uchar_t)bind_cpu;
	apic_redist_cpu_skip &= ~(1 << (bind_cpu & ~IRQ_USER_BOUND));
	return (0);
}

/*
 * apic_intr_redistribute does all the messy computations for identifying
 * which interrupt to move to which CPU. Currently we do just one interrupt
 * at a time. This reduces the time we spent doing all this within clock
 * interrupt. When it is done in idle, we could do more than 1.
 * First we find the most busy and the most free CPU (time in ISR only)
 * skipping those CPUs that has been identified as being ineligible (cpu_skip)
 * Then we look for IRQs which are closest to the difference between the
 * most busy CPU and the average ISR load. We try to find one whose load
 * is less than difference.If none exists, then we chose one larger than the
 * difference, provided it does not make the most idle CPU worse than the
 * most busy one. In the end, we clear all the busy fields for CPUs. For
 * IRQs, they are cleared as they are scanned.
 */
static void
apic_intr_redistribute()
{
	int busiest_cpu, most_free_cpu;
	int cpu_free, cpu_busy, max_busy, min_busy;
	int min_free, diff;
	int	average_busy, cpus_online;
	int i, busy;
	apic_cpus_info_t *cpu_infop;
	apic_irq_t *min_busy_irq = NULL;
	apic_irq_t *max_busy_irq = NULL;

	busiest_cpu = most_free_cpu = -1;
	cpu_free = cpu_busy = max_busy = average_busy = 0;
	min_free = apic_ticks_for_redistribution;
	cpus_online = 0;
	/*
	 * Below we will check for CPU_INTR_ENABLE, bound, temp_bound, temp_cpu
	 * without ioapic_lock. That is OK as we are just doing statistical
	 * sampling anyway and any inaccuracy now will get corrected next time
	 * The call to rebind which actually changes things will make sure
	 * we are consistent.
	 */
	for (i = 0; i < apic_nproc; i++) {
	    if (!(apic_redist_cpu_skip & (1 << i)) &&
		(apic_cpus[i].aci_status & APIC_CPU_INTR_ENABLE)) {

		cpu_infop = &apic_cpus[i];
		/* If no unbound interrupts or only 1 total on this CPU, skip */
		if (!cpu_infop->aci_temp_bound ||
		    (cpu_infop->aci_bound + cpu_infop->aci_temp_bound) == 1) {
			apic_redist_cpu_skip |= 1 << i;
			continue;
		}
		busy = cpu_infop->aci_busy;
		average_busy += busy;
		cpus_online++;
		if (max_busy < busy) {
			max_busy = busy;
			busiest_cpu = i;
		}
		if (min_free > busy) {
			min_free = busy;
			most_free_cpu = i;
		}
		if (busy > apic_int_busy_mark) {
			cpu_busy |= 1 << i;
		} else {
			if (busy < apic_int_free_mark)
				cpu_free |= 1 << i;
		}
	    }
	}
	if ((cpu_busy && cpu_free) ||
	    (max_busy >= (min_free + apic_diff_for_redistribution))) {

	    apic_num_imbalance++;
#ifdef	DEBUG
	    if (apic_verbose) {
		prom_printf("redistribute busy=%x free=%x max=%x min=%x",
			cpu_busy, cpu_free, max_busy, min_free);
	    }
#endif

	    average_busy /= cpus_online;

	    diff = max_busy - average_busy;
	    min_busy = max_busy; /* start with the max possible value */
	    max_busy = 0;
	    min_busy_irq = max_busy_irq = NULL;
	    i = apic_min_device_irq;
	    for (; i < apic_max_device_irq; i++) {
		apic_irq_t *irq_ptr;
		/* Change to linked list per CPU ? */
		if ((irq_ptr = apic_irq_table[i]) == NULL) continue;
		/* Check for irq_busy & decide which one to move */
		/* Also zero them for next round */
		if ((irq_ptr->airq_temp_cpu == busiest_cpu) &&
						irq_ptr->airq_busy) {
		    if (irq_ptr->airq_busy < diff) {
			/* Check for least busy CPU, best fit or what ? */
			if (max_busy < irq_ptr->airq_busy) {
			/* Most busy within the required differential */
			    max_busy = irq_ptr->airq_busy;
			    max_busy_irq = irq_ptr;
			}
		    } else {
			if (min_busy > irq_ptr->airq_busy) {
			/* least busy, but more than the reqd diff */
			    if (min_busy < (diff + average_busy - min_free)) {
			    /* Making sure new cpu will not end up worse */
				min_busy = irq_ptr->airq_busy;
				min_busy_irq = irq_ptr;
			    }
			}
		    }
		}
		irq_ptr->airq_busy = 0;
	    }
	    if (max_busy_irq != NULL) {
#ifdef	DEBUG
		if (apic_verbose) {
		    prom_printf("rebinding %x to %x",
			max_busy_irq->airq_vector, most_free_cpu);
		}
#endif
		if (apic_rebind(max_busy_irq, most_free_cpu, 0) == 0)
			/* Make change permenant */
			max_busy_irq->airq_cpu = (uchar_t)most_free_cpu;
	    } else if (min_busy_irq != NULL) {
#ifdef	DEBUG
		if (apic_verbose) {
		    prom_printf("rebinding %x to %x",
			min_busy_irq->airq_vector, most_free_cpu);
		}
#endif
		if (apic_rebind(min_busy_irq, most_free_cpu, 0) == 0)
			/* Make change permenant */
			min_busy_irq->airq_cpu = (uchar_t)most_free_cpu;
	    } else {
		if (cpu_busy != (1 << busiest_cpu)) {
			apic_redist_cpu_skip |= 1 << busiest_cpu;
			/*
			 * We leave cpu_skip set so that next time we can
			 * choose another cpu
			 */
		}
	    }
	    apic_num_rebind++;
	} else {
		/*
		 * found nothing. Could be that we skipped over valid CPUs
		 * or we have balanced everything. If we had a variable
		 * ticks_for_redistribution, it could be increased here.
		 * apic_int_busy, int_free etc would also need to be
		 * changed.
		 */
		if (apic_redist_cpu_skip)
			apic_redist_cpu_skip = 0;
	}
	for (i = 0; i < apic_nproc; i++) {
		apic_cpus[i].aci_busy = 0;
	}
}

static void
apic_cleanup_busy()
{
	int i;
	apic_irq_t *irq_ptr;

	for (i = 0; i < apic_nproc; i++) {
		apic_cpus[i].aci_busy = 0;
	}

	for (i = apic_min_device_irq; i < apic_max_device_irq; i++) {
		if ((irq_ptr = apic_irq_table[i]) != NULL)
			irq_ptr->airq_busy = 0;
	}
	apic_skipped_redistribute = 0;
}
