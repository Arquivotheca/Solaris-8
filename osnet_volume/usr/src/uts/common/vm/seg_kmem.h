/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VM_SEG_KMEM_H
#define	_VM_SEG_KMEM_H

#pragma ident	"@(#)seg_kmem.h	1.4	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/vmem.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>

/*
 * VM - Kernel Segment Driver
 */

#if defined(_KERNEL)

extern char *kernelheap;	/* start of primary kernel heap */
extern char *ekernelheap;	/* end of primary kernel heap */
extern struct seg kvseg;	/* primary kernel heap segment */
extern vmem_t *heap_arena;
extern struct seg kvseg32;	/* 32-bit kernel heap segment */
extern vmem_t *heap32_arena;	/* 32-bit kernel heap vmem */
extern struct ctx *kctx;	/* kernel context */
extern struct as kas;		/* kernel address space */
extern struct vnode kvp;	/* vnode for all segkmem pages */

extern int segkmem_create(struct seg *);
extern page_t *segkmem_page_create(void *, size_t, int, void *);
extern void *segkmem_xalloc(vmem_t *, void *, size_t, int, uint_t,
	page_t *(*page_create_func)(void *, size_t, int, void *), void *);
extern void *segkmem_alloc(vmem_t *, size_t, int);
extern void segkmem_free(vmem_t *, void *, size_t);

extern void *boot_alloc(void *, size_t, uint_t);
extern void kernelheap_init(void *, void *, void *);
extern void segkmem_gc(void);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_KMEM_H */
