/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_JAV_ENVCTRLTWO_H
#define	_JAV_ENVCTRLTWO_H

#pragma ident	"@(#)envctrltwo.h	1.1	98/01/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

struct envctrlunit {
	struct envctrl_pcd8584_regs *bus_ctl_regs;
	ddi_acc_handle_t ctlr_handle;
	kmutex_t umutex;			/* lock for this structure */
	int instance;
	dev_info_t *dip;			/* device information */
	struct envctrl_ps2 ps_kstats[ENVCTRL_MAX_DEVS];	/* kstats for ps */
	struct envctrl_fan fan_kstats; 		/* kstats for fans */
	struct envctrl_encl encl_kstats;		/* kstats for FSP */
	struct envctrl_temp temp_kstats[ENVCTRL_MAX_DEVS]; /* tempreratures */
	struct envctrl_disk disk_kstats[ENVCTRL_MAX_DEVS]; /* disks */
	int cpu_pr_location[ENVCTRL_MAX_CPUS]; /* slot true if cpu present */
	uint_t num_fans_present;
	uint_t num_ps_present;
	uint_t num_encl_present;
	uint_t num_cpus_present;
	uint_t num_temps_present;
	uint_t num_disks_present;
	kstat_t *psksp;
	kstat_t *fanksp;
	kstat_t *enclksp;
	kstat_t *tempksp;
	kstat_t *diskksp;
	ddi_iblock_cookie_t ic_trap_cookie;	/* interrupt cookie */
	/*  CPR support */
	boolean_t suspended;			/* TRUE if driver suspended */
	boolean_t oflag;			/*  already open */
	int current_mode;			/* NORMAL or DIAG_MODE */
	timeout_id_t timeout_id;				/* timeout id */
	timeout_id_t pshotplug_id;			/* ps poll id */
	int activity_led_blink;
	int present_led_state; 			/* is it on or off?? */
	timeout_id_t blink_timeout_id;
	int initting; /* 1 is TRUE , 0 is FALSE , used to mask intrs */
	boolean_t shutdown; /* TRUE = power off in error event */
	boolean_t fan_failed; /* TRUE = fan failure detected */
	boolean_t tempr_warning; /* TRUE = thermal warning detected */
};

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _JAV_ENVCTRLTWO_H */
