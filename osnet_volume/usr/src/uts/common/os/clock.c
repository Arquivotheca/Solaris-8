/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)clock.c	1.160	99/11/20 SMI"

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/types.h>
#include <sys/tuneable.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/callo.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/swap.h>
#include <sys/vmsystm.h>
#include <sys/class.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/spl.h>
#include <sys/atomic.h>
#include <sys/dumphdr.h>
#include <sys/archsystm.h>
#include <sys/fs/swapnode.h>

#include <vm/page.h>
#include <vm/anon.h>
#include <vm/rm.h>
#include <sys/cyclic.h>

/*
 * for NTP support
 */
#include <sys/timex.h>
#include <sys/inttypes.h>

/*
 * clock is called straight from
 * the real time clock interrupt.
 *
 * Functions:
 *	reprime clock
 *	schedule callouts
 *	maintain date
 *	jab the scheduler
 */

static kmutex_t	delay_lock;
extern kcondvar_t	fsflush_cv;
extern sysinfo_t	sysinfo;
extern vminfo_t	vminfo;
extern int	idleswtch;	/* flag set while idle in pswtch() */

time_t	time;	/* time in seconds since 1970 - for compatibility only */

/*
 * Phase/frequency-lock loop (PLL/FLL) definitions
 *
 * The following variables are read and set by the ntp_adjtime() system
 * call.
 *
 * time_state shows the state of the system clock, with values defined
 * in the timex.h header file.
 *
 * time_status shows the status of the system clock, with bits defined
 * in the timex.h header file.
 *
 * time_offset is used by the PLL/FLL to adjust the system time in small
 * increments.
 *
 * time_constant determines the bandwidth or "stiffness" of the PLL.
 *
 * time_tolerance determines maximum frequency error or tolerance of the
 * CPU clock oscillator and is a property of the architecture; however,
 * in principle it could change as result of the presence of external
 * discipline signals, for instance.
 *
 * time_precision is usually equal to the kernel tick variable; however,
 * in cases where a precision clock counter or external clock is
 * available, the resolution can be much less than this and depend on
 * whether the external clock is working or not.
 *
 * time_maxerror is initialized by a ntp_adjtime() call and increased by
 * the kernel once each second to reflect the maximum error bound
 * growth.
 *
 * time_esterror is set and read by the ntp_adjtime() call, but
 * otherwise not used by the kernel.
 */
int32_t time_state = TIME_OK;	/* clock state */
int32_t time_status = STA_UNSYNC;	/* clock status bits */
int32_t time_offset = 0;		/* time offset (us) */
int32_t time_constant = 0;		/* pll time constant */
int32_t time_tolerance = MAXFREQ;	/* frequency tolerance (scaled ppm) */
int32_t time_precision = 1;	/* clock precision (us) */
int32_t time_maxerror = MAXPHASE;	/* maximum error (us) */
int32_t time_esterror = MAXPHASE;	/* estimated error (us) */

/*
 * The following variables establish the state of the PLL/FLL and the
 * residual time and frequency offset of the local clock. The scale
 * factors are defined in the timex.h header file.
 *
 * time_phase and time_freq are the phase increment and the frequency
 * increment, respectively, of the kernel time variable.
 *
 * time_freq is set via ntp_adjtime() from a value stored in a file when
 * the synchronization daemon is first started. Its value is retrieved
 * via ntp_adjtime() and written to the file about once per hour by the
 * daemon.
 *
 * time_adj is the adjustment added to the value of tick at each timer
 * interrupt and is recomputed from time_phase and time_freq at each
 * seconds rollover.
 *
 * time_reftime is the second's portion of the system time at the last
 * call to ntp_adjtime(). It is used to adjust the time_freq variable
 * and to increase the time_maxerror as the time since last update
 * increases.
 */
int32_t time_phase = 0;		/* phase offset (scaled us) */
int32_t time_freq = 0;		/* frequency offset (scaled ppm) */
int32_t time_adj = 0;		/* tick adjust (scaled 1 / hz) */
int32_t time_reftime = 0;		/* time at last adjustment (s) */

/*
 * The scale factors of the following variables are defined in the
 * timex.h header file.
 *
 * pps_time contains the time at each calibration interval, as read by
 * microtime(). pps_count counts the seconds of the calibration
 * interval, the duration of which is nominally pps_shift in powers of
 * two.
 *
 * pps_offset is the time offset produced by the time median filter
 * pps_tf[], while pps_jitter is the dispersion (jitter) measured by
 * this filter.
 *
 * pps_freq is the frequency offset produced by the frequency median
 * filter pps_ff[], while pps_stabil is the dispersion (wander) measured
 * by this filter.
 *
 * pps_usec is latched from a high resolution counter or external clock
 * at pps_time. Here we want the hardware counter contents only, not the
 * contents plus the time_tv.usec as usual.
 *
 * pps_valid counts the number of seconds since the last PPS update. It
 * is used as a watchdog timer to disable the PPS discipline should the
 * PPS signal be lost.
 *
 * pps_glitch counts the number of seconds since the beginning of an
 * offset burst more than tick/2 from current nominal offset. It is used
 * mainly to suppress error bursts due to priority conflicts between the
 * PPS interrupt and timer interrupt.
 *
 * pps_intcnt counts the calibration intervals for use in the interval-
 * adaptation algorithm. It's just too complicated for words.
 */
struct timeval pps_time;	/* kernel time at last interval */
int32_t pps_tf[] = {0, 0, 0};	/* pps time offset median filter (us) */
int32_t pps_offset = 0;		/* pps time offset (us) */
int32_t pps_jitter = MAXTIME;	/* time dispersion (jitter) (us) */
int32_t pps_ff[] = {0, 0, 0};	/* pps frequency offset median filter */
int32_t pps_freq = 0;		/* frequency offset (scaled ppm) */
int32_t pps_stabil = MAXFREQ;	/* frequency dispersion (scaled ppm) */
int32_t pps_usec = 0;		/* microsec counter at last interval */
int32_t pps_valid = PPS_VALID;	/* pps signal watchdog counter */
int32_t pps_glitch = 0;		/* pps signal glitch counter */
int32_t pps_count = 0;		/* calibration interval counter (s) */
int32_t pps_shift = PPS_SHIFT;	/* interval duration (s) (shift) */
int32_t pps_intcnt = 0;		/* intervals at current duration */

/*
 * PPS signal quality monitors
 *
 * pps_jitcnt counts the seconds that have been discarded because the
 * jitter measured by the time median filter exceeds the limit MAXTIME
 * (100 us).
 *
 * pps_calcnt counts the frequency calibration intervals, which are
 * variable from 4 s to 256 s.
 *
 * pps_errcnt counts the calibration intervals which have been discarded
 * because the wander exceeds the limit MAXFREQ (100 ppm) or where the
 * calibration interval jitter exceeds two ticks.
 *
 * pps_stbcnt counts the calibration intervals that have been discarded
 * because the frequency wander exceeds the limit MAXFREQ / 4 (25 us).
 */
int32_t pps_jitcnt = 0;		/* jitter limit exceeded */
int32_t pps_calcnt = 0;		/* calibration intervals */
int32_t pps_errcnt = 0;		/* calibration errors */
int32_t pps_stbcnt = 0;		/* stability limit exceeded */

/* The following variables require no explicit locking */
volatile clock_t lbolt;		/* time in Hz since last boot */
volatile int64_t lbolt64;	/* lbolt64 won't wrap for 2.9 billion yrs */

kcondvar_t lbolt_cv;
int one_sec = 1; /* turned on once every second */
static int fsflushcnt;	/* counter for t_fsflushr */
int	dosynctodr = 1;	/* patchable; enable/disable sync to TOD chip */
int	tod_needsync = 0;	/* need to sync tod chip with software time */
static int tod_broken = 0;	/* clock chip doesn't work */
time_t	boot_time = 0;		/* Boot time in seconds since 1970 */
cyclic_id_t clock_cyclic;	/* clock()'s cyclic_id */
cyclic_id_t deadman_cyclic;	/* deadman()'s cyclic_id */

