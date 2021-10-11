/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)intr15.c	1.68	99/07/27 SMI"

/*
 * intr15_handler
 */

/*
 * TODO:
 * SYNC_PANIC: figure out how to sync() from interrupt w/o an nfs panic
 * softint_temp(): check all BootBusses, not just local
 * bug: memerr() can be entered by 3 concurrent calls to softint_ecc(),
 * which can cause memerr_ECI to be enabled during memerr().
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/t_lock.h>
#include <sys/sunddi.h>
#include <sys/signal.h>
#include <sys/cpuvar.h>
#include <sys/procset.h>
#include <sys/cmn_err.h>
#include <sys/syserr.h>
#include <sys/memerr.h>
#include <sys/led.h>
#include <sys/mmu.h>	/* MXCC_ERR_* */
#include <sys/physaddr.h>	/* PA_SBUS_SPACE_BASE */
#include <sys/kmem.h>	/* KM_NOSLEEP */
#include <sys/thread.h>
#include <sys/promif.h>	/* prom_setprop(), obpdefs.h */
#include <sys/prom_plat.h>
#include <sys/debug.h>
#include <sys/privregs.h>
#include <sys/psr.h>

/*
 * External Routines:
 */
extern void poll_obp_mbox(void);
extern void power_off(void);
extern void debug_enter(char *msg);

/*
 * External Data:
 */
extern int pokefault;
extern int hz;

/*
 * Global Routines:
 */
void bb_ctl_bits_all(uint_t bits, uint_t enable);
void level15_mlsetup(void);
void level15_post_startup(void);
void kill_proc(kthread_t *t, proc_t *p, caddr_t addr);

/*
 * Global Data:
 */

/*
 * disable_xxxx is set == 1 in /etc/system if the interrupt enabling
 * and handling for that device needs to be disabled.
 */
uint_t disable_acfail;		/* override for /etc/system */
uint_t disable_tempfail;	/* override for /etc/system */
uint_t disable_fanfail;		/* override for /etc/system */
uint_t disable_power;		/* override for /etc/system */
uint_t disable_sbifail;		/* override for /etc/system */
uint_t disable_mxccfail;	/* override for /etc/system */
uint_t oven_timeout_sec = OVEN_TIMEOUT_SEC;	/* set to default */

/*
 * This variable tells us if we are in a heat chamber. It is set
 * early on in boot, after we check the OBP 'testarea' property in
 * the options node.
 */
static uint_t temperature_chamber = 0;
/*
 * to identify FRU cpu from cpu_id
 */
#define	CPU_ID_2_BOARD(id) ((id)/2)
#define	CPU_ID_IS_CPUB(id) ((id) & 0x1)

#define	ECI_DELAY (hz/20)

#define	ECC_THROTTLE 64	/* log no more than 128 errors/interrupt */
static uint_t ecc_throttle = ECC_THROTTLE;

/*
 * un-comment this line to test the level10 poll routine, syserr_poll().
 * #define LEVEL10_TEST
 */
#ifdef LEVEL10_TEST
#define	TRIGGER_INTR(handler) { \
	register uint_t level = (handler).ipl; \
	syserr_req[level] = 0xFF; \
}
#else !LEVEL10_TEST
#define	TRIGGER_INTR(handler) { \
	register uint_t level = (handler).ipl; \
	syserr_req[level] = 0xFF; \
	xmit_cpu_intr(CPU->cpu_id, level); \
}
#endif LEVEL10_TEST

/*
 * Static Routines:
 */
static void intr15_bbus(uint_t cpu_id);
static void intr15_mxcc(struct regs *);
static void intr15_xbus();
static void level15_init_bbus(void);
static void level15_init_ecc(void);
static void level15_init_mxcc(void);
static void level15_init_sbi(void);
static void bb_ctl_bits(uint_t bits, uint_t enable, int cpu_id);

static int softint_fan(caddr_t);
static int softint_mxcc(caddr_t);
static int softint_temp(caddr_t);
static int softint_power(caddr_t);
static void oven_timeout(void *);
static int softint_sbi(caddr_t);
static int softint_ecc(caddr_t);

/*
 * Static Data:
 */

typedef struct error_handler {
	lock_t	detected;
	uint_t	detected_cpu;
	uint_t	ipl;
	uint_t	enabled;
	lock_t	active;
	uint_t	active_cpu;
	int	(*handler)(caddr_t);
	caddr_t	arg;
} error_handler_t;

#define	NO_CPU (uint_t)-1	/* an illegal cpu_id uint_t */

#define	HANDLER(handler, ipl, enabled, arg) \
	{0, NO_CPU, ipl, enabled, 0, NO_CPU, handler, (caddr_t)(arg)}

static lock_t acfail_detected;

/*
 * to view in kadb: handler_temp,38/X
 */
static error_handler_t handler_temp =
	HANDLER(softint_temp, IPL_TEMP, 0, 0);

static error_handler_t handler_fan =
	HANDLER(softint_fan, IPL_FAN, 0, 0);

static error_handler_t handler_power =
	HANDLER(softint_power, IPL_POWER, 0, 0);

static error_handler_t handler_mxcc =
	HANDLER(softint_mxcc, IPL_MXCC, 1, 0);

static error_handler_t handler_sbi =
	HANDLER(softint_sbi, IPL_SBI, 0, 0);

static error_handler_t handler_ecc_ce =
	HANDLER(softint_ecc, IPL_ECC_CE, 0, MEMERR_CE);

static error_handler_t handler_ecc_ue =
	HANDLER(softint_ecc, IPL_ECC_UE, 0, MEMERR_UE);

static error_handler_t handler_ecc_ue_async =
	HANDLER(softint_ecc, IPL_ECC_UE, 0, MEMERR_UE | MEMERR_FATAL);

static error_handler_t *syserr_handlers[] = {
	&handler_temp,
	&handler_fan,
	&handler_power,
	&handler_mxcc,
	&handler_sbi,
	&handler_ecc_ce,
	&handler_ecc_ue,
	&handler_ecc_ue_async,
	NULL,
};

static int nvram_offset_powerfail;

/*
 * l15 can never do anything that requires a lock
 * other than these, else it could interrupt a thread that
 * holds the lock it needs, and hang the system.
 */

#ifdef DEBUG
uint_t intr15_count[NCPU];
uint_t intr15_loops[NCPU];
#endif	/* DEBUG */

/*
 * level15_mlsetup():  called from the end of mlsetup().
 * initialize locks as necessary.
 */

void
level15_mlsetup()
{
}


/*
 * level15_startup():  called from the end of startup().
 * enable the softint handlers.
 */

void
level15_startup()
{
}

/*
 * level15_startup():  called from post_startup();
 * we can now initialize handlers that use callouts, since
 * they're initialized after startup().
 */
void
level15_post_startup()
{
	level15_init_bbus();
	level15_init_mxcc();
	level15_init_sbi();
	level15_init_ecc();
}

/*
 * find out where OBP has stashed the option in NVRAM.
 * used for option = "powerfail-time".
 * On failure, return the reserved offset value 0.
 */

static int
nvram_get_offset(char *option)
{
	dev_info_t *options_dip;
	dnode_t options_node_id;
	char orig_buffer[80];
	char cmd_buffer[80];
	char offset_buffer[80];
	int retval;
	char *tmp;

	/*
	 * Find 'options' node in the tree, then get the
	 * underlying nodeid as understood by the OBP.
	 */
	if ((options_dip = ddi_find_devinfo("options", -1, 0)) == NULL) {
		cmn_err(CE_WARN, "nvram_get_offset: no options node");
		return (0);
	} else {
		options_node_id = (dnode_t)ddi_get_nodeid(options_dip);
	}

	retval = prom_getprop(options_node_id, option, orig_buffer);

	/*
	 * powerfail-time option does not exist.
	 * this will happen on systems with PROM older than 2.10 pilot.
	 */
	if (retval == -1)
		return (0);

	orig_buffer[retval] = '\0';

	(void) sprintf(cmd_buffer, "' %s >body w@ to %s\n", option, option);

	prom_interpret(cmd_buffer, 0, 0, 0, 0, 0);

	retval = prom_getprop(options_node_id, option, offset_buffer);
	offset_buffer[retval] = '\0';

	retval = prom_setprop(options_node_id, option, orig_buffer,
			strlen(orig_buffer) + 1);

	tmp = offset_buffer;
	retval = stoi(&tmp);

	return (retval);
}

