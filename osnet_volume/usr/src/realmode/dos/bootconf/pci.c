/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pci.c -- pci bus enumerator
 */

#ident "@(#)pci.c   1.86   99/08/12 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include "types.h"

#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "pci.h"
#include "pciutil.h"
#include "resmgmt.h"
#include "prop.h"

/*
 * Module data
 */
int Pci = 0;
u_char Max_bus_pci; /* 0 based */
u_char Bios_max_bus_pci;
u_char Max_dev_pci;
char **Bus_path_pci;
char *bus0path_pci = "/pci@0,0";

char	pciide_full_on;

/*
 * Module prototypes
 */
void get_configured_dev_resources(u_char bus);
void new_func_pci(u_char bus, u_char devfunc, u_char multi_func);
void save_bus_path_pci(u_char pbus, u_char bus, u_char devfunc);
int already_configured_pci(u_char bus, u_char devfunc);
#ifdef NOTYET
static int configure_bus_pci(u_char bus);
static int configure_func_pci(u_char bus, u_char devfunc);
#endif /* NOTYET */
void create_config_ports_board_pci();
int pci_is_mercury(void);
int pci_is_cyrix(void);
u_int device_is_compaq_8x5(u_int vendorid, u_int deviceid,
    u_char bus, u_char devfunc);
u_int device_is_ncr_pqs(u_int vendorid, u_int deviceid,
    u_char bus, u_char devfunc);
u_int peer_memory_exists(u_char bus);
u_int pci_bridge_parent_exists(u_char pqs_875_bus);
void save_ranges_pci(Board *bp, u_char secbus, u_char bus, u_char devfunc);

#pragma pack(1)
static struct pci_route {
	unsigned char	bus;
	unsigned char	dev;
	unsigned char	inta_link;
	unsigned short	inta_irq_map;
	unsigned char	intb_link;
	unsigned short	intb_irq_map;
	unsigned char	intc_link;
	unsigned short	intc_irq_map;
	unsigned char	intd_link;
	unsigned short	intd_irq_map;
	unsigned char	slot;
	unsigned char	reserved;
} pci_routes[4 * 8];

static struct pci_rbuf {
	short	size;
	struct pci_route *routes;
} pci_rbuf;

static int pci_routesz = sizeof (struct pci_route);

#pragma pack()

/*
 * Find pci slot information in routing tables.
 * If we have the information, the slot that the bus/device resides
 * in is returned, else 0xff is returned.
 */
unsigned char
find_pci_slot(unsigned char bus, unsigned char device)
{
	int i, nroutes;

	if (pci_rbuf.size == 0)
		return (0xff);
	nroutes = pci_rbuf.size / pci_routesz;
	for (i = 0; i < nroutes; i++) {
		if (pci_routes[i].bus == bus &&
			(pci_routes[i].dev & 0xf8) == (device & 0xf8)) {
			return (pci_routes[i].slot);
		}
	}
	return (0xff);
}

void
init_pci()
{
	u_char mechanism;
	u_short version;

	/*
	 * determine if we have any pci busses
	 */
	if (pci_present(&mechanism, &Bios_max_bus_pci, &version)) {
		Pci = 1;

		switch (mechanism & 3) {
		case 1: /* PCI_MECHANISM_1 */
		case 3: /* supports both */
			Max_dev_pci = 32;
			break;
		case 2: /* PCI_MECHANISM_2 */
			Max_dev_pci = 16;
		}
		/*
		 * Check for the Cyrix chipset, which reports config
		 * mechanism 1, but hangs the system later during
		 * bus enumeration if Max_dev_pci is set to 32.
		 * prtpci.exe failed in a similar way.
		 */
		if (pci_is_cyrix()) {
			Max_dev_pci = 16;
		}

		/*
		 * Check for the mercury chipset, which can report
		 * phantom busses
		 */
		if (pci_is_mercury()) {
			Bios_max_bus_pci = 0;
		}

		/*
		 * Allocate the bus path pointers array
		 * now we know a limit on how many busses there are.
		 *
		 * Unfortunately some pci bioses report the max pci bus
		 * as zero based and others as 1 based. So for the
		 * the moment assume zero based.
		 * The Max_bus_pci value is set correctly
		 * later after we've walked the pci tree(s).
		 *
		 * Alloc an additional one in case we have peer pci bus bridges
		 */
		if (!(Bus_path_pci =
		    (char **)calloc(Bios_max_bus_pci + 2, sizeof (char *)))) {
			MemFailure();
		}
		Bus_path_pci[0] = bus0path_pci;

		debug(D_FLOW,
		    "Pci system, max bus 0x%x, mechanism 0x%x, version 0x%x\n",
		    Bios_max_bus_pci, mechanism, version);
	} else {
		debug(D_FLOW, "No PCI busses found\n");
	}

}

/*
 * 1st pass through pci functions saves the resources used
 * by any already configured functions, and enumerates (tells
 * the system about these devices).
 */
