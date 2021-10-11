/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysctrl_dr.c	1.50	99/08/17 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ndi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/autoconf.h>
#include <sys/intreg.h>
#include <sys/proc.h>
#include <sys/machsystm.h>
#include <sys/modctl.h>
#include <sys/callb.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/jtag.h>
#include <sys/ac.h>
#include <sys/simmstat.h>
#include <sys/clock.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/cpr.h>
#include <sys/cpuvar.h>
#include <sys/machcpuvar.h>
#include <sys/x_call.h>

#ifdef DEBUG
struct	regs_data {
	caddr_t msg;
	u_longlong_t physaddr;
	uint_t pre_dsct;
	uint_t post_dsct;
	uint_t eflag;
	uint_t oflag;
};

static	struct regs_data reg_tmpl[] = {
	"AC  Control and Starus reg = 0x", AC_BCSR(0), 0, 0, 0, 0,
	"FHC Control and Starus reg = 0x", FHC_CTRL(0), 0, 0, 0, 0,
	"JTAG Control reg = 0x", FHC_JTAG_CTRL(0), 0, 0, 0, 0,
	"Interrupt Group Number reg = 0x", FHC_IGN(0), 0, 0, 0, 0,
	"System Interrupt Mapping reg = 0x", FHC_SIM(0), 0, 0, 0, 0,
	"System Interrupt State reg = 0x", FHC_SSM(0), 0, 0, 0, 0,
	"UART Interrupt Mapping reg = 0x", FHC_UIM(0), 0, 0, 0, 0,
	"UART Interrupt State reg = 0x", FHC_USM(0), 0, 0, 0, 0
};

#define	NUM_REG  (sizeof (reg_tmpl)/sizeof (reg_tmpl[0]))
static	struct regs_data reg_dt[MAX_BOARDS][NUM_REG];

int sysctrl_enable_regdump = 0;

static void precache_regdump(int board);
static void display_regdump(void);
static void boardstat_regdump(void);

#endif /* DEBUG */

extern void bd_remove_poll(struct sysctrl_soft_state *);
extern int sysctrl_getsystem_freq(void);
extern enum power_state compute_power_state(struct sysctrl_soft_state *, int);
extern struct dvd_end;
extern void sysctrl_getsystem_divisor(struct sysctrl_soft_state *,
					struct dvd_end *);
extern enum temp_state fhc_env_temp_state(int);
extern int sysctrl_hotplug_disabled;
extern int enable_dynamic_reconfiguration;
extern int intr_policy;

int enable_redist = 1;

static int sysc_attach_branch(dev_info_t *, dnode_t, void *);
static int sysc_detach_branch(dev_info_t *, void *);
static void sysc_dr_err_decode(sysc_dr_handle_t *, dev_info_t *, int);

static void ddi_walk_devs_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);
static int i_ddi_walk_devs_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);
static int i_ddi_while_sibling_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);

static u_int
sysc_policy_enough_cooling(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int ps_mutex_is_held);
static u_int
sysc_policy_enough_precharge(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat);
static u_int
sysc_policy_enough_power(struct sysctrl_soft_state *softsp,
	int plus_load, u_int ps_mutex_is_held);
static u_int
sysc_policy_hardware_compatible(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, sysc_cfga_pkt_t *pkt);
static void sysc_policy_empty_condition(
	struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int failure,
	u_int ps_mutex_is_held);
static void sysc_policy_disconnected_condition(
	struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int failure,
	u_int ps_mutex_is_held);
static void sysc_policy_connected_condition(struct sysctrl_soft_state *softsp,
		sysc_cfga_stat_t *sysc_stat,
		u_int ps_mutex_is_held);
static void sysc_policy_set_condition(void *sp, sysc_cfga_stat_t *sysc_stat,
				u_int ps_mutex_is_held);
static void sysc_policy_audit_messages(sysc_audit_evt_t event,
		sysc_cfga_stat_t *sysc_stat);

static void sysctrl_post_config_change(struct sysctrl_soft_state *softsp);
static int sysc_bd_connect(int, sysc_cfga_pkt_t *);
static int sysc_bd_disconnect(int, sysc_cfga_pkt_t *);
static int sysc_bd_configure(int, sysc_cfga_pkt_t *);
static int sysc_bd_unconfigure(int, sysc_cfga_pkt_t *);

static int sysc_dr_init(sysc_dr_handle_t *handle, int board);
static void sysc_dr_uninit(sysc_dr_handle_t *handle);
static int sysc_dr_attach(sysc_dr_handle_t *handle);
static int sysc_dr_detach(sysc_dr_handle_t *handle);

static int find_and_setup_cpu(int);

static int sysc_board_connect_supported(enum board_type);

/*
 * This function will basically do a prediction on the power state
 * based on adding one additional load to the equation implemented
 * by the function compute_power_state.
 */
/*ARGSUSED*/
static u_int
sysc_policy_enough_power(struct sysctrl_soft_state *softsp,
	int plus_load, u_int ps_mutex_is_held)
{
	int retval = 0;

	ASSERT(softsp);

	if (!ps_mutex_is_held) {
		mutex_enter(&softsp->ps_fail_lock);
	}

	/*
	 * note that we add one more load
	 * to the equation in compute_power_state
	 * and the answer better be REDUNDANT or
	 * MINIMUM before proceeding.
	 */
	switch (compute_power_state(softsp, plus_load)) {
		case REDUNDANT:
		case MINIMUM:
			retval = 1;
			break;
		case BELOW_MINIMUM:
		default:
			break;
	}

	if (!ps_mutex_is_held) {
		mutex_exit(&softsp->ps_fail_lock);
	}
	return (retval);
}

/*
 * This function gropes through the shadow registers in the sysctrl soft_state
 * for the core power supply status, since fan status for them are ORed into
 * the same status bit, and all other remaining fans.
 */
static u_int
sysc_policy_enough_cooling(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int ps_mutex_is_held)
{
	int	retval = 0;

	if (!ps_mutex_is_held) {
		mutex_enter(&softsp->ps_fail_lock);
	}

	/*
	 * check the power supply in the slot in question
	 * for fans then check all the common fans.
	 */
	retval = ((softsp->ps_stats[FHC_BOARD2PS(sysc_stat->board)].pshadow ==
			PRES_IN) &&
		(softsp->ps_stats[FHC_BOARD2PS(sysc_stat->board)].dcshadow ==
			PS_OK));
	if (!ps_mutex_is_held) {
		mutex_exit(&softsp->ps_fail_lock);
	}
	return (retval);
}

/*
 * This function will check all precharge voltage status.
 */
/*ARGSUSED*/
static u_int
sysc_policy_enough_precharge(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat)
{
	int	retval = 0;
	int	ppsval = 0;

	mutex_enter(&softsp->ps_fail_lock);

		/*
		 *	note that we always have to explictly check
		 *	the peripheral power supply for precharge since it
		 *	supplies all of the precharge voltages.
		 */
	ppsval = (softsp->ps_stats[SYS_PPS0_INDEX].pshadow == PRES_IN) &&
		(softsp->ps_stats[SYS_PPS0_INDEX].dcshadow == PS_OK);

		/*
		 *	check all the precharge status
		 */
	retval = ((softsp->ps_stats[SYS_V3_PCH_INDEX].pshadow == PRES_IN) &&
		(softsp->ps_stats[SYS_V3_PCH_INDEX].dcshadow == PS_OK) &&
		(softsp->ps_stats[SYS_V5_PCH_INDEX].pshadow == PRES_IN) &&
		(softsp->ps_stats[SYS_V5_PCH_INDEX].dcshadow == PS_OK));

	mutex_exit(&softsp->ps_fail_lock);
	return (retval&&ppsval);
}

static int Fsys;

/*
 * This function should only be called once as we may
 * zero the clock board registers to indicate a configuration change.
 * The code to calculate the bus frequency has been removed and we
 * read the eeprom property instead. Another static Fmod (module
 * frequency may be needed later but so far it is commented out.
 */
void
set_clockbrd_info(void)
{
	uint_t clock_freq = 0;

	dnode_t root = prom_nextnode((dnode_t)0);
	(void) prom_getprop(root, "clock-frequency", (caddr_t)&clock_freq);
	Fsys = clock_freq / 1000000;
}

#define	abs(x)	((x) < 0 ? -(x) : (x))