/*
 * for tod fault detection
 */
#define	TOD_REF_FREQ		((longlong_t)(NANOSEC))
#define	TOD_STALL_THRESHOLD	(TOD_REF_FREQ * 3 / 2)
#define	TOD_JUMP_THRESHOLD	(TOD_REF_FREQ / 2)
#define	TOD_FILTER_N		4
#define	TOD_FILTER_SETTLE	(4 * TOD_FILTER_N)
static int tod_faulted = TOD_NOFAULT;
static u_int tod_get_loopnum = 0;
static int tod_fault_reset_flag = 0;

/* patchable via /etc/system */
int tod_validate_enable = 0;

/*
 * tod_fault_table[] must be aligned with
 * enum tod_fault_type in systm.h
 */
static char *tod_fault_table[] = {
	{ "Reversed" },			/* TOD_REVERSED */
	{ "Stalled" },			/* TOD_STALLED */
	{ "Jumped" },			/* TOD_JUMPED */
	{ "Changed in Clock Rate" }	/* TOD_RATECHANGED */
	/*
	 * no strings needed for TOD_NOFAULT
	 */
};

/*
 * test hook for tod broken detection in tod_validate
 */
int tod_unit_test = 0;
time_t tod_test_injector;

#define	CLOCK_ADJ_HIST_SIZE	4

static int	adj_hist_entry;

int64_t clock_adj_hist[CLOCK_ADJ_HIST_SIZE];

static void clock_tick(kthread_id_t);

void (*cmm_clock_callout)() = NULL;

#ifdef	KSLICE
int kslice = KSLICE;
#endif

extern int	sync_making_progress(int);

