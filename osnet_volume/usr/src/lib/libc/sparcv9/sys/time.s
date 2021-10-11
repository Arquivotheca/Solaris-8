/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)time.s	1.4	97/08/12 SMI"

/*
 * C library -- time
 * time_t time (time_t *tloc);
 */

	.file	"time.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(time,function)

#include "SYS.h"

	ENTRY(time)
	mov	%o0, %o2	! save pointer
	SYSTRAP(time)
	brnz,a,pn %o2, 1f	! pointer is non-null?
	stx	%o0, [%o2]
1:
	RET
	SET_SIZE(time)