/*ARGSUSED*/
static u_int
sysc_policy_hardware_compatible(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, sysc_cfga_pkt_t *pkt)
{
	int status;

	ASSERT(Fsys > 0);

	/* Only allow DR operations on supported hardware */
	switch (sysc_stat->type) {
	case CPU_BOARD: {
#ifdef RFE_4174486
		int i;
		int cpu_freq;
		int sram_mode;

		ASSERT(Fmod > 0);

		cpu_freq = CPU->cpu_type_info.pi_clock;

		if (abs(cpu_freq - Fmod) < 8)
			sram_mode = 1;
		else
			sram_mode = 2;

		status = TRUE;
		for (i = 0; i < 2; i++) {
			/*
			 * XXX: Add jtag code which rescans disabled boards.
			 * For the time being disabled boards are not
			 * checked for compatiblity when cpu_speed is 0.
			 */
			if (sysc_stat->bd.cpu[i].cpu_speed == 0)
				continue;

			if (sysc_stat->bd.cpu[i].cpu_speed < cpu_freq) {
				cmn_err(CE_WARN, "board %d, cpu module %c "
					"rated at %d Mhz, system freq %d Mhz",
					sysc_stat->board, (i == 0) ? 'A' : 'B',
					sysc_stat->bd.cpu[i].cpu_speed,
					cpu_freq);
				status = FALSE;
			}

			if (sram_mode != sysc_stat->bd.cpu[i].cpu_sram_mode) {
				cmn_err(CE_WARN, "board %d, cpu module %c "
					"incompatible sram mode of %dx, "
					"system is %dx", sysc_stat->board,
					(i == 0) ? 'A' : 'B',
					sysc_stat->bd.cpu[i].cpu_sram_mode,
					sram_mode);
				status = FALSE;
			}
		}
		break;
#endif /* RFE_4174486 */
	}

	case MEM_BOARD:
	case IO_2SBUS_BOARD:
	case IO_SBUS_FFB_BOARD:
	case IO_PCI_BOARD:
	case IO_2SBUS_SOCPLUS_BOARD:
	case IO_SBUS_FFB_SOCPLUS_BOARD:
		status = TRUE;
		break;

	case CLOCK_BOARD:
	case DISK_BOARD:
	default:
		status = FALSE;		/* default is not supported */
		break;
	}

	if (status == FALSE)
		return (status);

	/* Check for Sunfire boards in a Sunfire+ system */
	if (status == TRUE && Fsys > 84 && !fhc_bd_is_plus(sysc_stat->board)) {
		(void) snprintf(pkt->errbuf, SYSC_OUTPUT_LEN,
		    "not 100 MHz capable   ");
		cmn_err(CE_WARN, "board %d, is not capable of running at "
		    "current system clock (%dMhz)", sysc_stat->board, Fsys);

		status = FALSE;
	}

	return (status);
}

/*
 * This function is called to check the policy for a request to transition
 * to the connected state from the disconnected state. The generic policy
 * is to do sanity checks again before going live.
 */
/*ARGSUSED*/
int
sysc_policy_connect(struct sysctrl_soft_state *softsp,
		sysc_cfga_pkt_t *pkt, sysc_cfga_stat_t *sysc_stat)
{
	int retval;

	ASSERT(fhc_bdlist_locked());

	DPRINTF(SYSC_DEBUG, ("Previous RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Previous OState: %d\n", sysc_stat->ostate));

	switch (sysc_stat->rstate) {
	case SYSC_CFGA_RSTATE_DISCONNECTED:
		/*
		 * Safety policy: only allow connect if board is UNKNOWN cond.
		 * cold start board will be demoted to UNKNOWN cond when
		 * disconnected
		 */
		if (sysc_stat->condition != SYSC_CFGA_COND_UNKNOWN) {
			SYSC_ERR_SET(pkt, SYSC_ERR_COND);
			return (EINVAL);
		}

		if (!enable_dynamic_reconfiguration) {
			SYSC_ERR_SET(pkt, SYSC_ERR_NON_DR_PROM);
			return (ENOTSUP);
		}

		if (sysctrl_hotplug_disabled) {
			SYSC_ERR_SET(pkt, SYSC_ERR_HOTPLUG);
			return (ENOTSUP);
		}

		/* Check PROM support. */
		if (!sysc_board_connect_supported(sysc_stat->type)) {
			cmn_err(CE_WARN, "%s board %d connect"
			    " is not supported by firmware.",
			    fhc_bd_typestr(sysc_stat->type), sysc_stat->board);
			SYSC_ERR_SET(pkt, SYSC_ERR_HW_COMPAT);
			return (ENOTSUP);
		}

		if (!sysc_policy_enough_power(softsp, TRUE, FALSE)) {
			SYSC_ERR_SET(pkt, SYSC_ERR_POWER);
			return (EAGAIN);
		}

		if (!sysc_policy_enough_precharge(softsp, sysc_stat)) {
			SYSC_ERR_SET(pkt, SYSC_ERR_PRECHARGE);
			return (EAGAIN);
		}

		if (!sysc_policy_enough_cooling(softsp, sysc_stat, FALSE)) {
			SYSC_ERR_SET(pkt, SYSC_ERR_COOLING);
			return (EAGAIN);
		}

		if (!sysc_policy_hardware_compatible(softsp, sysc_stat, pkt)) {
			SYSC_ERR_SET(pkt, SYSC_ERR_HW_COMPAT);
			return (ENOTSUP);
		}
		sysc_policy_audit_messages(SYSC_AUDIT_RSTATE_CONNECT,
			sysc_stat);

		retval = sysc_bd_connect(sysc_stat->board, pkt);
		if (!retval) {
			sysc_stat->rstate = SYSC_CFGA_RSTATE_CONNECTED;
			sysc_policy_connected_condition(softsp,
				sysc_stat, FALSE);
			sysc_policy_audit_messages(SYSC_AUDIT_RSTATE_SUCCEEDED,
				sysc_stat);
		} else {
			u_int prom_failure;

			prom_failure = (retval == EIO &&
			    pkt->cmd_cfga.errtype == SYSC_ERR_PROM) ?
			    TRUE : FALSE;
			sysc_policy_disconnected_condition(softsp,
				sysc_stat, prom_failure, FALSE);
			sysc_policy_audit_messages(
				SYSC_AUDIT_RSTATE_CONNECT_FAILED,
				sysc_stat);
		}
		break;
	case SYSC_CFGA_RSTATE_EMPTY:
	case SYSC_CFGA_RSTATE_CONNECTED:
	default:
		SYSC_ERR_SET(pkt, SYSC_ERR_RSTATE);
		retval = EINVAL;
		break;
	}

	DPRINTF(SYSC_DEBUG, ("Current RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Current OState: %d\n", sysc_stat->ostate));
	DPRINTF(SYSC_DEBUG, ("Current Condition: %d\n", sysc_stat->condition));

	return (retval);
}

/*
 * This function is called to check the policy for a request to transition
 * to the disconnected state from the connnected/unconfigured state only.
 * All other requests are invalid.
 */
/*ARGSUSED*/
int
sysc_policy_disconnect(struct sysctrl_soft_state *softsp,
			sysc_cfga_pkt_t *pkt, sysc_cfga_stat_t *sysc_stat)
{
	int retval;

	ASSERT(fhc_bdlist_locked());

	DPRINTF(SYSC_DEBUG, ("Previous RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Previous OState: %d\n", sysc_stat->ostate));

	switch (sysc_stat->rstate) {
	case SYSC_CFGA_RSTATE_CONNECTED:
		switch (sysc_stat->ostate) {
		case SYSC_CFGA_OSTATE_UNCONFIGURED:
			if (!enable_dynamic_reconfiguration) {
				SYSC_ERR_SET(pkt, SYSC_ERR_NON_DR_PROM);
				return (ENOTSUP);
			}

			/* Check PROM support. */
			if (!sysc_board_connect_supported(sysc_stat->type)) {
				cmn_err(CE_WARN, "%s board %d disconnect"
				    " is not supported by firmware.",
				    fhc_bd_typestr(sysc_stat->type),
				    sysc_stat->board);
				SYSC_ERR_SET(pkt, SYSC_ERR_HW_COMPAT);
				return (ENOTSUP);
			}

			if (!sysc_policy_hardware_compatible(softsp,
				sysc_stat, pkt)) {
				cmn_err(CE_WARN, "%s board %d disconnect"
				" is not yet supported.",
				fhc_bd_typestr(sysc_stat->type),
					sysc_stat->board);
				SYSC_ERR_SET(pkt, SYSC_ERR_HW_COMPAT);
				return (ENOTSUP);
			}

			if (fhc_bd_is_jtag_master(sysc_stat->board)) {
				sysc_policy_update(softsp, sysc_stat,
					SYSC_EVT_BD_CORE_RESOURCE_DISCONNECT);
				SYSC_ERR_SET(pkt, SYSC_ERR_CORE_RESOURCE);
				return (EINVAL);
			}

			sysc_policy_audit_messages(SYSC_AUDIT_RSTATE_DISCONNECT,
				sysc_stat);

			retval = sysc_bd_disconnect(sysc_stat->board, pkt);
			if (!retval) {
				sysc_stat->rstate =
					SYSC_CFGA_RSTATE_DISCONNECTED;
				DPRINTF(SYSCTRL_ATTACH_DEBUG,
				    ("disconnect starting bd_remove_poll()"));
				bd_remove_poll(softsp);
				sysc_policy_disconnected_condition(
					softsp,
					sysc_stat, FALSE, FALSE);
				sysc_policy_audit_messages(
					SYSC_AUDIT_RSTATE_SUCCEEDED,
					sysc_stat);
				cmn_err(CE_NOTE,
					"board %d is ready to remove",
					sysc_stat->board);
			} else {
				sysc_policy_connected_condition(
					softsp, sysc_stat, FALSE);
				sysc_policy_audit_messages(
					SYSC_AUDIT_RSTATE_DISCONNECT_FAILED,
					sysc_stat);
			}
			break;
		case SYSC_CFGA_OSTATE_CONFIGURED:
		default:
			SYSC_ERR_SET(pkt, SYSC_ERR_OSTATE);
			retval = EINVAL;
			break;
		}
		break;
	case SYSC_CFGA_RSTATE_EMPTY:
	case SYSC_CFGA_RSTATE_DISCONNECTED:
	default:
		SYSC_ERR_SET(pkt, SYSC_ERR_RSTATE);
		retval = EINVAL;
		break;
	}

	DPRINTF(SYSC_DEBUG, ("Current RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Current OState: %d\n", sysc_stat->ostate));
	DPRINTF(SYSC_DEBUG, ("Current Condition: %d\n", sysc_stat->condition));

	return (retval);
}

