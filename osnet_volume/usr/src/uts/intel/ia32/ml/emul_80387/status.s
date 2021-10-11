/*
 * Copyright (c) 1993,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)status.s	1.2	99/05/04 SMI"

#if !defined(lint)

/	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
/	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
/	  All Rights Reserved

/	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
/	UNIX System Laboratories, Inc.
/	The copyright notice above does not evidence any
/	actual or intended publication of such source code.

	.file	"status.s"

/$tt("80387	emulator	+ + + s t a t u s + + + ")
/ ************************************************************************
/
/                        s t a t u s . m o d
/                        ===================
/
/	================================================================
/               intel corporation proprietary information
/    this software  is  supplied  under  the  terms  of  a  license
/    agreement  or non-disclosure agreement  with intel corporation
/    and  may not be copied nor disclosed except in accordance with
/    the terms of that agreement.
/	=================================================================
/
/	function:
/	       operation cluster for 80387 status register
/
/	public procedures:
/		stcw			stenv			pop_free
/		incr_top		decr_top		save_status
/		restore_status		clear_p_error		init
/		clex			stsw			ldcw
/		get_precision		store_precision		get_rnd_control
/		store_rnd_control	u_masked?		z_masked?
/		d_masked?		i_masked?		o_masked?
/		get_reg_tag		affine_infinity?	get_top
/		set_p_error		p_error?		set_u_error
/		set_o_error		set_z_error		set_i_error
/		i_error?		d_error?		set_d_error
/		set_z_bit		clear_z_bit		set_s_bit
/		clear_s_bit		set_a_bit		clear_a_bits
/		set_c_bit		clear_c_bit		store_reg_tag
/		set_i_masked?		set_d_masked?		set_z_masked?
/		clear_cond_bits
/
/ ************************************************************************

#endif	/* lint */

#if defined(lint)

void
stcw(void)
{}

#else	/* lint */

/
/...March 3, 1987...
/
	.data	/a_msr	segment	rw	public
/
/...define the 80387 status register...
/
#include	"e80387.h"
/
	.globl	incr_top
	.globl	decr_top
	.globl	save_status
	.globl	pop_free
	.globl	ldcw
	.globl	stenv
	.globl	clex
	.globl	stsw
	.globl	stcw
	.globl	get_precision
	.globl	store_precision
	.globl	get_rnd_control
	.globl	store_rnd_control
	.globl	u_masked_
	.globl	d_masked_
	.globl	get_top
	.globl	i_masked_
	.globl	o_masked_
	.globl	get_reg_tag
	.globl	affine_infinity_
	.globl	set_p_error
	.globl	set_u_error
	.globl	set_o_error
	.globl	init
	.globl	clear_p_error
	.globl	set_i_error
	.globl	set_d_error
	.globl	i_error_
	.globl	d_error_
	.globl	p_error_
	.globl	set_z_bit
	.globl	clear_z_bit
	.globl	set_s_bit
	.globl	clear_s_bit
	.globl	set_a_bit
	.globl	clear_cond_bits
	.globl	set_c_bit
	.globl	clear_c_bit
	.globl	store_reg_tag
	.globl	set_i_masked_
	.globl	set_d_masked_
	.globl	set_z_masked_
	.globl	clear_a_bit
	.globl	restore_status
	.globl	set_stk_u_error
	.globl	set_stk_o_error
	.globl	correct_tag_word
	.globl	free_reg
/
	.text	/a_med	segment	er	public
/	--------to be added for unix
/..		extrn	fpfulong:far,fpfushort:far,fpsulong:far,fpsushort:far
/
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/
/	the routines in this section manipulate the status and control
/	information stored in the 80387 status data segment, a?msr.
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/	transfer field group:	get_precision, store_precision, get_rnd_control
/	 store_rnd_control, get_top, get_reg_tag, store_reg_tag
/
/	inputs:	the "get" routines require no input parameters, except for the
/		procedures get_reg_tag and store_reg_tag which expect the
/		register number to be in al and cl, respectively
/		the "store" routines input the value to be stored in al.
/
/	outputs:  get_precison returns the 2-bit precision field in dl.
/		  all other routines return justified values in al.
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

get_precision:	/proc
	movb	$precision_mask,%dl	/ load precision field mask
	andb    SR_CONTROLS,%dl   / mask in precision control
	ret
/get_precision	endp
/
store_precision:	/proc
	andb    $ -1! precision_mask,SR_CONTROLS / clear precision
	orb     %al,SR_CONTROLS          / store new precision control
	ret
