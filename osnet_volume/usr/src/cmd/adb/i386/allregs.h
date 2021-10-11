/*
 * Copyright (c) 1986-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)allregs.h	1.3     96/02/13 SMI"

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "adb_regs" structure.
 */

#ifndef _ALLREGS_H
#define	_ALLREGS_H

#ifndef rw_fp
#include <sys/reg.h>
#endif /* !rw_fp */

#ifndef _ASM
#include <sys/pcb.h>
#define	allregs regs
#endif /* !_ASM */

#endif /* !_ALLREGS_H */