/*
 * This function is called to check the policy for a request to transition
 * from the connected/configured state to the connected/unconfigured state only.
 * All other requests are invalid.
 */
/*ARGSUSED*/
int
sysc_policy_unconfigure(struct sysctrl_soft_state *softsp,
			sysc_cfga_pkt_t *pkt, sysc_cfga_stat_t *sysc_stat)
{
	int retval;

	ASSERT(fhc_bdlist_locked());

	DPRINTF(SYSC_DEBUG, ("Previous RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Previous OState: %d\n", sysc_stat->ostate));

	switch (sysc_stat->ostate) {
	case SYSC_CFGA_OSTATE_CONFIGURED:
		if (!enable_dynamic_reconfiguration) {
			SYSC_ERR_SET(pkt, SYSC_ERR_NON_DR_PROM);
			return (ENOTSUP);
		}

		if (!sysc_policy_hardware_compatible(softsp, sysc_stat, pkt)) {
			cmn_err(CE_WARN, "%s board %d unconfigure"
			" is not yet supported.",
			fhc_bd_typestr(sysc_stat->type), sysc_stat->board);
			SYSC_ERR_SET(pkt, SYSC_ERR_HW_COMPAT);
			return (ENOTSUP);
		}

		sysc_policy_audit_messages(SYSC_AUDIT_OSTATE_UNCONFIGURE,
			sysc_stat);

		retval = sysc_bd_unconfigure(sysc_stat->board, pkt);
		if (!retval) {
		    sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		    sysc_policy_audit_messages(
			SYSC_AUDIT_OSTATE_SUCCEEDED,
			sysc_stat);
		} else {
		    sysc_policy_audit_messages(
			SYSC_AUDIT_OSTATE_UNCONFIGURE_FAILED,
			sysc_stat);
		}
		sysc_policy_connected_condition(softsp, sysc_stat, FALSE);
		break;
	case SYSC_CFGA_OSTATE_UNCONFIGURED:
	default:
		SYSC_ERR_SET(pkt, SYSC_ERR_OSTATE);
		retval = EINVAL;
		break;
	}

	DPRINTF(SYSC_DEBUG, ("Current RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Current OState: %d\n", sysc_stat->ostate));
	DPRINTF(SYSC_DEBUG, ("Current Condition: %d\n", sysc_stat->condition));

	return (retval);
}

/*
 * This function is called to check the policy for a requested transition
 * from either the connected/unconfigured state or the connected/configured
 * state to the connected/configured state.  The redundant state transition
 * is permitted for partially configured set of devices.  Basically, we
 * retry the configure.
 */
/*ARGSUSED*/
int
sysc_policy_configure(struct sysctrl_soft_state *softsp,
			sysc_cfga_pkt_t *pkt, sysc_cfga_stat_t *sysc_stat)
{
	int retval;

	ASSERT(fhc_bdlist_locked());

	DPRINTF(SYSC_DEBUG, ("Previous RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Previous OState: %d\n", sysc_stat->ostate));

	switch (sysc_stat->rstate) {
	case SYSC_CFGA_RSTATE_CONNECTED:
		switch (sysc_stat->ostate) {
		case SYSC_CFGA_OSTATE_UNCONFIGURED:
			if (sysc_stat->condition != SYSC_CFGA_COND_OK) {
				SYSC_ERR_SET(pkt, SYSC_ERR_COND);
				return (EINVAL);
			}

			sysc_policy_audit_messages(SYSC_AUDIT_OSTATE_CONFIGURE,
				sysc_stat);
			retval = sysc_bd_configure(sysc_stat->board, pkt);
			sysc_stat->ostate = SYSC_CFGA_OSTATE_CONFIGURED;
			sysc_policy_connected_condition(softsp,
				sysc_stat, FALSE);
			if (!retval) {
				sysc_policy_audit_messages(
					SYSC_AUDIT_OSTATE_SUCCEEDED,
					sysc_stat);
			} else {
				sysc_policy_audit_messages(
					SYSC_AUDIT_OSTATE_CONFIGURE_FAILED,
					sysc_stat);
			}
			break;
		case SYSC_CFGA_OSTATE_CONFIGURED:
			SYSC_ERR_SET(pkt, SYSC_ERR_OSTATE);
			retval = ENOTSUP;
			break;
		default:
			SYSC_ERR_SET(pkt, SYSC_ERR_OSTATE);
			retval = EINVAL;
			break;
		}
		break;
	case SYSC_CFGA_RSTATE_EMPTY:
	case SYSC_CFGA_RSTATE_DISCONNECTED:
	default:
		SYSC_ERR_SET(pkt, SYSC_ERR_RSTATE);
		retval = EINVAL;
		break;
	}


	DPRINTF(SYSC_DEBUG, ("Current RState: %d\n", sysc_stat->rstate));
	DPRINTF(SYSC_DEBUG, ("Current OState: %d\n", sysc_stat->ostate));
	DPRINTF(SYSC_DEBUG, ("Current Condition: %d\n", sysc_stat->condition));

	return (retval);
}

/*ARGSUSED*/
static void
sysc_policy_empty_condition(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int failure,
	u_int ps_mutex_is_held)
{
	ASSERT(fhc_bdlist_locked());

	switch (sysc_stat->condition) {
	case SYSC_CFGA_COND_UNKNOWN:
	case SYSC_CFGA_COND_OK:
	case SYSC_CFGA_COND_FAILING:
	case SYSC_CFGA_COND_FAILED:
	/* nothing in the slot so just check power supplies */
	case SYSC_CFGA_COND_UNUSABLE:
	    if (sysc_policy_enough_cooling(softsp, sysc_stat,
		ps_mutex_is_held) &&
		sysc_policy_enough_power(softsp, FALSE,
		ps_mutex_is_held)) {
		sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;
	    } else {
		sysc_stat->condition = SYSC_CFGA_COND_UNUSABLE;
	    }
	    sysc_stat->last_change = hrestime.tv_sec;
	    break;
	default:
	    ASSERT(FALSE);
	    break;
	}
}
/*ARGSUSED*/
static void
sysc_policy_disconnected_condition(struct sysctrl_soft_state *softsp,
	sysc_cfga_stat_t *sysc_stat, u_int failure,
	u_int ps_mutex_is_held)
{
	ASSERT(fhc_bdlist_locked());

	if (failure) {
		sysc_stat->condition = SYSC_CFGA_COND_FAILED;
		sysc_stat->last_change = hrestime.tv_sec;
		return;
	}
	switch (sysc_stat->condition) {
	/*
	 * if unknown, we have come from hotplug case so do a quick
	 * reevaluation.
	 */
	case SYSC_CFGA_COND_UNKNOWN:
	/*
	 * if ok, we have come from connected to disconnected and we stay
	 * ok until removed or reevaluate when reconnect.  We might have
	 * experienced a ps fail so reevaluate the condition.
	 */
	case SYSC_CFGA_COND_OK:
	/*
	 * if unsuable, either power supply was missing or
	 * hardware was not compatible.  Check to see if
	 * this is still true.
	 */
	case SYSC_CFGA_COND_UNUSABLE:
	/*
	 * failing must transition in the disconnected state
	 * to either unsusable or unknown.  We may have come here
	 * from cfgadm -f -c disconnect after a power supply failure
	 * in an attempt to protect the board.
	 */
	case SYSC_CFGA_COND_FAILING:
	    if (sysc_policy_enough_cooling(softsp, sysc_stat,
		ps_mutex_is_held) &&
		sysc_policy_enough_power(softsp, FALSE,
		ps_mutex_is_held)) {
		sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;
	    } else {
		sysc_stat->condition = SYSC_CFGA_COND_UNUSABLE;
	    }
	    sysc_stat->last_change = hrestime.tv_sec;
	    break;
	/*
	 * if failed, we have failed POST and must stay in this
	 * condition until the board has been removed
	 * before ever coming back into another condition
	 */
	case SYSC_CFGA_COND_FAILED:
		break;
	default:
		ASSERT(FALSE);
		break;
	}
}

