/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KBTRANS_H
#define	_SYS_KBTRANS_H

#pragma ident	"@(#)kbtrans.h	1.16	99/08/19 SMI"

/*
 * Interface between hardware keyboard driver and generic keyboard
 * translation module.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Do not expose the internals of these structures to kbtrans clients
 */
struct kbtrans_hardware;

struct kbtrans;

enum kbtrans_message_response {
	KBTRANS_MESSAGE_HANDLED = 0,
	KBTRANS_MESSAGE_NOT_HANDLED = 1
};

typedef int kbtrans_key_t;

enum keystate { KEY_PRESSED = 0, KEY_RELEASED = 1 };

/*
 * Callbacks registered by the hardware specific driver/module
 */
struct kbtrans_hw_callbacks {

	/* Routine to set the LED's in non-polled mode */
	void (*streams_setled)(struct kbtrans_hardware *, int);

	/* Routine to set the LED's in polled mode */
	void (*polled_setled)(struct kbtrans_hardware *, int);

	/* Routine to indicate that a scande is available in polled mode */
	boolean_t (*polled_is_scancode_available)(struct kbtrans_hardware *,
		kbtrans_key_t *, enum keystate *);
};

/*
 * kbtrans_streams_init():
 *
 * Initializes the generic keyboard translation module.  Must be
 * called from the hardware module's open(9e) routine.
 *
 * Arguments:
 *	- queue_t *q
 *       	The read queue.
 *
 *   	- int sflag
 *        	sflag from the streams open routine
 *
 *   	- cred_t *crp
 *        	credentials from open
 *
 *   	- struct kbtrans_hardware *hw
 *       	hardware-specific data, passed to hardware callbacks
 *
 *    	- struct kbtrans_hw_callbacks *hw_callbacks
 *       	hardware support callbacks (see below)
 *
 *    	- struct kbtrans **kbtrans
 *        	returned state structure pointer
 *
 *    	- int initial_leds
 *    	- int initial_led_mask
 *        	Provides state information (if available) about the current
 *        	keyboard state, in the form of LED state.  initial_leds shows
 *        	which LEDs are lit; initial_led_mask shows which bits in
 *        	initial_leds are valid.  This mechanism exists primarily to
 *        	retain the existing state of NumLock across the transition
 *       	from firmware to the OS.
 */
extern int kbtrans_streams_init(queue_t *, int, cred_t *,
	struct kbtrans_hardware *, struct kbtrans_hw_callbacks *,
	struct kbtrans **, int, int);

/*
 * kbtrans_streams_fini():
 *
 * Shuts down the generic translation module.  Must be called from
 * the hardware module's close(9e) routine.
 */
extern int kbtrans_streams_fini(struct kbtrans *);

/*
 * kbtrans_streams_message():
 *
 * The hardware module should pass all streams messages received from
 * above to this routine.  The generic translation module will process
 * most of them, returning KBTRANS_MESSAGE_HANDLED for the ones that
 * it has handled and KBTRANS_MESSAGE_NOT_HANDLED for the ones that
 * it did not handle.  For KBTRANS_MESSAGE_HANDLED, the hardware module
 * should take no further action on the message.  For
 * KBTRANS_MESSAGE_NOT_HANDLED, the hardware module is responsible for
 * any action, including returning an appropriate error.
 *
 * Must be called from the hardware module's write put(9e) or srv(9e)
 * routine.
 */
extern enum kbtrans_message_response kbtrans_streams_message(struct kbtrans *,
	mblk_t *);

/*
 * kbtrans_streams_key():
 *
 * When a key is pressed or released, the hardware module should
 * call kbtrans, passing the key number and its new
 * state.  kbtrans is responsible for autorepeat handling;
 * the hardware module should report only actual press/release
 * events, suppressing any hardware-generated autorepeat.
 */
extern void kbtrans_streams_key(struct kbtrans *, kbtrans_key_t key,
	enum keystate state);

/*
 * kbtrans_streams_set_keyboard():
 *
 * At any time after calling kbtrans_streams_init, the hardware
 * module should make this call to report the type of keyboard
 * attached.  "type" is the keyboard type, typically KB_SUN4,
 * KB_PC, or KB_USB.
 */
extern void kbtrans_streams_set_keyboard(struct kbtrans *, int,
	struct keyboard *);

/*
 * kbtrans_streams_has_reset():
 *
 * At any time between kbtrans_streams_init and kbtrans_streams_fini,
 * the hardware module can call this routine to report that the
 * keyboard has been reset, e.g. by being unplugged and reattached.
 *
 * This function is for use by keyboard devices that do not formally
 * support hotplug.  If the keyboard hardware spontaneously resets
 * itself in a case other than hotplug, this routine is called to
 * report the rest.
 *
 */
extern void kbtrans_streams_has_reset(struct kbtrans *);

/*
 * kbtrans_ischar():
 * kbtrans_getchar():
 *
 * These routines are used for polled input, e.g. for kadb or PROM
 * input.  Note that, with suitable casting, these routines are usable
 * as CONSOPENPOLLEDIO routines.
 *
 * May only be called from single-threaded code, e.g. kadb.
 */
extern boolean_t kbtrans_ischar(struct kbtrans *);
extern int kbtrans_getchar(struct kbtrans *);

/*
 * kbtrans_streams_enable():
 *	Routine to be called from the hardware specific module when
 * 	the stream is ready to take messages.
 */
extern void kbtrans_streams_enable(struct kbtrans *);


/*
 * kbtrans_streams_releaseall():
 *	Release all the keys that are held down.
 */
extern void kbtrans_streams_releaseall(struct kbtrans *);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KBTRANS_H */
