/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)module_ross625_asm.s	1.1	95/07/18 SMI"

#include <sys/asm_linkage.h>
#include <sys/module_ross625.h>

/*
 * Flush trap disable.
 */
#ifdef	lint
void
ross625_module_ftd(void)
{ }

#else	/* def lint */
	ENTRY(ross625_module_ftd)
	rd	RT620_ICCR, %o2
	or	%o2, RT620_ICCR_FTD, %o2
	wr	%o2, RT620_ICCR
	retl
	mov	%o2, %o0
	SET_SIZE(ross625_module_ftd)
#endif	/* def lint */
