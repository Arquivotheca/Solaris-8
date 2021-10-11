/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYSMSG_IMPL_H
#define	_SYS_SYSMSG_IMPL_H

#pragma ident	"@(#)sysmsg_impl.h	1.2	99/01/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	SYSMSG		"/dev/sysmsg"

/*
 * consadm(1M) uses these ioctls to interface with /dev/sysmsg.
 */

/*
 * When the ioctl is called with a zero length buffer, then the
 * /dev/sysmsg module will return the size of the buffer needed to
 * contain a space separated list of auxiliary device names.
 *
 * When a buffer of the correct size is provided, the ioctl returns
 * a space separated list of auxiliary device names.
 */
#define	CIOCGETCONSOLE	0

/*
 * Set the given device to be an auxiliary console.  This will cause
 * console messages to also appear on that device.
 */
#define	CIOCSETCONSOLE	1

/*
 * Unset the given device as an auxiliary console.  Console
 * messages will not be displayed on that device any longer.
 */
#define	CIOCRMCONSOLE	2

/*
 * Return the dev_t for the controlling tty
 */
#define	CIOCTTYCONSOLE	3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSMSG_IMPL_H */
