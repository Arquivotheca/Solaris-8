/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA32_SYS_STACK_H
#define	_IA32_SYS_STACK_H

#pragma ident	"@(#)stack.h	1.1	99/05/04 SMI"

#if !defined(_ASM)

#include <sys/types.h>

#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * In the Intel world, a stack frame looks like this:
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
 * Stack alignment macros.
 */
#define	STACK_ALIGN32	4
#define	STACK_BIAS32	0
#define	SA32(X)		(((X)+(STACK_ALIGN32-1)) & ~(STACK_ALIGN32-1))
#define	MINFRAME32	0

#define	STACK_ALIGN	STACK_ALIGN32
#define	STACK_BIAS	STACK_BIAS32
#define	SA(X)		SA32(X)
#define	MINFRAME	MINFRAME32

#if defined(_KERNEL) && !defined(_ASM)

struct regs;

void traceregs(struct regs *);
void traceback(caddr_t);
void tracedump(void);

#endif /* defined(_KERNEL) && !defined(_ASM) */

#define	STACK_GROWTH_DOWN /* stacks grow from high to low addresses */

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_STACK_H */
