/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pci1275.c -- routines to handle pci 1275 binding
 */

#ident "@(#)pci1275.c   1.95   99/08/12 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "types.h"
#include <dos.h>

#include "boards.h"
#include "boot.h"
#include "bop.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "gettext.h"
#include "pci.h"
#include "pci1275.h"
#include "pciutil.h"
#include "tty.h"
#include "ur.h"
#include "mps_table.h"
#include "mpspec.h"
#include "hrt.h"

/*
 * Module data
 */
static int pci_bus_nodes_created = 0;

/*
 * Module local prototypes
 */
void add_node_pci1275(Board *bp, char *node_name);
void add_regs_pci1275(u_char bus, u_char devfunc, int regs);
void add_other_props_pci1275(Board *bp, char *node_name);
void build_bus_nodes_pci1275(void);
void create_bus_node_pci(int i);
void add_bus_props_pci1275(u_char bus);
void add_bus_hp_props_pci1275(u_char bus);
void add_ppb_props_pci1275(Board *bp, u_char bus, u_char secbus,
    u_char devfunc);
static void add_available_pci1275(Board *ppbp, u_short res, u_char bus,
    u_long phys_hi, u_long phys_lo, u_long len, char **buf);
void link_range_pci1275(struct range *head, struct range *r);
char *get_node_name_pci1275(Board *bp);
int pci_find_bus_range(int bus);
struct range **create_range_list(int size);

struct range **pci_bus_io_avail = NULL;
struct range **pci_bus_mem_avail = NULL;
struct range **pci_bus_pmem_avail = NULL;

void
reset_pci1275()
{
	pci_bus_nodes_created = 0;
}

void
init_pci1275()
{
	reset_pci1275();
}

/*
 * We must receive the parent bus node (bus bridge) before any
 * devices under it (on that bus) This implies the board records must
 * be keep in the order in which they are read from config space.
 */
int
build_node_pci1275(Board *bp)
{
	char *node_name;

	/*
	 * The only pci with a vendor, device of 0,0 is the node containing
	 * the pci programming ports. There is no 1275 interface for this.
	 * Note, the io ports are still set in the used_resources property.
	 */
	if (bp->devid == 0) {
		return (0); /* Success */
	}

	if (!pci_bus_nodes_created) {
		build_bus_nodes_pci1275();
	}

	node_name = get_node_name_pci1275(bp);

	add_node_pci1275(bp, node_name);
	add_other_props_pci1275(bp, node_name);

	return (0); /* Success */
}

/*
 * Make the pci bus nodes.
 */
void
build_bus_nodes_pci1275(void)
{
	u_char i;

	for (i = 0; i <= Max_bus_pci; i++) {
		create_bus_node_pci(i);
	}
	pci_bus_nodes_created = 1;
}

void
create_bus_node_pci(int i)
{
	/*
	 * Only make the root nodes eg /pci@0,0 and /pci@1,0
	 * The pci-pci bridge nodes are made later in the order
	 * in which they are found.
	 */
	if (strrchr(Bus_path_pci[i], '/') == Bus_path_pci[i]) {
		char *at, *comma;
		char pbuf[120];

		at = strrchr(Bus_path_pci[i], '@');
		ASSERT(at != NULL);
		*at = 0; /* temporarily terminate at the '@' */
		comma = strrchr(at + 1, ',');
		ASSERT(comma != NULL);
		*comma = 0; /* temporarily terminate at the ',' */
		(void) sprintf(pbuf, "mknod %s 0x%s,0,0\n", Bus_path_pci[i],
			at + 1);
		out_bop(pbuf);
		*at = '@'; /* restore string */
		*comma = ','; /* restore comma */
		(void) sprintf(pbuf, "setprop \\$at %s\n", at + 1);
		out_bop(pbuf);
		add_bus_props_pci1275((u_char) i);
		add_bus_hp_props_pci1275((u_char) i);
	}
}

/*
 * Add the following pci bus properties
 *	device_type
 *	#address-cells
 *	#size-cells
 *	slot-names
 */
void
add_bus_props_pci1275(u_char bus)
{
	char pbuf[120];
	int len;

	/*
	 * According to the 1.5 P1275 device_type uses a "_"
	 * and not a "-" as I expected
	 */
	out_bop("setprop device_type pci\n");
	out_bop("setbinprop \\#address-cells 3\n");
	out_bop("setbinprop \\#size-cells 2\n");
	len = pci_slot_names_prop(bus, pbuf, 120);
	if (len > 0) {
		u_long *lp;
		char c, buf2[16];

		len /= 4;
		out_bop("setbinprop slot-names ");
		for (lp = (u_long *)pbuf; len; lp++, len--) {
			if (len == 1) {
				c = '\n';
			} else {
				c = ',';
			}
			(void) sprintf(buf2, "0x%lx%c", *lp, c);
			out_bop(buf2);
		}
	}
	out_bop("#\n");
}

/*
 * Add the following pci bus properties for pci hot-plug:-
 *	available
 *	bus-range
 *
 * These properties are made for the root pci bus nodes only.
 */