/*ARGSUSED*/
static void
sysc_policy_connected_condition(struct sysctrl_soft_state *softsp,
		sysc_cfga_stat_t *sysc_stat,
		u_int ps_mutex_is_held)
{
	ASSERT(fhc_bdlist_locked());

	switch (sysc_stat->condition) {
	case SYSC_CFGA_COND_UNKNOWN:
	case SYSC_CFGA_COND_OK:
	case SYSC_CFGA_COND_FAILING:
	case SYSC_CFGA_COND_UNUSABLE:
	    if (sysc_policy_enough_cooling(softsp, sysc_stat,
		ps_mutex_is_held) &&
		sysc_policy_enough_power(softsp, FALSE,
		ps_mutex_is_held) &&
		(fhc_env_temp_state(sysc_stat->board) == TEMP_OK)) {
			sysc_stat->condition = SYSC_CFGA_COND_OK;
	    } else {
			sysc_stat->condition = SYSC_CFGA_COND_FAILING;
	    }
	    sysc_stat->last_change = hrestime.tv_sec;
	    break;
	case SYSC_CFGA_COND_FAILED:
	    break;
	default:
	    ASSERT(FALSE);
	    break;
	}
}

static void
sysc_policy_set_condition(void *sp, sysc_cfga_stat_t *sysc_stat,
		u_int ps_mutex_is_held)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *)sp;

	ASSERT(fhc_bdlist_locked());

	switch (sysc_stat->rstate) {
	case SYSC_CFGA_RSTATE_EMPTY:
		sysc_policy_empty_condition(softsp, sysc_stat,
			FALSE, ps_mutex_is_held);
		break;
	case SYSC_CFGA_RSTATE_DISCONNECTED:
		sysc_policy_disconnected_condition(softsp, sysc_stat,
			FALSE, ps_mutex_is_held);
		break;
	case SYSC_CFGA_RSTATE_CONNECTED:
		sysc_policy_connected_condition(softsp, sysc_stat,
			ps_mutex_is_held);
		break;
	default:
		ASSERT(FALSE);
		break;
	}
}

void
sysc_policy_update(void *softsp, sysc_cfga_stat_t *sysc_stat,
	sysc_evt_t event)
{
	fhc_bd_t *list;

	ASSERT(event == SYSC_EVT_BD_HP_DISABLED || fhc_bdlist_locked());

	switch (event) {
	case SYSC_EVT_BD_EMPTY:
		sysc_stat->rstate = SYSC_CFGA_RSTATE_EMPTY;
		sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;
		sysc_policy_empty_condition(softsp, sysc_stat, FALSE, FALSE);
		break;
	case SYSC_EVT_BD_PRESENT:
		if (sysc_stat->type == DISK_BOARD) {
			sysc_stat->rstate = SYSC_CFGA_RSTATE_DISCONNECTED;
			sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
			sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;
		} else {
			sysc_stat->rstate = SYSC_CFGA_RSTATE_CONNECTED;
			sysc_stat->ostate = SYSC_CFGA_OSTATE_CONFIGURED;
			sysc_stat->condition = SYSC_CFGA_COND_OK;
		}
		sysc_stat->last_change = hrestime.tv_sec;
		break;
	case SYSC_EVT_BD_DISABLED:
		sysc_stat->rstate = SYSC_CFGA_RSTATE_DISCONNECTED;
		sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;
		sysc_policy_disconnected_condition(softsp,
			sysc_stat, FALSE, FALSE);
		cmn_err(CE_NOTE,
			"disabled %s board in slot %d",
			fhc_bd_typestr(sysc_stat->type),
			sysc_stat->board);
		break;
	case SYSC_EVT_BD_FAILED:
		sysc_stat->rstate = SYSC_CFGA_RSTATE_DISCONNECTED;
		sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		sysc_stat->condition = SYSC_CFGA_COND_UNUSABLE;
		sysc_policy_disconnected_condition(softsp, sysc_stat,
			TRUE, FALSE);
		cmn_err(CE_NOTE,
			"failed %s board in slot %d",
			fhc_bd_typestr(sysc_stat->type),
			sysc_stat->board);
		break;
	case SYSC_EVT_BD_OVERTEMP:
	case SYSC_EVT_BD_TEMP_OK:
		sysc_policy_set_condition((void *)softsp, sysc_stat, FALSE);
		break;
	case SYSC_EVT_BD_PS_CHANGE:
		for (list = fhc_bd_first(); list; list = fhc_bd_next(list)) {
			sysc_stat = &(list->sc);
			sysc_policy_set_condition((void *)softsp,
				sysc_stat, TRUE);
		}
		break;
	case SYSC_EVT_BD_INS_FAILED:
		cmn_err(CE_WARN, "powerdown of board %d failed",
			sysc_stat->board);
		break;
	case SYSC_EVT_BD_INSERTED:
		sysc_stat->rstate = SYSC_CFGA_RSTATE_DISCONNECTED;
		sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		sysctrl_post_config_change(softsp);
		sysc_policy_disconnected_condition(softsp,
			sysc_stat, FALSE, FALSE);
		cmn_err(CE_NOTE, "%s board has been inserted into slot %d",
			fhc_bd_typestr(sysc_stat->type), sysc_stat->board);
		cmn_err(CE_NOTE,
			"board %d can be removed", sysc_stat->board);
		break;
	case SYSC_EVT_BD_REMOVED:
		sysc_stat->rstate = SYSC_CFGA_RSTATE_EMPTY;
		sysc_stat->ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		sysc_stat->condition = SYSC_CFGA_COND_UNKNOWN;

		/* now it is ok to free the ac pa memory database */
		fhc_del_memloc(sysc_stat->board);

		/* reinitialize sysc_cfga_stat structure */
		sysc_stat->type = UNKNOWN_BOARD;
		sysc_stat->fhc_compid = 0;
		sysc_stat->ac_compid = 0;
		(void) bzero(&(sysc_stat->prom_rev),
			sizeof (sysc_stat->prom_rev));
		(void) bzero(&(sysc_stat->bd),
			sizeof (union bd_un));
		sysc_stat->no_detach = sysc_stat->plus_board = 0;
		sysc_policy_disconnected_condition(softsp,
			sysc_stat, FALSE, FALSE);
		cmn_err(CE_NOTE, "board %d has been removed",
			sysc_stat->board);
		break;
	case SYSC_EVT_BD_HP_DISABLED:
		cmn_err(CE_NOTE, "Hot Plug not supported in this system");
		break;
	case SYSC_EVT_BD_CORE_RESOURCE_DISCONNECT:
		cmn_err(CE_WARN, "board %d cannot be disconnected because it"
			" is a core system resource", sysc_stat->board);
		break;
	default:
		ASSERT(FALSE);
		break;
	}

}

/*
 * signal to POST that the system has been reconfigured and that
 * the system configuration status information should be invalidated
 * the next time through POST
 */
static void
sysctrl_post_config_change(struct sysctrl_soft_state *softsp)
{
	/*
	 * We are heading into a configuration change!
	 * Tell post to invalidate its notion of the system configuration.
	 * This is done by clearing the clock registers...
	 */
	*softsp->clk_freq1 = 0;
	*softsp->clk_freq2 &=
		~(CLOCK_FREQ_8 | CLOCK_DIV_1 | CLOCK_RANGE | CLOCK_DIV_0);
}

static int
sysc_bd_connect(int board, sysc_cfga_pkt_t *pkt)
{
	int error = 0;
	fhc_bd_t *bdp;
	sysc_dr_handle_t *sh;
	uint_t	circular_count;
	uint64_t mempa;
	int del_kstat = 0;

	ASSERT(fhc_bd_busy(board));

	bdp = fhc_bd(board);

	/* find gap for largest supported simm in advance */
#define	MAX_BANK_SIZE_MB	(2 * 1024)
#define	BANKS_PER_BOARD		2
	mempa = fhc_find_memloc_gap(BANKS_PER_BOARD * MAX_BANK_SIZE_MB);

	fhc_bdlist_unlock();

	/* TODO: Is mempa vulnerable to re-use here? */

	sysctrl_suspend_prepare();

	i_ndi_block_device_tree_changes(&circular_count);

	if ((error = sysctrl_suspend(pkt)) == DDI_SUCCESS) {
		/* ASSERT(jtag not held) */
		error = prom_sunfire_attach_board((u_int)board);
		if (error) {
			error = EIO;
			SYSC_ERR_SET(pkt, SYSC_ERR_PROM);
		} else {
			/* attempt to program the memory while frozen */
			fhc_program_memory(board, mempa);
		}
		sysctrl_resume(pkt);
	}

	if (error) {
		goto done;
	}

	/*
	 * Must not delete kstat used by prtdiag until the PROM
	 * has successfully connected to board.
	 */
	del_kstat = 1;

	sh = &bdp->sh[SYSC_DR_HANDLE_FHC];
	sh->flags |= SYSC_DR_FHC;
	sh->errstr = pkt->errbuf;

	error = sysc_dr_init(sh, board);
	if (error) {
		SYSC_ERR_SET(pkt, SYSC_ERR_DR_INIT);
		goto done;
	}

	error = sysc_dr_attach(sh);
	if (error)
		SYSC_ERR_SET(pkt, SYSC_ERR_NDI_ATTACH);

	sysc_dr_uninit(sh);

	if (enable_redist) {
		mutex_enter(&cpu_lock);
		intr_redist_all_cpus(intr_policy);
		mutex_exit(&cpu_lock);
	}
done:
	i_ndi_allow_device_tree_changes(circular_count);

	if (del_kstat && bdp->ksp) {
		kstat_delete(bdp->ksp);
		bdp->ksp = NULL;
	}

	(void) fhc_bdlist_lock(-1);

	return (error);
}

