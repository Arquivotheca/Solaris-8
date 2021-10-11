/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)led.c	1.10	99/06/05 SMI"

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/led.h>
#include <sys/systm.h>
#include <sys/cyclic.h>

/*
 * The documentation shall say only
 * "The amber LEDS move when Unix is running."
 *
 * Here is what they really do.
 *
 * On power-up, the hardware initializes the 8 amber LEDS's on each board
 * to zero (all on).  On boards with CPU's, POST will output various patterns,
 * and when complete, will leave the LEDS set to zero (all on) and set the
 * green board LEDS for each CPU that passed POST.
 *
 * If POST finds an error, it will turn on the front pannel amber LED.
 * If it configures a CPU out of the system, the CPU's green board LED
 * will not be turned on.  If it configures a board out of the system,
 * it will leave a pattern on the amber LEDS on that board.
 * This pattern will persist, because Unix is not aware of the failed
 * board so it will not write to the failed board's LEDs.
 *
 * When OBP takes over, it will update the amber LEDs on the
 * OBP master's board.  Its pattern is 1 LED on, repeatedly scrolling from
 * bottom to top. (left to right on SC1000)
 *
 * When the kernel starts running, it will flash a pattern to the boot cpu's
 * LED's, but OBP is still in charge, so this pattern will scroll away.
 *
 * In startup(), the kernel will begin flashing the boot_pattern
 * (alternately 0x55 and 0xaa) on all boards.  This is from explicit
 * calls to led_blink_all().  When we mount root and spl0, level10's
 * interrupts start arriving and the flashing gets real fast.
 *
 * After the cyclic subsystem starts up, we add a CY_LOCK_LEVEL cyclic to
 * call led_blink_all() at a rate of 25 Hz.
 *
 * Once the cpu driver is attached, the cpu[] array gets filled in
 * and led_blink_all() writes patterns depending on the states of the cpus.
 * Each CPU gets 4 amber LEDS.  If a CPU is not present, then its 4 LEDS
 * are always set to be on.  Else if the CPU is idle, they cycle through
 * ledpat[] at 25 Hz.  Else if the CPU is not idle, they cycle through
 * ledpat[] at 25 Hz/LED_TICKS2 (1.3Hz)
 *
 * In summary...
 * If a board has no CPUs present, the LED's default to all on.
 * If a board has 1 cpu vacant, then that is indicated by 4 leds on.
 * If no leds move in the span of 1sec. then your machine is wedged.
 */

/*
 * External Data
 */
extern u_int n_bootbus;		/* number of BootBusses on system */
extern u_int bootbusses;		/* CPUSET of cpus owning bootbusses */

u_int leds;		/* CPUSET of enabled LEDS */
cyclic_id_t led_cyclic;

/*
 * Static Routines:
 */
static void bbus_led_update(u_int bbus_id);
static void bbus_led_boot(u_int bbus_id);

/*
 * Static Data:
 */

static u_char ledpat[] = {
	0x7,			/* #... */
	0xB,			/* .#.. */
	0xD,			/* ..#. */
	0xE,			/* ...# */
	0xD,			/* ..#. */
	0xB,			/* .#.. */
};

static u_char led_ptr;
static u_int led_idle_cpus;
static u_char update_idle_only;

static u_char led_cpu_pattern[NCPU];
static u_char led_cpu_pat_on = 1;


#define	LED_ON_PATTERN	0x0	/* #### */
#define	LED_OFF_PATTERN	0xf	/* .... */
#define	LED_QUIESCED_PATTERN 0x9	/* .##. */
#define	LED_PATTERNS	sizeof (ledpat)	/* 6 */
#define	LED_TICKS2	((3 * LED_PATTERNS) + 1)	/* 19 */

static u_char soft_leds[NCPU];	/* at least 1/2 are unused */
static u_char boot_pattern = 0x55;	/* #.#.#.#. */

/*
 * initialize the global "leds"
 * if a bit is set for a cpu in leds, then the LEDS associated with
 * that cpu will be moved by led_blink_all().
 *
 * in the beginning leds simply reflects the non-null intries in cpu[]
 * ie. the cpus that exist.
 */

void
led_init(void)
{
	u_int cpu_id;
	cyc_handler_t hdlr;
	cyc_time_t when;

	for (cpu_id = 0; cpu_id < NCPU; ++cpu_id) {
		if (cpu[cpu_id])
			CPUSET_ADD(leds, cpu_id);
	}

	mutex_enter(&cpu_lock);

	hdlr.cyh_level = CY_LOCK_LEVEL;
	hdlr.cyh_func = (cyc_func_t)led_blink_all;
	when.cyt_when = 0;
	when.cyt_interval = NANOSEC / 25;

	led_cyclic = cyclic_add(&hdlr, &when);

	mutex_exit(&cpu_lock);
}

void
led_set_cpu(u_char cpu_id, u_char cpu_pat)
{
	led_cpu_pattern[cpu_id] = cpu_pat & 0xF;

#ifdef NOT
	if (cpu_pat == LED_CPU_RESUME)
		return;
#endif NOT

	led_cpu_pat_on = 1;

	if (CPU_IN_SET(bootbusses, cpu_id))
		bbus_led_update(cpu_id);
	else
		bbus_led_update(cpu_id ^ 0x1);
}