static void
clock(void)
{
	kthread_id_t	t;
	kmutex_t	*plockp;	/* pointer to thread's process lock */
	int	pinned_intr = 0;
	uint_t	nrunnable, nrunning;
	uint_t	w_io;
	cpu_t	*cp;
	cpu_sysinfo_t *sysinfop;
	int	exiting;
	extern void set_anoninfo();
	extern	void	set_freemem();
	void	(*funcp)();
	int32_t ltemp;
	int64_t lltemp;
	int s;

	if (panicstr)
		return;

	set_anoninfo();
	/*
	 * Make sure that 'freemem' do not drift too far from the truth
	 */
	set_freemem();


	/*
	 * Before the section which is repeated is executed, we do
	 * the time delta processing which occurs every clock tick
	 *
	 * There is additional processing which happens every time
	 * the nanosecond counter rolls over which is described
	 * below - see the section which begins with : if (one_sec)
	 *
	 * This section marks the beginning of the precision-kernel
	 * code fragment.
	 *
	 * First, compute the phase adjustment. If the low-order bits
	 * (time_phase) of the update overflow, bump the higher order
	 * bits (time_update).
	 */
	time_phase += time_adj;
	if (time_phase <= -FINEUSEC) {
		ltemp = -time_phase / SCALE_PHASE;
		time_phase += ltemp * SCALE_PHASE;
		s = hr_clock_lock();
		timedelta -= ltemp * (NANOSEC/MICROSEC);
		hr_clock_unlock(s);
	} else if (time_phase >= FINEUSEC) {
		ltemp = time_phase / SCALE_PHASE;
		time_phase -= ltemp * SCALE_PHASE;
		s = hr_clock_lock();
		timedelta += ltemp * (NANOSEC/MICROSEC);
		hr_clock_unlock(s);
	}

	/*
	 * End of precision-kernel code fragment which is processed
	 * every timer interrupt.
	 *
	 * Continue with the interrupt processing as scheduled.
	 *
	 * Did we pin another interrupt thread?
	 */
	if (curthread->t_intr != NULL)
		pinned_intr = (curthread->t_intr->t_flag & T_INTR_THREAD);

	/*
	 * Count the number of threads waiting for some form
	 * of I/O to complete -- gets added to sysinfo.waiting
	 * To know the state of the system, must add wait counts from
	 * all CPUs.
	 */
	w_io = 0;
	nrunnable = 0;
	cp = cpu_list;
	do {
		w_io += cp->cpu_stat.cpu_syswait.iowait;
		nrunnable += cp->cpu_disp.disp_nrunnable;
	} while ((cp = cp->cpu_next) != cpu_list);

	/*
	 * Do tick processing for all the active threads running in
	 * the system.
	 * pidlock (above) prevents lwp's from disappearing (exiting).
	 * Could defer getting pidlock until now, but that might cause us
	 * to block for awhile, invalidating all the wait states.
	 */
	cp = cpu_list;
	nrunning = 0;
	do {
		klwp_id_t lwp;

		/*
		 * Don't do any tick processing on CPUs that
		 * aren't even in the system or aren't up yet.
		 */
		if ((cp->cpu_flags & CPU_EXISTS) == 0) {
			continue;
		}

		/*
		 * The locking here is rather tricky.  We use
		 * thread_free_lock to keep the currently running
		 * thread from being freed or recycled while we're
		 * looking at it.  We can then check if the thread
		 * is exiting and get the appropriate p_lock if it
		 * is not.  We have to be careful, though, because
		 * the _process_ can still be freed while we're
		 * holding thread_free_lock.  To avoid touching the
		 * proc structure we put a pointer to the p_lock in the
		 * thread structure.  The p_lock is persistent so we
		 * can acquire it even if the process is gone.  At that
		 * point we can check (again) if the thread is exiting
		 * and either drop the lock or do the tick processing.
		 */
		mutex_enter(&thread_free_lock);
		/*
		 * We cannot hold the cpu_lock to prevent the
		 * cpu_list from changing in the clock interrupt.
		 * As long as we don't block (or don't get pre-empted)
		 * the cpu_list will not change (all threads are paused
		 * before list modification). If the list does change
		 * any deleted cpu structures will remain with cpu_next
		 * set to NULL, hence the following test.
		 */
		if (cp->cpu_next == NULL) {
			mutex_exit(&thread_free_lock);
			break;
		}
		t = cp->cpu_thread;	/* Current running thread */
		if (CPU == cp) {
			/*
			 * 't' will be the clock interrupt thread on this
			 * CPU.
			 * Use the base lwp (if any) on this CPU
			 * (if it was running when the clock interrupt
			 * happened) as the target of the clock tick.
			 */
			lwp = cp->cpu_lwp;	/* Base lwp (if any) */
			if (lwp && !pinned_intr)
				t = lwptot(lwp);
		} else {
			lwp = ttolwp(t);
		}

		if (lwp == NULL || (t->t_proc_flag & TP_LWPEXIT)) {
			/*
			 * Thread is exiting (or uninteresting) so don't
			 * do tick processing or grab p_lock.
			 */
			exiting = 1;
		} else {
			/*
			 * OK, try to grab the process lock.  See
			 * comments above for why we're not using
			 * ttoproc(t)->p_lockp here.
			 */
			plockp = t->t_plockp;
			mutex_enter(plockp);
			/* See above comment. */
			if (cp->cpu_next == NULL) {
				mutex_exit(plockp);
				mutex_exit(&thread_free_lock);
				break;
			}
			/*
			 * The thread may have exited between when we
			 * checked above, and when we got the p_lock.
			 * Also, t_lwp may have changed if this is an
			 * interrupt thread.
			 */
			lwp = ttolwp(t);
			if (lwp == NULL || t->t_proc_flag & TP_LWPEXIT) {
				mutex_exit(plockp);
				exiting = 1;
			} else {
				exiting = 0;
			}
		}
		/*
		 * Either we have the p_lock for the thread's process,
		 * or we don't care about the thread structure any more.
		 * Either way we can drop thread_free_lock.
		 */
		mutex_exit(&thread_free_lock);

		/*
		 * Update user, system, and idle cpu times.
		 */
		sysinfop = &cp->cpu_stat.cpu_sysinfo;

		if (cp->cpu_flags & CPU_QUIESCED) {
			sysinfop->cpu[CPU_IDLE]++;
		} else if (cp->cpu_on_intr ||
		    (!exiting && t->t_intr != NULL &&
		    cp->cpu_thread != curthread)) {
			nrunning++;
			sysinfop->cpu[CPU_KERNEL]++;
		} else if (t == curthread && pinned_intr) {
			nrunning++;
			sysinfop->cpu[CPU_KERNEL]++;
		} else if (cp->cpu_dispthread == cp->cpu_idle_thread) {
			/*
			 * Add to the wait times for the CPU.
			 */
			if (cp->cpu_stat.cpu_syswait.iowait) {
				sysinfop->cpu[CPU_WAIT]++;
				sysinfop->wait[W_IO]++;
			} else {
				sysinfop->cpu[CPU_IDLE]++;
			}
		} else if (exiting) {
			nrunning++;
			sysinfop->cpu[CPU_KERNEL]++;
		} else {
			nrunning++;
			if (lwp->lwp_state == LWP_USER)
				sysinfop->cpu[CPU_USER]++;
			else
				sysinfop->cpu[CPU_KERNEL]++;
			/*
			 * If the current thread running on the CPU is not
			 * an interrupt thread then do tick processing for
			 * it.  We already know it's not exiting.
			 */
			if (!(t->t_flag & T_INTR_THREAD)) {
				clock_t ticks;

				/*
				 * If we haven't done tick processing for this
				 * lwp, then do it now. Since we don't hold the
				 * lwp down on a CPU it can migrate and show up
				 * more than once, hence the lbolt check.
				 * XXX what if LWP is swapped out?
				 */
				if ((ticks = lbolt - t->t_lbolt) != 0) {
					uint_t pct = t->t_pctcpu;

					if (--ticks != 0)
						pct = cpu_decay(pct, ticks);
					t->t_pctcpu = cpu_grow(pct, 1);
					t->t_lbolt = lbolt;
					clock_tick(t);
				}
			}
		}

#ifdef KSLICE
		/*
		 * Ah what the heck, give this kid a taste of the real
		 * world and yank the rug out from under it.
		 * But, only if we are running UniProcessor.
		 */
		if ((kslice) && (ncpus == 1)) {
			aston(t);
			cp->cpu_runrun = 1;
			cp->cpu_kprunrun = 1;
		}
#endif
		if (!exiting)
			mutex_exit(plockp);
	} while ((cp = cp->cpu_next) != cpu_list);

	/*
	 * bump time in ticks
	 *
	 * We rely on there being only one clock thread and hence
	 * don't need a lock to protect lbolt.
	 */
	lbolt++;
	lbolt64++;

	/*
	 * Check for a callout that needs be called from the clock
	 * thread to support the membership protocol in a clustered
	 * system.  Copy the function pointer so that we can reset
	 * this to NULL if needed.
	 */
	if ((funcp = cmm_clock_callout) != NULL)
		(*funcp)();

	/*
	 * Schedule timeout() requests if any are due at this time.
	 */
	callout_schedule();

	if (one_sec) {

		int drift, absdrift;
		timestruc_t tod;
		int s;

		/*
		 * Beginning of precision-kernel code fragment executed
		 * every second.
		 *
		 * On rollover of the second the phase adjustment to be
		 * used for the next second is calculated.  Also, the
		 * maximum error is increased by the tolerance.  If the
		 * PPS frequency discipline code is present, the phase is
		 * increased to compensate for the CPU clock oscillator
		 * frequency error.
		 *
		 * On a 32-bit machine and given parameters in the timex.h
		 * header file, the maximum phase adjustment is +-512 ms
		 * and maximum frequency offset is (a tad less than)
		 * +-512 ppm. On a 64-bit machine, you shouldn't need to ask.
		 */
		time_maxerror += time_tolerance / SCALE_USEC;

		/*
		 * Leap second processing. If in leap-insert state at
		 * the end of the day, the system clock is set back one
		 * second; if in leap-delete state, the system clock is
		 * set ahead one second. The microtime() routine or
		 * external clock driver will insure that reported time
		 * is always monotonic. The ugly divides should be
		 * replaced.
		 */
		switch (time_state) {

		case TIME_OK:
			if (time_status & STA_INS)
				time_state = TIME_INS;
			else if (time_status & STA_DEL)
				time_state = TIME_DEL;
			break;

		case TIME_INS:
			if (hrestime.tv_sec % 86400 == 0) {
				s = hr_clock_lock();
				hrestime.tv_sec--;
				hr_clock_unlock(s);
				time_state = TIME_OOP;
			}
			break;

		case TIME_DEL:
			if ((hrestime.tv_sec + 1) % 86400 == 0) {
				s = hr_clock_lock();
				hrestime.tv_sec++;
				hr_clock_unlock(s);
				time_state = TIME_WAIT;
			}
			break;

		case TIME_OOP:
			time_state = TIME_WAIT;
			break;

		case TIME_WAIT:
			if (!(time_status & (STA_INS | STA_DEL)))
				time_state = TIME_OK;
		default:
			break;
		}

		/*
		 * Compute the phase adjustment for the next second. In
		 * PLL mode, the offset is reduced by a fixed factor
		 * times the time constant. In FLL mode the offset is
		 * used directly. In either mode, the maximum phase
		 * adjustment for each second is clamped so as to spread
		 * the adjustment over not more than the number of
		 * seconds between updates.
		 */
		if (time_offset == 0)
			time_adj = 0;
		else if (time_offset < 0) {
			lltemp = -time_offset;
			if (!(time_status & STA_FLL)) {
				if ((1 << time_constant) >= SCALE_KG)
					lltemp *= (1 << time_constant) /
					    SCALE_KG;
				else
					lltemp = (lltemp / SCALE_KG) >>
					    time_constant;
			}
			if (lltemp > (MAXPHASE / MINSEC) * SCALE_UPDATE)
				lltemp = (MAXPHASE / MINSEC) * SCALE_UPDATE;
			time_offset += lltemp;
			time_adj = -(lltemp * SCALE_PHASE) / hz / SCALE_UPDATE;
		} else {
			lltemp = time_offset;
			if (!(time_status & STA_FLL)) {
				if ((1 << time_constant) >= SCALE_KG)
					lltemp *= (1 << time_constant) /
					    SCALE_KG;
				else
					lltemp = (lltemp / SCALE_KG) >>
					    time_constant;
			}
			if (lltemp > (MAXPHASE / MINSEC) * SCALE_UPDATE)
				lltemp = (MAXPHASE / MINSEC) * SCALE_UPDATE;
			time_offset -= lltemp;
			time_adj = (lltemp * SCALE_PHASE) / hz / SCALE_UPDATE;
		}

		/*
		 * Compute the frequency estimate and additional phase
		 * adjustment due to frequency error for the next
		 * second. When the PPS signal is engaged, gnaw on the
		 * watchdog counter and update the frequency computed by
		 * the pll and the PPS signal.
		 */
		pps_valid++;
		if (pps_valid == PPS_VALID) {
			pps_jitter = MAXTIME;
			pps_stabil = MAXFREQ;
			time_status &= ~(STA_PPSSIGNAL | STA_PPSJITTER |
			    STA_PPSWANDER | STA_PPSERROR);
		}
		lltemp = time_freq + pps_freq;

		if (lltemp)
			time_adj += (lltemp * SCALE_PHASE) / (SCALE_USEC * hz);

		/*
		 * End of precision kernel-code fragment
		 *
		 * The section below should be modified if we are planning
		 * to use NTP for synchronization.
		 *
		 * Note: the clock synchronization code now assumes
		 * the following:
		 *   - if dosynctodr is 1, then compute the drift between
		 *	the tod chip and software time and adjust one or
		 *	the other depending on the circumstances
		 *
		 *   - if dosynctodr is 0, then the tod chip is independent
		 *	of the software clock and should not be adjusted,
		 *	but allowed to free run.  this allows NTP to sync.
		 *	hrestime without any interference from the tod chip.
		 */

		mutex_enter(&tod_lock);
		tod = tod_get();
		drift = tod.tv_sec - hrestime.tv_sec;
		absdrift = (drift > 0) ? drift : -drift;
		if (tod_needsync || absdrift > 1) {
			int s;
			if (absdrift > 2) {
				if (!tod_broken && tod_faulted == TOD_NOFAULT) {
					s = hr_clock_lock();
					hrestime = tod;
					timedelta = 0;
					tod_needsync = 0;
					hr_clock_unlock(s);
				}
			} else {
				if (tod_needsync || !dosynctodr) {
					gethrestime(&tod);
					tod_set(tod);
					s = hr_clock_lock();
					if (timedelta == 0)
						tod_needsync = 0;
					hr_clock_unlock(s);
				} else {
					/*
					 * If the drift is 2 seconds on the
					 * money, then the TOD is adjusting
					 * the clock;  record that.
					 */
					clock_adj_hist[adj_hist_entry++ %
					    CLOCK_ADJ_HIST_SIZE] = lbolt64;
					s = hr_clock_lock();
					timedelta = (int64_t)drift*NANOSEC;
					hr_clock_unlock(s);
				}
			}
		}
		one_sec = 0;
		time = hrestime.tv_sec;	/* for crusty old kmem readers */
		mutex_exit(&tod_lock);

		/*
		 * Some drivers still depend on this... XXX
		 */
		cv_broadcast(&lbolt_cv);

		sysinfo.updates++;
		vminfo.freemem += freemem;
		{
			pgcnt_t maxswap, resv, free;
			pgcnt_t avail =
			    MAX((spgcnt_t)(availrmem - swapfs_minfree), 0);

			maxswap = k_anoninfo.ani_mem_resv
					+ k_anoninfo.ani_max +avail;
			free = k_anoninfo.ani_free + avail;
			resv = k_anoninfo.ani_phys_resv +
					k_anoninfo.ani_mem_resv;

			vminfo.swap_resv += resv;
			/* number of reserved and allocated pages */
#ifdef	DEBUG
			if (maxswap < free)
				cmn_err(CE_WARN, "clock: maxswap < free");
			if (maxswap < resv)
				cmn_err(CE_WARN, "clock: maxswap < resv");
#endif
			vminfo.swap_alloc += maxswap - free;
			vminfo.swap_avail += maxswap - resv;
			vminfo.swap_free += free;
		}
		if (nrunnable) {
			sysinfo.runque += nrunnable;
			sysinfo.runocc++;
		}
		if (nswapped) {
			sysinfo.swpque += nswapped;
			sysinfo.swpocc++;
		}
		sysinfo.waiting += w_io;

		/*
		 * Wake up fsflush to write out DELWRI
		 * buffers, dirty pages and other cached
		 * administrative data, e.g. inodes.
		 */
		if (--fsflushcnt <= 0) {
			fsflushcnt = tune.t_fsflushr;
			cv_signal(&fsflush_cv);
		}

		vmmeter(nrunnable + nrunning);

		/*
		 * Wake up the swapper thread if necessary.
		 */
		if (runin ||
		    (runout && (avefree < desfree || wake_sched_sec))) {
			t = &t0;
			thread_lock(t);
			if (t->t_state == TS_STOPPED) {
				runin = runout = 0;
				wake_sched_sec = 0;
				t->t_whystop = 0;
				t->t_whatstop = 0;
				t->t_schedflag &= ~TS_ALLSTART;
				THREAD_TRANSITION(t);
				setfrontdq(t);
			}
			thread_unlock(t);
		}
	}

	/*
	 * Wake up the swapper if any high priority swapped-out threads
	 * became runable during the last tick.
	 */
	if (wake_sched) {
		t = &t0;
		thread_lock(t);
		if (t->t_state == TS_STOPPED) {
			runin = runout = 0;
			wake_sched = 0;
			t->t_whystop = 0;
			t->t_whatstop = 0;
			t->t_schedflag &= ~TS_ALLSTART;
			THREAD_TRANSITION(t);
			setfrontdq(t);
		}
		thread_unlock(t);
	}
}

