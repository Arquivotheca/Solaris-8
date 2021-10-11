/*
 *       Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef	_LIBC_SPARC_INC_PIC_H
#define	_LIBC_SPARC_INC_PIC_H

#ident	"@(#)PIC.h	1.3	92/07/14 SMI"

/* Note: avoid label name conflict with calling macro */

#ifdef PIC 
#define PIC_SETUP(r) \
	or	%g0,%o7,%g1; \
8: \
	call	9f; \
	nop; \
9: \
	sethi	%hi(_GLOBAL_OFFSET_TABLE_ - (8b-.)), %r; \
	or	%r, %lo(_GLOBAL_OFFSET_TABLE_ - (8b-.)),%r; \
	add	%r, %o7, %r; \
	or	%g0,%g1,%o7
#else 
#define PIC_SETUP()
#endif 

#endif	/* _LIBC_SPARC_INC_PIC_H */
