/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)sync_instruction_memory.s	1.3	97/08/20 SMI"

	.file	"sync_instruction_memory.s"

#include <sys/asm_linkage.h>

/*
 * void sync_instruction_memory(caddr_t addr, int len)
 *
 * Make the memory at {addr, addr+len} valid for instruction execution.
 */

#ifdef lint
#define	nop
void
sync_instruction_memory(caddr_t addr, size_t len)
{
	caddr_t end = addr + len;
	caddr_t start = addr & ~7;
	for (; start < end; start += 8)
		flush(start);
	nop; nop; nop; nop; nop;
	return;
}
#else
	ENTRY(sync_instruction_memory)
	add	%o0, %o1, %o2
	andn	%o0, 7, %o0

	cmp	%o0, %o2
	bgeu,pn	%xcc, 2f
	nop
	flush	%o0
1:
	add	%o0, 8, %o0
	cmp	%o0, %o2
	blu,a,pt %xcc, 1b
	flush	%o0
2:
	retl
	clr	%o0
	SET_SIZE(sync_instruction_memory)

	ENTRY(nop)
	retl
	nop
	SET_SIZE(nop)
#endif
