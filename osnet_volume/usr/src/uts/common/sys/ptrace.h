/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PTRACE_H
#define	_SYS_PTRACE_H

#pragma ident	"@(#)ptrace.h	1.28	96/06/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Request values for the ptrace system call
 */

/*
 * XXX - SunOS 5.0 development version: 0-9 correspond to AT&T/SVID defined
 * requests. The remainder are extensions as defined for SunOS 4.1. Currently
 * only GETREGS, SETREGS, GETFPREGS, and SETFPREGS are implemented
 */
#define	PTRACE_TRACEME		0	/* 0, by tracee to begin tracing */
#define	PTRACE_CHILDDONE	0	/* 0, tracee is done with his half */
#define	PTRACE_PEEKTEXT		1	/* 1, read word from text segment */
#define	PTRACE_PEEKDATA		2	/* 2, read word from data segment */
#define	PTRACE_PEEKUSER		3	/* 3, read word from user struct */
#define	PTRACE_POKETEXT		4	/* 4, write word into text segment */
#define	PTRACE_POKEDATA		5	/* 5, write word into data segment */
#define	PTRACE_POKEUSER		6	/* 6, write word into user struct */
#define	PTRACE_CONT		7	/* 7, continue process */
#define	PTRACE_KILL		8	/* 8, terminate process */
#define	PTRACE_SINGLESTEP	9	/* 9, single step process */
#define	PTRACE_ATTACH		10	/* 10, attach to an existing process */
#define	PTRACE_DETACH		11	/* 11, detach from a process */
#define	PTRACE_GETREGS		12	/* 12, get all registers */
#define	PTRACE_SETREGS		13	/* 13, set all registers */
#define	PTRACE_GETFPREGS	14	/* 14, get all floating point regs */
#define	PTRACE_SETFPREGS	15	/* 15, set all floating point regs */
#define	PTRACE_READDATA		16	/* 16, read data segment */
#define	PTRACE_WRITEDATA	17	/* 17, write data segment */
#define	PTRACE_READTEXT		18	/* 18, read text segment */
#define	PTRACE_WRITETEXT	19	/* 19, write text segment */
#define	PTRACE_GETFPAREGS	20	/* 20, get all fpa regs */
#define	PTRACE_SETFPAREGS	21	/* 21, set all fpa regs */
#define	PTRACE_GETWINDOW	22	/* 22, get register window n */
#define	PTRACE_SETWINDOW	23	/* 23, set register window n */
#define	PTRACE_SYSCALL		24	/* 24, trap next sys call */
#define	PTRACE_DUMPCORE		25	/* 25, dump process core */
#define	PTRACE_SETWRBKPT	26	/* 26, set write breakpoint */
#define	PTRACE_SETACBKPT	27	/* 27, set access breakpoint */
#define	PTRACE_CLRDR7		28	/* 28, clear debug register 7 */
#define	PTRACE_TRAPCODE		29	/* get proc's trap code */
#define	PTRACE_SETBPP		30	/* set hw instruction breakpoint */
#define	PTRACE_WPPHYS		31	/* watchpoint on phys addr (sun4u) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PTRACE_H */