void
nvram_update_powerfail(uint_t pattern)
{
	uchar_t oldval[4];
	uchar_t newval[4];
	uchar_t old_checksum, new_checksum;
	extern struct cpu cpu0;
	uint_t nv_cpuid = cpu0.cpu_id;
	int i;

	newval[3] = pattern & 0xFF;
	newval[2] = (pattern >> 8) & 0xFF;
	newval[1] = (pattern >> 16) & 0xFF;
	newval[0] = (pattern >> 24) & 0xFF;

	/*
	 * get old value
	 */

	oldval[0] = nvram_get_byte(nv_cpuid, nvram_offset_powerfail);
	oldval[1] = nvram_get_byte(nv_cpuid, nvram_offset_powerfail + 1);
	oldval[2] = nvram_get_byte(nv_cpuid, nvram_offset_powerfail + 2);
	oldval[3] = nvram_get_byte(nv_cpuid, nvram_offset_powerfail + 3);

	/*
	 * get checksum
	 */
	old_checksum = nvram_get_byte(nv_cpuid, OFF_BB_NVRAM_CHECKSUM);
	new_checksum = old_checksum;

	/*
	 * if the byte value has changed, write the byte and update
	 * the checksum by removing old pattern and adding new.
	 */
	for (i = 0; i < 4; ++i)
		if (oldval[i] != newval[i]) {
			nvram_set_byte(nv_cpuid,
					nvram_offset_powerfail + i, newval[i]);
			new_checksum = new_checksum ^ oldval[i] ^ newval[i];
		}

	/*
	 * write new checksum
	 */
	if (new_checksum != old_checksum) {
		nvram_set_byte(nv_cpuid, OFF_BB_NVRAM_CHECKSUM, new_checksum);
	}
}

/*
 * enable the level15 BootBus error handlers handlers.
 * unmask the level15 BootBus interrupts on the master cpu
 */
void
level15_init_bbus(void)
{
	dev_info_t *options_dip;
	dnode_t options_node_id;
	int retval;
	int testarea_len;
	char *testarea;

	uchar_t cpu_id = CPU->cpu_id;

	if (disable_fanfail) {
		cmn_err(CE_WARN, "Disabling fan failure handler.");
		handler_fan.enabled = 0;
	} else {
		handler_fan.enabled = 1;
		if (CPU_IN_SET(bootbusses, cpu_id))
			bb_ctl_bits(BBUS_CTL_FAN_BIT, BITSET, cpu_id);
	}

	/*
	 * An OBP option, 'testarea' is being used to inform us as to
	 * whether we are in an enviromental chamber. It exists in
	 * the 'options' node. This is where all OBP 'setenv' (eeprom)
	 * parameters live.
	 */
	if ((options_dip = ddi_find_devinfo("options", -1, 0)) != NULL) {
		options_node_id = (dnode_t)ddi_get_nodeid(options_dip);
		testarea_len = prom_getproplen(options_node_id, "testarea");
		if (testarea_len == -1) {
			return;
		}
		if ((testarea = kmem_alloc(testarea_len+1, KM_SLEEP)) ==
		    NULL) {
			return;
		}

		retval = prom_getprop(options_node_id, "testarea", testarea);
		if (retval != -1) {
			testarea[retval] = 0;
			if (strcmp(testarea, CHAMBER_VALUE) == 0) {
				temperature_chamber = 1;
			}
		}
		kmem_free(testarea, testarea_len+1);
	}

	if (disable_tempfail) {
		cmn_err(CE_WARN, "Disabling temperature failure handler.");
		handler_temp.enabled = 0;
	} else {
		handler_temp.enabled = 1;
		if (CPU_IN_SET(bootbusses, cpu_id))
			bb_ctl_bits(BBUS_CTL_TEMP_BIT, BITSET, cpu_id);
	}

	if (disable_acfail) {
		cmn_err(CE_WARN, "Disabling power failure handler.");
	} else {
		/*
		 * this routine takes 18ms to run, mostly due
		 * to prom_setprop() taking 10ms.
		 */
		nvram_offset_powerfail = nvram_get_offset("powerfail-time");
	}

	/*
	 * Only SC2000 and SC2000+ can be equipped with dual power
	 * supply hardware and detection circuitry.
	 */
	if (sun4d_model == MODEL_SC2000) {
		if (disable_power) {
			cmn_err(CE_WARN, "Disabling power failure handler.");
			handler_power.enabled = 0;
		} else {
			handler_power.enabled = 1;
			if (CPU_IN_SET(bootbusses, cpu_id))
				bb_ctl_bits(BBUS_CTL_POWER_BIT, BITSET, cpu_id);
		}
	}
}

/*
 * enable the level15 MXCC handler
 */
static void
level15_init_mxcc()
{
	if (disable_mxccfail) {
		cmn_err(CE_WARN, "Disabling softint_mxcc().");
		handler_mxcc.enabled = 0;
	} else {
		handler_mxcc.enabled = 1;
	}
}

/*
 * enable the level15 ECC handlers
 */
static void
level15_init_ecc()
{
	/*
	 * Set up data structures for memerr().
	 */
	if (memerr_init()) {
		cmn_err(CE_WARN, "level15_init_ecc: ECC handling disabled!");
		return;
	}

	/*
	 * allow syserr_handler() to call memerr()
	 */
	handler_ecc_ce.enabled = 1;
	handler_ecc_ue.enabled = 1;
	handler_ecc_ue_async.enabled = 1;

#define	POST_WORKAROUND
	/*
	 * on reboot, old PROMs neglect to clear MQH.MCSR.ECI
	 * make sure it is clear, cause memerr_ce() asserts that it is.
	 */
#ifdef POST_WORKAROUND
	memerr_ECI(0);
#endif POST_WORKAROUND

	/*
	 * poll the MQH registers for outstanding CE and UE errors
	 * in case we had some before memerr_ECI() or before
	 * the handlers were enabled.
	 */
	(void) memerr(MEMERR_UE);
	(void) memerr(MEMERR_CE);

	/*
	 * Enable Correctable Interrupts on CE's
	 */
	memerr_ECI(1);

	/*
	 * now we're ready for ECC errors, lets look for some
	 */
	if (memscrub_init()) {
		cmn_err(CE_WARN,
			"level15_init_ecc: memory scrubbing disabled!");
		return;
	}
}
/*
 * initialize the level15 SBI error handler.
 */

static uint_t n_sbi;
static uint_t sbi_devids[10];
#define	SBI_ECSR_BASE 0x00800000
#define	SBI_DEVID_SHIFT 24
#define	SBI_OFF_STATUS 0x8
#define	SBI_STATUS_PPE	0x1

#define	sbi_board(devid) ((devid) >> 4)

static void
level15_init_sbi()
{
	dev_info_t *dip;

	dip = ddi_root_node();

	/*
	 * search 1st level children in devinfo tree
	 */
	dip = ddi_get_child(dip);	/* 1st child of root */
	while (dip) {
		char *name;
		int devid;

		name = ddi_get_name(dip);
		if (strcmp("io-unit", name) == 0) {
			int unit = n_sbi;

			devid = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
			    DDI_PROP_DONTPASS, PROP_DEVICE_ID, -1);
			if (devid == -1) {
				cmn_err(CE_WARN,
				    "level15_init_sbi(): no %s for %s\n",
				    PROP_DEVICE_ID, name);
				return;
			}
			sbi_devids[unit] = devid;
			n_sbi = unit + 1;
		}
		dip = ddi_get_next_sibling(dip);
	}

	/* found nothing */
	if (n_sbi == 0) {
		cmn_err(CE_WARN, "level15_init_sbi: no io-unit found!");
		return;
	}

	if (disable_sbifail) {
		cmn_err(CE_WARN, "Disabling softint_sbi().");
		handler_sbi.enabled = 0;
	} else {
		handler_sbi.detected = 1;
		handler_sbi.active = 1;
		(void) softint_sbi((caddr_t)0);

		handler_sbi.enabled = 1;
	}
}

/*
 * Enable local l15's for Environment Sensors
 * Call from cpu_attach for non-master cpus
 */
void
level15_enable_bbus(uint_t cpu_id)
{
	/*
	 * XXX before turning on the interrupt, poll for it?
	 */
	if (!disable_fanfail)
		bb_ctl_bits(BBUS_CTL_FAN_BIT, BITSET, cpu_id);

	if (!disable_tempfail)
		bb_ctl_bits(BBUS_CTL_TEMP_BIT, BITSET, cpu_id);

	if (sun4d_model == MODEL_SC2000 && !disable_power)
		bb_ctl_bits(BBUS_CTL_POWER_BIT, BITSET, cpu_id);
}

