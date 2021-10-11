#if !defined(lint)

/       Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
/	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
/	  All Rights Reserved

/	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
/	UNIX System Laboratories, Inc.
/	The copyright notice above does not evidence any
/	actual or intended publication of such source code.

	.ident	"@(#)ttrap.s	1.13	99/11/29 SMI"

#include "assym.s"

	.data
	.align	4

	.section	.data1,"aw"
	.align	4

kadblock:	.long	0	/ Our grand mutex lock

_tempa:
	.value  0
_tempb:
	.long   0               / temp variable - quick n dirty

	.comm   dr_cpubkptused,NCPU,4

	.globl	SAVEcr0
SAVEcr0:
	.long	0


	.text
/
/  The common entry point for traps from entry points in intr.s
/  For stack layout, see reg.h
/  When cmntrap gets called, the error code and trap number have been pushed.
/
/  **** Code in misc.s assumes %ds == %es. Initialize %ds and %es in the
/       various kernel entry points.

	.align	4
	.globl	cmntrap
	.globl	_tempa
	.globl  cur_cpuid
	.globl  kadblock
	.globl  psm_notifyf
	.globl  dr_cpubkptused
	.globl  xc_initted
	.globl  kxc_serv
	.globl	cmnint
	.globl	idt
	.globl	IDTdscr
	.globl	BIDTdscr
	.globl	GDTdscr
	.globl  BDSsav
	.globl  BESsav
	.globl  BFSsav
	.globl  BGSsav
	.globl	BCSsav
	.globl	nmitrap
	.globl	nmifault

nmitrap:
	pusha                   / save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	pushl	%ss
	call	nmifault
	jmp	nocr0_saved2

cmntrap:
cmnint:
	pusha                   / save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

/	restore our selectors except fs and gs. We use gs for getting cpu id.
	
	movw    %cs:BDSsav, %ds
	movw    BESsav, %es

/ Find out which cpu we are on

	xorl    %eax,%eax       / default cpuid is zero if we can not get it
	movl    xc_initted, %ebx
	cmpl    $0, %ebx        / are we initialized for multiprocessing
	je      no_multi        / if not, branch
	cmpl    $0, (%ebx)      / are we initialized for multiprocessing
	je      no_multi        / if not, branch
	movl    $KGSSEL,%ebx
	verr    %bx             / can we read this selector
	jnz     no_multi        / if not, then default cpuid is zero
	movw    %bx,%gs         / load in this selector
/ get the cpu number from the per-cpu structure
	movzbl  %gs:CPU_ID,%eax

/ If we are currently locked and our cpu did it then a fault (e.g. page
/ fault) within kadb must have occurred.  The fault() routine will longjmp
/ back to appropriate code.
	cmpl	cur_cpuid,%eax
	je	we_have_mutex

/ get the kadb mutex lock
	movl	$kadblock,%ebx
.L1:
	xchgl	(%ebx),%ebx
	orl	%ebx,%ebx
	jz	.L3

/ If we failed to get the lock, we have lost the race to enter kadb.  We
/ need to allow for the xc_call_debug() to effectively interrupt this
/ processor.  We do this by calling the kernel xc_serv(X_CALL_HIPRI).

	movl	$0xfffffff,%ecx
.L2:
	pushl	$X_CALL_HIPRI
	movl    $KGSSEL,%eax	/ restore selectors before calling kernel
	movw	%ax,%gs
	movl    $KFSSEL,%eax
	movw	%ax,%fs
	call	*kxc_serv
	add	$4,%esp
	movzbl  %gs:CPU_ID,%eax	/ restore %eax
	movl	$kadblock,%ebx
	cmpl	$0,(%ebx)
	loopne	.L2
	jmp     .L1
.L3:
/ We got the lock
we_have_mutex:
no_multi:
	movl	%eax,cur_cpuid

	/ now restore fs and gs
	movw    BFSsav, %fs
	movw    BGSsav, %gs

/	save the users idt pointer and restore ours.

	sidt	_tempa		/ stores into _tempb
	movl	$idt,%eax

	cmp 	_tempb,%eax
	je	is_indbg
	sidt	IDTdscr		
	lidt	BIDTdscr
