/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MACHTRAP_H
#define	_SYS_MACHTRAP_H

#pragma ident	"@(#)machtrap.h	1.10	99/08/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file is machine specific as is.
 * Some trap types could be made common
 * for all sparcs, but that is a project
 * in and of itself.
 */

/*
 * Hardware traps.
 */
#define	T_POR			0x001
#define	T_WDR			0x002
#define	T_XIR			0x003
#define	T_SIR			0x004
#define	T_RED_EXCEPTION		0x005
#define	T_INSTR_EXCEPTION	0x008
#define	T_INSTR_MMU_MISS	0x009
#define	T_INSTR_ERROR		0x00A
#define	T_UNIMP_INSTR		0x010
#define	T_PRIV_INSTR		0x011
#define	T_FP_DISABLED		0x020
#define	T_FP_EXCEPTION_IEEE	0x021
#define	T_FP_EXCEPTION_OTHER	0x022
#define	T_TAG_OVERFLOW		0x023
#define	T_CLEAN_WINDOW		0x024
#define	T_IDIV0			0x028
#define	T_DATA_EXCEPTION	0x030
#define	T_DATA_MMU_MISS		0x031
#define	T_DATA_ERROR		0x032
#define	T_DATA_PROT		0x033
#define	T_ALIGNMENT		0x034
#define	T_LDDF_ALIGN		0x035
#define	T_STDF_ALIGN		0x036
#define	T_PRIV_ACTION		0x037
#define	T_ASYNC_ERROR		0x040
#define	T_INT_LEVEL_1		0x041
#define	T_INT_LEVEL_2		0x042
#define	T_INT_LEVEL_3		0x043
#define	T_INT_LEVEL_4		0x044
#define	T_INT_LEVEL_5		0x045
#define	T_INT_LEVEL_6		0x046
#define	T_INT_LEVEL_7		0x047
#define	T_INT_LEVEL_8		0x048
#define	T_INT_LEVEL_9		0x049
#define	T_INT_LEVEL_10		0x04A
#define	T_INT_LEVEL_11		0x04B
#define	T_INT_LEVEL_12		0x04C
#define	T_INT_LEVEL_13		0x04D
#define	T_INT_LEVEL_14		0x04E
#define	T_INT_LEVEL_15		0x04F
#define	T_PA_WATCHPOINT		0x61
#define	T_VA_WATCHPOINT		0x62
#define	T_WIN_OVERFLOW		0x80
#define	T_WIN_UNDERFLOW		0xC0


/*
 * Software trap type values.
 */
#define	T_SOFTWARE_TRAP		0x100
#define	T_ESOFTWARE_TRAP	0x1FF
#define	T_OSYSCALL		(T_SOFTWARE_TRAP + ST_OSYSCALL)
#define	T_BREAKPOINT		(T_SOFTWARE_TRAP + ST_BREAKPOINT)
#define	T_DIV0			(T_SOFTWARE_TRAP + ST_DIV0)
#define	T_FLUSH_WINDOWS		(T_SOFTWARE_TRAP + ST_FLUSH_WINDOWS)
#define	T_CLEAN_WINDOWS		(T_SOFTWARE_TRAP + ST_CLEAN_WINDOWS)
#define	T_RANGE_CHECK		(T_SOFTWARE_TRAP + ST_RANGE_CHECK)
#define	T_FIX_ALIGN		(T_SOFTWARE_TRAP + ST_FIX_ALIGN)
#define	T_INT_OVERFLOW		(T_SOFTWARE_TRAP + ST_INT_OVERFLOW)
#define	T_SYSCALL		(T_SOFTWARE_TRAP + ST_SYSCALL)
#define	T_GETCC			(T_SOFTWARE_TRAP + ST_GETCC)
#define	T_SETCC			(T_SOFTWARE_TRAP + ST_SETCC)
#define	T_HOTPATCH_TRAP		(T_SOFTWARE_TRAP + ST_HOTPATCH_TRAP)


#define	T_AST			0x200
#define	T_FLUSH_PCB		(T_AST + 0x10)
#define	T_SYS_RTT_PAGE		(T_AST + 0x20)

/* user mode flag added to trap type */
#define	T_USER			0x10000


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTRAP_H */
