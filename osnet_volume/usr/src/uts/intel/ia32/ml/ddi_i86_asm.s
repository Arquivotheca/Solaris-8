/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_i86_asm.s	1.23	99/07/22 SMI"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/sunddi.h>
#else
#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include "assym.h"
#endif

#if defined(lint) || defined(__lint)

#ifdef _LP64

/*ARGSUSED*/
int
ddi_dma_sync(ddi_dma_handle_t h, off_t o, size_t l, u_int whom)
{ return (0); }

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
ddi_io_rep_get32(ddi_acc_handle_t handle,
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

#else /* _ILP32 */

/*ARGSUSED*/
int
ddi_dma_sync(ddi_dma_handle_t h, off_t o, size_t l, u_int whom)
{ return (0); }

uint8_t
ddi_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

uint8_t
ddi_io_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

uint16_t
ddi_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

uint16_t
ddi_io_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

uint32_t
ddi_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

uint32_t
ddi_io_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

uint64_t
ddi_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}

uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}


void
ddi_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_io_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_io_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_io_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

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

#endif /* _LP64 */

#else	/* lint */


	ENTRY(ddi_dma_sync)
	movl	4(%esp), %eax
	movl	DMAI_RFLAGS(%eax), %ecx
	andl	$DMP_NOSYNC, %ecx
	cmpl	$DMP_NOSYNC, %ecx
	jne	ddi_dma_sync_cont
	xorl	%eax, %eax	/ DDI_SUCCESS
	ret
ddi_dma_sync_cont:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	0x14(%ebp)
	pushl	0x10(%ebp)
	pushl	0x0c(%ebp)
	pushl	0x08(%ebp)
	movl	DMAI_RDIP(%eax), %ecx
	movl	DEVI_BUS_DMA_FLUSH(%ecx), %edx
	pushl	%ecx
	pushl	%edx
	movl	DEVI_DEV_OPS(%edx), %eax
	movl	DEVI_BUS_OPS(%eax), %ecx
	call	*OPS_FLUSH(%ecx)
	addl	$0x18, %esp
	leave
	ret
	SET_SIZE(ddi_dma_sync)
	


	ENTRY(ddi_getb)
	ALTENTRY(ddi_get8)
	ALTENTRY(ddi_mem_getb)
	ALTENTRY(ddi_mem_get8)
	ALTENTRY(ddi_io_getb)
	ALTENTRY(ddi_io_get8)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	1f
	movl	8(%esp), %edx
	xorl	%eax, %eax
	inb	(%dx)
	ret
1:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	2f
	movl	8(%esp), %eax
	movzbl	(%eax), %eax
	ret
2:
	jmp	*ACC_GETB(%eax)
	SET_SIZE(ddi_getb)
	SET_SIZE(ddi_get8)
	SET_SIZE(ddi_mem_getb)
	SET_SIZE(ddi_mem_get8)
	SET_SIZE(ddi_io_getb)
	SET_SIZE(ddi_io_get8)

	ENTRY(ddi_getw)
	ALTENTRY(ddi_get16)
	ALTENTRY(ddi_mem_getw)
	ALTENTRY(ddi_mem_get16)
	ALTENTRY(ddi_io_getw)
	ALTENTRY(ddi_io_get16)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	3f
	movl	8(%esp), %edx
	xorl	%eax, %eax
	inw	(%dx)
	ret
3:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	4f
	movl	8(%esp), %eax
	movzwl	(%eax), %eax
	ret
4:
	jmp	*ACC_GETW(%eax)
	SET_SIZE(ddi_getw)
	SET_SIZE(ddi_get16)
	SET_SIZE(ddi_mem_getw)
	SET_SIZE(ddi_mem_get16)
	SET_SIZE(ddi_io_getw)
	SET_SIZE(ddi_io_get16)

	ENTRY(ddi_getl)
	ALTENTRY(ddi_get32)
	ALTENTRY(ddi_mem_getl)
	ALTENTRY(ddi_mem_get32)
	ALTENTRY(ddi_io_getl)
	ALTENTRY(ddi_io_get32)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	5f
	movl	8(%esp), %edx
	inl	(%dx)
	ret
5:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	6f
	movl	8(%esp), %eax
	movl	(%eax), %eax
	ret
