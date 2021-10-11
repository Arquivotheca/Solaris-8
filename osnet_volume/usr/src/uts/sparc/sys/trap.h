/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_TRAP_H
#define	_SYS_TRAP_H

#pragma ident	"@(#)trap.h	1.29	99/08/03 SMI"

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
 * Software traps (ticc instructions).
 */
#define	ST_OSYSCALL		0x00
#define	ST_BREAKPOINT		0x01
#define	ST_DIV0			0x02
#define	ST_FLUSH_WINDOWS	0x03
#define	ST_CLEAN_WINDOWS	0x04
#define	ST_RANGE_CHECK		0x05
#define	ST_FIX_ALIGN		0x06
#define	ST_INT_OVERFLOW		0x07
#define	ST_SYSCALL		0x08
#define	ST_HOTPATCH_TRAP	0x0a

/*
 * Software trap vectors 16 - 31 are reserved for use by the user
 * and will not be usurped by Sun.
 */

#define	ST_GETCC		0x20
#define	ST_SETCC		0x21
#define	ST_GETPSR		0x22
#define	ST_SETPSR		0x23
#define	ST_GETHRTIME		0x24
#define	ST_GETHRVTIME		0x25
#define	ST_SELFXCALL		0x26
#define	ST_GETHRESTIME		0x27
#define	ST_SETV9STACK		0x28

/*
 * Tracing traps.
 * The ST_TRACE_* traps are used to implement kernel tracing in kernels
 * built with -DTRACE.  In normal kernels, these are BAD_TRAPs.
 */
#define	ST_TRACE_0		0x30
#define	ST_TRACE_1		0x31
#define	ST_TRACE_2		0x32
#define	ST_TRACE_3		0x33
#define	ST_TRACE_4		0x34
#define	ST_TRACE_5		0x35
#define	ST_TRACE_RESERVED	0x36
#define	ST_TRACE_WRITE_BUFFER	0x37

#define	ST_KADB_TRAP		0x7d
#define	ST_KADB_BREAKPOINT	0x7e
#define	ST_MON_BREAKPOINT	0x7f

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