/store_precision	endp
/
get_rnd_control:	/proc
	movb    SR_CONTROLS,%al             / load control byte
	andb	$rnd_control_mask,%al	/ mask in rounding control
	ret
/get_rnd_control	endp
/
store_rnd_control:	/proc
	andb    $-1 ! rnd_control_mask,SR_CONTROLS / clear old field
	orb     %al,SR_CONTROLS             / store new rounding control
	ret
/store_rnd_control	endp
/
get_top:	/proc
	movb    SR_FLAGS,%al                / load status flag byte
	andb	$top_mask,%al		/ mask in top field
	shrb	$3,%al			/ right justify top field
	ret
/get_top	endp
/
get_reg_tag:	/proc
	leal    SR_REGSTACK,%ebx / start of SR_REGSTACK
	movzbl	%al,%eax	/ form word bit count
	mov	%eax,%ecx	/ store in cx
	shl	$1,%ecx		/ bit count = 2 * reg num
	add     %ecx,%ebx       / else, must examine register
	movw    SR_TAGS,%ax     / load tag word
	shrw	%cl,%ax		/ shift to tag of interest
	shl	$2,%ecx		/ form index to SR_REGSTACK
	add	%ecx,%ebx	/ index = 10 * register num
	andb	$empty,%al		/ mask out other tags
	cmpb	inv,%al			/ if tag not = 2,
	jne	got_tag			/ no further decoding needed
	movw    SR_SEG:8(%ebx),%ax  / load register exponent
	andw	$0x7fff,%ax	/ mask off sign bit
	jnz	check_unsupp	/ if expon /= 0, check unsupported format
	movb	denormd,%al		/ if exponent zero, denormal
	ret
check_unsupp:
	mov     SR_SEG:4(%ebx),%eax        / get top 32 bits of significand
	test	$0x080000000,%eax	/ is highest bit set.
	jnz	check_nan			/ if so, check for nan
	movb	unsupp,%al		/ if not, it's an unsupported format.
	ret
check_nan:
	and	$0x07fffffff,%eax	/ zero out most significant bit
	or      SR_SEG:(%ebx),%eax      / or bottom 32 bits into top 32 bits.
	movb	infinty,%al		/ tentatively set tag to infinity's
	jz	got_tag			/ if fraction is 0, it's infinity
	movb	inv,%al			/ fraction is non zero, so tag as nan.
got_tag:
	ret
/get_reg_tag	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/	test status bit group:	u_masked?, z_masked?, d_masked?, i_masked?,
/	 o_masked?, affine_infinity?, p_error?, i_error?, d_error?,
/	 set_i_masked?, set_d_masked?
/
/	inputs:	 no input values are required.
/
/	outputs:  all boolean function return the complemented bit value
/	 in the zf.  (test with jz on bit = 0.)
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
u_masked_:	/proc
	testb   underflow_mask,SR_MASKS     / test the u mask
	ret
/u_masked_	endp
/
set_d_masked_:	/proc
	call	set_d_error		/ set the d error
d_masked_:
	testb   denorm_mask,SR_MASKS        / test the d mask
	ret
/set_d_masked_	endp
/
set_i_masked_:	/proc
	call	set_i_error		/ set the i error
i_masked_:
	testb   invalid_mask,SR_MASKS       / test the i mask
	ret
/set_i_masked_	endp
/
o_masked_:	/proc
	testb   overflow_mask,SR_MASKS      / test the o mask
	ret
/o_masked_	endp
/
affine_infinity_:	/proc
	testb   infinity_control_mask,SR_CONTROLS / test ic
	ret
/affine_infinity_	endp
/
p_error_:	/proc
	testb   $inexact_mask,SR_ERRORS     / test the p error
	ret
/p_error_	endp
/
i_error_:	/proc
	testb   invalid_mask,SR_ERRORS      / test the i error
	ret
/i_error_	endp
/
d_error_:	/proc
	testb   denorm_mask,SR_ERRORS       / test the d error
	ret
/d_error_	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/	set and reset bit group:  set_p_error, set_u_error, set_o_error
/	 set_z_masked?, set_i_error, set_d_error, clear_s_bit, clear_z_bit
/	 set_s_bit, set_z_bit, set_a_bit, clear_a_bit, set_c_bit, clear_c_bit
/	 clear_cond_bits, clear_p_error, set_stk_u_error, set_stk_o_error
/
/	inputs:	 no input values are required.
/
/	outputs:  all procedures set or reset the indicated status bit
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
set_stk_u_error:	/proc
	orb     invalid_mask+zero_mask,SR_ERRORS
	andb    $-1 ! a_mask,SR_FLAGS       / clear the a-bit
	ret
