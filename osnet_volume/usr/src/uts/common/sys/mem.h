/*
 * Copyright (c) 1987-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MEM_H
#define	_SYS_MEM_H

#pragma ident	"@(#)mem.h	1.18	98/06/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Memory Device Minor Numbers
 */
#define	M_MEM		0	/* /dev/mem - physical main memory */
#define	M_KMEM		1	/* /dev/kmem - virtual kernel memory & I/O */
#define	M_NULL		2	/* /dev/null - EOF & Rathole */
#define	M_ZERO		12	/* /dev/zero - source of private memory */

/*
 * Private ioctl for libkvm: translate virtual address to physical address.
 */
#define	MEM_VTOP	(('M' << 8) | 0x01)

typedef struct mem_vtop {
	struct as	*m_as;
	void		*m_va;
	pfn_t		m_pfn;
} mem_vtop_t;

#ifdef	_KERNEL

extern pfn_t impl_obmem_pfnum(pfn_t);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEM_H */
