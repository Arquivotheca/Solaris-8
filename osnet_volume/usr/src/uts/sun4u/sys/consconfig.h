/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CONSCONFIG_H
#define	_SYS_CONSCONFIG_H

#pragma ident	"@(#)consconfig.h	1.4	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This structure contains the information for
 * an open device.
 */
typedef struct consconfig_vnode {

	/* vnode pointer */
	vnode_t	*consconfig_vp;

	/* file structure pointer */
	file_t	*consconfig_fp;

	/* file ptr (ctxt of calling thread) */
	int	consconfig_fd;

	/* device type */
	dev_t	consconfig_dev;
} consconfig_vnode_t;

/*
 * This structure maintains the state of the console.
 */
typedef struct cons_state {

	/* conskbd file ptrs */
	consconfig_vnode_t  *cons_kbd_avp;

	/* wc file pointers */
	consconfig_vnode_t  *cons_rws_avp;

	/* final console file pointer */
	consconfig_vnode_t  *cons_final_avp;

	/* protects kb configuration */
	kmutex_t	cons_kb_mutex;

	/* protects ms configuration */
	kmutex_t	cons_ms_mutex;

	/* state of driver loading */
	int		cons_dacf_booting;

	/* kb major and minor number */
	major_t		cons_kb_major;
	minor_t		cons_kb_minor;

	/* ms major and minor number */
	major_t		cons_ms_major;
	minor_t		cons_ms_minor;

	/* mux id's */
	int		cons_kb_muxid;
	int		cons_ms_muxid;
	int		cons_conskbd_muxid;

	/* Keyboard path */
	char    *cons_keyboard_path;

	/* Mouse path */
	char    *cons_mouse_path;

	/* Standard input path. */
	char    *cons_stdin_path;

	/* Standard output path */
	char    *cons_stdout_path;

	/* Type of console input (See below) */
	int	cons_input_type;

	/* Indicates problem with console keyboard */
	int	cons_keyboard_problem;

} cons_state_t;


/*
 * Types of console input
 */
#define	CONSOLE_INPUT_KEYBOARD		0x1	/* keyboard */
#define	CONSOLE_INPUT_TTY		0x2	/* serial line */
#define	CONSOLE_INPUT_SERIAL_KEYBOARD	0x4	/* serial kbd */

/*
 * These macros indicate the state of the system while
 * the console configuration is running.
 * CONSCONFIG_BOOTING implies that the driver loading
 * is in process during boot.  CONSCONFIG_DRIVERS_LOADED
 * means that the driver loading during boot has completed.
 *
 * During driver loading while the boot is happening, the
 * keyboard and mouse minor nodes that are hooked into the console
 * stream must match those defined by the firmware.  After boot
 * minor nodes are hooked according to a first come first serve
 * basis.
 */
#define	CONSCONFIG_BOOTING			1
#define	CONSCONFIG_DRIVERS_LOADED		0

/*
 * If there is a problem with the console keyboard and polled input
 * is required, then the polled input won't get setup properly.  If
 * another keyboard is hot-attached, attempt to register console input
 * for that device.
 */
#define	CONSCONFIG_KB_PROBLEM			1

/*
 * Debug information
 */
/*
 * Severity levels for printing
 */
#define	DPRINT_L0	0	/* print every message */
#define	DPRINT_L1	1	/* debug */
#define	DPRINT_L2	2	/* minor errors */
#define	DPRINT_L3	3	/* major errors */
#define	DPRINT_L4	4	/* catastophic errors */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONSCONFIG_H */
