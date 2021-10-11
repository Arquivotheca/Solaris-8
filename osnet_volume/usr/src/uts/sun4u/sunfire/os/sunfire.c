/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)sunfire.c	1.19	99/10/22 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/kobj.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/mem_cage.h>

#include <sys/platform_module.h>
#include <sys/errno.h>

/*
 * By default the DR Cage is disabled for maximum
 * OS performance.  Users wishing to utilize DR must
 * specifically turn on this variable via /etc/system.
 */
int	kernel_cage_enable;

/* Let user disable overtemp cpu power off */
int enable_overtemp_cpu_poweroff = 1;

/* Let user disabled cpu power on */
int enable_cpu_poweron = 1;

/* Preallocation of spare tsb's for DR */
int sunfire_tsb_spares = 32;

/* Set the maximum number of boards... for DR */
int sunfire_boards = MAX_BOARDS;

/* Let user disable Dynamic Reconfiguration */
extern int enable_dynamic_reconfiguration;

/* Preferred minimum cage size (expressed in pages)... for DR */
pgcnt_t sunfire_minimum_cage_size = 0;

static int get_platform_slot_info();

int
set_platform_max_ncpus(void)
{
	int slots;

	sunfire_boards = MIN(sunfire_boards, MAX_BOARDS);

	slots = get_platform_slot_info();
	if (slots)
		sunfire_boards = slots;

	if (sunfire_boards < 1)
		sunfire_boards = 1;

	return ((sunfire_boards-1)*2);
}

/*
 * probe for the slot information of the system
 */
static int
get_platform_slot_info()
{
	u_char status1, clk_ver;
	int slots = 0;

	status1 = ldbphysio(SYS_STATUS1_PADDR);
	clk_ver = ldbphysio(CLK_VERSION_REG_PADDR);

	/*
	 * calculate the number of slots on this system
	 */
	switch (SYS_TYPE(status1)) {
	case SYS_16_SLOT:
		slots = 16;
		break;

	case SYS_8_SLOT:
		slots = 8;
		break;

	case SYS_4_SLOT:
		if (SYS_TYPE2(clk_ver) == SYS_PLUS_SYSTEM) {
			slots = 5;
		} else {
			slots = 4;
		}
		break;
	}

	return (slots);
}

int
set_platform_tsb_spares()
{
	return (MIN(sunfire_tsb_spares, MAX_UPA));
}

void
set_platform_defaults(void)
{
	extern short max_ce_err;
	extern int   ce_enable_verbose;

	/*
	 * Originally for SunFire ce_verbose = report_ce_console
	 * = report_ce_log = 1.  This turned out to be too verbose
	 * causing confusion in the field.  Instead let's use a
	 * modified desktop behavior.  Setting max_ce_err = 5
	 * should only print out UNUM's when there really is cause
	 * for concern.  If we do receive more than 5 CE's on a
	 * certain SIMM then let's go back to the old verbose
	 * behavior.
	 */

	max_ce_err = 5;
	ce_enable_verbose = 1;

	/* Check the firmware for CPU poweroff support */
	if (prom_test("SUNW,Ultra-Enterprise,cpu-off") != 0) {
		cmn_err(CE_WARN, "Firmware does not support CPU power off");
		enable_overtemp_cpu_poweroff = 0;
	}

	/* Also check site-settable enable flag for power down support */
	if (enable_overtemp_cpu_poweroff == 0)
		cmn_err(CE_WARN, "Automatic CPU shutdown on over-temperature "
		    "disabled");

	/* Check the firmware for CPU poweron support */
	if (prom_test("SUNW,wakeup-cpu") != 0) {
		cmn_err(CE_WARN, "Firmware does not support CPU restart "
		    "from power off");
		enable_cpu_poweron = 0;
	}

	/* Also check site-settable enable flag for power on support */
	if (enable_cpu_poweron == 0)
		cmn_err(CE_WARN, "The ability to restart individual CPUs "
		    "disabled");

	/* Check the firmware for CPU poweroff support */
	if (prom_test("SUNW,Ultra-Enterprise,rm-brd") != 0) {
		cmn_err(CE_WARN, "Firmware does not support"
			" Dynamic Reconfiguration");
		enable_dynamic_reconfiguration = 0;
	}
}

