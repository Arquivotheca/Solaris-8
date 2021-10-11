/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_HOTPLUG_HPCTRL_H
#define	_SYS_HOTPLUG_HPCTRL_H

#pragma ident	"@(#)hpctrl.h	1.6	99/04/13 SMI"

/*
 * ****************************************************************
 * Hot Plug Controller interfaces for PCI and CompactPCI platforms.
 * ****************************************************************
 */
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Type definition for slot handle. This is an opaque pointer
 * created by the HPS framework.
 */
typedef void *hpc_slot_t;

#define	HPC_SLOT_OPS_VERSION	0

/*
 * slot operations structure definition.
 *
 *	Function		Description
 *	--------		-----------
 *	xxx_op_connect		CONNECT the slot to the bus to enable
 *				access to the adapter.
 *	xxx_op_disconnect	DISCONNECT the slot from the bus. For PCI,
 *				this disables the power to the slot.
 *	xxx_op_insert		Prepare the slot for card insertion. This
 *				may not be applicable for all bus types.
 *	xxx_op_remove		Prepare the slot for card removal. This
 *				may not be applicable for all bus types.
 *	xxx_op_control		Perform misc. commands to control the
 *				LEDs, get status information, etc.
 */
typedef struct hpc_slot_ops {
	int	hpc_version;			/* HPC_SLOT_OPS_VERSION */
	int	(*hpc_op_connect)(caddr_t ops_arg, hpc_slot_t slot_hdl,
			void *data, uint_t flags);
	int	(*hpc_op_disconnect)(caddr_t ops_arg, hpc_slot_t slot_hdl,
			void *data, uint_t flags);
	int	(*hpc_op_insert)(caddr_t ops_arg, hpc_slot_t slot_hdl,
			void *data, uint_t flags);
	int	(*hpc_op_remove)(caddr_t ops_arg, hpc_slot_t slot_hdl,
			void *data, uint_t flags);
	int	(*hpc_op_control)(caddr_t ops_arg, hpc_slot_t slot_hdl,
			int request, caddr_t arg);
} hpc_slot_ops_t;

#define	HPC_SLOT_INFO_VERSION	0

/*
 * Slot information structure.
 */
typedef struct hpc_slot_info {
	uint16_t	version;		/* HPC_SLOT_INFO_VERSION */
	uint16_t	slot_type;		/* slot type: PCI, ... */
	union {
	    /* pci bus slot */
	    struct pci_slot_info {
		uint16_t	device_number;		/* PCI device number */
		uint16_t	slot_capabilities;	/* 64bit, etc. */
		char		slot_logical_name[256];	/* slot logical name */
	    } pci;
	    /* other bus types go here... */
	} slot;
} hpc_slot_info_t;

/* short names for bus specific fields in hpc_slot_info structure */
#define	pci_dev_num		slot.pci.device_number
#define	pci_slot_name		slot.pci.slot_logical_name
#define	pci_slot_capabilities	slot.pci.slot_capabilities

/* slot_type definitions */
#define	HPC_SLOT_TYPE_PCI	0x1		/* PCI bus slot */
#define	HPC_SLOT_TYPE_CPCI	0x2		/* Compact PCI bus slot */
#define	HPC_SLOT_TYPE_SAFARI	0x3		/* Safari bus slot */

/* bit definitions in slot_capabilities field for PCI or cPCI bus slots */
#define	HPC_SLOT_64BITS		0x0001	/* slot is a 64bit slot */
#define	HPC_SLOT_TEST		0x0002	/* testing capability on the slot */

/*
 * xxx_op_control command definitions.
 *
 * 	Command (request)	   arg			Descritpion
 *	-----------------	   ---			-----------
 *	HPC_CTRL_GET_LED_STATE	   hpc_led_info *	Get state of an LED.
 *	HPC_CTRL_SET_LED_STATE	   hpc_led_info *	Set state of an LED.
 *	HPC_CTRL_GET_SLOT_STATE	   hpc_slot_state_t *	Get the slot state.
 *	HPC_CTRL_DEV_CONFIGURED	   NULL 		Board is configured.
 *	HPC_CTRL_DEV_UNCONFIGURED  NULL 		Board is unconfigured.
 *	HPC_CTRL_GET_BOARD_TYPE    hpc_board_type_t *	Get board type info.
 *	HPC_CTRL_DISABLE_AUTOCFG   NULL			Disable auto config-
 *							uration for this slot.
 *	HPC_CTRL_ENABLE_AUTOCFG    NULL			Enable auto config-
 *							uration for this slot.
 *	HPC_CTRL_DISABLE_SLOT	   NULL			Disable the slot for
 *							hot plug operations.
 *	HPC_CTRL_ENABLE_SLOT	   NULL			ReEnable the slot for
 *							hot plug operations.
 */
