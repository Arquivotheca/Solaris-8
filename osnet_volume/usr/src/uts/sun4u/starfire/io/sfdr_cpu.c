/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)sfdr_cpu.c 1.34     99/08/31 SMI"

/*
 * Starfire CPU support routines for DR
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/mem_config.h>
#include <sys/promif.h>
#include <sys/x_call.h>
#include <sys/cpu_sgnblk_defs.h>
#include <sys/membar.h>
#include <sys/stack.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <sys/spitregs.h>
#include <sys/cvc.h>
#include <sys/cpupart.h>
#include <sys/starfire.h>

#include <sys/archsystm.h>
#include <vm/hat_sfmmu.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/x_call.h>
#include <sys/cpu_module.h>

#include <sys/autoconf.h>
#include <sys/cmn_err.h>

#include <sys/dr.h>
#include <sys/sfdr.h>

extern processorid_t	sfdr_get_cpuid(dr_handle_t *hp, dnode_t nodeid);
extern struct cpu	*SIGBCPU;
extern void		kadb_cpu_on(int), kadb_cpu_off(int);
extern caddr_t		sfdr_shutdown_va;

static int	sfdr_obp_detach_cpu(int cpuid);
static int	sfdr_obp_cpu_detached(int cpuid);
static int	sfdr_obp_init_cvc_offset(int cpuid);
static int	sfdr_obp_move_bootproc(int cpuid);
static int	sfdr_cpu_start(struct cpu *cp);

static void	sfdr_cpu_stop_self(void);
static void	sfdr_cpu_shutdown_self(void);

/* Global */
static int	sfdr_cpu_delay = 100;
static int	sfdr_cpu_ntries = 50000;


int
sfdr_pre_release_cpu(dr_handle_t *hp, dr_devlist_t *devlist, int devnum)
{
	int		i, rv = 0, unit;
	dnode_t		nodeid;
	processorid_t	cpuid;
	struct cpu	*cpup;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_cpu_unit_t	*cp;
	static fn_t	f = "sfdr_pre_release_cpu";

	/*
	 * May have to juggle bootproc so need to grab cp_list_lock
	 * in addition cpu_lock.
	 */
	mutex_enter(&cp_list_lock);
	mutex_enter(&cpu_lock);

	/*
	 * Juggle the "bootproc", if necessary.
	 */
	if ((rv = sfdr_juggle_bootproc(hp, -1)) < 0) {
		mutex_exit(&cpu_lock);
		mutex_exit(&cp_list_lock);
		return (rv);
	}

	for (i = 0; i < devnum; i++) {
		nodeid = devlist[i].dv_nodeid;
		cpuid = sfdr_get_cpuid(hp, nodeid);
		if (cpuid < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get cpuid for "
				"nodeid (0x%x)", f, (uint_t)nodeid);
			continue;
		}
		unit = sfdr_get_unit(nodeid, DR_NT_CPU);
		if (unit < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get unit (cpu %d)",
				f, cpuid);
			continue;
		}
		cp = SFDR_GET_BOARD_CPUUNIT(sbp, unit);
		cp->sbc_cpu_status = cpu_status(cpu[cpuid]);

		if (cp->sbc_cpu_status == P_ONLINE) {
			PR_CPU("%s: offlining cpu %d\n", f, cpuid);
			if (cpu_offline(cpu[cpuid])) {
				char	errstr[80];

				sprintf(errstr,
					"%s: failed to offline cpu %d",
					f, cpuid);
				rv = -1;
				SFDR_SET_ERR_STR(HD2MACHERR(hp),
						SFDR_ERR_OFFLINE, errstr);
				PR_CPU("%s\n", errstr);
				cpup = cpu_get(cpuid);
				if (cpup && disp_bound_threads(cpup)) {
					cmn_err(CE_WARN, "sfdr:%s: thread(s) "
						"bound to cpu %d",
						f, cpup->cpu_id);
				}
			}
		}

		if (rv)
			break;
	}

	mutex_exit(&cpu_lock);
	mutex_exit(&cp_list_lock);

	if (rv) {
		/*
		 * Need to unwind others since at this level (pre-release)
		 * the device state has not yet transitioned and failures
		 * will prevent us from reaching the "post" release
		 * function where states are normally transitioned.
		 */
		for (; i >= 0; i--) {
			nodeid = devlist[i].dv_nodeid;
			unit = sfdr_get_unit(nodeid, DR_NT_CPU);
			if (unit >= 0)
				(void) sfdr_cancel_cpu(hp, unit);
		}
	}

	return (rv);
}