6:
	jmp	*ACC_GETL(%eax)
	SET_SIZE(ddi_getl)
	SET_SIZE(ddi_get32)
	SET_SIZE(ddi_mem_getl)
	SET_SIZE(ddi_mem_get32)
	SET_SIZE(ddi_io_getl)
	SET_SIZE(ddi_io_get32)

	ENTRY(ddi_getll)
	ALTENTRY(ddi_get64)
	ALTENTRY(ddi_mem_getll)
	ALTENTRY(ddi_mem_get64)
	movl	4(%esp), %eax
	jmp	*ACC_GETLL(%eax)
	SET_SIZE(ddi_getll)
	SET_SIZE(ddi_get64)
	SET_SIZE(ddi_mem_getll)
	SET_SIZE(ddi_mem_get64)

	ENTRY(ddi_putb)
	ALTENTRY(ddi_put8)
	ALTENTRY(ddi_mem_putb)
	ALTENTRY(ddi_mem_put8)
	ALTENTRY(ddi_io_putb)
	ALTENTRY(ddi_io_put8)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	7f
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outb	(%dx)
	ret
7:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	8f
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	movb	%cl, (%eax)
	ret
8:
	jmp	*ACC_PUTB(%eax)
	SET_SIZE(ddi_putb)
	SET_SIZE(ddi_put8)
	SET_SIZE(ddi_mem_putb)
	SET_SIZE(ddi_mem_put8)
	SET_SIZE(ddi_io_putb)
	SET_SIZE(ddi_io_put8)

	ENTRY(ddi_putw)
	ALTENTRY(ddi_put16)
	ALTENTRY(ddi_mem_putw)
	ALTENTRY(ddi_mem_put16)
	ALTENTRY(ddi_io_putw)
	ALTENTRY(ddi_io_put16)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	9f
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outw	(%dx)
	ret
9:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	acc_putw
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	movw	%cx, (%eax)
	ret
acc_putw:
	jmp	*ACC_PUTW(%eax)
	SET_SIZE(ddi_putw)
	SET_SIZE(ddi_put16)
	SET_SIZE(ddi_mem_putw)
	SET_SIZE(ddi_mem_put16)
	SET_SIZE(ddi_io_putw)
	SET_SIZE(ddi_io_put16)

	ENTRY(ddi_putl)
	ALTENTRY(ddi_put32)
	ALTENTRY(ddi_mem_putl)
	ALTENTRY(ddi_mem_put32)
	ALTENTRY(ddi_io_putl)
	ALTENTRY(ddi_io_put32)
	movl	4(%esp), %eax
	movl	ACC_ATTR(%eax), %ecx
	cmpl	$[DDI_ACCATTR_IO_SPACE|DDI_ACCATTR_DIRECT], %ecx
	jne	acc_meml
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outl	(%dx)
	ret
acc_meml:
	cmpl	$[DDI_ACCATTR_CPU_VADDR|DDI_ACCATTR_DIRECT], %ecx
	jne	acc_putl
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	movl	%ecx, (%eax)
	ret
