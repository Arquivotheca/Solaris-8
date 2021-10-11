/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_mech2.c	1.4	96/08/19 SMI"

/*
 * PCI Mechanism 2 primitives
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/pci_impl.h>
#include "pci_nexus_internal.h"

/*
 * The "mechanism 2" interface only has 4 bits for device number.  To
 * hide this implementation detail, we return all ones for accesses to
 * devices 16..31.
 */
#define	PCI_MAX_DEVS_2	16

/*
 * the PCI LOCAL BUS SPECIFICATION 2.0 does not say that you need to
 * save the value of the register and restore them.  The Intel chip
 * set documentation indicates that you should.
 */
static uchar_t
pci_mech2_config_enable(uchar_t bus, uchar_t function)
{
	uchar_t	old;

	old = inb(PCI_CSE_PORT);

	outb(PCI_CSE_PORT,
		PCI_MECH2_CONFIG_ENABLE | ((function & PCI_FUNC_MASK) << 1));
	outb(PCI_FORW_PORT, bus);

	return (old);
}

static void
pci_mech2_config_restore(uchar_t oldstatus)
{
	outb(PCI_CSE_PORT, oldstatus);
}

uchar_t
pci_mech2_getb(int bus, int device, int function, int reg)
{
	uchar_t tmp;
	uchar_t val;

	if (device >= PCI_MAX_DEVS_2)
		return (0xff);

	tmp = pci_mech2_config_enable(bus, function);
	val = inb(PCI_CADDR2(device, reg));
	pci_mech2_config_restore(tmp);

	return (val);
}

ushort_t
pci_mech2_getw(int bus, int device, int function, int reg)
{
	uchar_t	tmp;
	ushort_t val;

	if (device >= PCI_MAX_DEVS_2)
		return (0xffff);

	tmp = pci_mech2_config_enable(bus, function);
	val = inw(PCI_CADDR2(device, reg));
	pci_mech2_config_restore(tmp);

	return (val);
}

ulong_t
pci_mech2_getl(int bus, int device, int function, int reg)
{
	uchar_t	tmp;
	ulong_t	val;

	if (device >= PCI_MAX_DEVS_2)
		return (0xffffffffUL);

	tmp = pci_mech2_config_enable(bus, function);
	val = inl(PCI_CADDR2(device, reg));
	pci_mech2_config_restore(tmp);

	return (val);
}

void
pci_mech2_putb(int bus, int device, int function, int reg, uchar_t val)
{
	uchar_t	tmp;

	if (device >= PCI_MAX_DEVS_2)
		return;

	tmp = pci_mech2_config_enable(bus, function);
	outb(PCI_CADDR2(device, reg), val);
	pci_mech2_config_restore(tmp);
}

void
pci_mech2_putw(int bus, int device, int function, int reg, ushort val)
{
	uchar_t	tmp;

	if (device >= PCI_MAX_DEVS_2)
		return;

	tmp = pci_mech2_config_enable(bus, function);
	outw(PCI_CADDR2(device, reg), val);
	pci_mech2_config_restore(tmp);
}

void
pci_mech2_putl(int bus, int device, int function, int reg, ulong val)
{
	uchar_t	tmp;

	if (device >= PCI_MAX_DEVS_2)
		return;

	tmp = pci_mech2_config_enable(bus, function);
	outl(PCI_CADDR2(device, reg), val);
	pci_mech2_config_restore(tmp);
}