/*ARGSUSED*/
int
sfdr_pre_attach_cpu(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		i;
	int		unit;
	processorid_t	cpuid;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_state_t	dstate;
	dnode_t		nodeid;
	static fn_t	f = "sfdr_pre_attach_cpu";

	PR_CPU("%s...\n", f);

	for (i = 0; i < devnum; i++) {
		nodeid = devlist[i].dv_nodeid;
		cpuid = sfdr_get_cpuid(hp, nodeid);
		if (cpuid < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get cpuid for "
				"nodeid (0x%x)", f, (uint_t)nodeid);
			continue;
		}
		unit = sfdr_get_unit(nodeid, DR_NT_CPU);
		if (unit < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get unit for cpu %d",
				f, cpuid);
			continue;
		}

		cmn_err(CE_CONT,
			"DR: OS attach cpu-unit (%d.%d)\n",
			sbp->sb_num, unit);

		dstate = SFDR_DEVICE_STATE(sbp, DR_NT_CPU, unit);

		if (dstate == SFDR_STATE_UNCONFIGURED) {
			/*
			 * If we're coming from the UNCONFIGURED
			 * state then the cpu's sigblock will
			 * still be mapped in.  Need to unmap it
			 * before continuing with attachment.
			 */
			PR_CPU("%s: unmapping sigblk for cpu %d\n",
				f, cpuid);

			CPU_SGN_MAPOUT(cpuid);
		}
	}

	mutex_enter(&cpu_lock);

	return (0);
}

/*ARGSUSED*/
int
sfdr_post_attach_cpu(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		i;
	int		status;
	processorid_t	cpuid;
	struct cpu	*cp;
	dnode_t		nodeid;
	sfdr_handle_t	*shp = HD2MACHHD(hp);
	sfdr_err_t	err = SFDR_ERR_NOERROR;
	char		errstr[80];
	static fn_t	f = "sfdr_post_attach_cpu";

	PR_CPU("%s...\n", f);

	/* Startup and online newly-attached CPUs */
	for (i = 0; i < devnum; i++) {
		nodeid = devlist[i].dv_nodeid;
		cpuid = sfdr_get_cpuid(hp, nodeid);
		if (cpuid < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get cpuid for "
				"nodeid (0x%x)", f, (uint_t)nodeid);
			continue;
		}

		cp = cpu_get(cpuid);

		if (cp == NULL) {
			cmn_err(CE_WARN,
				"sfdr:%s: cpu_get failed for cpu %d",
				f, cpuid);
			continue;
		}

		if ((shp->sh_iap->i_flags & SFDR_FLAG_AUTO_CPU) == 0)
			continue;

		status = cpu_status(cp);

		switch (status) {
		case P_POWEROFF:
			if (cpu_poweron(cp) != 0) {
				err = SFDR_ERR_CPUSTART;
				sprintf(errstr,
					"%s: failed to power-on cpu %d",
					f, cpuid);
				cmn_err(CE_WARN, "sfdr:%s", errstr);
				break;
			}
			PR_CPU("%s: cpu %d powered ON\n", f, cpuid);
			/*FALLTHROUGH*/

		case P_OFFLINE:
			PR_CPU("%s: onlining cpu %d...\n", f, cpuid);

			if (cpu_online(cp) != 0) {
				sprintf(errstr,
					"%s: failed to online cpu %d",
					f, cp->cpu_id);
				err = SFDR_ERR_ONLINE;
				cmn_err(CE_WARN, "sfdr:%s", errstr);
			}
			break;

		default:
			break;
		}
	}

	mutex_exit(&cpu_lock);

	/* Return last error, if there was one. */
	if (err != SFDR_ERR_NOERROR) {
		SFDR_SET_ERR_STR(HD2MACHERR(hp), err, errstr);
		return (-1);
	} else {
		return (0);
	}
}

