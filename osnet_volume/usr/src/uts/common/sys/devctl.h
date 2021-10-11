/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DEVCTL_H
#define	_SYS_DEVCTL_H

#pragma ident	"@(#)devctl.h	1.13	99/04/14 SMI"

/*
 * Device control interfaces
 */
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * structure used to pass IOCTL data between the libdevice interfaces
 * and nexus driver devctl IOCTL interface.
 *
 * Applications and nexus drivers may not access the contents of this
 * structure directly.  Instead, drivers must use the ndi_dc_XXX(9n)
 * interfaces, while applications must use the interfaces provided by
 * libdevice.so.1.
 */
struct devctl_iocdata {
	uint_t	cmd;			/* ioctl cmd */
	char	*dev_path;		/* Full pathname */
	char	*dev_name;		/* MAXNAMELEN */
	char	*dev_addr;		/* MAXNAMELEN */
	char	*dev_minor;		/* MAXNAMELEN */
	uint_t	*ret_state;		/* return from getstate */
};

#if defined(_SYSCALL32)
/*
 * Structure to pass/return data from 32-bit program's.
 */
struct devctl_iocdata32 {
	uint32_t cmd;			/* ioctl cmd */
	caddr32_t dev_path;		/* Full pathname */
	caddr32_t dev_name;		/* MAXNAMELEN */
	caddr32_t dev_addr;		/* MAXNAMELEN */
	caddr32_t dev_minor;		/* MAXNAMELEN */
	caddr32_t ret_state;		/* return from getstate */
};
#endif

/*
 * State of receptacle for an Attachment Point.
 */
typedef enum {
	AP_RSTATE_EMPTY,
	AP_RSTATE_DISCONNECTED,
	AP_RSTATE_CONNECTED
} ap_rstate_t;

/*
 * State of occupant for an Attachment Point.
 */
typedef enum {
	AP_OSTATE_UNCONFIGURED,
	AP_OSTATE_CONFIGURED
} ap_ostate_t;

/*
 * condition of an Attachment Point.
 */
typedef enum {
	AP_COND_UNKNOWN,
	AP_COND_OK,
	AP_COND_FAILING,
	AP_COND_FAILED,
	AP_COND_UNUSABLE
} ap_condition_t;

/*
 * structure used to return the state of Attachment Point (AP) thru
 * devctl_ap_getstate() interface.
 */

typedef struct devctl_ap_state {
	ap_rstate_t	ap_rstate; 	/* receptacle state */
	ap_ostate_t	ap_ostate;	/* occupant state */
	ap_condition_t	ap_condition;	/* condition of AP */
	time_t		ap_last_change;
	uint32_t	ap_error_code;	/* error code */
	uint8_t		ap_in_transition;
} devctl_ap_state_t;

#if defined(_SYSCALL32)
/*
 * Structure to pass/return data from 32-bit program's.
 */
typedef struct devctl_ap_state32 {
	ap_rstate_t	ap_rstate; 	/* receptacle state */
	ap_ostate_t	ap_ostate;	/* occupant state */
	ap_condition_t	ap_condition;	/* condition of AP */
	time32_t	ap_last_change;
	uint32_t	ap_error_code;	/* error code */
	uint8_t		ap_in_transition;
} devctl_ap_state32_t;
#endif

#define	DEVCTL_IOC		(0xDC << 16)
#define	DEVCTL_BUS_QUIESCE	(DEVCTL_IOC | 1)
#define	DEVCTL_BUS_UNQUIESCE	(DEVCTL_IOC | 2)
#define	DEVCTL_BUS_RESETALL	(DEVCTL_IOC | 3)
#define	DEVCTL_BUS_RESET	(DEVCTL_IOC | 4)
#define	DEVCTL_BUS_GETSTATE	(DEVCTL_IOC | 5)
#define	DEVCTL_DEVICE_ONLINE	(DEVCTL_IOC | 6)
#define	DEVCTL_DEVICE_OFFLINE	(DEVCTL_IOC | 7)
#define	DEVCTL_DEVICE_GETSTATE	(DEVCTL_IOC | 9)
#define	DEVCTL_DEVICE_RESET	(DEVCTL_IOC | 10)
#define	DEVCTL_BUS_CONFIGURE	(DEVCTL_IOC | 11)
#define	DEVCTL_BUS_UNCONFIGURE	(DEVCTL_IOC | 12)
#define	DEVCTL_DEVICE_REMOVE	(DEVCTL_IOC | 13)
#define	DEVCTL_AP_CONNECT	(DEVCTL_IOC | 14)
#define	DEVCTL_AP_DISCONNECT	(DEVCTL_IOC | 15)
#define	DEVCTL_AP_INSERT	(DEVCTL_IOC | 16)
#define	DEVCTL_AP_REMOVE	(DEVCTL_IOC | 17)
#define	DEVCTL_AP_CONFIGURE	(DEVCTL_IOC | 18)
#define	DEVCTL_AP_UNCONFIGURE	(DEVCTL_IOC | 19)
#define	DEVCTL_AP_GETSTATE	(DEVCTL_IOC | 20)
#define	DEVCTL_AP_CONTROL	(DEVCTL_IOC | 21)

/*
 * Device and Bus State definitions
 *
 * Device state is returned as a set of bit-flags that indicate the current
 * operational state of a device node.
 *
 * Device nodes for leaf devices only contain state information for the
 * device itself.  Nexus device nodes contain both Bus and Device state
 * information.
 *
 * 	DEVICE_ONLINE  - Device is available for use by the system.  Mutually
 *                       exclusive with DEVICE_OFFLINE.
 *
 *	DEVICE_OFFLINE - Device is unavailable for use by the system.
 *			 Mutually exclusive with DEVICE_ONLINE and DEVICE_BUSY.
 *
 *	DEVICE_DOWN    - Device has been placed in the "DOWN" state by
 *			 its controlling driver.
 *
 *	DEVICE_BUSY    - Device has open instances or nexus has INITALIZED
 *                       children (nexi).  A device in this state is by
 *			 definition Online.
 *
 * Bus state is returned as a set of bit-flags which indicates the
 * operational state of a bus associated with the nexus dev_info node.
 *
 * 	BUS_ACTIVE     - The bus associated with the device node is Active.
 *                       I/O requests from child devices attached to the
 *			 are initiated (or queued for initiation) as they
 *			 are received.
 *
 *	BUS_QUIESCED   - The bus associated with the device node has been
 *			 Quieced. I/O requests from child devices attached
 *			 to the bus are held pending until the bus nexus is
 *			 Unquiesced.
 *
 *	BUS_SHUTDOWN   - The bus associated with the device node has been
 *			 shutdown by the nexus driver.  I/O requests from
 *			 child devices are returned with an error indicating
 *			 the requested operation failed.
 */
#define	DEVICE_ONLINE	0x1
#define	DEVICE_BUSY	0x2
#define	DEVICE_OFFLINE  0x4
#define	DEVICE_DOWN	0x8

#define	BUS_ACTIVE	0x10
#define	BUS_QUIESCED	0x20
#define	BUS_SHUTDOWN	0x40

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEVCTL_H */