is_indbg:
	movl    %cr0, %eax
	andl	$0x00010000, %eax
	cmpl	$0, %eax
	je	no_cr0_save
	movl    %cr0, %eax
	movl    %eax, SAVEcr0
	andl    $0xfffeffff, %eax       / clear write protect to do breakpoints
	movl    %eax, %cr0
no_cr0_save:


/	save the users gdt 

	sgdt	GDTdscr

	movl    %esp, %ebp
	pushl	%ss
	movw    BDSsav, %ss
	pushl	%ebp
	call fault
	addl    $4,%esp

/BH	lgdt	GDTdscr

	cmpl    $1, %eax	/ Are we doing a single step?
	je	skip_unlock	/ Don't unlock other cpus, and just
				/ don't bother dropping the kadblock.

	movl    SAVEcr0, %ecx           / first time we will not get it
	jcxz    nocr0_saved             / may be we can shift this to within
	movl    %ecx, %cr0              / test for gs to be 1b0
nocr0_saved:
	movl    $-1, cur_cpuid

/ unlock the mutex now
	xorl    %ecx, %ecx
	xchgl   %ecx, kadblock

	cmpl    $-1, %eax	    / are we going back to cmntrap
	je      return_to_kernel    / if so, branch

/ we had some work todo so do not pass the int back to kernel

	popl	%ss
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	addl    $8, %esp        / get TRAPNO and ERROR off the stack

	/* restore the user idt */
	lidt	IDTdscr		
	iret

skip_unlock:
	movl    SAVEcr0, %ecx	/ first time we will not get it
	jcxz    nocr0_saved2
	movl    %ecx, %cr0
nocr0_saved2:
	popl	%ss
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	addl    $8, %esp        / get TRAPNO and ERROR off the stack

	/* use kadb idt */
	iret

/ We got an interrupt that should goto to OS so pass it on 
/ by calling special entery point
 
	.align  4
return_to_kernel:
	popl	%ss
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	/* restore the user idt and gdt*/
	lidt	%cs:IDTdscr		
	ljmp    $KCSSEL,$on_to_da_kernel
on_to_da_kernel:
	jmp	*%cs:kcmntrap

	.align	4
	.globl	psm_shutdownf
	.globl	pc_reset
pc_reset:
	cmpl    $0,cur_cpuid
	jne	poke_cpu0
pc_reset_on_cpu0:
	movl	psm_shutdownf,%eax
	testl	%eax,%eax
	jz	no_psm_call
	movl	(%eax),%eax
	testl	%eax,%eax
	jz	no_psm_call
	pushl	$AD_BOOT
	pushl	$A_SHUTDOWN
	call	*%eax
	addl	$8,%esp
no_psm_call:
	movw    $0x64, %dx
	movb    $0xfe, %al
	outb    (%dx)
	hlt
poke_cpu0:
	movl    $-2,cur_cpuid
self:	jmp	self

/ function argument to x_call.  All other processors should get
/ to this code while the one processor does kadb stuff.
/
/ This function should save away something that allows for kadb
/ to easily determine the state of other processors.

	.globl	i_fparray
	.globl	get_smothered
	.align  4
get_smothered:                          / on entry saved eip is where we will
	push    %cs                     / want the saved efl; save the cs
	pushfl                          / save the efl here for now. This
	cli                             / is where we will want the saved eip
	pushl   $-1                     / push a phony error
	pushl   $-1                     / push a phony trap number
	pushal
	push	%ds
	push	%es
	push	%fs
	push	%gs
	addl    $0x14, [ESP\*4](%esp)   / fix up %esp to point where we enterd
	movl    [EFL\*4](%esp), %eax    / get the saved eip
	xchgl   [EIP\*4](%esp), %eax    / store saved eip, get saved efl
	movl    %eax, [EFL\*4](%esp)    / store efl where it goes
	movzbl  %gs:CPU_ID,%esi
	pushl   %esi
	call    clear_kadb_debug_registers
	addl    $4, %esp

	cmpl    $NCPU, %esi
	jae     loop0
	movl    %esp,i_fparray(,%esi,4)
