/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBC_SPARCV9_INC_PIC_H
#define	_LIBC_SPARCV9_INC_PIC_H

#pragma ident	"@(#)PIC.h	1.2	97/06/17 SMI"

#ifdef PIC
/*
 * updated to be in sync with lib/libc/sparcv9/inc version
 */
#define PIC_SETUP(r)                                            \
	mov	%o7, %g1;                                       \
9:	call	8f;                                             \
	sethi	%hi(_GLOBAL_OFFSET_TABLE_ - (9b - .)), %r;      \
8:	or	%r, %lo(_GLOBAL_OFFSET_TABLE_ - (9b - .)), %r;  \
	add	%r, %o7, %r;                                    \
	mov	%g1, %o7
#else
#define	PIC_SETUP()
#endif

#endif	/* _LIBC_SPARCV9_INC_PIC_H */
