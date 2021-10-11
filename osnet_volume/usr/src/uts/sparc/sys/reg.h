/*
 * Copyright (c) 1989, 1995 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_REG_H
#define	_SYS_REG_H

#pragma ident	"@(#)reg.h	1.25	95/02/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file only exists for v7 backwards compatibility.
 * Kernel code should not include it.
 */
#ifdef _KERNEL
#error Kernel include of reg.h
#else

#include <sys/regset.h>

/*
 * NWINDOW is obsolete; it exists only for existing application compatibility.
 */
#define	NWINDOW		7

/*
 * Location of the users' stored registers relative to R0.
 * Used as an index into a gregset_t array.
 */
#define	PSR	(0)
#define	PC	(1)
#define	nPC	(2)
#define	Y	(3)
#define	G1	(4)
#define	G2	(5)
#define	G3	(6)
#define	G4	(7)
#define	G5	(8)
#define	G6	(9)
#define	G7	(10)
#define	O0	(11)
#define	O1	(12)
#define	O2	(13)
#define	O3	(14)
#define	O4	(15)
#define	O5	(16)
#define	O6	(17)
#define	O7	(18)

/*
 * The following defines are for portability.
 */
#define	PS	PSR
#define	SP	O6
#define	R0	O0
#define	R1	O1

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REG_H */