loop0:
	movl	$0xfffffff,%ecx
loop1:
	cmpl    $-2,cur_cpuid
	je	reboot_requested
	cmpl    $-1,cur_cpuid
	loopne	loop1
	cmpl    $-1,cur_cpuid
	jne	loop0
	cmpl    $NCPU, %esi
	jae     smothered_ret
	movl    $0,i_fparray(,%esi,4)
smothered_ret:
	pushl   %esi
	call    set_kadb_debug_registers
	addl    $4, %esp
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popal
	addl    $8, %esp        / skip over trap and err
	iret
reboot_requested:
	cmpl	$0,%esi			/ on cpu 0?
	jne	reboot_requested	/ if not, just loop
	jmp	pc_reset_on_cpu0

/ routine to initialize other mp cpus.
/ Cause any data breakpoints that might be set
/ to migrate cpus that are starting.

	.globl  kadb_mpinit
	.align  4
kadb_mpinit:
	movzbl  %gs:CPU_ID,%eax
	pushl   %eax
	call    clear_kadb_debug_registers
	addl    $4, %esp

	movzbl  %gs:CPU_ID,%eax
	pushl   %eax
	call    set_kadb_debug_registers
	addl    $4, %esp
	ret

	/ goto_kernel (int arg1, int arg2, int arg3, int pri, cpuset_t set,
	/   int (*func)(), void (*xc_call)());


	.globl	goto_kernel
	.align  4
goto_kernel:
	cli
	movl    28(%esp), %ecx          / get address of xc_call function
	popl    %eax                    / get return address
	movl    %eax, 24(%esp)          / remember it where xc_call fnptr was
	/ now set the seg registers the kernel expects.
	/ should we be taking this from the stack ?
	movl    $KDSSEL,%eax
	movw	%eax,%ds
	movw	%eax,%es
	movl    $KFSSEL,%eax
	movw	%eax,%fs
	movl    $KGSSEL,%eax
	movw	%eax,%gs

	pushl	$returnfrom_kernel	/return address for called routine

	pushf
	pushl   $KCSSEL
	pushl	%ecx

	lidt	IDTdscr		
	iret
returnfrom_kernel:
	cli			/ we dont know in what state kernel left us
	pushf
	pushl	BCSsav
	pushl	$set_kadb_cs
	iret
set_kadb_cs:
	movw	BDSsav, %ds
	movw	BESsav, %es
	movw	BFSsav, %fs
	movw	BGSsav, %gs
	pushl   24(%esp)            / get the address to return to
	lidt    BIDTdscr
	ret

	.globl	delayl
delayl:
	movl	4(%esp),%ecx
delayll:
	loop	delayll
	ret

/ After the kernel converts to using its standard CS, kadb can
/ go ahead and use all of the kernel's normal selector values.
	.globl	fix_kadb_selectors
fix_kadb_selectors:
	cmpw	$KCSSEL,4(%esp)
	je	change_sels
	ret
change_sels:
	movl    $KDSSEL, BDSsav
	movl    $KDSSEL, BESsav
	movl    $KFSSEL, BFSsav
	movl    $KGSSEL, BGSsav
	movl    $KCSSEL, BCSsav
	movw	BDSsav, %ds
	movw	BESsav, %es
	movw	BFSsav, %fs
	movw	BGSsav, %gs
	pushl	(%esp)
	movl	$KCSSEL, 4(%esp)
	lret

	/ implementation of ::call
	/
	/ retval = kernel_invoke(func, nargs, args);
	/
	/ call kernel function "func" with "nargs" number of long arguments

	.globl	kernel_invoke
kernel_invoke:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi

	movl	12(%ebp), %ecx	/ nargs
	movl	16(%ebp), %edi	/ args
1:	cmpl	$0, %ecx
	je	2f
	dec	%ecx
	pushl	(%edi,%ecx,4)
	jmp	1b

