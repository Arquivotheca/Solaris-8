/*	Copyright (c) 1993 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


.ident	"@(#)PIC.h	1.4 94/11/03	SMI" /* SVr4.0 1.9 */

#ifndef	_LIBC_SPARC_INC_PIC_H
#define	_LIBC_SPARC_INC_PIC_H

#ifdef PIC
#define	PIC_SETUP(r) \
	or	%g0, %o7, %g1; \
9: \
	call	8f; \
	sethi	%hi(_GLOBAL_OFFSET_TABLE_ - (9b-.)), %r; \
8: \
	or	%r, %lo(_GLOBAL_OFFSET_TABLE_ - (9b-.)), %r; \
	add	%r, %o7, %r; \
	or	%g0, %g1, %o7
#else
#define	PIC_SETUP()
#endif

#endif	/* _LIBC_SPARC_INC_PIC_H */
