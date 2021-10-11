/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)mmap.s	1.3	97/02/12 SMI"

/*
 * C library -- mmap
 * void *mmap(void *addr, size_t len, int prot,
 *	int flags, int fd, off_t off);
 */

	.file	"mmap.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mmap,function)

#include "SYS.h"
#include <sys/mman.h>		/* Need _MAP_NEW definition	*/

/*
 * Note that the code depends upon the _MAP_NEW flag being in the top bits
 */

#define FLAGS   %o3

	ENTRY(mmap)
	sethi   %hi(_MAP_NEW), %g1
	or      %g1, FLAGS, FLAGS
	SYSTRAP(mmap)
	SYSCERROR
	RET

	SET_SIZE(mmap)
