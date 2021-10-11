/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * pciutil.h -- public definitions for pci utility routines
 */

#ifndef	_PCIUTIL_H
#define	_PCIUTIL_H

#ident "@(#)pciutil.h   1.6   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
u_char pci_getb(u_char bus, u_char devfunc, u_int offset);
u_int pci_getw(u_char bus, u_char devfunc, u_int offset);
u_long pci_getl(u_char bus, u_char devfunc, u_int offset);
void pci_putl(u_char bus, u_char devfunc, u_int offset, u_long value);
int pci_getl2(u_char bus, u_char devfunc, u_int off, u_long *val);
int pci_putl2(u_char bus, u_char devfunc, u_int off, u_long val);
int pci_present(u_char *mechanism, u_char *nbus, u_short *vers);
int pci_present2(u_char *mechanism, u_char *nbus, u_short *vers);

#ifdef	__cplusplus
}
#endif

#endif	/* _PCIUTIL_H */
