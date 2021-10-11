
#if !defined(lint)

	.file	"locore.s"
	.ident "@(#)locore.s	1.10	99/08/19 SMI"

	.globl	idt
	.globl	gdt
	.globl	romvec


/ The kadb stack's are used as follows
/  The 12k area that starts at rstk up till mstk-1 is our running kadb stack
/        ie our context uses this.
/  The mstk is our tmp setup stack used for kadb initialization and that is 
/        used by the app we will debug until it sets up its own stack.
/        We could reuse this area, but we do not know when its safe to do-so.
/  The value in estack is the top of the kadb stack. This is used for fault
/        checking in the exception handler.
	.data
	.align	4
	.comm	rstk,0xC000	/* set up our real 12K stack */

	.align 4
	.comm	mstk,0x1000	/* our setup stack and apps start up stack*/

	.data
	.align	4
	.globl	estack
estack:				/* The top of our stack (its empty here) */
	.long	0



	.align	4
	.data
	.long	IDTLIM		/* size of interrupt descriptor table */
	.set	IDTLIM, [8\*256-1]	/* IDTSZ = 256 (for now....) */

	.long	GDTLIM
	.set	GDTLIM, [8\*90-1]	/* GDTSZ = 90 (for now....) */

	.globl	ivect
ivect:
	.long	0
	.comm	romvec,20	/* array of syscall functions */



	/* Boot IDTR */
	.align	8
	.globl	BIDTdscr
BIDTdscr:
	.value	IDTLIM
	.long	idt

	/* Contents for our new IDTR */
	.globl	IDTdscr
IDTdscr:
	.value	IDTLIM
	.long	idt

	.globl	OIDTdscr
OIDTdscr:			/* pseudo (zero'd) IDTR contents */
	.value	0		/* to protect system while new */
	.long	0		/* IDT is under construction */

	/* contents for our new GDTR */
	.globl	GDTdscr
GDTdscr:
	.value  GDTLIM
	.long	gdt

	.align	8
	.globl	sysp
sysp:				/* boot services - common entry point */
	.long 0

	.globl	bopp
bopp:				/* boot services - common entry point */
	.long 0

	/* 
   	/* The following descriptors are used eventually
	/* to translate the pseudo descriptors to 386 descriptors. They
 	/* are also used for the lgdt and lidt intructions.
	/*/


	.globl pRgdtdscr
pRgdtdscr:
	.value	[8\*256-1]	/ Limit
	.long	gdt		/ Base

	.align	8
	.globl pRidtdscr
pRidtdscr:
	.value	[8\*256-1]	/ Limit
	.long	idt		/ Base

	.align	8
	.globl pBidtdscr
pBidtdscr:			/ IDT from boot program
	.value	[8\*256-1]	/ Limit
	.long	0		/ Base




/	.align	8
/	.globl	num
/num:
/	.long	0
/	.comm	c,4

	.section	.data1
	.align	4

/*	preserve current parameters (passed in from boot) */

	.align	4
_btstk:
	.long	0		/* boot stack pointer */
_tempa:
	.long	0		/* temp variable for %eax preservation */

/* 	our selector save area */
/* These are for the part of kadb context that is not on the stack and
 * needs to be restored when we run.
 */
	.globl	BESsav
	.globl	BFSsav
	.globl	BGSsav
	.globl	BCSsav
	.globl	BDSsav
BDSsav:
	.long	0		/* save of our ds */
BESsav:
	.long	0		/* save of our es */
BFSsav:
	.long	0		/* save of our fs */
BGSsav:
	.long	0		/* save of our gs */
BCSsav:
	.long	0		/* save of our cs */

	
/////////////////////////////////////////////////
/
/ THIS IS WHERE IT ALL STARTS, FIRST CODE RUN
/
/////////////////////////////////////////////////
	.text
	.align 4
	.globl  _start
	.globl  start
_start:
start:

/*	At this point, we are still running on the boot stack. */
/*	We will now find our stack and switch to it. */

	cli
	pushl	%ebp
	movl	%esp,%ebp
	movl	%eax, _tempa
	movl    %ecx, sysp
	movl	%ebx, bopp


	movl	%esp, _btstk	/* save the boot stack pointer */
	movl	$rstk, estack	/* set up our real stack */
	addl	$0xC000, estack

	movl	$mstk, %eax
	addl	$0x1000, %eax
	movl	%eax, %esp	/* activate our new stack */


