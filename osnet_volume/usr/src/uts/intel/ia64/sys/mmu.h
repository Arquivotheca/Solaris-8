/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_IA64_SYS_MMU_H
#define	_IA64_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.1	99/05/04 SMI"


#ifdef	__cplusplus
extern "C" {
#endif


#define	MMU_STD_PAGESIZE	PAGESIZE
#define	MMU_STD_PAGEMASK	PAGEMASK
#define	MMU_STD_PAGESHIFT	PAGEMASK


#if defined(_KERNEL) && !defined(_ASM)

/* >>> */

#endif /* defined(_KERNEL) && !defined(_ASM) */


#define	HAT_INVLDPFNUM		0xffffffff


#ifndef _ASM
/* >>> */
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _IA64_SYS_MMU_H */