/set_stk_u_error	endp
/
set_stk_o_error:	/proc
	orb     invalid_mask+zero_mask,SR_ERRORS
	orb     $a_mask,SR_FLAGS            / set the a-bit
	ret
/set_stk_o_error	endp
/
set_p_error:	/proc
	orb     $inexact_mask,SR_ERRORS     / set the p-error
	ret
/set_p_error	endp
/
set_u_error:	/proc
	orb     underflow_mask,SR_ERRORS / set the u-error
	ret
/set_u_error	endp
/
set_o_error:	/proc
	orb     overflow_mask,SR_ERRORS     / set the o-error
	ret
/set_o_error	endp
/
set_i_error:	/proc
	orb     invalid_mask,SR_ERRORS      / set the i-error
	ret
/set_i_error	endp
/
set_d_error:	/proc
	orb     denorm_mask,SR_ERRORS       / set the d-error
	ret
/set_d_error	endp
/
set_z_masked_:	/proc
	orb     zero_divide_mask,SR_ERRORS / set the z-error
	testb   zero_divide_mask,SR_MASKS / test the z mask
	ret
/set_z_masked_	endp
/
set_s_bit:	/proc
	orb     $sign_mask,SR_FLAGS / set the s-bit
	ret
/set_s_bit	endp
/
set_z_bit:	/proc
	orb     $zero_mask,SR_FLAGS / set the z-bit
	ret
/set_z_bit	endp
/
set_a_bit:	/proc
	orb     $a_mask,SR_FLAGS            / set the a-bit
	ret
/set_a_bit	endp
/
set_c_bit:	/proc
	orb     $c_mask,SR_FLAGS            / set the c-bit
	ret
/set_c_bit	endp
/
clear_s_bit:	/proc
	andb    $-1 ! sign_mask,SR_FLAGS    / clear the s-bit
	ret
/clear_s_bit	endp
/
clear_z_bit:	/proc
	andb    $ -1 ! zero_mask,SR_FLAGS   / clear the z-bit
	ret
/clear_z_bit	endp
/
clear_a_bit:	/proc
	andb    $-1 ! a_mask,SR_FLAGS       / clear the a-bit
	ret
/clear_a_bit	endp
/
clear_cond_bits:	/proc
	andb    $-1 ![c_mask+zero_mask+sign_mask+a_mask],SR_FLAGS
	ret				/ clear all condition bits
/clear_cond_bits	endp
/
clear_c_bit:	/proc
	andb    $-1! c_mask,SR_FLAGS        / clear the c-bit
	ret
/clear_c_bit	endp
/
clear_p_error:	/proc
	andb    $-1 ! inexact_mask ,SR_ERRORS       / clear the p-error
	ret
/clear_p_error	endp
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			pop_free:
/	function:
/		pops the stack and/or frees the register(s) as required
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
pop_free:	/proc
	call	i_masked_		/ is invalid masked?
	jnz	check_op1		/ if so, forget error
	call	i_error_		/ check for i-error
	jnz	common_return		/ if error, don't pop

free_reg:
check_op1:
	movb	op1_use_up(%ebp),%al	/ pop or free op1
	call	process_use_up
	movb	op2_use_up(%ebp),%al	/ pop or free op2
process_use_up:
	cmpb	pop_stack,%al		/ is it a pop_stack?
	je	pop_it			/ yes, process it
	xorb	free,%al			/ no, is it free reg?
	jnz	exit_process		/ no, done with use_up
	call	get_top			/ yes, get top pointer
	addb	reg_num(%ebp),%al	/ convert relative num
	andb	$0x07,%al			/  to absolute reg num
empty_reg_tag:
	movb	%al,%cl			/ cl = reg number
	movb	$empty,%al		/ al = new tag value
store_reg_tag:
	shlb	$1,%cl			/ bit count = 2 * reg num
	rorw    %cl,SR_TAGS         / rotate tag to low bits
	andb    $-1 ! empty,SR_TAGS / clear old reg num tag
	andb	$empty,%al		/ clear bits 2-7 of new tag
	orb     %al,SR_TAGS / store new reg num tag and
	rolw    %cl,SR_TAGS         /  rotate the tag word back
exit_process:
	ret
pop_it:
	call	get_top			/ pop the stack once