/*
 * Begin code that runs at level15
 */

/*
 * _interrupt() in locore.s has masked level15 in the IMR before enabling
 * traps and vectoring to this routine.
 *
 * Special rules apply to intr15_handler() and anything it calls.
 * It can not sleep or spin on any locks that might be held by a
 * non-level15 thread -- as level15 may have interrupted that thread.
 *
 * intr15_handler may be called as soon as the kernel's SCB
 * is installed and traps are enabled.  For this reason, it
 * must be very careful what it touches.
 */

#ifdef DEBUG
#define	DEBUG_LED(x) led_set_cpu(CPU->cpu_id, x)
#else
#define	DEBUG_LED(x)
#endif	/* DEBUG */

void
intr15_handler(struct regs *rp)
{
#ifdef DEBUG_loops
	uint_t local_loops = 0;
#endif DEBUG
	uint_t cpu_id;

	DEBUG_INC(intr15_count[CPU->cpu_id]);

	cpu_id = CPU->cpu_id;

	DEBUG_LED(0x5);

	do {
#ifdef DEBUG_loops
		++local_loops;
		if (local_loops == 8)
			debug_enter("loop15: 8");
		if (!(local_loops % 16384))
			debug_enter("loop15: n * 16384");
#endif DEBUG

		DEBUG_INC(intr15_loops[CPU->cpu_id]);

		/*
		 * Poll the OBP mailbox to see if this CPU should enter
		 * the PROM in response to watchdog, breakpoint etc.
		 */
		poll_obp_mbox();

		/*
		 * Check local L15's from MXCC
		 */
		intr15_mxcc(rp);

		/*
		 * If we are a BootBus master, Check BootBus sources:
		 * Power-fail, over-temp, and fan-fail
		 */
		if (CPU_IN_SET(bootbusses, cpu_id))
			intr15_bbus(cpu_id);

		/*
		 * Check for L15's brought to us by the BW. These are either
		 * ECC errors from the MQH or XPT errors from the SBI.
		 */
		intr15_xbus();

	} while (intr_get_pend_local() & (1 << 15));

	DEBUG_LED(LED_CPU_RESUME);

	/*
	 * allow level15's in on this processor again
	 */
	intr_clear_mask_bits(1 << 15);
}

/*
 * BootBus Environment Indicators
 *
 * These are generated by the BootBus, and are sent
 * to the processor that holds the BootBus semaphore.
 *
 * If we detect a Fan Fail or Temperature Fail interrupt,
 * we squelch them via the bootBus control register,
 * and re-clear the IPR.
 *
 * If we detect an AC failure, we handle it here by
 * idling the other cpus in the PROM, recording a timestamp in NVRAM,
 * and then dropping into the PROM ourselves.
 */

uint_t intr15_temp_detected[NCPU];

#ifdef DEBUG
uint_t intr15_count_temp;
uint_t intr15_count_fan;
uint_t intr15_count_power;
uint_t intr15_fan_still_pend[NCPU];
uint_t intr15_ac_detected[NCPU];
uint_t intr15_ac_claimed[NCPU];

#endif	/* DEBUG */

static void
intr15_bbus(uint_t cpu_id)
{
	uint_t status2;

	status2 = xdb_bb_status2_get();

	/*
	 * Power Supply detected AC failure.
	 *
	 * Either we have 5ms till power is gone,
	 * or this is a brownout and this bit will be cleared.
	 *
	 * This is broadcast to all BootBusses
	 */
	if (status2 & STATUS2_AC_INT) {
		extern dev_info_t *cpu_get_dip();
		dev_info_t *dip;

		DEBUG_INC(intr15_ac_detected[cpu_id]);

		if (!acfail_detected && lock_try(&acfail_detected)) {
			char buffer[100];
			int id;

			DEBUG_INC(intr15_ac_claimed[cpu_id]);

			/*
			 * idle other cpus
			 */
			for (id = 0; id < NCPU; id++)
			    if ((id != cpu_id) && (dip = cpu_get_dip(id))) {
				    (void) prom_idlecpu(
					(dnode_t)ddi_get_nodeid(dip));
			}

			/*
			 * record time in NVRAM parameter "powerfail-time"
			 */
			if (nvram_offset_powerfail)
				nvram_update_powerfail(hrestime.tv_sec);

			(void) sprintf(buffer,
			    "POWER FAILURE (detected on board %d),"
			    " entering monitor.\n", (CPU->cpu_id) / 2);

			debug_enter(buffer);
			/*
			 * The operator resumed us.
			 * we clear l15 on all bootbus cpus and continue on.
			 * If the powerfail interrupt line is still active
			 * we'll end up back here proto -- else the system
			 * will continue to run.
			 */


			for (id = 0; id < NCPU; id++) {
				if (bootbusses & (1 << id)) {
					intr_clear_pend_ecsr(id, 0xF);
				}
			}
			lock_clear(&acfail_detected);
		}
		/*
		 * else continue looking for level15 source
		 */
	}
	/*
	 * Temperature Warning.
	 *
	 * Every board has a temperature sensor,
	 * so we may get 1 of these for every BootBus.
	 *
	 */
	if (status2 & STATUS2_TMP_INT) {
		intr15_temp_detected[CPU->cpu_id] += 1;

		/*
		 * Squelch the BootBus source of this interrupt and re-clear
		 * the IPR.
		 */

		bb_ctl_bits(BBUS_CTL_TEMP_BIT, BITCLR, cpu_id);
		(void) intr_clear_pend_local(0xF);

		/*
		 * if handler is not disabled
		 * and we're the 1st to notice,
		 * then handle it.
		 */
		if (!handler_temp.detected &&
		    lock_try(&handler_temp.detected)) {
			DEBUG_INC(intr15_count_temp);
			handler_temp.detected_cpu = cpu_id;

			/*
			 * handler_temp.detected will be owned by the softint
			 */
			if (!disable_tempfail)
				TRIGGER_INTR(handler_temp);
		}
	}
	/*
	 * Fan Failure.
	 *
	 * There is 1 blower sensor per system.
	 * this level15 is broadcast to all BootBusses
	 */
	if (status2 & STATUS2_FAN_INT) {
		DEBUG_LED(6);

		/*
		 * Squelch the BootBus source of this interrupt and re-clear
		 * the IPR.
		 */

		bb_ctl_bits(BBUS_CTL_FAN_BIT, BITCLR, cpu_id);
		(void) intr_clear_pend_local(0xF);

		/*
		 * if we're the 1st to notice, then handle it
		 */
		if (!handler_fan.detected && lock_try(&handler_fan.detected)) {
			DEBUG_INC(intr15_count_fan);
			handler_fan.detected_cpu = cpu_id;

			/*
			 * handler_fan.detected will be owned by the softint
			 */
			if (!disable_fanfail)
				TRIGGER_INTR(handler_fan);
		}
	}
	/*
	 * Dual Power Supply Failure.
	 *
	 * The SC2000+ systems have dual power supplies. If one of them
	 * fails, then we will get a level 15 interrupt and the
	 * STATUS2_PWR_INT bit will be set.
	 */
	if (status2 & STATUS2_PWR_INT) {
		/*
		 * Squelch the BootBus source of this interrupt and re-clear
		 * the IPR.
		 */

		bb_ctl_bits(BBUS_CTL_POWER_BIT, BITCLR, cpu_id);
		(void) intr_clear_pend_local(0xF);

		/*
		 * if we're the 1st to notice, then handle it
		 */
		if (!handler_power.detected &&
		    lock_try(&handler_power.detected)) {
			DEBUG_INC(intr15_count_power);
			handler_power.detected_cpu = cpu_id;

			/*
			 * handler_power.detected will be owned by the softint
			 */
			if (!disable_power)
				TRIGGER_INTR(handler_power);
		}
	}
}

/*
 * Local level15 interrupts from the MXCC don't set the INTSID,
 * so we poll the local MXCC.ER.
 *
 * Also, since they don't go away on clearing the IPR, we're
 * running with level15 masked in the IMR.
 *
 * The interrupt is squelched when we clear the MXCC.ER.
 */

#ifdef DEBUG
uint_t intr15_pokefault[NCPU];
#endif DEBUG

/*
 * per-cpu mxcc globals & associated locks
 */