acc_putl:
	jmp	*ACC_PUTL(%eax)
	SET_SIZE(ddi_putl)
	SET_SIZE(ddi_put32)
	SET_SIZE(ddi_mem_putl)
	SET_SIZE(ddi_mem_put32)
	SET_SIZE(ddi_io_putl)
	SET_SIZE(ddi_io_put32)

	ENTRY(ddi_putll)
	ALTENTRY(ddi_put64)
	ALTENTRY(ddi_mem_putll)
	ALTENTRY(ddi_mem_put64)
	movl	4(%esp), %eax
	jmp	*ACC_PUTLL(%eax)
	SET_SIZE(ddi_putll)
	SET_SIZE(ddi_put64)
	SET_SIZE(ddi_mem_putll)
	SET_SIZE(ddi_mem_put64)

	ENTRY(ddi_rep_getb)
	ALTENTRY(ddi_rep_get8)
	ALTENTRY(ddi_mem_rep_getb)
	ALTENTRY(ddi_mem_rep_get8)
	movl	4(%esp), %eax
	jmp	*ACC_REP_GETB(%eax)
	SET_SIZE(ddi_rep_getb)
	SET_SIZE(ddi_rep_get8)
	SET_SIZE(ddi_mem_rep_getb)
	SET_SIZE(ddi_mem_rep_get8)

	ENTRY(ddi_rep_getw)
	ALTENTRY(ddi_rep_get16)
	ALTENTRY(ddi_mem_rep_getw)
	ALTENTRY(ddi_mem_rep_get16)
	movl	4(%esp), %eax
	jmp	*ACC_REP_GETW(%eax)
	SET_SIZE(ddi_rep_getw)
	SET_SIZE(ddi_rep_get16)
	SET_SIZE(ddi_mem_rep_getw)
	SET_SIZE(ddi_mem_rep_get16)

	ENTRY(ddi_rep_getl)
	ALTENTRY(ddi_rep_get32)
	ALTENTRY(ddi_mem_rep_getl)
	ALTENTRY(ddi_mem_rep_get32)
	movl	4(%esp), %eax
	jmp	*ACC_REP_GETL(%eax)
	SET_SIZE(ddi_rep_getl)
	SET_SIZE(ddi_rep_get32)
	SET_SIZE(ddi_mem_rep_getl)
	SET_SIZE(ddi_mem_rep_get32)

	ENTRY(ddi_rep_getll)
	ALTENTRY(ddi_rep_get64)
	ALTENTRY(ddi_mem_rep_getll)
	ALTENTRY(ddi_mem_rep_get64)
	movl	4(%esp), %eax
	jmp	*ACC_REP_GETLL(%eax)
	SET_SIZE(ddi_rep_getll)
	SET_SIZE(ddi_rep_get64)
	SET_SIZE(ddi_mem_rep_getll)
	SET_SIZE(ddi_mem_rep_get64)

	ENTRY(ddi_rep_putb)
	ALTENTRY(ddi_rep_put8)
	ALTENTRY(ddi_mem_rep_putb)
	ALTENTRY(ddi_mem_rep_put8)
	movl	4(%esp), %eax
	jmp	*ACC_REP_PUTB(%eax)
	SET_SIZE(ddi_rep_putb)
	SET_SIZE(ddi_rep_put8)
	SET_SIZE(ddi_mem_rep_putb)
	SET_SIZE(ddi_mem_rep_put8)

	ENTRY(ddi_rep_putw)
	ALTENTRY(ddi_rep_put16)
	ALTENTRY(ddi_mem_rep_putw)
	ALTENTRY(ddi_mem_rep_put16)
	movl	4(%esp), %eax
	jmp	*ACC_REP_PUTW(%eax)
	SET_SIZE(ddi_rep_putw)
	SET_SIZE(ddi_rep_put16)
	SET_SIZE(ddi_mem_rep_putw)
	SET_SIZE(ddi_mem_rep_put16)

	ENTRY(ddi_rep_putl)
	ALTENTRY(ddi_rep_put32)
	ALTENTRY(ddi_mem_rep_putl)
	ALTENTRY(ddi_mem_rep_put32)
	movl	4(%esp), %eax
	jmp	*ACC_REP_PUTL(%eax)
	SET_SIZE(ddi_rep_putl)
	SET_SIZE(ddi_rep_put32)
	SET_SIZE(ddi_mem_rep_putl)
	SET_SIZE(ddi_mem_rep_put32)

	ENTRY(ddi_rep_putll)
	ALTENTRY(ddi_rep_put64)
	ALTENTRY(ddi_mem_rep_putll)
	ALTENTRY(ddi_mem_rep_put64)
	movl	4(%esp), %eax
	jmp	*ACC_REP_PUTLL(%eax)
	SET_SIZE(ddi_rep_putll)
	SET_SIZE(ddi_rep_put64)
	SET_SIZE(ddi_mem_rep_putll)
	SET_SIZE(ddi_mem_rep_put64)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
i_ddi_vaddr_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (*addr);
}

#else	/* lint */

	ENTRY(i_ddi_vaddr_get8)
	movl	8(%esp), %eax
	movzbl	(%eax), %eax
	ret
	SET_SIZE(i_ddi_vaddr_get8)

	ENTRY(i_ddi_vaddr_get16)
	movl	8(%esp), %eax
	movzwl	(%eax), %eax
	ret
	SET_SIZE(i_ddi_vaddr_get16)

	ENTRY(i_ddi_vaddr_get32)
	movl	8(%esp), %eax
	movl	(%eax), %eax
	ret
	SET_SIZE(i_ddi_vaddr_get32)

	ENTRY(i_ddi_vaddr_get64)
	movl	8(%esp), %ecx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	ret
	SET_SIZE(i_ddi_vaddr_get64)

#endif /* lint */


#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
i_ddi_io_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (inb((u_int)addr));
}

/*ARGSUSED*/
uint16_t
i_ddi_io_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (inw((u_int)addr));
}

/*ARGSUSED*/
uint32_t
i_ddi_io_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (inl((int)addr));
}

#else	/* lint */

	ENTRY(i_ddi_io_get8)
	movl	8(%esp), %edx
	inb	(%dx)
	movzbl	%al,%eax
	ret
	SET_SIZE(i_ddi_io_get8)

	ENTRY(i_ddi_io_get16)
	movl	8(%esp), %edx
	inw	(%dx)
	movzwl	%ax,%eax
	ret
	SET_SIZE(i_ddi_io_get16)

	ENTRY(i_ddi_io_get32)
	movl	8(%esp), %edx
	inl	(%dx)
	ret
	SET_SIZE(i_ddi_io_get32)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