/	the following six instructions have been commented out to prevent
/   an invalid operation from being signaled when the '87 tos is subject
/	to a freep st(i) (*6 in the 80387 t-spec).  in other words, it will
/	be o.k. to free st(0) and decrement stackpointer, so it will also be
/   o.k. to free an already empty st(0).
/		call	get_reg_tag		; get register tag
/		cmp	al,empty		; is the top empty?
/		jne	pop_ok			; no, stack may be popped
/		call	set_i_masked?		; yes, stack error
/		jz	exit_process		; abort if unmasked
/pop_ok:
/		call	get_top
	call	empty_reg_tag		/set tag of top empty
/pop_free	endp				; enter incr_top
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			incr_top:
/	function:
/		increments stack pointer
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
incr_top:	/proc
	movb	$0x08,%cl			/ load increment top constant
	jmp	adjust_top	/ merge with decr_top
/incr_top	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			decr_top:
/	function:
/		decrements stack pointer
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
decr_top:	/proc
	movb	$0x38,%cl			/ load decrement top constant
adjust_top:
	movb    SR_FLAGS,%al                / get old top
	andb	$top_mask,%al
	xorb    %al,SR_FLAGS                / clear old top field
	addb	%cl,%al			/ increment/decrement top
	andb	$top_mask,%al		/ mask off bits 6-7
	orb     %al,SR_FLAGS                / store new top field
common_return:
	ret
/decr_top	endp
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			restore_status:
/	function:
/		implements the 80387 ldenv instruction.
/		restores status register from memory.
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
restore_status:	/proc
	push	%ds			/ save %ds
	push    SR_SEG
	pop	%es			/ load destination base
	lds	mem_operand_pointer(%ebp),%esi / load environment pointer
	leal    SR_MASKS,%edi           / load dest offset
	mov	$0x0007,%ecx		/ load environment from memory
	cmpb	$1,oprnd_siz32(%ebp)	/is it a 32 bit operand
	jne	restore_status16	/no restore 16 bit status
	rep
	movsl				/ move environment to a?msr
	pop	%ds
	orb     $0x40,SR_MASKS              / set stack mask
	jmp	correct_tag_word
restore_status16:					/16 bit protected mode
	movsw				/ mov words
	inc	%edi
	inc	%edi
	loop	restore_status16
/	---------


/------------------------------------------------------------------------
/..		pushad
/..		push	ds
/..restore_status32:
/..		push	esi
/..		call	fpfulong
/..		pop	esi
/..		stosd
/..		add	esi,4
/..		loop	restore_status32
/..		pop	ds
/..		popad
/..		cld
/..		ret
/..restore_status16:
/..		pushad
/..		push	ds
/..restor_loop:
/..		push	esi
/..		call	fpfushort
/..		pop	esi
/..		stosw
/..		add	esi,2
/..		add	edi,2
/..		loop	restor_loop
/..		pop	ds
/..		popad
/..		cld
/---------------------------------------------------------------------
	pop	%ds
	orb     $0x40,SR_MASKS              / set stack mask
correct_tag_word:
	mov	$8,%ecx
	xor	%ebx,%ebx
set_tags_loop:		/looping through physical locations of 0-7 regs
	mov	$10,%eax
	mul	%ebx
	leal    SR_REGSTACK,%esi
	add	%eax,%esi
	mov	$0x00000003,%eax
	and     SR_TAGS,%eax
	cmp	$3,%eax
	je	loop_back
	movw    SR_SEG:8(%esi),%ax
	andw	$0x7fff,%ax			/ strip off sign bit
	jz	expon_zero				/ branch if exponent 0
	testb   $0x80,SR_SEG:7(%esi)
	jnz	max_expon_
	movb	$2,%dl				/ we have an
	jmp	set_tag				/ unsupported format
max_expon_:
	cmpw	$0x7fff,%ax			/ check for invalid
	je	not_validx			/ or infinity
	movb	$0,%dl		 		/ operand is valid
	jmp	set_tag
not_validx:
	movb	$2,%dl
	jmp	set_tag
expon_zero:
	mov     SR_SEG:4(%esi),%eax
	or      SR_SEG:(%esi),%eax
	movb	$1,%dl				/ set tag to special
	jz	set_tag				/ if number is +/- 0
	movb	$2,%dl				/ set tag to denormd
set_tag:
	andb    $-1 ! empty,SR_TAGS / clear old reg num tag
	orb     %dl,SR_TAGS / store new reg num tag and
loop_back:
	rorw    $2,SR_TAGS          /  get next two bits
	inc	%ebx
	loop	set_tags_loop
	ret
