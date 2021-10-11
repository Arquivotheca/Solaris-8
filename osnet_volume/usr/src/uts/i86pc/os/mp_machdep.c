/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_machdep.c	1.37	99/10/22 SMI"

#define	PSMI_1_2
#include <sys/smp_impldefs.h>
#include <sys/psm.h>
#include <sys/psm_modctl.h>
#include <sys/pit.h>
#include <sys/cmn_err.h>
#include <sys/strlog.h>
#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/rtc.h>
#include <sys/x86_archext.h>

/* pointer to array of frame pointers for other processors */
/* temporary till debugger gives us commands to do so */
int	i_fparray_ptr;

/*
 *	Local function prototypes
 */
static int mp_disable_intr(processorid_t cpun);
static void mp_enable_intr(processorid_t cpun);
static void mach_init();
static void mach_picinit();
static void mach_clkinit();
static void mach_smpinit(void);
static void mach_set_softintr(int ipl);
static void mach_cpu_start(int cpun);
static int mach_softlvl_to_vect(int ipl);
static void mach_get_platform(int owner);
static void mach_construct_info();
static int mach_translate_irq(dev_info_t *dip, int irqno);
static timestruc_t mach_tod_get(void);
static void mach_tod_set(timestruc_t ts);
static void mach_notify_error(int level, char *errmsg);
static hrtime_t dummy_hrtime(void);
/*
 *	External reference functions
 */
extern void return_instr();
extern timestruc_t (*todgetf)(void);
extern void (*todsetf)(timestruc_t);
extern long gmt_lag;

/*
 *	PSM functions initialization
 */
void (*psm_shutdownf)(int, int)	= return_instr;
void (*psm_notifyf)(int)	= return_instr;
void (*psm_set_idle_cpuf)(int)	= return_instr;
void (*psm_unset_idle_cpuf)(int) = return_instr;
void (*psminitf)()		= mach_init;
void (*picinitf)() 		= return_instr;
void (*clkinitf)() 		= return_instr;
void (*cpu_startf)() 		= return_instr;
int (*ap_mlsetup)() 		= (int (*)(void))return_instr;
void (*send_dirintf)() 		= return_instr;
void (*setspl)(int)		= return_instr;
int (*addspl)(int, int, int, int) = (int (*)(int, int, int, int))return_instr;
int (*delspl)(int, int, int, int) = (int (*)(int, int, int, int))return_instr;
void (*setsoftint)(int)		= (void (*)(int))return_instr;
int (*slvltovect)(int)		= (int (*)(int))return_instr;
int (*setlvl)(int, int *)	= (int (*)(int, int *))return_instr;
void (*setlvlx)(int, int)	= (void (*)(int, int))return_instr;
int (*psm_disable_intr)(int)	= mp_disable_intr;
void (*psm_enable_intr)(int)	= mp_enable_intr;
hrtime_t (*gethrtimef)(void)	= dummy_hrtime;
int (*psm_translate_irq)(dev_info_t *, int) = mach_translate_irq;
int (*psm_todgetf)(todinfo_t *) = (int (*)(todinfo_t *))return_instr;
int (*psm_todsetf)(todinfo_t *) = (int (*)(todinfo_t *))return_instr;
void (*psm_notify_error)(int, char *) = (void (*)(int, char *))NULL;
int (*psm_get_clockirq)(int) = NULL;
int (*psm_get_ipivect)(int, int) = NULL;

void (*notify_error)(int, char *) = (void (*)(int, char *))return_instr;
void (*hrtime_tick)(void)	= return_instr;

int tsc_gethrtime_enable = 1;

/*
 * Local Static Data
 */
static struct psm_ops mach_ops;
static struct psm_ops *mach_set[4] = {&mach_ops, NULL, NULL, NULL};
static ushort_t mach_ver[4] = {0, 0, 0, 0};

/*
 * Routine to ensure initial callers to hrtime gets 0 as return
 */
static hrtime_t
dummy_hrtime(void)
{
	return (0);
}

static int
mp_disable_intr(int cpun)
{
	/*
	 * switch to the offline cpu
	 */
	affinity_set(cpun);
	/*
	 *raise ipl to just below cross call
	 */
	splx(XC_MED_PIL-1);
	/*
	 *	set base spl to prevent the next swtch to idle from
	 *	lowering back to ipl 0
	 */
	CPU->cpu_intr_actv |= (1 << (XC_MED_PIL-1));
	set_base_spl();
	affinity_clear();
	return (DDI_SUCCESS);
}

