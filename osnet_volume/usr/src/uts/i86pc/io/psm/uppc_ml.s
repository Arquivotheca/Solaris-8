/*
 * Copyright (c) 1992-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)uppc_ml.s	1.9	98/01/23 SMI"

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/time.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

/*
 * uppc_intr_enter() raises the ipl to the level of the current interrupt,
 * and sends EOI to the pics.
 * if interrupt is 7 or 15 and not spurious interrupt, send specific EOI
 * else send non-specific EOI 
 * uppc_intr_enter() returns the new priority level, 
 * or -1 for spurious interrupt 
 *
 *	if (intno & 7) != 7) {
 *		uppc_setspl(newipl);
 *		outb(MCMD_PORT, PIC_NSEOI);
 *		if (slave pic)
 *			outb(SCMD_PORT, PIC_NSEOI);
 *      	return newipl;
 *      } else {
 *		if (autovect[intno].pri <= cur_pri) {
 *			if (intno != 7)
 *				outb(MCMD_PORT, PIC_NSEOI);
 *			return -1;
 *		} else {
 *			uppc_setspl(newipl);
 *			if (slave pic) {
 *				outb(MCMD_PORT, PIC_NSEOI);
 *				outb(SCMD_PORT, PIC_SEOI_LVL7);
 *			}
 *			else 
 *				outb(MCMD_PORT, PIC_SEOI_LVL7);
 *			return newipl;
 *		}
 *	}
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
uppc_intr_enter(int current_ipl, int *vector)
{ return (0); }

#else	/* lint */

	ENTRY(uppc_intr_enter)
	movl	8(%esp), %ecx
	movl	(%ecx), %ecx		/ %ecx - intr vector
	movl	%ecx, %eax		/ %eax - intno
	movl	%eax, %edi		/ %edi - saved in cmnint 

	/ setup fpr detecting spurious interrupt reflected to IRQ7 or 15 of PIC
	andl	$7, %eax
	cmpl	$7, %eax		/ if intno != IRQ7 of slave
	je	spur_check		/ then handle interrupt 

real_int:
	movl	$autovect, %eax
	movzwl	AVH_HI_PRI(%eax, %ecx, 8), %eax

	pushl	%eax			/ new ipl for setspl
	call    uppc_setspl		/ load masks for new level

	/ send EOI to master PIC
	movb    $PIC_NSEOI, %al         / non-specific EOI
	movw	$MCMD_PORT, %dx		/ get master PIC command port addr
	outb    (%dx)                   / send EOI to master pic

	cmpl	$0x8, %edi		/ master pic ?
	jb      int_ret

	/ send EOI to slave PIC
	movw	$SCMD_PORT, %dx		/ get slave PIC command port addr
	outb    (%dx)                   / send EOI to slave pic

int_ret:
	popl	%eax
	ret

spur_check:
	movl	$autovect, %eax
	movzwl	AVH_HI_PRI(%eax, %ecx, 8), %eax

	cmpl	4(%esp), %eax
	jbe	spur_int	

	pushl	%eax			/ new ipl for setspl
	call    uppc_setspl		/ load masks for new level

	cmpl	$0x7, %edi
	je	master_seoi

	movb    $PIC_NSEOI, %al         / non-specific EOI
	movw	$MCMD_PORT, %dx		/ get master PIC command port addr
	outb    (%dx)                   / send EOI to master pic

	movb	$PIC_SEOI_LVL7, %al	/ SEOI to slave
	movw	$SCMD_PORT, %dx		/ get slave PIC command port addr
	outb    (%dx)                   / send EOI to slave pic
	
	popl	%eax
	ret

master_seoi:
	movb	$PIC_SEOI_LVL7, %al	/ SEOI
	movw	$MCMD_PORT, %dx		/ get master PIC command port addr
	outb    (%dx)                   / send EOI to master pic

	popl	%eax
	ret

spur_int:
	cmpl	$0x7, %edi
	je	spurintret	
	
slave_spur:
	movb	$PIC_NSEOI, %al		/ send EOI to master PIC
	movw	$MCMD_PORT, %dx		/ get master PIC command port addr
	outb	(%dx)

spurintret:
	movl	$-1, %eax		/ return spurious
	ret

	SET_SIZE(uppc_intr_enter)

#endif	/* lint */