/*ARGSUSED*/
int
sfdr_pre_detach_cpu(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		i;
	int		unit;
	processorid_t	cpuid;
	dnode_t		nodeid;
	struct cpu	*cp;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	char		errstr[80];
	static fn_t	f = "sfdr_pre_detach_cpu";

	PR_CPU("%s...\n", f);

	mutex_enter(&cpu_lock);

	for (i = 0; i < devnum; i++) {
		nodeid = devlist[i].dv_nodeid;
		cpuid = sfdr_get_cpuid(hp, nodeid);
		if (cpuid < 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to get cpuid for "
				"nodeid (0x%x)", f, (uint_t)nodeid);
			continue;
		}

		cp = cpu_get(cpuid);

		if (cp == NULL)
			continue;

		unit = sfdr_get_unit(nodeid, DR_NT_CPU);

		cmn_err(CE_CONT,
			"DR: OS detach cpu-unit (%d.%d)\n",
			sbp->sb_num, unit);

		/*
		 * CPUs were offlined during Release.
		 */
		if (cpu_status(cp) == P_POWEROFF) {
			PR_CPU("%s: cpu %d already powered OFF\n", f, cpuid);
			continue;
		}

		if (cpu_down(cp)) {
			int	e;

			if ((e = cpu_poweroff(cp)) != 0) {
				sprintf(errstr,
					"%s: failed to power-off cpu %d "
					"(errno = %d)",
					f, cp->cpu_id, e);
				cmn_err(CE_WARN, "sfdr:%s", errstr);
				SFDR_SET_ERR_STR(HD2MACHERR(hp),
						SFDR_ERR_CPUSTOP,
						errstr);
				mutex_exit(&cpu_lock);
				return (-1);
			} else {
				PR_CPU("%s: cpu %d powered OFF\n",
					f, cpuid);
			}
		} else {
			sprintf(errstr, "%s: cpu %d still active",
				f, cp->cpu_id);
			SFDR_SET_ERR_STR(HD2MACHERR(hp),
					SFDR_ERR_BUSY,
					errstr);
			mutex_exit(&cpu_lock);
			return (-1);
		}
	}

	return (0);
}

/*ARGSUSED*/
int
sfdr_post_detach_cpu(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	static fn_t	f = "sfdr_post_detach_cpu";

	PR_CPU("%s...\n", f);

	mutex_exit(&cpu_lock);

	return (0);
}

/*
 * Cancel previous release operation for cpu.
 * For cpus this means simply bringing cpus that
 * were offline back online.  Note that they had
 * to have been online at the time there were
 * released.
 */
int
sfdr_cancel_cpu(dr_handle_t *hp, int unit)
{
	int		rv = 0;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_cpu_unit_t	*cp;
	static fn_t	f = "sfdr_cancel_cpu";

	cp = SFDR_GET_BOARD_CPUUNIT(sbp, unit);

	if (cp->sbc_cpu_status == P_ONLINE) {
		struct cpu	*cpup;

		/*
		 * CPU had been online, go ahead
		 * bring it back online.
		 */
		PR_CPU("%s: bringing cpu %d back ONLINE\n",
			f, cp->sbc_cpu_id);

		mutex_enter(&cpu_lock);
		cpup = cpu[cp->sbc_cpu_id];

		switch (cpu_status(cpup)) {
		case P_POWEROFF:
			if (cpu_poweron(cpup)) {
				cmn_err(CE_WARN,
					"sfdr:%s: failed to power-on "
					"cpu %d",
					f, cp->sbc_cpu_id);
				rv = -1;
				break;
			}
			/*FALLTHROUGH*/

		case P_OFFLINE:
			if (cpu_online(cpup)) {
				cmn_err(CE_WARN,
					"sfdr:%s: failed to online cpu %d",
					f, cp->sbc_cpu_id);
				rv = -1;
			}
			break;

		default:
			break;
		}
		mutex_exit(&cpu_lock);
	}

	return (rv);
}