i_ddi_vaddr_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	*addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	*addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	*(uint32_t *)addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*addr = value;
}

#else	/* lint */

	ENTRY(i_ddi_vaddr_put8)
	movl	8(%esp), %eax
	movb	12(%esp), %cl
	movb	%cl, (%eax)
	ret
	SET_SIZE(i_ddi_vaddr_put8)

	ENTRY(i_ddi_vaddr_put16)
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	movw	%cx, (%eax)
	ret
	SET_SIZE(i_ddi_vaddr_put16)

	ENTRY(i_ddi_vaddr_put32)
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	movl	%ecx, (%eax)
	ret
	SET_SIZE(i_ddi_vaddr_put32)

	ENTRY(i_ddi_vaddr_put64)
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	movl	16(%esp), %eax
	movl	%edx, (%ecx)
	movl	%eax, 4(%ecx)
	ret
	SET_SIZE(i_ddi_vaddr_put64)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
i_ddi_io_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	outb((u_int)addr, value);
}

/*ARGSUSED*/
void
i_ddi_io_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	outw((u_int)addr, value);
}

/*ARGSUSED*/
void
i_ddi_io_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	outl((int)addr, value);
}

#else	/* lint */

	ENTRY(i_ddi_io_put8)
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outb	(%dx)
	ret
	SET_SIZE(i_ddi_io_put8)

	ENTRY(i_ddi_io_put16)
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outw	(%dx)
	ret
	SET_SIZE(i_ddi_io_put16)

	ENTRY(i_ddi_io_put32)
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	outl	(%dx)
	ret
	SET_SIZE(i_ddi_io_put32)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
i_ddi_io_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 1)
			*h++ = inb(port);
	else
		repinsb(port, h, repcount);
}

/*ARGSUSED*/
void
i_ddi_io_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_io_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

#else	/* lint */

	ENTRY(i_ddi_io_rep_get8)
	pushl	%edi

	movl	12(%esp),%edi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	gb_ioadv

	rep	
	insb
	popl	%edi
	ret

gb_ioadv:
	andl	%ecx, %ecx
	jz	gb_ioadv_done
gb_ioadv2:
	inb	(%dx)
	movb	%al,(%edi)
	incl	%edi
	incl	%edx
	decl	%ecx
	jg	gb_ioadv2

gb_ioadv_done:
	popl	%edi
	ret

	SET_SIZE(i_ddi_io_rep_get8)

	ENTRY(i_ddi_io_rep_get16)
	pushl	%edi

	movl	12(%esp),%edi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	gw_ioadv

	rep	
	insw
	popl	%edi
	ret

gw_ioadv:
	andl	%ecx, %ecx
	jz	gw_ioadv_done
gw_ioadv2:
	inw	(%dx)
	movw	%ax,(%edi)
	addl	$2, %edi
	addl	$2, %edx
	decl	%ecx
	jg	gw_ioadv2

gw_ioadv_done:
	popl	%edi
	ret
	SET_SIZE(i_ddi_io_rep_get16)

	ENTRY(i_ddi_io_rep_get32)
	pushl	%edi

	movl	12(%esp),%edi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	gl_ioadv

	rep	
	insl
	popl	%edi
	ret

gl_ioadv:
	andl	%ecx, %ecx
	jz	gl_ioadv_done
gl_ioadv2:
	inl	(%dx)
	movl	%eax,(%edi)
	addl	$4, %edi
	addl	$4, %edx
	decl	%ecx
	jg	gl_ioadv2

gl_ioadv_done:
	popl	%edi
	ret

	SET_SIZE(i_ddi_io_rep_get32)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
i_ddi_io_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 1)
			outb(port, *h++);
	else
		repoutsb(port, h, repcount);
}

/*ARGSUSED*/
void
i_ddi_io_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
i_ddi_io_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

#else	/* lint */

	ENTRY(i_ddi_io_rep_put8)
	pushl	%esi

	movl	12(%esp),%esi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	pb_ioadv

	rep	
	outsb
	popl	%esi
	ret

pb_ioadv:
	andl	%ecx, %ecx
	jz	pb_ioadv_done
pb_ioadv2:
	movb	(%esi), %al
	outb	(%dx)
	incl	%esi
	incl	%edx
	decl	%ecx
	jg	pb_ioadv2

pb_ioadv_done:
	popl	%esi
	ret
	SET_SIZE(i_ddi_io_rep_put8)

	ENTRY(i_ddi_io_rep_put16)
	pushl	%esi

	movl	12(%esp),%esi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	pw_ioadv

	rep	
	outsw
	popl	%esi
	ret

