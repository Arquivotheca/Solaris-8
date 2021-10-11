/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_v9_asm.s	1.33	99/07/14 SMI"

#include <sys/asi.h>
#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/privregs.h>
#ifndef lint
#include "assym.h"
#endif

#if defined(lint)
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sunddi.h>
#endif  /* lint */

/*
 * This file implements the following ddi common access 
 * functions:
 *
 *	ddi_get{b,h,l,ll}
 *	ddi_put{b,h,l.ll}
 *
 * and the underlying "trivial" implementations
 *
 *      i_ddi_{get,put}{b,h,l,ll}
 *
 * which assume that there is no need to check the access handle -
 * byte swapping will be done by the mmu and the address is always
 * accessible via ld/st instructions.
 */

#if defined(lint)

#ifdef _LP64

/*ARGSUSED*/
uint8_t
ddi_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_mem_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_io_get8(ddi_acc_handle_t handle, uint8_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_mem_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_io_get16(ddi_acc_handle_t handle, uint16_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_mem_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_io_get32(ddi_acc_handle_t handle, uint32_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_mem_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
void
ddi_put8(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_mem_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_io_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_mem_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_io_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_put32(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_mem_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_io_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_put64(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_mem_put64(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr,
        uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr,
        uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr,
        uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr,
        uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr,
        uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr,
        uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr,
        uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
        uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount) {}
 
/*ARGSUSED*/
void
ddi_io_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount) {}
 
/*ARGSUSED*/
void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount) {}
 
/*ARGSUSED*/
void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount) {}
 
/*ARGSUSED*/
void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount) {}
 

/*ARGSUSED*/
void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
uint8_t
i_ddi_get8(ddi_acc_impl_t *hdlp, uint8_t *addr) 
{
	return (0);
}

/*ARGSUSED*/
uint16_t
i_ddi_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
i_ddi_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
i_ddi_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
void
i_ddi_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value) {}

/*ARGSUSED*/
void
i_ddi_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value) {}

/*ARGSUSED*/
void
i_ddi_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value) {}

/*ARGSUSED*/
void
i_ddi_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value) {}

/*ARGSUSED*/
void
i_ddi_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr, 
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr, 
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr, 
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr, 
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr, 
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr, 
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}


#else /* _ILP32 */