2:	movl	8(%ebp), %eax	/ func
	call	*%eax

	movl	12(%ebp), %ecx	/ nargs
	sall	$2,%ecx		/ 4 bytes per arg
	addl	%ecx, %esp	/ pop "nargs" args
	popl	%edi
	leave
	ret

	/ set_kadb_debug_registers (cpu)
	/ int cpu;

	/ The idea is that we will store any kadb  debug  registers
	/ while leaving the unused user debug registers alone.

	/ First we compute in %ebx a 4 bit value containing  a  bit
	/ mask  of  the debugging registers that the kadb wishes to
	/ set.  From this 4  bit  value,  a  mask  is  loaded  with
	/ indicates  the  valid  parts  of  %dr7 for the kadb debug
	/ registers that he wants set.  The kadb %dr7 (%ecx) is the
	/ "and" with this mask.

	/ Then we compute a new %dr7 (%edx)  that  has   the   user
	/ debug  registers  left  after  deleting  the user   debug
	/ registers  that  conflict  with  the   new   kadb   debug
	/ registers.   This  is  the  "and" with the inverse of the
	/ kadb mask.

	/ This %dr7 will have have LE and  GE  set  if   any   user
	/ debug  registers remain.  Kadb sets LE and GE whenever it
	/ sets kernel breakpoints.  Looked  like  a  good  idea  to
	/ always set when  there  is  any  debugging  register  set
	/ anyway.

	/ Then we compute the final %dr7 (%ecx) that is  the  merge
	/ of  the  user    and kadb breakpoints.  This value is the
	/ "or" of the remaining kernel debug registers and the user
	/ debug registers.

	/ Then each debugging register value that the  kadb  wanted
	/ is  stored  while  ignoring  ones  that  the kadb did not
	/ specify.

	/ Finally the merged %dr7 (%ecx) is stored.


	.align  4
	.globl  set_kadb_debug_registers
set_kadb_debug_registers:
	pushl   %ebp                        / C linkage
	movl    %esp, %ebp
	pushal                              / save all regs.  call us anywhere

	movl    dr_registers+28, %ecx       / get the kadb %dr7
	movzbl  %cl, %ebx                   / get the local/global enable byte
	movb    dbreg_control_enable_to_bkpts_table(%ebx), %bl

	movl    8(%ebp), %esi               / get the cpuid
	cmpl    $NCPU, %esi                 / is it in range
	jae     set_got_mask                / if not, branch
	orb     %bl, dr_cpubkptused(%esi)   / or in the kadb breakpoints used
set_got_mask:
	movl    dbreg_bkpts_to_set_mask_table(,%ebx,4), %eax
	andl    %eax, %ecx                  / clear bkpts kadb is not setting
	notl    %eax                        / inverse is mask to clear
	movl    %dr7, %edx                  / get the current %dr7
	andl    %eax, %edx                  / clear kernel bkpts
	cmpb    $1, %dl                     / if any kernel bkpts left
	cmc                                 / set local and global slowdown
	sbbb    %dh, %dh                    / the control word for remaining
	andb    $3, %dh                     / kernel bkpts is in edx
	orl     %edx, %ecx                  / ecx has merge of user and kadb
	movl    %edx, %dr7                  / store kernel bkpts left
test_setting_dr0:
	testb   $1, %bl                     / is kadb setting dr0?
	jz      test_setting_dr1            / if not, branch
	movl    dr_registers+0, %eax        / get new value of dr0
	movl    %eax, %dr0                  / store kadb dr0
test_setting_dr1:
	testb   $2, %bl                     / is kadb setting dr1?
	jz      test_setting_dr2            / if not, branch
	movl    dr_registers+4, %eax        / get new value of dr1
	movl    %eax, %dr1                  / store kadb dr1
test_setting_dr2:
	testb   $4, %bl                     / is kadb setting dr2?
	jz      test_setting_dr3            / if not, branch
	movl    dr_registers+8, %eax        / get new value of dr2
	movl    %eax, %dr2                  / store kadb dr2
test_setting_dr3:
	testb   $8, %bl                     / is kadb setting dr3?
	jz      store_new_dr7               / if not, branch
	movl    dr_registers+12, %eax       / get new value of dr3
	movl    %eax, %dr3                  / store kadb dr3
