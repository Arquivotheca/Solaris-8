/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)physmem.s	1.2	97/11/25 SMI"

/*
 * Physical memory access for non-Fusion KADB.
 * Support for this feature is not currently implemented
 * for this hardware.
 */

#ifdef lint
#include <sys/types.h>
#endif

#include <sys/asm_linkage.h>


#ifdef lint
/*ARGSUSED*/
int
physmem_read(paddr_t p, caddr_t c, size_t s) { return -1; }
#else
	ENTRY(physmem_read)

	retl
	mov	-1, %o0

	SET_SIZE(physmem_read)
#endif

#ifdef lint
/*ARGSUSED*/
int
physmem_write(paddr_t p, caddr_t c, size_t s) { return -1; }
#else
	ENTRY(physmem_write)

	retl
	mov	-1, %o0

	SET_SIZE(physmem_write)
#endif
