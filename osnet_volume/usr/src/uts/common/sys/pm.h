/*
 * Copyright (c) 1993 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PM_H
#define	_SYS_PM_H

#pragma ident	"@(#)pm.h	1.19	99/10/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 *	The following ioctls may not exist or may have a different
 *	interpretation in a future release.
 */

typedef enum {
	PM_SCHEDULE,		/* obsolete, not supported */
	PM_GET_IDLE_TIME,	/* obsolete, not supported  */
	PM_GET_NUM_CMPTS,	/* obsolete, not supported */
	PM_GET_THRESHOLD,	/* obsolete, not supported  */
	PM_SET_THRESHOLD,	/* obsolete */
	PM_GET_NORM_PWR,	/* obsolete */
	PM_SET_CUR_PWR,		/* obsolete */
	PM_GET_CUR_PWR,		/* obsolete */
	PM_GET_NUM_DEPS,	/* obsolete, not supported */
	PM_GET_DEP,		/* obsolete, not supported */
	PM_ADD_DEP,		/* obsolete */
	PM_REM_DEP,		/* obsolete, not supported */
	PM_REM_DEVICE,		/* obsolete, not supported */
	PM_REM_DEVICES,		/* obsolete */
	PM_REPARSE_PM_PROPS,	/* used only by ddivs pm tests */
	PM_DISABLE_AUTOPM,	/* obsolete */
	PM_REENABLE_AUTOPM,	/* obsolete */
	PM_SET_NORM_PWR,	/* obsolete, not supported */
	PM_SET_DEVICE_THRESHOLD,
	PM_GET_SYSTEM_THRESHOLD,
	PM_SET_SYSTEM_THRESHOLD,
	PM_START_PM,
	PM_STOP_PM,
	PM_RESET_PM,
	PM_GET_STATS,
	PM_GET_DEVICE_THRESHOLD,
	PM_GET_POWER_NAME,
	PM_GET_POWER_LEVELS,
	PM_GET_NUM_COMPONENTS,
	PM_GET_COMPONENT_NAME,
	PM_GET_NUM_POWER_LEVELS,
	PM_GET_STATE_CHANGE,
	PM_GET_STATE_CHANGE_WAIT,
	PM_DIRECT_PM,
	PM_RELEASE_DIRECT_PM,
	PM_DIRECT_NOTIFY,
	PM_DIRECT_NOTIFY_WAIT,
	PM_RESET_DEVICE_THRESHOLD,
	PM_GET_PM_STATE,
	PM_GET_DEVICE_TYPE,
	PM_SET_COMPONENT_THRESHOLDS,
	PM_GET_COMPONENT_THRESHOLDS,
	PM_IDLE_DOWN,
	PM_GET_DEVICE_THRESHOLD_BASIS,
	PM_SET_CURRENT_POWER,	/* replaces PM_SET_CUR_PWR */
	PM_GET_CURRENT_POWER,	/* replaces PM_GET_CUR_PWR */
	PM_GET_FULL_POWER,	/* replaces PM_GET_NORM_PWR */
	PM_ADD_DEPENDENT,	/* replaces PM_ADD_DEP */
	PM_GET_TIME_IDLE,	/* replaces PM_IDLE_TIME */
	PM_GET_DEFAULT_SYSTEM_THRESHOLD
} pm_cmds;

/*
 * Old name for these ioctls.
 */
#define	PM_GET_POWER		PM_GET_NORM_PWR
#define	PM_SET_POWER		PM_SET_CUR_PWR

/*
 * This structure is obsolete and will be removed in a later release
 */
typedef struct {
	caddr_t	who;		/* Device to configure */
	int	select;		/* Selects the component or dependent */
				/* of the device */
	int	level;		/* Power or threshold level */
	caddr_t dependent;	/* Buffer to hold name of dependent */
	int	size;		/* Size of dependent buffer */
} pm_request;

/*
 * This is the new struct that replaces pm_request
 */
typedef struct pm_req {
	char	*physpath;	/* physical path of device to configure */
				/* see libdevinfo(3) */
	int	component;	/* Selects the component of the device */
	int	value;		/* power level, threshold value, or count */
	void	*data;		/* command-dependent variable sized data */
	size_t	datasize;	/* Size of data buffer */
} pm_req_t;

/*
 * Possible values for the event field of pm_state_change below
 */
typedef enum {
	PSC_PENDING_CHANGE,	/* device needs to change, is blocked */
	PSC_HAS_CHANGED		/* device level has changed */
} psc_events;

#define	PSC_EVENT_LOST	0x40000000	/* buffer overrun */

/*
 * Special value for power level fields in pm_state_change below
 */
#define	PM_LEVEL_UNKNOWN	-1	/* power level is unknown */

typedef struct pm_state_change {
	caddr_t	physpath;	/* Device which has changed state */
	int	component;	/* which component changed state */
	int	event;		/* type of event */
	time_t	timestamp;	/* time of state change */
	int	old_level;	/* power level changing from */
	int	new_level;	/* power level changing to */
	size_t	size;		/* size of buffer physpath points to */
} pm_state_change_t;

#ifdef _SYSCALL32

/* Kernel's view of ILP32 structure version. */

/*
 * This struct is obsolete and will be removed in a later release
 */
typedef struct {
	caddr32_t	who;	/* Device to configure */
	int		select;	/* Selects the component or dependent */
				/* of the device */
	int		level;	/* Power or threshold level */
	caddr32_t	dependent;	/* Buffer to hold name of */
					/* dependent */
	size32_t	size;	/* Size of dependent buffer */
} pm_request32;

typedef struct pm_req32 {
	caddr32_t physpath;	/* physical path of device to configure */
				/* see libdevinfo(3) */
	int	component;	/* selects the component of the device */
	int	value;		/* power level, threshold value, or count */
	caddr32_t data;		/* command-dependent variable sized data */
	size32_t datasize;	/* Size of data buffer */
} pm_req32_t;

typedef struct pm_state_change32 {
	caddr32_t	physpath;	/* Device which has changed state */
	int		component;	/* which component changed state */
	int		event;		/* type of event */
	time32_t	timestamp;	/* time of state change */
	int		old_level;	/* power level changing from */
	int		new_level;	/* power level changing to */
	size32_t	size;		/* size of buffer physpath points to */
} pm_state_change32_t;

#endif

/*
 * Return values from ioctl commands that return pm state info.
 */

typedef enum {
	PM_SYSTEM_PM_ENABLED,
	PM_SYSTEM_PM_DISABLED,
	PM_NO_PM_COMPONENTS,
	PM_CREATE_COMPONENTS,
	PM_AUTOPM,
	PM_DEFAULT_THRESHOLD,
	PM_DEVICE_THRESHOLD,
	PM_COMPONENT_THRESHOLD,
	PM_OLD_THRESHOLD,
	PM_DIRECTLY_MANAGED
} pm_states;


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PM_H */