void
add_bus_hp_props_pci1275(u_char bus)
{
	char pbuf[80];
	char *abuf = NULL;
	int type, restype, max_bus;
	struct range *res, *rp;
	u_long len;
	u_long phys_hi;
	u_long phys_lo;

	for (type = 0, res = NULL; type <= PREFETCH_TYPE; type++) {
		if ((hrt_find_bus_res((int)bus, type, &res) == 0) &&
			(mps_find_bus_res((int)bus, type, &res) == 0))
			continue;
		if (type == IO_TYPE) {
			if (pci_bus_io_avail == NULL) {
				pci_bus_io_avail =
					create_range_list(Max_bus_pci + 1);
				if (pci_bus_io_avail == NULL)
					return;
			}
			pci_bus_io_avail[bus] = res;
			phys_hi = PCI_RELOCAT_B | PCI_ADDR_IO |
					PCI_BCNF_IO_BASE_LOW;
			restype = RESF_Port;
		} else if (type == MEM_TYPE) {
			if (pci_bus_mem_avail == NULL) {
				pci_bus_mem_avail =
					create_range_list(Max_bus_pci + 1);
				if (pci_bus_mem_avail == NULL)
					return;
			}
			pci_bus_mem_avail[bus] = res;
			phys_hi = PCI_RELOCAT_B | PCI_ADDR_MEM32 |
					PCI_BCNF_MEM_BASE;
			restype = RESF_Mem;
		} else if (type == PREFETCH_TYPE) {
			if (pci_bus_pmem_avail == NULL) {
				pci_bus_pmem_avail =
					create_range_list(Max_bus_pci + 1);
				if (pci_bus_pmem_avail == NULL)
					return;
			}
			pci_bus_pmem_avail[bus] = res;
			phys_hi = PCI_PREFETCH_B | PCI_RELOCAT_B |
				PCI_ADDR_MEM32 | PCI_BCNF_PF_BASE_LOW;
			restype = RESF_Mem;
		}

		rp = res;
		while (rp) {
			phys_lo = rp->addr;
			len = rp->len;
			add_available_pci1275(NULL, restype, bus,
				phys_hi, phys_lo, len, &abuf);
			rp = rp->next;
		}
		res = NULL;
	}
	if (abuf) {
		out_bop(abuf); /* available */
		out_bop("\n");
		free(abuf);
		if ((max_bus = hrt_find_bus_range(bus)) == -1)
			max_bus = pci_find_bus_range(bus);
		sprintf(pbuf, "setbinprop bus-range 0x%x,0x%x\n", bus, max_bus);
		out_bop(pbuf);
	}
}

void
add_node_pci1275(Board *bp, char *node_name)
{
	u_char bus;
	u_char devfunc;
	char pbuf[120];

	bus = bp->pci_busno;
	devfunc = bp->pci_devfunc;

	(void) sprintf(pbuf, "mknod %s/%s", Bus_path_pci[bus], node_name);

	out_bop(pbuf);

	add_regs_pci1275(bus, devfunc, 1);

	/*
	 * Create address property (ie "dev,func" in /pci/dvr@dev,func/...)
	 * Note we set only dev if func is zero.
	 * P1275 requires hex digits for the dev.
	 */
	if (devfunc & 7) {
		(void) sprintf(pbuf, "setprop \\$at %x,%d\n",
			devfunc >> 3, devfunc & 7);
	} else {
		(void) sprintf(pbuf, "setprop \\$at %x\n", devfunc >> 3);
	}
	out_bop(pbuf);
}

