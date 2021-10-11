/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DEVADDR_H
#define	_SYS_DEVADDR_H

#pragma ident	"@(#)devaddr.h	1.31	98/09/30 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/physaddr.h>
#include <sys/cpu.h>
#include <sys/vm_machparam.h>

/*
 * Fixed virtual addresses.
 * Allocated from the top of the
 * available virtual address space
 * and working down.
 */

#define	MX_SUN4D_BRDS		(10)
#define	MX_XDBUS		(2)
#define	XDB_OFFSET		(0x100)

/*
 * number of pages needed for "well known" addresses
 *	- only segkmap.
 */
#define	NWKPGS			(mmu_btop(SEGMAPSIZE))

/*
 * The base address of well known addresses, offset from PPMAPBASE
 * and rounded down so the bits will be right for the AGETCPU
 * macro (currenlty only used by sun4m).
 */
#define	V_WKBASE_ADDR		((PPMAPBASE - (NWKPGS * MMU_PAGESIZE)))
#define	V_SEGMAP_ADDR		(PPMAPBASE - (SEGMAPSIZE))

/* compatibility */
/* FIXME: these needs to be revisited */
#define	V_EEPROM_ADDR		0
#define	V_EEPROM_PGS		0x2		/* Pages needed for EEPROM */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_DEVADDR_H */
