/*
 * Copyright (c) 1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_PLAT_H
#define	_SYS_PROM_PLAT_H

#pragma ident	"@(#)prom_plat.h	1.6	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains external platform-specific promif interface definitions
 * for SMCC's 32 bit SPARC sun4m platform architecture. Note that the prom
 * interfaces for sun4m and sun4d platforms are identical.
 */

/*
 * "reg"-format for 32 bit cell-size, 2-cell physical addresses,
 * with a single 'size' cell:
 */

struct prom_reg {
	unsigned int hi, lo, size;
};

/*
 * resource allocation group: OBP only. (mapping functions are platform
 * dependent because they use physical address arguments.)
 */
extern	caddr_t		prom_map(caddr_t virthint, uint_t space,
			    uint_t phys, uint_t size);

/*
 * I/O Group: (Possibly only on 2.0 proms).
 */

extern	int		prom_input_source(void);
extern	int		prom_output_sink(void);

/*
 * Administrative group: SMCC platform specific.
 *
 * This stuff assumes SMCC idprom hardware.
 */

extern	int		prom_getidprom(caddr_t addr, int size);
extern	int		prom_getmacaddr(ihandle_t hd, caddr_t ea);

/*
 * CPU Control:
 */

extern	int		prom_startcpu(dnode_t node, struct prom_reg *context,
			    int whichcontext, caddr_t pc);
extern	int		prom_stopcpu(dnode_t node);
extern	int		prom_idlecpu(dnode_t node);
extern	int		prom_resumecpu(dnode_t node);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_PLAT_H */