char *sunfire_drivers[] = {
	"ac",
	"sysctrl",
	"environ",
	"simmstat",
	"sram",
};

int must_load_sunfire_modules = 1;

void
load_platform_drivers(void)
{
	int i;
	char *c = NULL;
	char buf[128];

	buf[0] = '\0';

	for (i = 0; i < (sizeof (sunfire_drivers) / sizeof (char *)); i++) {
		if ((modload("drv", sunfire_drivers[i]) < 0) ||
		    (ddi_install_driver(sunfire_drivers[i]) != DDI_SUCCESS)) {
			(void) strcat(buf, sunfire_drivers[i]);
			c = strcat(buf, ",");
		}
	}

	if (c) {
		c = strrchr(buf, ',');
		*c = '\0';
		cmn_err(must_load_sunfire_modules ? CE_PANIC : CE_WARN,
		    "Cannot load the [%s] system module(s) which "
		    "monitor hardware including temperature, "
		    "power supplies, and fans", buf);
	}
}

int
plat_cpu_poweron(struct cpu *cp)
{
	int (*sunfire_cpu_poweron)(struct cpu *) = NULL;

	sunfire_cpu_poweron =
	    (int (*)(struct cpu *))kobj_getsymvalue("fhc_cpu_poweron", 0);

	if (enable_cpu_poweron == 0 || sunfire_cpu_poweron == NULL)
		return (ENOTSUP);
	else
		return ((sunfire_cpu_poweron)(cp));
}

int
plat_cpu_poweroff(struct cpu *cp)
{
	int (*sunfire_cpu_poweroff)(struct cpu *) = NULL;

	sunfire_cpu_poweroff =
	    (int (*)(struct cpu *))kobj_getsymvalue("fhc_cpu_poweroff", 0);

	if (enable_overtemp_cpu_poweroff == 0 || sunfire_cpu_poweroff == NULL)
		return (ENOTSUP);
	else
		return ((sunfire_cpu_poweroff)(cp));
}

#ifdef DEBUG
pgcnt_t sunfire_cage_size_limit;
#endif

void
set_platform_cage_params(void)
{
	extern pgcnt_t total_pages;
	extern struct memlist *phys_avail;
	int ret;

	if (kernel_cage_enable) {
		pgcnt_t preferred_cage_size;

		preferred_cage_size =
			MAX(sunfire_minimum_cage_size, total_pages / 256);
#ifdef DEBUG
		if (sunfire_cage_size_limit)
			preferred_cage_size = sunfire_cage_size_limit;
#endif
		kcage_range_lock();
		/*
		 * Note: we are assuming that post has load the
		 * whole show in to the high end of memory. Having
		 * taken this leap, we copy the whole of phys_avail
		 * the glist and arrange for the cage to grow
		 * downward (descending pfns).
		 */
		ret = kcage_range_init(phys_avail, 1);
		if (ret == 0)
			kcage_init(preferred_cage_size);
		kcage_range_unlock();
	}

	if (kcage_on)
		cmn_err(CE_NOTE, "!DR Kernel Cage is ENABLED");
	else
		cmn_err(CE_NOTE, "!DR Kernel Cage is DISABLED");
}

/*ARGSUSED*/
void
plat_freelist_process(int mnode)
{
}

/*
 * No platform pm drivers on this platform
 */
char *platform_pm_module_list[] = {
	(char *)0
};

void
plat_tod_fault(enum tod_fault_type tod_bad)
{
extern void fhc_tod_fault(enum tod_fault_type);

#pragma	weak	fhc_tod_fault

	fhc_tod_fault(tod_bad);
}
