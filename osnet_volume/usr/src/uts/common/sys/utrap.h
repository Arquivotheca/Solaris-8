/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 */

#ifndef _UTRAP_H
#define	_UTRAP_H

#pragma ident	"@(#)utrap.h	1.5	99/01/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * this file contains definitions for user-level traps.
 */

#define	UT_INSTRUCTION_DISABLED			1
#define	UT_INSTRUCTION_ERROR			2
#define	UT_INSTRUCTION_PROTECTION		3
#define	UT_ILLTRAP_INSTRUCTION			4
#define	UT_ILLEGAL_INSTRUCTION			5
#define	UT_PRIVILEGED_OPCODE			6
#define	UT_FP_DISABLED				7
#define	UT_FP_EXCEPTION_IEEE_754		8
#define	UT_FP_EXCEPTION_OTHER			9
#define	UT_TAG_OVERFLOW				10
#define	UT_DIVISION_BY_ZERO			11
#define	UT_DATA_EXCEPTION			12
#define	UT_DATA_ERROR				13
#define	UT_DATA_PROTECTION			14
#define	UT_MEM_ADDRESS_NOT_ALIGNED		15
#define	UT_PRIVILEGED_ACTION			16
#define	UT_ASYNC_DATA_ERROR			17
#define	UT_TRAP_INSTRUCTION_16			18
#define	UT_TRAP_INSTRUCTION_17			19
#define	UT_TRAP_INSTRUCTION_18			20
#define	UT_TRAP_INSTRUCTION_19			21
#define	UT_TRAP_INSTRUCTION_20			22
#define	UT_TRAP_INSTRUCTION_21			23
#define	UT_TRAP_INSTRUCTION_22			24
#define	UT_TRAP_INSTRUCTION_23			25
#define	UT_TRAP_INSTRUCTION_24			26
#define	UT_TRAP_INSTRUCTION_25			27
#define	UT_TRAP_INSTRUCTION_26			28
#define	UT_TRAP_INSTRUCTION_27			29
#define	UT_TRAP_INSTRUCTION_28			30
#define	UT_TRAP_INSTRUCTION_29			31
#define	UT_TRAP_INSTRUCTION_30			32
#define	UT_TRAP_INSTRUCTION_31			33

/*
 * These defines exist only for the private v8plus install_utrap interface.
 */
#define	UTRAP_V8P_FP_DISABLED			UT_FP_DISABLED
#define	UTRAP_V8P_MEM_ADDRESS_NOT_ALIGNED	UT_MEM_ADDRESS_NOT_ALIGNED

#ifndef _ASM

#define	UTH_NOCHANGE ((utrap_handler_t)(-1))
#define	UTRAP_UTH_NOCHANGE	UTH_NOCHANGE

typedef int utrap_entry_t;
typedef void *utrap_handler_t;	/* user trap handler entry point */

int
install_utrap(utrap_entry_t type, utrap_handler_t new_handler,
	utrap_handler_t *old_handlerp);

#ifdef __sparcv9cpu
struct proc;
void utrap_dup(struct proc *pp, struct proc *cp);
void utrap_free(struct proc *p);
#endif

#ifdef __sparcv9
int
__sparc_utrap_install(utrap_entry_t type,
	utrap_handler_t new_precise, utrap_handler_t new_deferred,
	utrap_handler_t *old_precise, utrap_handler_t *old_deferred);
#endif

/* The trap_instruction user traps are precise only. */
#define	UT_PRECISE_MAXTRAPS				33

#endif /* ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _UTRAP_H */
