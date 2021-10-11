/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * boot.c -- routines to handle "boot solaris" menus
 */

#ident	"@(#)boot.c	1.198	99/11/08 SMI"

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include <names.h>
#include <biosmap.h>
#include <dostypes.h>
#include <dos.h>
#include <conio.h>
#include <time.h>

#include "menu.h"
#include "adv.h"
#include "boot.h"
#include "befinst.h"
#include "biosprim.h"
#include "boards.h"
#include "bop.h"
#include "bus.h"
#include "cfname.h"
#include <dev_info.h>
#include "devdb.h"
#include "debug.h"
#include "eisa.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "isa1275.h"
#include "main.h"
#include "menu.h"
#include "mount.h"
#include "pci.h"
#include "pci1275.h"
#include "probe.h"
#include "prop.h"
#include "spmalloc.h"
#include "tree.h"
#include "tty.h"
#include "ur.h"
#include "version.h"

/*
 * Module data
 */

/*
 * options for "boot Solaris" menu
 */
static struct menu_options Boot_options[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Back" },
	{ FKEY(4), MA_RETURN, "Boot Tasks" },
	{ 'h', MA_RETURN, NULL },
	{ 'n', MA_RETURN, NULL },
	{ 'c', MA_RETURN, NULL },
};

#define	NBOOT_OPTIONS (sizeof (Boot_options) / sizeof (*Boot_options))

int firstforceload = 0;
int n_boot_dev = 0;
bef_dev *bef_devs;
static int lastdev = FIRST_BOOT_DEVNUM - 1;
int dos_emul_boot = 0;
static int holdscreen = 0;
static int befoverflo = 0;
char bootpath_in[120];
int Autoboot = 0;
u_char bootpath_set = 0;
static struct bdev_info dev80;
Board *Same_bef_chain; /* by bef name */
char *boot_driver = 0;
int hide_bootpath = FALSE;

/*
 * Bus strings.
 * Index corresponds to Configuration Manager bus type which
 * is actually a bit setting, and is why the 1st entry is null.
 */
char *busen[] = {0,
		"isa",		/* bus type  0x1 */
		"eisa",		/* bus type  0x2 */
		"pci",		/* bus type  0x4 */
		"pcmcia",	/* bus type  0x8 */
		"isa",		/* bus type 0x10 (actually "pnpisa") */
		"mc",		/* bus type 0x20 */
		"i8042",	/* bus type 0x40 */
		};

int nbusen = sizeof (busen) / sizeof (char *);
/*
 * Module local prototypes
 */
void gather_devinfo();
char *get_dev_type(struct bdev_info *pdev);
int is_bootpath_dev(bef_dev *bdp);
Board *save_bef_info(Board *bp, int devid, int *bios_prim);
void copy_bdev_info(bef_dev *bdp, struct bdev_info *dip);
void save_bef_1(Board *bp);
int save_bef_2(bef_dev *bdp, int devid);
int used_port(Board *bp, bef_dev *bdp);
Board *match_bdp_to_bp(Board *bp, bef_dev *end_bdp, int devs_no);
void sanitise_devices();
void order_devices();
int install_boot_bef(bef_dev *bootdev);
int add_dev80();
int parse_target(char *s, u_char *targp, u_char *lunp, char *slicep);
int parse_slice(char *s, char *slicep);
int forceload(devtrans *dtp);
int set_stdin(void);
int check_auto_boot(void);

struct bdev_info *
get_id(int devcode)
{
	struct bdev_info *dip;

#ifdef __lint
	/*
	 * The next three lines are here only to keep lint happy. solaris
	 * lint doesn't understand anything about asm stuff.
	 */
	struct bdev_info di;
	dip = &di;
	dip->base_port = (u_short)devcode;
#else
	/*
	 * call INT13h, function F8h  to get device information
	 */
	_asm {
	push	es
	mov	dx, devcode
	mov	ah, 0F8h
	int	13h

	cmp	dx, 0BEF1h	/* check for the magic cookie! */
	jne	gi_fail		/* can't use CF - BIOS usage is inconsistent */

	mov	dx, es		/* function F8h returns ptr to data structure */
	mov	ax, bx		/* in ES:BX */
	jmp	gi_exit
gi_fail:
	xor	ax, ax		/* return null pointer if error occurs */
	mov	dx, ax
gi_exit:
	pop	es
	mov	word ptr dip, ax
	mov	word ptr dip+2, dx
	}
#endif

	return (dip);
}

/*
 * For each device we check for an entry in the device
 * database, and if its bootable (a real mode driver exists)
 * and not a network device then install the real mode driver (.bef).
 * And probe for attached disks.  After probing the bef is
 * de-installed unless it is the bios-primary .bef.
 *
 * Note network real-mode drivers are only installed when the
 * user selects them, or if they are the only bootable device.
 * For the menu just the devicedb long description is displayed.
 */
void
gather_devinfo()
{
	Board *bp;
	devtrans *dtp;
	int devid;
	int bios_prim;

	debug(D_FLOW, "gather_devinfo\n");

	iprintf_tty("\n\n");
	n_boot_dev = 0;
	lastdev = FIRST_BOOT_DEVNUM - 1;
	bios_primaryp = NULL;
	bios_boot_devp = NULL;

	if (get_bootpath()) {
		bootpath_set = parse_bootpath();
	}

	/*
	 * Driver for boot device must be installed first!
	 */
	if (boot_driver) {
		for (bp = Same_bef_chain; bp; bp = bp->beflink) {
			dtp = bp->dbentryp;
			Bef_printfs_done_tty = 0;
			if ((dtp == DB_NODB) ||
			    strcmp(boot_driver, dtp->real_driver)) {
				continue;
			}

			devid = install_bef_befinst(dtp->real_driver, bp, 1);

			if (devid > 0) {
				(void) save_bef_info(bp, devid, &bios_prim);
			}
			if (!bios_prim) {
				deinstall_bef_befinst();
			}
			break;
		}
		ASSERT(bp);
	}

	/*
	 * Now load up the other drivers
	 */
	for (bp = Same_bef_chain; bp; bp = bp->beflink) {
		if (bp->flags & BRDF_DISAB)
			continue; /* can't boot from disabled boards */
		dtp = bp->dbentryp;
		Bef_printfs_done_tty = 0;
		if (boot_driver &&
		    (strcmp(boot_driver, dtp->real_driver) == 0)) {
			continue; /* bios primary handled above */
		}
		if (forceload(dtp) || (dtp->category == DCAT_MSD)) {
			devid = install_bef_befinst(dtp->real_driver, bp, 0);
			bios_prim = 0;
			if (devid > 0) {
				bp = save_bef_info(bp, devid, &bios_prim);
			} else if (forceload(dtp)) {
				/*
				 * Skip over the next devices that are
				 * handled by the same bef. These have
				 * already been presented to the bef in
				 * install_bef_befinst().
				 */
				Board *nbp;
				devtrans *ndtp;

				do {
					nbp = bp->beflink;
					ndtp = nbp->dbentryp;
					if (strcmp(ndtp->real_driver,
					    dtp->real_driver)) {
						break;
					}
					bp = nbp;
				/*CONSTANTCONDITION*/
				} while (1);
			}
			if (!bios_prim) {
				deinstall_bef_befinst();
			}
		} else if (dtp->category == DCAT_NET) {
			save_bef_1(bp);
		}
	}
	(void) add_dev80();
}