void
clock_init(void)
{
	cyc_handler_t hdlr;
	cyc_time_t when;

	hdlr.cyh_func = (cyc_func_t)clock;
	hdlr.cyh_level = CY_LOCK_LEVEL;
	hdlr.cyh_arg = NULL;

	when.cyt_when = 0;
	when.cyt_interval = nsec_per_tick;

	mutex_enter(&cpu_lock);
	clock_cyclic = cyclic_add(&hdlr, &when);
	mutex_exit(&cpu_lock);
}

/*
 * clock_update() - local clock update
 *
 * This routine is called by ntp_adjtime() to update the local clock
 * phase and frequency. The implementation is of an
 * adaptive-parameter, hybrid phase/frequency-lock loop (PLL/FLL). The
 * routine computes new time and frequency offset estimates for each
 * call.  The PPS signal itself determines the new time offset,
 * instead of the calling argument.  Presumably, calls to
 * ntp_adjtime() occur only when the caller believes the local clock
 * is valid within some bound (+-128 ms with NTP). If the caller's
 * time is far different than the PPS time, an argument will ensue,
 * and it's not clear who will lose.
 *
 * For uncompensated quartz crystal oscillatores and nominal update
 * intervals less than 1024 s, operation should be in phase-lock mode
 * (STA_FLL = 0), where the loop is disciplined to phase. For update
 * intervals greater than this, operation should be in frequency-lock
 * mode (STA_FLL = 1), where the loop is disciplined to frequency.
 *
 * Note: mutex(&tod_lock) is in effect.
 */
void
clock_update(int offset)
{
	int ltemp, mtemp, s;

	ASSERT(MUTEX_HELD(&tod_lock));

	if (!(time_status & STA_PLL) && !(time_status & STA_PPSTIME))
		return;
	ltemp = offset;
	if ((time_status & STA_PPSTIME) && (time_status & STA_PPSSIGNAL))
		ltemp = pps_offset;

	/*
	 * Scale the phase adjustment and clamp to the operating range.
	 */
	if (ltemp > MAXPHASE)
		time_offset = MAXPHASE * SCALE_UPDATE;
	else if (ltemp < -MAXPHASE)
		time_offset = -(MAXPHASE * SCALE_UPDATE);
	else
		time_offset = ltemp * SCALE_UPDATE;

	/*
	 * Select whether the frequency is to be controlled and in which
	 * mode (PLL or FLL). Clamp to the operating range. Ugly
	 * multiply/divide should be replaced someday.
	 */
	if (time_status & STA_FREQHOLD || time_reftime == 0)
		time_reftime = hrestime.tv_sec;

	mtemp = hrestime.tv_sec - time_reftime;
	time_reftime = hrestime.tv_sec;

	if (time_status & STA_FLL) {
		if (mtemp >= MINSEC) {
			ltemp = ((time_offset / mtemp) * (SCALE_USEC /
			    SCALE_UPDATE));
			if (ltemp)
				time_freq += ltemp / SCALE_KH;
		}
	} else {
		if (mtemp < MAXSEC) {
			ltemp *= mtemp;
			if (ltemp)
				time_freq += (int)(((int64_t)ltemp *
				    SCALE_USEC) / SCALE_KF)
				    / (1 << (time_constant * 2));
		}
	}
	if (time_freq > time_tolerance)
		time_freq = time_tolerance;
	else if (time_freq < -time_tolerance)
		time_freq = -time_tolerance;

	s = hr_clock_lock();
	tod_needsync = 1;
	hr_clock_unlock(s);
}

