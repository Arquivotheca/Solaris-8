/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CONSDEV_H
#define	_SYS_CONSDEV_H

#pragma ident	"@(#)consdev.h	5.22	99/03/05 SMI"	/* from SunOS-4.0 5.7 */

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * Console redirection.
 */
extern dev_t	rconsdev;	/* real (underlying) console */
extern struct vnode *rconsvp;	/* pointer to vnode for that device */

/*
 * Mouse, keyboard, and frame buffer configuration information.
 *
 * XXX:	Assumes a single mouse/keyboard/frame buffer triple.
 */
extern dev_t	mousedev;	/* default mouse device */
extern dev_t	kbddev;		/* default (actual) keyboard device */
extern dev_t	stdindev;	/* default standard input device */
extern dev_t	fbdev;		/* default framebuffer device */
extern struct vnode *fbvp;	/* pointer to vnode for that device */

/*
 * Workstation console redirection.
 *
 * The workstation console device is the multiplexor that hooks keyboard and
 * frame buffer together into a single tty-like device.  Access to it is
 * through the redirecting driver, so that frame buffer output can be
 * redirected to other devices.  wsconsvp names the redirecting access point,
 * and rwsconsvp names the workstation console itself.
 *
 * XXX:	Assumes a single workstation console.
 */
extern struct vnode *wsconsvp;	/* vnode for redirecting ws cons access */
extern struct vnode *rwsconsvp;	/* vnode for underlying workstation console */

extern int cn_conf;

/*
 * Generic console ioctls.
 *
 * On systems without OBP, all potential console devices should implement these.
 *
 * On systems with OBP, all potential console devices should implement
 * the ABORTENABLE ioctls.  All potential console devices that cannot share
 * their hardware with OBP should implement the POLLEDIO ioctls.
 */
#define	_CONSIOC	(('C'<<24)|('O'<<16)|('N'<<8))

/*
 * Get the structure of function pointers to be used for polled I/O
 *
 *	struct cons_polledio *polledio;
 *	struct strioctl str;
 *
 *	str.ic_cmd = CONS_OPENPOLLEDIO;
 *	str.ic_timout = INFTIM;
 *	str.ic_len = sizeof (polledio);
 *	str.ic_dp = (char *)&polledio;
 *	ioctl(fd, I_STR, &str);
 */
#define	CONSOPENPOLLEDIO	(_CONSIOC|0)

/*
 * Get the current state of abort enable
 * enable = ioctl(fd, CONSGETABORTENABLE, 0)
 */
#define	CONSGETABORTENABLE	(_CONSIOC|1)

/*
 * Set the current state of abort enable
 * ioctl(fd, CONSSETABORTENABLE, boolean_t)
 */
#define	CONSSETABORTENABLE	(_CONSIOC|2)

/*
 * Undo anything that was done with CONSOPENPOLLEDIO
 * ioctl(fd, CONSCLOSEPOLLEDIO, 0)
 */
#define	CONSCLOSEPOLLEDIO	(_CONSIOC|3)

#define	CONSPOLLEDIO_V0		0

/*
 * Opaque state structure for driver state.  Each driver has its own
 * implementation (with different names!), and casts to/from this.
 * This allows better type-checking than "void *", helping to ensure
 * that the structure passed in is the structure used in the callback.
 */
struct cons_polledio_arg;

/*
 * This is the structure that is used to handle polled I/O.  It is filled
 * in by a lower driver, passed up, and eventually registered with the
 * debugger that needs to do polled I/O.
 */
typedef struct cons_polledio {

	/*
	 * version of this structure
	 */
	unsigned	cons_polledio_version;

	/*
	 * Argument that is passed to the following routines.
	 */
	struct cons_polledio_arg	*cons_polledio_argument;

	/*
	 * Pointer to the routine and its argument that handles putting
	 * characters out to the polled device.
	 */
	void		(*cons_polledio_putchar)(struct cons_polledio_arg *,
				uchar_t);

	/*
	 * Pointer to the routine and its argument that handles getting
	 * characters from the polled device.  This routine is blocking.
	 */
	int		(*cons_polledio_getchar)(struct cons_polledio_arg *);

	/*
	 * Pointer to the routine and its argument that checks to see
	 * if a character is pending input.  This routine is non-blocking.
	 */
	boolean_t	(*cons_polledio_ischar)(struct cons_polledio_arg *);

	/*
	 * Initialize the polled subsystem.  This routine is called once
	 * per mode change from non-polled to polled mode.
	 */
	void		(*cons_polledio_enter)(struct cons_polledio_arg *);

	/*
	 * Restore the non-polled subsystem.  This routine is called once
	 * per mode change from non-polled to polled mode.
	 */
	void		(*cons_polledio_exit)(struct cons_polledio_arg *);

} cons_polledio_t;

/*
 * Workstation Console
 */
#define	_WCIOC		(('W'<<24)|('C'<<16))
#if	defined(_CONSOLE_OUTPUT_VIA_SOFTWARE)
#define	WC_OPEN_FB	(_WCIOC | 0)
#define	WC_CLOSE_FB	(_WCIOC | 1)
#endif

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONSDEV_H */