int
sfdr_disconnect_cpu(dr_handle_t *hp, int unit)
{
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	int		ntries, rv, cpuid;
	dnode_t		nodeid;
	static fn_t	f = "sfdr_disconnect_cpu";

	PR_CPU("%s...\n", f);

	ASSERT((SFDR_DEVICE_STATE(sbp, DR_NT_CPU, unit) ==
						SFDR_STATE_CONNECTED) ||
		(SFDR_DEVICE_STATE(sbp, DR_NT_CPU, unit) ==
						SFDR_STATE_UNCONFIGURED));

	if (SFDR_DEVICE_STATE(sbp, DR_NT_CPU, unit) == SFDR_STATE_CONNECTED) {
		/*
		 * Cpus were never brought in and so are still
		 * effectively disconnected, so nothing to do here.
		 */
		PR_CPU("%s: cpu %d never brought in\n",
			f, (sbp->sb_num * 4) + unit);
		return (0);
	}

	nodeid = sbp->sb_devlist[NIX(DR_NT_CPU)][unit];

	if ((cpuid = sfdr_get_cpuid(hp, nodeid)) < 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to cpuid (%d.%d) for nodeid (0x%x)",
			f, sbp->sb_num, unit, (uint_t)nodeid);
		return (-1);
	}

	PR_CPU("%s: marking cpu %d detached in OBP\n", f, cpuid);

	if ((rv = sfdr_obp_detach_cpu(cpuid)) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed obp detach of cpu %d",
			f, cpuid);
		return (rv);
	}

	/*
	 * Make sure SIGBST_DETACHED is set before
	 * mapping out the sig block.
	 */
	ntries = sfdr_cpu_ntries;
	while (!sfdr_obp_cpu_detached(cpuid) && ntries) {
		DELAY(sfdr_cpu_delay);
		ntries--;
	}
	if (!sfdr_obp_cpu_detached(cpuid)) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to mark cpu %d detached in sigblock",
			f, cpuid);
	} else if (CPU_SGN_EXISTS(cpuid)) {
		CPU_SGN_MAPOUT(cpuid);
	}

	if (SFDR_DEVICE_STATE(sbp, DR_NT_CPU, unit) == SFDR_STATE_CONNECTED) {
		/*
		 * Cpus were never brought in and so are still
		 * effectively disconnected, so nothing to do here.
		 */
		PR_CPU("%s: cpu %d never brought in - reset not needed\n",
			f, cpuid);
	} else {
		/*
		 * We now PC IDLE the processor to guarantee we
		 * stop any transactions from coming from it.
		 * The alternative is to "RESET" (sfhw_cpu_reset_on)
		 * the processor, however that has not proven a reliable
		 * mechanism.  We have seen weird results that we are
		 * currently attributing to the RESET (e.g. arbstops, etc.).
		 */
		(void) sfhw_cpu_pc_idle(cpuid);
	}

	return (0);
}

/*
 * Start up a cpu.  It is possible that we're attempting to restart
 * the cpu after an UNCONFIGURE in which case the cpu will be
 * spinning in its cache.  So, all we have to do is wakeup him up.
 * Under normal circumstances the cpu will be coming from a previous
 * CONNECT and thus will be spinning in OBP.  In both cases, the
 * startup sequence is the same.
 */
int
sfdr_cpu_poweron(struct cpu *cp)
{
	static fn_t	f = "sfdr_cpu_poweron";

	PR_CPU("%s...\n", f);

	ASSERT(MUTEX_HELD(&cpu_lock));

	if (sfdr_cpu_start(cp) != 0)
		return (EBUSY);
	else
		return (0);
}