/*
 * ddi_hardpps() - discipline CPU clock oscillator to external PPS signal
 *
 * This routine is called at each PPS interrupt in order to discipline
 * the CPU clock oscillator to the PPS signal. It measures the PPS phase
 * and leaves it in a handy spot for the clock() routine. It
 * integrates successive PPS phase differences and calculates the
 * frequency offset. This is used in clock() to discipline the CPU
 * clock oscillator so that intrinsic frequency error is cancelled out.
 * The code requires the caller to capture the time and hardware counter
 * value at the on-time PPS signal transition.
 *
 * Note that, on some Unix systems, this routine runs at an interrupt
 * priority level higher than the timer interrupt routine clock().
 * Therefore, the variables used are distinct from the clock()
 * variables, except for certain exceptions: The PPS frequency pps_freq
 * and phase pps_offset variables are determined by this routine and
 * updated atomically. The time_tolerance variable can be considered a
 * constant, since it is infrequently changed, and then only when the
 * PPS signal is disabled. The watchdog counter pps_valid is updated
 * once per second by clock() and is atomically cleared in this
 * routine.
 *
 * tvp is the time of the last tick; usec is a microsecond count since the
 * last tick.
 *
 * Note: In Solaris systems, the tick value is actually given by
 *       usec_per_tick.  This is called from the serial driver cdintr(),
 *	 or equivalent, at a high PIL.  Because the kernel keeps a
 *	 highresolution time, the following code can accept either
 *	 the traditional argument pair, or the current highres timestamp
 *       in tvp and zero in usec.
 */
void
ddi_hardpps(struct timeval *tvp, int usec)
{
	int u_usec, v_usec, bigtick;
	time_t cal_sec;
	int cal_usec;

	/*
	 * An occasional glitch can be produced when the PPS interrupt
	 * occurs in the clock() routine before the time variable is
	 * updated. Here the offset is discarded when the difference
	 * between it and the last one is greater than tick/2, but not
	 * if the interval since the first discard exceeds 30 s.
	 */
	time_status |= STA_PPSSIGNAL;
	time_status &= ~(STA_PPSJITTER | STA_PPSWANDER | STA_PPSERROR);
	pps_valid = 0;
	u_usec = -tvp->tv_usec;
	if (u_usec < -(MICROSEC/2))
		u_usec += MICROSEC;
	v_usec = pps_offset - u_usec;
	if (v_usec < 0)
		v_usec = -v_usec;
	if (v_usec > (usec_per_tick >> 1)) {
		if (pps_glitch > MAXGLITCH) {
			pps_glitch = 0;
			pps_tf[2] = u_usec;
			pps_tf[1] = u_usec;
		} else {
			pps_glitch++;
			u_usec = pps_offset;
		}
	} else
		pps_glitch = 0;

	/*
	 * A three-stage median filter is used to help deglitch the pps
	 * time. The median sample becomes the time offset estimate; the
	 * difference between the other two samples becomes the time
	 * dispersion (jitter) estimate.
	 */
	pps_tf[2] = pps_tf[1];
	pps_tf[1] = pps_tf[0];
	pps_tf[0] = u_usec;
	if (pps_tf[0] > pps_tf[1]) {
		if (pps_tf[1] > pps_tf[2]) {
			pps_offset = pps_tf[1];		/* 0 1 2 */
			v_usec = pps_tf[0] - pps_tf[2];
		} else if (pps_tf[2] > pps_tf[0]) {
			pps_offset = pps_tf[0];		/* 2 0 1 */
			v_usec = pps_tf[2] - pps_tf[1];
		} else {
			pps_offset = pps_tf[2];		/* 0 2 1 */
			v_usec = pps_tf[0] - pps_tf[1];
		}
	} else {
		if (pps_tf[1] < pps_tf[2]) {
			pps_offset = pps_tf[1];		/* 2 1 0 */
			v_usec = pps_tf[2] - pps_tf[0];
		} else  if (pps_tf[2] < pps_tf[0]) {
			pps_offset = pps_tf[0];		/* 1 0 2 */
			v_usec = pps_tf[1] - pps_tf[2];
		} else {
			pps_offset = pps_tf[2];		/* 1 2 0 */
			v_usec = pps_tf[1] - pps_tf[0];
		}
	}
	if (v_usec > MAXTIME)
		pps_jitcnt++;
	v_usec = (v_usec << PPS_AVG) - pps_jitter;
	pps_jitter += v_usec / (1 << PPS_AVG);
	if (pps_jitter > (MAXTIME >> 1))
		time_status |= STA_PPSJITTER;

	/*
	 * During the calibration interval adjust the starting time when
	 * the tick overflows. At the end of the interval compute the
	 * duration of the interval and the difference of the hardware
	 * counters at the beginning and end of the interval. This code
	 * is deliciously complicated by the fact valid differences may
	 * exceed the value of tick when using long calibration
	 * intervals and small ticks. Note that the counter can be
	 * greater than tick if caught at just the wrong instant, but
	 * the values returned and used here are correct.
	 */
	bigtick = (int)usec_per_tick * SCALE_USEC;
	pps_usec -= pps_freq;
	if (pps_usec >= bigtick)
		pps_usec -= bigtick;
	if (pps_usec < 0)
		pps_usec += bigtick;
	pps_time.tv_sec++;
	pps_count++;
	if (pps_count < (1 << pps_shift))
		return;
	pps_count = 0;
	pps_calcnt++;
	u_usec = usec * SCALE_USEC;
	v_usec = pps_usec - u_usec;
	if (v_usec >= bigtick >> 1)
		v_usec -= bigtick;
	if (v_usec < -(bigtick >> 1))
		v_usec += bigtick;
	if (v_usec < 0)
		v_usec = -(-v_usec >> pps_shift);
	else
		v_usec = v_usec >> pps_shift;
	pps_usec = u_usec;
	cal_sec = tvp->tv_sec;
	cal_usec = tvp->tv_usec;
	cal_sec -= pps_time.tv_sec;
	cal_usec -= pps_time.tv_usec;
	if (cal_usec < 0) {
		cal_usec += MICROSEC;
		cal_sec--;
	}
	pps_time = *tvp;

	/*
	 * Check for lost interrupts, noise, excessive jitter and
	 * excessive frequency error. The number of timer ticks during
	 * the interval may vary +-1 tick. Add to this a margin of one
	 * tick for the PPS signal jitter and maximum frequency
	 * deviation. If the limits are exceeded, the calibration
	 * interval is reset to the minimum and we start over.
	 */
	u_usec = (int)usec_per_tick << 1;
	if (!((cal_sec == -1 && cal_usec > (MICROSEC - u_usec)) ||
	    (cal_sec == 0 && cal_usec < u_usec)) ||
	    v_usec > time_tolerance || v_usec < -time_tolerance) {
		pps_errcnt++;
		pps_shift = PPS_SHIFT;
		pps_intcnt = 0;
		time_status |= STA_PPSERROR;
		return;
	}

	/*
	 * A three-stage median filter is used to help deglitch the pps
	 * frequency. The median sample becomes the frequency offset
	 * estimate; the difference between the other two samples
	 * becomes the frequency dispersion (stability) estimate.
	 */
	pps_ff[2] = pps_ff[1];
	pps_ff[1] = pps_ff[0];
	pps_ff[0] = v_usec;
	if (pps_ff[0] > pps_ff[1]) {
		if (pps_ff[1] > pps_ff[2]) {
			u_usec = pps_ff[1];		/* 0 1 2 */
			v_usec = pps_ff[0] - pps_ff[2];
		} else if (pps_ff[2] > pps_ff[0]) {
			u_usec = pps_ff[0];		/* 2 0 1 */
			v_usec = pps_ff[2] - pps_ff[1];
		} else {
			u_usec = pps_ff[2];		/* 0 2 1 */
			v_usec = pps_ff[0] - pps_ff[1];
		}
	} else {
		if (pps_ff[1] < pps_ff[2]) {
			u_usec = pps_ff[1];		/* 2 1 0 */
			v_usec = pps_ff[2] - pps_ff[0];
		} else  if (pps_ff[2] < pps_ff[0]) {
			u_usec = pps_ff[0];		/* 1 0 2 */
			v_usec = pps_ff[1] - pps_ff[2];
		} else {
			u_usec = pps_ff[2];		/* 1 2 0 */
			v_usec = pps_ff[1] - pps_ff[0];
		}
	}

	/*
	 * Here the frequency dispersion (stability) is updated. If it
	 * is less than one-fourth the maximum (MAXFREQ), the frequency
	 * offset is updated as well, but clamped to the tolerance. It
	 * will be processed later by the clock() routine.
	 */
	v_usec = (v_usec >> 1) - pps_stabil;
	if (v_usec < 0)
		pps_stabil -= -v_usec >> PPS_AVG;
	else
		pps_stabil += v_usec >> PPS_AVG;
	if (pps_stabil > MAXFREQ >> 2) {
		pps_stbcnt++;
		time_status |= STA_PPSWANDER;
		return;
	}
	if (time_status & STA_PPSFREQ) {
		if (u_usec < 0) {
			pps_freq -= -u_usec >> PPS_AVG;
			if (pps_freq < -time_tolerance)
				pps_freq = -time_tolerance;
			u_usec = -u_usec;
		} else {
			pps_freq += u_usec >> PPS_AVG;
			if (pps_freq > time_tolerance)
				pps_freq = time_tolerance;
		}
	}

	/*
	 * Here the calibration interval is adjusted. If the maximum
	 * time difference is greater than tick / 4, reduce the interval
	 * by half. If this is not the case for four consecutive
	 * intervals, double the interval.
	 */
	if (u_usec << pps_shift > bigtick >> 2) {
		pps_intcnt = 0;
		if (pps_shift > PPS_SHIFT)
			pps_shift--;
	} else if (pps_intcnt >= 4) {
		pps_intcnt = 0;
		if (pps_shift < PPS_SHIFTMAX)
			pps_shift++;
	} else
		pps_intcnt++;

	/*
	 * If recovering from kadb, then make sure the tod chip gets resynced.
	 * If we took an early exit above, then we don't yet have a stable
	 * calibration signal to lock onto, so don't mark the tod for sync
	 * until we get all the way here.
	 */
	{
		int s = hr_clock_lock();

		tod_needsync = 1;
		hr_clock_unlock(s);
	}
}

