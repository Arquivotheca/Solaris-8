/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Derived from pseudocode supplied by Intel.
 */

#pragma ident	"@(#)pci_orion.c	1.4	96/08/19 SMI"

/*
 * Workaround for Intel Orion chipset bug
 *
 * It is intended that this code implements exactly the workaround
 * described in the errata.  There is one exception, described below.
 */

#include <sys/types.h>
#include <sys/pci.h>
#include "pci_nexus_internal.h"

#define	PCI_82454_RW_CONTROL	0x54

static int ncDevNo;

boolean_t
pci_is_broken_orion()
{
	int Num82454 = 0;
	boolean_t A2B0Found = B_FALSE;
	boolean_t c82454PostingEnabled = B_FALSE;
	unsigned char PciReg;
	unsigned short VendorID;
	unsigned short DeviceID;
	boolean_t A2B0WorkAroundReqd;

	int BusNo = 0;
	int FunctionNo = 0;
	int DeviceNo;
	unsigned char RevisionID;

	for (DeviceNo = 0; DeviceNo < PCI_MAX_DEVS; DeviceNo++) {
		VendorID = pci_mech1_getw(BusNo, DeviceNo, FunctionNo,
						PCI_CONF_VENID);
		DeviceID = pci_mech1_getw(BusNo, DeviceNo, FunctionNo,
						PCI_CONF_DEVID);
		RevisionID = pci_mech1_getb(BusNo, DeviceNo, FunctionNo,
						PCI_CONF_REVID);
		if (VendorID == 0x8086 && DeviceID == 0x84c4) {
			/* Found 82454 PCI Bridge */
			Num82454++;
			if (RevisionID <= 4) {
				A2B0Found = B_TRUE;
			}
			if (DeviceNo == (0xc8 >> 3)) {
				/*
				 * c82454 Found - determine the status of
				 * inbound posting.
				 */
				PciReg = pci_mech1_getb(BusNo, DeviceNo,
					FunctionNo, PCI_82454_RW_CONTROL);
				if (PciReg & 0x01) {
					c82454PostingEnabled = B_TRUE;
				}
			} else {
				/* nc82454 Found - store device no. */
				ncDevNo = DeviceNo;
			}
		}
	} /* DeviceNo */
	/*
	 * Determine if nc82454 posting is to be enabled
	 * and need of workaround.
	 *
	 * [[ This is a deviation from the pseudocode in the errata.
	 *    The errata has mismatched braces, leading to uncertainty
	 *    as to whether this code is inside the test for 8086/84c4.
	 *    The errata has this code clearly inside the DeviceNo loop.
	 *    This code is obviously pointless until you've at least found
	 *    the second 82454, and there's no need to execute it more
	 *    than once, so I'm moving it outside that loop to execute
	 *    once on completion of the scan. ]]
	 */
	if (Num82454 >= 2 && A2B0Found &&
	    c82454PostingEnabled) {
		A2B0WorkAroundReqd = B_TRUE;
		/* Enable inbound posting on nc82454 */
		PciReg = pci_mech1_getb(0, ncDevNo, 0,
			PCI_82454_RW_CONTROL);
		PciReg |= 0x01;
		pci_mech1_putb(0, ncDevNo, 0,
			PCI_82454_RW_CONTROL, PciReg);
	} else {
		A2B0WorkAroundReqd = B_FALSE;
	}

	return (A2B0WorkAroundReqd);
}

/*
 * When I first read this code in the errata document, I asked "why doesn't
 * the initial read of CFC (possibly) lead to the 'two responses' problem?"
 *
 * After thinking about it for a while, the answer is that we're trying to
 * talk to the nc82454 itself.  The c82454 doesn't have the problem, so it
 * will recognize that this request is *not* for it, and won't respond.
 * The nc82454 will either respond or not, depending on whether it "saw"
 * the CF8 write, and if it responds it might or might not return the
 * right data.  That's all pretty much OK, if we're willing to assume
 * that the only way that 84C48086 will come back is from the vendor ID/
 * device ID registers on the nc82454.  This is probabilistic, of course,
 * because the nc82454 *could* be pointing at a register on some device
 * that just *happened* to have that value, but that seems unlikely.
 */
static void
FuncDisableInboundPostingnc82454()
{
	unsigned long test;
	unsigned char PciReg;

	do {
		test = pci_mech1_getl(0, ncDevNo, 0, PCI_CONF_VENID);
	} while (test != 0x84c48086UL);

	/*
	 * At this point we are guaranteed to be pointing to the nc82454 PCI
	 * bridge Vendor ID register.
	 */
	do {
		/*
		 * Impact of the erratum is that the configuration read will
		 * return the value which was last read.
		 * Hence read register 0x54 until the previous read value
		 * (VendorId/DeviceId) is not read anymore.
		 */
		test = pci_mech1_getl(0, ncDevNo, 0, PCI_82454_RW_CONTROL);
	} while (test == 0x84c48086UL);
	/*
	 * At this point we are guaranteed to be pointing to the PCI
	 * Read/Write Control Register in the nc82454 PCI Bridge.
	 */
	PciReg = pci_mech1_getb(0, ncDevNo, 0, PCI_82454_RW_CONTROL);
	PciReg &= ~0x01;
	pci_mech1_putb(0, ncDevNo, 0, PCI_82454_RW_CONTROL, PciReg);
}

static void
FuncEnableInboundPostingnc82454()
{
	unsigned char PciReg;

	PciReg = pci_mech1_getb(0, ncDevNo, 0, PCI_82454_RW_CONTROL);
	PciReg |= 0x01;
	pci_mech1_putb(0, ncDevNo, 0, PCI_82454_RW_CONTROL, PciReg);
}

uchar_t
pci_orion_getb(int bus, int device, int function, int reg)
{
	uchar_t	val;

	FuncDisableInboundPostingnc82454();

	val = pci_mech1_getb(bus, device, function, reg);

	FuncEnableInboundPostingnc82454();
	return (val);
}

ushort_t
pci_orion_getw(int bus, int device, int function, int reg)
{
	ushort_t val;

	FuncDisableInboundPostingnc82454();

	val = pci_mech1_getw(bus, device, function, reg);

	FuncEnableInboundPostingnc82454();
	return (val);
}

ulong_t
pci_orion_getl(int bus, int device, int function, int reg)
{
	ulong_t	val;

	FuncDisableInboundPostingnc82454();

	val = pci_mech1_getl(bus, device, function, reg);

	FuncEnableInboundPostingnc82454();
	return (val);
}

void
pci_orion_putb(int bus, int device, int function, int reg, unchar val)
{
	FuncDisableInboundPostingnc82454();

	pci_mech1_putb(bus, device, function, reg, val);

	FuncEnableInboundPostingnc82454();
}

void
pci_orion_putw(int bus, int device, int function, int reg, ushort val)
{
	FuncDisableInboundPostingnc82454();

	pci_mech1_putw(bus, device, function, reg, val);

	FuncEnableInboundPostingnc82454();
}

void
pci_orion_putl(int bus, int device, int function, int reg, ulong val)
{
	FuncDisableInboundPostingnc82454();

	pci_mech1_putl(bus, device, function, reg, val);

	FuncEnableInboundPostingnc82454();
}