/*ARGSUSED*/
uint8_t
ddi_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_io_getb(ddi_acc_handle_t handle, uint8_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_io_getw(ddi_acc_handle_t handle, uint16_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_io_getl(ddi_acc_handle_t handle, uint32_t *dev_addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
int
ddi_check_acc_handle(ddi_acc_handle_t handle)
{
	return (0); 
}

/*ARGSUSED*/
int
i_ddi_acc_fault_check(ddi_acc_impl_t *hdlp)
{
	return (0); 
}

/*ARGSUSED*/
void
ddi_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_io_putb(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_io_putw(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_io_putl(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_rep_getb(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getw(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getl(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putb(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putw(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
        size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putl(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_io_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount) {}

/*ARGSUSED*/
int
do_peek(size_t size, void *addr, void *val)
{
	return (0); 
}

/*ARGSUSED*/
int
do_poke(size_t size, void *addr, void *val)
{
	return (0); 
}

#endif /* _LP64 */

#else

/*
 * The functionality of each of the ddi_get/put routines is performed by
 * the respective indirect function defined in the access handle.  Use of
 * the access handle functions provides compatibility across platforms for
 * drivers.
 * 
 * By default, the indirect access handle functions are initialized to the
 * i_ddi_get/put routines to perform memory mapped IO.  If memory mapped IO
 * is not possible or desired, the access handle must be intialized to another
 * valid routine to perform the sepcified IO operation.
 *
 * The alignment and placement of the following functions have been optimized
 * such that the implementation specific versions, i_ddi*, fall within the 
 * same cache-line of the generic versions, ddi_*.  This insures that an
 * I-cache hit will occur thus minimizing the performance impact of using the
 * access handle.
 */

	.align 32
	ENTRY(ddi_getb)
	ALTENTRY(ddi_get8)
	ALTENTRY(ddi_io_get8)
	ALTENTRY(ddi_io_getb)
	ALTENTRY(ddi_mem_get8)
	ALTENTRY(ddi_mem_getb)
	ldn      [%o0 + AHI_GET8], %g1   /* hdl->ahi_get8 access hndl */
	jmpl    %g1, %g0                 /* jump to access handle routine */
	nop
	SET_SIZE(ddi_getb)
        SET_SIZE(ddi_get8)
        SET_SIZE(ddi_io_get8)
        SET_SIZE(ddi_io_getb)
        SET_SIZE(ddi_mem_get8)
        SET_SIZE(ddi_mem_getb)

	.align 16
	ENTRY(i_ddi_get8)
	retl
	ldub	[%o1], %o0
	SET_SIZE(i_ddi_get8)

	.align 32
	ENTRY(ddi_getw)
        ALTENTRY(ddi_get16)
        ALTENTRY(ddi_io_get16)
        ALTENTRY(ddi_io_getw)
        ALTENTRY(ddi_mem_get16)
        ALTENTRY(ddi_mem_getw)
	ldn      [%o0 + AHI_GET16], %g1   /* hdl->ahi_get16 access hndl */
	jmpl    %g1, %g0                  /* jump to access handle routine */
	nop
	SET_SIZE(ddi_getw)
        SET_SIZE(ddi_get16)
        SET_SIZE(ddi_io_get16)
        SET_SIZE(ddi_io_getw)
        SET_SIZE(ddi_mem_get16)
        SET_SIZE(ddi_mem_getw)

	.align 16
	ENTRY(i_ddi_get16)
	ALTENTRY(i_ddi_swap_get16)
	retl
	lduh	[%o1], %o0
	SET_SIZE(i_ddi_get16)
	SET_SIZE(i_ddi_swap_get16)

	.align 32
	ENTRY(ddi_getl)
        ALTENTRY(ddi_get32)
        ALTENTRY(ddi_io_get32)
        ALTENTRY(ddi_io_getl)
        ALTENTRY(ddi_mem_get32)
        ALTENTRY(ddi_mem_getl)
	ldn      [%o0 + AHI_GET32], %g1   /* hdl->ahi_get32 access handle */
	jmpl    %g1, %g0		  /* jump to access handle routine */
	nop
	SET_SIZE(ddi_getl)
        SET_SIZE(ddi_get32)
        SET_SIZE(ddi_io_get32)
        SET_SIZE(ddi_io_getl)
        SET_SIZE(ddi_mem_get32)
        SET_SIZE(ddi_mem_getl)

	.align 16
	ENTRY(i_ddi_get32)
	ALTENTRY(i_ddi_swap_get32)
	retl
	ld	[%o1], %o0
	SET_SIZE(i_ddi_get32)
	SET_SIZE(i_ddi_swap_get32)

	.align 32
	ENTRY(ddi_getll)
        ALTENTRY(ddi_get64)
        ALTENTRY(ddi_io_get64)
        ALTENTRY(ddi_io_getll)
        ALTENTRY(ddi_mem_get64)
        ALTENTRY(ddi_mem_getll)
	ldn      [%o0 + AHI_GET64], %g1   /* hdl->ahi_get64 access handle */
	jmpl    %g1, %g0                  /* jump to access handle routine */
	nop
	SET_SIZE(ddi_getll)
        SET_SIZE(ddi_get64)
        SET_SIZE(ddi_io_get64)
        SET_SIZE(ddi_io_getll)
        SET_SIZE(ddi_mem_get64)
        SET_SIZE(ddi_mem_getll)

	.align 16
	ENTRY(i_ddi_get64)
	ALTENTRY(i_ddi_swap_get64)
#ifdef __sparcv9
	retl
	ldx	[%o1], %o0
#else
	ldx	[%o1], %o4
	srlx	%o4, 32, %o0
	retl
	srl	%o4, 0, %o1
#endif
	SET_SIZE(i_ddi_get64)
	SET_SIZE(i_ddi_swap_get64)

	.align 32
	ENTRY(ddi_putb)
        ALTENTRY(ddi_put8)
        ALTENTRY(ddi_io_put8)
        ALTENTRY(ddi_io_putb)
        ALTENTRY(ddi_mem_put8)
        ALTENTRY(ddi_mem_putb)
	ldn      [%o0 + AHI_PUT8], %g1   /* hdl->ahi_put8 access handle */
	jmpl    %g1, %g0                 /* jump to access handle routine */
	nop
	SET_SIZE(ddi_putb)
        SET_SIZE(ddi_put8)
        SET_SIZE(ddi_io_put8)
        SET_SIZE(ddi_io_putb)
        SET_SIZE(ddi_mem_put8)
        SET_SIZE(ddi_mem_putb)

	.align 16
	ENTRY(i_ddi_put8)
	retl
	stub	%o2, [%o1]
	SET_SIZE(i_ddi_put8)

	.align 32
	ENTRY(ddi_putw)
        ALTENTRY(ddi_put16)
        ALTENTRY(ddi_io_put16)
        ALTENTRY(ddi_io_putw)
        ALTENTRY(ddi_mem_put16)
        ALTENTRY(ddi_mem_putw)
	ldn      [%o0 + AHI_PUT16], %g1   /* hdl->ahi_put16 access handle */
	jmpl    %g1, %g0                  /* jump to access handle routine */
	nop
	SET_SIZE(ddi_putw)
        SET_SIZE(ddi_put16)
        SET_SIZE(ddi_io_put16)
        SET_SIZE(ddi_io_putw)
        SET_SIZE(ddi_mem_put16)
        SET_SIZE(ddi_mem_putw)

	.align 16
	ENTRY(i_ddi_put16)
	ALTENTRY(i_ddi_swap_put16)
	retl
	stuh	%o2, [%o1]
	SET_SIZE(i_ddi_put16)
	SET_SIZE(i_ddi_swap_put16)

	.align 32
	ENTRY(ddi_putl)
        ALTENTRY(ddi_put32)
        ALTENTRY(ddi_io_put32)
        ALTENTRY(ddi_io_putl)
        ALTENTRY(ddi_mem_put32)
        ALTENTRY(ddi_mem_putl)
	ldn      [%o0 + AHI_PUT32], %g1   /* hdl->ahi_put16 access handle */
	jmpl    %g1, %g0                  /* jump to access handle routine */
	nop
	SET_SIZE(ddi_putl)
        SET_SIZE(ddi_put32)
        SET_SIZE(ddi_io_put32)
        SET_SIZE(ddi_io_putl)
        SET_SIZE(ddi_mem_put32)
        SET_SIZE(ddi_mem_putl)

	.align 16
	ENTRY(i_ddi_put32)
	ALTENTRY(i_ddi_swap_put32)
	retl
	st	%o2, [%o1]
	SET_SIZE(i_ddi_put32)
	SET_SIZE(i_ddi_swap_put32)

	.align 32
        ENTRY(ddi_putll)
        ALTENTRY(ddi_put64)
        ALTENTRY(ddi_io_put64)
        ALTENTRY(ddi_io_putll)
        ALTENTRY(ddi_mem_put64)
        ALTENTRY(ddi_mem_putll)
	ldn      [%o0 + AHI_PUT64], %g1   /* hdl->ahi_put64 access handle */
	jmpl    %g1, %g0                  /* jump to access handle routine */ 
	nop
	SET_SIZE(ddi_putll)
        SET_SIZE(ddi_put64)
        SET_SIZE(ddi_io_put64)
        SET_SIZE(ddi_io_putll)
        SET_SIZE(ddi_mem_put64)
        SET_SIZE(ddi_mem_putll)

	.align 16
	ENTRY(i_ddi_put64)
	ALTENTRY(i_ddi_swap_put64)
#if !defined(__sparcv9)
	sllx    %o2, 32, %o2
	srl     %o3, 0, %o3
	or      %o2, %o3, %o2
#endif
	retl
	stx	%o2, [%o1]
	SET_SIZE(i_ddi_put64)
	SET_SIZE(i_ddi_swap_put64)

/*
 * The ddi_io_rep_get/put routines don't take a flag argument like the "plain"
 * and mem versions do.  This flag is used to determine whether or not the 
 * device address or port should be automatically incremented.  For IO space,
 * the device port is never incremented and as such, the flag is always set
 * to DDI_DEV_NO_AUTOINCR.
 *
 * This define processes the repetitive get functionality.  Automatic 
 * incrementing of the device address is determined by the flag field 
 * %o4.  If this is set for AUTOINCR, %o4 is updated with 1 for the 
 * subsequent increment in 2:.
 * 
 * If this flag is not set for AUTOINCR, %o4 is update with a value of 0 thus
 * making the increment operation a non-operation.
 */

#define DDI_REP_GET(n,s)			\
	cmp	DDI_DEV_NO_AUTOINCR, %o4;	\
	mov	%g0, %o4;			\
	brz,pn	%o3, 1f;			\
	movnz	%xcc, n, %o4;			\
2:						\
	dec	%o3;				\
	ld/**/s	[%o2], %g4;			\
	add	%o2, %o4, %o2;			\
	st/**/s	%g4, [%o1];			\
	brnz,pt	%o3, 2b;			\
	add	%o1, n, %o1;			\
1:						\
	retl;					\
	nop

	.align 32
	ENTRY(ddi_rep_getb)
        ALTENTRY(ddi_rep_get8)
        ALTENTRY(ddi_mem_rep_get8)
        ALTENTRY(ddi_mem_rep_getb)
	ldn      [%o0 + AHI_REP_GET8], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_getb)
        SET_SIZE(ddi_rep_get8)
        SET_SIZE(ddi_mem_rep_get8)
        SET_SIZE(ddi_mem_rep_getb)

	.align 16
	ENTRY(i_ddi_rep_get8)
	DDI_REP_GET(1,ub)
	SET_SIZE(i_ddi_rep_get8)
	
	.align 32
	ENTRY(ddi_rep_getw)
        ALTENTRY(ddi_rep_get16)
        ALTENTRY(ddi_mem_rep_get16)
        ALTENTRY(ddi_mem_rep_getw)
	ldn	[%o0 + AHI_REP_GET16], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_getw)
        SET_SIZE(ddi_rep_get16)
        SET_SIZE(ddi_mem_rep_get16)
        SET_SIZE(ddi_mem_rep_getw)

	.align 16
	ENTRY(i_ddi_rep_get16)
	ALTENTRY(i_ddi_swap_rep_get16)
	DDI_REP_GET(2,uh)
	SET_SIZE(i_ddi_rep_get16)
	SET_SIZE(i_ddi_swap_rep_get16)

	.align 32
	ENTRY(ddi_rep_getl)
        ALTENTRY(ddi_rep_get32)
        ALTENTRY(ddi_mem_rep_get32)
        ALTENTRY(ddi_mem_rep_getl)
	ldn      [%o0 + AHI_REP_GET32], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_getl)
        SET_SIZE(ddi_rep_get32)
        SET_SIZE(ddi_mem_rep_get32)
        SET_SIZE(ddi_mem_rep_getl)

	.align 16
	ENTRY(i_ddi_rep_get32)
	ALTENTRY(i_ddi_swap_rep_get32)
	DDI_REP_GET(4,/**/)
	SET_SIZE(i_ddi_rep_get32)
	SET_SIZE(i_ddi_swap_rep_get32)

	.align 32
	ENTRY(ddi_rep_getll)
        ALTENTRY(ddi_rep_get64)
        ALTENTRY(ddi_mem_rep_get64)
        ALTENTRY(ddi_mem_rep_getll)
	ldn      [%o0 + AHI_REP_GET64], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_getll)
        SET_SIZE(ddi_rep_get64)
        SET_SIZE(ddi_mem_rep_get64)
        SET_SIZE(ddi_mem_rep_getll)

	.align 16
	ENTRY(i_ddi_rep_get64)
	ALTENTRY(i_ddi_swap_rep_get64)
	DDI_REP_GET(8,x)
	SET_SIZE(i_ddi_rep_get64)
	SET_SIZE(i_ddi_swap_rep_get64)

/* 
 * This define processes the repetitive put functionality.  Automatic 
 * incrementing of the device address is determined by the flag field 
 * %o4.  If this is set for AUTOINCR, %o4 is updated with 1 for the 
 * subsequent increment in 2:.
 * 
 * If this flag is not set for AUTOINCR, %o4 is update with a value of 0 thus
 * making the increment operation a non-operation.
 */
#define DDI_REP_PUT(n,s)			\
	cmp	DDI_DEV_NO_AUTOINCR, %o4;	\
	mov	%g0, %o4;			\
	brz,pn	%o3, 1f;			\
	movnz	%xcc, n, %o4;			\
2:						\
	dec	%o3;				\
	ld/**/s	[%o1], %g4;			\
	add	%o1, n, %o1;			\
	st/**/s	%g4, [%o2];			\
	brnz,pt	%o3, 2b;			\
	add	%o2, %o4, %o2;			\
1:						\
	retl;					\
	nop

	.align 32
	ENTRY(ddi_rep_putb)
        ALTENTRY(ddi_rep_put8)
        ALTENTRY(ddi_mem_rep_put8)
        ALTENTRY(ddi_mem_rep_putb)
	ldn      [%o0 + AHI_REP_PUT8], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_putb)
        SET_SIZE(ddi_rep_put8)
        SET_SIZE(ddi_mem_rep_put8)
        SET_SIZE(ddi_mem_rep_putb)

	.align 16
	ENTRY(i_ddi_rep_put8)
	DDI_REP_PUT(1,ub)
	SET_SIZE(i_ddi_rep_put8)

	.align 32
	ENTRY(ddi_rep_putw)
        ALTENTRY(ddi_rep_put16)
        ALTENTRY(ddi_mem_rep_put16)
        ALTENTRY(ddi_mem_rep_putw)
	ldn      [%o0 + AHI_REP_PUT16], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_putw)
        SET_SIZE(ddi_rep_put16)
        SET_SIZE(ddi_mem_rep_put16)
        SET_SIZE(ddi_mem_rep_putw)

	.align 16
	ENTRY(i_ddi_rep_put16)
	ALTENTRY(i_ddi_swap_rep_put16)
	DDI_REP_PUT(2,uh)
	SET_SIZE(i_ddi_rep_put16)
	SET_SIZE(i_ddi_swap_rep_put16)

	.align 32
	ENTRY(ddi_rep_putl)
        ALTENTRY(ddi_rep_put32)
        ALTENTRY(ddi_mem_rep_put32)
        ALTENTRY(ddi_mem_rep_putl)
	ldn      [%o0 + AHI_REP_PUT32], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_putl)
        SET_SIZE(ddi_rep_put32)
        SET_SIZE(ddi_mem_rep_put32)
        SET_SIZE(ddi_mem_rep_putl)

	.align 16
	ENTRY(i_ddi_rep_put32)
	ALTENTRY(i_ddi_swap_rep_put32)
	DDI_REP_PUT(4,/**/)
	SET_SIZE(i_ddi_rep_put32)
	SET_SIZE(i_ddi_swap_rep_put32)

	.align 32
	ENTRY(ddi_rep_putll)
        ALTENTRY(ddi_rep_put64)
        ALTENTRY(ddi_mem_rep_put64)
        ALTENTRY(ddi_mem_rep_putll)
	ldn      [%o0 + AHI_REP_PUT64], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_rep_putll)
        SET_SIZE(ddi_rep_put64)
        SET_SIZE(ddi_mem_rep_put64)
        SET_SIZE(ddi_mem_rep_putll)

	.align 16
	ENTRY(i_ddi_rep_put64)
	ALTENTRY(i_ddi_swap_rep_put64)
	DDI_REP_PUT(8,x)
	SET_SIZE(i_ddi_rep_put64)
	SET_SIZE(i_ddi_swap_rep_put64)

	.align 16
        ENTRY(ddi_io_rep_get8)
        ALTENTRY(ddi_io_rep_getb)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_GET8], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_get8)
        SET_SIZE(ddi_io_rep_getb)

	.align 16
        ENTRY(ddi_io_rep_get16)
        ALTENTRY(ddi_io_rep_getw)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_GET16], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_get16)
        SET_SIZE(ddi_io_rep_getw)

	.align 16
        ENTRY(ddi_io_rep_get32)
        ALTENTRY(ddi_io_rep_getl)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_GET32], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_get32)
        SET_SIZE(ddi_io_rep_getl)

	.align 16
        ENTRY(ddi_io_rep_get64)
        ALTENTRY(ddi_io_rep_getll)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_GET64], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_get64)
        SET_SIZE(ddi_io_rep_getll)

        .align 32
	ENTRY(ddi_check_acc_handle)
	ldn     [%o0 + AHI_FAULT_CHECK], %g1
	jmpl    %g1, %g0
	nop
	SET_SIZE(ddi_check_acc_handle)

        .align 16
        ENTRY(i_ddi_acc_fault_check)
	retl
	ld      [%o0 + AHI_FAULT], %o0
        SET_SIZE(i_ddi_acc_fault_check)

	.align 16
        ENTRY(ddi_io_rep_put8)
        ALTENTRY(ddi_io_rep_putb)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_PUT8], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_put8)
        SET_SIZE(ddi_io_rep_putb)

	.align 16
        ENTRY(ddi_io_rep_put16)
        ALTENTRY(ddi_io_rep_putw)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_PUT16], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_put16)
        SET_SIZE(ddi_io_rep_putw)

	.align 16
        ENTRY(ddi_io_rep_put32)
        ALTENTRY(ddi_io_rep_putl)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_PUT32], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_put32)
        SET_SIZE(ddi_io_rep_putl)

	.align 16
        ENTRY(ddi_io_rep_put64)
        ALTENTRY(ddi_io_rep_putll)
	set	DDI_DEV_NO_AUTOINCR, %o4 /* Set flag to DDI_DEV_NO_AUTOINCR */
	ldn	[%o0 + AHI_REP_PUT64], %g1
	jmpl    %g1, %g0
	nop
        SET_SIZE(ddi_io_rep_put64)
        SET_SIZE(ddi_io_rep_putll)

	ENTRY(do_peek)
	rdpr	%pstate, %o3	! check ints
	andcc	%o3, PSTATE_IE, %g0
	bz,a	done
	or	%g0, 1, %o0	! Return failure if ints are disabled
	wrpr	%o3, PSTATE_IE, %pstate
	cmp	%o0, 8		! 64-bit?
	bne,a	.peek_int
	cmp	%o0, 4		! 32-bit?
	ldx	[%o1], %g1
	ba	.peekdone
	stx	%g1, [%o2]