pw_ioadv:
	andl	%ecx, %ecx
	jz	pw_ioadv_done
pw_ioadv2:
	movw	(%esi), %ax
	outw	(%dx)
	addl	$2, %esi
	addl	$2, %edx
	decl	%ecx
	jg	pw_ioadv2

pw_ioadv_done:
	popl	%esi
	ret
	SET_SIZE(i_ddi_io_rep_put16)

	ENTRY(i_ddi_io_rep_put32)
	pushl	%esi

	movl	12(%esp),%esi			/ get host_addr
	movl	16(%esp),%edx			/ get port
	movl	20(%esp),%ecx			/ get repcount
	cmpl	$DDI_DEV_AUTOINCR, 24(%esp)
	je	pl_ioadv

	rep	
	outsl
	popl	%esi
	ret

pl_ioadv:
	andl	%ecx, %ecx
	jz	pl_ioadv_done
pl_ioadv2:
	movl	(%esi), %eax
	outl	(%dx)
	addl	$4, %esi
	addl	$4, %edx
	decl	%ecx
	jg	pl_ioadv2

pl_ioadv_done:
	popl	%esi
	ret
	SET_SIZE(i_ddi_io_rep_put32)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* Request bus_ctl parent to handle a bus_ctl request */
/*ARGSUSED*/
int
ddi_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t op, void *a, void *v)
{
 	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_ctlops)
	movl	4(%esp),%eax			/ dip
	movl	8(%esp),%edx			/ rdip
	testl	%eax,%eax
	jz	_ctlop_er
	movl	DEVI_BUS_CTL(%eax),%eax
	testl	%edx,%edx
	jz	_ctlop_er
	testl	%eax,%eax
	jz	_ctlop_er
	movl	DEVI_DEV_OPS(%eax),%edx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%edx),%edx
	jmp	*OPS_CTL(%edx)			/ call parent function
_ctlop_er:
	movl	$-1,%eax			/ return (DDI_FAILURE);
	ret
	SET_SIZE(ddi_ctlops)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* Request bus_dma_map parent to setup a dma request */
/*ARGSUSED*/
int
ddi_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareqp, ddi_dma_handle_t *handlep)
{
 	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_map)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_MAP(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_MAP(%ecx)			/ call parent function
	SET_SIZE(ddi_dma_map)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* Request bus_dma_ctl parent to fiddle with a dma request. */
/*ARGSUSED*/
int
ddi_dma_mctl(register dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, u_int flags)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_mctl)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_CTL(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_MCTL(%ecx)			/ call parent function
	SET_SIZE(ddi_dma_mctl)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ddi_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	return (DDI_SUCCESS);
}
#else	/* lint */

	ENTRY(ddi_dma_allochdl)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_ALLOCHDL(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_ALLOCHDL(%ecx)		/ call parent function
	SET_SIZE(ddi_dma_allochdl)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ddi_dma_freehdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle)
{
	return (DDI_SUCCESS);
}
#else	/* lint */

	ENTRY(ddi_dma_freehdl)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_FREEHDL(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_FREEHDL(%ecx)		/ call parent function
	SET_SIZE(ddi_dma_freehdl)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Request bus_dma_bindhdl parent to bind object to handle
 */
/* ARGSUSED */
int
ddi_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cp, u_int *ccountp)
{
	return (DDI_SUCCESS);
}
#else	/* lint */

	ENTRY(ddi_dma_bindhdl)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_BINDHDL(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_BINDHDL(%ecx)		/ call parent function
	SET_SIZE(ddi_dma_bindhdl)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Request bus_dma_unbindhdl parent to unbind object from handle
 */
/* ARGSUSED */
int
ddi_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle)
{
	return (DDI_SUCCESS);
}
#else	/* lint */

	ENTRY(ddi_dma_unbindhdl)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_UNBINDHDL(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_UNBINDHDL(%ecx)		/ call parent function
	SET_SIZE(ddi_dma_unbindhdl)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ddi_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len, u_int cache_flags)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_flush)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_FLUSH(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_FLUSH(%ecx)		/ call parent function
	SET_SIZE(ddi_dma_flush)

#endif /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ddi_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_win)
	movl	4(%esp),%ecx			/ dip
	movl	DEVI_BUS_DMA_WIN(%ecx),%eax
	movl	DEVI_DEV_OPS(%eax),%ecx
	movl	%eax,4(%esp)			/ replace dip in arg list
	movl	DEVI_BUS_OPS(%ecx),%ecx
	jmp	*OPS_WIN(%ecx)			/ call parent function
	SET_SIZE(ddi_dma_win)

#endif /* lint */