static void
mp_enable_intr(int cpun)
{
	/*
	 * switch to the online cpu
	 */
	affinity_set(cpun);
	/*
	 * clear the interrupt active mask
	 */
	CPU->cpu_intr_actv &= ~(1 << (XC_MED_PIL-1));
	set_base_spl();
	(void) spl0();
	affinity_clear();
}

static void
mach_get_platform(int owner)
{
	long *srv_opsp;
	long *clt_opsp;
	int	i;
	int	total_ops;

	/* fix up psm ops						*/
	srv_opsp = (long *)mach_set[0];
	clt_opsp = (long *)mach_set[owner];
	if (mach_ver[owner] == (ushort_t)PSM_INFO_VER01)
		total_ops = sizeof (struct psm_ops_ver01) /
				sizeof (void (*)(void));
	else if (mach_ver[owner] == (ushort_t)PSM_INFO_VER01_1)
		total_ops = sizeof (struct psm_ops) / sizeof (void (*)(void)) -
		    1;	/* no psm_notify_func */
	else
		total_ops = sizeof (struct psm_ops) / sizeof (void (*)(void));

	for (i = 0; i < total_ops; i++) {
		if (*clt_opsp != (long)NULL)
			*srv_opsp = *clt_opsp;
		srv_opsp++;
		clt_opsp++;
	}
}

static void
mach_construct_info()
{
	register struct psm_sw *swp;
	int	mach_cnt[PSM_OWN_OVERRIDE+1] = {0};
	int	conflict_owner = 0;

	mutex_enter(&psmsw_lock);
	for (swp = psmsw->psw_forw; swp != psmsw; swp = swp->psw_forw) {
		if (!(swp->psw_flag & PSM_MOD_IDENTIFY))
			continue;
		mach_set[swp->psw_infop->p_owner] = swp->psw_infop->p_ops;
		mach_ver[swp->psw_infop->p_owner] = swp->psw_infop->p_version;
		mach_cnt[swp->psw_infop->p_owner]++;
	}
	mutex_exit(&psmsw_lock);

	mach_get_platform(PSM_OWN_SYS_DEFAULT);

	/* check to see are there any conflicts */
	if (mach_cnt[PSM_OWN_EXCLUSIVE] > 1)
		conflict_owner = PSM_OWN_EXCLUSIVE;
	if (mach_cnt[PSM_OWN_OVERRIDE] > 1)
		conflict_owner = PSM_OWN_OVERRIDE;
	if (conflict_owner) {
		/* remove all psm modules except uppc */
		cmn_err(CE_WARN,
			"Conflicts detected on the following PSM modules:");
		mutex_enter(&psmsw_lock);
		for (swp = psmsw->psw_forw; swp != psmsw; swp = swp->psw_forw) {
			if (swp->psw_infop->p_owner == conflict_owner)
				cmn_err(CE_WARN, "%s ",
					swp->psw_infop->p_mach_idstring);
		}
		mutex_exit(&psmsw_lock);
		cmn_err(CE_WARN,
			"Setting the system back to SINGLE processor mode!");
		cmn_err(CE_WARN,
		    "Please edit /etc/mach to remove the invalid PSM module.");
		return;
	}

	if (mach_set[PSM_OWN_EXCLUSIVE])
		mach_get_platform(PSM_OWN_EXCLUSIVE);

	if (mach_set[PSM_OWN_OVERRIDE])
		mach_get_platform(PSM_OWN_OVERRIDE);

}

static void
mach_init()
{
	register struct psm_ops  *pops;

	mach_construct_info();

	pops = mach_set[0];

	/* register the interrupt and clock initialization rotuines	*/
	picinitf = mach_picinit;
	clkinitf = mach_clkinit;
	psm_get_clockirq = pops->psm_get_clockirq;

	/* register the interrupt setup code				*/
	slvltovect = mach_softlvl_to_vect;
	addspl	= pops->psm_addspl;
	delspl	= pops->psm_delspl;

	if (pops->psm_translate_irq)
		psm_translate_irq = pops->psm_translate_irq;
	if (pops->psm_tod_get) {
		todgetf = mach_tod_get;
		psm_todgetf = pops->psm_tod_get;
	}
	if (pops->psm_tod_set) {
		todsetf = mach_tod_set;
		psm_todsetf = pops->psm_tod_set;
	}
	if (pops->psm_notify_error) {
		psm_notify_error = mach_notify_error;
		notify_error = pops->psm_notify_error;
	}

	(*pops->psm_softinit)();

	mach_smpinit();
}

