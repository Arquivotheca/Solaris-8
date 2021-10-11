/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SRAM_H
#define	_SYS_SRAM_H

#pragma ident	"@(#)sram.h	1.3	96/02/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* useful debugging stuff */
#define	SRAM_ATTACH_DEBUG	0x1
#define	SRAM_REGISTERS_DEBUG	0x2

/* Use predefined strings to name the kstats from this driver. */
#define	RESETINFO_KSTAT_NAME	"reset-info"

/* Define Maximum size of the reset-info data passed up by POST. */
#define	MX_RSTINFO_SZ		0x2000

#if defined(_KERNEL)

/* Structures used in the driver to manage the hardware */
struct sram_soft_state {
	dev_info_t *dip;	/* dev info of myself */
	dev_info_t *pdip;	/* dev info of my parent */
	int board;		/* Board number for this sram */
	char *sram_base;	/* base of sram */
	int offset;		/* offset into sram of reset info */
	char *reset_info;	/* base of reset-info structure */
	char *os_private;	/* base of OS private area; */
};

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SRAM_H */