static u_longlong_t mxcc_err_reg[NCPU];
static kthread_t *mxcc_curthread[NCPU];
static greg_t	mxcc_psr[NCPU];
static lock_t	mxcc_detected[NCPU];
static lock_t	mxcc_active[NCPU];

static void
intr15_mxcc(struct regs *rp)
{
	u_longlong_t mxcc_err;
	uint_t mxcc_err_hi;

	mxcc_err = (u_longlong_t)intr_mxcc_error_get();

	mxcc_err_hi = (uint_t)(mxcc_err >> 32);

	if (!(mxcc_err_hi &
	    (MXCC_ERR_ME | MXCC_ERR_VP | MXCC_ERR_CP | MXCC_ERR_AE)))
		return;

	/*
	 * else detected an MXCC error.
	 *
	 * Squelch any MXCC Error interrupt sources
	 * and re-clear the IPR
	 */
	intr_mxcc_error_set((longlong_t)mxcc_err);
	(void) intr_clear_pend_local(0xF);

	/*
	 * If Async Error due to ddi_poke(), handle it here
	 * else the poke can complete on another cpu before
	 * we get to softint_mxcc().
	 */
	if (mxcc_err_hi & MXCC_ERR_AE) {
		uint_t ccop, dcmd;

		ccop = (mxcc_err_hi & MXCC_ERR_CCOP) >>
			MXCC_ERR_CCOP_SHFT;
		dcmd = (ccop >> 4);

		/*
		 * If AE is I/O Write
		 * and there is a ddi_poke(9F) outstanding (someplace)
		 */
		if ((dcmd == DCMD_IOW) && (pokefault == -1)) {
			/*
			 * indicate the poke failed
			 * sure hope this AE is for the poke (1122211)
			 */

			DEBUG_INC(intr15_pokefault[CPU->cpu_id]);

			pokefault = 1;

			/*
			 * no softint_mxcc is triggered
			 */
			return;
		}
	}

	/*
	 * Indicate and error on this CPU.
	 */
	if (lock_try(&mxcc_detected[CPU->cpu_id])) {
		mxcc_curthread[CPU->cpu_id] = curthread;
		mxcc_psr[CPU->cpu_id] = rp->r_psr;
		mxcc_err_reg[CPU->cpu_id] = mxcc_err;
	}

	/*
	 * if it isn't already dispatched,
	 * kick off handler
	 */
	if (!disable_mxccfail && !handler_mxcc.detected &&
		lock_try(&handler_mxcc.detected)) {
		handler_mxcc.detected_cpu = CPU->cpu_id;
		TRIGGER_INTR(handler_mxcc);
	}
}

#ifdef DEBUG
uint_t intr15_count_ecc_ce[NCPU];
uint_t intr15_count_ecc_ue[NCPU];
uint_t intr15_count_sbi[NCPU];

#endif	/* DEBUG */

/*
 * look through the BW Interrupt Tables
 * to to see if an SBI or MQH sent us a level15.
 */
static void
intr15_xbus()
{
	ushort_t inttab0;
	uchar_t bus;

	uint_t cpu_id = CPU->cpu_id;

	/*
	 * check for SBI XPT errors (they're all reported on bus0)
	 */
	inttab0 = intr_get_table(0);	/* get entry 0 */
	if (inttab0 & INTTABLE_SBI_XPT) {
		/*
		 * Clear Interrupt Source Identifier.
		 */
		(void) intr_clear_table(0, INTTABLE_SBI_XPT);
		/*
		 * Clear local pending interrupt
		 */
		(void) intr_clear_pend_local(0xF);

		DEBUG_INC(intr15_count_sbi[cpu_id]);

		if (!disable_sbifail && !handler_sbi.detected &&
			lock_try(&handler_sbi.detected)) {
			handler_sbi.detected_cpu = cpu_id;
			/*
			 * handler_sbi.detected will be owned by the softint
			 */
			TRIGGER_INTR(handler_sbi);
		}
	}

	/*
	 * check both busses for ECC errors from the MQH's
	 * we will not see any CE's until memerr_init() is called,
	 * but we could see UE's before that.
	 */

	for (bus = 0; bus < n_xdbus; ++bus) {

		int (*get_table) (int entry) = intr_get_table;
		int (*clear_table) (int entry, int mask) = intr_clear_table;

		if (bus == 1) {
			get_table = intr_get_table_bwb;
			clear_table = intr_clear_table_bwb;
		}
		inttab0 = get_table(0);	/* get entry 0 */

		/*
		 * Handle the sources indicated in Interrupt Source Identifier
		 * Note that while the MQH checks parity over 64-bit memory
		 * words, it sends only 1 interrupt per 64-byte block.
		 * So we could get 1 interrupt for 8 ECC errors.
		 *
		 * We know which bus this came in on, but
		 * we throw that info away, as memerr polls both.
		 */

		if (inttab0 & (INTTABLE_MQH_UE_CE | INTTABLE_MQH_UE)) {
			DEBUG_INC(intr15_count_ecc_ue[cpu_id]);
			/*
			 * Clear Interrupt Source Identifier.
			 */
			(clear_table) (0, INTTABLE_MQH_UE_CE |
				INTTABLE_MQH_UE);

			/*
			 * Clear local pending interrupt
			 */
			(void) intr_clear_pend_local(0xF);

			if (!handler_ecc_ue.detected &&
				lock_try(&handler_ecc_ue.detected)) {
				handler_ecc_ue.detected_cpu = cpu_id;
				TRIGGER_INTR(handler_ecc_ue);
			}
		}
		if (inttab0 & (INTTABLE_MQH_CE | INTTABLE_MQH_CE)) {
			DEBUG_INC(intr15_count_ecc_ce[cpu_id]);
			/*
			 * Clear Interrupt Source Identifier.
			 */
			(clear_table) (0, INTTABLE_MQH_UE_CE |
				INTTABLE_MQH_CE);

			/*
			 * Clear local pending interrupt
			 */
			(void) intr_clear_pend_local(0xF);

			if (!handler_ecc_ce.detected &&
				lock_try(&handler_ecc_ce.detected)) {
				handler_ecc_ce.detected_cpu = cpu_id;
				TRIGGER_INTR(handler_ecc_ce);
			}
		}
	}
}

/*
 * Enable/Disable selected bbus interrupts on a cpu's Boot Bus
 * asserts that the given cpu_id indeed owns a Bootbus
 */
static void
bb_ctl_bits(uint_t bits, uint_t enable, int cpu_id)
{
	uchar_t old, new;

	ASSERT(CPU_IN_SET(bootbusses, cpu_id));

	old = bb_ctl_get_ecsr(cpu_id);
	if (enable == BITSET)
		new = (old | bits);
	else
		new = (old & ~bits);

#ifdef dont_print_from_15
	if (enable == 1)	/* print only for enable, b/c disable is @15 */
		cmn_err(CE_CONT, "bb_ctl_set_bits: cpu %d, bits 0x%x "
		    "old 0x%x new 0x%x\n", cpu_id, bits, old, new);
#endif	/* DEBUG */
	if (new != old)
		bb_ctl_set_ecsr(cpu_id, new);

}

/*
 * Enable/Disable selected bbus interrupts on All Boot Busses.
 * But only if the cpu for that BootBus is attached and CPU_ENABLEd
 * to take interrupts.
 * Requires that global "bootbusses" be initialized.
 */
void
bb_ctl_bits_all(uint_t bits, uint_t enable)
{
	uint_t cpu_id;
	uchar_t nfound = 0;

	for (cpu_id = 0; cpu_id < NCPU; ++cpu_id) {
		/* if cpu does not own a bootbus */
		if (!CPU_IN_SET(bootbusses, cpu_id))
			continue;

		if (!cpu[cpu_id] || !(cpu[cpu_id]->cpu_flags & CPU_ENABLE))
			continue;

		bb_ctl_bits(bits, enable, cpu_id);

		/* if seen all there is to see */
		if (++nfound == n_bootbus)
			break;
	}
}

/*
 * End code that runs at level15
 */

/*
 * syserr_handler()
 * called via softint to handle errors
 * calls the handlers in syserr_handlers[] as appropriate.
 */

lock_t syserr_req[16];

static lock_t syserr_active[16];

#ifdef DEBUG
static uint_t syserr_handler_called1[NCPU];
static uint_t syserr_handler_called4[NCPU];
static uint_t syserr_handler_called8[NCPU];
static uint_t syserr_handler_claimed1[NCPU];
static uint_t syserr_handler_claimed4[NCPU];
static uint_t syserr_handler_claimed8[NCPU];
static uint_t syserr_handler_loops1[NCPU];
static uint_t syserr_handler_loops4[NCPU];
static uint_t syserr_handler_loops8[NCPU];
#endif DEBUG