static int
sysc_bd_disconnect(int board, sysc_cfga_pkt_t *pkt)
{
	int error;
	fhc_bd_t *bdp;
	sysc_dr_handle_t *sh;
	struct jt_mstr *jtm;
	void fhc_bd_ks_alloc(fhc_bd_t *);
	uint_t circular_count;
	cpuset_t xcset;

	ASSERT(fhc_bd_busy(board));
	ASSERT(!fhc_bd_is_jtag_master(board));

	CPUSET_ALL(xcset);

	bdp = fhc_bd(board);

	bdp->flags |= BDF_DETACH;

	fhc_bdlist_unlock();

	sh = &bdp->sh[SYSC_DR_HANDLE_FHC];
	sh->errstr = pkt->errbuf;

	i_ndi_block_device_tree_changes(&circular_count);

	if (sh->dev_list == NULL) {
		sh->flags |= SYSC_DR_FHC;
		error = sysc_dr_init(sh, board);
		if (error) {
			SYSC_ERR_SET(pkt, SYSC_ERR_DR_INIT);
			goto done;
		}
	}
	sh->flags |= SYSC_DR_FORCE | SYSC_DR_REMOVE;
	error = sysc_dr_detach(sh);
	sh->flags &= ~SYSC_DR_REMOVE;

	sysc_dr_uninit(sh);
	if (error) {
		SYSC_ERR_SET(pkt, SYSC_ERR_NDI_DETACH);
		goto done;
	}

	(void) fhc_bdlist_lock(-1);

#ifdef DEBUG
	/* it is important to have fhc_bdlist_lock() earlier */
	if (sysctrl_enable_regdump)
		precache_regdump(board);
#endif /* DEBUG */

	jtm = jtag_master_lock();
	xc_attention(xcset);

#ifdef DEBUG
	if (sysctrl_enable_regdump)
		boardstat_regdump();
#endif /* DEBUG */

	error = prom_sunfire_detach_board((u_int)board);

#ifdef DEBUG
	if (sysctrl_enable_regdump)
		display_regdump();
#endif /* DEBUG */

	xc_dismissed(xcset);
	jtag_master_unlock(jtm);
	fhc_bdlist_unlock();

	if (error) {
		error = EIO;
		SYSC_ERR_SET(pkt, SYSC_ERR_PROM);
		goto done;
	}

	if (enable_redist) {
		mutex_enter(&cpu_lock);
		intr_redist_all_cpus(intr_policy);
		mutex_exit(&cpu_lock);
	}

	fhc_bd_ks_alloc(bdp);
done:
	i_ndi_allow_device_tree_changes(circular_count);

	(void) fhc_bdlist_lock(-1);

	return (error);
}

static int
sysc_bd_configure(int board, sysc_cfga_pkt_t *pkt)
{
	int error = 0;
	fhc_bd_t *bdp;
	sysc_dr_handle_t *sh;
	uint_t circular_count;

	ASSERT(fhc_bd_busy(board));

	bdp = fhc_bd(board);

	fhc_bdlist_unlock();

	if (bdp->sc.type == CPU_BOARD) {
		mutex_enter(&cpu_lock);

		error = find_and_setup_cpu(FHC_BOARD2CPU_A(board));
		if ((error == 0) || (error == ENODEV)) {
			int retval_b;

			retval_b = find_and_setup_cpu(FHC_BOARD2CPU_B(board));
			if (retval_b != ENODEV)
				error = retval_b;
		}

		mutex_exit(&cpu_lock);

		(void) fhc_bdlist_lock(-1);

		return (error);
	}

	sh = &bdp->sh[SYSC_DR_HANDLE_DEVS];
	sh->errstr = pkt->errbuf;

	i_ndi_block_device_tree_changes(&circular_count);

	if (sh->dev_list == NULL) {
		error = sysc_dr_init(sh, board);
	}

	if (error) {
		SYSC_ERR_SET(pkt, SYSC_ERR_DR_INIT);
		goto done;
	}

	sh->flags |= SYSC_DR_DEVS;
	error = sysc_dr_attach(sh);
	if (error) {
		SYSC_ERR_SET(pkt, SYSC_ERR_NDI_ATTACH);
		sysc_dr_uninit(sh);
		goto done;
	}

	sysc_dr_uninit(sh);

	if (enable_redist) {
		mutex_enter(&cpu_lock);
		intr_redist_all_cpus(intr_policy);
		mutex_exit(&cpu_lock);
	}
done:
	i_ndi_allow_device_tree_changes(circular_count);

	(void) fhc_bdlist_lock(-1);

	return (error);
}

static int
sysc_bd_unconfigure(int board, sysc_cfga_pkt_t *pkt)
{
	int error;
	fhc_bd_t *bdp;
	sysc_dr_handle_t *sh;
	uint_t circular_count;

	ASSERT(fhc_bdlist_locked());
	ASSERT(fhc_bd_busy(board));

	bdp = fhc_bd(board);

	if (bdp->sc.type == CPU_BOARD || bdp->sc.type == MEM_BOARD) {
		struct ac_soft_state *acsp;

		/*
		 * Check that any memory on board is not in use.
		 * This must be done while the board list lock is held
		 * as memory state can change while fhc_bd_busy() is true
		 * even though a memory operation cannot be started
		 * if fhc_bd_busy() is true.
		 */
		if ((acsp = (struct ac_soft_state *)bdp->ac_softsp) != NULL) {
			if (acsp->bank[Bank0].busy != 0 ||
			    acsp->bank[Bank0].ostate ==
			    SYSC_CFGA_OSTATE_CONFIGURED) {
				cmn_err(CE_WARN, "memory bank %d in "
				    "slot %d is in use.", Bank0, board);
				(void) snprintf(pkt->errbuf,
				    SYSC_OUTPUT_LEN,
				    "memory bank %d in use",
				    Bank0);
				return (EBUSY);
			}

			if (acsp->bank[Bank1].busy != 0 ||
			    acsp->bank[Bank1].ostate ==
			    SYSC_CFGA_OSTATE_CONFIGURED) {
				cmn_err(CE_WARN, "memory bank %d in "
				    "slot %d is in use.", Bank1, board);
				(void) snprintf(pkt->errbuf,
				    SYSC_OUTPUT_LEN,
				    "memory bank %d in use",
				    Bank1);
				return (EBUSY);
			}
			/*
			 * Nothing more to do here. The memory interface
			 * will not make any transitions while
			 * fhc_bd_busy() is true. Once the ostate
			 * becomes unconfigured, the memory becomes
			 * invisible.
			 */
		}
		error = 0;
		if (bdp->sc.type == CPU_BOARD) {
			struct cpu *cpua, *cpub;

			fhc_bdlist_unlock();

			mutex_enter(&cpu_lock);	/* protects CPU states */

			error = fhc_board_poweroffcpus(board, pkt->errbuf);

			cpua = cpu_get(FHC_BOARD2CPU_A(board));
			cpub = cpu_get(FHC_BOARD2CPU_B(board));

			if ((error == 0) && (cpua != NULL)) {
				error = cpu_unconfigure(cpua->cpu_id);
				if (error != 0) {
					(void) snprintf(pkt->errbuf,
					    SYSC_OUTPUT_LEN,
					    "processor %d unconfigure failed",
					    cpua->cpu_id);
				}
			}
			if ((error == 0) && (cpub != NULL)) {
				error = cpu_unconfigure(cpub->cpu_id);
				if (error != 0) {
					(void) snprintf(pkt->errbuf,
					    SYSC_OUTPUT_LEN,
					    "processor %d unconfigure failed",
					    cpub->cpu_id);
				}
			}

			mutex_exit(&cpu_lock);

			(void) fhc_bdlist_lock(-1);
		}

		return (error);
	}

	fhc_bdlist_unlock();

	sh = &bdp->sh[SYSC_DR_HANDLE_DEVS];
	sh->errstr = pkt->errbuf;

	i_ndi_block_device_tree_changes(&circular_count);

	if (sh->dev_list == NULL) {
		sh->flags |= SYSC_DR_DEVS;
		error = sysc_dr_init(sh, board);
		if (error) {
			SYSC_ERR_SET(pkt, SYSC_ERR_DR_INIT);
			goto done;
		}
	}
	sh->flags |= SYSC_DR_FORCE | SYSC_DR_REMOVE;
	error = sysc_dr_detach(sh);
	sh->flags &= ~SYSC_DR_REMOVE;
	if (error) {
		SYSC_ERR_SET(pkt, SYSC_ERR_NDI_DETACH);
		sysc_dr_uninit(sh);
		goto done;
	}

	sysc_dr_uninit(sh);

	if (enable_redist) {
		mutex_enter(&cpu_lock);
		intr_redist_all_cpus(intr_policy);
		mutex_exit(&cpu_lock);
	}

done:
	i_ndi_allow_device_tree_changes(circular_count);

	(void) fhc_bdlist_lock(-1);

	return (error);
}

