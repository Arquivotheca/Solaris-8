/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_DKTP_CHS_DEBUG_H
#define	_SYS_DKTP_CHS_DEBUG_H


#pragma	ident	"@(#)chs_debug.h	1.3	99/01/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern	char	*dbits;
extern	char	*sbits;

extern	ulong	chs_debug_flags;

#if defined(CHS_DEBUG)
#define	CHS_DBGPR(m, args)	\
			((chs_debug_flags & (m)) \
			? (void) printf args \
			: (void) 0)
#else	/* ! defined(CHS_DEBUG) */
#define	CHS_DBGPR(m, args) ((void) 0)
#endif	/* defined(CHS_DEBUG) */

#define	MDBG0(args)	CHS_DBGPR(0x01, args)
#define	MDBG1(args)	CHS_DBGPR(0x02, args)
#define	MDBG2(args)	CHS_DBGPR(0x04, args)
#define	MDBG3(args)	CHS_DBGPR(0x08, args)

#define	MDBG4(args)	CHS_DBGPR(0x10, args)
#define	MDBG5(args)	CHS_DBGPR(0x20, args)
#define	MDBG6(args)	CHS_DBGPR(0x40, args)
#define	MDBG7(args)	CHS_DBGPR(0x80, args)

#define	MDBG8(args)	CHS_DBGPR(0x0100, args)
#define	MDBG9(args)	CHS_DBGPR(0x0200, args)
#define	MDBG10(args)	CHS_DBGPR(0x0400, args)
#define	MDBG11(args)	CHS_DBGPR(0x0800, args)

#define	MDBG12(args)	CHS_DBGPR(0x1000, args)
#define	MDBG13(args)	CHS_DBGPR(0x2000, args)
#define	MDBG14(args)	CHS_DBGPR(0x4000, args)
#define	MDBG15(args)	CHS_DBGPR(0x8000, args)

#define	MDBG16(args)	CHS_DBGPR(0x010000, args)
#define	MDBG17(args)	CHS_DBGPR(0x020000, args)
#define	MDBG18(args)	CHS_DBGPR(0x040000, args)
#define	MDBG19(args)	CHS_DBGPR(0x080000, args)

#define	MDBG20(args)	CHS_DBGPR(0x100000, args)
#define	MDBG21(args)	CHS_DBGPR(0x200000, args)
#define	MDBG22(args)	CHS_DBGPR(0x400000, args)
#define	MDBG23(args)	CHS_DBGPR(0x800000, args)

#define	MDBG24(args)	CHS_DBGPR(0x1000000, args)
#define	MDBG25(args)	CHS_DBGPR(0x2000000, args)
#define	MDBG26(args)	CHS_DBGPR(0x4000000, args)
#define	MDBG27(args)	CHS_DBGPR(0x8000000, args)

#define	MDBG28(args)	CHS_DBGPR(0x10000000, args)
#define	MDBG29(args)	CHS_DBGPR(0x20000000, args)
#define	MDBG30(args)	CHS_DBGPR(0x40000000, args)
#define	MDBG31(args)	CHS_DBGPR(0x80000000, args)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_CHS_DEBUG_H */
