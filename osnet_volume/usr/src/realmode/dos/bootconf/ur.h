/*
 * Copyright (c) 1999 by Sun Microsystems, Inc..
 * All rights reserved.
 *
 * ur.h -- public definitions for used resource routines
 */

#ifndef	_UR_H
#define	_UR_H

#ident	"@(#)ur.h	1.14	99/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct range {
	struct range *next;
	u_long addr;
	u_long len;
};

/*
 * Public function prototypes
 */
void used_resources_node_ur(void);
void add_range_ur(u_long addr, u_long len, struct range **headp);

#define	NUM_IRQ 32 /* include apic (iss uses irqs > 16) */
#define	NUM_DMA 8 /* dma channels */

extern u_long low_dev_memaddr;

#ifdef	__cplusplus
}
#endif

#endif	/* _UR_H */
