/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CONSCONFIG_DACF_H
#define	_SYS_CONSCONFIG_DACF_H

#pragma ident	"@(#)consconfig_dacf.h	1.3	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This structure contains the information for
 * an open device.
 */
typedef struct consconfig_dacf_vnode {

	/* vnode pointer */
	vnode_t	*consconfig_dacf_vp;

	/* file structure pointer */
	file_t	*consconfig_dacf_fp;

	/* file ptr (ctxt of calling thread) */
	int	consconfig_dacf_fd;

	/* device type */
	dev_t	consconfig_dacf_dev;
} consconfig_dacf_vnode_t;

/*
 * Helper functions for accessing consconfig_dacf_vnode_t
 */
#define	CONSCONFIG_VNODE(avp)   ((avp)->consconfig_dacf_vp)
#define	CONSCONFIG_FILE(avp)    ((avp)->consconfig_dacf_fp)
#define	CONSCONFIG_FD(avp)	((avp)->consconfig_dacf_fd)
#define	CONSCONFIG_DEV(avp)	((avp)->consconfig_dacf_dev)

/*
 * This structure contains information about the console
 */
typedef struct consconfig_dacf_info {

	/* Keyboard path */
	char	*consconfig_dacf_keyboard_path;

	/* Mouse path */
	char	*consconfig_dacf_mouse_path;

	/* Frame Buffer path */
	char	*consconfig_dacf_fb_path;

	/* Standard input path. */
	char	*consconfig_dacf_stdin_path;

	/* Standard output path */
	char	*consconfig_dacf_stdout_path;

	/* Type of console input (See below) */
	int	consconfig_dacf_console_input_type;

	/* Indicates problem with console keyboard */
	boolean_t	consconfig_dacf_keyboard_problem;

} consconfig_dacf_info_t;

/*
 * Types of console input
 */
#define	CONSOLE_INPUT_UNSUPPORTED	0	/* unsupported */
#define	CONSOLE_INPUT_KEYBOARD		1	/* keyboard */
#define	CONSOLE_INPUT_TTY		2	/* serial line */

/*
 * These macros indicate the state of the system while
 * the console configuration is running.
 * CONSCONFIG_DACF_BOOTING implies that the driver loading
 * is in process during boot.  CONSCONFIG_DACF_DRIVERS_LOADED
 * means that the driver loading during boot has completed.
 *
 * During driver loading while the boot is happening, the
 * keyboard and mouse minor nodes that are hooked into the console
 * stream must match those defined by the firmware.  After boot
 * minor nodes are hooked according to a first come first serve
 * basis.
 */
#define	CONSCONFIG_DACF_BOOTING			1
#define	CONSCONFIG_DACF_DRIVERS_LOADED		0

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

#define	DPRINTF consconfig_dacf_dprintf

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONSCONFIG_DACF_H */
