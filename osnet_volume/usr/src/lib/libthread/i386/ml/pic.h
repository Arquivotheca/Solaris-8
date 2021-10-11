/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)pic.h	1.4	94/11/03	SMI"

/*
 * PIC support
 */

#ifdef PIC
#define	unsafe_pic_prolog(lab) \
	call	lab; \
lab:	popl	%ebx; \
	addl	$_GLOBAL_OFFSET_TABLE_+[.-lab], %ebx

#define	pic_prolog(lab) \
	pushl	%ebx; \
	call	lab; \
lab:	popl	%ebx; \
	addl	$_GLOBAL_OFFSET_TABLE_+[.-lab], %ebx
#define	pic_epilog \
	popl	%ebx

#define	fcnref(routine) \
	routine@PLT

#define	dataaddr(sym) \
	sym@GOT(%ebx)		/* GOT contains pointers */
#else
#define	unsafe_pic_prolog(lab)
#define	pic_prolog(lab)
#define	pic_epilog

#define	fcnref(routine) \
	routine

#define	dataaddr(sym) \
	$sym
#endif
