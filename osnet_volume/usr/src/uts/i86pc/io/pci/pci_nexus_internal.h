/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Interfaces internal to the i86pc PCI nexus driver.
 */

#ifndef	_SYS_PCI_AUTOCONFIG_H
#define	_SYS_PCI_AUTOCONFIG_H

#pragma ident	"@(#)pci_nexus_internal.h	1.5	96/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Routines to support particular PCI chipsets
 */

/*
 * Generic Mechanism 1 routines
 */
extern uchar_t pci_mech1_getb(int bus, int dev, int func, int reg);
extern ushort_t pci_mech1_getw(int bus, int dev, int func, int reg);
extern ulong_t pci_mech1_getl(int bus, int dev, int func, int reg);
extern void pci_mech1_putb(int bus, int dev, int func, int reg, uchar_t val);
extern void pci_mech1_putw(int bus, int dev, int func, int reg, ushort_t val);
extern void pci_mech1_putl(int bus, int dev, int func, int reg, ulong_t val);

/*
 * Generic Mechanism 2 routines
 */
extern uchar_t pci_mech2_getb(int bus, int dev, int func, int reg);
extern ushort_t pci_mech2_getw(int bus, int dev, int func, int reg);
extern ulong_t pci_mech2_getl(int bus, int dev, int func, int reg);
extern void pci_mech2_putb(int bus, int dev, int func, int reg, uchar_t val);
extern void pci_mech2_putw(int bus, int dev, int func, int reg, ushort_t val);
extern void pci_mech2_putl(int bus, int dev, int func, int reg, ulong_t val);

/*
 * Intel Neptune routines.  Neptune is Mech 1, except that BIOSes
 * often initialize it into Mech 2 so we dynamically switch it to
 * Mech 1.  The chipset's buggy, so we have to do it carefully.
 */
extern boolean_t pci_check_neptune(void);
extern uchar_t pci_neptune_getb(int bus, int dev, int func, int reg);
extern ushort_t pci_neptune_getw(int bus, int dev, int func, int reg);
extern ulong_t pci_neptune_getl(int bus, int dev, int func, int reg);
extern void pci_neptune_putb(int bus, int dev, int func, int reg, uchar_t val);
extern void pci_neptune_putw(int bus, int dev, int func, int reg, ushort_t val);
extern void pci_neptune_putl(int bus, int dev, int func, int reg, ulong_t val);

/*
 * Intel Orion routines.  Orion is Mech 1, except that there's a bug
 * in the peer bridge that requires that it be tweaked specially
 * around accesses to config space.
 */
extern boolean_t pci_is_broken_orion(void);
extern uchar_t pci_orion_getb(int bus, int dev, int func, int reg);
extern ushort_t pci_orion_getw(int bus, int dev, int func, int reg);
extern ulong_t pci_orion_getl(int bus, int dev, int func, int reg);
extern void pci_orion_putb(int bus, int dev, int func, int reg, uchar_t val);
extern void pci_orion_putw(int bus, int dev, int func, int reg, ushort_t val);
extern void pci_orion_putl(int bus, int dev, int func, int reg, ulong_t val);

/*
 * Generic PCI constants.  Probably these should be in pci.h.
 */
#define	PCI_MAX_BUSSES		256
#define	PCI_MAX_DEVS		32
#define	PCI_MAX_FUNCS		8

/*
 * PCI access mechanism constants.  Probably these should be in pci_impl.h.
 */
#define	PCI_MECH2_CONFIG_ENABLE	0x10	/* any nonzero high nibble works */

#define	PCI_MECH1_SPEC_CYCLE_DEV	0x1f	/* dev to request spec cyc */
#define	PCI_MECH1_SPEC_CYCLE_FUNC	0x07	/* func to request spec cyc */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_AUTOCONFIG_H */