uint_t
syserr_handler(caddr_t ipl)
{
	uint_t pil = (uint_t)ipl;
	uint_t errors_found = 0;
	uchar_t check_again = 1;

#ifdef DEBUG
	if (pil == 1) DEBUG_INC(syserr_handler_called1[CPU->cpu_id]);
	else if (pil == 4) DEBUG_INC(syserr_handler_called4[CPU->cpu_id]);
	else if (pil == 8) DEBUG_INC(syserr_handler_called8[CPU->cpu_id]);
#endif DEBUG

	/*
	 * if there is no request for syserr_handler()
	 * to run at this level or it is already active,
	 * or we can't get the active lock, return;
	 */
	if (!syserr_req[pil] ||
		syserr_active[pil] || !lock_try(&syserr_active[pil]))
		return (1);	/* always return 1 (xxx why?) */

	/*
	 * else we've claimed the interrupt
	 * squelch the source (syserr_req[]) before
	 * we poll the handlers, in case another is
	 * detected while we're polling
	 */
	lock_clear(&syserr_req[pil]);

#ifdef DEBUG
	if (pil == 1) DEBUG_INC(syserr_handler_claimed1[CPU->cpu_id]);
	else if (pil == 4) DEBUG_INC(syserr_handler_claimed4[CPU->cpu_id]);
	else if (pil == 8) DEBUG_INC(syserr_handler_claimed8[CPU->cpu_id]);
#endif DEBUG

	/*
	 * loop through syserr_handlers[]
	 * dispatching handlers for errors at our level
	 * while there are errors at our level that are not handled
	 */

	DEBUG_LED(7);

	while (check_again) {
		int i;
		error_handler_t *h;

		check_again = 0;

		DEBUG_LED(9);
#ifdef DEBUG
		if (pil == 1)
			DEBUG_INC(syserr_handler_loops1[CPU->cpu_id]);
		else if (pil == 4)
			DEBUG_INC(syserr_handler_loops4[CPU->cpu_id]);
		else if (pil == 8)
			DEBUG_INC(syserr_handler_loops8[CPU->cpu_id]);
#endif DEBUG

		for (i = 0; NULL != (h = syserr_handlers[i]); ++i) {
			/*
			 * If handler is registered at our ipl, and
			 * if there is a failure detected for it, and
			 * if the handler is enabled, and
			 * if nobody is handling it, and
			 * if we can grab the lock to handle it
			 *
			 * call the handler
			 */
			if ((h->ipl == pil) &&
				h->detected &&
				h->enabled &&
				!(h->active) &&
				lock_try(&h->active)) {
				DEBUG_LED(0xa);

				/*
				 * note active_cpu for handler
				 */
				h->active_cpu = CPU->cpu_id;
				/*
				 * call the softint handler
				 * ignore its return code
				 */
				(void) (*h->handler)(h->arg);
				/*
				 * note active_cpu for handler
				 */
				h->active_cpu = NO_CPU;
				errors_found++;
				check_again = 1;
				/*
				 * note that softint handler now
				 * owns the handled lock
				 */
			}
		}

	}
	DEBUG_LED(LED_CPU_RESUME);

#ifdef LEVEL10_TEST
	cmn_err(CE_CONT, "syserr_handler() pil %d, %d errors handled\n",
		pil, errors_found);
#endif LEVEL10_TEST

	/*
	 * it is okay to clear the syserr_active[] lock now.
	 * if we got a new error after we already called the
	 * handler, then intr15_handler will set detected, and send
	 * us another interrupt at this pil.  If it is this cpu,
	 * then we'll get it when we return.  if it is another cpu,
	 * then they'll ignore it b/c we still have the active lock
	 * for this cpu.  but syserr_poll will save our butt by
	 * noticing a request with no active and will kick off a
	 * syserr_handler on itself.
	 */
	lock_clear(&syserr_active[pil]);

	return (1);	/* always return 1 (xxx why?) */
}

/*
 * syserr_poll()
 * intr15 will not return until it has squelched the pending level15
 * interrupt.  But the cpu may not be able to squelch the level15,
 * and thus it would never leave intr15, and thus never receive the softint
 * necessary to handle the error.
 *
 * syserr_poll() exists to detect this situation.
 * it is called by the clock ticker and it looks for error indications
 * in syserr_handlers[] which are not being handled.
 * if any are found, it sends a softint to itself at the appropriate level.
 *
 * yes, if we're not taking level10's then the system is dead,
 * but we'd notice that right away.
 */
void
syserr_poll(void)
{
	/*
	 * common case, no error handling requested
	 */
	if (!syserr_req[SOFTERR_IPL_HIGH] && !syserr_req[SOFTERR_IPL_MED] &&
		!syserr_req[SOFTERR_IPL_LOW])
		return;

	/*
	 * if syserr_handler() isn't already running at the requested
	 * level, then send an interrupt to self to kick it off.
	 */
	if (syserr_req[SOFTERR_IPL_HIGH] && !syserr_active[SOFTERR_IPL_HIGH]) {
#ifdef LEVEL10_TEST
		cmn_err(CE_CONT, "syserr_poll() ipl %d\n", SOFTERR_IPL_HIGH);
#endif LEVEL10_TEST
		xmit_cpu_intr(CPU->cpu_id, SOFTERR_IPL_HIGH);
	}

	if (syserr_req[SOFTERR_IPL_MED] && !syserr_active[SOFTERR_IPL_MED]) {
#ifdef LEVEL10_TEST
		cmn_err(CE_CONT, "syserr_poll() ipl %d\n", SOFTERR_IPL_MED);
#endif LEVEL10_TEST
		xmit_cpu_intr(CPU->cpu_id, SOFTERR_IPL_MED);
	}

	if (syserr_req[SOFTERR_IPL_LOW] && !syserr_active[SOFTERR_IPL_LOW]) {
#ifdef LEVEL10_TEST
		cmn_err(CE_CONT, "syserr_poll() ipl %d\n", SOFTERR_IPL_MED);
#endif LEVEL10_TEST
		xmit_cpu_intr(CPU->cpu_id, SOFTERR_IPL_LOW);
	}
}

#ifdef DEBUG
static uint_t softint_fan_called[NCPU];

#endif	/* DEBUG */

#ifdef DEBUG
static uint_t softint_temp_called[NCPU];

#endif	/* DEBUG */

#ifdef DEBUG
static uint_t softint_power_called[NCPU];

#endif	/* DEBUG */

/*
 * when we were called, there was a fan failure.
 * but it could have gone away.
 */

/*ARGSUSED*/
static int
softint_fan(caddr_t arg)
{
	static uint_t been_called;

	DEBUG_INC(softint_fan_called[CPU->cpu_id]);

	ASSERT(handler_fan.detected != 0);
	ASSERT(handler_fan.active != 0);

	/*
	 * If we haven't been muzzled by somebody setting disable_fanfail, we
	 * check the local BootBus to see if it is still failing. This is
	 * sufficient as the same fan wire goes to all the BootBusses.
	 */

	if (!disable_fanfail && (STATUS2_FAN_INT & xdb_bb_status2_get())) {
		/*
		 * Fan is still failing.
		 * Print a warning on the 1st 8 timeouts (8 sec apart),
		 * and every 8th timeout after that (64 sec. apart).
		 */

		if (been_called++ == 0) {
			cmn_err(CE_WARN, "FAN FAILURE, "
			    "Check that system fan is still spinning");
		} else {
			if (been_called <= 8 || !(been_called % 8))
				cmn_err(CE_WARN, "FAN FAILURE still sensed.");
		}

#ifdef SYNC_PANIC
		/*
		 * xxx change the sync interval until we figure out how to do
		 * that, simply call sync() from this timeout thread
		 */
		sync();
#endif	/* SYNC_PANIC */

		(void) timeout((void(*)(void *))softint_fan, NULL,
			(SOFTINT_FAN_TIMEOUT_SEC * hz));
	} else {
		been_called = 0;	/* for when it fails again */

		/*
		 * allow a softint to handle fan fail again.
		 */
		lock_clear(&handler_fan.active);

		/*
		 * Tell intr15_handler that it is done handling this error.
		 */
		lock_clear(&handler_fan.detected);

		if (disable_fanfail) {
			cmn_err(CE_WARN, "Fan Fail Warning Disabled.");
			/*
			 * return without re-enabling the interrupt
			 */
		}
		/*
		 * Fan fail signal went away perhaps somebody plugged it back
		 * in.
		 */
		cmn_err(CE_NOTE, "FAN RECOVERED");

		/*
		 * Re-enable Fan interrupt in case it happens again.
		 */
		bb_ctl_bits_all(BBUS_CTL_FAN_BIT, BITSET);
	}
	return (0);
}