/*	Clear out our bss space. */

	movl	$0, %eax	/* sstol users this for soruce */
	movl	$edata, %edi		/ starting address value
	movl	$end, %ecx		/ ending address val
	subl	%edi, %ecx		/ calc count of bytes
	shrl	$2, %ecx		/ Count of double words to zero
	repz
	sstol				/ %ecx contains words to clear(%eax=0)

/*	Set up stack to look like we are calling into the kernel */
	pushl	$elfbootvec
	pushl	$bootops
	pushl	$dvec
	pushl	sysp
	pushl	$0x0	/* no return */

	/* Save the boot (real mode) interrupt table pointer */

/*	lidt	OIDTdscr	/ protect system while IDT is being created */

/*	*** NOTICE *** NOTICE *** NOTICE *** NOTICE *** NOTICE *** */
/* */
/*		Do not try to single step past this point!!!! */
/*		use a 'go till' command!!!! */

	/* Prepare to create the new interrupt table */

	movl	$idt, %eax	/* pointer to IDT */
	movl	%eax, ivect
	movl	$IDTLIM, %ecx	/* size of IDT */
	call	munge_table	/* rearranges/builds table in place */

	lidt	IDTdscr		/* activate new interrupt table */
	sidt	BIDTdscr	/* save our idt for later use */

	/* create new GDT */

	movl	$gdt, %eax	/* pointer to GDT */
	movl	$GDTLIM, %ecx	/* size of GDT */
	call	munge_table	/* rearranges/builds table in place */

	/* copy valid entries from old gdt table to our gdt table */
	sgdt	pRgdtdscr
	movl	pRgdtdscr+2,%esi	/* src */
	movl	$gdt,%edi		/* dst */
	movzwl	pRgdtdscr,%ecx		/* limit */
	incl	%ecx			/* count = limit + 1 */
	repz
	smovb

	/* install our gdt */
	lgdt	GDTdscr

/*	Preserve as much of the previous (boot) environment as possible. */
	/* New stack has been created. */


	pushl	$0		/* user stack segment r_ss */
	pushl	$0		/* user stack pointer r_uesp */
	pushfl			/* flags r_efl */
	pushl	$0		/* code segment r_cs */
/	pushl	$bh_test_ret	/* eip r_eip */
	pushl	$0		/* eip r_eip */
	pushl	$0		/* err reg r_err */
	pushl	$0xd		/* trap # r_trapno */

	movl	_tempa, %eax
	pusha			/* save user registers */
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	/ save our ds, es, fs, gs, and cs

	movw	%ds, BDSsav
	movw	%es, BESsav
	movw	%fs, BFSsav
	movw	%gs, BGSsav
	movw	%cs, BCSsav


	movl	%esp, %ebp
	pushl	%ebp
	call	main 
	addl	$4, %esp 
/*	Restore previous environment */

/	lidt	BIDTdscr	/* replace original boot IDT */

	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	addl    $8, %esp        / get TRAPNO and ERROR off the stack

	popl	_tempa		/ preserve old eip
	addl	$4, %esp	/ throw away old cs
	popfl
	addl	$8, %esp	/ throw away old uesp, ss
	pushl	_tempa
	ret

/* this is the low-level call to exit from the debugger into the */
/* requested standalone application. */
/* the argument to this call is the destination function pointer */
	.globl	_exitto
_exitto:
	ret

/* get our current stack pointer for a c routine  */
/* to check which stack we are using  */
	.globl	getsp
getsp:
	movl	%esp, %eax
	ret

/* get our current frame pointer for a c routine  */
/* to check which stack we are using  */
	.globl	getbp
getbp:
	movl	%ebp, %eax
	ret

	.text


/* ********************************************************************* */
/* */
/*	munge_table: */
/*		This procedure will 'munge' a descriptor table to */
/*		change it from initialized format to runtime format. */
/* */
/*		Assumes: */
/*			%eax -- contains the base address of table. */
/*			%ecx -- contains size of table. */
/* */
/* ********************************************************************* */
munge_table:

	addl	%eax, %ecx	/* compute end of IDT array */
	movl	%eax, %esi	/* beginning of IDT */

