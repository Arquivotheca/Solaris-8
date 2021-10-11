/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)sync_instruction_memory.c	1.2	95/04/28 SMI"

#include <sys/asm_linkage.h>

/*
 * void sync_instruction_memory(caddr_t addr, int len)
 *
 * Make the memory at {addr, addr+len} valid for instruction execution.
 * This is a no-op on x86 because it has a uniform cache.
 */

void
sync_instruction_memory(caddr_t addr, int len)
{
}