static int
sfdr_cpu_start(struct cpu *cp)
{
	int		cpuid = cp->cpu_id;
	int		ntries = sfdr_cpu_ntries;
	dnode_t		nodeid;
	extern int	restart_other_cpu(int, boolean_t);
	static fn_t	f = "sfdr_cpu_start";

	PR_CPU("%s...\n", f);

	ASSERT(MUTEX_HELD(&cpu_lock));

	nodeid = cpunodes[cpuid].nodeid;
	ASSERT(nodeid != (dnode_t)0);

	cp->cpu_flags &= ~CPU_POWEROFF;

	/*
	 * NOTE: restart_other_cpu pauses cpus during the
	 *	 slave cpu start.  This helps to quiesce the
	 *	 bus traffic a bit which makes the tick sync
	 *	 routine in the prom more robust.
	 */
	PR_CPU("%s: COLD START for cpu (%d)\n", f, cpuid);

	if (restart_other_cpu(cpuid, B_FALSE) != 0) {	/* start cpu */
		/*
		 * In the rare case that we have an error,
		 * try to recover.  The cpu will be "awake",
		 * but not started.  Note that we can't undo
		 * the wakeup, and if we retry the wakeup it
		 * should be a no-op.
		 */
		cp->cpu_flags |= CPU_POWEROFF;
		return (-1);
	}

	kadb_cpu_on(cpuid);		/* inform kadb */

	/*
	 * Wait for the cpu to reach its idle thread before
	 * we zap him with a request to blow away the mappings
	 * he (might) have for the sfdr_shutdown_asm code
	 * he may have executed on unconfigure.
	 */
	while ((cp->cpu_thread != cp->cpu_idle_thread) && (ntries > 0)) {
		DELAY(sfdr_cpu_delay);
		ntries--;
	}

	PR_CPU("%s: waited %d out of %d loops for cpu %d\n",
		f, sfdr_cpu_ntries - ntries, sfdr_cpu_ntries, cpuid);

	xt_one(cpuid, vtag_flushpage_tl1,
		(uint64_t)sfdr_shutdown_va, (uint64_t)KCONTEXT);

	return (0);
}

int
sfdr_cpu_poweroff(struct cpu *cp)
{
	int		ntries, cnt;
	processorid_t	cpuid;
	void		sfdr_cpu_shutdown_self(void);
	static fn_t	f = "sfdr_cpu_poweroff";

	PR_CPU("%s...\n", f);

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Capture all CPUs (except for detaching proc) to prevent
	 * crosscalls to the detaching proc until it has cleared its
	 * bit in cpu_ready_set.
	 */
	cpuid = cp->cpu_id;
	cp->cpu_m.in_prom = 0;

	pause_cpus(NULL);

	xt_one(cpuid, (xcfunc_t *)idle_stop_xcall,
		(uint64_t)sfdr_cpu_shutdown_self, NULL);

	PR_CPU("%s: waiting for cpu %d to go idle...\n", f, cpuid);

	ntries = sfdr_cpu_ntries;
	cnt = 0;
	while (!cp->cpu_m.in_prom && ntries) {
		DELAY(sfdr_cpu_delay);
		ntries--;
		cnt++;
	}
	cp->cpu_m.in_prom = 0;		/* steal the cache line back */
	DELAY(100000);

	start_cpus();

	return (0);
}

/*
 * A detaching CPU is xcalled with an xtrap to sfdr_cpu_stop_self() after
 * it has been offlined. The function of this routine is to get the cpu
 * spinning in a safe place. The requirement is that the system will not
 * reference anything on the detaching board (memory and i/o is detached
 * elsewhere) and that the CPU not reference anything on any other board
 * in the system.  This isolation is required during and after the writes
 * to the domain masks to remove the board from the domain.
 *
 * To accomplish this isolation the following is done:
 *	1) Create a locked mapping to a location in BBSRAM where
 *	   the cpu will execute.
 *	2) Copy the target function (sfdr_shutdown_asm) in which
 *	   the cpu will execute into BBSRAM.
 *	3) Jump into function with BBSRAM.
 *	   Function will:
 *	   3.1) Flush its Ecache (displacement).
 *	   3.2) Flush its Dcache with HW mechanism.
 *	   3.3) Flush its Icache with HW mechanism.
 *	   3.4) Flush all valid and _unlocked_ D-TLB entries.
 *	   3.5) Flush all valid and _unlocked_ I-TLB entries.
 *	4) Jump into a tight loop.
 */
#define	SFDR_BBSRAM_OFFSET	0x1000

