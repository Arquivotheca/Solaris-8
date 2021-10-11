/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ident	"@(#)audio_79C30_intr.s	1.16	92/07/14 SMI"

#ifdef lint
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/audio_79C30.h>
#endif
#include <sys/asm_linkage.h>

#ifndef lint
#include "audio_assym.h"
#endif

/*
 * This is the assembly language level 13 trap handler for the AM79C30 audio
 * device.  It is derived from C code (at the bottom of audio_79C30.c).
 *
 * Normally, AUDIO_C_TRAP is not defined.
 * If handling interrupts from C, don't compile anything in this file.
 */
#if !defined(AUDIO_C_TRAP)

#ifdef lint
uint_t
audio_79C30_asmintr()
{
	return (DDI_INTR_CLAIMED);
}
#else /* !lint */

#define	dataseg		.seg	".data"
#define	textseg		.seg	".text"

/* Return from interrupt code */
#define	RETI		mov %l0,%psr; nop; nop; jmp %l1; rett %l2
#define	Zero		%g0		/* Global zero register */


/* %l3-%l7 are scratch registers. %l2 must be saved and restored. */
#define	Int_Active	%l7
#define	Chip		%l6
#define	Unitp		%l5
#define	Cmdp		%l4
#define	Temp1		%l3
#define	Temp2		%l2

/* Int_Active register bits: schedule level 4 intr, stop device when inactive */
#define	Interrupt	1
#define	Active		2

/*
 * Since this is part of a Sparc "generic" module, it may be loaded during
 * reconfigure time on systems that do not support the fast interrupt
 * handler.  On these machines the symbol "impl_setintreg_on" will be
 * undefined but we don't want to cause error messages when we load.
 */
	.weak	impl_setintreg_on
	.type	impl_setintreg_on, #function

/*
 * %l2 must be saved and restored within the Interrupt Service Routine.
 * It is ok to save it in a global because the ISR is non-reentrant.
 */
	dataseg
	.align	4
save_temp2:.word	0


	textseg
	.proc	4

	.global	audio_79C30_asmintr
audio_79C30_asmintr:

	/*
	 * Go grab our spin mutex (uses %l5-%l7)
	 */
	sethi	%hi(audio_79C30_hilev), %l6
	sethi	%hi(asm_mutex_spin_enter), %l7
	jmpl	%l7 + %lo(asm_mutex_spin_enter), %l7
	or	%l6, %lo(audio_79C30_hilev), %l6

	set	save_temp2,Temp1;
	st	Temp2,[Temp1]			/* save one extra register */

	/*
	 * Get the address of the amd_unit_t array.
	 * XXX - assume unit 0 for now.
	 */
	sethi	%hi(amd_units),Unitp
	ld	[Unitp+%lo(amd_units)],Unitp

	ld	[Unitp+AUD_DEV_CHIP],Chip	/* get the chip address */
	ldsb	[Chip+AUD_CHIP_IR],Temp1	/* clear interrupt condition */
	ldsb	[Unitp+AUD_REC_ACTIVE],Temp1
	tst	Temp1				/* is record active? */
	be	play
	mov	Zero,Int_Active			/* clear the status flag */

	ld	[Unitp+AUD_REC_CMDPTR],Cmdp
	tst	Cmdp				/* NULL command list? */
	be	recnull
	mov	1,Temp1

recskip:
	lduh	[Cmdp+AUD_CMD_SKIP],Temp2	/* check skip & done flags */
	tst	Temp2
	be,a	recactive
	or	Int_Active,Active,Int_Active	/* record is active */

	stb	Temp1,[Cmdp+AUD_CMD_DONE]	/* mark this command done */
	ld	[Cmdp+AUD_CMD_NEXT],Cmdp	/* update to next command */
	st	Cmdp,[Unitp+AUD_REC_CMDPTR]
	tst	Cmdp				/* end of list? */
	bne	recskip				/* if not, check skip flag */
	or	Int_Active,Interrupt,Int_Active	/* if so, data overflow */

recnull:
	stb	Temp1,[Unitp+AUD_REC_ERROR]	/* set error flag */
	b	recintr
	stb	Zero,[Unitp+AUD_REC_ACTIVE]	/* disable recording */

	or	Int_Active,Active,Int_Active	/* record is active */
recactive:
	ld	[Unitp+AUD_REC_SAMPLES],Temp1
	inc	Temp1				/* increment sample count */
	st	Temp1,[Unitp+AUD_REC_SAMPLES]
	ldub	[Chip+AUD_CHIP_BBRB],Temp2	/* get data from device */
	ld	[Cmdp+AUD_CMD_DATA],Temp1
	stb	Temp2,[Temp1]			/* store data in buffer */
	inc	Temp1				/* increment buffer pointer */
	ld	[Cmdp+AUD_CMD_ENDDATA],Temp2
	cmp	Temp1,Temp2			/* buffer complete? */
	bcs	play				/* branch if not */
	st	Temp1,[Cmdp+AUD_CMD_DATA]	/* update buffer pointer */

	mov	1,Temp1
	stb	Temp1,[Cmdp+AUD_CMD_DONE]	/* mark command done */
	ld	[Cmdp+AUD_CMD_NEXT],Cmdp	/* update to next command */
	st	Cmdp,[Unitp+AUD_REC_CMDPTR]

