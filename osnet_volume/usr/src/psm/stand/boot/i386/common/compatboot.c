/*
 * Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)compatboot.c	1.4     99/08/13 SMI"

#include <sys/types.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootconf.h>
#include <sys/bootp2s.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/salib.h>

extern struct pri_to_secboot *compatboot_ip;
extern struct bootops *bop;
extern int boot_device;
extern struct int_pb ic;
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int doint(void);

#define	PCI_FUNCTION_ID		0xb1

#define	PCI_READ_CONFIG_BYTE	0x8
#define	PCI_READ_CONFIG_WORD	0x9

#define	PCI_RC_SUCCESSFUL	0

void
compatboot_createprops(char *outline)
{
	/*
	 *  Use compatboot_ip to create some properties expected with
	 *  the older style boots that relied on the compatboot_ip
	 *  information.  These include MDB boots and boots from ufsbootblk.
	 */
	if (compatboot_ip->bootfrom.ufs.boot_dev) {
		/* non-zero, wasn't floppy */
		if (compatboot_ip->F8.dev_type == MDB_NET_CARD) {
			/* network devices */
			boot_device = BOOT_FROM_NET;
			(void) bsetprop(bop, "b.net-mfg",
			    (char *)compatboot_ip->F8.hba_id, 0, 0);

			(void) sprintf(outline, "%x",
			    compatboot_ip->bootfrom.nfs.ioaddr);
			(void) bsetprop(bop, "b.net-ioaddr", outline, 0, 0);

			(void) sprintf(outline, "%x",
			    compatboot_ip->bootfrom.nfs.membase*16);
			(void) bsetprop(bop, "b.net-membase", outline, 0, 0);

			(void) sprintf(outline, "%x",
			    compatboot_ip->bootfrom.nfs.memsize * 1024);
			(void) bsetprop(bop, "b.net-memsize", outline, 0, 0);

			(void) sprintf(outline, "%d",
			    compatboot_ip->bootfrom.nfs.irq);
			(void) bsetprop(bop, "b.net-irq", outline, 0, 0);
		} else {
			/* disk devices */
			boot_device = BOOT_FROM_DISK;
			(void) sprintf(outline, "%d",
			    compatboot_ip->bootfrom.ufs.ncyls);
			(void) bsetprop(bop, "bootdev-ncyl", outline, 0, 0);

			(void) sprintf(outline, "%d",
			    compatboot_ip->bootfrom.ufs.trkPerCyl);
			(void) bsetprop(bop, "bootdev-nhead", outline, 0, 0);

			(void) sprintf(outline, "%d",
			    compatboot_ip->bootfrom.ufs.secPerTrk);
			(void) bsetprop(bop, "bootdev-nsect", outline, 0, 0);

			(void) sprintf(outline, "%d",
			    compatboot_ip->bootfrom.ufs.bytPerSec);
			(void) bsetprop(bop, "bootdev-sectsiz", outline, 0, 0);

			if (compatboot_ip->bootfrom.ufs.boot_dev != 0x80) {
				/* get F8 info */
				compatboot_ip->F8.blank1 = '\0';
				(void) bsetprop(bop, "b.hba-mfg",
				    (char *)compatboot_ip->F8.hba_id, 0, 0);
				(void) sprintf(outline, "%x",
				    compatboot_ip->F8.base_port);
				(void) bsetprop(bop, "b.hba-ioaddr",
					outline, 0, 0);
				(void) sprintf(outline, "%d",
				    (short)compatboot_ip->F8.MDBdev.scsi.targ);
				(void) bsetprop(bop, "b.target", outline, 0, 0);
				(void) sprintf(outline, "%d",
				    (short)compatboot_ip->F8.MDBdev.scsi.lun);
				(void) bsetprop(bop, "b.lun", outline, 0, 0);
			}
		}
	}
}

/*
 * pci_cfg_getb()
 *
 *	Invoke the PCI BIOS to read a byte from a PCI configuration
 *	space register.
 */