/*ARGSUSED*/
static void
sfdr_cpu_stop_self(void)
{
	int		size, cpuid = (int)CPU->cpu_id;
	cpu_t		*cp = CPU;
	tte_t		tte;
	volatile uint_t	*src, *dst;
	uint_t		funclen;
	uint64_t	bbsram_pa, bbsram_offset;
	uint_t		bbsram_pfn;
	uint64_t	bbsram_addr, bbsram_estack;
	void		(*bbsram_func)(uint64_t, uint64_t, int);
	extern void	sfdr_shutdown_asm(uint64_t, uint64_t, int);
	extern void	sfdr_shutdown_asm_end(void);

	funclen = (uint_t)sfdr_shutdown_asm_end - (uint_t)sfdr_shutdown_asm;
	ASSERT(funclen <= MMU_PAGESIZE);
	/*
	 * We'll start from the 0th's base.
	 */
	bbsram_pa = STARFIRE_UPAID2UPS(cpuid) | STARFIRE_PSI_BASE;
	bbsram_offset = bbsram_pa | 0xfe0ULL;
	bbsram_pa += ldphysio(bbsram_offset) + SFDR_BBSRAM_OFFSET;

	bbsram_pfn = (uint_t)(bbsram_pa >> MMU_PAGESHIFT);

	bbsram_addr = (uint64_t)sfdr_shutdown_va;
	bbsram_estack = bbsram_addr + (uint64_t)funclen;

	tte.tte_inthi = TTE_VALID_INT | TTE_SZ_INT(TTE8K) |
			TTE_PFN_INTHI(bbsram_pfn);
	tte.tte_intlo = TTE_PFN_INTLO(bbsram_pfn) |
			TTE_HWWR_INT | TTE_PRIV_INT | TTE_LCK_INT;
	sfmmu_dtlb_ld(sfdr_shutdown_va, KCONTEXT, &tte);	/* load dtlb */
	sfmmu_itlb_ld(sfdr_shutdown_va, KCONTEXT, &tte);	/* load itlb */

	for (src = (uint_t *)sfdr_shutdown_asm, dst = (uint_t *)bbsram_addr;
		src < (uint_t *)sfdr_shutdown_asm_end; src++, dst++)
		*dst = *src;

	bbsram_func = (void (*)())bbsram_addr;
	size = (size_t)cpunodes[cpuid].ecache_size << 1;

	/*
	 * Signal to sfdr_cpu_poweroff() that we're just
	 * about done.
	 */
	cp->cpu_m.in_prom = 1;

	(*bbsram_func)(bbsram_estack, ecache_flushaddr, size);
}

static void
sfdr_cpu_shutdown_self(void)
{
	cpu_t		*cp = CPU;
	int		cpuid = cp->cpu_id;
	extern void	kadb_cpu_off(int);
	extern void	flush_windows(void);
	static fn_t	f = "sfdr_cpu_shutdown_self";

	flush_windows();
	CPUSET_DEL(cpu_ready_set, cpuid);

	(void) spl8();

	kadb_cpu_off(cpuid);

	cp->cpu_flags = CPU_OFFLINE | CPU_QUIESCED | CPU_POWEROFF;

	SGN_UPDATE_CPU_OS_DETACHED_NULL(cpuid);

	sfdr_cpu_stop_self();

	cmn_err(CE_PANIC, "%s: CPU %d FAILED TO SHUTDOWN", f, cpuid);
}


/*
 * sfdr_obp_detach_cpu()
 *  This requires two steps, first, we must put the cpuid into the OBP
 *  idle loop (Idle in Program) state.  Then we call OBP to place the CPU
 *  into the "Detached" state, which does any special processing to
 *  actually detach the cpu, such as flushing ecache, and also ensures
 *  that a subsequent breakpoint won't restart the cpu (if it was just in
 *  Idle in Program state).
 */
