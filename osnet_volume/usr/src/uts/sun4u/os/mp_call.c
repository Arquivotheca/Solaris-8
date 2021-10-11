/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_call.c	1.10	99/04/13 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 */

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/systm.h>
#include <sys/intr.h>

/*
 * Interrupt another CPU.
 * 	This is useful to make the other CPU go through a trap so that
 *	it recognizes an address space trap (AST) for preempting a thread.
 *
 *	It is possible to be preempted here and be resumed on the CPU
 *	being poked, so it isn't an error to poke the current CPU.
 *	We could check this and still get preempted after the check, so
 *	we don't bother.
 */
void
poke_cpu(int cpun)
{
	/*
	 * If panicstr is set or a poke_cpu is already pending,
	 * no need to send another one.
	 */
	if (!panicstr && cpu[cpun]->cpu_m.poke_cpu_outstanding != B_TRUE) {
		cpu[cpun]->cpu_m.poke_cpu_outstanding = B_TRUE;
		xt_one(cpun, (xcfunc_t *)poke_cpu_inum, 0, 0);
	}
}