/*
 * Attaching devices on a board.
 */
static int
sysc_dr_attach(sysc_dr_handle_t  *handle)
{
	dnode_t			*nodeid;
	int			i;

	for (i = 0, nodeid = handle->dev_list;
	    i < handle->dev_list_len; i++, nodeid++) {
		(void) sysc_attach_branch(ddi_root_node(),
					*nodeid, (void *)handle);
		if (handle->error) {
			return (handle->error);
		}
	}
	return (handle->error);
}

/*
 * Detaching devices on a board.
 */
static int
sysc_dr_detach(sysc_dr_handle_t  *handle)
{
	dev_info_t	**dip;
	dev_info_t	**dev_list;
	int		dev_list_len = 0;
	dnode_t		*nodeid;
	int		i;
	int		error = 0;

	dev_list = kmem_zalloc(sizeof (dev_info_t *) * SYSC_DR_MAX_NODE,
	    KM_SLEEP);
	if (dev_list == NULL) {
		handle->error = ENOMEM;
		return (handle->error);
	}

	for (i = 0, nodeid = &handle->dev_list[i],
	    dip = dev_list; i < handle->dev_list_len; i++, nodeid++) {
		*dip = e_ddi_nodeid_to_dip(ddi_root_node(), *nodeid);
		if (*dip != NULL) {
			++dip;
			++dev_list_len;
		}
	}

	for (i = dev_list_len, dip = &dev_list[i - 1];
	    i > 0; i--, dip--) {
		ddi_walk_devs_reverse(*dip, sysc_detach_branch,
		    (void *)handle);
		if (handle->error) {
			error = handle->error;
			break;
		}
	}

	kmem_free(dev_list, sizeof (dev_info_t *) * SYSC_DR_MAX_NODE);
	return (error);
}

/*
 * Initialize devices for the state of a board.
 */
static int
sysc_dr_init(sysc_dr_handle_t *handle, int board)
{
	dnode_t			*nodeid;
	static dnode_t		child, firstchild;

	static int		bd_id;
	static char		name[OBP_MAXDRVNAME];
	static int		len;
	int			*regp;



	ASSERT(handle->dev_list_len == 0);

	handle->dev_list = kmem_zalloc(sizeof (dnode_t) * SYSC_DR_MAX_NODE,
				KM_SLEEP);
	if (handle->dev_list == NULL) {
		handle->error = ENOMEM;
		return (handle->error);
	}

	/*
	 * Find the obp top nodes that belong to the targeted board.
	 */

	nodeid = handle->dev_list;
	firstchild = prom_childnode(prom_rootnode());
	for (child = firstchild; child != OBP_NONODE;
	    child = prom_nextnode(child)) {
		bd_id = -1;
		len = prom_getproplen(child, OBP_REG);
		if (len > 0) {
			regp = kmem_alloc(len, KM_SLEEP);
			(void) prom_getprop(child, OBP_REG,
				(caddr_t)regp);
			/*
			* Get board id for EXXXX platforms where
			* 0x1c0 is EXXXX platform specific data to
			* acquire board id.
			*/
			bd_id = (*regp - 0x1c0) >> 2;
			kmem_free(regp, len);
		}

		(void) prom_getprop(child, OBP_NAME, (caddr_t)name);
		if ((bd_id == board) &&
		    ((handle->flags & SYSC_DR_FHC) ?
		    (strcmp(name, "fhc") == 0):
		    (strcmp(name, "fhc") != 0)) &&
		    (strcmp(name, "central") != 0)) {
			if (handle->dev_list_len >= SYSC_DR_MAX_NODE) {
				cmn_err(CE_PANIC, "sysc_dr_init(): list "
				    "overflow");
				/*NOTREACHED*/
			}
			*nodeid = child;
			nodeid++;
			handle->dev_list_len++;
		}
	}

	return (0);
}

/*
 * Uninitialize devices for the state of a board.
 */
static void
sysc_dr_uninit(sysc_dr_handle_t *handle)
{
	kmem_free(handle->dev_list,
	    sizeof (dnode_t) * SYSC_DR_MAX_NODE);
	handle->dev_list = NULL;
	handle->dev_list_len = 0;
}

static void
sysc_dr_err_decode(sysc_dr_handle_t *handle, dev_info_t *dip, int attach)
{
	char	*p;

	ASSERT(handle->error != 0);
	switch (handle->error) {
	case NDI_NOMEM:
		handle->error = ENOMEM;
		break;
	case NDI_BUSY:
		handle->error = EBUSY;
		(void) ddi_pathname(dip, handle->errstr);
		break;
	case NDI_FAILURE:
	case NDI_BADHANDLE:
	case NDI_FAULT:
	default:
		handle->error = EFAULT;
		if (attach)
			(void) ddi_pathname(ddi_get_parent(dip),
			    handle->errstr);
		else
			(void) ddi_pathname(dip, handle->errstr);
		if (attach) {
			p = "/";
			(void) strcat(handle->errstr, p);
			(void) strcat(handle->errstr, ddi_node_name(dip));
		}
		break;
	}
}

/*
 * Walk a branch from top to bottom and attach driver for
 * each dev_info.
 */
static int
sysc_attach_branch(dev_info_t *pdip, dnode_t nodeid, void *arg)
{
	sysc_dr_handle_t *handle = (sysc_dr_handle_t *)arg;
	dnode_t			child_id;
	dev_info_t		*dip = NULL;
	static int		err = 0;
	static int		len = 0;
	static char		buf[OBP_MAXDRVNAME];

	handle->error = 0;

	if ((nodeid == OBP_NONODE) ||
	    (!DDI_CF2(pdip))) {
		return (handle->error);
	}

	(void) prom_getprop(nodeid, OBP_NAME, (caddr_t)buf);
	err = ndi_devi_alloc(pdip, buf, nodeid, &dip);
	if (err != NDI_SUCCESS) {
		handle->error = err;
		sysc_dr_err_decode(handle, NULL, TRUE);
		return (handle->error);
	}
	len = prom_getproplen(nodeid, OBP_REG);
	if (len <= 0) {
		(void) ndi_devi_free(dip);
		return (handle->error);
	}
	err = ndi_devi_online(dip, NDI_ONLINE_ATTACH);
	if (err != NDI_SUCCESS) {
		if (handle->flags & SYSC_DR_FHC) {
			handle->error = err;
			sysc_dr_err_decode(handle, dip, TRUE);
		}
		(void) ndi_devi_free(dip);
		return (handle->error);
	}
	child_id = prom_childnode(nodeid);
	if (child_id != OBP_NONODE) {
		for (; child_id != OBP_NONODE;
		    child_id = prom_nextnode(child_id)) {
			(void) sysc_attach_branch(dip, child_id,
			    (void *)handle);
		}
	}

	return (handle->error);
}

/*
 * Walk a branch from bottom to top and detach driver or clean up
 * each dev_info.
 */
static int
sysc_detach_branch(dev_info_t *dip, void *arg)
{
	sysc_dr_handle_t *handle = (sysc_dr_handle_t *)arg;
	static int		flag = 0;
	static int		err = 0;
	dev_info_t		*pdip = ddi_get_parent(dip);

	handle->error = 0;

	if (handle->flags & SYSC_DR_REMOVE) {
		flag |= NDI_DEVI_REMOVE;
	}
	if ((handle->flags & SYSC_DR_FORCE) &&
	    (ddi_streams_driver(dip) == DDI_SUCCESS)) {
		flag |= NDI_DEVI_FORCE;
	}
	if (DDI_CF2(pdip)) {
		(void) ndi_post_event(dip, dip,
		    ndi_event_getcookie(DDI_DEVI_REMOVE_EVENT), NULL);
	}
	err = ndi_devi_offline(dip, flag);
	if (err != NDI_SUCCESS) {
		handle->error = err;
		sysc_dr_err_decode(handle, dip, FALSE);
		return (DDI_WALK_TERMINATE);
	}

	return (DDI_WALK_CONTINUE);
}

