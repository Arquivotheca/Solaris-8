/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)debug.h	1.1	97/07/21 SMI"
 */

/*
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

/*
 * This file was derived from the debug.h file in the Solaris
 * driver.
 */


extern	char	*dbits;
extern	char	*sbits;

extern	ulong	ncr_debug_flags;

#if defined(NCR_DEBUG)
#define	NCR_DBGPR(m, args)	\
		(void) ((ncr_debug_flags & (m)) \
			? (void) printf args \
			: (void) 0)
#else	/* ! defined(NCR_DEBUG) */
#define	NCR_DBGPR(m, args) ((void) 0)
#endif	/* defined(NCR_DEBUG) */

#define	NDBG0(args)	NCR_DBGPR(0x01, args)
#define	NDBG1(args)	NCR_DBGPR(0x02, args)
#define	NDBG2(args)	NCR_DBGPR(0x04, args)
#define	NDBG3(args)	NCR_DBGPR(0x08, args)

#define	NDBG4(args)	NCR_DBGPR(0x10, args)
#define	NDBG5(args)	NCR_DBGPR(0x20, args)
#define	NDBG6(args)	NCR_DBGPR(0x40, args)
#define	NDBG7(args)	NCR_DBGPR(0x80, args)

#define	NDBG8(args)	NCR_DBGPR(0x0100, args)
#define	NDBG9(args)	NCR_DBGPR(0x0200, args)
#define	NDBG10(args)	NCR_DBGPR(0x0400, args)
#define	NDBG11(args)	NCR_DBGPR(0x0800, args)

#define	NDBG12(args)	NCR_DBGPR(0x1000, args)
#define	NDBG13(args)	NCR_DBGPR(0x2000, args)
#define	NDBG14(args)	NCR_DBGPR(0x4000, args)
#define	NDBG15(args)	NCR_DBGPR(0x8000, args)

#define	NDBG16(args)	NCR_DBGPR(0x010000, args)
#define	NDBG17(args)	NCR_DBGPR(0x020000, args)
#define	NDBG18(args)	NCR_DBGPR(0x040000, args)
#define	NDBG19(args)	NCR_DBGPR(0x080000, args)

#define	NDBG20(args)	NCR_DBGPR(0x100000, args)
#define	NDBG21(args)	NCR_DBGPR(0x200000, args)
#define	NDBG22(args)	NCR_DBGPR(0x400000, args)
#define	NDBG23(args)	NCR_DBGPR(0x800000, args)

#define	NDBG24(args)	NCR_DBGPR(0x1000000, args)
#define	NDBG25(args)	NCR_DBGPR(0x2000000, args)
#define	NDBG26(args)	NCR_DBGPR(0x4000000, args)
#define	NDBG27(args)	NCR_DBGPR(0x8000000, args)

#define	NDBG28(args)	NCR_DBGPR(0x10000000, args)
#define	NDBG29(args)	NCR_DBGPR(0x20000000, args)
#define	NDBG30(args)	NCR_DBGPR(0x40000000, args)
#define	NDBG31(args)	NCR_DBGPR(0x80000000, args)
