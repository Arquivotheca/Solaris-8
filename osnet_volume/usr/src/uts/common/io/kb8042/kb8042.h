/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_KB8042_H
#define	_KB8042_H

#pragma ident	"@(#)kb8042.h	1.18	99/03/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Messages from keyboard.
 */
#define	KB_ERROR	0x00	/* Keyboard overrun or detection error */
#define	KB_POST_OK	0xAA	/* Sent at completion of poweron */
#define	KB_ECHO		0xEE	/* Response to Echo command (EE)  */
#define	KB_ACK		0xFA	/* Acknowledgement byte from keyboard */
#define	KB_POST_FAIL	0xFC	/* Power On Self Test failed */
#define	KB_RESEND	0xFE	/* response from keyboard to resend data */

/*
 * Commands to keyboard.
 */
#define	KB_SET_LED	0xED	/* Tell kbd that following byte is led status */
#define	KB_READID	0xF2	/* command to read keyboard id */
#define	KB_ENABLE	0xF4	/* command to to enable keyboard */
#define	KB_RESET	0xFF	/* command to reset keyboard */
#define	KB_SET_TYPE	0xF3	/* command--next byte is typematic values */
#define	KB_SET_SCAN	0xF0	/* kbd command to set scan code set */

/*
 * LED bits
 */
#define	LED_SCR		0x01	/* Flag bit for scroll lock */
#define	LED_CAP		0x04	/* Flag bit for cap lock */
#define	LED_NUM		0x02	/* Flag bit for num lock */

/*
 * Keyboard scan code prefixes
 */
#define	KAT_BREAK	0xf0	/* first byte in two byte break sequence */
#define	KAT_EXTEND	0xe0	/* first byte in two byte extended sequence */
#define	KAT_EXTEND2	0xe1	/* Used in "Pause" sequence */

/*
 * Korean keyboard keys.  We handle these specially to avoid having to
 * dramatically extend the table.
 */
#define	KAT_HANGUL_HANJA	0xf1
#define	KAT_HANGUL		0xf2

#ifdef _KERNEL

struct kb8042 {
	kmutex_t	w_hw_mutex;	/* hardware mutex */
	int	w_init;		/* workstation has been initialized */
	queue_t	*w_qp;		/* pointer to queue for this minor device */
	int	w_kblayout;	/* keyboard layout code */
	dev_t	w_dev;		/* major/minor for this device */
	ddi_iblock_cookie_t	w_iblock;
	ddi_acc_handle_t	handle;
	uint8_t			*addr;
	int	kb_old_scan;	/* scancode for autorepeat filtering */
	int	command_state;
	struct {
		int desired;
		int commanded;
	}	leds;
	struct {
	    enum keystate	break_prefix_received;
	    int			state;
	} parse_scan;
	struct kbtrans	*hw_kbtrans;
	struct cons_polledio	polledio;
	struct {
		unsigned char mod1;
		unsigned char mod2;
		unsigned char trigger;
		boolean_t mod1_down;
		boolean_t mod2_down;
		boolean_t enabled;
	}		debugger;
	boolean_t	polled_synthetic_release_pending;
	int		polled_synthetic_release_key;
};

#define	KB_COMMAND_STATE_IDLE	0
#define	KB_COMMAND_STATE_LED	1
#define	KB_COMMAND_STATE_WAIT	2

extern boolean_t atKeyboardConvertScan(struct kb8042 *, unsigned char scan,
			int *keynum, enum keystate *, boolean_t *);
extern void atKeyboardConvertScan_init(struct kb8042 *);

#if	defined(i86pc)
/*
 * We pick up the initial state of the keyboard from the BIOS state.
 */
#define	BIOS_KB_FLAG		0x417	/* address of BIOS keyboard state */
#define	BIOS_SCROLL_STATE	0x10
#define	BIOS_NUM_STATE		0x20
#define	BIOS_CAPS_STATE		0x40
#endif

#endif

#ifdef	__cplusplus
}
#endif

#endif /* _KB8042_H */