/*
 * Handle clock tick processing for a thread.
 * Check for timer action, enforce CPU rlimit, do profiling etc.
 */
void
clock_tick(kthread_id_t t)
{
	struct proc *pp;
	klwp_id_t    lwp;
	struct as *as;
	clock_t	utime;
	clock_t	stime;
	int	poke = 0;		/* notify another CPU */
	int	user_mode;

	/* Must be operating on a lwp/thread */
	if ((lwp = ttolwp(t)) == NULL)
		cmn_err(CE_PANIC, "clock_tick: no lwp");

	CL_TICK(t);	/* Class specific tick processing */

	pp = ttoproc(t);

	/* pp->p_lock makes sure that the thread does not exit */
	ASSERT(MUTEX_HELD(&pp->p_lock));

	user_mode = (lwp->lwp_state == LWP_USER);

	/*
	 * Update process times. Should use high res clock and state
	 * changes instead of statistical sampling method. XXX
	 */
	if (user_mode) {
		pp->p_utime++;
		lwp->lwp_utime++;
	} else {
		pp->p_stime++;
		lwp->lwp_stime++;
	}
	as = pp->p_as;

	/*
	 * Update user profiling statistics. Get the pc from the
	 * lwp when the AST happens.
	 */
	if (pp->p_prof.pr_scale) {
		atomic_add_32(&lwp->lwp_oweupc, 1);
		if (user_mode) {
			poke = 1;
			aston(t);
		}
	}

	utime = pp->p_utime;
	stime = pp->p_stime;

	/*
	 * If CPU was in user state, process lwp-virtual time
	 * interval timer.
	 */
	if (user_mode &&
	    timerisset(&lwp->lwp_timer[ITIMER_VIRTUAL].it_value) &&
	    itimerdecr(&lwp->lwp_timer[ITIMER_VIRTUAL], usec_per_tick) == 0) {
		poke = 1;
		sigtoproc(pp, t, SIGVTALRM);
	}

	if (timerisset(&lwp->lwp_timer[ITIMER_PROF].it_value) &&
	    itimerdecr(&lwp->lwp_timer[ITIMER_PROF], usec_per_tick) == 0) {
		poke = 1;
		sigtoproc(pp, t, SIGPROF);
	}

	/*
	 * Enforce CPU rlimit.
	 */
	if (!UNLIMITED_CUR(pp, RLIMIT_CPU) &&
	    (rlim64_t)((utime/hz) + (stime/hz)) > P_CURLIMIT(pp, RLIMIT_CPU)) {
		poke = 1;
		sigtoproc(pp, NULL, SIGXCPU);
	}

	/*
	 * Update memory usage for the currently running process.
	 */
	PTOU(pp)->u_mem += rm_asrss(as);

	/*
	 * Notify the CPU the thread is running on.
	 */
	if (poke && t->t_cpu != CPU)
		poke_cpu(t->t_cpu->cpu_id);
}

void
profil_tick(uintptr_t upc)
{
	int ticks;
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct prof *pr = &p->p_prof;

	do {
		ticks = lwp->lwp_oweupc;
	} while (cas32(&lwp->lwp_oweupc, ticks, 0) != ticks);

	mutex_enter(&p->p_pflock);
	if (pr->pr_scale >= 2 && upc >= pr->pr_off) {
		/*
		 * Old-style profiling
		 */
		uint16_t *slot = pr->pr_base;
		uint16_t old, new;
		if (pr->pr_scale != 2) {
			uintptr_t delta = upc - pr->pr_off;
			uintptr_t byteoff = ((delta >> 16) * pr->pr_scale) +
			    (((delta & 0xffff) * pr->pr_scale) >> 16);
			if (byteoff >= (uintptr_t)pr->pr_size) {
				mutex_exit(&p->p_pflock);
				return;
			}
			slot += byteoff / sizeof (uint16_t);
		}
		if (fuword16(slot, &old) < 0 ||
		    (new = old + ticks) > SHRT_MAX ||
		    suword16(slot, new) < 0) {
			pr->pr_scale = 0;
		}
	} else if (pr->pr_scale == 1) {
		/*
		 * PC Sampling
		 */
		model_t model = lwp_getdatamodel(lwp);
		int result;

#ifdef lint
		model = model;
#endif

		while (ticks-- > 0) {
			if (pr->pr_samples == pr->pr_size) {
				/* buffer full, turn off sampling */
				pr->pr_scale = 0;
				break;
			}
			switch (SIZEOF_PTR(model)) {
			case sizeof (uint32_t):
				result = suword32(pr->pr_base, (uint32_t)upc);
				break;
/* XXX Merced -- fix me */
#if !(defined(lint) || defined(__lint)) || !defined(__ia64)
			case sizeof (uint64_t):
				result = suword64(pr->pr_base, (uint64_t)upc);
				break;
#endif
			default:
				cmn_err(CE_WARN, "profil_tick: unexpected "
				    "data model");
				result = -1;
				break;
			}
			if (result != 0) {
				pr->pr_scale = 0;
				break;
			}
			pr->pr_base = (caddr_t)pr->pr_base + SIZEOF_PTR(model);
			pr->pr_samples++;
		}
	}
	mutex_exit(&p->p_pflock);
}

static void
delay_wakeup(void *arg)
{
	kthread_id_t t = arg;

	mutex_enter(&delay_lock);
	cv_signal(&t->t_delay_cv);
	mutex_exit(&delay_lock);
}

void
delay(clock_t ticks)
{
	clock_t deadline;

	if (panicstr) {
		/*
		 * Timeouts aren't running, so all we can do is spin.
		 */
		drv_usecwait(TICK_TO_USEC(ticks));
		return;
	}

	if (ticks <= 0)
		return;
	mutex_enter(&delay_lock);
	deadline = lbolt + ticks;
	(void) timeout(delay_wakeup, curthread, ticks);
	while ((deadline - lbolt) > 0)
		cv_wait(&curthread->t_delay_cv, &delay_lock);
	mutex_exit(&delay_lock);
}

/*
 * Like delay, but interruptible by a signal.
 */
