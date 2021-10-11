/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA64_SYS_STACK_H
#define	_IA64_SYS_STACK_H

#pragma ident	"@(#)stack.h	1.1	99/05/04 SMI"

#if !defined(_ASM)

#include <sys/types.h>

#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * On the ia32 architecture, a stack frame looks like this:
 *
 * %fp0->|				 |
 *	 |-------------------------------|
 *	 |  Args to next subroutine      |
 *	 |-------------------------------|-\
 * %sp0->|  One word struct-ret address	 | |
 *	 |-------------------------------|  > minimum stack frame (8 bytes)
 *	 |  Previous frame pointer (%fp0)| |
 * %fp1->|-------------------------------|-/
 *	 |  Local variables              |
 * %sp1->|-------------------------------|
 */

/*
 * ia32 stack alignment macros.
 */
#define	STACK_ALIGN32	4
#define	STACK_BIAS32	0
#define	SA32(X)	(((X)+(STACK_ALIGN32-1)) & ~(STACK_ALIGN32-1))
#define	MINFRAME32	0

/*
 * ia64 stack alignment macros.
 */
#define	STACK_ALIGN	16
#define	STACK_BIAS	0
#define	SA(X)		(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))
#define	MINFRAME	16

#if defined(_KERNEL) && !defined(_ASM)

struct regs;

void traceregs(struct regs *);
void traceback(caddr_t);
void tracedump(void);

#endif /* defined(_KERNEL) && !defined(_ASM) */

#define	STACK_GROWTH_DOWN

#ifdef	__cplusplus
}
#endif

#endif	/* _IA64_SYS_STACK_H */