recintr:
	or	Int_Active,Interrupt,Int_Active	/* schedule level 4 intr */

play:
	ldsb	[Unitp+AUD_PLAY_ACTIVE],Temp1
	tst	Temp1				/* is play active? */
	be,a	checkactive
	andcc	Int_Active,Active,Zero		/* if not, any activity? */

	ld	[Unitp+AUD_PLAY_CMDPTR],Cmdp
	tst	Cmdp				/* NULL command list? */
	be	playnull
	mov	1,Temp1

playskip:
	lduh	[Cmdp+AUD_CMD_SKIP],Temp2	/* check skip & done flags */
	tst	Temp2
	be,a	playactive
	or	Int_Active,Active,Int_Active	/* play is active */

	stb	Temp1,[Cmdp+AUD_CMD_DONE]	/* mark this command done */
	ld	[Cmdp+AUD_CMD_NEXT],Cmdp	/* update to next command */
	st	Cmdp,[Unitp+AUD_PLAY_CMDPTR]
	tst	Cmdp				/* end of list? */
	bne	playskip			/* if not, check skip flag */
	or	Int_Active,Interrupt,Int_Active	/* if so, data underflow */

playnull:
	stb	Temp1,[Unitp+AUD_PLAY_ERROR]	/* set error flag */
	b	playintr
	stb	Zero,[Unitp+AUD_PLAY_ACTIVE]	/* disable play */

	or	Int_Active,Active,Int_Active	/* play is active */
playactive:
	ld	[Unitp+AUD_PLAY_SAMPLES],Temp1
	inc	Temp1				/* increment sample count */
	st	Temp1,[Unitp+AUD_PLAY_SAMPLES]
	ld	[Cmdp+AUD_CMD_DATA],Temp1
	ldsb	[Temp1],Temp2			/* get sample from buffer */
	stb	Temp2,[Chip+AUD_CHIP_BBRB]	/* write it to the device */
	inc	Temp1				/* increment buffer pointer */
	ld	[Cmdp+AUD_CMD_ENDDATA],Temp2
	cmp	Temp1,Temp2			/* buffer complete? */
	bcs	done				/* branch if not */
	st	Temp1,[Cmdp+AUD_CMD_DATA]	/* update buffer pointer */

	mov	1,Temp1
	stb	Temp1,[Cmdp+AUD_CMD_DONE]	/* mark command done */
	ld	[Cmdp+AUD_CMD_NEXT],Cmdp	/* update to next command */
	st	Cmdp,[Unitp+AUD_PLAY_CMDPTR]
playintr:
	or	Int_Active,Interrupt,Int_Active	/* schedule level 4 intr */

done:
	andcc	Int_Active,Active,Zero		/* anything active? */
checkactive:
	bne	checkintr			/* branch if so */
	sethi	%hi(save_temp2),Temp2		/* prepare to restore reg */

	mov	AUD_CHIP_INIT_REG,Temp1
	stb	Temp1,[Chip+AUD_CHIP_CR]	/* set init register */
	mov	AUD_CHIP_DISABLE,Temp1
	stb	Temp1,[Chip+AUD_CHIP_DR]	/* disable device interrupts */

checkintr:
	andcc	Int_Active,Interrupt,Zero	/* schedule level 4 intr? */
	be	reti				/* branch if not */
	or	Temp2,%lo(save_temp2),Temp2	/* get saved reg address */

	/*
	 * Schedule a soft interrupt.
	 * Since we're running in a trap window with traps disabled,
	 * it's okay to modify the interrupt register.
	 * We can reuse %l5-%l7 now.
	 */
	sethi	%hi(audio_79C30_softintr_cookie), %l6
	sethi	%hi(impl_setintreg_on), %l7
	jmpl	%l7 + %lo(impl_setintreg_on), %l7
	ld	[%l6 + %lo(audio_79C30_softintr_cookie)], %l6

reti:
	ld	[Temp2],Temp2			/* restore saved register */

#ifdef NOTDEF
	/* XXX - anything like this in SVr4?? */
	sethi	%hi(cnt+V_INTR),Temp1		/* increment interrupt count */
	ld	[Temp1+%lo(cnt+V_INTR)],Cmdp
	inc	Cmdp
	st	Cmdp,[Temp1+%lo(cnt+V_INTR)]
#endif

	/*
	 * Release the spin mutex now (uses %l5-%l7)
	 */
	sethi	%hi(audio_79C30_hilev), %l6
	sethi	%hi(asm_mutex_spin_exit), %l7
	jmpl	%l7 + %lo(asm_mutex_spin_exit), %l7
	or	%l6, %lo(audio_79C30_hilev), %l6

	RETI					/* return from interrupt */

#endif	/* !AUDIO_C_TRAP */

#endif	/* !lint*/