static void
mach_smpinit(void)
{
	register struct psm_ops  *pops;
	register processorid_t cpu_id;
	int	 cnt;
	int	 cpumask;

	pops = mach_set[0];

	cpu_id = -1;
	cpu_id = (*pops->psm_get_next_processorid)(cpu_id);
	for (cnt = 0, cpumask = 0; cpu_id != -1; cnt++) {
		cpumask |= 1 << cpu_id;
		cpu_id = (*pops->psm_get_next_processorid)(cpu_id);
	}

	mp_cpus = cpumask;

	/* MP related routines						*/
	cpu_startf = mach_cpu_start;
	ap_mlsetup = pops->psm_post_cpu_start;
	send_dirintf = pops->psm_send_ipi;

	/* optional MP related routines					*/
	if (pops->psm_shutdown)
		psm_shutdownf = pops->psm_shutdown;
	if (pops->psm_notify_func)
		psm_notifyf = pops->psm_notify_func;
	if (pops->psm_set_idlecpu)
		psm_set_idle_cpuf = pops->psm_set_idlecpu;
	if (pops->psm_unset_idlecpu)
		psm_unset_idle_cpuf = pops->psm_unset_idlecpu;

	/* check for multiple cpu's					*/
	if (cnt < 2)
		return;

	/* check for MP platforms					*/
	if (pops->psm_cpu_start == NULL)
		return;

	if (pops->psm_disable_intr)
		psm_disable_intr = pops->psm_disable_intr;
	if (pops->psm_enable_intr)
		psm_enable_intr  = pops->psm_enable_intr;

	psm_get_ipivect = pops->psm_get_ipivect;

	(void) add_avintr((void *)NULL, XC_HI_PIL, xc_serv, "xc_hi_intr",
		(*pops->psm_get_ipivect)(XC_HI_PIL, PSM_INTR_IPI_HI),
		(caddr_t)X_CALL_HIPRI);
	(void) add_avintr((void *)NULL, XC_MED_PIL, xc_serv, "xc_med_intr",
		(*pops->psm_get_ipivect)(XC_MED_PIL, PSM_INTR_IPI_LO),
		(caddr_t)X_CALL_MEDPRI);

	(void) (*pops->psm_get_ipivect)(XC_CPUPOKE_PIL, PSM_INTR_POKE);
}

static void
mach_picinit()
{
	register struct psm_ops  *pops;
	extern void install_spl(void);	/* XXX: belongs in a header file */

	pops = mach_set[0];

	/* register the interrupt handlers				*/
	setlvl = pops->psm_intr_enter;
	setlvlx = pops->psm_intr_exit;

	/* initialize the interrupt hardware				*/
	(*pops->psm_picinit)();

	/* set interrupt mask for current ipl 				*/
	setspl = pops->psm_setspl;
	setspl(CPU->cpu_pri);

	/* Install proper spl routine now that we can Program the PIC   */
	install_spl();
}

#define	DIFF_LE_TWO(a, b) (((a) > (b) ? ((a) - (b)) : ((b) - (a))) <= 2)
#define	DIFF_LE_TEN(a, b) (((a) > (b) ? ((a) - (b)) : ((b) - (a))) <= 10)
int x86_cpu_freq[] =	{20, 25, 33, 40, 50, 60, 66, 75, 80,
			90, 100, 120, 133, 150, 160, 166, 175,
			180, 200, 233, 266, 300, 333, 350, 400, 450, 500, 600};
int	cpu_freq;

#define	MEGA_HZ		1000000

static void
mach_clkinit()
{
	register struct psm_ops  *pops;
	unsigned long pit_counter, processor_clks, i;
	extern int find_cpufrequency(ulong_t *, ulong_t *);

	pops = mach_set[0];
	clksetup();

	if (find_cpufrequency(&pit_counter, &processor_clks)) {
		long long cpu_hz = ((long long) PIT_HZ *
		    (long long) processor_clks) / (long long) pit_counter;
		ASSERT(cpu_hz < LONG_MAX);
		cpu_freq = (cpu_hz + (MEGA_HZ / 2)) / MEGA_HZ;

		for (i = 0; i < sizeof (x86_cpu_freq) / sizeof (int); i++) {
			if ((DIFF_LE_TWO(cpu_freq, x86_cpu_freq[i])) ||
			    ((cpu_freq > 200) &&
			    (DIFF_LE_TEN(cpu_freq, x86_cpu_freq[i])))) {
				cpu_freq = x86_cpu_freq[i];
				break;
			}
		}
	}
	if (!(x86_feature & X86_TSC))
		tsc_gethrtime_enable = 0;
	if (tsc_gethrtime_enable) {
		extern void tsc_hrtimeinit(uint32_t cpu_freq);
		extern	hrtime_t tsc_gethrtime();
		tsc_hrtimeinit(cpu_freq);
		gethrtimef = tsc_gethrtime;
		hrtime_tick = tsc_tick;
	} else {
		if (pops->psm_hrtimeinit)
			(*pops->psm_hrtimeinit)();
		gethrtimef = pops->psm_gethrtime;
	}
	(*pops->psm_clkinit)(hz);
}