void
enumerator_pci()
{
	u_char i, j;
	char *buf;
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	char far *fp;

	/*
	 * decide which PCI-IDE mode to use this time
	 */
	buf = read_prop("pciide", NULL);
	if (buf != NULL && strncmp(buf, "true", 4) == 0) {
		debug(D_PCI, "Full-on pci-ide mode\n");
		pciide_full_on = TRUE;
	} else {
		pciide_full_on = FALSE;
	}

	if (Pci) {
		/*
		 * Get PCI interrupt routing tables from BIOS, if the
		 * BIOS suuuports it.  The table contain device to
		 * slot mapping information.
		 * Note: currently we use a fixed size buffer that
		 * allows up to 32 pci devices.
		 */
		pci_rbuf.size = sizeof (pci_routes);
		pci_rbuf.routes = pci_routes;
		_segread(&segregs);
		fp = (char far *)(&pci_rbuf);
		inregs.x.di = _FP_OFF(fp);
		segregs.es = _FP_SEG(fp);
		inregs.h.ah = PCI_FUNCTION_ID;
		inregs.h.al = 0x0e; /* Get Routing Options */
		inregs.x.bx = 0;
		fp = (char far *)0xf0000000;
		segregs.ds = _FP_SEG(fp);
		_int86x(0x1a, &inregs, &outregs, &segregs);
		if (outregs.x.cflag || outregs.h.ah != PCI_SUCCESS) {
			pci_rbuf.size = 0;
		}
		Max_bus_pci = 0;
		create_config_ports_board_pci();
		get_configured_dev_resources(0);
		if (Bios_max_bus_pci == 0) {
			/*
			 * Some bioses return Bios_max_bus_pci as 0 based
			 * and some 1 based. If Bios_max_bus_pci is 0
			 * we know we have only one bus and can optimise.
			 */
			return;
		}

		/*
		 * Now enumerate any peer busses
		 *
		 * Note we need the (i < (Bios_max_bus_pci + 1))
		 * test on the loop to stop perpetual looping due to
		 * the system returning a valid vendor id on an invalid
		 * bus (eg the Boulder system : marg).
		 * Perhaps this is even a bios problem.
		 */
		for (i = 1, j = 1; i < (u_char)(Bios_max_bus_pci + 1); i++) {
			/*
			 * 1st check if bus has already been enumerated
			 */
			if (Bus_path_pci[i] == NULL) {
				buf = malloc(sizeof ("/pci@FF,0"));
				(void) sprintf(buf, "/pci@%x,0", j++);
				Bus_path_pci[i] =  buf;
			}
			get_configured_dev_resources(i);
		}
	}
}

/*
 * Use the results of the PCI BIOS call that returned the routing tables
 * to build the 1275 slot-names property for the indicated bus.
 * Results are returned in buf.  Length is return value, -1 is returned on
 * overflow and zero is returned if no data exists to build a property.
 */
int
pci_slot_names_prop(int bus, char *buf, int len)
{
	int i, nroutes, nnames, plen;
	unsigned char dev, slot[32];
	unsigned long mask;

	if (pci_rbuf.size == 0)
		return (0);
	nroutes = pci_rbuf.size / pci_routesz;
	nnames = 0;
	mask = 0;
	for (i = 0; i < 32; i++)
		slot[i] = 0xff;
	for (i = 0; i < nroutes; i++) {
		if (pci_routes[i].bus != bus)
			continue;
		if (pci_routes[i].slot != 0) {
			dev = (pci_routes[i].dev & 0xf8) >> 3;
			slot[dev] = pci_routes[i].slot;
			mask |= (1L << dev);
			nnames++;
		}
	}
	if (nnames == 0)
		return (0);
	if (len < (4 + nnames * 8))
		return (-1);
	*(unsigned long *)buf = mask;
	plen = 4;
	for (i = 0; i < 32; i++) {
		if (slot[i] == 0xff)
			continue;
		sprintf(buf + plen, "Slot %d", slot[i]);
		plen += strlen(buf+plen) + 1;
		*(buf + plen) = 0;
	}
	for (; plen % 4; plen++)
		*(buf + plen) = 0;
	return (plen);
}

#ifdef NOTYET
/*
 * 2nd pass configures any unconfigured pci functions and enumerates them.
 * Returns 0 on success, 1 on a configuration failure.
 */
int
configure_pci()
{
	u_char i;

	if (!Pci) {
		return (0); /* configuration success */
	}

	for (i = 0; i <= Max_bus_pci; i++) {
		if (configure_bus_pci(i)) {
			return (1); /* configuration failure */
		}
	}
	return (0); /* configuration success */
}
#endif /* NOTYET */

void
/*ARGSUSED0*/
program_pci(Board *bp)
{

}

/*
 * For any fixed configuration (often compatability) pci devices
 * and those with their own expansion rom, a Board is constructed
 * to hold the already configured device details.
 */
void
get_configured_dev_resources(u_char bus)
{
	u_char dev;
	u_char func;
	u_char nfunc;
	u_char devfunc;
	u_int venid;
	u_char header;
	u_char multi_func;

	for (dev = 0; dev < Max_dev_pci; dev++) {
		nfunc = 1;
		multi_func = 0;
		for (func = 0; func < nfunc; func++) {
			devfunc = (dev << FUNF_DEVSHFT) | func;

			venid = pci_getw(bus, devfunc, PCI_CONF_VENID);
			if ((venid == 0xffff) || (venid == 0)) {
				/* no function at this address */
				continue;
			}

			header = pci_getb(bus, devfunc, PCI_CONF_HEADER);
			if (header == 0xff) {
				continue; /* illegal value */
			}

			/*
			 * according to some mail from Microsoft posted
			 * to the pci-drivers alias, their only requirement
			 * for a multifunction device is for the 1st
			 * function to have to PCI_HEADER_MULTI bit set.
			 */
			if ((func == 0) && (header & PCI_HEADER_MULTI)) {
				multi_func = 1;
				nfunc = 8;
			}
			if (Max_bus_pci < bus) {
				Max_bus_pci = bus;
			}
			new_func_pci(bus, devfunc, multi_func);
		}
	}
}

#ifdef NOTYET
/*
 * Configure and enumerate all pci functions on this bus only
 */