store_new_dr7:
	movl    %ecx, %dr7                  / store merged dr7
	popal                               / restore all regs. call us anywhr
	popl    %ebp
	ret

	/ clear_kadb_debug_registers (cpu)
	/ int cpu;

	/ The idea is that we will clear  any  debugging  registers
	/ that  the kadb had used.  Any  user  debug registers will
	/ be left alone.

	/ First we compute in %ebx a 4 bit value containing  a  bit
	/ mask  of  the debugging registers that the kadb had used.
	/ From this 4 bit value, a mask is  loaded  with  indicates
	/ the parts of %dr7 that the kadb had used.

	/ Then we compute a new %dr7  (%edx)  that  has  the   user
	/ debug  registers  left  after  deleting  the user   debug
	/ registers that conflict with the  just  used  kadb  debug
	/ registers.   This  is  the  "and" with the inverse of the
	/ user mask.

	/ This %dr7 will have have LE and  GE  set  if  any  kernel
	/ debug  registers remain.  Kadb sets LE and GE whenever it
	/ sets kernel breakpoints.  Looked  like  a  good  idea  to
	/ always set when  there  is  any  debugging  register  set
	/ anyway.

	/ Then each debugging register value that the kadb had used
	/ is cleared while ignoring ones that the kadb did not use.


	.align  4
	.globl  clear_kadb_debug_registers
clear_kadb_debug_registers:
	pushl   %ebp                        / C linkage
	movl    %esp, %ebp
	pushal                              / save all regs.  call us anywhere

	movl    $0xf, %ebx                  / presume kadb set all regs
	movl    8(%ebp), %esi               / get the cpu
	cmpl    $NCPU, %esi                 / is it in range
	jae     clr_got_used
	movzbl  dr_cpubkptused(%esi), %ebx  / get the breakpoints in use
clr_got_used:
	movl    dbreg_bkpts_to_set_mask_table(,%ebx,4), %eax
	notl    %eax                        / inverse is mask to clear
	movl    %dr7, %edx                  / get the current %dr7
	andl    %eax, %edx                  / clear kadb bkpts
	cmpb    $1, %dl                     / if any user bkpts left
	cmc                                 / set local and global slowdown
	sbbb    %dh, %dh                    / the control word for remaining
	andb    $3, %dh                     / kernel bkpts is in edx
	movl    %edx, %dr7                  / store user bkpts left
	xorl    %eax, %eax                  / clear the used dr[0123] regs
test_clearing_dr0:
	testb   $1, %bl                     / did kadb use dr0?
	jz      test_clearing_dr1           / if not, branch
	movl    %eax, %dr0                  / store cleared value of dr0
test_clearing_dr1:
	testb   $2, %bl                     / did kadb use dr1?
	jz      test_clearing_dr2           / if not, branch
	movl    %eax, %dr1                  / store cleared value of dr1
test_clearing_dr2:
	testb   $4, %bl                     / did kadb use dr2?
	jz      test_clearing_dr3           / if not, branch
	movl    %eax, %dr2                  / store cleared value of dr2
test_clearing_dr3:
	testb   $8, %bl                     / did kadb use dr3?
	jz      clear_return                / if not, branch
	movl    %eax, %dr3                  / store cleared value of dr3
clear_return:
	cmpl    $NCPU, %esi                 / is cpuid in range
	jae     clr_dont_store              / if not branch
	movb    $0, dr_cpubkptused(%esi)    / clear data breakpoints in use
clr_dont_store:
	popal                               / restore all regs. call us anywhr
	popl    %ebp
	ret


	/ The following table takes the low 8  bits  of  %dr7,  and
	/ returns  a  4  bit value indicating which breakpoints are
	/ present.  Bit 0 is on if either L0 or G0 are on.   Bit  1
	/ is  on  if either L1 or G1 are on.  Bit 2 is on if either
	/ L2 or G2 are on.  Bit 3 is on if either L3 or G3 are on.

#define DBREG_CONTROL_ENABLE_TO_BKPTS(x) \
    [[x & 1] | [[x >> 1] & 3] | [[x >> 2] & 6] | [[x >> 3] & 0xc]] | [[x >> 4] & 8]

	.align  4
	.globl	dbreg_control_enable_to_bkpts_table
