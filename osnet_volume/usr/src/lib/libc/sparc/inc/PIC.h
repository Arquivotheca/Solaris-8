/*
 *       Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef	_LIBC_SPARC_INC_PIC_H
#define	_LIBC_SPARC_INC_PIC_H

#ident	"@(#)PIC.h	1.18	92/09/05 SMI"

#ifdef PIC 
#define PIC_SETUP(r) \
	or	%g0,%o7,%g1; \
9: \
	call	8f; \
	sethi	%hi(_GLOBAL_OFFSET_TABLE_ - (9b-.)), %r; \
8: \
	or	%r, %lo(_GLOBAL_OFFSET_TABLE_ - (9b-.)),%r; \
	add	%r, %o7, %r; \
	or	%g0,%g1,%o7
#else 
#define PIC_SETUP()
#endif 

#endif	/* _LIBC_SPARC_INC_PIC_H */