static int
configure_bus_pci(u_char bus)
{
	u_char dev;
	u_char func;
	u_char nfunc;
	u_char devfunc;
	u_char secbus;
	u_int venid;
	u_char header;
	u_char multi_func;

	for (dev = 0; dev < Max_dev_pci; dev++) {
		nfunc = 1;
		multi_func = 0;
		for (func = 0; func < nfunc; func++) {
			devfunc = (dev << FUNF_DEVSHFT) | func;

			venid = pci_getw(bus, devfunc, PCI_CONF_VENID);
			if ((venid == 0xffff) || (venid == 0)) {
				/* no function at this address */
				continue;
			}

			debug(D_PCI, "Before pci configure\n");
			print_confspace_pci(bus, devfunc);

			header = pci_getb(bus, devfunc, PCI_CONF_HEADER);
			if (header == 0xff) {
				continue; /* illegal value */
			}
			/*
			 * according to some mail from Microsoft posted
			 * to the pci-drivers alias, their only requirement
			 * for a multifunction device is for the 1st
			 * function to have to PCI_HEADER_MULTI bit set.
			 */
			if ((func == 0) && (header & PCI_HEADER_MULTI)) {
				multi_func = 1;
				nfunc = 8;
			}

			if (!(already_configured_pci(bus, devfunc))) {
				if (configure_func_pci(bus, devfunc)) {
					return (1);
				}
				new_func_pci(bus, devfunc, multi_func);

				debug(D_PCI, "After pci configure\n");
				print_confspace_pci(bus, devfunc);

			}
		}
	}
	return (0);
}

static int
configure_func_pci(u_char bus, u_char devfunc)
{
	u_char max_basereg;
	u_char i;
	u_char header;
	u_long base;
	u_long value;
	u_long len;
	u_int offset;
	u_long start;
	u_long end;
	u_int bar_sz;

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

	/*
	 * Get the resource length by saving the current
	 * value of the base register.  Disable memory/io, then
	 * write 0xffffffff to the base register.  Read the
	 * value back to determine the required size of the
	 * address space.  Restore the base register
	 * contents.
	 */
	for (i = 0, offset = PCI_CONF_BASE0; i < max_basereg;
		i++, offset += bar_sz) {

		base = pci_getl(bus, devfunc, offset);

		/* determine the size of the address space */
		pci_putl(bus, devfunc, offset, 0xffffffff);
		value = pci_getl(bus, devfunc, offset);

		/* restore the original value in the base */
		pci_putl(bus, devfunc, offset, base);

		if (base & PCI_BASE_SPACE_IO) {
			bar_sz = PCI_BAR_SZ_32;
			/* skip base regs with size of 0 */
			value &= PCI_BASE_IO_ADDR_M;
			if (value == 0) {
				continue;
			}

			/*
			 * Get io space length. Some pci devices
			 * eg buslogic bt946C clear the top bits
			 * So the folling calculation essentially
			 * counts the number of trailing zeros.
			 */
			len = ((value ^ (value-1)) + 1) >> 1;
			ASSERT(len <= 256); /* PCI 2.1 spec */
			/*
			 * Find io space of the required length
			 */
			for (start = 0x400;
			    start < (0x10000 - len); start += len) {
				if (Query_Port(start, len) == NULL) {
					/*
					 * Found space, set base.
					 */
					pci_putl(bus, devfunc, offset,
					    start | PCI_BASE_SPACE_IO);
					break;
				}
			}
			if (start >= (0x10000 - len)) {
				return (1);
			}
		} else { /* memory space */

			/*
			 * Check for 64 bit memory addresses
			 */
			if ((base & PCI_BASE_TYPE_M) == PCI_BASE_TYPE_ALL) {
				bar_sz = PCI_BAR_SZ_64;
				if (pci_getl(bus, devfunc, offset + 4) != 0) {
					fatal("64 PCI bit addr unsupported\n");
				}
				pci_putl(bus, devfunc, offset, 0xffffffff);
				if (pci_getl(bus, devfunc, offset + 4) != 0) {
					fatal("64 PCI bit len unsupported\n");
				}
				/*
				 * Upper 32 bits of length and address are zero
				 * just continue as though 32 bits ...
				 */
			} else {
				bar_sz = PCI_BAR_SZ_32;
			}


			/* skip base regs with size of 0 */
			value &= PCI_BASE_M_ADDR_M;
			if (value == 0) {
				continue;
			}

			/*
			 * Find memory space of the required length
			 */
			len = ((value ^ (value-1)) + 1) >> 1;

			/*
			 * Ensure min of 1KB
			 */
			if (len < 0x400) {
				len = 0x400;
			}
			if (base & PCI_BASE_TYPE_LOW) {
				start = 0;
				end = 0x100000 - len;
			} else {
				start = 0x100000; /* start at 1MB */
				end = 0xFFFFFFFF - len + 1;
			}
			while (start < end) {
				if (Query_Mem(start, len) == NULL) {
					/*
					 * Found space, set base.
					 */
					pci_putl(bus, devfunc, offset,
					    (base & ~PCI_BASE_M_ADDR_M) |
					    start);
					break;
				}
				start += len;
			}
			if (start >= end) {
				return (1);
			}
		}
	}

	return (0);
}
#endif /* NOTYET */

#define	ALL_BUS 0
#define	NON_0_BUS 1

/*
 * table of pci devices to discard - buggy hardware
 * see bug 1257459
 */
