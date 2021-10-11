/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ddi_v7_asm.s	1.11	99/07/14 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/sunddi.h>
#else
#include <sys/asm_linkage.h>
#include "assym.h"
#endif

#ifdef lint

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
i_ddi_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
}

/*ARGSUSED*/
void
i_ddi_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
}

/*ARGSUSED*/
void
i_ddi_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
}

/*ARGSUSED*/
void
i_ddi_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
}
#else
	ENTRY(ddi_getb)
	ALTENTRY(ddi_mem_getb)
	ALTENTRY(ddi_io_getb)
	ld	[%o0 + AHI_GETB], %g1	! f = handle->ahi_getb
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_getb)
	SET_SIZE(ddi_mem_getb)
	SET_SIZE(ddi_io_getb)

	ENTRY(ddi_getw)
	ALTENTRY(ddi_mem_getw)
	ALTENTRY(ddi_io_getw)
	ld	[%o0 + AHI_GETW], %g1	! f = handle->ahi_getw
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_getw)
	SET_SIZE(ddi_mem_getw)
	SET_SIZE(ddi_io_getw)

	ENTRY(ddi_getl)
	ALTENTRY(ddi_mem_getl)
	ALTENTRY(ddi_io_getl)
	ld	[%o0 + AHI_GETL], %g1	! f = handle->ahi_getl
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_getl)
	SET_SIZE(ddi_mem_getl)
	SET_SIZE(ddi_io_getl)

	ENTRY(ddi_getll)
	ALTENTRY(ddi_mem_getll)
	ld	[%o0 + AHI_GETLL], %g1	! f = handle->ahi_getll
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_getll)
	SET_SIZE(ddi_mem_getll)

	ENTRY(ddi_putb)
	ALTENTRY(ddi_mem_putb)
	ALTENTRY(ddi_io_putb)
	ld	[%o0 + AHI_PUTB], %g1	! f = handle->ahi_putb
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_putb)
	SET_SIZE(ddi_mem_putb)
	SET_SIZE(ddi_io_putb)

	ENTRY(ddi_putw)
	ALTENTRY(ddi_mem_putw)
	ALTENTRY(ddi_io_putw)
	ld	[%o0 + AHI_PUTW], %g1	! f = handle->ahi_putw
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_putw)
	SET_SIZE(ddi_mem_putw)
	SET_SIZE(ddi_io_putw)

	ENTRY(ddi_putl)
	ALTENTRY(ddi_mem_putl)
	ALTENTRY(ddi_io_putl)
	ld	[%o0 + AHI_PUTL], %g1	! f = handle->ahi_putl
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_putl)
	SET_SIZE(ddi_mem_putl)
	SET_SIZE(ddi_io_putl)

	ENTRY(ddi_putll)
	ALTENTRY(ddi_mem_putll)
	ld	[%o0 + AHI_PUTLL], %g1	! f = handle->ahi_putll
	jmpl	%g1, %g0		! f(...)
	nop
	SET_SIZE(ddi_putll)
	SET_SIZE(ddi_mem_putll)

	ENTRY(ddi_rep_getb)
	ALTENTRY(ddi_mem_rep_getb)
	ld	[%o0 + AHI_REP_GETB], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_getb)
	SET_SIZE(ddi_mem_rep_getb)

	ENTRY(ddi_rep_getw)
	ALTENTRY(ddi_mem_rep_getw)
	ld	[%o0 + AHI_REP_GETW], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_getw)
	SET_SIZE(ddi_mem_rep_getw)

	ENTRY(ddi_rep_getl)
	ALTENTRY(ddi_mem_rep_getl)
	ld	[%o0 + AHI_REP_GETL], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_getl)
	SET_SIZE(ddi_mem_rep_getl)

	ENTRY(ddi_rep_getll)
	ALTENTRY(ddi_mem_rep_getll)
	ld	[%o0 + AHI_REP_GETLL], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_getll)
	SET_SIZE(ddi_mem_rep_getll)

	ENTRY(ddi_rep_putb)
	ALTENTRY(ddi_mem_rep_putb)
	ld	[%o0 + AHI_REP_PUTB], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_putb)
	SET_SIZE(ddi_mem_rep_putb)

	ENTRY(ddi_rep_putw)
	ALTENTRY(ddi_mem_rep_putw)
	ld	[%o0 + AHI_REP_PUTW], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_putw)
	SET_SIZE(ddi_mem_rep_putw)

	ENTRY(ddi_rep_putl)
	ALTENTRY(ddi_mem_rep_putl)
	ld	[%o0 + AHI_REP_PUTL], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_putl)
	SET_SIZE(ddi_mem_rep_putl)

	ENTRY(ddi_rep_putll)
	ALTENTRY(ddi_mem_rep_putll)
	ld	[%o0 + AHI_REP_PUTLL], %g1
	jmpl	%g1, %g0
	nop
	SET_SIZE(ddi_rep_putll)
	SET_SIZE(ddi_mem_rep_putll)

	ENTRY(i_ddi_get8)
	retl
	ldub	[%o1], %o0
	SET_SIZE(i_ddi_get8)

	ENTRY(i_ddi_get16)
	retl
	lduh	[%o1], %o0
	SET_SIZE(i_ddi_get16)

	ENTRY(i_ddi_get32)
	retl
	ld	[%o1], %o0
	SET_SIZE(i_ddi_get32)

	ENTRY(i_ddi_get64)
	retl
	ldd	[%o1], %o0
	SET_SIZE(i_ddi_get64)

	ENTRY(i_ddi_put8)
	retl
	stb	%o2, [%o1]
	SET_SIZE(i_ddi_put8)

	ENTRY(i_ddi_put16)
	retl
	sth	%o2, [%o1]
	SET_SIZE(i_ddi_put16)

	ENTRY(i_ddi_put32)
	retl
	st	%o2, [%o1]
	SET_SIZE(i_ddi_put32)

	ENTRY(i_ddi_put64)
	retl
	std	%o2, [%o1]
	SET_SIZE(i_ddi_put64)
#endif
