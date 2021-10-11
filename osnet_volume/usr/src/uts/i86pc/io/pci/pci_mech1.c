/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_mech1.c	1.5	96/08/19 SMI"

/*
 * PCI Mechanism 1 low-level routines
 */

#include <sys/types.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/sunddi.h>
#include "pci_nexus_internal.h"

/*
 * Per PCI 2.1 section 3.7.4.1 and PCI-PCI Bridge Architecture 1.0 section
 * 5.3.1.2:  dev=31 func=7 reg=0 means a special cycle.  We don't want to
 * trigger that by accident, so we pretend that dev 31, func 7 doesn't
 * exist.  If we ever want special cycle support, we'll add explicit
 * special cycle support.
 */

uchar_t
pci_mech1_getb(int bus, int device, int function, int reg)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xff);
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	return (inb(PCI_CONFDATA | (reg & 0x3)));
}

ushort_t
pci_mech1_getw(int bus, int device, int function, int reg)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xffff);
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	return (inw(PCI_CONFDATA | (reg & 0x2)));
}

ulong_t
pci_mech1_getl(int bus, int device, int function, int reg)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xffffffffUL);
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	return (inl(PCI_CONFDATA));
}

void
pci_mech1_putb(int bus, int device, int function, int reg, unchar val)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	outb(PCI_CONFDATA | (reg & 0x3), val);
}

void
pci_mech1_putw(int bus, int device, int function, int reg, ushort val)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	outw(PCI_CONFDATA | (reg & 0x2), val);
}

void
pci_mech1_putl(int bus, int device, int function, int reg, ulong val)
{
	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}

	outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
	outl(PCI_CONFDATA, val);
}