moretable:
	cmpl	%esi, %ecx
	jl	donetable	/* Have we done every descriptor?? */

	movl	%esi, %ebx	/*long-vector/*short-selector/*char-rsrvd/*char-type */
			
	movb	7(%ebx), %al	/* Find the byte containing the type field */
	testb	$0x10, %al	/* See if this descriptor is a segment */
	jne	notagate
	testb	$0x04, %al	/* See if this destriptor is a gate */
	je	notagate
				/* Rearrange a gate descriptor. */
	movl	4(%ebx), %edx	/* Selector, type lifted out. */
	movw	2(%ebx), %ax	/* Grab Offset 16..31 */
	movl	%edx, 2(%ebx)	/* Put back Selector, type */
	movw	%ax, 6(%ebx)	/* Offset 16..31 now in right place */
	jmp	descdone

notagate:			/* Rearrange a non gate descriptor. */
	movw	4(%ebx), %dx	/* Limit 0..15 lifted out */
	movw	6(%ebx), %ax	/* acc1, acc2 lifted out */
	movb	%ah, 5(%ebx)	/* acc2 put back */
	movw	2(%ebx), %ax	/* 16-23, 24-31 picked up */
	movb	%ah, 7(%ebx)	/* 24-31 put back */
	movb	%al, 4(%ebx)	/* 16-23 put back */
	movw	(%ebx), %ax	/* base 0-15 picked up */
	movw	%ax, 2(%ebx)	/* base 0-15 put back */
	movw	%dx, (%ebx)	/* lim 0-15 put back */

descdone:
	addl	$8, %esi	/* Go for the next descriptor */
	jmp	moretable

donetable:
	ret


/*
 * set_gdt_entry(int selector, int address, int word2);
 *
 * set up the gdt entry associated with 'selector' to access
 * memory starting at 'address'.  'word2' is the of the format
 * consistent with the kernel gdt entry word 2 before it is munged.
 *
 * The algorithm is to store the unaltered arguments into the
 * gdt entry, then use the exact same munge code as above to
 * get the desired result.
 */
	.globl	set_gdt_entry
set_gdt_entry:
	pushl	%ebx

	movl	8(%esp),%ebx
	addl	$gdt,%ebx	/* pointer to gdt entry */
	movl	12(%esp),%eax
	movl	%eax,(%ebx)	/* word1 (address) */
	movl	16(%esp),%eax
	movl	%eax,4(%ebx)	/* word2 */

	/* munge */
	movw	4(%ebx), %dx	/* Limit 0..15 lifted out */
	movw	6(%ebx), %ax	/* acc1, acc2 lifted out */
	movb	%ah, 5(%ebx)	/* acc2 put back */
	movw	2(%ebx), %ax	/* 16-23, 24-31 picked up */
	movb	%ah, 7(%ebx)	/* 24-31 put back */
	movb	%al, 4(%ebx)	/* 16-23 put back */
	movw	(%ebx), %ax	/* base 0-15 picked up */
	movw	%ax, 2(%ebx)	/* base 0-15 put back */
	movw	%dx, (%ebx)	/* lim 0-15 put back */

	popl	%ebx
	ret

/*
 * dr0()
 *	return the value of register cr0
 *
 */
	.globl	dr0
dr0:
	movl	%dr0, %eax
	ret

/*
 * dr1()
 *	return the value of register cr1
 *
 */
	.globl	dr1
dr1:
	movl	%dr1, %eax
	ret

/*
 * dr2()
 *	return the value of register cr2
 *
 */
	.globl	dr2
dr2:
	movl	%dr2, %eax
	ret

/*
 * dr3()
 *	return the value of register cr3
 *
 */
	.globl	dr3
dr3:
	movl	%dr3, %eax
	ret

/*
 * dr6()
 *	return the value of register cr6
 *
 */
	.globl	dr6
dr6:
	movl	%dr6, %eax
	ret

/*
 * dr7()
 *	return the value of register cr7
 *
 */
	.globl	dr7
dr7:
	movl	%dr7, %eax
	ret

/*
 * setdr0()
 *      Set the value of register dr0
 */
	.globl	setdr0
setdr0:
	movl	4(%esp), %eax
	movl    %eax, %dr0
	ret

/*
 * setdr1()
 *      Set the value of register dr1
 */
	.globl	setdr1
setdr1:
	movl	4(%esp), %eax
	movl    %eax, %dr1
	ret

/*
 * setdr2()
 *      Set the value of register dr2
 */
	.globl	setdr2
setdr2:
	movl	4(%esp), %eax
	movl    %eax, %dr2
	ret