struct bad_pci_s {
	u_int vendorid;
	u_int deviceid;
	u_int bus;
} bad_pci[] = {
	{0x1045, 0xc621, ALL_BUS},	/* OPTi 82c621 compatible pci ide */
				/* ... not marked as compatible though */
	{0x1039, 0x406, ALL_BUS},	/* SIS 85C501/2 Pentium Bridge  */
				/* ... bad BARS, irq, class, subsys ids */
	{0x907f, 0x2015, ALL_BUS},	/* Atronics IDE-2015PL EIDE */
				/* ... bad read only bars */
/*
 * 	Some S3/864 chips ignore the distinction between type 0 and type 1
 *	configuration cycles.  They respond to either if their IDSEL line
 *	is high.  Since many motherboard implementations wire IDSEL to the
 *	high-order address lines unused in type 0 cycles, this results in
 *	the chip responding to any type 1 cycle where the right bit happens
 *	to be set as part of the bus or device number.  We suppress these
 *	"ghost" devices by suppressing instances of these devices where
 * 	bus>1.  Since we don't know what S3/864 devices are affected, we
 *	do this for all four device IDs.  Since VGA devices are required
 *	to be on bus 0, there's no harm in specially handling non-broken
 *	versions.
 */
	{0x5333, 0x88c0, NON_0_BUS},	/* S3/864 (breakage presumed) */
	{0x5333, 0x88c1, NON_0_BUS},	/* S3/864 (breakage observed) */
	{0x5333, 0x88c2, NON_0_BUS},	/* S3/864 (breakage presumed) */
	{0x5333, 0x88c3, NON_0_BUS},	/* S3/864 (breakage presumed) */
	{0, 0, 0},	/* Space for patching in new values, for field fixes */
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},	/* Terminating entry, must be zero */
};

#define	NBAD_PCI (sizeof (bad_pci) / sizeof (struct bad_pci_s))