void
add_regs_pci1275(u_char bus, u_char devfunc, int regs)
{
	u_char baseclass;
	u_char subclass;
	u_char progclass;
	u_char header;
	u_long value = 0;
	u_long len;
	u_long devloc;
	u_long base, base_hi;
	u_int offset, end;
	int max_basereg;
	int j;
	u_long phys_hi;
	u_long phys_mid;
	u_long phys_lo;
	char ch;
	char pbuf[120];
	u_int bar_sz;
	u_char pciide;

	devloc = (u_long) bus << 16 | (u_long) devfunc << 8;

	if (regs) {
		/*
		 * setup the initial configuration entry
		 */
		(void) sprintf(pbuf, " 0x%lx,0,0,0,0", devloc);
		out_bop(pbuf);
		ch = ',';
	} else {
		ch = ' '; /* first array entry */
	}

	baseclass = pci_getb(bus, devfunc, PCI_CONF_BASCLASS);
	subclass = pci_getb(bus, devfunc, PCI_CONF_SUBCLASS);
	progclass = pci_getb(bus, devfunc, PCI_CONF_PROGCLASS);
	header = pci_getb(bus, devfunc, PCI_CONF_HEADER) & PCI_HEADER_TYPE_M;

	switch (header) {
	case PCI_HEADER_ZERO:
		max_basereg = PCI_BASE_NUM;
		break;
	case PCI_HEADER_PPB:
		max_basereg = PCI_BCNF_BASE_NUM;
		break;
	case PCI_HEADER_CARDBUS:
		max_basereg = PCI_CBUS_BASE_NUM;
		break;
	default:
		max_basereg = 0;
		break;
	}

	if ((baseclass == PCI_CLASS_MASS) && (subclass == PCI_MASS_IDE)) {
		pciide = TRUE;
	} else {
		pciide = FALSE;
	}

	/*
	 * Create the register property by saving the current
	 * value of the base register.  Disable memory/io, then
	 * write 0xffffffff to the base register.  Read the
	 * value back to determine the required size of the
	 * address space.  Restore the base register
	 * contents.
	 */
	end = PCI_CONF_BASE0 + max_basereg * sizeof (u_long);
	for (j = 0, offset = PCI_CONF_BASE0; offset < end;
		j++, offset += bar_sz) {

		base = pci_getl(bus, devfunc, offset);

		/* determine the size of the address space */
		pci_putl(bus, devfunc, offset, 0xffffffff);
		value = pci_getl(bus, devfunc, offset);

		/* restore the original value in the base */
		pci_putl(bus, devfunc, offset, base);

		/* construct phys hi,med.lo, size hi, lo */
		if (pciide || (base & PCI_BASE_SPACE_IO)) { /* i/o space */
			int	hard_decode = FALSE;

			bar_sz = PCI_BAR_SZ_32;
			value &= PCI_BASE_IO_ADDR_M;
			len = ((value ^ (value-1)) + 1) >> 1;

			if (pciide) {
				hard_decode = PciIdeAdjustBAR(progclass, j,
					&base, &len);
			} else if (value == 0) {
				/* skip base regs with size of 0 */
				continue;
			}

			if (regs && !hard_decode) {
				phys_hi = (PCI_ADDR_IO | devloc) + offset;
				phys_lo = 0;
			} else {
				phys_hi = (PCI_RELOCAT_B | PCI_ADDR_IO |
				    devloc) + offset;
				phys_lo = base & PCI_BASE_IO_ADDR_M;
			}
			(void) sprintf(pbuf, "%c0x%lx,0,0x%lx,0,0x%lx",
			    ch, phys_hi, phys_lo, len);
			out_bop(pbuf);
			ch = ',';
		} else {
			/* memory space */

			if ((base & PCI_BASE_TYPE_M) == PCI_BASE_TYPE_ALL) {
				bar_sz = PCI_BAR_SZ_64;
				base_hi = pci_getl(bus, devfunc, offset + 4);
				phys_hi = PCI_ADDR_MEM64;
			} else {
				bar_sz = PCI_BAR_SZ_32;
				base_hi = 0;
				phys_hi = PCI_ADDR_MEM32;
			}

			/* skip base regs with size of 0 */
			value &= PCI_BASE_M_ADDR_M;

			if (value == 0) {
				continue;
			}

			phys_hi |= (devloc | offset);
			if (regs) {
				phys_lo = phys_mid = 0;
			} else {
				phys_hi |= PCI_RELOCAT_B;
				phys_mid = base_hi;
				phys_lo = base & PCI_BASE_M_ADDR_M;
			}
			if (base & PCI_BASE_PREF_M) {
				phys_hi |= PCI_PREFETCH_B;
			}

			len = ((value ^ (value-1)) + 1) >> 1;
			(void) sprintf(pbuf, "%c0x%lx,0x%lx,0x%lx,0,0x%lx",
			    ch, phys_hi, phys_mid, phys_lo, len);
			out_bop(pbuf);
			ch = ',';
		}
	}
	switch (header) {
	case PCI_HEADER_ZERO:
		offset = PCI_CONF_ROM;
		break;
	case PCI_HEADER_PPB:
		offset = PCI_BCNF_ROM;
		break;
	default: /* including PCI_HEADER_CARDBUS */
		goto done;
	}

	/*
	 * Add the expansion rom memory space
	 * Determine the size of the ROM base reg; don't write reserved bits
	 */
	base = pci_getl(bus, devfunc, offset);
	pci_putl(bus, devfunc, offset, PCI_BASE_ROM_ADDR_M);
	value = pci_getl(bus, devfunc, offset);
	pci_putl(bus, devfunc, offset, base);
	value &= PCI_BASE_ROM_ADDR_M;

	if (value != 0) {
		if (regs) {
			phys_hi = (PCI_ADDR_MEM32 | devloc) + offset;
			phys_lo = 0;
		} else {
			phys_hi = (PCI_RELOCAT_B |
			    PCI_ADDR_MEM32 | devloc) + offset;
			phys_lo = base & PCI_BASE_ROM_ADDR_M;
		}

		len = ((value ^ (value-1)) + 1) >> 1;
		(void) sprintf(pbuf, "%c0x%lx,0,0x%lx,0,0x%lx",
		    ch, phys_hi, phys_lo, len);
		out_bop(pbuf);
		ch = ',';
	}

	/* add the three hard-decode, aliased address spaces for VGA */
	if ((baseclass == PCI_CLASS_DISPLAY && subclass == PCI_DISPLAY_VGA) ||
	    (baseclass == PCI_CLASS_NONE && subclass == PCI_NONE_VGA)) {

		/* VGA hard decode 0x3b0-0x3bb */
		(void) sprintf(pbuf, "%c0x%lx,0,0x3b0,0,0xc",
		    ch, (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc));
		out_bop(pbuf);

		/* VGA hard decode 0x3c0-0x3df */
		(void) sprintf(pbuf, ",0x%lx,0,0x3c0,0,0x20",
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc));
		out_bop(pbuf);

		(void) sprintf(pbuf, ",0x%lx,0,0xa0000,0,0x20000",
		    (PCI_RELOCAT_B | PCI_ADDR_MEM32 | devloc));
		out_bop(pbuf);
	}

	/* add the hard-decode, aliased address spaces for 8514 */
	if ((baseclass == PCI_CLASS_DISPLAY) &&
		(subclass == PCI_DISPLAY_VGA) &&
		(progclass & PCI_DISPLAY_IF_8514)) {

		/* hard decode 0x2e8 */
		(void) sprintf(pbuf, ",0x%lx,0,0x2e8,0,0x1",
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc));
		out_bop(pbuf);

		/* hard decode 0x2ea-0x2ef */
		(void) sprintf(pbuf, ",0x%lx,0,0x2ea,0,0x6",
		    (PCI_RELOCAT_B | PCI_ALIAS_B | PCI_ADDR_IO | devloc));
		out_bop(pbuf);
	}
done:
	out_bop("\n");
}

