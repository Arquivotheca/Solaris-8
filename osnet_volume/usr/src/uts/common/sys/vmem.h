/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VMEM_H
#define	_SYS_VMEM_H

#pragma ident	"@(#)vmem.h	1.3	99/12/04 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	VM_SLEEP	0x00000000	/* same as KM_SLEEP */
#define	VM_NOSLEEP	0x00000001	/* same as KM_NOSLEEP */
#define	VM_PANIC	0x00000002	/* same as KM_PANIC */
#define	VM_KMFLAGS	0x000000ff	/* flags that must match KM_* flags */

#define	VM_BESTFIT	0x00000100

/*
 * Public segment types
 */
#define	VMEM_ALLOC	0x01
#define	VMEM_FREE	0x02

/*
 * Implementation-private segment types
 */
#define	VMEM_SPAN	0x10

typedef struct vmem vmem_t;

#ifdef _KERNEL

extern void vmem_init(void);
extern void vmem_kstat_init(int);
extern void vmem_mp_init(void);

extern vmem_t *vmem_create(const char *, void *, size_t, size_t,
	void *(*)(vmem_t *, size_t, int), void (*)(vmem_t *, void *, size_t),
	vmem_t *, size_t, int);
extern void vmem_destroy(vmem_t *);
extern void *vmem_alloc(vmem_t *, size_t, int);
extern void *vmem_xalloc(vmem_t *, size_t, size_t, size_t, size_t,
	void *, void *, int);
extern void vmem_free(vmem_t *, void *, size_t);
extern void vmem_xfree(vmem_t *, void *, size_t);
extern void *vmem_add(vmem_t *, void *, size_t, int);
extern int vmem_contains(vmem_t *, void *, size_t);
extern void vmem_walk(vmem_t *, int, void (*)(void *, void *, size_t), void *);
extern size_t vmem_size(vmem_t *, int);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VMEM_H */
