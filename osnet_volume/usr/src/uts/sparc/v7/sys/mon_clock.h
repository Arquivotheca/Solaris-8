/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MON_CLOCK_H
#define	_SYS_MON_CLOCK_H

#pragma ident	"@(#)mon_clock.h	1.1	99/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#define	MON_CLK_EXCLUSIVE	0
#define	MON_CLK_EXCLDISBL	1
#define	MON_CLK_SHARED		2
#define	MON_CLK_DISABLED	3

#ifndef _ASM

#include <sys/processor.h>
#include <sys/types.h>
#include <sys/scb.h>

extern void mon_clock_init(void);
extern void mon_clock_start(void);
extern void mon_clock_stop(void);
extern void mon_clock_share(void);
extern void mon_clock_unshare(void);

extern char mon_clock;
extern char mon_clock_go;
extern processorid_t mon_clock_cpu;

extern trapvec mon_clock14_vec;
extern trapvec kclock14_vec;

/*
 * The platform must provide the following functions.  mon_clock will only
 * call level14_enable() or level14_disable() when it is in an exclusive
 * state (either MON_CLK_EXCLUSIVE or MON_CLK_EXCLDISBL).
 */
extern uint_t level14_nsec(processorid_t);
extern void level14_soft(processorid_t);
extern void level14_enable(processorid_t, uint_t);
extern void level14_disable(processorid_t);

#endif /* !_ASM */

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MON_CLOCK_H */
