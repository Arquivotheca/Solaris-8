/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBC_SPARCV9_INC_PIC_H
#define	_LIBC_SPARCV9_INC_PIC_H

#ident	"@(#)PIC.h	1.8	97/05/02 SMI"

#ifdef PIC 

/*
 * While it's tempting to imagine we could use 'rd %pc' here,
 * in fact it's a rather slow operation that consumes many
 * cycles, so we use the usual side-effect of 'call' instead.
 */
#define PIC_SETUP(r)						\
	mov	%o7, %g1;					\
9:	call	8f;						\
	sethi	%hi(_GLOBAL_OFFSET_TABLE_ - (9b - .)), %r;	\
8:	or	%r, %lo(_GLOBAL_OFFSET_TABLE_ - (9b - .)), %r;	\
	add	%r, %o7, %r;					\
	mov	%g1, %o7
#else 
#define PIC_SETUP()
#endif 

#endif	/* _LIBC_SPARCV9_INC_PIC_H */
