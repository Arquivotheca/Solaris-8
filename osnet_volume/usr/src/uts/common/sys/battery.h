/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_BATTERY_H
#define	_SYS_BATTERY_H

#pragma ident	"@(#)battery.h	1.3	95/02/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * battery.h:	Declarations for the common battery interface.
 *	It is expected that any module supporting /dev/battery
 *	will support the following ioctls. When the BATT_STATUS
 *	ioctl is used, a module may return -1 for any fields which
 *	are not known.
 */

#define	BATT_IDSTR_LEN	40

/*
 * Generic ioctls
 */
typedef enum {
	BATT_STATUS = 0,	/* Module will return a battery_t structure */
	BATT_ESCAPE		/* Module specific */
} batt_ioctl_t;

/*
 * Response fields
 */
typedef enum {
	NOT_PRESENT = 0,
	EMPTY,			/* Battery has (effectively) no capacity */
	LOW_CAPACITY,		/* Battery has less than 25% capacity */
	MED_CAPACITY,		/* Battery has less than 50% capacity */
	HIGH_CAPACITY,		/* Battery has less than 75% capacity */
	FULL_CAPACITY,		/* Battery has more than 75% capacity */
	EOL			/* Battery is dead */
} batt_status_t;

typedef enum {
	DISCHARGE = 0,		/* Battery is discharging (i.e. in use) */
	FULL_CHARGE,		/* Battery is charging at its fastest rate */
	TRICKLE_CHARGE		/* Battery is charging at a slower rate */
} batt_charge_t;

typedef struct {
	char		id_string[BATT_IDSTR_LEN];
	int		total;		/* Total capacity (mWhrs) */
	char		capacity;	/* Current capacity (percentage) */
	int		discharge_rate;	/* Current discharge rate (mW) */
	int		discharge_time;	/* Discharge time at current rate (s) */
	batt_status_t	status;		/* General battery status */
	batt_charge_t	charge;		/* Current charging condition */
} battery_t;


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BATTERY_H */