void
init_boot()
{
	if ((_osmajor == 3) && (_osminor == 10)) {
		/*
		 * 2nd level boot dos emulation
		 */
		dos_emul_boot = 1;
	}

	bef_devs = (bef_dev *)spcl_calloc(N_BEF_DEVS, sizeof (bef_dev));

	ondoneadd(fini_boot, 0, CB_EVEN_FATAL);

	init_befinst();
	init_tree();

	Autoboot = check_auto_boot();
}

/* ARGSUSED */
void
fini_boot(void *arg, int exitcode)
{
	if ((exitcode == 1) /* ie fatal */ ||
	    (_osmajor != 3) || (_osminor != 10)) { /* real dos */
		deinstall_bef_befinst();
		_dos_setvect(0xfb, int_13_initial);
	}
}

/*
 * menu_boot -- display menus to "boot solaris"
 */
void
menu_boot(void)
{
	struct menu_list *boot_list;
	struct menu_list *choice;
	char *devname;
	bef_dev *bdp;
	int nboot_list;
	int i, fin = 0;
	int ret;

restart:
	/*
	 * Everytime this function is called we could potentially have a
	 * different list of devices (both additions and subtractions)
	 * so we have to be prepared for reinitialising everything.
	 */
	holdscreen = befoverflo = 0;
	Bef_printfs_done_tty = 0;

	sanitise_devices();

	order_devices();

	gather_devinfo();

	if (holdscreen || Bef_printfs_done_tty) {
		/*
		 *  There's an error message on the screen.  Hold the screen
		 *  until user gets a chance to look at it.
		 */
		option_menu(0, Continue_options, NC_options);
		refresh_tty(1);
	}

	write_escd();

	if (n_boot_dev == 0) {
		enter_menu("MENU_HELP_BOOT", "MENU_NO_BOOTDEV");
		return;
	}

	while (!fin) {
		make_boot_list(&boot_list, &nboot_list);
redisplay:
		/*
		 * user is ready to see boot devices menu
		 */
		switch (select_menu("MENU_HELP_BOOT", Boot_options,
		    NBOOT_OPTIONS, boot_list, nboot_list, MS_ZERO_ONE,
		    "MENU_BOOT", nboot_list)) {

		case FKEY(2):
			/* "Boot Solaris" */
			if ((choice = get_selection_menu(boot_list,
			    nboot_list)) == NULL) {
				/* user didn't pick one */
				beep_tty();
				free_boot_list(boot_list, nboot_list);
				continue;
			}

			bdp = (bef_dev *) choice->datum;
			devname = strdup(choice->string);
			free_boot_list(boot_list, nboot_list);

			if (!bdp->installed) {
				if (!Autoboot) {
					status_menu(Please_wait,
					    "MENU_LOADING", gettext("driver"));
				}
				if (install_boot_bef(bdp)) {
					free(devname);
					fin = 1;
					break;
				}
			}

			build_tree(bdp);
			output_biosprim();
			used_resources_node_ur();
			set_prop_version();
			if (set_stdin() == TRUE) {
				ret = try_mount(bdp, devname);
				free(devname);
				devname = NULL;
				if (ret) {
					set_boot_control_props();
					done(0);
				}
			} else
				enter_menu(0, "CONSOLE_CONFLICT_BOOT",
				    gettext("CONSOLE_CONFLICT_MSG"));
			if (!bdp->installed) /* really if not bios primary */
				deinstall_bef_befinst();
			if (devname)
				free(devname);
			break;

		case FKEY(3):
			/* "Go Back" */
			free_boot_list(boot_list, nboot_list);
			fin = 1;
			break;

		case FKEY(4):
			/*
			 * advanced options
			 *
			 * Must clean up entirely in case then user backs
			 * out of the boot menu or re-enters it from
			 * one of its options.
			 */
			free_boot_list(boot_list, nboot_list);
			boot_tasks_adv();
			goto restart;

		case 'h':
			/*
			 * select first disk in boot list
			 */
			for (i = 0; i < nboot_list; i++)
				boot_list[i].flags &= ~MF_SELECTED;
			for (i = 0; i < nboot_list; i++) {
				bdp = (bef_dev *) boot_list[i].datum;
				if (strncmp(bdp->dev_type, "DISK", 4) == 0) {
					boot_list[i].flags |= MF_SELECTED;
					break;
				}
			}
			if (i != nboot_list)
				goto redisplay;
			beep_tty(); /* no hard disk device in list */
			free_boot_list(boot_list, nboot_list);
			break;
		case 'n':
			/*
			 * select first net device in boot list
			 */
			for (i = 0; i < nboot_list; i++)
				boot_list[i].flags &= ~MF_SELECTED;
			for (i = 0; i < nboot_list; i++) {
				bdp = (bef_dev *) boot_list[i].datum;
				if (strncmp(bdp->dev_type, "NET", 3) == 0) {
					boot_list[i].flags |= MF_SELECTED;
					break;
				}
			}
			if (i != nboot_list)
				goto redisplay;
			beep_tty(); /* no net device in list */
			free_boot_list(boot_list, nboot_list);
			break;
		case 'c':
			/*
			 * select first net device in boot list
			 */
			for (i = 0; i < nboot_list; i++)
				boot_list[i].flags &= ~MF_SELECTED;
			for (i = 0; i < nboot_list; i++) {
				bdp = (bef_dev *) boot_list[i].datum;
				if (strncmp(bdp->dev_type, "CD", 2) == 0) {
					boot_list[i].flags |= MF_SELECTED;
					break;
				}
			}
			if (i != nboot_list)
				goto redisplay;
			beep_tty(); /* no cd device in list */
			free_boot_list(boot_list, nboot_list);
			break;
		}
	}
}

/*
 * auto_boot -- just attempt to boot from boot-path.
 * Any return from this function means we failed to boot from the
 * bootpath and so we cycle around to the boot menu
 */