/*
 * setdr3()
 *      Set the value of register dr3
 */
	.globl	setdr3
setdr3:
	movl	4(%esp), %eax
	movl    %eax, %dr3
	ret

/*
 * setdr6()
 *      Set the value of register dr6
 */
	.globl	setdr6
setdr6:
	movl	4(%esp), %eax
	movl    %eax, %dr6
	ret

/*
 * setdr7()
 *      Set the value of register dr7
 */
	.globl	setdr7
setdr7:
	movl	4(%esp), %eax
	movl    %eax, %dr7
	ret


/*
 * cr0()
 *	return the value of register cr0
 *
 */
	.globl	cr0
cr0:
	movl	%cr0, %eax
	ret

/*
 * cr2()
 *      Return the value of register cr2
 */
	.globl	cr2
cr2:
	movl    %cr2,%eax
	ret

/*
 * cr3()
 *      Return the value of register cr3
 */
	.globl	cr3
cr3:
	movl    %cr3,%eax
	andl	$-1![0x10 + 0x08], %eax
	ret

/*
 * setcr2()
 *      Set the value of register cr2
 */
	.globl	setcr2
setcr2:
	movl	4(%esp), %eax
	movl    %eax, %cr2
	ret


/*
 * rtaskreg()
 *	Read task register
 */
	.globl	rtaskreg
rtaskreg:
        xorl    %eax, %eax
        str     %ax
	ret
			
/*
 * rgdt()
 *	Read the GDT register and return a pointer to the memory location.
 */
	.globl	rgdt
rgdt:
/	sgdt	pRgdtdscr		/ Get GDT register
/	movl	$pRgdtdscr, %eax	/ Return pointer to contents
	movl	$GDTdscr,   %eax	/ Return pointer to contents
	ret

/*
 * ridt()
 *	Read the IDT register and return a pointer to the memory location.
 */
	.globl	ridt
ridt:
/	sidt	pRidtdscr		/ Get IDT register
/	movl	$pRidtdscr, %eax	/ Return pointer to contents
	movl	$IDTdscr,   %eax	/ Return pointer to contents
	ret

/*
 * rldt()
 *	Read the LDT register and return a pointer to the memory location.
 */
	.globl	rldt
rldt:
	sldt	Rldtdscr		/ Get LDT register
	movl	$Rldtdscr, %eax		/ Return pointer to contents
	ret

	.data
Rldtdscr:
	.long	0
	.text

/*
 * getidt()
 *	Return the value of the IDT register.
 *	Returns the high order 4 bytes, which point to the IDT./
	.globl	getidt
getidt:
	sidt	pRidtdscr		/ Get IDT register
	movl	pRidtdscr+2, %eax	/ Save address bytes
	ret

/*
 * setidt()
 *	Sets the value of the IDT register.
 *	Also resets the pic controller.
 */
	.globl	setidt
setidt:
	cli
	movl	4(%esp), %eax		/ Get IDT address
	movl	%eax, pRidtdscr+2
	lidt	pRidtdscr		/ Set IDT
/	call	start8259
/ comment this out - it looks dangerous.....    vla fornow
	ret

/ Offsets on stack used for port code
	.set	PORT, 8
	.set	VAL, 12

/ read a byte from a port
/ ret = inb(POR)
	.globl	inb
inb:	pushl	%ebp
	movl	%esp, %ebp
	subl    %eax, %eax
	movw	PORT(%ebp), %dx
	inb	(%dx)
	popl	%ebp
	ret

	.globl	inw
inw:	pushl	%ebp
	movl	%esp, %ebp
	subl    %eax, %eax
	movw	PORT(%ebp), %dx
	inw	(%dx)
	popl	%ebp
	ret

	.globl	inl
inl:	pushl	%ebp
	movl	%esp, %ebp
	subl    %eax, %eax
	movw	PORT(%ebp), %dx
	inl	(%dx)
	popl	%ebp
	ret

/ write a byte to port
/ outb(PORT, VAL)
	.globl	outb
outb:	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movb	VAL(%ebp), %al
	outb	(%dx)
	popl	%ebp
	ret

	.globl	outw
outw:	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movb	VAL(%ebp), %al
	outw	(%dx)
	popl	%ebp
	ret

	.globl	outl
outl:	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movb	VAL(%ebp), %al
	outl	(%dx)
	popl	%ebp
	ret