void
add_other_props_pci1275(Board *bp, char *node_name)
{
	u_char	bus;
	u_char	devfunc;
	u_char	revid;
	u_char	ipin;
	u_long	classcode;
	u_char	header;
	u_char	mingrant;
	u_char	maxlatency;
	u_char	baseclass;
	u_char	subclass;
	u_int	status;
	u_int	devsel_speed;
	u_int	vendorid;
	u_int	deviceid;
	char	pbuf[120];
	u_long	*lp;
	int	len;
	char	buf2[12];
	char	c;

	bus = bp->pci_busno;
	devfunc = bp->pci_devfunc;

	out_bop("setbinprop assigned-addresses");
	add_regs_pci1275(bus, devfunc, 0);

	vendorid = bp->pci_venid;
	(void) sprintf(pbuf, "setbinprop vendor-id 0x%x\n", vendorid);
	out_bop(pbuf);

	deviceid = bp->pci_devid;
	(void) sprintf(pbuf, "setbinprop device-id 0x%x\n", deviceid);
	out_bop(pbuf);

	revid = pci_getb(bus, devfunc, PCI_CONF_REVID);
	(void) sprintf(pbuf, "setbinprop revision-id 0x%x\n", revid);
	out_bop(pbuf);

	classcode = pci_getl(bus, devfunc, PCI_CONF_REVID);
	classcode >>= 8;
	(void) sprintf(pbuf, "setbinprop class-code 0x%lx\n", classcode);
	out_bop(pbuf);

	/*
	 * Note PCI 1275 requires that the PCI interrupt pin be stored
	 * in the interrupts property. The Interrupt line (IRQ for x86)
	 * is not used in this interface.
	 * 1275 requires that only if the pin is non zero
	 * should a interrupts property be craeted
	 */
	if (ipin = pci_getb(bus, devfunc, PCI_CONF_IPIN)) {
		(void) sprintf(pbuf, "setbinprop interrupts %d\n", ipin);
		out_bop(pbuf);
	}

	/*
	 * Only if header type is type 0
	 * do we set min-grant, max-latency, subsystem-id
	 * and subsystem-vendor-id properties.
	 */
	header = pci_getb(bus, devfunc, PCI_CONF_HEADER) & PCI_HEADER_TYPE_M;
	if (header == PCI_HEADER_ZERO) {
		mingrant = pci_getb(bus, devfunc, PCI_CONF_MIN_G);
		(void) sprintf(pbuf, "setbinprop min-grant 0x%x\n", mingrant);
		out_bop(pbuf);

		maxlatency = pci_getb(bus, devfunc, PCI_CONF_MAX_L);
		(void) sprintf(pbuf, "setbinprop max-latency 0x%x\n",
			maxlatency);
		out_bop(pbuf);

		if (bp->pci_subvenid) {
			(void) sprintf(pbuf, "setbinprop subsystem-id 0x%x\n",
				bp->pci_subdevid);
			out_bop(pbuf);
			(void) sprintf(pbuf,
				"setbinprop subsystem-vendor-id 0x%x\n",
				bp->pci_subvenid);
			out_bop(pbuf);
		}
	}

	status = pci_getw(bus, devfunc, PCI_CONF_STAT);

	devsel_speed = (status & PCI_STAT_DEVSELT) >> 9;
	(void) sprintf(pbuf, "setbinprop devsel-speed 0x%x\n", devsel_speed);
	out_bop(pbuf);

	if (status & PCI_STAT_FBBC) {
		out_bop("setprop fast-back-to-back\n");
	}

	if (status & PCI_STAT_66MHZ) {
		out_bop("setprop 66mhz-capable\n");
	}

	if (status & PCI_STAT_UDF) {
		out_bop("setprop udf-supported\n");
	}

	/*
	 * The power-consumption on the PRSNT1# and PRSNT2 connector
	 * pins is another P1275 PCI bus binding spec rev 1.4 requirement.
	 * The consumption can not be programmatically determined.
	 * Setting the power-consumption property to 1,1 essentially
	 * declares the standby and full-on consumption as unspecified
	 * - see the spec Page 14.
	 */
	out_bop("setbinprop power-consumption 1,1\n");

	/*
	 * Set additional properties for pci-pci bus bridges
	 */
	baseclass = classcode >> 16;
	subclass = (classcode >> 8) & 0xff;
	if ((baseclass == PCI_CLASS_BRIDGE) && (subclass == PCI_BRIDGE_PCI)) {
		u_char secbus;

		secbus = pci_getb(bus, devfunc, PCI_BCNF_SECBUS);
		(void) sprintf(pbuf, "setbinprop bus-range 0x%x,0x%x\n",
		    secbus, pci_getb(bus, devfunc, PCI_BCNF_SUBBUS));
		out_bop(pbuf);
		add_bus_props_pci1275(secbus);

		add_ppb_props_pci1275(bp, bus, secbus, devfunc);

	} else if ((baseclass == PCI_CLASS_MASS) &&
			(subclass == PCI_MASS_IDE)) {
		/*
		 * Create properties specified by P1275 Working Group
		 * Proposal #414 Version 1
		 */
		out_bop("setprop device_type pci-ide\n");
		out_bop("setbinprop \\#address-cells 1\n");
		out_bop("setbinprop \\#size-cells 0\n");
		/* make child nodes for the actual controller instances */
		out_bop("mknod ide 0\n");
		out_bop("setprop \\$at 0\n");
		out_bop("cd ..\n");
		out_bop("mknod ide 1\n");
		out_bop("setprop \\$at 1\n");
		out_bop("cd ..\n");
	}

	/*
	 * add a helpful description
	 */
	(void) sprintf(pbuf, "setprop model \"%s\"\n",
		bp_to_desc_pci1275(bp, 1));
	out_bop(pbuf);

	/*
	 * Set the compatible property which is currently:-
	 * node_name NULL <if exist, subid name> NULL <vendor id name>
	 * NULL <class code>
	 *
	 * We have to use setbinprop because of the embedded NULL
	 */
	(void) sprintf(pbuf, "%s", node_name);
	len = strlen(pbuf) + 1;
	if (bp->pci_subvenid) {
		(void) sprintf(pbuf + len, "pci%x,%x", bp->pci_subvenid,
		    bp->pci_subdevid);
		len += strlen(pbuf + len) + 1;	/* include NULL separator */
	}
	(void) sprintf(pbuf + len, "pci%x,%x", vendorid, deviceid);
	len += strlen(pbuf + len) + 1; /* include NULL separator */
	(void) sprintf(pbuf + len, "%s%06lx", "pciclass,", classcode);
	len += strlen("pciclass,123456");
	pbuf[len+1] = 0; /* pad the end with zeros */
	pbuf[len+2] = 0;
	pbuf[len+3] = 0;
	len = (len + 4) >> 2; /* number of longs */

	out_bop("setbinprop compatible ");
	for (lp = (u_long *) pbuf; len; lp++, len--) {
		if (len == 1) {
			c = '\n';
		} else {
			c = ',';
		}
		(void) sprintf(buf2, "0x%lx%c", *lp, c);
		out_bop(buf2);
	}
	/*
	 * setbinprop slot n
	 * - for devices where the slot is known
	 */
	if (bp->slot != 0xff) {
		(void) sprintf(pbuf, "setbinprop slot %d\n", bp->slot);
		out_bop(pbuf);
	}

}