void
auto_boot(void)
{
	Board *bp;
	devtrans *dtp, *save_dtp;
	int devid = 0;
	int i;
	char *prev_rmdvr;
	Board *fbp, *save_fbp = 0;
	bef_dev *bdp;
	char mnt_dev_desc[80];
	int bios_prim = 0;

	if (get_bootpath()) {
		if ((bootpath_set = parse_bootpath()) == 0) {
			return;
		}
	}

	Bef_printfs_done_tty = 0;

	sanitise_devices();

	order_devices();

	/*
	 * we need to find the first board in the Same_bef_chain list
	 * that would load the bef that handles the boot-path device.
	 */
	for (prev_rmdvr = NULL, bp = Same_bef_chain; bp != NULL;
	    bp = bp->beflink) {
		dtp = bp->dbentryp;
		if ((dtp == DB_NODB) || (*dtp->real_driver == 0)) {
			continue;
		}
		if ((prev_rmdvr == NULL) ||
		    (strcmp(dtp->real_driver, prev_rmdvr) != 0)) {
			fbp = bp; /* first board that handles this bef */
			prev_rmdvr = dtp->real_driver;
		}
		if ((*bus_ops[ffbs(bp->bustype)].is_bp_bootpath)(bp, NULL)) {
			save_fbp = fbp;
			save_dtp = dtp;
		}

		if (forceload(dtp) && firstforceload == 0) {
			Board *nbp;
			devtrans *ndtp;

			devid = install_bef_befinst(dtp->real_driver, bp, 0);
			bios_prim = 0;
			if (devid <= 0) {
				deinstall_bef_befinst();
			} else {
				(void) save_bef_info(bp, devid, &bios_prim);
				if (!bios_prim) {
					deinstall_bef_befinst();
					n_boot_dev--;
				}
			}

			/*
			 * Skip over the next devices that are
			 * handled by the same bef. These have
			 * already been presented to the bef in
			 * install_bef_befinst().
			 */
			do {
				nbp = bp->beflink;
				ndtp = nbp->dbentryp;
				if (strcmp(ndtp->real_driver,
				    dtp->real_driver)) {
					break;
				}
				bp = nbp;
			/*CONSTANTCONDITION*/
			} while (1);
		}
	}
	firstforceload++;
	if (!save_fbp) {
		if (add_dev80()) {
			return;
		}
		bdp = &bef_devs[0];
	} else {
		/*
		 * Check if bef is already installed for the bootpath device.
		 */
		for (i = 0; i < n_boot_dev; i++) {
			if ((strcmp(bef_devs[i].name,
			    save_dtp->real_driver) == 0) &&
			    bef_devs[i].installed == TRUE) {
				break;
			}
		}

		/*
		 * Not found - load it
		 */
		if (i == n_boot_dev) {
			devid = install_bef_befinst(save_dtp->real_driver,
				save_fbp, 0);
			bios_prim = 0;
			if (devid <= 0) {
				return;
			}
			(void) save_bef_info(save_fbp, devid, &bios_prim);
		}
		/*
		 * Check if a bef device is the bootpath device.
		 */
		for (i = 0; i < n_boot_dev; i++) {
			if (is_bootpath_dev(&bef_devs[i])) {
				break;
			}
		}
		if (i == n_boot_dev) {
			return;
		}
		bdp = &bef_devs[i];
	}

	write_escd();
	build_tree(bdp);
	output_biosprim();
	used_resources_node_ur();
	set_prop_version();
	if (set_stdin() == FALSE) {
		enter_menu(0, "CONSOLE_CONFLICT_REBOOT",
		    gettext("CONSOLE_CONFLICT_MSG"));
		fatal("Reboot and change console output");
	}

	/*
	 * create the mount device description string in case we
	 * get a mount error and have to print a msg
	 */
	(void) sprintf(mnt_dev_desc, "%4s: %.70s", bdp->dev_type, bdp->desc);
	if (try_mount(bdp, mnt_dev_desc)) {
		/* mounted successfully */
		set_boot_control_props();
		done(0);
		/*NOTREACHED*/
	}
	/* Unload non-bios primary device on mount failure */
	if (devid > 0 && n_boot_dev > 0 && !bios_prim) {
		deinstall_bef_befinst();
		n_boot_dev--;
	}
}

int
check_auto_boot(void)
{
	char *val;

	if (((val = read_prop(Auto_boot, "options")) != 0) &&
	    (strncmp(val, "true", 4) == 0)) {
		return (1);
	}
	return (0);
}

/*
 * check_for_keypress -- return next key if available.
 */
int
check_for_keypress()
{
	if (_kbhit()) {
		int c = getc_tty();
		if (c == '\033') { /* ESCape aborts */
			Autoboot = 0;
			iprintf_tty("\n%s",
			    gettext("** Autoboot interrupted by user **"));
		}
		return (c);		/* user requests menus */
	} else
		return (0);
}

void
flush_keyboard()
{
	int	maxKeysToFlush = 20;

	while ((maxKeysToFlush-- > 0) && _kbhit())
		(void) getc_tty();
}

/*
 * auto_boot_timeout
 *
 * Read the bootpath and print it out, then get the auto_boot-timeout
 * property, and if set repeatedly count doen the specified number
 * of seconds until it expires. Return 1 for continue autobooting
 * and 0 to drop into menus.
 */
void
auto_boot_timeout(void)
{
	char *timeout_string;
	long timeout;
	int len;

	if (!get_bootpath()) {
		Autoboot = 0;
		return; /* no bootpath, must go to menu option */
	}

	hide_bootpath = TRUE;

	iprintf_tty("\n%s\n\n", gettext("autoboot2"));
	if (timeout_string = read_prop(Auto_boot_timeout, "options")) {
		timeout = atol(timeout_string);
	} else {
		timeout = 10; /* no timeout use default */
	}

	/*
	 * Get the auto-boot-timeout property and timeout if non zero.
	 */
	if (timeout != 0) {
		time_t prev_time = time(NULL);

		/*
		 * Need to flush the keyboard because on some machines
		 * garbage characters are left in the input buffer after
		 * a reboot. It would appear that at least some of the
		 * time an escape is found which causes us to interrupt
		 * the autoboot sequence.
		 */
		flush_keyboard();

		while (timeout > 0) {
			if (timeout == 1) {
				iprintf_tty("%s     \r",
				    gettext("Press ESCape to interrupt "
				    "autoboot in 1 second."));
			} else {
				iprintf_tty("%s %ld %s    \r",
				    gettext("Press ESCape to interrupt "
				    "autoboot in"),
				    timeout, gettext("seconds."));
			}
			while (prev_time == time(NULL)) {
				if (check_for_keypress())
					return;
			}
			prev_time = time(NULL);
			timeout--;
		}
		len = strlen(gettext(
			"Press ESCape to interrupt autoboot in 1 second."));
		iprintf_tty("%*s", len+5, "");
	} else if (timeout == 0)
		/*
		 * when the timeout is set to zero at least check once
		 * to see if an escape key has been pressed which will
		 * allow the user to enter the menu system.
		 */
		(void) check_for_keypress();
	/*
	 * if timeout is set to -1 then you can't interrupt the autoboot
	 * process.
	 */
}

