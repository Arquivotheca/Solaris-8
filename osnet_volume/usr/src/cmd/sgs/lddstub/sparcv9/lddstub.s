/*
 *	Copyright (c) 1991, Sun Microsystems, Inc.
 */

#ident	"@(#)lddstub.s	1.1	97/06/05 SMI"

/*
 * Stub file for ldd(1).  Provides for preloading shared libraries.
 */
#include <sys/syscall.h>

	.file	"lddstub.s"
	.seg	".text"
	.global	stub

stub:	clr	%o0
	mov	SYS_exit, %g1
	t	8
	nop
