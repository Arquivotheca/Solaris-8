/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SUNPM_H
#define	_SYS_SUNPM_H

#pragma ident	"@(#)sunpm.h	1.6	99/10/11 SMI"

/*
 * Sun Specific Power Management definitions
 */

#include <sys/isa_defs.h>
#include <sys/dditypes.h>
#include <sys/ddipropdefs.h>
#include <sys/devops.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/ddidevmap.h>
#include <sys/ddi_implfuncs.h>
#include <sys/ddi_isa.h>
#include <sys/model.h>
#include <sys/devctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * Power cycle transition check is supported for these devices.
 */
#define	DC_SCSI_FORMAT		0x1		/* SCSI */

#define	DC_SCSI_MFR_LEN		6		/* YYYYWW */

struct pm_scsi_cycles {
	int	lifemax;			/* lifetime max power cycles */
	int	ncycles;			/* number of cycles so far */
	char	svc_date[DC_SCSI_MFR_LEN];	/* service date YYYYWW */
	int	flag;				/* reserved for future */
};

struct pm_trans_data {
	int	format;				/* data format */
	union {
		struct pm_scsi_cycles scsi_cycles;
	} un;
};

/*
 * Power levels for devices supporting ACPI based D0, D1, D2, D3 states.
 *
 * Note that 0 is off in Solaris PM framework but D0 is full power
 * for these devices.
 */
#define	PM_LEVEL_D3		0	/* D3 state - off */
#define	PM_LEVEL_D2		1	/* D2 state */
#define	PM_LEVEL_D1		2	/* D1 state */
#define	PM_LEVEL_D0		3	/* D0 state - fully on */

/*
 * Useful strings for creating pm-components property for these devices.
 * If a device driver wishes to provide more specific description of power
 * levels (highly recommended), it should NOT use following generic defines.
 */
#define	PM_LEVEL_D3_STR		"0=Device D3 State"
#define	PM_LEVEL_D2_STR		"1=Device D2 State"
#define	PM_LEVEL_D1_STR		"2=Device D1 State"
#define	PM_LEVEL_D0_STR		"3=Device D0 State"

/*
 * If you add or remove a function or data reference, please
 * remember to duplicate the action below the #else clause for
 * __STDC__.
 */

#ifdef	__STDC__

/*
 * Generic Sun PM definitions.
 */

/*
 * These are obsolete power management interfaces, they will be removed from
 * a subsequent release.
 */
int
pm_create_components(dev_info_t *dip, int num_components);

void
pm_destroy_components(dev_info_t *dip);

void
pm_set_normal_power(dev_info_t *dip, int component_number, int level);

int
pm_get_normal_power(dev_info_t *dip, int component_number);

/*
 * These are power management interfaces.
 */

int
pm_busy_component(dev_info_t *dip, int component_number);

int
pm_idle_component(dev_info_t *dip, int component_number);

int
pm_get_current_power(dev_info_t *dip, int component, int *levelp);

int
pm_power_has_changed(dev_info_t *, int, int);

int
pm_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t o, void *a, void *v);

int
pm_trans_check(struct pm_trans_data *datap, time_t *intervalp);

int
pm_lower_power(dev_info_t *dip, int comp, int level);

int
pm_raise_power(dev_info_t *dip, int comp, int level);

#else	/* __STDC__ */

/*
 * Obsolete interfaces.
 */
extern int pm_create_components();
extern void pm_destroy_components();
extern void pm_set_normal_power();
extern int pm_get_normal_power();

/*
 * PM interfaces
 */
extern int pm_busy_component();
extern int pm_idle_component();
extern int pm_get_current_power();
extern int pm_power_has_changed();
extern int pm_ctlops();
extern int pm_trans_check();
extern int pm_lower_power();
extern int pm_raise_power();

#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SUNPM_H */