int
make_boot_desc(bef_dev *bdp, char *s, int show_def)
{
	unsigned short iobase;
	const char *instr, *onstr;
	char *busnum;
	int pcidev;
	int selected = 0;
	char buf[MAXLINE];
	char buf2[MAXLINE];
	Board *bp = bdp->bp;

	/*
	 * Put out boot device type
	 */
	(void) sprintf(s, "%4s: ", bdp->dev_type);

	/*
	 * Mark default boot device
	 */
	if (show_def && bootpath_set && is_bootpath_dev(bdp)) {
		selected = 1;
		strcat(s, "(*) ");
	}
	/*
	 * Format boot choice as follows:
	 * Line 1: type: target# (lun#), id info from disk
	 * Line 2: ctlr desc (chan #), bus type(bus #), (slot#) device#
	 */
	if (strcmp("NET ", bdp->dev_type) != 0) {
		if (bdp->slice != 'a') {
			/* put up slice letter only for those who want it */
			(void) sprintf(buf, gettext("Target %d:%c, "),
			    bdp->info->MDBdev.scsi.targ, bdp->slice);
		} else {
			(void) sprintf(buf, gettext("Target %d, "),
			    bdp->info->MDBdev.scsi.targ);
		}
		strcat(s, buf);
		if (bdp->info->MDBdev.scsi.lun) {
			(void) sprintf(buf, gettext("lun %d, "),
			    bdp->info->MDBdev.scsi.lun);
			strcat(s, buf);
		}
	}
	/*
	 * Put out boot device description
	 */
	(void) sprintf(buf, "%.50s", bdp->desc);
	strcat(s, buf);
	strcat(s, "\n");
	/*
	 * For disk devices put out relevant controller information
	 */
	if (strcmp("NET ", bdp->dev_type)) {
		strcat(s, gettext("      on "));
		(void) format_device_id(bp, buf, 0);
		strcat(s, strchr(buf, ':')+2);
		if (bdp->info->user_bootpath[0]) {
			if (strncmp(bdp->info->user_bootpath, "mscsi@",
				    6) == 0) {
				(void) sprintf(buf, gettext(", chan %c"),
				    bdp->info->user_bootpath[6]);
				strcat(s, buf);
			}
		}
		instr = gettext(" in ");
		onstr = gettext(" on ");
	} else {
		instr = gettext("      in ");
		onstr = gettext("      on ");
	}

	/*
	 * For all devices put out bus/device/slot information
	 */
	switch (bp->bustype) {
	case RES_BUS_PNPISA:
	case RES_BUS_ISA:
		strcat(s, onstr);
		if (Eisa)
			strcat(s, gettext("EISA bus"));
		else
			strcat(s, gettext("ISA bus"));
		iobase = primary_probe(bp)->base;
		(void) sprintf(buf, gettext(" at %x"), iobase);
		strcat(s, buf);
		break;
	case RES_BUS_EISA:
		strcat(s, instr);
		(void) sprintf(buf, gettext("EISA Slot %d"), bp->slot);
		strcat(s, buf);
		break;
	case RES_BUS_PCI:
		pcidev = bp->pci_devfunc >> FUNF_DEVSHFT;
		if (Max_bus_pci != 0) {
			(void) sprintf(buf, gettext("bus %d, "), bp->pci_busno);
			strcpy(buf2, buf);
			busnum = buf2;
		} else {
			busnum = "";
		}
		if (bp->slot == 0xff) {
			if (*busnum == '\0')
				busnum = (char *)gettext("bus ");
			(void) sprintf(buf, gettext("%sPCI %sat Dev %d"),
					    onstr, busnum, pcidev);
		} else if (bp->slot == 0) {
			(void) sprintf(buf, gettext("%sBoard PCI %sat Dev %d"),
					    onstr, busnum, pcidev);
		} else  {
			(void) sprintf(buf, gettext("%sPCI %sSlot %d"),
					    instr, busnum, bp->slot);
		}
		/*
		 * Add the function if this is a multi-function device.
		 */
		if (bp->pci_multi_func) {
			(void) sprintf(buf + strlen(buf), ", Func %d",
			    bp->pci_devfunc & FUNF_FCTNNUM);
		}
		strcat(s, buf);
		break;
	case RES_BUS_PCMCIA:
		strcat(s, instr);
		strcat(s, gettext("PCMCIA bus"));
		break;
	default:
		strcat(s, instr);
		strcat(s, gettext("Unknown bus"));
		break;
	}
	return (selected);
}

/*
 * Construct the menu list from the loaded befs
 */
void
make_boot_list(struct menu_list **boot_listp, int *nboot_listp)
{
	char *s;
	struct menu_list *mlp;
	int i;
	bef_dev *bdp;

	*nboot_listp = n_boot_dev; /* 1 line per device */

	if ((*boot_listp = spcl_calloc(n_boot_dev,
	    sizeof (struct menu_list))) == NULL) {
		MemFailure();
	}

	for (mlp = *boot_listp, i = 0; i < n_boot_dev; i++, mlp++) {
		bdp = &bef_devs[i];
		if ((s = spcl_malloc(200)) == NULL) {
			MemFailure();
		}
		if (make_boot_desc(bdp, s, 1))
			mlp->flags = MF_SELECTED;

		mlp->datum = (void *) bdp;
		mlp->string = s;
	}
}

/*
 * Get bootpath property
 */