static const struct {
	u_char base_class;
	u_char sub_class;
	u_char prog_class;
	char *desc;
} class_pci[] = {
	0, 0, 0,	"Unspecified class",
	0, 1, 0,	"VGA compatible controller",

	1, 0, 0,	"SCSI bus controller",
	1, 1, 0x80,	"IDE controller", /* Special case - see below */
	1, 2, 0,	"Floppy controller",
	1, 3, 0,	"IPI bus controller",
	1, 4, 0,	"RAID controller",
	1, 0x80, 0,	"Mass storage controller",

	2, 0, 0,	"Ethernet controller",
	2, 1, 0,	"Token ring controller",
	2, 2, 0,	"FDDI controller",
	2, 3, 0,	"ATM controller",
	2, 0x80, 0,	"Network controller",

	3, 0, 0,	"VGA compatible controller",
	3, 0, 1,	"8514-compatible display controller",
	3, 1, 0,	"XGA video controller",
	3, 0x80, 0,	"Video controller",

	4, 0, 0,	"Video device",
	4, 1, 0,	"Audio device",

	5, 0, 0,	"Ram",
	5, 1, 0,	"Flash memory",
	5, 0x80, 0,	"Memory controller",

	6, 0, 0,	"Host bridge",
	6, 1, 0,	"ISA bridge",
	6, 2, 0,	"EISA bridge",
	6, 3, 0,	"MCA bridge",
	6, 4, 0,	"PCI-PCI bridge",
	6, 5, 0,	"PCMCIA bridge",
	6, 6, 0,	"NuBus bridge",
	6, 7, 0,	"CardBus bridge",
	6, 0x80, 0,	"Bridge device",

	7, 0, 0,	"Serial controller",
	7, 0, 1,	"16450-compatible serial controller",
	7, 0, 2,	"16550-compatible serial controller",
	7, 1, 0,	"Parallel port",
	7, 1, 1,	"Bidirectional parallel port",
	7, 1, 2,	"ECP 1.X compliant parallel port",
	7, 0x80, 0,	"Communication device",

	8, 0, 0,	"8259 PIC",
	8, 0, 1,	"ISA PIC",
	8, 0, 2,	"EISA PIC",
	8, 1, 0,	"8237 DMA controller",
	8, 1, 1,	"ISA DMA controller",
	8, 1, 2,	"EISA DMA controller",
	8, 2, 0,	"8254 system timer",
	8, 2, 1,	"ISA system timer",
	8, 2, 2,	"EISA system timers",
	8, 3, 0,	"Real time clock",
	8, 3, 1,	"ISA Real time clock",
	8, 0x80, 0,	"System peripheral",

	9, 0, 0,	"Keyboard controller",
	9, 1, 0,	"Digitizer (pen)",
	9, 2, 0,	"Mouse controller",
	9, 0x80, 0,	"Input controller",

	10, 0, 0,	"Generic Docking station",
	10, 0x80, 0,	"Docking station",

	11, 0, 0,	"386",
	11, 1, 0,	"486",
	11, 2, 0,	"Pentium",
	11, 0x10, 0,	"Alpha",
	11, 0x20, 0,	"Power-PC",
	11, 0x40, 0,	"Co-processor",

	12, 0, 0,	"FireWire (IEEE 1394)",
	12, 1, 0,	"ACCESS.bus",
	12, 2, 0,	"SSA",
	12, 3, 0,	"Universal Serial Bus",
	12, 4, 0,	"Fibre Channel",
};

int class_pci_items = sizeof (class_pci) / sizeof (class_pci[0]);

char *
get_desc_pci(u_char baseclass, u_char subclass, u_char progclass)
{
	const char *desc;
	int i;

	if ((baseclass == PCI_CLASS_MASS) && (subclass == PCI_MASS_IDE)) {
		desc = gettext("IDE controller");
	} else {
		for (desc = 0, i = 0; i < class_pci_items; i++) {
			if ((baseclass == class_pci[i].base_class) &&
			    (subclass == class_pci[i].sub_class) &&
			    (progclass == class_pci[i].prog_class)) {
				desc = gettext(class_pci[i].desc);
				break;
			}
		}
		if (i == class_pci_items) {
			desc = gettext("Unknown class of pci/pnpbios device");
		}
	}
	return ((char *)desc);
}