#define	HPC_CTRL_GET_LED_STATE		0x1
#define	HPC_CTRL_SET_LED_STATE		0x2
#define	HPC_CTRL_GET_SLOT_STATE		0x3
#define	HPC_CTRL_DEV_CONFIGURED		0x4
#define	HPC_CTRL_DEV_UNCONFIGURED	0x5
#define	HPC_CTRL_GET_BOARD_TYPE		0x6
#define	HPC_CTRL_DISABLE_AUTOCFG	0x7
#define	HPC_CTRL_ENABLE_AUTOCFG		0x8
#define	HPC_CTRL_DISABLE_SLOT		0x9
#define	HPC_CTRL_ENABLE_SLOT		0x10
#define	HPC_CTRL_DISABLE_ENUM		0x11
#define	HPC_CTRL_ENABLE_ENUM		0x12

/*
 * type definitions for led information.
 *
 * Note: ATTN/ACTIVE leds are platform specific and they may not be
 *	 available on all platforms.
 */
typedef enum { HPC_FAULT_LED, HPC_POWER_LED, HPC_ATTN_LED,
	HPC_ACTIVE_LED} hpc_led_t;

typedef enum { HPC_LED_OFF, HPC_LED_ON, HPC_LED_BLINK } hpc_led_state_t;

typedef struct hpc_led_info {
	hpc_led_t	led;	/* led id: HPC_POWER_LED, HPC_FAULT_LED, ... */
	hpc_led_state_t	state;	/* led state: HPC_LED_ON, HPC_LED_OFF, ... */
} hpc_led_info_t;

/*
 * type definition for slot state.
 *
 *	HPC_SLOT_EMPTY		Slot has no card present.
 *	HPC_SLOT_CONNECTED	Card is present in the slot and it is
 *				connected to the bus.
 *	HPC_SLOT_DISCONNECTED	Card is present in the slot and it is
 *				disconnected from the bus.
 *	HPC_SLOT_UNKNOWN	If the HPC driver can not figure out
 *				the receptacle state. This is possible
 *				on Compact PCI Hot Swap platform.
 */
typedef enum { HPC_SLOT_EMPTY, HPC_SLOT_DISCONNECTED,
	HPC_SLOT_CONNECTED, HPC_SLOT_UNKNOWN } hpc_slot_state_t;

/*
 * type definition for board type.
 *
 *	HPC_BOARD_UNKNOWN	Board is either not present or unknown.
 *	HPC_BOARD_PCI_HOTPLUG	PCI bus adapter.
 *	HPC_BOARD_CPCI_NON_HS	Non Hot Swap cPCI board.
 *	HPC_BOARD_CPCI_BASIC_HS	Basic Hot Swap cPCI board.
 *	HPC_BOARD_CPCI_FULL_HS	Full Hot Swap cPCI board.
 *	HPC_BOARD_CPCI_HS	Indicates if HSC driver can not determine
 *				the type of Hot Swap board.
 */
typedef enum { HPC_BOARD_UNKNOWN, HPC_BOARD_PCI_HOTPLUG,
	HPC_BOARD_CPCI_NON_HS, HPC_BOARD_CPCI_BASIC_HS,
	HPC_BOARD_CPCI_FULL_HS, HPC_BOARD_CPCI_HS } hpc_board_type_t;

/*
 * Event type definitions (for hpc_event_notify() interface).
 *
 *	Event			   Descritpion
 *	-----			   -----------
 *	HPC_EVENT_SLOT_INSERTION   Card is inserted in the slot.
 *	HPC_EVENT_SLOT_REMOVAL	   Card is removed from the slot.
 *	HPC_EVENT_SLOT_POWER_ON	   Slot is powered ON.
 *	HPC_EVENT_SLOT_POWER_OFF   Slot is powered OFF.
 *	HPC_EVENT_SLOT_LATCH_OPEN  LATCH on the slot is open.
 *	HPC_EVENT_SLOT_LATCH_SHUT  LATCH on the slot is shut.
 *	HPC_EVENT_SLOT_ENUM	   ENUM# signal is generated on the bus
 *				   and it may be generated from this slot.
 *	HPC_EVENT_SLOT_NOT_HEALTHY HEALTHY# signal is lost on this slot.
 *	HPC_EVENT_SLOT_HEALTHY_OK  HEALTHY# signal on this slot is OK now.
 *	HPC_EVENT_SLOT_CONFIGURE   Configure the occupant in the slot.
 *	HPC_EVENT_SLOT_UNCONFIGURE Unconfigure the occupant in the slot.
 */