/ This is called from main before we ever reach the cmnd interp.
/ This is so the first entry looks just like an interrupt (so the stack
/ is more uniform).
/ Trap will reset first_time then call cmd().
	.globl	fake_bp
fake_bp:
	int	$3
	int	$3
	int	$3
	jmp 	fake_bp


////////////////////////////////////////////////////////////////////////////////
/ This is hack code for debugging of promif interfaces to boot svc when they
/ do not work. This SHOULD be there only use. They should be removed soon
/
/ml_printf:	printf with string and single variable !
/ml_pause:	print a strings then wait for <cr> then return
////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
	.text
	.globl	ml_printf

ml_printf:
/#PROLOGUE# 0
	jmp	.L2000000
.L2000001:
/#PROLOGUE# 1
.L14:
	.text
	movl	$0,-16(%ebp)
	leal	12(%ebp),%eax
	movl	%eax,-8(%ebp)
.L15:
	movl	8(%ebp),%eax
	movsbl	(%eax),%eax
	movl	%eax,-4(%ebp)
	movl	-4(%ebp),%eax
	movl	%eax,-20(%ebp)
	incl	8(%ebp)
	movl	-20(%ebp),%eax
	movl	%eax,-24(%ebp)
       cmpl	$37,-24(%ebp)
	jne	.L19
	jmp	.L18
.L19:
.L16:
       cmpl	$0,-4(%ebp)
	je	.L21
	jmp	.L20
.L21:
	cmpl	$0,-16(%ebp)
	jne	.L23
	jmp	.L22
.L23:
       cmpl	$10,-16(%ebp)
	jl	.L27
	jmp	.L26
.L27:
.L24:
	incl	-16(%ebp)
       cmpl	$10,-16(%ebp)
	jl	.L24
	jmp	.L28
.L28:
.L26:
.L22:
	jmp	.LE12
.L20:
	pushl	-4(%ebp)
	call	ml_putchar
	popl	%ecx
	movl	8(%ebp),%eax
	movsbl	(%eax),%eax
	movl	%eax,-4(%ebp)
	movl	-4(%ebp),%eax
	movl	%eax,-28(%ebp)
	incl	8(%ebp)
	movl	-28(%ebp),%eax
	movl	%eax,-32(%ebp)
       cmpl	$37,-32(%ebp)
	jne	.L16
	jmp	.L30
.L30:
.L18:
	movl	8(%ebp),%eax
	movsbl	(%eax),%eax
	movl	%eax,-4(%ebp)
	incl	8(%ebp)
       cmpl	$100,-4(%ebp)
	je	.L32
	jmp	.L33
.L33:
       cmpl	$117,-4(%ebp)
	je	.L32
	jmp	.L34
.L34:
       cmpl	$111,-4(%ebp)
	je	.L32
	jmp	.L35
.L35:
       cmpl	$120,-4(%ebp)
	je	.L36
	jmp	.L31
.L36:
.L32:
       cmpl	$111,-4(%ebp)
	je	.L39
	jmp	.L38
.L39:
	movl	$8,-36(%ebp)
	jmp	.L40
.L38:
       cmpl	$120,-4(%ebp)
	je	.L42
	jmp	.L41
.L42:
	movl	$16,-40(%ebp)
	jmp	.L43
.L41:
	movl	$10,-40(%ebp)
.L43:
	movl	-40(%ebp),%eax
	movl	%eax,-36(%ebp)
.L40:
	pushl	-36(%ebp)
	movl	-8(%ebp),%eax
	movl	(%eax),%edx
	pushl	%edx
	call	ml_printn
	addl	$8,%esp
	addl	$4,-8(%ebp)
	jmp	.L44
.L31:
       cmpl	$115,-4(%ebp)
	je	.L46
	jmp	.L45
.L46:
	movl	-8(%ebp),%eax
	movl	(%eax),%edx
	movl	%edx,-12(%ebp)
	jmp	.L50
.L47:
	pushl	-4(%ebp)
	call	ml_putchar
	popl	%ecx
.L50:
	movl	-12(%ebp),%eax
	movsbl	(%eax),%eax
	movl	%eax,-4(%ebp)
	movl	-4(%ebp),%eax
	movl	%eax,-36(%ebp)
	incl	-12(%ebp)
	movl	-36(%ebp),%eax
	movl	%eax,-40(%ebp)
	cmpl	$0,-40(%ebp)
	jne	.L47
	jmp	.L51