static int
i_ddi_while_sibling_reverse(dev_info_t *dev,
				int (*f)(dev_info_t *, void *),
				void *arg)
{
	register dev_info_t *dip;
	register dev_info_t *sib;

	sib = dip = dev;

	while (dip != (dev_info_t *)NULL) {
		sib = (dev_info_t *)DEVI(dip)->devi_sibling;

		/* Recurse on siblings. */
		switch (i_ddi_walk_devs_reverse(dip, f, arg)) {
		case DDI_WALK_TERMINATE:
			return (DDI_WALK_TERMINATE);

		case DDI_WALK_PRUNESIB:
			return (DDI_WALK_CONTINUE);

		case DDI_WALK_CONTINUE:
		default:
			/* Continue down the sibling chain */
			break;
		}
		/*
		 * continue with other siblings, if any
		 */
		dip = sib;
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * The implementation of ddi_walk_devs_reverse().
 *
 * The routine can deal with functions removing the child node, but only by the
 * fact that ddi_remove_child() will link parent->devi_child to the siblings, if
 * one exists.
 */
static int
i_ddi_walk_devs_reverse(dev_info_t *dev,
			int (*f)(dev_info_t *, void *),
			void *arg)
{
	dev_info_t *lw = dev;
	dev_info_t *parent = dev;
	dev_info_t *start = dev;
	dev_info_t *sib = NULL;

	if (lw == (dev_info_t *)NULL)
		return (DDI_WALK_CONTINUE);

	/* Find the last child */
	while (ddi_get_child(lw) != NULL) {
		lw = ddi_get_child(lw);
	}

	/* lw should point to last child at this point */
	while (lw != (dev_info_t *)NULL) {
		/*
		 * If there are at that start of this tree, execute and return.
		 * Otherwise, save the parent and sibling pointer, in case lw
		 * got removed by (*f)()
		 */
		if (lw == start) {
			return ((*f)(lw, arg));
		} else {
			parent = ddi_get_parent(lw);
			sib = ddi_get_next_sibling(lw);
		}


		switch ((*f)(lw, arg)) {
		case DDI_WALK_TERMINATE:
			/*
			 * Caller is done!  Just return.
			 */
			return (DDI_WALK_TERMINATE);
			/*NOTREACHED*/

		case DDI_WALK_PRUNESIB:
			/*
			 * Caller has told us not to continue with our siblings.
			 * Set lw to point the parent and start over.
			 * Be careful lw may be removed at this point (by *f())
			 * Thus we use the saved parent pointer.
			 */
			lw = parent;
			break;

		case DDI_WALK_CONTINUE:
		default:
			/*
			 * If lw is removed by (*f)(), then parent->devi_child
			 * is pointing to the sibling.  If lw has no sibling,
			 * then we move up to the parent level and continue;
			 *
			 * If parent is at the start level, we stop.
			 */
			if ((lw == NULL) && (sib == NULL)) {
				lw = parent;
				break;
			}

			/*
			 * If we have siblings, we need to use recursion to
			 * continue with the siblings before we go back up to
			 * our parent. When all sibling nodes and their children
			 * are done, then we can go back to our parent node.
			 */
			if (sib != NULL) {
				if (i_ddi_while_sibling_reverse(
					(dev_info_t *)sib, f, arg)
					== DDI_WALK_TERMINATE) {
					return (DDI_WALK_TERMINATE);
				}
			}

			/*
			 * Set lw to our parent node and start over.
			 */
			lw = parent;
			break;
		}
	}

	return (DDI_WALK_CONTINUE);
}

/*
 * This general-purpose routine traverses the tree of dev_info nodes bottom-up,
 * starting from the end node, and calls the given function for each
 * node that it finds with the current node and the pointer arg (which
 * can point to a structure of information that the function
 * needs) as arguments.
 *
 */

static void
ddi_walk_devs_reverse(dev_info_t *dev,
			int (*f)(dev_info_t *, void *),
			void *arg)
{
	(void) i_ddi_walk_devs_reverse(dev, f, arg);
}

static char *
sysc_rstate_typestr(sysc_cfga_rstate_t rstate, sysc_audit_evt_t event)
{
	char *type_str;

	switch (rstate) {
	case SYSC_CFGA_RSTATE_EMPTY:
		switch (event) {
		case SYSC_AUDIT_RSTATE_EMPTY:
			type_str = "emptying";
			break;
		case SYSC_AUDIT_RSTATE_SUCCEEDED:
			type_str = "emptied";
			break;
		case SYSC_AUDIT_RSTATE_EMPTY_FAILED:
			type_str = "empty";
			break;
		default:
			type_str = "empty?";
			break;
		}
		break;
	case SYSC_CFGA_RSTATE_DISCONNECTED:
		switch (event) {
		case SYSC_AUDIT_RSTATE_DISCONNECT:
			type_str = "disconnecting";
			break;
		case SYSC_AUDIT_RSTATE_SUCCEEDED:
			type_str = "disconnected";
			break;
		case SYSC_AUDIT_RSTATE_DISCONNECT_FAILED:
			type_str = "disconnect";
			break;
		default:
			type_str = "disconnect?";
			break;
		}
		break;
	case SYSC_CFGA_RSTATE_CONNECTED:
		switch (event) {
		case SYSC_AUDIT_RSTATE_CONNECT:
			type_str = "connecting";
			break;
		case SYSC_AUDIT_RSTATE_SUCCEEDED:
			type_str = "connected";
			break;
		case SYSC_AUDIT_RSTATE_CONNECT_FAILED:
			type_str = "connect";
			break;
		default:
			type_str = "connect?";
			break;
		}
		break;
	default:
		type_str = "undefined receptacle state";
		break;
	}
	return (type_str);
}

static char *
sysc_ostate_typestr(sysc_cfga_ostate_t ostate, sysc_audit_evt_t event)
{
	char *type_str;

	switch (ostate) {
	case SYSC_CFGA_OSTATE_UNCONFIGURED:
		switch (event) {
		case SYSC_AUDIT_OSTATE_UNCONFIGURE:
			type_str = "unconfiguring";
			break;
		case SYSC_AUDIT_OSTATE_SUCCEEDED:
		case SYSC_AUDIT_OSTATE_UNCONFIGURE_FAILED:
			type_str = "unconfigured";
			break;
		default:
			type_str = "unconfigure?";
			break;
		}
		break;
	case SYSC_CFGA_OSTATE_CONFIGURED:
		switch (event) {
		case SYSC_AUDIT_OSTATE_CONFIGURE:
			type_str = "configuring";
			break;
		case SYSC_AUDIT_OSTATE_SUCCEEDED:
		case SYSC_AUDIT_OSTATE_CONFIGURE_FAILED:
			type_str = "configured";
			break;
		default:
			type_str = "configure?";
			break;
		}
		break;

	default:
		type_str = "undefined occupant state";
		break;
	}
	return (type_str);
}

static void
sysc_policy_audit_messages(sysc_audit_evt_t event, sysc_cfga_stat_t *sysc_stat)
{
	switch (event) {
		case SYSC_AUDIT_RSTATE_CONNECT:
			cmn_err(CE_NOTE,
				"%s %s board in slot %d",
				sysc_rstate_typestr(SYSC_CFGA_RSTATE_CONNECTED,
				event),
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board);
			break;
		case SYSC_AUDIT_RSTATE_DISCONNECT:
			cmn_err(CE_NOTE,
				"%s %s board in slot %d",
				sysc_rstate_typestr(
					SYSC_CFGA_RSTATE_DISCONNECTED,
					event),
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board);
			break;
		case SYSC_AUDIT_RSTATE_SUCCEEDED:
			cmn_err(CE_NOTE,
				"%s board in slot %d is %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_rstate_typestr(sysc_stat->rstate,
					event));
			break;
		case SYSC_AUDIT_RSTATE_CONNECT_FAILED:
			cmn_err(CE_NOTE,
				"%s board in slot %d failed to %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_rstate_typestr(SYSC_CFGA_RSTATE_CONNECTED,
					event));
			break;
		case SYSC_AUDIT_RSTATE_DISCONNECT_FAILED:
			cmn_err(CE_NOTE,
				"%s board in slot %d failed to %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_rstate_typestr(
					SYSC_CFGA_RSTATE_DISCONNECTED,
					event));
			break;
		case SYSC_AUDIT_OSTATE_CONFIGURE:
			cmn_err(CE_NOTE,
				"%s %s board in slot %d",
				sysc_ostate_typestr(SYSC_CFGA_OSTATE_CONFIGURED,
				event),
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board);
			break;
		case SYSC_AUDIT_OSTATE_UNCONFIGURE:
			cmn_err(CE_NOTE,
				"%s %s board in slot %d",
				sysc_ostate_typestr(
					SYSC_CFGA_OSTATE_UNCONFIGURED,
					event),
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board);
			break;
		case SYSC_AUDIT_OSTATE_SUCCEEDED:
			cmn_err(CE_NOTE,
				"%s board in slot %d is %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_ostate_typestr(sysc_stat->ostate,
					event));
			break;
		case SYSC_AUDIT_OSTATE_CONFIGURE_FAILED:
			cmn_err(CE_NOTE,
				"%s board in slot %d partially %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_ostate_typestr(
					SYSC_CFGA_OSTATE_CONFIGURED,
					event));
			break;
		case SYSC_AUDIT_OSTATE_UNCONFIGURE_FAILED:
			cmn_err(CE_NOTE,
				"%s board in slot %d partially %s",
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board,
				sysc_ostate_typestr(
					SYSC_CFGA_OSTATE_UNCONFIGURED,
					event));
			break;
		default:
			cmn_err(CE_NOTE,
				"unknown audit of a %s %s board in"
				" slot %d",
				sysc_rstate_typestr(sysc_stat->rstate,
					event),
				fhc_bd_typestr(sysc_stat->type),
				sysc_stat->board);
			break;
	}
}

#define	MAX_PROP_LEN	33	/* must be > strlen("cpu") */