int
delay_sig(clock_t ticks)
{
	clock_t deadline;
	clock_t rc;

	if (ticks <= 0)
		return (0);

	mutex_enter(&delay_lock);
	deadline = lbolt + ticks;
	do {
		rc = cv_timedwait_sig(&curthread->t_delay_cv, &delay_lock,
		    deadline);
	} while (rc > 0);
	mutex_exit(&delay_lock);
	if (rc == 0)
		return (EINTR);
	return (0);
}

#define	SECONDS_PER_DAY 86400

/*
 * Initialize the system time based on the TOD chip.  approx is used as
 * an approximation of time (e.g. from the filesystem) in the event that
 * the TOD chip has been cleared or is unresponsive.  An approx of -1
 * means the filesystem doesn't keep time.
 */
void
clkset(time_t approx)
{
	timestruc_t ts;
	int spl;
	int set_hrestime = 0;

	mutex_enter(&tod_lock);
	ts = tod_get();

	if (ts.tv_sec > 365 * SECONDS_PER_DAY) {
		/*
		 * If the TOD chip is reporting some time after 1971,
		 * then it probably didn't lose power or become otherwise
		 * cleared in the recent past;  check to assure that
		 * the time coming from the filesystem isn't in the future
		 * according to the TOD chip.
		 */
		if (approx != -1 && approx > ts.tv_sec) {
			cmn_err(CE_WARN, "Last shutdown is later "
			    "than time on time-of-day chip; check date.");
		}
	} else {
		/*
		 * If the TOD chip isn't giving correct time, then set it to
		 * the time that was passed in as a rough estimate.  If we
		 * don't have an estimate, then set the clock back to a time
		 * when Oliver North, ALF and Dire Straits were all on the
		 * collective brain:  1987.
		 */
		timestruc_t tmp;
		if (approx == -1)
			ts.tv_sec = (1987 - 1970) * 365 * SECONDS_PER_DAY;
		else
			ts.tv_sec = approx;
		ts.tv_nsec = 0;

		/*
		 * Attempt to write the new time to the TOD chip.  Set spl high
		 * to avoid getting preempted between the tod_set and tod_get.
		 */
		spl = splhi();
		tod_set(ts);
		tmp = tod_get();
		splx(spl);

		if (tmp.tv_sec != ts.tv_sec && tmp.tv_sec != ts.tv_sec + 1) {
			tod_broken = 1;
			dosynctodr = 0;
			cmn_err(CE_WARN, "Time-of-day chip unresponsive;"
			    " dead batteries?");
		} else {
			cmn_err(CE_WARN, "Time-of-day chip had "
			    "incorrect date; check and reset.");
		}
		set_hrestime = 1;
	}

	if (!boot_time) {
		boot_time = ts.tv_sec;
		set_hrestime = 1;
	}

	if (set_hrestime) {
		spl = hr_clock_lock();
		hrestime = ts;
		timedelta = 0;
		hr_clock_unlock(spl);
	}
	mutex_exit(&tod_lock);
}

/*
 * The following is for computing the percentage of cpu time used recently
 * by an lwp.  The function cpu_decay() is also called from /proc code.
 *
 * exp_x(x):
 * Given x as a 64-bit non-negative scaled integer of arbitrary magnitude,
 * Return exp(-x) as a 64-bit scaled integer in the range [0 .. 1].
 *
 * Scaling for 64-bit scaled integer:
 * The binary point is to the right of the high-order bit
 * of the low-order 32-bit word.
 */

#define	LSHIFT	31
#define	LSI_ONE	((uint32_t)1 << LSHIFT)	/* 32-bit scaled integer 1 */

#ifdef DEBUG
uint_t expx_cnt = 0;	/* number of calls to exp_x() */
uint_t expx_mul = 0;	/* number of long multiplies in exp_x() */
#endif

static uint64_t
exp_x(uint64_t x)
{
	int i;
	uint64_t ull;
	uint32_t ui;

#ifdef DEBUG
	expx_cnt++;
#endif
	/*
	 * By the formula:
	 *	exp(-x) = exp(-x/2) * exp(-x/2)
	 * we keep halving x until it becomes small enough for
	 * the following approximation to be accurate enough:
	 *	exp(-x) = 1 - x
	 * We reduce x until it is less than 1/4 (the 2 in LSHIFT-2 below).
	 * Our final error will be smaller than 4% .
	 */

	/*
	 * Use a uint64_t for the initial shift calculation.
	 */
	ull = x >> (LSHIFT-2);

	/*
	 * Short circuit:
	 * A number this large produces effectively 0 (actually .005).
	 * This way, we will never do more than 5 multiplies.
	 */
	if (ull >= (1 << 5))
		return (0);

	ui = ull;	/* OK.  Now we can use a uint_t. */
	for (i = 0; ui != 0; i++)
		ui >>= 1;

	if (i != 0) {
#ifdef DEBUG
		expx_mul += i;	/* seldom happens */
#endif
		x >>= i;
	}

	/*
	 * Now we compute 1 - x and square it the number of times
	 * that we halved x above to produce the final result:
	 */
	x = LSI_ONE - x;
	while (i--)
		x = (x * x) >> LSHIFT;

	return (x);
}

/*
 * Given the old percent cpu and a time delta in clock ticks,
 * return the new decayed percent cpu:  pct * exp(-tau),
 * where 'tau' is the time delta multiplied by a decay factor.
 * We have chosen the decay factor (cpu_decay_factor in param.c)
 * to make the decay over five seconds be approximately 20%.
 *
 * 'pct' is a 32-bit scaled integer <= 1
 * The binary point is to the right of the high-order bit
 * of the 32-bit word.
 */
uint32_t
cpu_decay(uint32_t pct, clock_t ticks)
{
	uint64_t delta = (uint_t)ticks;

	delta *= cpu_decay_factor;
	return ((pct * exp_x(delta)) >> LSHIFT);
}

/*
 * Given the old percent cpu and a time delta in clock ticks,
 * return the new grown percent cpu:  1 - ( 1 - pct ) * exp(-tau)
 */
uint32_t
cpu_grow(uint32_t pct, clock_t ticks)
{
	return (LSI_ONE - cpu_decay(LSI_ONE - pct, ticks));
}

static clock_t deadman_lbolt;
static uint_t deadman_seconds;
static uint_t deadman_countdown;
static uint_t deadman_panics;
static int deadman_enabled = 0;
static int deadman_sync_timeout = 20;
int deadman_sync_timeleft;
int deadman_panic_timers = 1;

static void
deadman(void)
{
	if (panicstr) {
		/*
		 * If we're panicking, the deadman cyclic continues to increase
		 * lbolt in case the dump device driver relies on this for
		 * timeouts.  Note that we rely on deadman() being invoked once
		 * per second, and credit lbolt and lbolt64 with hz ticks each.
		 */
		lbolt += hz;
		lbolt64 += hz;

		if (!deadman_panic_timers)
			return; /* allow all timers to be manually disabled */

		/*
		 * If the panic thread is still attempting to sync filesystems,
		 * we force a timeout panic if (1) we expire the sync_timeleft
		 * timer AND (2) an additional deadman_sync_timeout seconds
		 * elapse without the panicking thread setting sync_aborted or
		 * incrementing sync_timeleft.  We are assuming that if the
		 * panicking thread sets the sync_aborted flag, it will return
		 * to panicsys() and begin the dump.
		 */
		if (!sync_aborted) {
			if (deadman_sync_timeleft && sync_timeleft <= 0) {
				if (--deadman_sync_timeleft == 0)
					panic("panic sync timeout");
			} else if (sync_timeleft && (--sync_timeleft <= 0) &&
			    !sync_making_progress(0))
				deadman_sync_timeleft = deadman_sync_timeout;
		}

		/*
		 * If the dump timer is set and we expire it, panic again.
		 */
		if (dump_timeleft && (--dump_timeleft == 0))
			panic("panic dump timeout");
		return;
	}

	if (lbolt != deadman_lbolt) {
		deadman_lbolt = lbolt;
		deadman_countdown = deadman_seconds;
		return;
	}

	if (deadman_countdown-- > 0)
		return;

	/*
	 * Regardless of whether or not we actually bring the system down,
	 * bump the deadman_panics variable.
	 */
	deadman_panics++;

	if (!deadman_enabled) {
		deadman_countdown = deadman_seconds;
		return;
	}

	/*
	 * If we're here, we want to bring the system down.
	 */
	cmn_err(CE_PANIC, "deadman: timed out after %d seconds of clock "
	    "inactivity", deadman_seconds);
}