.L51:
	addl	$4,-8(%ebp)
	jmp	.L52
.L45:
       cmpl	$108,-4(%ebp)
	je	.L54
	jmp	.L53
.L54:
	movl	8(%ebp),%eax
	movsbl	(%eax),%eax
	movl	%eax,-4(%ebp)
	incl	8(%ebp)
       cmpl	$100,-4(%ebp)
	je	.L56
	jmp	.L57
.L57:
       cmpl	$117,-4(%ebp)
	je	.L56
	jmp	.L58
.L58:
       cmpl	$111,-4(%ebp)
	je	.L56
	jmp	.L59
.L59:
       cmpl	$120,-4(%ebp)
	je	.L60
	jmp	.L55
.L60:
.L56:
       cmpl	$111,-4(%ebp)
	je	.L62
	jmp	.L61
.L62:
	movl	$8,-36(%ebp)
	jmp	.L63
.L61:
       cmpl	$120,-4(%ebp)
	je	.L65
	jmp	.L64
.L65:
	movl	$16,-40(%ebp)
	jmp	.L66
.L64:
	movl	$10,-40(%ebp)
.L66:
	movl	-40(%ebp),%eax
	movl	%eax,-36(%ebp)
.L63:
	pushl	-36(%ebp)
	movl	-8(%ebp),%eax
	pushl	(%eax)
	call	ml_printn
	addl	$8,%esp
	addl	$4,-8(%ebp)
.L55:
	jmp	.L67
.L53:
       cmpl	$89,-4(%ebp)
	je	.L69
	jmp	.L68
.L69:
	movl	$1,-16(%ebp)
.L68:
.L67:
.L52:
.L44:
	jmp	.L15
.LE12:
	leave
	ret
.L2000000:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$40,%esp
	jmp	.L2000001
	.set	LF12,40
/FUNCEND
	.type	ml_printf,@function
	.size	ml_printf,.-ml_printf
	.section	.data1
	.align	4
.L116:
	.byte	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39
	.byte	0x41,0x42,0x43,0x44,0x45,0x46,0x00
	.text
	.globl	ml_printn

ml_printn:
/#PROLOGUE# 0
	jmp	.L2000002
.L2000003:
/#PROLOGUE# 1
.L72:
	movl	$1,-12(%ebp)
       cmpl	$0,8(%ebp)
	jge	.L2000004
	movl	$1,%eax
	jmp	.L2000005
.L2000004:
	movl	$0,%eax
.L2000005:
	movl	%eax,-16(%ebp)
	cmpl	$0,-16(%ebp)
	jne	.L74
	jmp	.L73
.L74:
	movl	8(%ebp),%eax
	negl	%eax
	movl	%eax,8(%ebp)
.L73:
       cmpl	$0,12(%ebp)
	jge	.L2000006
	movl	$1,%eax
	jmp	.L2000007
.L2000006:
	movl	$0,%eax
.L2000007:
	movl	%eax,-20(%ebp)
	cmpl	$0,-20(%ebp)
	jne	.L76
	jmp	.L75
.L76:
	movl	12(%ebp),%eax
	negl	%eax
	movl	%eax,12(%ebp)
.L75:
       cmpl	$8,12(%ebp)
	je	.L78
	jmp	.L77
.L78:
	movl	$11,-24(%ebp)
	jmp	.L79
.L77:
       cmpl	$10,12(%ebp)
	je	.L81
	jmp	.L80
.L81:
	movl	$10,-24(%ebp)
	jmp	.L82
.L80:
       cmpl	$16,12(%ebp)
	je	.L84
	jmp	.L83
.L84:
	movl	$8,-24(%ebp)
.L83:
.L82:
.L79:
	cmpl	$0,-16(%ebp)
	jne	.L86
	jmp	.L85
.L86:
       cmpl	$10,12(%ebp)
	je	.L87
	jmp	.L85
.L87:
	movl	$0,-16(%ebp)
	pushl	$45
	call	ml_putchar
	popl	%ecx
.L85:
	movl	$0,-4(%ebp)
	movl	-24(%ebp),%eax
	cmpl	%eax,-4(%ebp)
	jl	.L91
	jmp	.L90
.L91:
.L88:
	movl	8(%ebp),%eax
	cltd
	idivl	12(%ebp)
	movl	%edx,-8(%ebp)
	cmpl	$0,-16(%ebp)
	jne	.L93
	jmp	.L92