char *
bp_to_desc_pci1275(Board *bp, int verbose)
{
	static char *buf = NULL;
	static int blen = 0;
	int len;
	u_char bus;
	u_char devfunc;
	u_char baseclass;
	u_char subclass;
	u_char progclass;
	const char *desc;
	devtrans *dtp;

	/*
	 * First check for the special case of the pci configuration ports.
	 */
	if (bp->devid == 0) {
		return ("PCI: bus configuration ports");
	}

	bus = bp->pci_busno;
	devfunc = bp->pci_devfunc;

	/*
	 * Next check for an entry in the master file
	 */

	if ((dtp = bp->dbentryp) != 0) {
		len = strlen("PCI: XXXXXXXX,YYYYYYYY - ") +
			strlen(dtp->dname) + 8;
		/* error check buf overflow */
		if (len > blen) {
			blen = len;
			buf = realloc(buf, blen);
			if (!buf)
				MemFailure();
		}

		if (verbose) {
			if ((bp->devid == MAGIC_COMPAQ_8X5_DEVID ||
				bp->devid == MAGIC_NCR_PQS_DEVID) &&
				dtp->unix_driver)
				(void) sprintf(buf, "PCI: %s - %s",
					dtp->unix_driver, dtp->dname);
			else
				(void) sprintf(buf, "PCI: %lx,%lx - %s",
					bp->devid >> 16, bp->devid & 0xFFFF,
					dtp->dname);
		} else
			(void) sprintf(buf, "PCI: %s", dtp->dname);
		return (buf);
	}

	/*
	 * Next create a generic string from the pci class code
	 */
	baseclass = pci_getb(bus, devfunc, PCI_CONF_BASCLASS);
	subclass = pci_getb(bus, devfunc, PCI_CONF_SUBCLASS);
	progclass = pci_getb(bus, devfunc, PCI_CONF_PROGCLASS);

	desc = get_desc_pci(baseclass, subclass, progclass);

	len = strlen("PCI: XXXXXXXX,YYYYYYYY - class: ") + strlen(dtp->dname) +
		strlen(desc) + 8;
	/* error check buf overflow */
	if (len > blen) {
		blen = len;
		buf = realloc(buf, blen);
		if (!buf)
			MemFailure();
	}

	if (verbose) {
		if ((bp->devid == MAGIC_COMPAQ_8X5_DEVID ||
			bp->devid == MAGIC_NCR_PQS_DEVID) && dtp->unix_driver)
			(void) sprintf(buf, "PCI: %s - %s",
				dtp->unix_driver, dtp->dname);
		else
			(void) sprintf(buf, "PCI: %lx,%lx - class: %s",
			    bp->devid >> 16, bp->devid & 0xFFFF, desc);
	} else
		(void) sprintf(buf, "PCI: %s", desc);
	return (buf);
}

void
bp_to_name_pci1275(Board *bp, char *buf)
{
	(void) sprintf(buf, "pci%lx,%lx", bp->devid >> 16,
	    bp->devid & 0xFFFF);
}

static struct bdev_info pci_bootpath_bdev;
static char pci_bootpath_slice;

int
parse_bootpath_pci1275(char *path, char **rest)
{
	char *s = path;
	int i;

	pci_bootpath_bdev.pci_valid = 0;

	/*
	 * Compare initial bootpath against pci bus path strings.
	 * If we compare against the array from the end to the
	 * beginning and stop on the first match then we will get the
	 * deepest or correct pci path.
	 */
	for (i = Max_bus_pci; i >= 0; i--) {
		if (!(strncmp(s, Bus_path_pci[i],
		    strlen(Bus_path_pci[i])))) {
			pci_bootpath_bdev.pci_bus = (u_char)i;
			s += strlen(Bus_path_pci[i]);
			break;
		}
	}
	if (i < 0) {
		debug(D_ERR, "pci bus not found\n");
		return (1);
	}

	/*
	 * Skip name (/pcivvvv,dddd)
	 * =========================
	 */
	if (*s++ != '/') {
		return (1);
	}
	while ((*s != '@') && (*s != 0)) {
		s++;
	}

	/*
	 * Get pci device and if present the function
	 * ==========================================
	 */
	if (*s++ == 0) {
		return (1);
	}
	pci_bootpath_bdev.pci_dev = (u_char) strtol(s, &s, 16);
	if (*s != ',') {
		pci_bootpath_bdev.pci_func = 0;
	} else {
		pci_bootpath_bdev.pci_func = (u_char) strtol(s + 1, &s, 16);
	}

	parse_target(s, &pci_bootpath_bdev.MDBdev.scsi.targ,
	    &pci_bootpath_bdev.MDBdev.scsi.lun, &pci_bootpath_slice);

	*rest = s;

	pci_bootpath_bdev.pci_valid = 1;
	return (0); /* success */
}

/*
 * Check if this is the default (or bootpath) device.
 * Further check the target/lun if we are dealing with a disk.
 */
int
is_bdp_bootpath_pci1275(bef_dev *bdp)
{
	char path[120];

	/*
	 * For net befs we must use the device record
	 */
	if (strcmp("NET ", bdp->dev_type) == 0) {
		return (is_bp_bootpath_pci1275(bdp->bp, NULL));
	}

	get_path_from_bdp_pci1275(bdp, path, 0);
	return (strncmp(path, bootpath_in, strlen(path)) == 0);
}

int
is_bp_bootpath_pci1275(Board *bp, struct bdev_info *dip)
{
	if (pci_bootpath_bdev.pci_valid &&
	    (pci_bootpath_bdev.pci_bus == bp->pci_busno) &&
	    (pci_bootpath_bdev.pci_dev == (bp->pci_devfunc >> FUNF_DEVSHFT)) &&
	    (pci_bootpath_bdev.pci_func == (bp->pci_devfunc & FUNF_FCTNNUM))) {
		/* yes, it's the path; save info in dip if present */
		if (dip != NULL) {
			dip->pci_valid = 1;
			dip->pci_bus = pci_bootpath_bdev.pci_bus;
			dip->pci_dev = pci_bootpath_bdev.pci_dev;
			dip->pci_func = pci_bootpath_bdev.pci_func;
		}

		return (TRUE);
	}
	return (FALSE);
}

