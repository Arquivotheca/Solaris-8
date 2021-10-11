/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_USBKBM_H
#define	_SYS_USB_USBKBM_H

#pragma ident	"@(#)usbkbm.h	1.4	99/10/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <sys/vuid_event.h>
#include <sys/stream.h>
#include <sys/kbd.h>


/*
 * USB keyboard LED masks (used to set LED's on USB keyboards)
 */
#define	USB_LED_NUM_LOCK	0x1
#define	USB_LED_CAPS_LOCK	0x2
#define	USB_LED_SCROLL_LOCK	0x4
#define	USB_LED_COMPOSE		0x8
#define	USB_LED_KANA		0x10	/* Valid only on Japanese layout */

/* Modifier key masks */
#define	USB_LCTLBIT   0x01
#define	USB_LSHIFTBIT 0x02
#define	USB_LALTBIT   0x04
#define	USB_LMETABIT  0x08
#define	USB_RCTLBIT   0x10
#define	USB_RSHIFTBIT 0x20
#define	USB_RALTBIT   0x40
#define	USB_RMETABIT  0x80

#define	USB_LSHIFTKEY	225
#define	USB_LCTLCKEY	224
#define	USB_LALTKEY	226
#define	USB_LMETAKEY	227
#define	USB_RCTLCKEY	228
#define	USB_RSHIFTKEY	229
#define	USB_RMETAKEY	231
#define	USB_RALTKEY	230

/*
 * The keyboard would report ErrorRollOver in all array fields when
 * the number of non-modifier keys pressed exceeds the Report Count.
 */
#define	USB_ERRORROLLOVER 1


/*
 * This defines the format of translation tables.
 *
 * A translation table is USB_KEYMAP_SIZE "entries", each of which is 2
 * bytes (unsigned shorts).  The top 8 bits of each entry are decoded by
 * a case statement in getkey.c.  If the entry is less than 0x100, it
 * is sent out as an EUC character (possibly with bucky bits
 * OR-ed in).  "Special" entries are 0x100 or greater, and
 * invoke more complicated actions.
 */

/*
 * Default packet size in bytes
 */

#define	USB_KBD_DEFAULT_PACKET_SIZE	8

/* definitions for various state machines */
#define	USBKBM_OPEN	0x00000001 /* keyboard is open for business */
#define	USBKBM_QWAIT	0x00000002 /* keyboard is waiting for a response */

/*
 * Polled key state
 */
typedef struct poll_keystate {
	int		poll_key;		/* scancode */
	enum keystate   poll_state;		/* pressed or released */
} poll_keystate_t;

#define	USB_POLLED_BUFFER_SIZE	20	/* # of characters in poll buffer */

#define	USBKBM_MAXPKTSIZE	10	/* Maximum size of a packet */

/* state structure for usbkbm */
typedef struct  usbkbm_state_struct {
	struct kbtrans		*usbkbm_kbtrans;
	queue_t			*usbkbm_readq;		/* read queue */
	queue_t			*usbkbm_writeq;		/* write queue */
	int			usbkbm_flags;
	uint32_t		usbkbm_packet_size;	/* size usb packet */
	/* Pointer to the parser handle */
	hidparser_handle_t	usbkbm_report_descr;
	uint16_t		usbkbm_layout;		/* keyboard layout */
	/*
	 * Setting this indicates that the second IOCTL
	 * after KBD_CMD_SETLED follows
	 */
	int			usbkbm_setled_second_byte;
	/* Keyboard packets sent last */
	uchar_t			usbkbm_lastusbpacket[10];

	hid_polled_input_callback_t
				usbkbm_hid_callback;	/* poll information */

	mblk_t			*usbkbm_pending_link; /* mp waiting response */

	/* "ioctl" awaiting buffer */
	mblk_t			*usbkbm_streams_iocpending;

	/* id from qbufcall on allocb failure */
	bufcall_id_t		usbkbm_streams_bufcallid;

	/* Polled input information */
	struct cons_polledio	usbkbm_polled_info;

	/* These entries are for polled input */
	uint_t		usbkbm_polled_buffer_num_characters;
	poll_keystate_t	usbkbm_polled_scancode_buffer[USB_POLLED_BUFFER_SIZE];
	poll_keystate_t	*usbkbm_polled_buffer_head;
	poll_keystate_t	*usbkbm_polled_buffer_tail;

} usbkbm_state_t;

#define	USB_PRESSED	0x00	/* key was pressed */
#define	USB_RELEASED	0x01	/* key was released */

/* Number of entries in the keytable */
#define	KEYMAP_SIZE_USB		255

/* Size in bytes of the keytable */
#define	USB_KEYTABLE_SIZE	(KEYMAP_SIZE_USB * sizeof (keymap_entry_t))

/* structure to save global state */
typedef struct usbkbm_save_state {
	/* LED state */
	uchar_t		usbkbm_save_led;

	/* Keymap information */
	struct keyboard usbkbm_save_keyindex;

} usbkbm_save_state_t;

/*
 * Masks for debug printing
 */
#define	PRINT_MASK_ATTA		0x00000001
#define	PRINT_MASK_OPEN 	0x00000002
#define	PRINT_MASK_CLOSE	0x00000004
#define	PRINT_MASK_PACKET	0x00000008
#define	PRINT_MASK_ALL		0xFFFFFFFF


#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_USBKBM_H */