unchar
pci_cfg_getb(
	int	bus,
	int	dev,
	int	func,
	int	reg
)
{
	ic.intval = 0x1a;
	ic.ax = (PCI_FUNCTION_ID << 8) | PCI_READ_CONFIG_BYTE;
	ic.bx = ((bus & 0xff) << 8) | ((dev & 0x1f) << 3) | (func & 0x7);
	ic.di = reg & 0xff;

	(void) doint();

	/* ah (the high byte of ax) must be zero */
	if ((ic.ax & 0xff00) != 0)
		return (0xff);

	return (ic.cx & 0xff);
}

/*
 * pci_cfg_getb()
 *
 *	Invoke the PCI BIOS to read a byte from a PCI configuration
 *	space register.
 */
static ushort
pci_cfg_getw(
	int	bus,
	int	dev,
	int	func,
	int	reg
)
{
	ic.intval = 0x1a;
	ic.ax = (PCI_FUNCTION_ID << 8) | PCI_READ_CONFIG_WORD;
	ic.bx = ((bus & 0xff) << 8) | ((dev & 0x1f) << 3) | (func & 0x7);
	ic.di = reg & 0xff;

	(void) doint();

	/* ah (the high byte of ax) must be zero */
	if ((ic.ax & 0xff00) != 0)
		return (0xffff);

	return (ic.cx);

}

/*
 * Macros to read specific registers from PCI configuration space
 * using the above two functions.
 */

/* get the secondary bus number of the PPB */
#define	PPB_BUS_SECONDARY(bus, dev, func)	\
    pci_cfg_getb(bus, dev, func, PCI_BCNF_SECBUS)

/* get the subordinate bus number of the PPB */
#define	PPB_BUS_SUB(bus, dev, func)	\
    pci_cfg_getb(bus, dev, func, PCI_BCNF_SUBBUS)

/* get the device's Vendor ID */
#define	PPB_VENDID(bus, dev, func)	\
    pci_cfg_getw(bus, dev, func, PCI_CONF_VENID)

/* get the device's Device ID */
#define	PPB_DEVID(bus, dev, func)	\
    pci_cfg_getw(bus, dev, func, PCI_CONF_DEVID)


/*
 * ppb_append()
 *
 *	Append the name of the next PCI to PCI Bridge (PPB) to the output
 *	buffer. If the boot device is behind several PPBs, then this routine
 *	will be invoked once for each level of PPB.
 */

static int
ppb_append(
	char	*bp,
	int	cur_size,
	int	bus,
	int	dev,
	int	func
)
{
	int	cnt;
	static	char	buf[] = "/pciVVVV,DDDD@dd,ff";

	(void) sprintf(buf, "/pci%x,%x@%x", PPB_VENDID(bus, dev, func),
	    PPB_DEVID(bus, dev, func), dev);
	cnt = strlen(buf);

	/* make certain it will fit before appending it to the end */
	if (cnt >= cur_size) {
		return (-1);
	}
	(void) strcat(bp, buf);
	cur_size -= cnt;

	/* add the function number if necessary */
	if (func != 0) {
		(void) sprintf(buf, ",%x", func);
		cnt = strlen(buf);
		/* make certain it will fit before appending it to the end */
		if (cnt >= cur_size)
			return (-1);
		(void) strcat(bp, buf);
		cur_size -= cnt;
	}
	return (cur_size);
}

/*
 * ppb_class()
 *
 *	Check the base class and sub class to see if it's a PPB.
 */

static int
ppb_class(
	int	bus,
	int	dev,
	int	func
)
{
	ushort	vid = pci_cfg_getw(bus, dev, func, PCI_CONF_VENID);

	if (vid == 0xffff || vid == 0)
		return (FALSE);

	if (pci_cfg_getb(bus, dev, func, PCI_CONF_BASCLASS)
			!= PCI_CLASS_BRIDGE)
		return (FALSE);

	if (pci_cfg_getb(bus, dev, func, PCI_CONF_SUBCLASS)
			!=  PCI_BRIDGE_PCI)
		return (FALSE);

	return (TRUE);
}


