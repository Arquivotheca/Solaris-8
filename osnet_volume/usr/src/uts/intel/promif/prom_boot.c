/*
 * Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_boot.c	1.16	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>
#include <sys/obpdefs.h>

#ifdef I386BOOT
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/bootconf.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>

extern struct bootops *bop;
#endif

#define	OBP_MAXPATHLEN	256

extern dnode_t prom_chosennode(void);
extern dnode_t prom_rootnode(void);
extern int prom_bounded_getprop(dnode_t nodeid, caddr_t name, caddr_t value,
	int len);

char *
prom_bootargs(void)
{
#ifdef I386BOOT
	/* Get this from primary real mode boot */
	return ("kernel/unix");
#endif

#ifdef	KADB
	return ((char *)0);
#endif

#if !defined(KADB) && !defined(I386BOOT)
	int length;
	dnode_t node;
	static char *name = "bootargs";
	static char bootargs[OBP_MAXPATHLEN];

	if (bootargs[0] != (char)0)
		return (bootargs);

	node = prom_chosennode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE))
		node = prom_rootnode();
	length = prom_getproplen(node, name);
	if ((length == -1) || (length == 0))
		return (NULL);
	if (length > OBP_MAXPATHLEN)
		length = OBP_MAXPATHLEN - 1;	/* Null terminator */
	(void) prom_bounded_getprop(node, name, bootargs, length);
	return (bootargs);
#endif
}


#ifdef I386BOOT

static char bootpath[256];
extern struct pri_to_secboot *realp;

extern	struct int_pb ic;

#define	PCI_FUNCTION_ID		0xb1

#define	PCI_READ_CONFIG_BYTE	0x8
#define	PCI_READ_CONFIG_WORD	0x9

#define	PCI_RC_SUCCESSFUL	0


/*
 * pci_cfg_getb()
 *
 *	Invoke the PCI BIOS to read a byte from a PCI configuration
 *	space register.
 */

static uchar_t
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

static ushort_t
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
	ushort_t vid = pci_cfg_getw(bus, dev, func, PCI_CONF_VENID);

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
propcat(char *bufp, char *prop)
{
	/* append a property string to the end of bufp */
	(void) bgetprop(bop, prop, bufp + strlen(bufp));
}


char *
prom_bootpath(void)
{
	char	*bp = bootpath;

	*bp = '\0';

	/*
	 * Boot-path assignment mechanism:
	 * If we booted from an extended MDB device, i.e., one whose device code
	 * is other than 0x80, then we assume that we are probably installing
	 * a new system.  Therefore, no boot-path information exists in the
	 * /etc/bootrc file.  In this case we generate the boot path.
	 *
	 * If the boot device code is 0x80, then we assume that we are running
	 * a configured system.  We do not generate a boot path, because a
	 * default value will either be coming from the /etc/bootrc file,
	 * or we give users the opportunity to override the default at the
	 * boot shell command line.
	 */
	if (!realp ||
	    realp->bootfrom.ufs.boot_dev == 0 ||
	    realp->bootfrom.ufs.boot_dev == 0x80) {

	/*
	 * We only have enough information to construct a bootpath if we booted
	 * off an extended MDB device.  So, if we booted from a non-MDB device,
	 * return a null string; the system will have to get the bootpath from
	 * the /etc/bootrc file, or the user will have to type it in at the
	 * boot shell command line.
	 */

		(void) bsetprop(bop, "boot-path", bp);
		return (bp);
	}

	if (realp->F8.version == 1 && realp->F8.user_bootpath[0] == '/') {
		/* absolute user_bootpath from realmode driver */
		(void) strcpy(bp, realp->F8.user_bootpath);
		(void) bsetprop(bop, "boot-path", bp);
		return (bp);
	}

	/*
	 * Step 1 - determine the bus type prefix and the pathname to
	 *	    boot device controller.
	 */
	if (realp->F8.version == 1 && realp->F8.pci_valid) {
		/*
		 * Bug 1213648
		 *	Support for booting a PCI device not on
		 *	bus zero, and peer PCI buses. The pathname
		 *	may require several levels due to PCI to PCI
		 *	Bridge devices (PPBs).
		 */
		bp = ppb_setup_path(bootpath, sizeof (bootpath),
				    realp->F8.pci_bus);

		/*
		 * Bug 1188262 - Support for pci and user defined
		 * boot paths.
		 */
		(void) sprintf(bp, "/pci%x,%x@%x",
			realp->F8.pci_ven_id,
			realp->F8.pci_vdev_id,
			realp->F8.pci_dev);

		if (realp->F8.pci_func != 0) {
			bp += strlen(bp);
			(void) sprintf(bp, ",%x", realp->F8.pci_func);
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
		if (realp->F8.dev_type == MDB_NET_CARD) {
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
	if (realp->F8.version == 1 && realp->F8.user_bootpath[0]) {
		/* relative user_bootpath from realmode driver */
		bp += strlen(bp);
		(void) sprintf(bp, "/%s", realp->F8.user_bootpath);
	}

	/*
	 * Step 3 - add the partition suffix, if any.
	 */
	if (realp->F8.dev_type == MDB_SCSI_HBA) {
		bp += strlen(bp);
		(void) sprintf(bp, "/cmdk@%d,%d:%c",
			realp->F8.MDBdev.scsi.targ,
			realp->F8.MDBdev.scsi.lun,
			(char)realp->bootfrom.ufs.root_slice + 'a');
	}
	(void) bsetprop(bop, "boot-path", bootpath);
	return (bootpath);
}

#else

char *
prom_bootpath(void)
{
	return ((char *)0);
}

#endif