/*
 * This function handles the dual power supply interrupt. It is only
 * enabled on SC2000 and SC2000+, since the SS1000 and SS1000+
 * use the hardware interrupt enable bit for the ethernet link
 * test enable function instead.
 */

/*ARGSUSED*/
static int
softint_power(caddr_t arg)
{
	static uint_t been_called;

	DEBUG_INC(softint_power_called[CPU->cpu_id]);

	ASSERT(handler_power.detected != 0);
	ASSERT(handler_power.active != 0);

	/*
	 * If we haven't been muzzled by somebody setting disable_power, we
	 * check the local BootBus to see if it is still failing. This is
	 * sufficient as the same power supply failure signal wire goes
	 * to all the BootBusses.
	 */

	if (!disable_power && (STATUS2_PWR_INT & xdb_bb_status2_get())) {
		/*
		 * Power is still failing.
		 * Print a warning on the 1st 8 timeouts (8 sec apart),
		 * and every 8th timeout after that (64 sec. apart).
		 */

		if (been_called++ == 0) {
			cmn_err(CE_WARN, "POWER SUPPLY FAILURE, "
			    "One of the redundant power supplies has failed");
		} else {
			if (been_called <= 8 || !(been_called % 8))
				cmn_err(CE_WARN,
					"POWER SUPPLY FAILURE still sensed.");
		}

		(void) timeout((void(*)(void *))softint_power, NULL,
			(SOFTINT_PWR_TIMEOUT_SEC * hz));
	} else {
		been_called = 0;	/* for when it fails again */

		/*
		 * allow a softint to handle power supply fail again.
		 */
		lock_clear(&handler_power.active);

		/*
		 * Tell intr15_handler that it is done handling this error.
		 */
		lock_clear(&handler_power.detected);

		if (disable_power) {
			cmn_err(CE_WARN, "Power Supply Fail Warning Disabled.");
			/*
			 * return without re-enabling the interrupt
			 */
		}
		/*
		 * Fan fail signal went away perhaps somebody plugged it back
		 * in.
		 */
		cmn_err(CE_NOTE, "POWER SUPPLY RECOVERED");

		/*
		 * Re-enable Fan interrupt in case it happens again.
		 */
		bb_ctl_bits_all(BBUS_CTL_POWER_BIT, BITSET);
	}
	return (0);
}
/*
 * softint_temp()
 */

/*ARGSUSED*/
static int
softint_temp(caddr_t arg)
{
	proc_t *initpp;

#ifdef DEBUG
	extern void drv_usecwait(clock_t usec);
#endif DEBUG

	DEBUG_INC(softint_temp_called[CPU->cpu_id]);

	/*
	 * we own both
	 * handler_temp.detected and handler_temp.active
	 */
	ASSERT(handler_temp.detected != 0);
	ASSERT(handler_temp.active != 0);

	/*
	 * if we're cookin' in the oven return with handler_temp.active
	 * held. (and don't un-mask the bbus temp interrupt.) this
	 * will prevent this bootbus from getting any more and prevent
	 * other bootbusses from handling any.
	 */
	if (temperature_chamber) {
		oven_timeout(NULL);
		return (0);
	}

	/*
	 * Note the board number in case a board has a bad sensor and needs to
	 * be fixed.
	 *
	 */
	cmn_err(CE_WARN, "TEMPERATURE FAILURE!\n\tdetected on board %d",
	    (CPU->cpu_id) / 2);

#ifdef SYNC_PANIC
	/*
	 * sync the disks Is there a better way to do this (like vfs_syncall())
	 * which will delay till the sync is done?
	 */
	sync();
#endif	/* SYNC_PANIC */

#ifdef DEBUG
	drv_usecwait(5000000);	/* give tester a chance to chill out */
#endif DEBUG

	/*
	 * when done, re-check to see if we still have high temp. If yes,
	 * shutdown, if no, continue as normal.
	 */

	if (!(xdb_bb_status2_get() & STATUS2_TMP_INT)) {
		/*
		 * XXX are we guaranteed to be on the same cpu? if no, then we
		 * can't reliably re-check the local temp fail bit, cause the
		 * bit is per-board, and they may not have all triggered.
		 */
		cmn_err(CE_NOTE, "Board %d, Temperature Recovered, "
		    "aborting shutdown.", CPU_ID_2_BOARD(CPU->cpu_id));

		lock_clear(&handler_temp.detected);
		lock_clear(&handler_temp.active);
		bb_ctl_bits_all(BBUS_CTL_TEMP_BIT, BITSET);
		return (0);
	}

	/*
	 * Send SIGPWR to init(1) it will run rc0, which will uadmin to
	 * powerdown.
	 */

	mutex_enter(&pidlock);
	initpp = prfind(P_INITPID);
	mutex_exit(&pidlock);

	/*
	 * If we're still booting and init(1) isn't set up yet, simply halt.
	 */
	if (initpp == NULL) {
		extern void halt(char *);
		cmn_err(CE_WARN, "Over Temperature");
		power_off();
		halt("Power off the System");	/* just in case */
	}

	/*
	 * else, graceful shutdown with inittab and all getting involved
	 */
	psignal(initpp, SIGPWR);

	/*
	 * RFE: kick off a sanity timeout panic in case the /etc/inittab
	 * or /etc/rc0 files are hosed.
	 */
	return (0);
}

/*
 * oven_timeout()
 *
 * This routine gets called by the softint_temp routine when an overtemp
 * interrupt occurs and the SS1000/SC2000 is running inside an
 * environmental chamber. It will check for and print NOTICEs about
 * which boards are getting overtemp interrupts. It will register
 * itself for another callback with the timeout variable. When no
 * more overtemp interrupts are being detected, the routine will
 * stop registering itself for a callback.
 */

/*ARGSUSED*/
static void
oven_timeout(void *arg)
{
	int cpu_id;
	static uint_t ot_detected_old;	/* bit mask for boards */
	uint_t ot_detected_new = 0;
	uint_t ot_detected_cnt = 0;

	/*
	 * Loop over all the CPUs and check if they own a bootbus. If so,
	 * check if they have the overtemp status set in their bootbus
	 * register. Record this in the ot_detected_new bit mask.
	 */
	for (cpu_id = 0; cpu_id < NCPU; cpu_id++) {
		/*
		 * Does CPU n own a bootbus?
		 */
		if (CPU_IN_SET(bootbusses, cpu_id)) {
			/*
			 * Does this bootbus have overtemp interrupts
			 * detected?
			 */
			if (bb_ecsr_read_stat2(cpu_id) & STATUS2_TMP_INT) {
				ot_detected_new |= (1 << (cpu_id/2));
				ot_detected_cnt++;
			}
		}
	}

	/*
	 * If the new and the old ot_detected masks do not match,
	 * then the state of one of the detectors has changed.
	 * In this case we will print out the summary of the
	 * information.
	 */
	if (ot_detected_old != ot_detected_new) {
		cmn_err(CE_NOTE, "Environmental Chamber Temperature Status");
		for (cpu_id = 0; cpu_id < NCPU; cpu_id++) {
			char *condition;

			/*
			 * Does CPU n own a bootbus?
			 */
			if (CPU_IN_SET(bootbusses, cpu_id)) {
				if ((1 << (cpu_id/2)) & ot_detected_new) {
					condition = "Over";
				} else {
					condition = "Normal";
				}
				cmn_err(CE_CONT, "\tSystem Board %d: %s "
					"Temperature\n", cpu_id/2, condition);
			}
		}
		/*
		 * If all bootbusses are registering overtemp
		 * interrupts, then notify the user of this
		 * fact.
		 */
		if (ot_detected_cnt == n_bootbus) {
			cmn_err(CE_NOTE,
				"All system boards over temperature");
		}
	}

	ot_detected_old = ot_detected_new;	/* update the bit mask */

	/*
	 * If all interrupts are cleared, then don't setup the timeout
	 * again. We only want to keep calling back this routine if
	 * the overtemp interrupts are still occurring.
	 */
	if (ot_detected_new == 0) {
		cmn_err(CE_NOTE,
			"All system boards at normal temperature");

		/*
		 * Allow all boards to receive overtemp interrupts
		 * once again. And clear lock on active.overtemp.
		 */
		lock_clear(&handler_temp.active);
		lock_clear(&handler_temp.detected);
		bb_ctl_bits_all(BBUS_CTL_TEMP_BIT, BITSET);
		return;
	}

	/*
	 * Set up the callback timeout
	 */
	(void) timeout(oven_timeout, NULL, oven_timeout_sec * hz);
}

