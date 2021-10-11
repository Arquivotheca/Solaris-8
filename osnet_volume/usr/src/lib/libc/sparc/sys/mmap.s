/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)mmap.s	1.9	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- mmap	*/
/* caddr_t mmap(caddr_t addr, size_t len, int prot,
	int flags, int fd, off_t off)				*/

	.file	"mmap.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(mmap,function)
#else
	ANSI_PRAGMA_WEAK(mmap64,function)
#endif

#include "SYS.h"
#include <sys/mman.h>		/* Need _MAP_NEW definition	*/

/*
 * Note that the code depends upon the _MAP_NEW flag being in the top bits
 */

#define FLAGS   %o3

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(mmap)
	sethi   %hi(_MAP_NEW), %g1
	or      %g1, FLAGS, FLAGS
	SYSTRAP(mmap)
	SYSCERROR
	RET

	SET_SIZE(mmap)

#else
/* C library -- mmap	*/
/* caddr_t mmap64(caddr_t addr, size_t len, int prot,
	int flags, int fd, off64_t off)				*/
	
	ENTRY(mmap64)
	sethi   %hi(_MAP_NEW), %g1
	or      %g1, FLAGS, FLAGS
	SYSTRAP(mmap64)
	SYSCERROR
	RET

	SET_SIZE(mmap64)

#endif

