/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 */

#ifndef _SYS_VIS_H
#define	_SYS_VIS_H

#pragma ident	"@(#)vis.h	1.1	97/09/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file is cpu dependent.
 */

#ifdef _KERNEL

#include <sys/asi.h>
#include <sys/machparam.h>

#define	BSTORE_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	stda	%d0, [FP] %asi; \
	stda	%d16, [FP + 64] %asi; \
	stda	%d32, [FP + 128] %asi; \
	stda	%d48, [FP + 192] %asi;

#define	BSTORE_V8_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	stda	%d0, [FP] %asi; \
	stda	%d16, [FP + 64] %asi;

#define	BSTORE_V8P_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	stda	%d32, [FP + 128] %asi; \
	stda	%d48, [FP + 192] %asi;

#define	BLOAD_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	ldda	[FP] %asi, %d0; \
	ldda	[FP + 64] %asi, %d16; \
	ldda	[FP + 128] %asi, %d32; \
	ldda	[FP + 192] %asi, %d48;

#define	BLOAD_V8_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	ldda	[FP] %asi, %d0; \
	ldda	[FP + 64] %asi, %d16;

#define	BLOAD_V8P_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	ldda	[FP + 128] %asi, %d32; \
	ldda	[FP + 192] %asi, %d48;

#define	BZERO_FPREGS(FP) \
	wr	%g0, ASI_BLK_P, %asi; \
	ldda	[FP] %asi, %d0; \
	ldda	[FP] %asi, %d16; \
	ldda	[FP] %asi, %d32; \
	ldda	[FP] %asi, %d48;

#define	GSR_SIZE 8	/* Graphics Status Register size 64 bits */

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VIS_H */