/*
 * uppc_intr_exit() restores the old interrupt
 * priority level after processing an interrupt.
 * It is called with interrupts disabled, and does not enable interrupts.
 * The new interrupt level is passed as an arg on the stack.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
uppc_intr_exit(int ipl, int vector)
{}

#else	/* lint */

	ENTRY(uppc_intr_exit)
	movl    4(%esp), %eax           / get new ipl
	pushl	%eax			/ argument to setspl
	call    uppc_setspl		/ load masks for new level
	popl	%eax			
	ret				
	SET_SIZE(uppc_intr_exit)

#endif	/* lint */


/*
 * uppc_setspl() loads new interrupt masks into the pics 
 * based on input ipl.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
uppc_setspl(int ipl)
{}

#else	/* lint */

	ENTRY(uppc_setspl)
	movl	4(%esp), %eax		/ new ipl
	pushl	%esi

	leal	pics0, %esi
	movw 	C_IPLMASK(%esi,%eax,2), %ax
	movw	C_CURMASK(%esi), %cx
	cmpw	%ax, %cx
	je	setpicmasksret

	movw	%ax, C_CURMASK(%esi)

	/ program the slave
	cmpb    %cl, %al       		/ compare to current mask
	je      pset_master		/ skip if identical

	movw	$SIMR_PORT, %dx	
	outb    (%dx)			/ output new mask that is in %al

	/ program the master
pset_master:
	cmpb    %ch, %ah       		/ compare to current mask
	je      picskip			/ skip if identical

	movb	%ah, %al
	movw	$MIMR_PORT, %dx	
	outb    (%dx)			/ output new mask that is in %al

picskip:
					/ read master
	inb     (%dx)                   / to allow the pics to settle
					
setpicmasksret:
	popl	%esi			/ pop non-volatile regs
	ret
	SET_SIZE(uppc_setspl)

#endif	/* lint */

/*
 * uppc_gethrtime() returns high resolution timer value
 */
#if defined(lint) || defined(__lint)

hrtime_t
uppc_gethrtime()
{ return (0); }

#else	/* lint */

	.data
	.align	8
	.globl	lasthrtime
lasthrtime:
	.long 0x0, 0x0
	.size lasthrtime, 8

i8254_lock:
	.long 0
	.size i8254_lock, 4

	ENTRY(uppc_gethrtime)
	pushl	%ebx

	pushfl
	cli

	movl	$i8254_lock, %eax
.CL1:
	lock
	btsw	$0, (%eax)		/ try to set lock
	jnc	.CL3			/ got it
.CL2:
	testb	$1, (%eax)		/ possible to get lock?
	jnz	.CL2
	jmp	.CL1			/ yes, try again
.CL3:
	movl	hrtime_base, %ebx
	movl	hrtime_base+4, %ecx

	xorl	%eax, %eax		/ counter 0 latch command
	movw	$PITCTL_PORT, %dx
	outb	(%dx)
	xorl	%edx, %edx		/ zero edx

	inb	$PITCTR0_PORT		/ least significant byte into %al
	movb	%al, %dl
	inb	$PITCTR0_PORT		/ most significant byte into %al
	movb	%al, %dh


	imull	$NSEC_PER_COUNTER_TICK, %edx, %eax

	cmpl	hrtime_base+4, %ecx	/ has hrtime_base changed ?
	jne	.CL3
	cmpl	hrtime_base, %ebx	/ has hrtime_base changed ?
	jne	.CL3			/ this heuristics assumes that the
					/ another CPU will never take more time
					/ between 2 writes than the time we take
					/ for inb, sall etc
					/ Even if it does, lasthrtime will
					/ bail us out.
	subl	%eax, %ebx
	sbbl	$0, %ecx

	cmpl	%ecx,lasthrtime+4
	jl	newtimeok
	jg	newtimenotok
	cmpl	%ebx,lasthrtime
	jbe	newtimeok

newtimenotok:
	movl	lasthrtime,%eax
	movl	lasthrtime+4,%edx
	jmp	gethrtime_ret

newtimeok:

	movl	%ebx, %eax
	movl	%ecx, %edx
	movl	%eax,lasthrtime
	movl	%edx,lasthrtime+4
gethrtime_ret:
	lock
	decl	i8254_lock
	popfl
	popl	%ebx
	ret

	SET_SIZE(uppc_gethrtime)
#endif	/* lint */