void
deadman_init(void)
{
	cyc_handler_t hdlr;
	cyc_time_t when;

	if (deadman_seconds == 0)
		deadman_seconds = snoop_interval / MICROSEC;

	if (snooping)
		deadman_enabled = 1;

	hdlr.cyh_func = (cyc_func_t)deadman;
	hdlr.cyh_level = CY_HIGH_LEVEL;
	hdlr.cyh_arg = NULL;

	/*
	 * Assure that we'll start precisely on the second, starting with
	 * the next second.  The interval must be one second in accordance with
	 * the code in deadman() above to increase lbolt during panic.
	 */
	when.cyt_when = 0;
	when.cyt_interval = NANOSEC;

	mutex_enter(&cpu_lock);
	deadman_cyclic = cyclic_add(&hdlr, &when);
	mutex_exit(&cpu_lock);
}

/*
 * tod_fault() is for updating tod validate mechanism state:
 * (1) TOD_NOFAULT: for resetting the state to 'normal'.
 *     currently used for debugging only
 * (2) The following four cases detected by tod validate mechanism:
 *       TOD_REVERSED: current tod value is less than previous value.
 *       TOD_STALLED: current tod value hasn't advanced.
 *       TOD_JUMPED: current tod value advanced too far from previous value.
 *       TOD_RATECHANGED: the ratio between average tod delta and
 *       average tick delta has changed.
 */
enum tod_fault_type
tod_fault(enum tod_fault_type ftype, int off)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	if (tod_faulted != ftype) {
		switch (ftype) {
		case TOD_NOFAULT:
			plat_tod_fault(TOD_NOFAULT);
			cmn_err(CE_NOTE, "Restarted tracking "
					"Time of Day clock.");
			tod_get_loopnum = 0;
			tod_faulted = ftype;
			break;
		case TOD_REVERSED:
		case TOD_JUMPED:
			if (tod_faulted == TOD_NOFAULT) {
				plat_tod_fault(ftype);
				cmn_err(CE_WARN, "Time of Day clock error: "
					"reason [%s by 0x%x]. -- "
					" Stopped tracking Time Of Day clock.",
					tod_fault_table[ftype], off);
				tod_faulted = ftype;
			}
			break;
		case TOD_STALLED:
		case TOD_RATECHANGED:
			if (tod_faulted == TOD_NOFAULT) {
				plat_tod_fault(ftype);
				cmn_err(CE_WARN, "Time of Day clock error: "
					"reason [%s]. -- "
					" Stopped tracking Time Of Day clock.",
					tod_fault_table[ftype]);
				tod_faulted = ftype;
			}
			break;
		default:
			break;
		}
	}
	return (tod_faulted);
}

void
tod_fault_reset()
{
	tod_fault_reset_flag = 1;
}


/*
 * tod_validate() is used for checking values returned by tod_get().
 * Four error cases can be detected by this routine:
 *   TOD_REVERSED: current tod value is less than previous.
 *   TOD_STALLED: current tod value hasn't advanced.
 *   TOD_JUMPED: current tod value advanced too far from previous value.
 *   TOD_RATECHANGED: the ratio between average tod delta and
 *   average tick delta has changed.
 * NOTE: if the tick parameter equals zero, tod_validate() does not
 * compare tod to a reference timer, and only the TOD_REVERSED
 * error case can be detected.
 */
time_t
tod_validate(time_t tod, hrtime_t tick)
{
	hrtime_t diff_tick;
	time_t dtodsec;
	time_t todsec = tod;
	int off = 0;
	long dtick;
	int dtick_delta;
	enum tod_fault_type tod_bad;
	static long dtick_avg;		/* init when tod_get_loopnum == 1 */
	static hrtime_t prev_tick;	/* init when tod_get_loopnum > 0 */
	static time_t prev_todsec;	/* init when tod_get_loopnum > 0 */

	ASSERT(MUTEX_HELD(&tod_lock));

	if (tod_validate_enable == 0) {
		return (todsec);
	}


	if (tod_fault_reset_flag > 0) {
		tod_get_loopnum = 0;
		tod_fault_reset_flag = 0;
	}

	/* test hook */
	switch (tod_unit_test) {
	case 1: /* for testing jumping tod */
		todsec += tod_test_injector;
		tod_unit_test = 0;
		break;
	case 2:	/* for testing stuck tod bit */
		todsec |= 1 << tod_test_injector;
		tod_unit_test = 0;
		break;
	case 3:	/* for testing stalled tod */
		todsec = prev_todsec;
		tod_unit_test = 0;
		break;
	case 4:	/* reset tod fault status */
		(void) tod_fault(TOD_NOFAULT, 0);
		tod_unit_test = 0;
		break;
	default:
		break;
	}

	tod_bad = TOD_NOFAULT;

	if (tod_get_loopnum == 0) {
		tod_get_loopnum++;
	} else if (todsec < prev_todsec) {
		/* ERROR - tod reversed */
		tod_bad = TOD_REVERSED;
		off = prev_todsec - todsec;
	} else if (tick == (hrtime_t)0) {
		return (todsec);
	} else if ((diff_tick = tick - prev_tick) < 0) {
		/* this should not happen */
		tod_get_loopnum = 0;
	} else if (tod_get_loopnum < TOD_FILTER_SETTLE) {
		dtodsec = todsec - prev_todsec;
		if (dtodsec == 0) {
			/* tod did not advance */
			if (diff_tick > TOD_STALL_THRESHOLD) {
				/* ERROR - tod stalled */
				tod_bad = TOD_STALLED;
			}
		} else {
			/* dtodsec >= 1 */
			dtick = diff_tick / dtodsec;

			if ((dtodsec > 2) && (dtick < TOD_JUMP_THRESHOLD)) {
				/* ERROR - tod jumped */
				tod_bad = TOD_JUMPED;
				off = (int)dtodsec;
			} else {
				if (tod_get_loopnum == 1) {
					dtick_avg = dtick;
				} else {
					dtick_avg += ((dtick - dtick_avg) /
							TOD_FILTER_N);
				}
			}
			tod_get_loopnum++;
		}
	} else {
		/* tod_get_loopnum >= TOD_FILTER_SETTLE */
		dtodsec = todsec - prev_todsec;

		if (dtodsec == 0) {
			/* tod did not advance */
			if (diff_tick > TOD_STALL_THRESHOLD) {
				/* ERROR - tod stalled */
				tod_bad = TOD_STALLED;
			}
		} else {
			/* calculate dtick */
			dtick = diff_tick / dtodsec;
			if ((dtodsec > 2) && (dtick < TOD_JUMP_THRESHOLD)) {
				/* ERROR - tod jumped */
				tod_bad = TOD_JUMPED;
				off = (int)dtodsec;
			} else {
				/* update dtick averages */
				dtick_avg += ((dtick - dtick_avg) /
							TOD_FILTER_N);

				/* calc dtick_delta */
				/* variation from reference freq in quartiles */
				dtick_delta = (dtick_avg - TOD_REF_FREQ) /
							(TOD_REF_FREQ >> 2);

				if (dtick_delta != 0) {
					/* ERROR - change in clock rate */
					tod_bad = TOD_RATECHANGED;

				}
			}
		}
	}

	if (tod_bad != TOD_NOFAULT) {
		(void) tod_fault(tod_bad, off);
		/* todsec correction -- crude */
		todsec = prev_todsec + (tick - prev_tick) / TOD_REF_FREQ;
	}

	prev_tick = tick;
	prev_todsec = todsec;

	return (todsec);
}