dbreg_control_enable_to_bkpts_table:
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x00)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x01)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x02)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x03)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x04)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x05)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x06)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x07)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x08)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x09)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x0f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x10)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x11)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x12)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x13)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x14)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x15)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x16)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x17)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x18)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x19)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x1f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x20)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x21)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x22)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x23)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x24)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x25)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x26)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x27)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x28)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x29)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x2f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x30)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x31)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x32)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x33)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x34)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x35)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x36)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x37)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x38)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x39)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x3f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x40)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x41)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x42)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x43)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x44)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x45)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x46)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x47)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x48)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x49)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x4f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x50)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x51)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x52)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x53)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x54)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x55)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x56)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x57)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x58)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x59)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x5f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x60)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x61)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x62)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x63)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x64)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x65)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x66)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x67)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x68)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x69)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x6f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x70)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x71)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x72)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x73)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x74)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x75)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x76)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x77)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x78)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x79)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x7f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x80)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x81)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x82)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x83)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x84)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x85)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x86)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x87)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x88)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x89)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x8f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x90)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x91)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x92)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x93)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x94)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x95)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x96)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x97)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x98)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x99)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9a)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9b)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9c)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9d)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9e)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0x9f)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xa9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xaa)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xab)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xac)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xad)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xae)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xaf)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xb9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xba)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xbb)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xbc)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xbd)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xbe)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xbf)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xc9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xca)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xcb)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xcc)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xcd)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xce)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xcf)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xd9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xda)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xdb)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xdc)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xdd)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xde)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xdf)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xe9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xea)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xeb)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xec)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xed)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xee)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xef)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf0)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf1)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf2)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf3)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf4)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf5)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf6)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf7)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf8)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xf9)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xfa)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xfb)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xfc)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xfd)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xfe)
	.byte   DBREG_CONTROL_ENABLE_TO_BKPTS (0xff)

	/ The following table takes a 4 bit value indicating  which
	/ breakpoints  are present.  It returns a 32 bit mask which
	/ indicates  which  parts  of  %dr7  corresponds   to   the
	/ breakpoints  that  are  present.   If  bit 0 is on in the
	/ breakpoints source, then LEN0, RW0, and L0 will  be  set.
	/ If bit 1 is on in the breakpoints source, then LEN1, RW1,
	/ and  L1  will  be set.  If bit 2 is on in the breakpoints
	/ source, then LEN2, RW2, and L2 will be set.  If bit 3  is
	/ on in the breakpoints source, then LEN3, RW3, and L3 will
	/ be set.  For all table entries LE and GE are set.

#define DBREG_BKPTS_TO_MASK(x) \
	[[[x & 1] \* 0x000f0003] | [[[x >> 1] & 1] \* 0x00f0000c] | \
	[[[x >> 2] & 1] \* 0x0f000030] | [[[x >> 3] & 1] \* 0xf00000c0] | 0x300]

	.align  4
	.globl	dbreg_bkpts_to_set_mask_table
dbreg_bkpts_to_set_mask_table:
	.long    DBREG_BKPTS_TO_MASK (0x0)
	.long    DBREG_BKPTS_TO_MASK (0x1)
	.long    DBREG_BKPTS_TO_MASK (0x2)
	.long    DBREG_BKPTS_TO_MASK (0x3)
	.long    DBREG_BKPTS_TO_MASK (0x4)
	.long    DBREG_BKPTS_TO_MASK (0x5)
	.long    DBREG_BKPTS_TO_MASK (0x6)
	.long    DBREG_BKPTS_TO_MASK (0x7)
	.long    DBREG_BKPTS_TO_MASK (0x8)
	.long    DBREG_BKPTS_TO_MASK (0x9)
	.long    DBREG_BKPTS_TO_MASK (0xa)
	.long    DBREG_BKPTS_TO_MASK (0xb)
	.long    DBREG_BKPTS_TO_MASK (0xc)
	.long    DBREG_BKPTS_TO_MASK (0xd)
	.long    DBREG_BKPTS_TO_MASK (0xe)
	.long    DBREG_BKPTS_TO_MASK (0xf)
#endif	/* !defined(lint) */
