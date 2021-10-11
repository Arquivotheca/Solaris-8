/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_ISA_H
#define	_SYS_PROM_ISA_H

#pragma ident	"@(#)prom_isa.h	1.9	97/06/30 SMI"

#include <sys/obpdefs.h>

/*
 * This file contains external ISA-specific promif interface definitions.
 * There may be none.  This file is included by reference in <sys/promif.h>
 *
 * This version of the file contains definitions for both a 32-bit client
 * program or a 64-bit client program calling the 64-bit cell-sized SPARC
 * v9 firmware client interface handler.
 *
 * On SPARC v9 machines, a 32-bit client program must provide
 * a function to manage the conversion of the 32-bit stack to
 * a 64-bit stack, before calling the firmware's client interface
 * handler.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_NO_LONGLONG)
#error "This header won't work with _NO_LONGLONG"
#endif

typedef	unsigned long long cell_t;

#define	p1275_ptr2cell(p)	((cell_t)((uintptr_t)((void *)(p))))
#define	p1275_int2cell(i)	((cell_t)((int)(i)))
#define	p1275_uint2cell(u)	((cell_t)((unsigned int)(u)))
#define	p1275_size2cell(u)	((cell_t)((size_t)(u)))
#define	p1275_phandle2cell(ph)	((cell_t)((unsigned int)((phandle_t)(ph))))
#define	p1275_dnode2cell(d)	((cell_t)((unsigned int)((dnode_t)(d))))
#define	p1275_ihandle2cell(ih)	((cell_t)((unsigned int)((ihandle_t)(ih))))
#define	p1275_ull2cell_high(ll)	(0LL)
#define	p1275_ull2cell_low(ll)	((cell_t)(ll))
#define	p1275_uintptr2cell(i)	((cell_t)((uintptr_t)(i)))

#define	p1275_cell2ptr(p)	((void *)((cell_t)(p)))
#define	p1275_cell2int(i)	((int)((cell_t)(i)))
#define	p1275_cell2uint(u)	((unsigned int)((cell_t)(u)))
#define	p1275_cell2size(u)	((size_t)((cell_t)(u)))
#define	p1275_cell2phandle(ph)	((phandle_t)((cell_t)(ph)))
#define	p1275_cell2dnode(d)	((dnode_t)((cell_t)(d)))
#define	p1275_cell2ihandle(ih)	((ihandle_t)((cell_t)(ih)))
#define	p1275_cells2ull(h, l)	((unsigned long long)(cell_t)(l))
#define	p1275_cell2uintptr(i)	((uintptr_t)((cell_t)(i)))

/*
 * Define default cif handlers:  This port uses SPARC V8 32 bit semantics
 * on the calling side and the prom side.
 */
#define	p1275_cif_init			p1275_sparc_cif_init
#define	p1275_cif_handler		p1275_sparc_cif_handler

extern void	*p1275_sparc_cif_init(void *);
extern int	p1275_cif_handler(void *);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_ISA_H */
