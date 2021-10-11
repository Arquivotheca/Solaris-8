/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NCAKMEM_H
#define	_NCAKMEM_H

#pragma ident	"@(#)ncakmem.h	1.1	99/08/06 SMI"

#include <sys/types.h>
#include <vm/page.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

page_t **kmem_phys_alloc(size_t, int);
void kmem_phys_free(page_t **);
void *kmem_phys_mapin(page_t **, int);
void kmem_phys_mapout(page_t **, void *);

extern void vmem_init(void);
extern void vmem_fini(void);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _NCAKMEM_H */
