/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BEEP_H
#define	_SYS_BEEP_H

#pragma ident	"@(#)beep.h	1.5	99/03/05 SMI"

/*
 * Interface to the system beeper.
 *
 * (This is the API, not the hardware interface.)
 */

#ifdef __cplusplus
extern "C" {
#endif

#if	defined(_KERNEL)
enum beep_type { BEEP_DEFAULT = 0, BEEP_CONSOLE = 1, BEEP_TYPE4 = 2 };

void beep(enum beep_type);
void beep_polled(enum beep_type);
void beeper_on(enum beep_type);
void beeper_off(void);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BEEP_H */