/restore_status	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			init:
/	function:
/		implements 80387 init instruction.  intializes
/		status register including mode word and error mask.
/
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
init:	/proc
	movw	$0x137f,%ax		/ initialize mode word
	movw    %ax,SR_MASKS
	movw	$0x0ffff,%ax		/ ax = 0ffffh
	movw    %ax,SR_TAGS         / register tags = empty
	incw	%ax			/ ax = 0000h
	movw    %ax,SR_ERRORS               / clear the error flags
/               and     SR_FLAGS,not top_mask       ; top of stack = 0, cbit=0
	ret
/init	endp
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			ldcw:
/	function:
/		implements 80387 ldcw instruction.  80387 control word
/		loaded (from memory location specified in instruction).
/
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
ldcw:	/proc
	les	mem_operand_pointer(%ebp),%ebx /get new mode word
	movw	%es:(%ebx),%ax
/..
/..		pushad
/..		push	es
/..		push	ebx
/..		call	fpfushort
/..		pop	ebx
/..		pop	es
/..		popad
/..		cld
/..
	orw	$0x40,%ax			/ set stack mask
	movw    %ax,SR_MASKS        / store in status reg
	ret
/ldcw	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			stenv:
/	function:
/		implements 80387 fstenv instruction.  80387 environmemt
/		stored (in memory location specified in the instruction).
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
stenv:	/proc
	call	save_status		/ store status to memory
	orb     $0x07f,SR_MASKS             / set all individual masks
	ret
/stenv	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			clex:
/	function:
/		 clears all 80387 errors set in status register
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
clex:	/proc
	movb    $0,SR_ERRORS                / clear error byte
	ret
/clex	endp
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/                        stsw
/       function:
/                stores status word in memory location.  (i.e., implements
/                fstsw instruction.)  also implements 'fstsw ax'.
/
/""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
stsw:	/proc
	movb	op2_location(%ebp),%al	/ load op2 location
	cmpb	$reg,%al			/ is this fstsw ax?
	movw    SR_ERRORS,%ax       / get status word
	je	stsw_ax			/ branch on fstsw ax
store_word:
	les	mem_operand_pointer(%ebp),%ebx
	movw	%ax,%es:(%ebx)           	/ store status to memory

/..		pushad
/..		push	es
/..		push	ebx
/..		push	ax
/..		call	fpsushort
/..		add	esp,12
/..		popad
/..		cld
/..
	ret
stsw_ax:
	movw	%ax,saved_eax(%ebp)	/ store into register ax
/			above instruction left for completeness but the ax
/			really has to be restored to the EAX above the global
/			re-entrant segment
	movl	%eax, [offset_eax](%ebp)
	ret
/stsw	endp
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			stcw:
/	function:
/		stores control word in memory location. (i.e., implements
/		fstcw instruction.)
/
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
stcw:	/proc
	movw    SR_MASKS,%ax        /get control word
	andw	$0x1f7f,%ax
	orw	$0x1040,%ax		/set/reset reserved bits
	jmp	store_word           	/store it
/stcw	endp
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
/			save_status:
/	function:
/		saves status register to location specified by
/		memory operand pointer(in es:di)
/
/"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
save_status:	/proc
	push	%ds			/ save %ds
	push    SR_SEG
	pop	%ds			/ and set it to address sr_masks
	andw    $0x1f7f,SR_MASKS
	orw     $0x1040,SR_MASKS    /set/reset reserved bits
	leal    SR_MASKS,%esi       / load source offset
	les	mem_operand_pointer(%ebp),%edi / load destination pointer
	mov	$0x0007,%ecx
	cmpb	$1,oprnd_siz32(%ebp)
	jne	save_status16
	rep
	movsl				/ store 80387 state to memory
	pop	%ds			/ restore %ds
	ret
save_status16:					/16 bit protected mode
	movsw				/ mov words
	inc	%esi
	inc	%esi
	loop	save_status16
/------------------------------------------------------------------------
/..		pushad
/..		push	es
/..save_status32:
/..		push	edi
/..		push	dword ptr [esi]
/..		call	fpsulong
/..		add	esp,8
/..		add	esi,4
/..		add	edi,4
/..		loop	save_status32
/..		pop	es
/..		popad
/..		cld
/..		ret
/..save_status16:
/..		pushad
/..		push	es
/..save_loop:
/..		push	edi
/..		push	word ptr [esi]
/..		call	fpsushort
/..		add	esp,8
/..		add	edi,2
/..		add	esi,4
/..		loop	save_loop
/..		pop	es
/..		popad
/..		cld
	pop	%ds			/ restore %ds
	ret
/save_status	endp
/
/a_med	ends
/
/	end

#endif	/* lint */
