/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)memscrub_asm.s 1.16	99/06/04 SMI"

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/machsystm.h>
#include <sys/t_lock.h>
#else   /* lint */
#include "assym.h"
#endif  /* lint */

#include <sys/asm_linkage.h>
#include <sys/eeprom.h>
#include <sys/param.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include <sys/iocache.h>
#include <sys/privregs.h>
#include <sys/archsystm.h>

#if defined(lint)

/*ARGSUSED*/
void
memscrub_read(caddr_t vaddr, u_int blks)
{}

#else	/* lint */

	!
	! void	memscrub_read(caddr_t src, u_int blks)
	!

	.seg ".text"
	.align	4

	ENTRY(memscrub_read)
	srl	%o1, 0, %o1			! clear upper word of blk count
        rd	%fprs, %o2			! get the status of fp
	wr	%g0, FPRS_FEF, %fprs		! enable fp

1:
	ldda	[%o0]ASI_BLK_P, %d0
	add	%o0, 64, %o0
	ldda	[%o0]ASI_BLK_P, %d16
	add	%o0, 64, %o0
	ldda	[%o0]ASI_BLK_P, %d32
	add	%o0, 64, %o0
	ldda	[%o0]ASI_BLK_P, %d48
	dec	%o1
	brnz,a	%o1, 1b
	add	%o0, 64, %o0

	retl
	wr	%o2, 0, %fprs			! restore fprs (disabled)
	SET_SIZE(memscrub_read)

#endif	/* lint */