static int
find_and_setup_cpu(int cpuid)
{
	dnode_t nodeid;
	int upaid;
	char type[MAX_PROP_LEN];

	nodeid = prom_childnode(prom_rootnode());
	while (nodeid != OBP_NONODE) {
		if (prom_getproplen(nodeid, "device_type") < MAX_PROP_LEN)
			(void) prom_getprop(nodeid, "device_type",
			    (caddr_t)type);
		else
			type[0] = '\0';
		(void) prom_getprop(nodeid, "upa-portid", (caddr_t)&upaid);
		if ((strcmp(type, "cpu") == 0) && (upaid == cpuid)) {
			return (cpu_configure(cpuid));
		}
		nodeid = prom_nextnode(nodeid);
	}
	return (ENODEV);
}

#define	MAX_BOARD_TYPE	IO_SBUS_FFB_SOCPLUS_BOARD

static char sysc_supp_conn[MAX_BOARD_TYPE + 1];

static int
sysc_board_connect_supported(enum board_type type)
{
	if (type > MAX_BOARD_TYPE)
		return (0);
	return (sysc_supp_conn[type]);
}

void
sysc_board_connect_supported_init(void)
{
	dnode_t openprom_node;
	char sup_list[16];
	int proplen;
	int i;
	char tstr[3 * 5 + 1];

	openprom_node = prom_finddevice("/openprom");
	if (openprom_node != OBP_BADNODE) {
		proplen = prom_bounded_getprop(openprom_node,
		    "add-brd-supported-types",
		    sup_list, sizeof (sup_list) - 1);
	} else {
		proplen = -1;
	}

	if (proplen < 0) {
		/*
		 * This is an old prom which may cause a fatal reset,
		 * so don't allow any DR operations.
		 * If enable_dynamic_reconfiguration is 0
		 * we have already printed a similar message.
		 */
		if (enable_dynamic_reconfiguration) {
			cmn_err(CE_WARN, "Firmware does not support"
			    " Dynamic Reconfiguration");
			enable_dynamic_reconfiguration = 0;
		}
		return;
	}
	for (i = 0; i < proplen; i++) {
		switch (sup_list[i]) {
		case '0':
			sysc_supp_conn[CPU_BOARD] = 1;
			sysc_supp_conn[MEM_BOARD] = 1;
			break;
		case '1':
			sysc_supp_conn[IO_2SBUS_BOARD] = 1;
			break;
		case '2':
			sysc_supp_conn[IO_SBUS_FFB_BOARD] = 1;
			break;
		case '3':
			sysc_supp_conn[IO_PCI_BOARD] = 1;
			break;
		case '4':
			sysc_supp_conn[IO_2SBUS_SOCPLUS_BOARD] = 1;
			break;
		case '5':
			sysc_supp_conn[IO_SBUS_FFB_SOCPLUS_BOARD] = 1;
			break;
		default:
			/* Ignore other characters. */
			break;
		}
	}
	if (sysc_supp_conn[CPU_BOARD]) {
		cmn_err(CE_NOTE, "!Firmware supports Dynamic Reconfiguration"
		    " of CPU/Memory boards.");
	} else {
		cmn_err(CE_NOTE, "Firmware does not support Dynamic"
		    " Reconfiguration of CPU/Memory boards.");
	}

	tstr[0] = '\0';
	if (sysc_supp_conn[IO_2SBUS_BOARD])
		(void) strcat(tstr, ", 1");
	if (sysc_supp_conn[IO_SBUS_FFB_BOARD])
		(void) strcat(tstr, ", 2");
	if (sysc_supp_conn[IO_PCI_BOARD])
		(void) strcat(tstr, ", 3");
	if (sysc_supp_conn[IO_2SBUS_SOCPLUS_BOARD])
		(void) strcat(tstr, ", 4");
	if (sysc_supp_conn[IO_SBUS_FFB_SOCPLUS_BOARD])
		(void) strcat(tstr, ", 5");
	if (tstr[0] != '\0') {
		/* Skip leading ", " using &tstr[2]. */
		cmn_err(CE_NOTE, "!Firmware supports Dynamic Reconfiguration"
		    " of I/O board types %s.", &tstr[2]);
	} else {
		cmn_err(CE_NOTE, "Firmware does not support Dynamic"
		    " Reconfiguration of I/O boards.");
	}
}

#ifdef DEBUG

static void
precache_regdump(int board)
{
	fhc_bd_t *curr_bdp;
	int bd_idx;
	int reg_idx;

	for (bd_idx = 0; bd_idx < fhc_max_boards(); bd_idx++) {
		bcopy((void *) reg_tmpl, (void *) &reg_dt[bd_idx][0],
		    (sizeof (struct regs_data))*NUM_REG);
		curr_bdp = fhc_bd(bd_idx);
		if (curr_bdp->sc.rstate == SYSC_CFGA_RSTATE_CONNECTED) {
			for (reg_idx = 0; reg_idx < NUM_REG; reg_idx++) {
				reg_dt[bd_idx][reg_idx].eflag = TRUE;
				if (bd_idx != board)
					reg_dt[bd_idx][reg_idx].oflag = TRUE;
				reg_dt[bd_idx][reg_idx].physaddr +=
				    (FHC_BOARD_SPAN*2*bd_idx);
				reg_dt[bd_idx][reg_idx].pre_dsct =
				    ldphysio(reg_dt[bd_idx][reg_idx].physaddr);
			}
		}
	}


}

static void
boardstat_regdump(void)
{
	int bd_idx;

	prom_printf("\nBoard status before disconnect.\n");
	for (bd_idx = 0; bd_idx < fhc_max_boards(); bd_idx++) {
		if (reg_dt[bd_idx][0].eflag == 0) {
			prom_printf("Board #%d is idle.\n", bd_idx);
		} else {
			prom_printf("Board #%d is on.\n", bd_idx);
		}
	}

	for (bd_idx = 0; bd_idx < fhc_max_boards(); bd_idx++) {
		if (reg_dt[bd_idx][0].eflag) {
			prom_printf("\nRegisters for Board #%d", bd_idx);
			prom_printf(" (before disconnect).\n");
			prom_printf("AC_BCSR   FHC_CTRL  JTAG      IGN   SIM"
			    "       SISM  UIM       USM\n");
			prom_printf("%08x  %08x  %08x  %04x"
			    "  %08x  %04x  %08x  %04x\n",
			    reg_dt[bd_idx][0].pre_dsct,
			    reg_dt[bd_idx][1].pre_dsct,
			    reg_dt[bd_idx][2].pre_dsct,
			    reg_dt[bd_idx][3].pre_dsct,
			    reg_dt[bd_idx][4].pre_dsct,
			    reg_dt[bd_idx][5].pre_dsct,
			    reg_dt[bd_idx][6].pre_dsct,
			    reg_dt[bd_idx][7].pre_dsct);
		}
	}

}

static void
display_regdump(void)
{
	int bd_idx;
	int reg_idx;

	prom_printf("Board status after disconnect.\n");
	for (bd_idx = 0; bd_idx < fhc_max_boards(); bd_idx++) {
		if (reg_dt[bd_idx][0].oflag == 0) {
			prom_printf("Board #%d is idle.\n", bd_idx);
		} else {
			prom_printf("Board #%d is on.\n", bd_idx);
			for (reg_idx = 0; reg_idx < NUM_REG; reg_idx++)
				reg_dt[bd_idx][reg_idx].post_dsct =
				    ldphysio(reg_dt[bd_idx][reg_idx].physaddr);
		}
	}

	for (bd_idx = 0; bd_idx < fhc_max_boards(); bd_idx++) {
		if (reg_dt[bd_idx][0].eflag) {
			prom_printf("\nRegisters for Board #%d", bd_idx);
			prom_printf(" (before and after disconnect).\n");
			prom_printf("AC_BCSR   FHC_CTRL  JTAG      IGN   SIM"
			    "       SISM  UIM       USM\n");
			prom_printf("%08x  %08x  %08x  %04x"
			    "  %08x  %04x  %08x  %04x\n",
			    reg_dt[bd_idx][0].pre_dsct,
			    reg_dt[bd_idx][1].pre_dsct,
			    reg_dt[bd_idx][2].pre_dsct,
			    reg_dt[bd_idx][3].pre_dsct,
			    reg_dt[bd_idx][4].pre_dsct,
			    reg_dt[bd_idx][5].pre_dsct,
			    reg_dt[bd_idx][6].pre_dsct,
			    reg_dt[bd_idx][7].pre_dsct);
			if (reg_dt[bd_idx][0].oflag) {
				prom_printf("%08x  %08x  %08x  %04x"
				    "  %08x  %04x  %08x  %04x\n",
				    reg_dt[bd_idx][0].post_dsct,
				    reg_dt[bd_idx][1].post_dsct,
				    reg_dt[bd_idx][2].post_dsct,
				    reg_dt[bd_idx][3].post_dsct,
				    reg_dt[bd_idx][4].post_dsct,
				    reg_dt[bd_idx][5].post_dsct,
				    reg_dt[bd_idx][6].post_dsct,
				    reg_dt[bd_idx][7].post_dsct);
			} else {
				prom_printf("no data (board got"
				    " disconnected)-------------------"
				    "---------------\n");
			}
		}

	}

}

#endif /* DEBUG */
