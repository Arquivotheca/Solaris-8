/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_USBMS_H
#define	_SYS_USB_USBMS_H

#pragma ident	"@(#)usbms.h	1.2	99/05/25 SMI"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct usbms_state {
	queue_t			*usbms_rq_ptr;   /* pointer to read queue */
	queue_t			*usbms_wq_ptr;   /* pointer to write queue */

	/* Flag for mouse open/qwait status */

	int			usbms_flags;

	/*
	 * Is an ioctl fails because an mblk wasn't
	 * available, the mlbk is saved here.
	 */

	mblk_t			*usbms_iocpending;

	/* mouse software structure from msreg.h */

	struct ms_softc		usbms_softc;

	/* Previous button byte */

	char			usbms_oldbutt;

	/* Report descriptor handle received from hid */

	hidparser_handle_t	usbms_report_descr_handle;

	/*
	 * Max pixel delta of jitter controlled. As this number increases
	 * the jumpiness of the msd increases, i.e., the coarser the motion
	 * for mediumm speeds.
	 * jitter_thresh is the maximum number of jitters suppressed. Thus,
	 * hz/jitter_thresh is the maximum interval of jitters suppressed. As
	 * jitter_thresh increases, a wider range of jitter is suppressed.
	 * However, the more inertia the mouse seems to have, i.e., the slower
	 * the mouse is to react.
	 */

	int			usbms_jitter_thresh;

	/* Timeout used when mstimeout in effect */

	clock_t			usbms_jittertimeout;

	/*
	 * Measure how many (speed_count) msd deltas exceed threshold
	 * (speedlimit). If speedlaw then throw away deltas over speedlimit.
	 * This is to keep really bad mice that jump around from getting
	 * too far.
	 */

	/* Threshold above which deltas are thrown out */

	int		usbms_speedlimit;

	int		usbms_speedlaw;	/* Whether to throw away deltas */

	/*  No. of deltas exceeding spd. limit */

	int		usbms_speed_count;

	int		usbms_iocid;	/* ID of "ioctl" being waited for */
	short		usbms_state;	/* button state at last sample */
	short		usbms_jitter;	/* state counter for input routine */
	timeout_id_t	usbms_timeout_id;	/* id returned by timeout() */
	bufcall_id_t	usbms_reioctl_id;	/* id returned by bufcall() */
	bufcall_id_t	usbms_resched_id;	/* id returned by bufcall() */
	int32_t		usbms_num_buttons;	/* No. of buttons */

} usbms_state_t;


#define	USBMS_OPEN    0x00000001 /* mouse is open for business */
#define	USBMS_QWAIT   0x00000002 /* mouse is waiting for a response */

/* Macro to find absolute value */

#define	USB_ABS(x)		((x) < 0 ? -(x) : (x))

/*
 * Macro to restrict the value of x to lie between 127 & -127 :
 * if x > 127 return 127
 * else if x < -127 return -127
 * else return x
 */

#define	USB_BYTECLIP(x)	(char)((x) > 127 ? 127 : ((x) < -127 ? -127 : (x)))

/*
 * Default number of buttons
 */

#define	USB_MS_DEFAULT_BUTTON_NO	3

/*
 * Input routine states. See usbms_input().
 */
#define	USBMS_WAIT_BUTN		0	/* Button byte */
#define	USBMS_WAIT_X		1	/* Delta X byte */
#define	USBMS_WAIT_Y    	2	/* Delta Y byte */


/*
 * USB buttons
 */
#define	USBMS_BUT1	0x1	/* left button position */
#define	USBMS_BUT2	0x2	/* right button position. */
#define	USBMS_BUT3	0x4	/* middle button position. */

/*
 * These defines are for converting USB button information to the
 * format that Type 5 mouse sends upstream, which is what the xserver
 * expects.
 */

#define	USB_NO_BUT_PRESSED	0x7
#define	USB_LEFT_BUT_PRESSED	0x3
#define	USB_RIGHT_BUT_PRESSED	0x6
#define	USB_MIDDLE_BUT_PRESSED	0x5

/*
 * Private data are initialized to these values
 */
#define	USBMS_JITTER_THRESH	0	/* Max no. of jitters suppressed */
#define	USBMS_SPEEDLIMIT	48	/* Threshold for msd deltas */
#define	USBMS_SPEEDLAW		0	/* Whether to throw away deltas */
#define	USBMS_SPEED_COUNT	0	/* No. of deltas exceeding spd. limit */
#define	USBMS_BUF_BYTES		4096	/* Mouse buffer size */
#define	USBMS_USAGE_PAGE_BUTTON	0x9	/* Usage Page data value : Button */

#define	JITTERRATE		12	/* No of jitters before timeout */

/* Jitter Timeout while initialization */
#define	JITTER_TIMEOUT		(hz/JITTERRATE)

/*
 * Masks for debug printing
 */
#define	PRINT_MASK_ATTA		0x00000001
#define	PRINT_MASK_OPEN 	0x00000002
#define	PRINT_MASK_CLOSE	0x00000004
#define	PRINT_MASK_SERV		0x00000008
#define	PRINT_MASK_IOCTL	0x00000010
#define	PRINT_MASK_INPUT_INCR	0x00000020
#define	PRINT_MASK_ALL		0xFFFFFFFF

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_USBMS_H */