void
new_func_pci(u_char bus, u_char devfunc, u_char multi_func)
{
	Board *bp;
	u_char basecl;
	u_char subcl;
	u_char progcl;
	u_char header;
	u_char irq;
	u_int max_basereg;
	u_int i;
	u_long base;
	u_long base_hi;
	u_long value;
	u_int offset, end;
	u_int roffset;
	u_long len;
	u_int vendorid;
	u_int deviceid;
	u_int bar_sz;
	u_long id, subid;
	u_char pciide;

	basecl = pci_getb(bus, devfunc, PCI_CONF_BASCLASS);
	subcl = pci_getb(bus, devfunc, PCI_CONF_SUBCLASS);
	progcl = pci_getb(bus, devfunc, PCI_CONF_PROGCLASS);

	/*
	 * Throw away any compatible pci ide controllers as they are
	 * generated as standard devices, as part of the probe only ata.bef.
	 * Unfortunately, the OPTi82c621 sets the native mode bits when
	 * its in compatible mode, so we additional check for this broken
	 * hardware.
	 *
	 * Note, for the moment throw away all PCI/IDE devices, because we
	 * don't correctly handle decoding the PCI config space when in
	 * native mode - which contains 2 devices. See bugs 4029610 & 4028238.
	 */
	if ((basecl == PCI_CLASS_MASS) && (subcl == PCI_MASS_IDE)) {
		debug(D_FLOW, "Adding pci ide controller\n");
		pciide = TRUE;
	} else {
		pciide = FALSE;
	}

	vendorid = pci_getw(bus, devfunc, PCI_CONF_VENID);
	deviceid = pci_getw(bus, devfunc, PCI_CONF_DEVID);

	for (i = 0; bad_pci[i].vendorid != 0; i++) {
		if ((vendorid == bad_pci[i].vendorid) &&
		    (deviceid == bad_pci[i].deviceid)) {
			if ((bus == 0) && (bad_pci[i].bus == NON_0_BUS)) {
				continue;
			}
			debug(D_FLOW,
			    "Discarded bad pci hardware: pci%x,%x, bus 0x%x\n",
			    vendorid, deviceid, bus);
			return;
		}
	}

	/*
	 * Fill in the Board fields
	 */
	bp = new_board();
	bp->bustype = RES_BUS_PCI;
	bp->category = DCAT_UNKNWN;
	bp->slot = find_pci_slot(bus, devfunc);
	bp->pci_venid = vendorid;
	bp->pci_devid = deviceid;
	bp->pci_class = ((u_long) basecl << 16) | ((u_long) subcl << 8) |
	    (u_long) progcl;
	bp->pci_multi_func = multi_func;

	header = pci_getb(bus, devfunc, PCI_CONF_HEADER) & PCI_HEADER_TYPE_M;

	switch (header) {
	case PCI_HEADER_ZERO:
		bp->pci_subvenid = pci_getw(bus, devfunc, PCI_CONF_SUBVENID);
		bp->pci_subdevid = pci_getw(bus, devfunc, PCI_CONF_SUBSYSID);
		break;
	case PCI_HEADER_CARDBUS:
		bp->pci_subvenid = pci_getw(bus, devfunc, PCI_CBUS_SUBVENID);
		bp->pci_subdevid = pci_getw(bus, devfunc, PCI_CBUS_SUBSYSID);

		/*
		 * We need to always disable cardbus legacy mode
		 * This is a convenient place to do it.
		 * It must happen before any legacy mode bef
		 * probes for pcmcia controllers.
		 */
		pci_putl(bus, devfunc, PCI_CBUS_LEG_MODE_ADDR, 0);
		break;
	default:
		bp->pci_subvenid = 0;
		break;
	}
	subid = (((u_long) bp->pci_subvenid) << 16) | bp->pci_subdevid;
	id = (((u_long) vendorid) << 16) | deviceid;
	if (bp->pci_subvenid) {
		bp->devid = subid;
	} else {
		bp->devid = id;
	}

	if (device_is_compaq_8x5(vendorid, deviceid, bus, devfunc)) {
		bp->devid = MAGIC_COMPAQ_8X5_DEVID;
		bp->pci_cpqdid = deviceid;
	} else if (device_is_ncr_pqs(vendorid, deviceid, bus, devfunc))
		bp->devid = MAGIC_NCR_PQS_DEVID;

	bp->pci_busno = bus;
	bp->pci_devfunc = devfunc;

	if (!already_configured_pci(bus, devfunc)) {
		/*
		 * Bios hasn't configured the device or it has no resources
		 */
		goto done;
	}

	switch (header) {
	case PCI_HEADER_ZERO:
		max_basereg = PCI_BASE_NUM;
		roffset = PCI_CONF_ROM;
		break;
	case PCI_HEADER_PPB:
		max_basereg = PCI_BCNF_BASE_NUM;
		roffset = PCI_BCNF_ROM;
		break;
	case PCI_HEADER_CARDBUS:
		max_basereg = PCI_CBUS_BASE_NUM;
		roffset = 0;
		break;
	default: /* safety */
		max_basereg = 0;
		roffset = 0;
		break;
	}

	/*
	 * Get the resource details by saving the current
	 * value of the base register.  Disable memory/io, then
	 * write 0xffffffff to the base register.  Read the
	 * value back to determine the required size of the
	 * address space.  Restore the base register
	 * contents.
	 */
	end = PCI_CONF_BASE0 + max_basereg * sizeof (u_long);
	for (i = 0, offset = PCI_CONF_BASE0; offset < end;
		i++, offset += bar_sz) {

		base = pci_getl(bus, devfunc, offset);

		/* determine the size of the address space */
		pci_putl(bus, devfunc, offset, 0xffffffff);
		value = pci_getl(bus, devfunc, offset);

		/* restore the original value in the base */
		pci_putl(bus, devfunc, offset, base);

		if (pciide || (base & PCI_BASE_SPACE_IO)) {
			bar_sz = PCI_BAR_SZ_32;

			value &= PCI_BASE_IO_ADDR_M;
			base &= PCI_BASE_IO_ADDR_M;

			len = ((value ^ (value-1)) + 1) >> 1;

			if (pciide) {
				(void) PciIdeAdjustBAR(progcl, i, &base, &len);

			} else if ((base == 0) || (value == 0)) {
				/* skip base regs with size of 0 */
				continue;
			}

			bp = AddResource_devdb(bp, RESF_Port, base, len);
		} else {
			/*
			 * Check for 64 bit memory addresses
			 */
			if ((base & PCI_BASE_TYPE_M) == PCI_BASE_TYPE_ALL) {
				bar_sz = PCI_BAR_SZ_64;
				if ((base_hi =
					pci_getl(bus, devfunc, offset + 4))
					!= 0) {
					fatal("64 PCI bit addr unsupported\n");
				}
				pci_putl(bus, devfunc, offset + 4, 0xffffffff);
				if (pci_getl(bus, devfunc, offset + 4)
					!= 0xffffffff) {
					pci_putl(bus, devfunc, offset + 4,
						base_hi);
					fatal("64 PCI bit len unsupported\n");
				}
				pci_putl(bus, devfunc, offset + 4, base_hi);
				/*
				 * Upper 32 bits of length and address are zero
				 * just continue as though 32 bits ...
				 */
			} else {
				bar_sz = PCI_BAR_SZ_32;
			}

			/* skip base regs with size of 0 */
			value &= PCI_BASE_M_ADDR_M;
			base &= PCI_BASE_M_ADDR_M;
			if ((base == 0) || (value == 0)) {
				continue;
			}
			len = ((value ^ (value-1)) + 1) >> 1;
			bp = AddResource_devdb(bp, RESF_Mem, base, len);
		}
	}

	/*
	 * Add the irq (interrupt line) if the interrupt pin is non zero
	 * and the interrupt line is valid (0 <= irq <= 15)
	 * Note the 2.1 pci spec says that 255 is possible and should
	 * be considered as unknown.
	 */

	if (pci_getb(bus, devfunc, PCI_CONF_IPIN))
		irq = pci_getb(bus, devfunc, PCI_CONF_ILINE);
	else
		irq = 16;

	if (!pciide) {
		if (irq < 16)
			bp = AddResource_devdb(bp, RESF_Irq, irq, 1);

	} else if (pciide_full_on) {
		/*
		 * Bug 4102415:
		 *
		 * When the controller is in compatibility mode use the
		 * legacy IRQ values, 14 and 15.
		 */
		if ((progcl & 0x01) == 0)
			bp = AddResource_devdb(bp, RESF_Irq, 14, 1);

		if ((progcl & 0x04) == 0)
			bp = AddResource_devdb(bp, RESF_Irq, 15, 1);

		/*
		 * If this is a pci-ide device only include its
		 * ILINE IRQ if one or both its two controllers
		 * is in native PCI mode.  Some buggy BIOSes
		 * program the ILINE register even when only
		 * ISA legacy interrupts are being used.
		 */
		if ((progcl & 0x05) && (irq < 16)) {
			bp = AddResource_devdb(bp, RESF_Irq, irq, 1);
		}
	}

	/*
	 * Add the expansion rom memory space
	 *
	 * Rom offset is different for pci->pci bus bridge
	 */
	if (roffset == 0) {
		goto done;
	}
	base = pci_getl(bus, devfunc, roffset);
	pci_putl(bus, devfunc, roffset, PCI_BASE_ROM_ADDR_M);
	value = pci_getl(bus, devfunc, roffset);
	pci_putl(bus, devfunc, roffset, base);
	value &= PCI_BASE_ROM_ADDR_M;
	base &= PCI_BASE_M_ADDR_M;

	if ((base != 0) && (value != 0)) {
		len = ((value ^ (value-1)) + 1) >> 1;
		bp = AddResource_devdb(bp, RESF_Mem, base, len);
	}

	/* add the three hard-decode, aliased address spaces for VGA */
	if (((basecl == PCI_CLASS_NONE) && (subcl == PCI_NONE_VGA)) ||
	    ((basecl == PCI_CLASS_DISPLAY) && (subcl == PCI_DISPLAY_VGA))) {
		unsigned short cmdreg;
		cmdreg = pci_getw(bus, devfunc, PCI_CONF_COMM);
		if (cmdreg & PCI_COMM_IO) {
			bp = AddResource_devdb(bp, RESF_Port, 0x3b0, 0xc);
			bp = AddResource_devdb(bp, RESF_Port, 0x3c0, 0x20);
		}
		if (cmdreg & PCI_COMM_MAE) {
			bp = AddResource_devdb(bp, RESF_Mem, 0xa0000, 0x20000);
		}
	}

	/* add the hard-decode, aliased address spaces for 8514 */
	if ((basecl == PCI_CLASS_DISPLAY) && (subcl == PCI_DISPLAY_VGA) &&
	    (progcl & PCI_DISPLAY_IF_8514)) {
		unsigned short cmdreg;

		cmdreg = pci_getw(bus, devfunc, PCI_CONF_COMM);
		if (cmdreg & PCI_COMM_IO) {
			bp = AddResource_devdb(bp, RESF_Port, 0x2e8, 1);
			bp = AddResource_devdb(bp, RESF_Port, 0x2ea, 6);
		}
	}

	if ((basecl == PCI_CLASS_BRIDGE) && (subcl == PCI_BRIDGE_PCI)) {
		u_char secbus;

		secbus = pci_getb(bus, devfunc, PCI_BCNF_SECBUS);
		if (secbus != 0) {
			save_bus_path_pci(bus, secbus, devfunc);
		}
		save_ranges_pci(bp, secbus, bus, devfunc);
		bp->pci_ppb_bcntrl = pci_getb(bus, devfunc, PCI_BCNF_BCNTRL);
	}

done:
	add_board(bp); /* Tell rest of system about device */
}