/* ARGSUSED */
void
get_path_from_bdp_pci1275(bef_dev *bdp, char *path, int compat)
{
	Board *bp = bdp->bp;
	u_char func;
	char *ubp = bdp->info->user_bootpath;
	u_char version = bdp->info->version;

	/*
	 * Check for absolute user boot path
	 */
	if ((version >= 1) && (ubp[0] == '/')) {
		strcpy(path, ubp);
		return;
	}

	/* ignore pci_valid because we're not using F8 info for device */

	(void) sprintf(path, "%s", Bus_path_pci[bp->pci_busno]);

	/*
	 * Add pci leaf device name only if no user bootpath; otherwise,
	 * assume user bootpath includes the leaf device name.  This
	 * makes it the same as ufsboot in 2.5.1 (see bug 4018145).
	 */

	if (ubp[0] == '\0') {
		(void) sprintf(path + strlen(path), "/%s@%x",
		    get_node_name_pci1275(bp),
		    bp->pci_devfunc >> FUNF_DEVSHFT);
		func = bp->pci_devfunc & FUNF_FCTNNUM;
		if (func) {
			(void) sprintf(path + strlen(path), ",%x", func);
		}
	}

	/*
	 * Add the relative portion of the pathname, if any.
	 * This is primarily for SCSI HBA's which support multiple
	 * channels (e.g., the MLX adapter). This "middle" portion
	 * of the pathname may designate the channel on the adapter.
	 */
	if ((version >= 1) && ubp[0]) {
		(void) sprintf(path + strlen(path), "/%s", ubp);
	}
	if (bdp->info->dev_type == MDB_SCSI_HBA) {
		(void) sprintf(path + strlen(path), "/%s@%x,%x",
		    determine_scsi_target_driver(bdp),
		    bdp->info->MDBdev.scsi.targ,
		    bdp->info->MDBdev.scsi.lun);
		if (is_bp_bootpath_pci1275(bp, NULL)) {
			sprintf(path + strlen(path), ":%c", pci_bootpath_slice);
		} else {
			strcat(path, ":a");
		}
	}
}

/*
 * Set the ranges and available properties
 *
 * Format of ranges property is tuples of:-
 *	child-phys parent-phys size (ie 3 x 32bits, 3 x 32bits, 2 x 32bits)
 * for any non zero io, memory and prefetchable memory
 * in the ppb config space.
 * Note according to the pci1275 spec (1.7) Section 12.
 * the phys hi of the child-phys should only contain
 * the n, p and ss bits.
 *
 * Format of the available property is the same as the assigned-addresses
 */
void
add_ppb_props_pci1275(Board *bp, u_char bus, u_char secbus, u_char devfunc)
{
	char rbuf[300];
	char *abuf = NULL;
	u_long len;
	char ch;
	u_long phys_hi;
	u_long phys_lo;
	u_long devloc;

	if ((bp->pci_ppb_io.start == 0) &&
	    (bp->pci_ppb_mem.start == 0) &&
	    (bp->pci_ppb_pmem.start == 0)) {
		return;
	}

	sprintf(rbuf, "setbinprop ranges");
	devloc = (u_long) bus << 16 | (u_long) devfunc << 8;
	ch = ' ';

	if (bp->pci_ppb_io.start) {
		phys_lo = bp->pci_ppb_io.start;
		phys_hi = PCI_RELOCAT_B | PCI_ADDR_IO;
		len = bp->pci_ppb_io.len;
		(void) sprintf(rbuf + strlen(rbuf),
		    " 0x%lx,0,0x%lx,0x%lx,0,0x%lx,0,0x%lx",
		    phys_hi, phys_lo, phys_hi, phys_lo,
		    bp->pci_ppb_io.len);
		phys_hi |= devloc | PCI_BCNF_IO_BASE_LOW;
		add_available_pci1275(bp, RESF_Port, secbus,
		    phys_hi, phys_lo, len, &abuf);
		ch = ',';
	}
	if (bp->pci_ppb_mem.start) {
		len = bp->pci_ppb_mem.len;
		phys_hi = PCI_RELOCAT_B | PCI_ADDR_MEM32;
		phys_lo = bp->pci_ppb_mem.start;
		(void) sprintf(rbuf + strlen(rbuf),
		    "%c0x%lx,0,0x%lx,0x%lx,0,0x%lx,0,0x%lx",
		    ch, phys_hi, phys_lo, phys_hi, phys_lo, len);
		phys_hi |= devloc | PCI_BCNF_MEM_BASE;
		add_available_pci1275(bp, RESF_Mem, secbus,
		    phys_hi, phys_lo, len, &abuf);
		ch = ',';
	}

	if (bp->pci_ppb_pmem.start) {
		len = bp->pci_ppb_pmem.len;
		phys_hi = PCI_PREFETCH_B | PCI_RELOCAT_B | PCI_ADDR_MEM32;
		phys_lo = bp->pci_ppb_pmem.start;
		(void) sprintf(rbuf + strlen(rbuf),
		    "%c0x%lx,0,0x%lx,0x%lx,0,0x%lx,0,0x%lx",
		    ch, phys_hi, phys_lo, phys_hi, phys_lo, len);
		phys_hi |= devloc | PCI_BCNF_PF_BASE_LOW;
		add_available_pci1275(bp, RESF_Mem, secbus,
		    phys_hi, phys_lo, len, &abuf);
	}
	out_bop(rbuf); /* ranges */
	out_bop("\n");
	if (abuf) {
		out_bop(abuf); /* available */
		out_bop("\n");
		free(abuf);
	}
}

