/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989-1999 by Sun Microsystems, Inc.		*/

.ident	"@(#)cuexit.s	1.8	99/10/07 SMI"	/* SVr4.0 1.7	*/

/* C library -- exit						*/
/* void exit (int status);					*/

	.file	"cuexit.s"

#include "SYS.h"

	.global	__exit_frame_monitor
	.type	__exit_frame_monitor,#object
	.size	__exit_frame_monitor,4

	ENTRY(exit)
	save	%sp, -SA(MINFRAME), %sp
#ifdef PIC
	PIC_SETUP(o5)
	ld	[%o5 + __exit_frame_monitor], %g1
	st	%fp, [%g1]
#else
	sethi	%hi(__exit_frame_monitor), %g1
	st	%fp, [%g1 + %lo(__exit_frame_monitor)]
#endif
	call	_exithandle
	nop
	restore
	SYSTRAP(exit)

	SET_SIZE(exit)