/*
 * ppb_scan_bus()
 *
 *	Scan the current bus (specified by *next_bus on input) to find a
 *	PPB which is configured to handle the bus specified by match_bus.
 *
 *	This routine returns:
 *
 *	TRUE && *cur_size != -1
 *		found the next PPB on the path to match_bus, *bp has the
 *		name of the PPB appended to in, *next_bus is updated with
 *		the bus number to start the scan of the next level of
 *		hierarchical bus, and *cur_size is decremented
 *
 *	TRUE && *cur_size == -1
 *		the complete pathname won't fit in the output buffer
 *
 *	FALSE	there's no path from the current bus to match_bus
 */

int
ppb_scan_bus(
	char	*bp,		/* output buffer */
	int	*cur_size,	/* size of the output buffer */
	int	*next_bus,	/* next bus to scan */
	int	match_bus	/* bus number to scan for */
)
{
	int	cur_bus = *next_bus;
	int	max_bus = *next_bus;
	int	cur_dev;
	int	cur_func;
	int	sub_bus;
	int	sec_bus;

	for (cur_dev = 0; cur_dev < 32; cur_dev++) {
		for (cur_func = 0; cur_func < 8; cur_func++) {

			if (!ppb_class(cur_bus, cur_dev, cur_func)) {
				continue;
			}

			/* skip over uninitialized PPBs */
			sec_bus = PPB_BUS_SECONDARY(cur_bus, cur_dev, cur_func);
			sub_bus = PPB_BUS_SUB(cur_bus, cur_dev, cur_func);
			if (sec_bus == 0 || sub_bus == 0) {
				continue;
			}

			/*
			 * Remember the highest subordinate bus number
			 * below the current bus.
			 */
			if (max_bus < sub_bus)
				max_bus = sub_bus;

			if (sec_bus <= match_bus && match_bus <= sub_bus) {
				goto foundit;
			}
		}
	}

	/*
	 * None of the PPBs on this bus are what I'm looking for
	 * so skip directly to the next peer bus.
	 */
	*next_bus = max_bus;
	return (FALSE);

foundit:
	/*
	 * Found a PPB. Add its name to the bootpath and start
	 * the scan over at the first device on its secondary bus.
	 */
	*cur_size = ppb_append(bp, *cur_size, cur_bus, cur_dev, cur_func);
	*next_bus = sec_bus;
	return (TRUE);
}


/*
 * ppb_path()
 *
 *	Loops until the specified match_bus has been found. It looks
 *	behind the appropriate PPBs or all the peer PCI bus controllers
 */

static int
ppb_path(
	char	*bp,		/* output buffer */
	int	osize,		/* size of the output buffer */
	int	match_bus	/* bus number to scan for */
)
{
	int	peer_pci_controller = 0;
	int	next_pci_bus = 0;
	int	cur_size;

	(void) sprintf(bp, "/pci@%d,0", peer_pci_controller);
	cur_size = osize - strlen(bp) - 1;

	while (next_pci_bus < match_bus) {
		if (ppb_scan_bus(bp, &cur_size, &next_pci_bus, match_bus)) {
			/* check for buffer full condition */
			if (cur_size == -1)
				return (FALSE);

			/*
			 * Step thru the PPB and continue the search.
			 * At this point the value of next_pci_bus has already
			 * been set to the secondary bus number of the PPB.
			 */
			continue;
		}

		/* exhausted all the PPBs, try the next peer PCI bus */

		peer_pci_controller++;
		(void) sprintf(bp, "/pci@%d,0", peer_pci_controller);
		cur_size = osize - strlen(bp) - 1;

		/*
		 * next_pci_bus was set to the highest PPB subordinate bus
		 * on the prior peer bus, increment by one to start
		 * search the next peer PCI bus
		 */
		next_pci_bus++;
	}
	return (next_pci_bus == match_bus);
}


/*
 * ppb_setup_path()
 *
 *	Setup to scan all the PPBs and peer PCI	bridge controllers to
 *	determine the path to the specified pci bus number.
 */