#ifdef DEBUG
uint_t intr15_count_cc_ae;
uint_t intr15_count_cc_cp;
uint_t intr15_count_cc_vp;
uint_t intr15_count_cc_me;
#endif	/* DEBUG */

#ifdef DEBUG
static uint_t softint_mxcc_called[NCPU];

#endif	/* DEBUG */

/*
 * MXCC.ERR single bit values
 * note that %b bit numbers start with base, and are are [40:01] octal
 */

static char *mxcc_err_hi_bits = "\20\40ME\37XP\36CC\35VP\34CP\33AE\32EV\7S";

/*
 * Load up a string with a decoded MXCC error register.
 * No \n on the end
 */
void
mxcc_sprint_err(u_longlong_t mxcc_err, char *buf)
{
	uint_t ccop, dcmd, err_type, err;
	uint_t mxcc_err_hi = (uint_t)(mxcc_err >> 32);
	u_longlong_t pa = mxcc_err & ~((u_longlong_t)(~MXCC_ERR_PA) << 32);
	char *command, *ae_type;
	char tmp_buf[120];

	/*
	 * if !(Error Valid), don't bother with
	 * the CCOP, ERR, S, and PA
	 */
	if (!(mxcc_err_hi & MXCC_ERR_EV))
		return;

	(void) sprintf(buf, "MXCC.ERR=0x%b",
	    (long)mxcc_err_hi, mxcc_err_hi_bits);

	ccop = (mxcc_err_hi & MXCC_ERR_CCOP) >> MXCC_ERR_CCOP_SHFT;
	dcmd = (ccop >> 4);

	if (mxcc_err_hi & (MXCC_ERR_AE | MXCC_ERR_CP)) {
		switch (dcmd) {
		case DCMD_BC_MR:
				err_type = MXCC_ERR_AE;
				command = "BCOPY Memory Read";
				break;
		case DCMD_BC_MW:
				err_type = MXCC_ERR_AE;
				command = "BCOPY Memory Write";
				break;
		case DCMD_IOW:
				err_type = MXCC_ERR_AE;
				command = "Store to I/O Space";
				break;
		case DCMD_BC_IOR:
				err_type = MXCC_ERR_AE;
				command = "BCOPY I/O Read";
				break;
		case DCMD_BC_IOW:
				err_type = MXCC_ERR_AE;
				command = "BCOPY I/O Write";
				break;
		case DCMD_SBUS_SR:
				err_type = MXCC_ERR_CP;
				command = "SBus Device Stream Read";
				break;
		case DCMD_WB:
				err_type = MXCC_ERR_CP;
				command = "Ecache Writeback";
				break;
		case DCMD_CFR:
				err_type = MXCC_ERR_CP;
				command = "Consistent Foreign Read";
				break;
		case DCMD_FECSRR:
				err_type = MXCC_ERR_CP;
				command = "Foreign ECSR Cache Read";
				break;
		default:
				command = "Unknown";
				err_type = 0;
		}
	}

	err = (mxcc_err_hi & MXCC_ERR_ERR) >> MXCC_ERR_ERR_SHFT;

	if (err_type == MXCC_ERR_AE) {
		switch (err) {
		case 0x1:	ae_type = "UC - Uncorrectable Error";
				break;
		case 0x2:	ae_type = "TO - Time-Out";
				break;
		case 0x3:	ae_type = "BE - Bus Error";
				break;
		case 0x4:	ae_type = "UE - Undefined Error";
				break;
		default:
				ae_type = "Unknown";
		}
	}
	(void) sprintf(tmp_buf, "\n\tCCOP=0x%x<%s>", ccop, command);
	(void) strcat(buf, tmp_buf);

	if (err_type == MXCC_ERR_AE) {
		(void) sprintf(tmp_buf, "\n\tERR=0x%x<%s>", err, ae_type);
		(void) strcat(buf, tmp_buf);
	}

	/*
	 * if address is in SBus Space, identify offending SBus slot
	 */
	if ((pa & PA_SBUS_SPACE_MASK) == PA_SBUS_SPACE_BASE) {
		(void) sprintf(tmp_buf, "\n\tPA=0x%llx (System Board %d, SBus "
			"Slot %d, Offset 0x%x)", pa, PA_SBUS_TO_SYSBRD(pa),
			PA_SBUS_TO_SBUS_SLOT(pa), PA_SBUS_TO_SBUS_OFF(pa));
		(void) strcat(buf, tmp_buf);
	} else {
		(void) sprintf(tmp_buf, "\n\tPA=0x%llx", pa);
		(void) strcat(buf, tmp_buf);
	}
}

/*
 * softint_mxcc()
 */

/*ARGSUSED*/
static int
softint_mxcc(caddr_t arg)
{
	uint_t cur_cpu_id = CPU->cpu_id;
	uint_t err_cpu_id;

	DEBUG_INC(softint_mxcc_called[cur_cpu_id]);

	ASSERT(handler_mxcc.detected != 0);
	ASSERT(handler_mxcc.active != 0);

	/*
	 * Clear the .detected lock.
	 * If we detect another while .active, syserr_poll will call
	 * us again.
	 */
	lock_clear(&handler_mxcc.detected);

	/*
	 * for each CPU, process any recorded MXCC errors
	 */
	for (err_cpu_id = 0; err_cpu_id < NCPU; ++err_cpu_id) {
		u_longlong_t mxcc_err;
		uint_t mxcc_err_hi;
		uint_t ccop, dcmd;
		char mxcc_err_buf[256];

		/*
		 * if no MXCC error on CPU err_cpu_id
		 */
		if (!mxcc_detected[err_cpu_id])
			continue;

		mxcc_err = mxcc_err_reg[err_cpu_id];
		mxcc_err_hi = (uint_t)(mxcc_err >> 32);

		mxcc_sprint_err(mxcc_err, mxcc_err_buf);

		ccop = (mxcc_err_hi & MXCC_ERR_CCOP) >> MXCC_ERR_CCOP_SHFT;
		dcmd = (ccop >> 4);

		/*
		 * If HW bcopy got an ECC error on a memory read
		 * ask softint_ecc() if we should panic the system.
		 */
		if ((mxcc_err_hi & MXCC_ERR_AE) && (dcmd == DCMD_BC_MR)) {
			error_handler_t *h = &handler_ecc_ue_async;

			if (!h->detected && lock_try(&h->detected) &&
				!(h->active) && lock_try(&h->active)) {
				h->active_cpu = CPU->cpu_id;

				/*
				 * note that softint handler
				 * owns the job of clearing the handled lock
				 */
				if ((*h->handler)(h->arg))
					cmn_err(CE_PANIC, "%s", mxcc_err_buf);

				h->active_cpu = NO_CPU;
			}
#ifdef DEBUG
			else
				cmn_err(CE_WARN, "Huh?");
#endif DEBUG

			/*
			 * if make it here, then the UE has been handled.
			 */

			goto mxcc_done;
		}

		/*
		 * from here on out we're on shakey turf, get verbose
		 */
		cmn_err(CE_WARN, "Board %d Cpu%c (cpu%d)"
			" MXCC ERROR 0x%016llx\n%s",
			CPU_ID_2_BOARD(err_cpu_id),
			CPU_ID_IS_CPUB(err_cpu_id) ? 'B' : 'A',
			err_cpu_id, mxcc_err, mxcc_err_buf);

#ifdef useless_info
		if (mxcc_err_hi & MXCC_ERR_ME) {
			DEBUG_INC(intr15_count_cc_me);
			cmn_err(CE_CONT, "Multiple MXCC Errors.\n");
		}
#endif not
		if (mxcc_err_hi & MXCC_ERR_VP) {
			/*
			 * these are all synchronous
			 * so we shouldn't get here
			 */
			DEBUG_INC(intr15_count_cc_vp);
			cmn_err(CE_WARN, "SuperSPARC VBus Parity Error.");
		}
		if (mxcc_err_hi & MXCC_ERR_CP) {
			/* Cache Parity Error */
			/* Read By SBus stream Device */
			/* Write back by cache to memory */
			/* foreign read by other processor or SBus device */
			/* foreign read to cache data via ECSR space */

			/*
			 * If cache parity error is due to Write Back by cache
			 * to memory panic the system else print warning message
			 */
			DEBUG_INC(intr15_count_cc_cp);
			if (dcmd == DCMD_WB)
				cmn_err(CE_PANIC, "ECache Parity Error.");
			else
				cmn_err(CE_WARN, "ECache Parity Error.");
		}
		/*
		 * Asynchronous Error
		 * CPU Store to I/O Space
		 * BCOPY MW, IOR, IOW
		 *
		 * pokes never get here -- they're handled in intr15_mxcc().
		 * xxx assert (dcmd == DCMD_IOW)
		 */

		if (mxcc_err_hi & MXCC_ERR_AE) {
			proc_t *p;
			kthread_t *t;
			uint_t was_usermode = !(mxcc_psr[err_cpu_id] & PSR_PS);

			DEBUG_INC(intr15_count_cc_ae);

			t = mxcc_curthread[err_cpu_id];
			p = ttoproc(t);

			cmn_err(CE_WARN, "Process %d (%s): async error.",
				p->p_pid, p->p_user.u_comm);

			/*
			 * if we were in user mode when we took the error
			 * and it was a non-system process, kill it
			 */
			if (was_usermode && p && !(p->p_flag & SSYS)) {
				/* xxx dummy addr */
				kill_proc(t, p, (caddr_t)0);
			} else {
				cmn_err(CE_PANIC, "Kernel Mode Async Error.");
			}

		}

mxcc_done:
		mxcc_err_reg[err_cpu_id] = 0;	/* leave for entrails */
		lock_clear(&mxcc_detected[err_cpu_id]);
		lock_clear(&mxcc_active[err_cpu_id]);
	}
	lock_clear(&handler_mxcc.active);
	return (0);
}

