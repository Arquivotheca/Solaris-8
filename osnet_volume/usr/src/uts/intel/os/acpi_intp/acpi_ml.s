/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if defined(lint) || defined(__lint)
#else
	.ident	"@(#)acpi_ml.s	1.1	99/05/21 SMI"
	.file	"acpi_ml.s"
#endif


/*
 * use the stuff in the spec in 5.2.6.1
 */

#ifndef _ASM
#define	_ASM			/* needed to properly read asm_linkage.h */
#endif
#include <sys/asm_linkage.h>


/*
 * int
 * acpi_gl_acquire(void)
 */
#if defined(lint) || defined(__lint)
/*LINTLIBRARY*/
int
acpi_gl_acquire(void)
{
	return (0);
}
#else
/* XXX need to reverse sense of return */
	ENTRY(acpi_gl_acquire)
	movl	gl_addr, %ecx
.acpi_gl_acquire0:
	movl	(%ecx), %eax
	movl	%eax, %edx
	andl	$0xFFFFFFFE, %edx
	btsl	$1, %edx
	adcl	$0, %edx
	lock
	cmpxchgl %edx, (%ecx)
	jnz	.acpi_gl_acquire0
	cmpl	$3, %edx
	sbbl	%eax, %eax
	ret
	SET_SIZE(acpi_gl_acquire)
#endif

/*
 * int
 * acpi_gl_release(void)
 */
#if defined(lint) || defined(__lint)
/*LINTLIBRARY*/
int
acpi_gl_release(void)
{
	return (0);
}
#else
/* XXX need to reverse sense of return */
	ENTRY(acpi_gl_release)
	movl	gl_addr, %ecx
.acpi_gl_release0:
	movl	(%ecx), %eax
	movl	%eax, %edx
	andl	$0xFFFFFFFB, %edx
	lock
	cmpxchgl %edx, (%ecx)
	jnz	.acpi_gl_release0
	andl	$1, %eax
	ret
	SET_SIZE(acpi_gl_release)
#endif

/*
 * void
 * io8_load(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io8_load(unsigned int addr, unsigned char *value)
{
}
#else	
	ENTRY(io8_load)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	inb	(%dx)
	movb	%al, (%ecx)
	ret
	SET_SIZE(io8_load)
#endif

/*
 * void
 * io16_load(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io16_load(unsigned int addr, unsigned char *value)
{
}
#else
	ENTRY(io16_load)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	inw	(%dx)
	movw	%ax, (%ecx)
	ret
	SET_SIZE(io16_load)
#endif

/*
 * void
 * io32_load(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io32_load(unsigned int addr, unsigned char *value)
{
}
#else
	ENTRY(io32_load)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	inl	(%dx)
	movl	%eax, (%ecx)
	ret
	SET_SIZE(io32_load)
#endif


/*
 * void
 * io8_store(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io8_store(unsigned int addr, unsigned char *value)
{
}
#else
	ENTRY(io8_store)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	movb	(%ecx), %al
	outb	(%dx)
	ret
	SET_SIZE(io8_store)
#endif

/*
 * void
 * io16_store(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io16_store(unsigned int addr, unsigned char *value)
{
}
#else
	ENTRY(io16_store)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	movw	(%ecx), %ax
	outw	(%dx)
	ret
	SET_SIZE(io16_store)
#endif

/*
 * void
 * io32_store(unsigned int addr, unsigned char *value)
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
io32_store(unsigned int addr, unsigned char *value)
{
}
#else
	ENTRY(io32_store)
	movl	4(%esp), %edx
	movl	8(%esp), %ecx
	movl	(%ecx), %eax
	outl	(%dx)
	ret
	SET_SIZE(io32_store)
#endif

/* XXX need to setup gl_addr */
#if defined(lint) || defined(__lint)
#else
	.globl gl_addr
gl_addr: .long 0
#endif

/* eof */