/*
 * save away the solaris kernel /devices path of all pci busses
 */
void
save_bus_path_pci(u_char pbus, u_char bus, u_char devfunc)
{
	u_char func = devfunc & FUNF_FCTNNUM;

	/*
	 * Allocate the maximum space for the bus path string.
	 * This of its parents (pbus) string plus the maximum pci component
	 */
	if (!(Bus_path_pci[bus] = (char *)malloc(strlen(Bus_path_pci[pbus])
	    + strlen("pciVVVV,DDDD@dd,f") + 1))) {
		MemFailure();
	}

	if (func) {
		(void) sprintf(Bus_path_pci[bus], "%s/pci%x,%x@%x,%x",
		    Bus_path_pci[pbus],
		    pci_getw(pbus, devfunc, PCI_CONF_VENID),
		    pci_getw(pbus, devfunc, PCI_CONF_DEVID),
		    devfunc >> FUNF_DEVSHFT,
		    func);
	} else {
		(void) sprintf(Bus_path_pci[bus], "%s/pci%x,%x@%x",
		    Bus_path_pci[pbus],
		    pci_getw(pbus, devfunc, PCI_CONF_VENID),
		    pci_getw(pbus, devfunc, PCI_CONF_DEVID),
		    devfunc >> FUNF_DEVSHFT);
	}
	if (bus > Max_bus_pci) {
		Max_bus_pci = bus;
	}
}

int
already_configured_pci(u_char bus, u_char devfunc)
{
	if (pci_getw(bus, devfunc, PCI_CONF_COMM) &
	    (PCI_COMM_IO | PCI_COMM_MAE)) {
		return (1);
	}
	return (0);
}

void
create_config_ports_board_pci()
{
	Board *bp;

	/*
	 * Fill in the Board fields
	 */
	bp = new_board();
	bp->bustype = RES_BUS_PCI;
	bp->category = DCAT_OTH;
	bp->devid = 0; /* pci0,0 special marker this board */

	/*
	 * Fill in resource fields
	 */
	bp = AddResource_devdb(bp, RESF_Port, 0xcf8, 8);

	add_board(bp); /* Tell rest of system about device */
}

/*
 * Special hack for Intel's Mercury chipset, 82433LX(?) and 82434LX(?).
 *
 * It seems that at least some revs of this chipset will respond to
 * d=0 f=0 for all bus numbers.  This makes us think that there are 256
 * host bridges.  This is bad.  Check to see whether we're a Mercury
 * so the higher-level code can suppress these phantoms.
 */
int
pci_is_mercury(void)
{
	if (pci_getw(0, 0, PCI_CONF_VENID) == 0x8086 &&
	    pci_getw(0, 0, PCI_CONF_DEVID) == 0x04a3 &&
	    pci_getw(0, 0, PCI_CONF_REVID) < 0x10) {
		return (TRUE);
	}
	return (FALSE);
}

int
pci_is_cyrix(void)
{
	if ((pci_getw(0, 0, PCI_CONF_VENID) == 0x1078) && /* Cyrix */
	    (pci_getw(0, 0, PCI_CONF_DEVID) == 1) &&
	    (pci_getw(0, 0, PCI_CONF_REVID) == 0)) {
		return (TRUE);
	}
	return (FALSE);
}