static void
add_available_pci1275(Board *ppbp, u_short res, u_char bus,
    u_long phys_hi, u_long phys_lo, u_long len, char **buf)
{
	struct range *head, *r;
	Board *bp;
	Resource *rp;
	u_int rc, j;
	u_long addr;

	/*
	 * Set up initial used map
	 */
	head = (struct range *)calloc(1, sizeof (*head));
	head->len = phys_lo;
	r = (struct range *)calloc(1, sizeof (*r));
	head->next = r;
	if (phys_lo + len != 0)	/* check wrap around case */
		r->addr = phys_lo + len;
	else
		r->addr = phys_lo + (len - 1);

	/*
	 * Reserve ISA-compatible ranges if ISA Enable bit set.
	 */
	if ((res == RESF_Port) && (ppbp != NULL) &&
	    (ppbp->pci_ppb_bcntrl & PCI_BCNF_BCNTRL_ISA_ENABLE)) {
		for (addr = phys_lo; addr < phys_lo + len; addr += 0x400) {
			r = (struct range *)calloc(1, sizeof (*r));
			r->addr = addr + 0x100;
			r->len = 0x300;
			link_range_pci1275(head, r);
		}
	}

	/*
	 * Search device records for the list of used resources in
	 * the specified range, and create a used resource map (linked
	 * list of ranges).
	 */
	for (bp = Head_board; bp; bp = bp->link) {
		if (bp->bustype == RES_BUS_PCI) {
			if (bp->pci_busno != bus)
				continue;

			if (((phys_hi & 0xff) == PCI_BCNF_IO_BASE_LOW) &&
			    (bp->pci_ppb_io.start) &&
			    (bp->pci_ppb_io.start >= phys_lo) &&
			    ((bp->pci_ppb_io.start + bp->pci_ppb_io.len) <=
			    (phys_lo + len))) {
				r = (struct range *)calloc(1, sizeof (*r));
				r->addr = bp->pci_ppb_io.start;
				r->len = bp->pci_ppb_io.len;
				link_range_pci1275(head, r);
				continue;
			}
			if (((phys_hi & 0xff) == PCI_BCNF_MEM_BASE) &&
			    (bp->pci_ppb_mem.start) &&
			    (bp->pci_ppb_mem.start >= phys_lo) &&
			    ((bp->pci_ppb_mem.start + bp->pci_ppb_mem.len) <=
			    (phys_lo + len))) {
				r = (struct range *)calloc(1, sizeof (*r));
				r->addr = bp->pci_ppb_mem.start;
				r->len = bp->pci_ppb_mem.len;
				link_range_pci1275(head, r);
				continue;
			}
			if (((phys_hi & 0xff) == PCI_BCNF_PF_BASE_LOW) &&
			    (bp->pci_ppb_pmem.start) &&
			    (bp->pci_ppb_pmem.start >= phys_lo) &&
			    ((bp->pci_ppb_pmem.start + bp->pci_ppb_pmem.len) <=
			    (phys_lo + len))) {
				r = (struct range *)calloc(1, sizeof (*r));
				r->addr = bp->pci_ppb_pmem.start;
				r->len = bp->pci_ppb_pmem.len;
				link_range_pci1275(head, r);
				continue;
			}
		}

		rc = resource_count(bp);
		for (j = 0, rp = resource_list(bp); j < rc; j++, rp++) {
			if ((RTYPE(rp) == res) &&
			    (rp->base >= phys_lo) &&
			    (rp->base + (rp->length - 1) <=
			    (phys_lo + (len - 1)))) {
				r = (struct range *)calloc(1, sizeof (*r));
				r->addr = rp->base;
				/* check wrap around case */
				if (rp->base + rp->length != 0)
					r->len = rp->length;
				else
					r->len = rp->length - 1;
				link_range_pci1275(head, r);
			}
		}
	}

	/* clear out config address bits in phys_hi */
	phys_hi &= ~PCI_CONF_ADDR_MASK;

	/*
	 * Now create the available property from the
	 * list of used resources
	 */
	for (r = head; r->next; r = r->next) {
		if ((r->addr + r->len) < r->next->addr) {
			u_long gap_addr = r->addr + r->len;
			u_long gap_len = r->next->addr - (r->addr + r->len);

			if (*buf == NULL) {
				*buf = malloc(59); /* sizeof expanded string */
				(void) sprintf(*buf,
				    "setbinprop available"
				    " 0x%lx,0,0x%lx,0,0x%lx",
				    phys_hi, gap_addr, gap_len);
			} else {
				*buf = realloc(*buf, strlen(*buf) + 38);
				(void) sprintf(*buf + strlen(*buf),
				    ",0x%lx,0,0x%lx,0,0x%lx",
				    phys_hi, gap_addr, gap_len);
			}
		}
	}

	/*
	 * Finally release the malloced resource ranges
	 */
	do {
		r = head;
		head = head->next;
		free(r);
	} while (head);
}

/*
 * Link r into in order into the chain
 */
void
link_range_pci1275(struct range *head, struct range *r)
{
	struct range *p;

	p = head;
	while (r->addr > p->next->addr) {
		p = p->next;
	}
	r->next = p->next;
	p->next = r;
}

char *
get_node_name_pci1275(Board *bp)
{
	devtrans *dtp = bp->dbentryp;
	char *node_name;

	/*
	 * If we did not find a matching device database entry, or the
	 * database entry has no node name ("unix_driver" is really the
	 * node name), generate the node name using GetDeviceId_devdb.
	 *
	 * If the device has a sub vendor/device ID, but the database
	 * entry we are using was for the primary vendor/device ID,
	 * we want the node name to be derived from the sub vendor/device
	 * ID, except for devices for which we invoke the special Compaq
	 * hack (these devices use the node name from the database entry).
	 * GetDeviceId_devdb uses bp->devid which will be the sub
	 * vendor/device ID in this case.
	 *
	 * For everything else, just use the name from the database entry.
	 */
	if ((dtp == NULL) || (*dtp->unix_driver == 0)) {
		node_name = GetDeviceId_devdb(bp, 0);
	} else if ((bp->devid != MAGIC_COMPAQ_8X5_DEVID) && bp->pci_subvenid &&
	    ((((u_long) bp->pci_venid) << 16) | bp->pci_devid) == dtp->devid) {
		node_name = GetDeviceId_devdb(bp, 0);
	} else {
		node_name = dtp->unix_driver;
	}

	return (node_name);
}

/*
 * find the largest bus number for this PCI bus
 */
int
pci_find_bus_range(int bus)
{
	int i;

	i = bus + 1;
	while (i <=  Max_bus_pci)
		/*
		 * Check to see if there's only one slash, and it's at the
		 * beginning.
		 */
		if (strrchr(Bus_path_pci[i], '/') == Bus_path_pci[i])
			return (i - 1);
		else
			i++;
	return (Bios_max_bus_pci);
}

struct range **
create_range_list(int size)
{
	struct range **new;
	int bytes;

	bytes = size * sizeof (struct range *);
	if ((new = (struct range **)malloc(bytes)) == NULL)
		return (NULL);
	memset(new, 0, bytes);
	return (new);
}