.L93:
	movl	12(%ebp),%eax
	decl	%eax
	subl	-8(%ebp),%eax
	addl	-12(%ebp),%eax
	movl	%eax,-8(%ebp)
	movl	12(%ebp),%eax
	cmpl	%eax,-8(%ebp)
	jge	.L95
	jmp	.L94
.L95:
	movl	12(%ebp),%eax
	subl	%eax,-8(%ebp)
	movl	$1,-12(%ebp)
	jmp	.L96
.L94:
	movl	$0,-12(%ebp)
.L96:
.L92:
	leal	-36(%ebp),%eax
	movl	-4(%ebp),%edx
	movb	-8(%ebp),%cl
	movb	%cl,(%eax,%edx)
	movl	8(%ebp),%eax
	cltd
	idivl	12(%ebp)
	movl	%eax,8(%ebp)
       cmpl	$0,8(%ebp)
	je	.L98
	jmp	.L97
.L98:
       cmpl	$0,-16(%ebp)
	je	.L99
	jmp	.L97
.L99:
	jmp	.L90
.L97:
	incl	-4(%ebp)
	movl	-24(%ebp),%eax
	cmpl	%eax,-4(%ebp)
	jl	.L88
	jmp	.L100
.L100:
.L90:
	movl	-24(%ebp),%eax
	cmpl	%eax,-4(%ebp)
	je	.L102
	jmp	.L101
.L102:
	decl	-4(%ebp)
	jmp	.L103
.L101:
	cmpl	$0,-20(%ebp)
	jne	.L105
	jmp	.L104
.L105:
	decl	-24(%ebp)
	movl	-24(%ebp),%eax
	cmpl	%eax,-4(%ebp)
	jl	.L106
	jmp	.L104
.L106:
	movl	-24(%ebp),%eax
	subl	-4(%ebp),%eax
	movl	%eax,-12(%ebp)
       cmpl	$0,-12(%ebp)
	jg	.L110
	jmp	.L109
.L110:
.L107:
	pushl	$32
	call	ml_putchar
	popl	%ecx
	decl	-12(%ebp)
       cmpl	$0,-12(%ebp)
	jg	.L107
	jmp	.L111
.L111:
.L109:
.L104:
.L103:
       cmpl	$0,-4(%ebp)
	jge	.L115
	jmp	.L114
.L115:
.L112:
	.text
	leal	-36(%ebp),%eax
	movl	-4(%ebp),%edx
	movsbl	(%eax,%edx),%eax
	movsbl	.L116(%eax),%eax
	pushl	%eax
	call	ml_putchar
	popl	%ecx
	decl	-4(%ebp)
       cmpl	$0,-4(%ebp)
	jge	.L112
	jmp	.L117
.L117:
.L114:
.LE70:
	leave
	ret
.L2000002:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$36,%esp
	jmp	.L2000003
	.set	LF70,36
/FUNCEND
	.type	ml_printn,@function
	.size	ml_printn,.-ml_printn
	.data
	.globl	b_col
	.align	4
b_col:
	.data
	.align	4
	.long	0
	.type	b_col,@object
	.size       b_col,4
	.data
	.globl	b_row
	.align	4
b_row:
	.data
	.align	4
	.long	0x17
	.type	b_row,@object
	.size       b_row,4
	.data
	.globl	b_flag
	.align	4
b_flag:
	.data
	.align	4
	.long	0
	.type	b_flag,@object
	.size       b_flag,4
	.data
	.globl	dplybase
	.align	4
dplybase:
	.data
	.align	4
	.long	0xb0000
	.type	dplybase,@object
	.size       dplybase,4
	.text
	.globl	ml_putchar

ml_putchar:
/#PROLOGUE# 0
	jmp	.L2000008
.L2000009:
/#PROLOGUE# 1
.L124:
	.text
       cmpl	$23,b_row
	je	.L126
	jmp	.L125
.L126:
	cmpl	$0,b_flag
	jne	.L125
	jmp	.L127
.L127:
	movl	$1,b_flag
	call	ml_pause
	movl	$0,-12(%ebp)
       cmpl	$23,-12(%ebp)
	jle	.L132
	jmp	.L131
.L132:
.L129:
	movl	$0,-16(%ebp)
       cmpl	$80,-16(%ebp)
	jl	.L136
	jmp	.L135
