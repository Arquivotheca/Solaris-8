/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _CPUSAVE_H
#define	_CPUSAVE_H

#pragma ident	"@(#)cpusave.h	1.2	98/08/17 SMI"

/*
 * Declarations and definitions pertaining to the per-CPU saved
 * state.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM
#include <sys/fsr.h>

#include "allregs.h"
#endif

/*
 *  These constants indicate where a cpu is currently.
 *  On entering the debugger in save_cpu_state() or trap(),
 *  a cpu writes CPU_STATUS_SLAVE or CPU_STATUS_MASTER
 *  respectively into its status field.  On exit, it writes
 *  CPU_STATUS_RUNNING.
 */

#define	CPU_STATUS_RUNNING	0	/* running in the kernel */
#define	CPU_STATUS_MASTER	1	/* entering via trap() */
#define	CPU_STATUS_SLAVE	2	/* entering via save_cpu_state() */
#define	CPU_STATUS_INACTIVE	3	/* not a valid CPU */


#ifndef _ASM


/*
 *  On entry to the debugger, each cpu saves its state in a struct
 *  cpu_regsave for ultimate examination by the user.  On exit,
 *  the cpu restores its state from the same structure.  Changes
 *  made to the processor's registers thus show up when the cpu
 *  resumes execution in the kernel.
 */

typedef unsigned cpu_status_t;

struct cpu_regsave {
	cpu_status_t		cpu_status;
	struct allregs_v9	cpu_regs;
	v9_fpregset_t		cpu_fpregs;
};

#endif

#ifdef __cplusplus
}
#endif

#endif /* _CPUSAVE_H */