/*
 * Determine whether this device is a specially-handled Compaq device
 * (either an 825 or 875 with its SCRATCHB register byte-checksumming to
 * 0x5A)
 */

#define	COMPAQ_ADDR	((char far *)0xF000FFEA)
#define	COMPAQ		(char far *)"COMPAQ"
#define	NREG_SCRATCHB	(0x5C+0x80)	/* 5C, in config space (+0x80) */

u_int
device_is_compaq_8x5(u_int vendorid, u_int deviceid, u_char bus,
    u_char devfunc)
{
	unsigned long scratchb;
	unsigned char *p;
	unsigned char sum;
	int i;
	int retval;

	if (strncmp(COMPAQ_ADDR, COMPAQ, sizeof (COMPAQ)) != 0) {
		debug(D_PCI, "At %lp: %6.6ls\n", COMPAQ_ADDR, COMPAQ_ADDR);
		debug(D_PCI, "Machine is not a COMPAQ machine\n");
		return (FALSE);
	}

	/*
	 * Compaq promises that the only devices which will have a
	 * byte checksum of 0x5A in the SCRATCHB register, iff they're
	 * running in a Compaq system and are boards for which they want
	 * their driver loaded.  This may find an alien board too,
	 * but that's not a worry for Compaq.
	 */

	if (vendorid == SYMBIOS_VID &&
	    (deviceid == SYMBIOS_825_DID || deviceid == SYMBIOS_875_DID)) {
		scratchb = pci_getl(bus, devfunc, NREG_SCRATCHB);
		p = (unsigned char *)&scratchb;
		sum = 0;
		for (i = 0; i < 4; i++) {
			sum += *p++;
		}
		if (sum == 0x5A) {
			retval = TRUE;
		} else {
			retval = FALSE;
		}
	} else {
		retval = FALSE;
	}

	debug(D_PCI, "bus/dev/func %d/%d/%d (0x%x,0x%x) is %sa Compaq 8x5\n",
	    bus, (devfunc & FUNF_DEVNUM) >> FUNF_DEVSHFT,
	    devfunc & FUNF_FCTNNUM, vendorid, deviceid,
	    (retval == TRUE) ? "" : "not ");

	return (retval);
}

u_int
device_is_ncr_pqs(u_int vendorid, u_int deviceid, u_char bus,
    u_char devfunc)
{
	u_char dev = devfunc >> FUNF_DEVSHFT;
	int retval = FALSE;

	/*
	 * An NCR PQS device exists if
	 * 1) the device is a Symbios 875 (VID/DID 1000/f)
	 * 2) the bus number for the device is non-zero
	 * 3) the device number is between 1 and 5 inclusive
	 * 4) there is a direct parent PCI-PCI bridge
	 * 5) there is a peer (on the same bus) memory controller ASIC, with
	 *    VID/DID/class 101a/9/0580xx at device number 8
	 */

	if (vendorid == SYMBIOS_VID && deviceid == SYMBIOS_875_DID) {
		if (bus > 0 && (dev >= 1 && dev <= 5) &&
			peer_memory_exists(bus) &&
			pci_bridge_parent_exists(bus))
				retval = TRUE;
		else
			retval = FALSE;
	}
	else
		retval = FALSE;

	debug(D_PCI, "bus/dev/func %d/%d/%d (0x%x,0x%x) is %sa NCR PQS\n",
		bus, (devfunc & FUNF_DEVNUM) >> FUNF_DEVSHFT,
		devfunc & FUNF_FCTNNUM, vendorid, deviceid,
		(retval == TRUE) ? "" : "not ");
	return (retval);
}

#define	NCR_MEMORYDEV_VID	0x101a
#define	NCR_MEMORYDEV_DID	0x9
#define	NCR_MEMORYDEV_BCLASS	0x5
#define	NCR_MEMORYDEV_SUBCLASS	0x80
#define	NCR_MEMORYDEV_DEVNUM	0x8

u_int
peer_memory_exists(u_char bus)
{
	u_short	vendorid;
	u_short	deviceid;
	u_char	basecl;
	u_char	subcl;
	u_char	devfunc = NCR_MEMORYDEV_DEVNUM << FUNF_DEVSHFT;

	vendorid = pci_getw(bus, devfunc, PCI_CONF_VENID);
	deviceid = pci_getw(bus, devfunc, PCI_CONF_DEVID);
	basecl = pci_getb(bus, devfunc, PCI_CONF_BASCLASS);
	subcl = pci_getb(bus, devfunc, PCI_CONF_SUBCLASS);

	if (vendorid == NCR_MEMORYDEV_VID &&
		deviceid == NCR_MEMORYDEV_DID &&
		basecl == NCR_MEMORYDEV_BCLASS &&
		subcl == NCR_MEMORYDEV_SUBCLASS)
		return (TRUE);
	else
		return (FALSE);
}

