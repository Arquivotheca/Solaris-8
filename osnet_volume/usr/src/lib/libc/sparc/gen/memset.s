/*
 * Copyright (c) 1987-1995,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)memset.s	1.10	98/12/18 SMI"	/* SunOS 4.1 1.4	*/

	.file	"memset.s"

/*
 * char *memset(sp, c, n)
 *
 * Set an array of n chars starting at sp to the character c.
 * Return sp.
 *
 * Fast assembler language version of the following C-program for memset
 * which represents the `standard' for the C-library.
 *
 *	void *
 *	memset(void *sp1, int c, size_t n)
 *	{
 *	    if (n != 0) {
 *		char *sp = sp1;
 *		do {
 *		    *sp++ = (char)c;
 *		} while (--n != 0);
 *	    }
 *	    return (sp1);
 *	}
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memset,function)

#include "synonyms.h"

	ENTRY(memset)
	mov	%o0, %o5		! copy sp before using it
	cmp	%o2, 7			! if small counts, just write bytes
	blu	.wrchar
	.empty				! following lable is ok in delay slot

.walign:btst	3, %o5			! if bigger, align to 4 bytes
	bz	.wrword
	andn	%o2, 3, %o3		! create word sized count in %o3
	dec	%o2			! decrement count
	stb	%o1, [%o5]		! clear a byte
	b	.walign
	inc	%o5			! next byte

.wrword:and	%o1, 0xff, %o1		! generate a word filled with c
	sll	%o1, 8, %o4
	or 	%o1, %o4, %o1
	sll	%o1, 16, %o4
	or	%o1, %o4, %o1
1:	st	%o1, [%o5]		! word writing loop
	subcc	%o3, 4, %o3
	bnz	1b
	inc	4, %o5

	and	%o2, 3, %o2		! leftover count, if any
.wrchar:deccc	%o2			! byte clearing loop
	inc	%o5
	bgeu,a	.wrchar
	stb	%o1, [%o5 + -1]		! we've already incremented the address

	retl
	nop

	SET_SIZE(memset)
