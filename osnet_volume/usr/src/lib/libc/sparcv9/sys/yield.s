#ident "@(#)yield.s 1.1 96/12/04"

	.file "yield.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(yield,function)

#include "SYS.h"

/*
 * int
 * yield ()
 */
	SYSCALL(yield)
	RET
	SET_SIZE(yield)