static int
sfdr_obp_detach_cpu(int cpuid)
{
	static fn_t	f = "sfdr_obp_detach_cpu";

	/*
	 * Cpu may not be under OBP's control. Eg, if cpu exited to download
	 * helper on a prior attach.
	 */
	if (CPU_SGN_EXISTS(cpuid) &&
			!SGN_CPU_IS_OS(cpuid) &&
			!SGN_CPU_IS_OBP(cpuid)) {
		cmn_err(CE_WARN,
			"sfdr:%s: unexpected signature (0x%x) for cpu %d",
			f, get_cpu_sgn(cpuid), cpuid);
		return (0);
	}

	/*
	 * Now we place the CPU into the "Detached" idle loop in OBP.
	 * This is so that the CPU won't be restarted if we break into
	 * OBP with a breakpoint or BREAK key from the console, and also
	 * if we need to do any special processing, such as flushing the
	 * cpu's ecache, disabling interrupts (by turning of the ET bit in
	 * the PSR) and/or spinning in BBSRAM rather than global memory.
	 */
	PR_CPU("%s: Cpu[%d] Calling dr-detach-cpu\n", f, cpuid);

	prom_interpret("dr-detach-cpu", (uintptr_t)cpuid, 0, 0, 0, 0);

	return (0);
}

/*
 * sfdr_obp_cpu_detached() returns TRUE if the cpu sigblock signature state
 * is SIGBST_DETACHED; otherwise it returns FALSE. This routine should only
 * be called after we have asked OBP to detach the CPU. It should NOT be
 * called as a check during any other flow.
 */
static int
sfdr_obp_cpu_detached(int cpuid)
{
	if (!CPU_SGN_EXISTS(cpuid) ||
		(SGN_CPU_IS_OS(cpuid) && SGN_CPU_STATE_IS_DETACHED(cpuid)))
		return (1);
	else
		return (0);
}


static int
sfdr_obp_init_cvc_offset(int cpuid)
{
	static fn_t	f = "sfdr_obp_init_cvc_offset";

	PR_CPU("%s: cpu %d\n", f, cpuid);

	prom_interpret("dr-init-cvc-off", (uintptr_t)cpuid, 0, 0, 0, 0);

	return (0);
}


static int
sfdr_obp_move_bootproc(int cpuid)
{
	int		retval;
	static fn_t	f = "sfdr_obp_move_bootproc";

	PR_CPU("%s: cpu %d\n", f, cpuid);

	prom_interpret("dr-move-cpu0", (uintptr_t)cpuid,
			(uintptr_t)&retval, 0, 0, 0);

	PR_CPU("%s: retval: %d\n", f, retval);

	if (retval == SFDR_OBP_PROBE_GOOD)
		return (0);
	if (retval == SFDR_OBP_PROBE_BAD)
		return (EIO);
	return (ENXIO);
}


/*
 * sfdr_move_bootproc()
 *   The goal here is to get cpu0 as quiet as possible before we try to
 *   move the cpu0 structure.  Unfortunately, cpu0 is a static structure rather
 *   than having a pointer to a structure.  This means that we have to copy
 *   the info from the "new" cpu0 into the cpu0 structure (and also save away
 *   the "old" cpu0 information).
 */