/*
 * for the given bootbus
 * determine patterns for the cpu[s].
 * if pattern is different from soft_leds[],
 * then write it and save to soft_leds[]
 */
static void
bbus_led_update(u_int bbus_id)
{
	u_char pattern = 0;
	u_char cpu_id;
	u_int update_all = !update_idle_only;

	/*
	 * if called with bad bootbus
	 */
	if (!CPU_IN_SET(bootbusses, bbus_id))
		return;

#ifdef later
	/*
	 * if we're not updating all the LEDS, or if neither of the
	 * CPUs on this bootbus are idle, then skip it.
	 */
	if (!(!update_idle_only ||
		(led_idle_cpus & (0x3 << (bbus_id ^ 1)))))
		continue;
#endif later
	cpu_id = bbus_id & ~1;	/* cpuA */

	/*
	 * if we haven't called led_init() yet
	 * and we have no cpu_pattern's to blink
	 */
	if (!leds && !led_cpu_pattern[cpu_id] && !led_cpu_pattern[cpu_id + 1]) {
		bbus_led_boot(bbus_id);
		return;
	}

	/*
	 * determine pattern to write to LED
	 * if cpu doesn't exist, LED_ON_PATTERN;
	 * else if debug pattern, blink it;
	 * else if exists but disabled: LED_QUIESCED_PATTERN;
	 * else use moving pattern.
	 */
	if (!CPU_IN_SET(leds, cpu_id))
		pattern |= LED_ON_PATTERN << 4;
	else if (led_cpu_pattern[cpu_id]) {
		if (led_cpu_pat_on)
			pattern |= ((~led_cpu_pattern[cpu_id]) & 0xF) << 4;
		else
			pattern |= LED_OFF_PATTERN << 4;
	} else if (0 == (cpu[cpu_id]->cpu_flags & CPU_ENABLE)) {
		pattern |= LED_QUIESCED_PATTERN << 4;
	} else {
		/*
		 * if idle, update, else re-use old pattern
		 */
		if (update_all || CPU_IN_SET(led_idle_cpus, cpu_id))
			pattern |= ledpat[led_ptr] << 4;
		else
			pattern |= soft_leds[bbus_id] & 0xF0;

		if (update_all) {
			kthread_t *t = cpu[cpu_id]->cpu_thread;
			kthread_t *idle = cpu[cpu_id]->cpu_idle_thread;

			if (t == idle || (t == curthread && t->t_intr == idle))
				CPUSET_ADD(led_idle_cpus, cpu_id);
		}
	}

	/*
	 * cpu B
	 */

	cpu_id += 1;

	if (!CPU_IN_SET(leds, cpu_id))
		pattern |= LED_ON_PATTERN;
	else if (led_cpu_pattern[cpu_id]) {
		if (led_cpu_pat_on)
			pattern |= ((~led_cpu_pattern[cpu_id]) & 0xF);
		else
			pattern |= LED_OFF_PATTERN;
	} else if (0 == (cpu[cpu_id]->cpu_flags & CPU_ENABLE)) {
		pattern |= LED_QUIESCED_PATTERN;
	} else {
		/*
		 * if idle, update, else re-use old pattern
		 */
		if (update_all || CPU_IN_SET(led_idle_cpus, cpu_id))
		pattern |= ledpat[led_ptr];
		else
			pattern |= soft_leds[bbus_id] & 0xF;

		if (update_all) {
			kthread_t *t = cpu[cpu_id]->cpu_thread;
			kthread_t *idle = cpu[cpu_id]->cpu_idle_thread;

			if (t == idle || (t == curthread && t->t_intr == idle))
				CPUSET_ADD(led_idle_cpus, cpu_id);
		}
	}
	/*
	 * stash pattern, write to LEDS
	 */
	soft_leds[bbus_id] = pattern;

	led_set_ecsr((int)bbus_id, pattern);

}

/*
 * toggle boot_pattern, stash to soft_leds[]
 * and write it out to LEDs
 */
static void
bbus_led_boot(u_int bbus_id)
{
	boot_pattern = ~boot_pattern;
	soft_leds[bbus_id] = boot_pattern;
	led_set_ecsr((int)bbus_id, boot_pattern);
}

void
led_blink_all(void)
{
	u_int bbus_id;
	u_char nfound = 0;

	/*
	 * if we're updating all, we can forget which were idle
	 */
	if (!update_idle_only) {
		led_idle_cpus = 0;
	}

	led_cpu_pat_on = !led_cpu_pat_on;

	for (bbus_id = 0; bbus_id < NCPU; ++bbus_id) {
		/*
		 * does this cpu not own a bootbus?
		 */
		if (!CPU_IN_SET(bootbusses, bbus_id))
			continue;

		bbus_led_update(bbus_id);

		nfound += 1;

		/*
		 * if seen all there is to see
		 */
		if (nfound == n_bootbus)
			break;

	}			/* end for */

	/*
	 * update led_ptr for a new ledpat[] entry update_idle_only for next
	 * LED_TICKS2 times
	 */

	if (++led_ptr >= LED_PATTERNS)
		led_ptr = 0;

	if (++update_idle_only >= LED_TICKS2)
		update_idle_only = 0;
}