static int
mach_softlvl_to_vect(register int ipl)
{
	register int softvect;
	register struct psm_ops  *pops;

	pops = mach_set[0];

	/* check for null handler for set soft interrupt call		*/
	if (pops->psm_set_softintr == NULL) {
		setsoftint = set_pending;
		return (PSM_SV_SOFTWARE);
	}

	softvect = (*pops->psm_softlvl_to_irq)(ipl);
	/* check for hardware scheme					*/
	if (softvect > PSM_SV_SOFTWARE) {
		setsoftint = pops->psm_set_softintr;
		return (softvect);
	}

	if (softvect == PSM_SV_SOFTWARE)
		setsoftint = set_pending;
	else	/* hardware and software mixed scheme			*/
		setsoftint = mach_set_softintr;

	return (PSM_SV_SOFTWARE);
}

static void
mach_set_softintr(register int ipl)
{
	register struct psm_ops  *pops;

	/* set software pending bits					*/
	set_pending(ipl);

	/*	check if dosoftint will be called at the end of intr	*/
	if ((CPU->cpu_on_intr) || (curthread->t_intr))
		return;

	/* invoke hardware interrupt					*/
	pops = mach_set[0];
	(*pops->psm_set_softintr)(ipl);
}

static void
mach_cpu_start(register int cpun)
{
	register struct psm_ops  *pops;
	int	i;

	pops = mach_set[0];

	(*pops->psm_cpu_start)(cpun, rm_platter_va);

	/* wait for the auxillary cpu to be ready			*/
	for (i = 20000; i; i--) {
		if (cpu[cpun]->cpu_flags & CPU_READY)
			return;
		drv_usecwait(100);
	}
}

static int
/* LINTED: first argument dip is not used */
mach_translate_irq(dev_info_t *dip, register int irqno)
{
	return (irqno);		/* default to NO translation */
}

static timestruc_t
mach_tod_get(void)
{
	timestruc_t ts;
	todinfo_t tod;
	static int mach_range_warn = 1;	/* warn only once */

	ASSERT(MUTEX_HELD(&tod_lock));

	/* The year returned from is the last 2 digit only */
	if ((*psm_todgetf)(&tod)) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	/* assume that we wrap the rtc year back to zero at 2000 */
	if (tod.tod_year < 69) {
		if (mach_range_warn && tod.tod_year > 38) {
			cmn_err(CE_WARN, "hardware real-time clock is out "
				"of range -- time needs to be reset");
			mach_range_warn = 0;
		}
		tod.tod_year += 100;
	}

	/* tod_to_utc uses 1900 as base for the year */
	ts.tv_sec = tod_validate(tod_to_utc(tod) + gmt_lag, (hrtime_t)0);
	ts.tv_nsec = 0;

	return (ts);
}

static void
mach_tod_set(timestruc_t ts)
{
	todinfo_t tod = utc_to_tod(ts.tv_sec - gmt_lag);

	ASSERT(MUTEX_HELD(&tod_lock));

	if (tod.tod_year >= 100)
		tod.tod_year -= 100;

	(*psm_todsetf)(&tod);

	/* prevent false alarm in tod_validate() due to tod value change */
	tod_fault_reset();
}

static void
mach_notify_error(int level, char *errmsg)
{
	/*
	 * SL_FATAL is pass in once panicstr is set, deliver it
	 * as CE_PANIC.  Also, translate SL_ codes back to CE_
	 * codes for the psmi handler
	 */
	if (level & SL_FATAL)
		(*notify_error)(CE_PANIC, errmsg);
	else if (level & SL_WARN)
		(*notify_error)(CE_WARN, errmsg);
	else if (level & SL_NOTE)
		(*notify_error)(CE_NOTE, errmsg);
	else if (level & SL_CONSOLE)
		(*notify_error)(CE_CONT, errmsg);
}
