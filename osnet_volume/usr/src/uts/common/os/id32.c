/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)id32.c	1.1	99/09/13 SMI"

#include <sys/vmem.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <vm/seg_kmem.h>

static vmem_t *id32_arena;
static kmem_cache_t *id32_cache;

void
id32_init(void)
{
	id32_arena = vmem_create("id32", NULL, 0, PAGESIZE,
	    segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);

	id32_cache = kmem_cache_create("id32_cache", sizeof (void *), 0,
	    NULL, NULL, NULL, NULL, id32_arena, 0);
}

/*
 * Return a 32-bit identifier for the specified pointer.
 * The ID itself is just a pointer allocated from the 32-bit heap.
 */
uint32_t
id32_alloc(void *ptr, int kmflag)
{
	void **id = kmem_cache_alloc(id32_cache, kmflag);

	if (id == NULL)
		return (0);

	*id = ptr;
	ASSERT64((uintptr_t)id <= UINT32_MAX);
	return ((uint32_t)(uintptr_t)id);
}

/*
 * Free a 32-bit ID.
 */
void
id32_free(uint32_t id)
{
	kmem_cache_free(id32_cache, (void *)(uintptr_t)id);
}

/*
 * Return the pointer described by a 32-bit ID.
 */
void *
id32_lookup(uint32_t id)
{
	return (((void **)(uintptr_t)id)[0]);
}
