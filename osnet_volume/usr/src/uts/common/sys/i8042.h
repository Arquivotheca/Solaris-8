/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_I8042_H
#define	_SYS_I8042_H

#pragma ident	"@(#)i8042.h	1.6	99/03/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Here's the interface to the virtual registers on the device.
 *
 * Normal interrupt-driven I/O:
 *
 * I8042_INT_INPUT_AVAIL
 *	Interrupt mode input bytes available?  Zero = No.
 * I8042_INT_INPUT_DATA
 *	Fetch interrupt mode input byte.
 * I8042_INT_OUTPUT_DATA
 *	Interrupt mode output byte.
 *
 * Polled I/O, used by (e.g.) kadb, when normal system services are
 * unavailable:
 *
 * I8042_POLL_INPUT_AVAIL
 *	Polled mode input bytes available?  Zero = No.
 * I8042_POLL_INPUT_DATA
 *	Polled mode input byte.
 * I8042_POLL_OUTPUT_DATA
 *	Polled mode output byte.
 */

#define	I8042_INT_INPUT_AVAIL	0x00
#define	I8042_INT_INPUT_DATA	0x01
#define	I8042_INT_OUTPUT_DATA	0x03
#define	I8042_POLL_INPUT_AVAIL	0x10
#define	I8042_POLL_INPUT_DATA	0x11
#define	I8042_POLL_OUTPUT_DATA	0x13

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_I8042_H */