static char *
ppb_setup_path(
	char	*bp,		/* output buffer */
	int	osize,		/* size of the output buffer */
	int	bus		/* bus number to scan for */
)
{
	if (!ppb_path(bp, osize, bus)) {
		/* shouldn't happen, but use a reasonable default */
		(void) sprintf(bp, "/pci@0,0");
	}
	return (bp + strlen(bp));
}


/*
 * propcat()
 *
 *	Get the value of a property and append it to a string.
 */

static void
propcat(
	char	*bufp,
	char	*prop
)
{
	/* append a property string to the end of bufp */
	(void) bgetprop(bop, prop, bufp + strlen(bufp), 0, 0);
}

void
compatboot_bootpath(char *bootpath)
{
	char	*bp = bootpath;

	*bp = '\0';

	if (compatboot_ip->F8.version == 1 &&
	    compatboot_ip->F8.user_bootpath[0] == '/') {
		/* absolute user_bootpath from realmode driver */
		(void) strcpy(bp, compatboot_ip->F8.user_bootpath);
		return;
	}

	/*
	 * Step 1 - determine the bus type prefix and the pathname to
	 *	    boot device controller.
	 */
	if (compatboot_ip->F8.version == 1 && compatboot_ip->F8.pci_valid) {
		/*
		 * Bug 1213648
		 *	Support for booting a PCI device not on
		 *	bus zero, and peer PCI buses. The pathname
		 *	may require several levels due to PCI to PCI
		 *	Bridge devices (PPBs).
		 */
		bp = ppb_setup_path(bootpath, sizeof (bootpath),
				    compatboot_ip->F8.pci_bus);

		/*
		 * Bug 1188262 - Support for pci and user defined
		 * boot paths.
		 */
		(void) sprintf(bp, "/pci%x,%x@%x",
			compatboot_ip->F8.pci_ven_id,
			compatboot_ip->F8.pci_vdev_id,
			compatboot_ip->F8.pci_dev);

		if (compatboot_ip->F8.pci_func != 0) {
			bp += strlen(bp);
			(void) sprintf(bp, ",%x", compatboot_ip->F8.pci_func);
		}
		bp += strlen(bp);
	} else {
		/*
		 * It's not PCI, it must be one of the single level
		 * bus types using the original realmode interface.
		 *
		 * Incrementally build the boot-path string, based on
		 * the bus-type and controller type information previously
		 * collected.
		 */
		(void) strcat(bp, "/");
		propcat(bp, "bus-type");
		(void) strcat(bp, "/");

		/*
		 * Here we have to distinguish among the many different boot
		 * hardware, currently disk devices and network cards
		 */
		if (compatboot_ip->F8.dev_type == MDB_NET_CARD) {
			/* network device */
			propcat(bp, "b.net-mfg");
			(void) strcat(bp, "@");
			propcat(bp, "b.net-ioaddr");
			(void) strcat(bp, ",");
			propcat(bp, "b.net-membase");
		} else {
			/* disk device */
			propcat(bp, "b.hba-mfg");
			(void) strcat(bp, "@");
			propcat(bp, "b.hba-ioaddr");
			(void) strcat(bp, ",0");
		}
	}

	/*
	 * Step 2 - add the relative portion of the pathname, if any.
	 *	This is primarily for SCSI HBA's which support multiple
	 *	channels (e.g., the MLX adapter). This "middle" portion
	 *	of the pathname may designate the channel on the adapter.
	 */
	if (compatboot_ip->F8.version == 1 &&
	    compatboot_ip->F8.user_bootpath[0]) {
		/* relative user_bootpath from realmode driver */
		bp += strlen(bp);
		(void) sprintf(bp, "/%s", compatboot_ip->F8.user_bootpath);
	}

	/*
	 * Step 3 - add the partition suffix, if any.
	 */
	if (compatboot_ip->F8.dev_type == MDB_SCSI_HBA) {
		bp += strlen(bp);
		(void) sprintf(bp, "/cmdk@%d,%d:%c",
			compatboot_ip->F8.MDBdev.scsi.targ,
			compatboot_ip->F8.MDBdev.scsi.lun,
			(char)compatboot_ip->bootfrom.ufs.root_slice + 'a');
	}
}
