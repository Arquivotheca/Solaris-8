/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_DEFS_H
#define	_ACPI_DEFS_H

#pragma ident	"@(#)acpi_defs.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * ACPI definitions
 * mostly have to do with compiling in different modes: boot, kernel, user
 */

#ifdef _KERNEL
#ifdef _BOOT
#define	ACPI_BOOT		/* protected mode boot */
#else
#define	ACPI_KERNEL		/* kernel */
#endif
#else
#define	ACPI_USER		/* user-level */
#endif

/* round up memory sizes (min 4 bytes) that can be used as src/dst bufs */
#define	RND_UP4(X) (X == 0 ? 4 : ((X + 3) & 0xFFFFFFFC))


/*
 * protected mode boot
 */
#ifdef ACPI_BOOT

	/* standard includes */
#include <stddef.h>

	/* memory allocator */
#define	KM_SLEEP   0
#define	KM_NOSLEEP 1
#define	kmem_alloc(SIZE, FLAGS) (void *)bkmem_alloc(SIZE)
#define	kmem_free(PTR, SIZE) bkmem_free((char *)PTR, SIZE)
#endif


/*
 * Solaris kernel
 */
#ifdef ACPI_KERNEL

	/* standard includes */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#endif


/*
 * user space debugging setup
 */
#ifdef ACPI_USER

	/* standard includes */
#include <stddef.h>

	/* memory allocator */
#define	KM_SLEEP   0
#define	KM_NOSLEEP 1
#ifdef MEM_PROFILE
extern int heap_max;
extern int heap_size;
#define	MEM_PROBE printf("HEAP current = %d, max = %d\n", heap_size, heap_max)
extern void *my_malloc(size_t size);
extern void my_free(void *ptr, size_t size);
#define	kmem_alloc(SIZE, FLAGS) my_malloc(SIZE)
#define	kmem_free(PTR, SIZE) my_free(PTR, SIZE)
#else
#define	MEM_PROBE
#define	kmem_alloc(SIZE, FLAGS) malloc(SIZE)
#define	kmem_free(PTR, SIZE) free(PTR)
#endif
#endif


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_DEFS_H */
