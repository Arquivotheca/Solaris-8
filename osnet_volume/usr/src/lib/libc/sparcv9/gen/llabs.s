/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 */

.ident	"@(#)llabs.s	1.1	97/02/12 SMI"

	.file	"llabs.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(llabs,function)

#include "synonyms.h"

/*
 * long long llabs(register long long arg);
 * long labs(register long int arg);
 */
	ENTRY2(labs,llabs)
	brlz,a	%o0, .done
	neg %o0
.done:
	retl
	nop

	SET_SIZE(labs)
	SET_SIZE(llabs)