#ifdef DEBUG
static uint_t softint_sbi_called[NCPU];

#endif	/* DEBUG */

/*
 * softint_sbi()
 * poll the SBI status registers looking for PTE Parity Errors (PPE).
 * log any that we find and clear the status bit.
 */

/*ARGSUSED*/
static int
softint_sbi(caddr_t arg)
{
	int i;

	DEBUG_INC(softint_sbi_called[CPU->cpu_id]);

	ASSERT(handler_sbi.detected != 0);
	ASSERT(handler_sbi.active != 0);

	/*
	 * We clear the .detected lock and do a single scan of the SBI's.
	 * If we detect another while .active, syserr_poll will call
	 * us again.
	 */
	lock_clear(&handler_sbi.detected);

	/*
	 * sift through the SBI's looking for the PPE
	 * bit set in a status register.
	 */
	for (i = 0; i < n_sbi; i++) {
		uint_t *addr;
		uint_t status;
		extern uint_t lda_2f(uint_t *addr);
		extern void sta_2f(uint_t value, uint_t *addr);

		addr =	(uint_t *)((sbi_devids[i] << SBI_DEVID_SHIFT) |
			(SBI_ECSR_BASE | SBI_OFF_STATUS));

		status = lda_2f(addr);
		if (status & SBI_STATUS_PPE) {
			cmn_err(CE_WARN, "Board %d: SBus PTE Parity Error.",
				sbi_board(sbi_devids[i]));
			sta_2f(SBI_STATUS_PPE, addr);
		}
	}
	lock_clear(&handler_sbi.active);
	return (0);
}

#ifdef DEBUG
static uint_t softint_ecc_called[NCPU];
static uint_t softint_ecc_loops;
static uint_t softint_ecc_spurious;
static uint_t softint_ecc_throttle;

#endif	/* DEBUG */
/*ARGSUSED*/
static int
softint_ecc(caddr_t type)
{
	uint_t check_again = 1;
	uint_t errors_found = 0;
	uint_t detected_cpu;
	error_handler_t *handler;

	DEBUG_INC(softint_ecc_called[CPU->cpu_id]);

	if ((uint_t)type & MEMERR_FATAL)
		handler = &handler_ecc_ue_async;
	else if ((uint_t)type & MEMERR_UE)
		handler = &handler_ecc_ue;
	else
		handler = &handler_ecc_ce;

	ASSERT(handler->detected != 0);
	ASSERT(handler->active != 0);
	ASSERT(handler->enabled != 0);

	/*
	 * There is an MQH bug which can cause a system watchdog reset.
	 * The bug occurrs when we read an MQH register while
	 * the MQH happens to be sending out an interrupt.
	 *
	 * to reduce the likelyhood that we run into this, we disable
	 * ECI before we call memerr() -- as memerr() will poll all
	 * the MQHs.  This doesn't address the possibility of a UE
	 * happening during the MQH poll -- you have to fix the MQH
	 * to address that possibility.
	 *
	 * memerr_ECI(1) will write to the CE_ADDR and thus clear
	 * any pre-existing CE's.  We can't poll for them after ECI = 1
	 * due to the MQH bug.  So there is a hole between when memerr()
	 * checks for a CE and when we re-enable ECI where we could get
	 * a CE and ignore it.  oh well.
	 */
	memerr_ECI(0);

	/*
	 * keep looking till we find no error
	 * this is necessary because until ECI is set to 1 again,
	 * we're ignoring any additional CE's that occur.
	 *
	 * throttle_ecc detects when we get ECC errors in the process
	 * of calling memerr().
	 * RFE - replace this with per-simm or perf-MQH throttle
	 * which memerr() can use to return 0 even if errors.
	 *
	 * clear detected lock.
	 * if we detect another error while we're running softint_ecc,
	 * intr15_xbus() will trigger a syserr_handler.  However,
	 * syserr_handler() will not call us while we hold the .active lock.
	 * if this happens, syserr_poll will call us when we drop .active.
	 */
	detected_cpu =	handler->detected_cpu;	/* used in DEBUG only */

	handler->detected_cpu = NO_CPU;
	lock_clear(&handler->detected);

	DEBUG_LED(0x3);
	while (check_again && (errors_found < ecc_throttle)) {
		DEBUG_INC(softint_ecc_loops);
		check_again = memerr((uint_t)type);
		errors_found += check_again;
	}
	DEBUG_LED(LED_CPU_RESUME);

#ifdef DEBUG
	if (errors_found >= ecc_throttle) {
		DEBUG_INC(softint_ecc_throttle);
		cmn_err(CE_CONT, "?cpu%d ecc_throttle = %d: %d times, "
			"loops %d, spur %d\n", detected_cpu, ecc_throttle,
			softint_ecc_throttle, softint_ecc_loops,
			softint_ecc_spurious);
	}
#endif DEBUG

	if (!errors_found) {
		DEBUG_INC(softint_ecc_spurious);
	}

	/*
	 * We have to exit this routine very carefully.
	 *
	 * we cleared the .detected lock before re-enabling ECI
	 * so if we take any CE's while exiting this routine,
	 * intr15_xbus() will send another softint.
	 *
	 * we clear the .active lock after the .detected lock
	 * so that syserr_poll doesn't re-call us right away;
	 * and before re-enabling ECI so that we don't detect
	 * and error and drop the resulting softint on the
	 * floor b/c it is already "active".
	 */

	/*
	 * allow syserr_handler() to call us again
	 */
	lock_clear(&handler->active);

#ifdef MEMERR_DEBUG
	cmn_err(CE_CONT,
		"!cpu%d softint_ecc: called %d errors %d, loops %d spur %d\n",
		CPU->cpu_id, softint_ecc_called[CPU->cpu_id],
		errors_found, softint_ecc_loops, softint_ecc_spurious);
#endif MEMERR_DEBUG

	/*
	 * enable level15's for CE's (after a while)
	 */
	(void) delay(ECI_DELAY);
	memerr_ECI(1);

	return (errors_found);
}

/*
 * Called on unrecoverable bus errors at user level.
 * Send SIGBUS to the current LWP.
 */
void
kill_proc(kthread_t *t, proc_t *p, caddr_t addr)
{
	k_siginfo_t siginfo;

	bzero((caddr_t)&siginfo, sizeof (siginfo));
	siginfo.si_signo = SIGBUS;
	siginfo.si_code = FC_HWERR;
	siginfo.si_addr = addr;
	mutex_enter(&p->p_lock);
	sigaddq(p, t, &siginfo, KM_NOSLEEP);
	mutex_exit(&p->p_lock);
}