.peek_int:
	bne,a	.peek_half
	cmp	%o0, 2		! 16-bit?
	lduw	[%o1], %g1
	ba	.peekdone
	stuw	%g1, [%o2]
.peek_half:
	bne,a	.peek_byte
	ldub	[%o1], %g1	! 8-bit!
	lduh	[%o1], %g1
	ba	.peekdone
	stuh	%g1, [%o2]
.peek_byte:
	stub	%g1, [%o2]
.peekdone:
	membar	#Sync		! Make sure the loads take
	rdpr	%pstate, %o3	! check&enable ints
	andcc	%o3, PSTATE_IE, %g0
	bnz	1f
	nop
	wrpr	%o3, PSTATE_IE, %pstate
1:	
	mov	%g0, %o0
done:
	retl
	nop
	SET_SIZE(do_peek)

	ENTRY(do_poke)
	cmp	%o0, 8		! 64 bit?
	bne,a	.poke_int
	cmp	%o0, 4		! 32-bit?
	ldx	[%o2], %g1
	ba	.pokedone
	stx	%g1, [%o1]
.poke_int:
	bne,a	.poke_half
	cmp	%o0, 2		! 16-bit?
	lduw	[%o2], %g1
	ba	.pokedone
	stuw	%g1, [%o1]
.poke_half:
	bne,a	.poke_byte
	ldub	[%o2], %g1	! 8-bit!
	lduh	[%o2], %g1
	ba	.pokedone
	stuh	%g1, [%o1]
