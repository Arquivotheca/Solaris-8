/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BEEP_DRIVER_H
#define	_SYS_BEEP_DRIVER_H

#pragma ident	"@(#)beep_driver.h	1.1	99/09/15 SMI"

/*
 * beep_driver.h : Registration functions of beep driver to beeper module
 */

#ifdef __cplusplus
extern "C" {
#endif

#if	defined(_KERNEL)

/*
 * Registration mechanism of the hardware dependent
 * beep driver to the beeper module for sun4u platforms.
 */
int beep_init(dev_info_t *, void (*)(dev_info_t *),
		void (*)(dev_info_t *), void (*)(dev_info_t *, int));


#endif  /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BEEP_DRIVER_H */