u_int
pci_bridge_parent_exists(u_char pqs_875_bus)
{
	u_char	bus;
	u_char	dev;
	u_char	basecl;
	u_char	subcl;
	u_char	nfunc;
	u_char	func;
	u_char	header;
	u_char 	devfunc;
	u_int	venid;

	for (bus = 0; bus < Bios_max_bus_pci + 1; bus++) {
		for (dev = 0; dev < Max_dev_pci; dev++) {
			nfunc = 1;
			for (func = 0; func < nfunc; func++) {
				devfunc = (dev << FUNF_DEVSHFT) | func;
				venid = pci_getw(bus, devfunc, PCI_CONF_VENID);
				if ((venid == 0xffff) || (venid == 0)) {
					/* no function at this address */
					continue;
				}
				header = pci_getb(bus, devfunc,
					PCI_CONF_HEADER);
				if (header == 0xff) {
					continue; /* illegal value */
				}
				if ((func == 0) &&
					(header & PCI_HEADER_MULTI)) {
					nfunc = 8;
				}
				basecl = pci_getb(bus, devfunc,
					PCI_CONF_BASCLASS);
				subcl = pci_getb(bus, devfunc,
					PCI_CONF_SUBCLASS);
				if ((basecl == PCI_CLASS_BRIDGE) &&
					(subcl == PCI_BRIDGE_PCI)) {
					u_char secbus;

					secbus = pci_getb(bus, devfunc,
						PCI_BCNF_SECBUS);
					if (secbus == pqs_875_bus)
						return (TRUE);
				}
			}
		}
	}
	return (FALSE);
}

/*ARGSUSED*/
void
save_ranges_pci(Board *bp, u_char secbus, u_char bus, u_char devfunc)
{
	u_long base, start;
	u_long limit;
	u_long len;
	u_long tmp;

	if (base = (u_long) pci_getb(bus, devfunc, PCI_BCNF_IO_BASE_LOW)) {
		limit = (u_long) pci_getb(bus, devfunc, PCI_BCNF_IO_LIMIT_LOW);
		limit = (limit & PCI_PPB_M_ADDR_M) << 8;
		start = (base & PCI_PPB_M_ADDR_M) << 8;
		if ((base & PCI_PPB_IO_TYPE_M) == 1) { /* 32 bit */
			tmp = (u_long)
			    pci_getw(bus, devfunc, PCI_BCNF_IO_BASE_HI);
			start += (tmp << 16);
			tmp = (u_long)
			    pci_getw(bus, devfunc, PCI_BCNF_IO_LIMIT_HI);
			limit += (tmp << 16);
		}
		if (start <= limit) {
			len = limit - start + 0x1000;
			bp->pci_ppb_io.start = start;
			bp->pci_ppb_io.len = len;
		}
	}
	base = (u_long) pci_getw(bus, devfunc, PCI_BCNF_MEM_BASE);
	limit = (u_long) pci_getw(bus, devfunc, PCI_BCNF_MEM_LIMIT);
	if (base != 0UL && limit != 0UL && (limit >= base)) {
		bp->pci_ppb_mem.start = base << 16;
		bp->pci_ppb_mem.len = (limit - base + 0x10) << 16;
	}

	base = (u_long) pci_getw(bus, devfunc, PCI_BCNF_PF_BASE_LOW);
	if (base != 0UL &&
	    ((base & PCI_PPB_M_TYPE_M) == 0) /* only handle 32 bit */ &&
	    (limit = (u_long) pci_getw(bus, devfunc, PCI_BCNF_PF_LIMIT_LOW)) &&
	    (limit >= base)) {
		bp->pci_ppb_pmem.start = base << 16;
		bp->pci_ppb_pmem.len = (limit - base + 0x10) << 16;
	}
}


/*
 * config info for pci-ide devices
 */
struct {
	u_char	native_mask;	/* 0 == 'compatibility' mode, 1 == native */
	u_char	bar_offset;	/* offset for alt status register */
	u_short	addr;		/* compatibility mode base address */
	u_short	length;		/* number of ports for this BAR */
} pciide_bar[] = {
	{ 0x01, 0, 0x1f0, 8 },	/* primary lower BAR */
	{ 0x01, 2, 0x3f6, 1 },	/* primary upper BAR */
	{ 0x04, 0, 0x170, 8 },	/* secondary lower BAR */
	{ 0x04, 2, 0x376, 1 }	/* secondary upper BAR */
};

/*
 * Adjust the reg properties for a dual channel PCI-IDE device.
 *
 * NOTE: don't do anything that changes the order of the hard-decodes
 * and programmed BARs. The kernel driver depends on these values
 * being in this order regardless of whether they're for a 'native'
 * mode BAR or not.
 */

int
PciIdeAdjustBAR(u_char progcl, u_int index, u_long *basep, u_long *lenp)
{
	int	hard_decode = FALSE;

	/*
	 * Adjust the base and len for the BARs of the PCI-IDE
	 * device's primary and secondary controllers. The first
	 * two BARs are for the primary controller and the next
	 * two BARs are for the secondary controller. The fifth
	 * and sixth bars are never adjusted.
	 */
	if ((long)index >= 0 && index <= 3) {
		*lenp = pciide_bar[index].length;

		if (progcl & pciide_bar[index].native_mask) {
			debug(D_PCI, "pci-ide idx %d native\n", index);
			*basep += pciide_bar[index].bar_offset;
		} else {
			debug(D_PCI, "pci-ide idx %d compat\n", index);
			*basep = pciide_bar[index].addr;
			hard_decode = TRUE;
		}

		/* only do devices at 0x1f0 or 0x170 in full-on mode */
		if (pciide_full_on == FALSE &&
		    *basep == pciide_bar[index].addr) {
			*basep = 0;
			*lenp = 0;
		}
	}

	/*
	 * if either base or len is zero make certain both are zero
	 */
	if (*basep == 0 || *lenp == 0) {
		*basep = 0;
		*lenp = 0;
		hard_decode = FALSE;
	}


	/*
	 * If the BAR is unused or represents a ISA legacy dev
	 * then still create the reg prop but set size = 0, len = 0
	 */

	debug(D_PCI, "pci-ide p 0x%x x %d base 0x%lx len 0x%lx\n",
		progcl, index, *basep, *lenp);

	return (hard_decode);
}