#define	HPC_EVENT_SLOT_INSERTION	0x00000001
#define	HPC_EVENT_SLOT_REMOVAL		0x00000002
#define	HPC_EVENT_SLOT_POWER_ON		0x00000004
#define	HPC_EVENT_SLOT_POWER_OFF	0x00000008
#define	HPC_EVENT_SLOT_LATCH_OPEN	0x00000010
#define	HPC_EVENT_SLOT_LATCH_SHUT	0x00000020
#define	HPC_EVENT_SLOT_ENUM		0x00000040
#define	HPC_EVENT_SLOT_NOT_HEALTHY	0x00000080
#define	HPC_EVENT_SLOT_HEALTHY_OK	0x00000100
#define	HPC_EVENT_SLOT_CONFIGURE	0x00000200
#define	HPC_EVENT_SLOT_UNCONFIGURE	0x00000400
#define	HPC_EVENT_SLOT_BLUE_LED_ON	0x00000800
#define	HPC_EVENT_SLOT_BLUE_LED_OFF	0x00001000

/*
 * return values for errors from HPS framework interfaces.
 */
#define	HPC_SUCCESS			0x0
#define	HPC_ERR_INVALID			0x1	/* invalid arguments */
#define	HPC_ERR_SLOT_NOTREGISTERED	0x2	/* slot is not registered */
#define	HPC_ERR_SLOT_DUPLICATE		0x3	/* slot is already registered */
#define	HPC_ERR_BUS_NOTREGISTERED	0x4	/* slot is not registered */
#define	HPC_ERR_BUS_DUPLICATE		0x5	/* slot is already registered */
#define	HPC_ERR_NOTSUPPORTED		0x6	/* operation not supported */
#define	HPC_ERR_FAILED			0x7	/* operation failed */

/* return values for event notifications */
#define	HPC_EVENT_CLAIMED		0x10	/* HPC event is claimed */
#define	HPC_EVENT_UNCLAIMED		0x11	/* HPC event is not claimed */

/* definitions for slot (un)registration events */
#define	HPC_SLOT_ONLINE		1	/* slot is registered */
#define	HPC_SLOT_OFFLINE	2	/* slot is unregistered */

/*
 * function prototype definitions for interfaces between HPC driver
 * and Hot Plug Services framework.
 */
#ifdef	__STDC__
extern int hpc_slot_register(dev_info_t *dip, char *bus_path,
	hpc_slot_info_t *slot_info, hpc_slot_t *slot_hdl,
	hpc_slot_ops_t *slot_ops, caddr_t ops_arg, uint_t flags);
extern int hpc_slot_unregister(hpc_slot_t *slot_hdl);
extern struct hpc_slot_ops *hpc_alloc_slot_ops(int sleepflag);
extern void hpc_free_slot_ops(hpc_slot_ops_t *ops);
extern int hpc_slot_event_notify(hpc_slot_t slot_hdl, uint_t event,
	uint_t flags);
#else
extern int hpc_slot_register();
extern int hpc_slot_unregister();
extern struct hpc_slot_ops *hpc_alloc_slot_ops();
extern void hpc_free_slot_ops();
extern int hpc_slot_event_notify();
#endif	/* __STDC__ */

/*
 * *****************************************************************
 * Implementation specific data structures and definitons. These are
 * the private interfaces between cfgadm plug-in and the PCI nexus
 * driver.
 * *****************************************************************
 */

/*
 * Data structure used for DEVCTL_AP_CONTROL ioctl on the AP.
 */
struct hpc_control_data {
	uint_t	cmd;		/* HPC_CTRL_* command */
	void	*data;		/* pointer to data that is exchanged */
};

struct hpc_control32_data {
	uint_t	  cmd;		/* HPC_CTRL_* command */
	caddr32_t data;		/* pointer to data that is exchanged */
};

/* misc. control commands for DEVCTL_AP_CONTROL ioctl interface */
#define	HPC_CTRL_GET_SLOT_INFO	0x100
#define	HPC_CTRL_GET_CARD_INFO	0x101

/* card information structure to get data from the PCI config header */
typedef struct hpc_card_info {
	uint8_t	prog_class;	/* PCI_CONF_PROGCLASS byte */
	uint8_t	base_class;	/* PCI_CONF_BASCLASS byte */
	uint8_t	sub_class;	/* PCI_CONF_SUBCLASS byte */
	uint8_t	header_type;	/* PCI_CONF_HEADER byte */
} hpc_card_info_t;


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HOTPLUG_HPCTRL_H */
