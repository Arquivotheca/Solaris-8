/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _MACHLIBTHREAD_H
#define	_MACHLIBTHREAD_H

#pragma ident	"@(#)machlibthread.h	1.20	98/08/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <setjmp.h>

typedef struct {
	long	rs_sp;
	long	rs_pc;
	long	rs_fsr;
	long	rs_fpu_en;
	long	rs_g2;
	long	rs_g3;
#ifndef __sparcv9
	long	rs_g4;
	uint8_t	rs_t_lockflush;	/* for load-store order during unlock */
#endif /* __sparcv9 */
} resumestate_t;

#ifdef _SYSCALL32_IMPL

typedef struct {
	int	rs_sp;
	int	rs_pc;
	int	rs_fsr;
	int	rs_fpu_en;
	int	rs_g2;
	int	rs_g3;
	int	rs_g4;
	uint8_t	rs_t_lockflush;	/* for load-store order during unlock */
} resumestate32_t;

#endif /* _SYSCALL32_IMPL */

#define	t_sp		t_resumestate.rs_sp
#define	t_pc		t_resumestate.rs_pc
#define	t_fsr		t_resumestate.rs_fsr
#define	t_fpu_en	t_resumestate.rs_fpu_en
#define	t_g2		t_resumestate.rs_g2
#define	t_g3		t_resumestate.rs_g3
#ifndef __sparcv9
#define	t_g4		t_resumestate.rs_g4
#endif /* __sparcv9 */
#define	t_fp		t_sp

/*
 * sigjmp_struct_t is used by libthread's version of sigsetjmp().
 * The structure MUST match the ABI size specifier _SIGJBLEN.
 * This is 19 (words). The ABI value for _JBLEN is 12 (words). A sigset_t
 * is 16 bytes and a stack_t is 12 bytes.
 */
typedef struct sigjmp_struct {
	int	sjs_flags;	/*	JBUF[ 0]	*/
	greg_t	sjs_sp;		/*	JBUF[ 1]	*/
	greg_t	sjs_pc;		/*	JBUF[ 2]	*/
	greg_t	sjs_g2;		/*	JBUF[ 3]	*/
	greg_t	sjs_g3;		/*	JBUF[ 4]	*/
#ifndef __sparcv9
	greg_t	sjs_g4;		/*	JBUF[ 5]	*/
	u_long	sjs_pad[_JBLEN-6];
#else
	u_long	sjs_pad[_JBLEN-5];
#endif /* __sparcv9 */
	sigset_t	sjs_sigmask;
	stack_t	sjs_stack;
} sigjmp_struct_t;

#ifdef __sparcv9
uintptr_t	_flush_and_tell(void);
ulong_t		_getfprs(void);
caddr_t		_getsp(void);
#endif /* __sparcv9 */

#ifdef	__cplusplus
}
#endif

#endif	/* _MACHLIBTHREAD_H */