static int
sfdr_move_bootproc(dr_handle_t *hp, int new_bootproc)
{
	extern void juggle_sgnblk_poll(struct cpu *cp);
	register struct cpu	*new_sigbcpu;
	int			old_bootproc;
	int			rv;
	char			errstr[80];
	static fn_t		f = "sfdr_move_bootproc";

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&cpu_lock));

	if (new_bootproc == SIGBCPU->cpu_id) {
		cmn_err(CE_WARN,
			"sfdr:%s: SIGBCPU(%d) same as new selection(%d)",
			f, SIGBCPU->cpu_id, new_bootproc);
		return (0);
	}

	if ((new_bootproc < 0) || (new_bootproc >= max_ncpus)) {
		sprintf(errstr, "%s: invalid cpuid %d", f, new_bootproc);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_INVAL, errstr);
		return (-1);
	}

	if ((new_sigbcpu = cpu[new_bootproc]) == NULL) {
		sprintf(errstr, "%s: no cpu structure for cpuid %d",
			f, new_bootproc);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_INVAL, errstr);
		return (-1);
	}

	if ((new_sigbcpu->cpu_flags & (CPU_QUIESCED|CPU_OFFLINE))) {
		sprintf(errstr, "%s: cpu %d is not available",
			f, new_bootproc);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_INVAL, errstr);
		return (-1);
	}

	old_bootproc = SIGBCPU->cpu_id;

	/*
	 * If everything is copasetic, we give OBP the good news...
	 */
	cmn_err(CE_NOTE,
		"?sfdr:%s: relocating SIGBCPU from %d to %d",
		f, old_bootproc, new_bootproc);

	PR_CPU("%s: Cpu[%d] calling sfdr_obp_move_bootproc\n",
		f, new_bootproc);

	/*
	 * Tell OBP to initialize cvc-offset field of new CPU0
	 * so that it's in sync with OBP and cvc_server
	 */
	(void) sfdr_obp_init_cvc_offset(new_bootproc);

	/*
	 * Assign cvc to new cpu0's bbsram for I/O.  This has to be
	 * done BEFORE cpu0 is moved via obp, since sfdr_obp_move_bootproc()
	 * will cause obp_helper to switch to a different bbsram for
	 * cvc I/O.  We don't want cvc writing to a buffer from which
	 * nobody will pick up the data!
	 */
	cvc_assign_iocpu(new_bootproc);

	/* Juggle the sgnblk poll tick intr clnt to new bootproc */
	juggle_sgnblk_poll(cpu[new_bootproc]);

	if ((rv = sfdr_obp_move_bootproc(new_bootproc)) != 0) {
		/*
		 * The move failed, hopefully obp_helper is still back
		 * at the old_bootproc.  Move cvc back there.
		 */
		cvc_assign_iocpu(old_bootproc);

		juggle_sgnblk_poll(cpu[old_bootproc]);

		sprintf(errstr,
			"%s: failed to juggle SIGBCPU from %d to %d "
			"(err = %d)",
			f, old_bootproc, new_bootproc, rv);

		cmn_err(CE_WARN, "sfdr:%s", errstr);

		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_JUGGLE_BOOTPROC,
				errstr);

		return (-1);
	}

	/* We are here due to successful OBP operation */

	SIGBCPU = new_sigbcpu;

	return (0);
}

/*
 * If bootproc(SIGBCPU) is on the board we are performing an operation on,
 * move it to another board.
 */
int
sfdr_juggle_bootproc(dr_handle_t *hp, processorid_t cpuid)
{
	int		bdnum;
	int		lb_brd, ub_brd;
	int		max_cpu_per_sys = MAX_BOARDS * MAX_CPU_UNITS_PER_BOARD;
	struct cpu	*cp;
	static fn_t	f = "sfdr_juggle_bootproc";

	PR_CPU("%s...\n", f);

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&cpu_lock));

	if (cpuid < 0) {
		/*
		 * Target cpuid unspecified, need to select one.
		 */
		bdnum = BD2MACHBD(hp->h_bd)->sb_num;

		lb_brd = bdnum * MAX_CPU_UNITS_PER_BOARD;
		ub_brd = lb_brd + MAX_CPU_UNITS_PER_BOARD;

		if (SIGBCPU->cpu_id < lb_brd || SIGBCPU->cpu_id >= ub_brd) {

			/* SIGBCPU isn't on the board -- return OK */
			return (0);
		}

		/* Now determine if we've got an eligible cpu to switch to */

		for (cpuid = 0; cpuid < max_cpu_per_sys; cpuid++) {

			/* Must not be on the detaching board */
			if (cpuid >= lb_brd && cpuid < ub_brd) {
				continue;
			}

			cp = cpu_get(cpuid);

			if (cp == NULL)
				continue;	/* not present */

			/* must be online */
			if (cpu_status(cp) != P_ONLINE)
				continue;

			break;
		}
	}

	if (cpuid < max_cpu_per_sys) {

		/* candidate cpu found */
		PR_CPU("%s: move SIGBCPU to cpu[%d]\n", f, cpuid);
		if (sfdr_move_bootproc(hp, cpuid) != 0) {
			return (-1);
		} else {
			PR_CPU("%s: successfully juggled to cpu %d\n",
				f, cpuid);
			return (0);
		}
	} else {
		char	errstr[80];
		/* no eligible cpu for new SIGBCPU -- error */

		sprintf(errstr, "%s: no eligible cpu found for SIGBCPU", f);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_JUGGLE_BOOTPROC,
				errstr);
		return (-1);
	}
}