.poke_byte:
	stub	%g1, [%o1]
.pokedone:
	membar	#Sync
	retl
	mov	%g0, %o0
	SET_SIZE(do_poke)


/*
 * The next two routines are only to be used by trap handlers.  These
 * routines aren't explicitly called, instead they're addresses are placed
 * into t_pc register which will be executed once the trap is processed.
 * This routine manages the stack in such a way to mimic the stack usage
 * of i_ddi_peek/i_ddi_poke.  The routines are used to report traps back
 * to the callers of i_ddi_peek/i_ddi_poke.
 */

#define	PEEK_FAULT	4	/* yuck - should be done by genassym */
#define	POKE_FAULT	8

	.seg	".data"
.peek_panic:
	.asciz	"peek_fault: bad nofault data"
.poke_panic:
	.asciz	"poke_fault: bad nofault data"

	ENTRY(peek_fault)
	ldn	[THREAD_REG + T_NOFAULT], %o0	! Get the nofault data struct
	brz,pn	%o0, .peekfail		! Check for validity
	nop
	ld	[%o0 + NF_OP_TYPE], %o1	! Get the op_type
	cmp	%o1, PEEK_FAULT		! Check for the peek fault
	bne,pn	%icc, .peekfail		! Bad op_type
	rdpr	%pstate, %o3		! enable ints
	andcc	%o3, PSTATE_IE, %g0
	bnz	1f
	nop
	wrpr	%o3, PSTATE_IE, %pstate
1:	
	retl
	sub	%g0, 1, %o0		! Set DDI_FAILURE as return value
.peekfail:
	set	.peek_panic, %o0	! Load panic message
	call	panic			! Panic if bad t_nofault data
	nop
	SET_SIZE(peek_fault)


	ENTRY(poke_fault)
	ldn	[THREAD_REG + T_NOFAULT], %o0	! Get the nofault data struct
	brz,pn	%o0, .pokefail		! Check for validity
	nop
	ld	[%o0 + NF_OP_TYPE], %o1	! Get the op_type
	cmp	%o1, POKE_FAULT		! Check for the poke fault
	bne,pn	%icc, .pokefail		! Bad op_type
	nop
	retl
	sub	%g0, 1, %o0		! Set DDI_FAILURE as return value
.pokefail:
	set	.poke_panic, %o0	! Load panic message
	call	panic			! Panic if bad t_nofault data
	nop
	SET_SIZE(poke_fault)

#endif
