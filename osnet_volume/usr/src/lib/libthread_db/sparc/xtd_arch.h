/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _XTD_ARCH_H
#define	_XTD_ARCH_H

#pragma ident	"@(#)xtd_arch.h	1.8	94/12/31 SMI"

/*
* MODULE_td_arch.h__________________________________________________
*  Description:
*	Contains architecture dependent defintions.
*
____________________________________________________________________ */

#define	TD_NFPREGSINGLE 32
#define	TD_NFPREGDOUBLE 16
#define	TD_NFPREGSINGLESIZE sizeof (FPU_REGS_TYPE)
#define	TD_NFPREGDOUBLESIZE sizeof (double)
#define	TD_NFPQ	64
#define	TD_NFPQSIZE sizeof (u_long)

#define	TD_BITSPERBYTE 8
#define	TD_BITSPERWORD 32
#define	TD_BYTESPERWORD 4

#endif /* _XTD_ARCH_H */
