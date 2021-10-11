/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKTP_MLX_DEBUG_H
#define	_SYS_DKTP_MLX_DEBUG_H

#pragma ident	"@(#)debug.h	1.5	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern	char	*dbits;
extern	char	*sbits;

extern	ulong_t	mlx_debug_flags;

#if defined(MLX_DEBUG)
#define	MLX_DBGPR(m, args)	\
	((mlx_debug_flags & (m)) ? (void) printf args : (void) 0)
#else	/* ! defined(MLX_DEBUG) */
#define	MLX_DBGPR(m, args) ((void) 0)
#endif	/* defined(MLX_DEBUG) */

#define	MDBG0(args)	MLX_DBGPR(0x01, args)
#define	MDBG1(args)	MLX_DBGPR(0x02, args)
#define	MDBG2(args)	MLX_DBGPR(0x04, args)
#define	MDBG3(args)	MLX_DBGPR(0x08, args)

#define	MDBG4(args)	MLX_DBGPR(0x10, args)
#define	MDBG5(args)	MLX_DBGPR(0x20, args)
#define	MDBG6(args)	MLX_DBGPR(0x40, args)
#define	MDBG7(args)	MLX_DBGPR(0x80, args)

#define	MDBG8(args)	MLX_DBGPR(0x0100, args)
#define	MDBG9(args)	MLX_DBGPR(0x0200, args)
#define	MDBG10(args)	MLX_DBGPR(0x0400, args)
#define	MDBG11(args)	MLX_DBGPR(0x0800, args)

#define	MDBG12(args)	MLX_DBGPR(0x1000, args)
#define	MDBG13(args)	MLX_DBGPR(0x2000, args)
#define	MDBG14(args)	MLX_DBGPR(0x4000, args)
#define	MDBG15(args)	MLX_DBGPR(0x8000, args)

#define	MDBG16(args)	MLX_DBGPR(0x010000, args)
#define	MDBG17(args)	MLX_DBGPR(0x020000, args)
#define	MDBG18(args)	MLX_DBGPR(0x040000, args)
#define	MDBG19(args)	MLX_DBGPR(0x080000, args)

#define	MDBG20(args)	MLX_DBGPR(0x100000, args)
#define	MDBG21(args)	MLX_DBGPR(0x200000, args)
#define	MDBG22(args)	MLX_DBGPR(0x400000, args)
#define	MDBG23(args)	MLX_DBGPR(0x800000, args)

#define	MDBG24(args)	MLX_DBGPR(0x1000000, args)
#define	MDBG25(args)	MLX_DBGPR(0x2000000, args)
#define	MDBG26(args)	MLX_DBGPR(0x4000000, args)
#define	MDBG27(args)	MLX_DBGPR(0x8000000, args)

#define	MDBG28(args)	MLX_DBGPR(0x10000000, args)
#define	MDBG29(args)	MLX_DBGPR(0x20000000, args)
#define	MDBG30(args)	MLX_DBGPR(0x40000000, args)
#define	MDBG31(args)	MLX_DBGPR(0x80000000, args)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MLX_DEBUG_H */