.L136:
.L133:
	movl	-12(%ebp),%eax
	shll	$5,%eax
	leal	(%eax,%eax,4),%eax
	movl	-16(%ebp),%edx
	leal	(%edx,%edx),%edx
	addl	%edx,%eax
	addl	dplybase,%eax
	movl	%eax,-4(%ebp)
	movl	-4(%ebp),%eax
	movl	%eax,-8(%ebp)
	movl	-8(%ebp),%eax
	movb	$32,(%eax)
	incl	-16(%ebp)
       cmpl	$80,-16(%ebp)
	jl	.L133
	jmp	.L137
.L137:
.L135:
	incl	-12(%ebp)
       cmpl	$23,-12(%ebp)
	jle	.L129
	jmp	.L138
.L138:
.L131:
	movl	$0,b_row
	movl	$0,b_col
	movl	$0,b_flag
.L125:
	movsbl	8(%ebp),%eax
	cmpl	$10,%eax
	je	.L140
	jmp	.L139
.L140:
	movl	$0,b_col
	incl	b_row
	jmp	.L141
.L139:
	movl	b_row,%eax
	shll	$5,%eax
	leal	(%eax,%eax,4),%eax
	movl	b_col,%edx
	leal	(%edx,%edx),%edx
	addl	%edx,%eax
	addl	dplybase,%eax
	movl	%eax,-4(%ebp)
	movl	-4(%ebp),%eax
	movl	%eax,-8(%ebp)
	movl	-8(%ebp),%eax
	movb	8(%ebp),%dl
	movb	%dl,(%eax)
	incl	b_col
.L141:
	jmp	.LE122
.LE122:
	leave
	ret
.L2000008:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$16,%esp
	jmp	.L2000009
	.set	LF122,16
/FUNCEND
	.type	ml_putchar,@function
	.size	ml_putchar,.-ml_putchar
	.section	.data1
	.align	4
.L145:
	.byte	0x48,0x69,0x74,0x20,0x61,0x6e,0x79,0x20,0x6b,0x65
	.byte	0x79,0x20,0x74,0x6f,0x20,0x63,0x6f,0x6e,0x74,0x69
	.byte	0x6e,0x75,0x65,0x20,0x2e,0x2e,0x2e,0x0a,0x00
	.text
	.globl	ml_pause

ml_pause:
/#PROLOGUE# 0
	jmp	.L2000010
.L2000011:
/#PROLOGUE# 1
.L144:
	.text
	pushl	$.L145
	call	ml_printf
	popl	%ecx
	movl	$0,-8(%ebp)
       cmpl	$2,-8(%ebp)
	jl	.L149
	jmp	.L148
.L149:
.L146:
	jmp	.L154
.L151:
.L154:
	pushl	$100
	call	inb
	popl	%ecx
	andl	$1,%eax
	cmpl	$0,%eax
	je	.L151
	jmp	.L155
.L155:
	pushl	$96
	call	inb
	popl	%ecx
	movb	%al,-1(%ebp)
	pushl	$174
	call	kdskbyte
	popl	%ecx
	incl	-8(%ebp)
       cmpl	$2,-8(%ebp)
	jl	.L146
	jmp	.L157
.L157:
.L148:
	jmp	.LE142
.LE142:
	leave
	ret
.L2000010:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	jmp	.L2000011
	.set	LF142,8
/FUNCEND
	.type	ml_pause,@function
	.size	ml_pause,.-ml_pause
	.text
	.globl	kdskbyte

kdskbyte:
/#PROLOGUE# 0
	jmp	.L2000012
.L2000013:
/#PROLOGUE# 1
.L160:
	jmp	.L164
.L161:
.L164:
	pushl	$100
	call	inb
	popl	%ecx
	testl	$2,%eax
	jne	.L161
	jmp	.L165
.L165:
	movzbl	8(%ebp),%eax
	pushl	%eax
	pushl	$100
	call	outb
	addl	$8,%esp
.LE158:
	leave
	ret
.L2000012:
	pushl	%ebp
	movl	%esp,%ebp
	jmp	.L2000013
	.set	LF158,0
/FUNCEND
	.type	kdskbyte,@function
	.size	kdskbyte,.-kdskbyte
	.ident	"acomp: (CDS) SPARCompilers 2.0 pre-beta 07 Jan 1992"

#endif
#endif	/* !defined(lint) */