int
get_bootpath(void)
{
	char *s;

	if (s = read_prop("bootpath", "options")) {
		/*
		 * Save string for later
		 */
		strncpy(bootpath_in, s, 120);
		if (Autoboot && hide_bootpath == FALSE) {
			iprintf_tty("%s: %s\n\n",
			    gettext("autoboot1"), bootpath_in);
		}
		hide_bootpath = FALSE;
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/*
 * Determine bus type from path and call out to
 * appropriate bus handler to validate and break down the bootpath.
 */
int
parse_bootpath()
{
	char *s = bootpath_in;
	int i;

	/*
	 * Get bus
	 * =======
	 */
	if (*s++ != '/') {
		goto bad;
	}

	for (i = 1; i < nbusen; i++) {
		if (strncmp(s, busen[i], strlen(busen[i])) == 0) {
			if ((*bus_ops[i].parse_bootpath)(bootpath_in, &s)) {
				continue;
			} else {
				/* success */
				return (1);
			}
		}
	}
bad:
	enter_menu(0, "MENU_BAD_BOOTPATH", bootpath_in);
	return (0);

}

char *
get_dev_type(struct bdev_info *pdev)
{
	if (pdev->dev_type == MDB_NET_CARD)  {
		return ("NET ");
	}

	switch (pdev->MDBdev.scsi.pdt) {
	case INQD_PDT_DA:
		return ("DISK");

	case INQD_PDT_ROM:
		return ("CD  ");
	}
	return (NULL);
}



/*
 * Check if this is the bootpath device.
 * Call a bus-specific operator to check.  Bus operator
 * should match all device address info, including slice.
 */
int
is_bootpath_dev(bef_dev *bdp)
{
	int bus = ffbs(bdp->bp->bustype);

	if ((*bus_ops[bus].is_bdp_bootpath)(bdp)) {

		/*
		 * assumes is_bdp_bootpath won't return success without
		 * bootpath_in being initialized
		 */

		if (parse_slice(bootpath_in, &bdp->slice)) {
			/* failure; assume slice a */
			bdp->slice = 'a';
		}
		return (TRUE);
	}
	return (FALSE);
}

Board *
save_bef_info(Board *bp, int devid, int *bios_prim)
{
	bef_dev *bdp;
	struct bdev_info *dip;
	int i;
	devtrans *dtp;
	int n_bdp = 0, firstbdp;
	char *dev_type;
	int firstdrive = 1;

	*bios_prim = 0;
	firstbdp = n_boot_dev;
	for (i = lastdev + 1; i <= devid; i++) {

		/*
		 * Ignore any devices that will not fit in the table.
		 */
		if (n_boot_dev >= N_BEF_DEVS) {
			if (!befoverflo++)
				debug(D_ERR, "Too many boot devices (%d)\n",
								    n_boot_dev);
			continue;
		}

		/*
		 * Ignore any devices that were within the range advertised
		 * by the driver but which the driver does not handle.  This
		 * case can happen due to bugs in realmode drivers.
		 */
		dip = get_id(i);
		if (dip == 0) {
			debug(D_ERR, "bad devinfo from device 0x%X\n", i);
			continue;
		}

		/*
		 * Ignore non-bootable devices (eg tapes)
		 */
		if ((dev_type = get_dev_type(dip)) == NULL) {
			continue;
		}

		/*
		 * Check only for the first drive, but mark all Boards
		 * if the BEF does not handle LBA, because the BEF only
		 * loads once even if there are multiple Board records
		 */
		if (firstdrive && strcmp(dev_type, "DISK") == 0) {
			Board *mbp;

			debug(D_LBA,
			    "Checking dev 0x%x's BEF for LBA support...", i);
			if (!supports_lba(i)) {
				debug(D_LBA, "no\n");
				/* do all in BEF chain; BEF only loads once */
				for (mbp = bp; mbp; mbp = mbp->beflink) {
					mbp->flags |= BRDF_NOLBA;
				}
			} else {
				debug(D_LBA, "yes\n");
			}
			firstdrive = 0;
		}

		bdp = &bef_devs[n_boot_dev];
		bdp->dev_type = dev_type;
		bdp->info_orig = dip;
		copy_bdev_info(bdp, dip);
		dtp = bp->dbentryp;
		bdp->name = dtp->real_driver;
		bdp->installed = TRUE;
		n_boot_dev++;
		n_bdp++;
	}
	bp = match_bdp_to_bp(bp, bdp, n_bdp);
	for (i = firstbdp; i < firstbdp + n_bdp; i++) {
		bdp = &bef_devs[i];
		if (check_biosdev(bdp)) {
			boot_driver = bdp->name;
			save_biosprim_buf_befext();
			*bios_prim = 1;
			lastdev = devid;
			break;
		}
	}
	for (i = firstbdp; i < firstbdp + n_bdp; i++) {
		bdp = &bef_devs[i];
		if (!*bios_prim)
			bdp->installed = FALSE;
		if (strncmp("IDE(ATA)", (char *)bdp->info->vid, 8) == 0) {
			/* the new ata driver no longer uses this feature */
			bdp->desc = &bdp->info->user_bootpath[1];
		} else {
			bdp->desc = (char *)&bdp->info->vid[0];
		}
	}
	return (bp);
}

void
copy_bdev_info(bef_dev *bdp, struct bdev_info *dip)
{
	if (!bdp->info) {
		bdp->info = malloc(sizeof (struct bdev_info));
		if (bdp->info == NULL) {
			MemFailure();
		}
	}
	memcpy(bdp->info, dip, sizeof (struct bdev_info));
}

/*
 * We don't install network befs until the user selects one
 * or its the only bootable device. This "lazy loading" increases speed
 * and, for old style befs that still probe the world, cuts down
 * on probe conflicts.
 */
static struct bdev_info di;

void
save_bef_1(Board *bp)
{
	bef_dev *bdp;
	devtrans *dtp;

	debug(D_FLOW, "Found network boot device\n");

	dtp = bp->dbentryp;
	bdp = &bef_devs[n_boot_dev];
	bdp->dev_type = "NET ";
	bdp->desc = dtp->dname;
	bdp->name = dtp->real_driver;
	bdp->installed = FALSE;
	di.dev_type = MDB_NET_CARD;
	di.user_bootpath[0] = 0;
	copy_bdev_info(bdp, &di);
	bdp->bp = bp;
	n_boot_dev++;
}

int
save_bef_2(bef_dev *bdp, int devid)
{
	struct bdev_info *dip;
	int i;
	char *dev_type;

	for (i = lastdev + 1; i <= devid; i++, bdp++) {
		if (!(dip = get_id(i))) {
			iprintf_tty("bad devinfo from device 0x%x\n", i);
			continue;
		}

		/*
		 * Ignore non-bootable devices (eg tapes)
		 */
		if ((dev_type = get_dev_type(dip)) == NULL) {
			bdp--;
			continue;
		}

		bdp->dev_type = dev_type;
		copy_bdev_info(bdp, dip);
		bdp->desc = (char *)bdp->info;
	}
	return (1);
}

#define	CHAN_OFF 6
/*
 * Returns 1 if the specified port is part of the resource
 * list of the specified board
 */
int
used_port(Board *bp, bef_dev *bdp)
{
	Resource *rp = resource_list(bp);
	u_int rc;
	u_long base_port;

	base_port = (u_long) bdp->info->base_port;

	for (rc = resource_count(bp); rc--; rp++) {
		if (RTYPE(rp) == RESF_Port) {
			/*
			 * We don't do exact matching on the base address
			 * as the eisa drivers that add the eisa offset
			 * to base address wouldn't match, and channel
			 * drivers that add the channel number would fail
			 * as well. So we just check if the base_port is in
			 * the range.
			 */
			if ((rp->base <= base_port) &&
			    (base_port < (rp->base + rp->length))) {
				return (1);
			}
		}
	}
	return (0);
}


/*
 * This function matches up the boot device structures obtained from the bef
 * to boards. There are a few possibilities:-
 *
 *	1) The io address in the bef structure (DEV_INFO) matches the
 *	   the boards first io address. This should be the usual case.
 *	   This works for converted befs (that handle the new BEF_INSTALLONLY
 *	   functionality) - in which case the DEV_INFO structs and
 *	   boards should be in order. It also works for old befs
 *	   (where we used the BEF_PROBEINSTAL functionality) and the
 *	   DEV_INFO do not necessarily line up with the boards.
 *	2) The io addresses do not correspond between the DEV_INFO
 *	   and boards. This is the case for the dpt bef that
 *	   converts motherboard bios ide registers to the EISA slot
 *	   address space (eg 1f0 becomes 1c88). Here we just assume
 *	   the first set of DEV_INFOs with the same io address map to the
 *	   the first board, and if there is a 2nd set (ie 2nd controller)
 *	   then it maps to the 2nd set, and so on. This is reasonable,
 *	   especially if the bef is converted to handle BEF_INSTALLONLY.
 *	   However, pathological cases will exist, for example suppose
 *	   there are multiple dpt controllers with some having no scsi targets.
 *	3) If we ever find more io address sets than there are boards
 *	   then we have a configuration problem (or even driver problem).
 *
 * Its probably more efficient (and makes more sense) to chain together
 * bdp entries from the controller board, but its not done that way.
 * When a match is found a pointer to the board is stored in
 * bdp. Later when building the tree we scan all boot devices for
 * matches. This was just simpler, and the no of boot devices is expected
 * to be small.
 */
Board *
match_bdp_to_bp(Board *bp, bef_dev *end_bdp, int devs_no)
{
	bef_dev *bdp;
	u_short ioport;
	u_char found;
	Board *ctlr, *nbp;
	devtrans *dtp, *ndtp;

	/*
	 * Test for case 1 above
	 */
	for (bdp = (end_bdp - devs_no + 1); bdp <= end_bdp; bdp++) {
		ctlr = bp;
		do {
			found = FALSE;
			if ((bdp->info->version > 0) &&
			    (bdp->info->pci_valid)) {
				u_char dev = ctlr->pci_devfunc >> FUNF_DEVSHFT;
				u_char func = ctlr->pci_devfunc & FUNF_FCTNNUM;

				if ((bdp->info->pci_bus == ctlr->pci_busno) &&
				    (bdp->info->pci_dev == dev) &&
				    (bdp->info->pci_func == func)) {
					bdp->bp = ctlr;
					found = TRUE;
					break;
				}
			} else if (used_port(ctlr, bdp)) {
				bdp->bp = ctlr;
				found = TRUE;
				break;
			}
			dtp = ctlr->dbentryp;
			ctlr = ctlr->beflink;
		} while (ctlr && (ndtp = ctlr->dbentryp,
		    (strcmp(dtp->real_driver, ndtp->real_driver) == 0)));
		if (!found) {
			break;
		}
	}

	if (!found) {
		/*
		 * Its not case 1 assume case 2
		 */
		debug(D_ERR, "match_bdp_to_bp: unmatched io ports\n");
		bdp = (end_bdp - devs_no + 1);
		ctlr = bp;
		do {
			bdp->bp = ctlr;
			ioport = bdp->info->base_port;
			for (bdp++; bdp <= end_bdp; bdp++) {
				/*
				 * Check if same as previous ioport
				 */
				if (bdp->info->base_port != ioport) {
					break;
				}
				bdp->bp = ctlr;
			}
			if (bdp > end_bdp) {
				/*
				 * Success, we came to the end of the bdp
				 * records
				 */
				break;
			}
			dtp = ctlr->dbentryp;
			ctlr = ctlr->beflink;
		} while (ctlr && (ctlr->dbentryp == dtp));
		if (bdp <= end_bdp) {
			fatal("Configuration error - missing controller?\n");
		}
	}

	/*
	 * Finally skip over the set of boards relating to this bef
	 */
	dtp = bp->dbentryp;
	while (((nbp = bp->beflink) != 0) && ((ndtp = nbp->dbentryp) != 0)) {
		if (strcmp(dtp->real_driver, ndtp->real_driver) != 0)
			break;
		bp = nbp;
	}
	return (bp);
}

/*
 * Constructs the solaris path from the bef device
 */
void
get_path_from_bdp(bef_dev *bdp, char *path, int compat)
{
	int bus = ffbs(bdp->bp->bustype);

	(*bus_ops[bus].get_path_from_bdp)(bdp, path, compat);
}


/*
 * Constructs a 1275 path from a board pointer.
 *
 * HOKEY!
 */
void
get_path(Board *bp, char *path)
{
	struct bdev_info	di;
	bef_dev			bef;

	di.dev_type = MDB_NET_CARD;
	di.user_bootpath[0] = 0;
	bef.info = &di;
	bef.bp = bp;

	get_path_from_bdp(&bef, path, 0);
}

/*
 * Determines the appropriate target driver to use for a device
 *
 * If there is a property named "target-driver-for-XXX" where XXX
 * matches the name of the realmode driver, return the value of
 * that property as the target driver name.
 *
 * Determine the type of device and look for a property named
 * "target-driver-for-XXX" where XXX is the type.  Devices that
 * set the MDB_MFL_DIRECT bit are assumed to be direct attach and
 * all others scsi.  Return the value of the property if found.
 *
 * Return the fixed string "cmdk" for backwards compatibility.
 */
char *
determine_scsi_target_driver(bef_dev *bdp)
{
#define	MAX_TARGET_LEN	16
	static char name_buffer[MAX_TARGET_LEN + 1];
#define	LOCAL_BUF_LEN	80
	char buffer[LOCAL_BUF_LEN];
	int n;
	char *val;

	/* Build the driver-specific property name.  Truncate if necessary. */
	strcpy(buffer, "target-driver-for-");
	n = strlen(buffer);
	strncpy(buffer + n, bdp->bp->dbentryp->real_driver,
		LOCAL_BUF_LEN - n);
	buffer[LOCAL_BUF_LEN - 1] = 0;

	/* Look up the property and return its value if found */
	val = read_prop(buffer, "options");
	if (val && val[0] && val[0] != '\n') {
		strncpy(name_buffer, val, MAX_TARGET_LEN);
		name_buffer[MAX_TARGET_LEN] = 0;
		return (name_buffer);
	}

	/* Build the driver-specific property name using unix_driver name. */
	strcpy(buffer, "target-driver-for-");
	n = strlen(buffer);
	strncpy(buffer + n, bdp->bp->dbentryp->unix_driver,
		LOCAL_BUF_LEN - n);
	buffer[LOCAL_BUF_LEN - 1] = 0;

	/* Look up the property and return its value if found */
	val = read_prop(buffer, "options");
	if (val && val[0] && val[0] != '\n') {
		strncpy(name_buffer, val, MAX_TARGET_LEN);
		name_buffer[MAX_TARGET_LEN] = 0;
		return (name_buffer);
	}

	/* Build the device type property name */
	strcpy(buffer, "target-driver-for-");
	/*
	 * XXX: the correct fix for this bug includes parsing the
	 * real_driver string for "scsi" and "direct" keywords,
	 * and set flags in the devtrans structure. Unfortunately
	 * a bootconf bug made this unworkable for 2.7 beta;
	 * for now non-now non-bef drivers default to direct.
	 */
	if (bdp->bp->dbentryp->real_driver[0] == '\0' ||
	    (bdp->info->version >= MDB_VERS_MISC_FLAGS &&
			(bdp->info->misc_flags & MDB_MFL_DIRECT))) {
		strcat(buffer, "direct");
	} else {
		strcat(buffer, "scsi");
	}

	/* Look up the property and return its value if found */
	val = read_prop(buffer, "options");
	if (val && val[0] && val[0] != '\n') {
		strncpy(name_buffer, val, MAX_TARGET_LEN);
		name_buffer[MAX_TARGET_LEN] = 0;
		return (name_buffer);
	}

	strcpy(name_buffer, "cmdk");
	return (name_buffer);
}

void
sanitise_devices()
{
	Board *bp;
	devtrans *dtp;
	char name[8];

	if (!Autoboot) {
		status_menu(Please_wait, "MENU_LOADING", gettext("drivers"));
	}

	assign_prog_probe();
#ifdef DEBUG
	for (bp = Head_board; bp != NULL; bp = bp->link) {
		dtp = bp->dbentryp;
		DecompressName(bp->devid, name);
		if (dtp) {
			debug(D_FLOW,
			    "TranslateDevice 0x%lx, eisa %s, bus %x, bef %s\n",
			    bp->devid, name, bp->bustype, dtp->real_driver);
		} else {
			debug(D_FLOW,
			    "No translation for 0x%lx, eisa %s, bus %x\n",
			    bp->devid, name, bp->bustype);
			    continue;
		}
	}
#endif
}

/*
 * Ensure devices corresponding to the same bef are sequential
 * So that bef loading and match_bdp_to_bp() work.
 */
void
order_devices()
{
	Board *bp, *tbp, *ebp;
	devtrans *dtp;
	char *bef;

	debug(D_FLOW, "Ordering devices by bef\n");

	/*
	 * Clear the bef links
	 */
	for (bp = Head_board; bp; bp = bp->link) {
		bp->beflink = 0;
	}
	Same_bef_chain = NULL;
	ebp = NULL;

	for (bp = Head_board; bp; bp = bp->link) {
		/*
		 * If already found or no mapping, move to the next.
		 */
		if ((bp->beflink) || (!bp->dbentryp)) {
			continue;
		}
		dtp = bp->dbentryp;
		bef = dtp->real_driver;
		if ((dtp == DB_NODB) || (*bef == 0)) {
			continue;
		}
		if (!forceload(dtp) && (dtp->category != DCAT_MSD) &&
		    (dtp->category != DCAT_NET)) {
			continue;
		}
		if (ebp) {
			ebp->beflink = bp;
			ebp = bp;
		} else {
			Same_bef_chain = ebp = bp;
		}
		/* mark the fact we've been found */
		ebp->beflink = (Board *) 1;

		/*
		 * Find all others related to this same bef, and
		 * link them into the chain.
		 */
		for (tbp = bp->link; tbp; tbp = tbp->link) {
			dtp = tbp->dbentryp;
			/*
			 * If no mapping or different bef then move to next
			 */
			if ((dtp == DB_NODB) || strcmp(dtp->real_driver, bef)) {
				continue;
			}

			/*
			 * Found one - link it in.
			 */
			ebp->beflink = tbp;
			ebp = tbp;
			/* mark the fact we've been found */
			ebp->beflink = (Board *) 1;
		}
	}
	if (ebp) {
		ebp->beflink = NULL; /* clear the tail link */
	}
}

/*
 * If boot bef has not yet been installed, then install it.
 */
int
install_boot_bef(bef_dev *bootdev)
{
	char *boot_bef;
	int i;
	int devid;
	bef_dev *bdp;

	boot_bef = bootdev->name;
	debug(D_FLOW, "Force load of bef %s\n", boot_bef);

	/*
	 * Now find the first bdp linked to this bef.
	 * This is so that the save_bef_2 aligns the bdp entries with those
	 * allocated before.
	 */
	if (strcmp("NET ", bootdev->dev_type)) {
		for (i = 0; i < n_boot_dev; i++) {
			bdp = &bef_devs[i];
			if (strcmp(bdp->name, boot_bef) == 0) {
				break;
			}
		}
		ASSERT(i != n_boot_dev);
	} else {
		bdp = bootdev;
	}

	if (((devid = install_bef_befinst(boot_bef, bdp->bp, 0)) <= 0) ||
	    !save_bef_2(bdp, devid)) {
		iprintf_tty("\n%s: Boot error\n", bootdev->desc);
		option_menu("MENU_HELP_NETINST", Continue_options, NC_options);
		return (1);
	}
	return (0);
}

/*
 * Option list for dealing with mapping controller without
 * a bef to device 80
 */
static struct menu_options Dev_80_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" }
};

#define	NDEV_80_OPTS (sizeof (Dev_80_opts)/sizeof (*Dev_80_opts))

/*
 * Check if we should add a device 80, and * initialise the new bef struct
 */
int
add_dev80()
{
	Board *bp;
	Resource *rp;
	bef_dev *bdp;
	devtrans *dtp;

	if (bios_primary_failure || (bios_primaryp != NULL)) {
		return (1);
	}
	for (bp = Head_board; bp; bp = bp->link) {
		dtp = bp->dbentryp;
		if ((dtp == DB_NODB) || (*dtp->real_driver != 0) ||
		    (dtp->category != DCAT_MSD)) {
			continue;
		}

		if (bootpath_set) {
			if ((*bus_ops[ffbs(bp->bustype)].is_bp_bootpath)(bp,
			    &dev80)) {
				break;
			}
		} else {
			/*
			 * In this case we assume that the first
			 * MSD device without a bef is dev 0x80 at
			 * channel 0, target 0, lun 0, slice a.
			 */
			break;
		}
	}
	if (!bp) {
		return (1);
	}


	/*
	 * Initialise the common fields of the bef struct
	 */
	dev80.version = 1;
	dev80.MDBdev.scsi.pdt = INQD_PDT_DA;
	dev80.dev_type = MDB_SCSI_HBA;
	dev80.bios_dev = 0x80;

	if (((rp = primary_probe(bp)) != 0) &&
	    (RTYPE(rp) == RESF_Port)) {
		devtrans *dtp = bp->dbentryp;
		char *driver = dtp->unix_driver;

		/*
		 * Bug fix 4025180
		 * Special case for any self-identifying devices without
		 * drivers that have a non zero eisa offset (currently only
		 * dsa). This kludge can probably be removed when the
		 * kernel dsa driver is converted to 2.6
		 */
		if (strcmp(driver, "dsa") == 0) {
			dev80.base_port = (u_short) (bp->slot << 12) + 0xc80;
		} else {
			dev80.base_port = (u_short) rp->base;
		}
	}

	bdp = &bef_devs[n_boot_dev];
	n_boot_dev++;

	bdp->dev_type = "DISK";
	bdp->desc = "Bios primary drive - device 0x80";
	bdp->installed = 1;
	bdp->bp = bp;
	bdp->info = &dev80;
	if (!bootpath_set) {
		char path[120];

		get_path_from_bdp(bdp, path, 0);
		bdp->slice = 'a';
		if (!Autoboot && (text_menu(NULL, Dev_80_opts, NDEV_80_OPTS,
		    NULL, "MENU_DEV_80", path) == FKEY(3))) {
			n_boot_dev--;
			return (1);
		}
	} else {
		/* set slice either from bootpath or default it to 'a' */
		if ((*bus_ops[ffbs(bp->bustype)].is_bp_bootpath)(bp, NULL)) {
			if (parse_slice(bootpath_in, &bdp->slice)) {
				bdp->slice = 'a';
			}
		}
	}
	(void) check_biosdev(bdp);
	return (0); /* sucessfully added */
}

/*
 * If a non network device, parse the last component
 *	[/mscsi@<channel>,0]/targetdrivername@<targ>,lun:<slice>
 *
 * Save the results in the parameters passed
 *
 * This code used to depend on the bootpath being exactly
 * "/cmdk@..." or "/mscsi@../cmdk@...". The pci-ide driver
 * doesn't use "mscsi" and we plan to eventually change
 * the scsi disk target driver. Therefore, I've changed this
 * to handle any any number of intermediate nexus drivers
 * and any target driver name. This routine now locates the
 * target and LUN numbers by searching from the last '@', rather
 * than scanning driver names from the beginning of the path.
 *
 */
int
parse_target(char *s, u_char *targp, u_char *lunp, char *slicep)
{
	char *start;

	/*
	 * Check if network device
	 */
	if (*s == 0) {
		return (0);
	}

	while (*s == '/')
		s++; /* skip over repeated leading slashes */

	/*
	 * if there's more than one level of driver left in
	 * the remaining bootpath, turn it into a "user_bootpath"
	 */
	start = s;
	if (s = strrchr(s, '/')) {
		if ((s - start) >= MAX_BOOTPATH_LEN) {
			return (1);
		}
		*s = 0;
		strcpy(dev80.user_bootpath, start);
		*s = '/';
	}

	/*
	 * the target always follows the last '@'
	 */
	if (!(s = strrchr(start, '@')))
		return (1);

	s++;
	if (*s == 0)
		return (1);

	/*
	 * Get target
	 * ==========
	 */
	*targp = (u_char) strtol(s, &s, 16);

	/*
	 * Get lun
	 * =======
	 */
	if (*s++ != ',') {
		return (1);
	}
	*lunp = (u_char) strtol(s, &s, 16);

	/*
	 * Check for slice (partition)
	 * ===========================
	 */
	if (*s++ != ':') {
		return (1);
	}

	/* save slice letter */
	*slicep = *s++;

	if (*s) {
		return (1);
	}
	return (0);
}

/*
 * parse the slice name from the end of a device path.
 */
int
parse_slice(char *s, char *slicep)
{
	char *p;
	char c;

	p = strrchr(s, ':');
	if (p == NULL)
		return (1);

	c = *(p+1);

	/* verify between 'a' and 'z' */
	if (c < 'a' || c > 'z')
		return (1);

	*slicep = c;
	return (0);
}

/*
 * Check if the $forceload property is on the database entry
 */
int
forceload(devtrans *dtp)
{
	devprop *dpp;

	for (dpp = dtp->proplist; dpp != NULL; dpp = dpp->next) {
		if (strcmp(dpp->name, "$forceload") == 0) {
			return (1);
		}
	}
	return (0);
}

int
set_stdin(void)
{
	char			*val;
	Board			*bp;
	devtrans		*dtp;
	int			port;
	char			prop_buf[STDIN_PROPLEN],
				path_buf[STDIN_PROPLEN];

	if ((val = read_prop("input-device", "options")) != 0) {
		/*
		 * XXX should we worry about the machines that can change the
		 * port address for the comm ports.
		 */

		if ((strstr(val, "ttya") != (char *)0) ||
		    (strstr(val, "com1") != (char *)0))
			port = 0x3f8;
		else if ((strstr(val, "ttyb") != (char *)0) ||
		    (strstr(val, "com2") != (char *)0))
			port = 0x2f8;
		else {
			return (TRUE);
		}
	} else {
		return (TRUE);
	}

	for (bp = Head_board; bp; bp = bp->link) {
		dtp = bp->dbentryp;

		if (dtp && (strcmp(dtp->real_driver, "com") == 0)) {
			Resource	*rp = resource_list(bp);
			int		x;

			for (x = resource_count(bp); x--; rp++) {
				if (rp->base == port)
					goto got_match;
			}
		}
	}

	return (TRUE);

got_match:
	/*
	 * Now to fudge up a bef_dev structure so that get_path_from_bdp
	 * will work
	 */

	if (weak_binding_tree(bp)) {
		return (FALSE);
	}
	get_path(bp, path_buf);
	(void) sprintf(prop_buf, "setprop stdin %s\n", path_buf);
	out_bop("cd /options\n");
	out_bop(prop_buf);
	return (TRUE);
}
