/*
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)network.c	1.23	98/06/04 SMI\n"

/*
 * This file contains the HBA-independent C code for the BIOS extension
 * for SCSI devices.
 *
 * Comments throughout this file indicate which parts of the code are
 * specific to SCSI devices.  Removing these parts yields a BEF module
 * framework for developing BEF modules for non-SCSI devices.  This
 * file therefore serves as both an example and a template.  The
 * comments are based on the following assumptions:
 *
 *	all device-specific code will define "ident" to identify themselves.
 *
 *	all device-specific code will supply a routine dev_find
 *	for detecting the presence of any supported hardware.
 *
 *	all device-specific code will use a table of dev_info
 *	structures called dev_info.  The contents of the structure
 *	may vary for different devices but will always contain the
 *	fields used in the generic code below.
 *
 *	all device-specific code will define variables myfirstdev,
 *	mylastdev and devs.  myfirstdev and mylastdev refer to the
 *	first and last BEF device codes allocated to this module.
 *	devs is the number of devices supported.
 *
 * Routines before initmain() form part of the resident portion of
 * the driver and may be called during initialization and from the
 * resident driver.  The resident driver code is entered via resmain().
 *
 * The initialization code will be called via initmain() from the low
 * level support code.  The return value from initmain() will be returned
 * to the BIOS extension monitor.
 *
 * Revision 1.3  1992/12/04  12:22:53  alanka
 * This fixes the case that the boot floppy has SMC/WD listed as the
 * first choice, so the passing of parameters to the smc.com driver is
 * not correct, and the smc.com driver starts looking at the system to
 * find out that the smc card does not have a boot PROM.  It will use
 * that card incorrectly.
 *
 * This also fixes bug#3000597 to display the I/O base address and IRQ
 * values of the SMC card.
 *
 * This also fixes the problem that SMC.BEF must go on the floppy last.
 * Now this can go on the floppy anywhere in the directory structure.
 * The cause was due to uninitialized data in the program that does
 * not make it when the program is being relocated in memory when the
 * BIOS extension is being installed.
 *
 *
 */

#include "types.h"
#include "bef.h"
#include "dev_info.h"
#include "common.h"
#include "befext.h"
#include <stdio.h>

/*
 * Begin Debug Section
 */
/* #define DEBUG /* */
#ifdef DEBUG
#define	DPrintf(f, x) if (debug_network & f) printf x; else
dump_regs(ax, bx, cx, dx, es)
{
	printf("ax %02x, bx %02x, cx %02x, dx %02x, es %02x\n",
		ax, bx, cx, dx, es);
}
#else
#define	dump_regs(ax, bx, cx, dx, es)
#define	DPrintf(f, x)
#endif
#define	DBG_OFF		0x0000	/* ... codes in, but turned off */
#define	DBG_ERRS	0x0001	/* ... error messages */
#define	DBG_GEN		0x0002	/* ... general debug code */
#define	DBG_INIT	0x0004	/* ... code used during initialization */
#define	DBG_ALL		0xffff	/* ... let her rip. */
int debug_network = DBG_OFF;
/*
 * End Debug Section
 */

static int	is_installed(char far *, int far *);
extern char	ident[];
extern int	_end[];

#define	MAXDEVS 16

int		myfirstdev,
		mylastdev;
long		oldvect;
DEV_INFO	dev_info[MAXDEVS + 1];
ushort		devs;

char endstr[] = "END";

resmain(ax, bx, cx, dx, si, di, es, ds)
unsigned short ax, bx, cx, dx, si, di, es, ds;
{
	unchar req_dev = (dx & 0xFF);
	DEV_INFO *info;
	ushort dev;
	union halves un;
#define	blocknum un.l
#define	blocklo  un.s[0]
#define	blockhi  un.s[1]
	ushort blockoff;
	ushort buffer;
	ushort toread;
	ushort numblocks;
	int ret;
	extern int (*putvec)(int);

	/* -- make sure prints go through putc routine and not callback -- */
	putvec = 0;

	DPrintf(DBG_GEN, ("In %s handler, request 0x%x, device 0x%x\n",
		ax >> 8, req_dev));

	if (req_dev < myfirstdev || req_dev > mylastdev) {
		DPrintf(DBG_GEN, (": passing on\r\n"));
		return (0);
	}

	/* Find the dev_info slot for req_dev */
	for (dev = 0, info = dev_info; dev < devs; dev++, info++) {
		if (info->bios_dev == req_dev) {
			DPrintf(DBG_GEN, ("Device 0x%x, code 0x%x, port 0x%x\n",
				dev, info->bios_dev, info->base_port));
			break;
		}
	}
	if (dev == devs) {
		ax = 0xBB00;    /* Return "undefined error" for status */
		return (-1);
	}

	switch (ax >> 8) {
	case BEF_IDENT:
		/*
		 * ### For now return the vendor/product string in es:bx and
		 * the controller string in es:cx.
		 * Future version will probably return pointers to two
		 * structures.
		 * DX returns a magic word because we seem not to be able
		 * to rely on all BIOSes to set the carry flag for calls
		 * to unsupported devices.
		 */
		bx = (unsigned short)info;
		cx = (unsigned short)ident;
		dx = BEF_MAGIC;
		es = myds();
		return (1);

	case BEF_READ:
		DPrintf(DBG_GEN, ("Network read:"));
		dump_regs(ax, bx, cx, dx, es);

		/*
		 * reject anything except an attempt to read from
		 * the "first sector" of the network.
		 */
		if (cx != 1 || (dx & 0xFF00) != 0) {
			ax = (NO_SUCH_BLOCK << 8);
			return (-1);
		}

		/*
		 * dev_netboot attempts to boot over the network using
		 * the specified device.  It does not return unless the
		 * boot attempt fails.
		 */
		dev_netboot(info->MDBdev.net.index, info->base_port,
			info->MDBdev.net.irq_level,
			info->MDBdev.net.mem_base, info->MDBdev.net.mem_size,
			info->bios_dev, info);

		ax = 0xBB00;    /* Return "undefined error" for status */
		return (-1);

	case BEF_GETPARMS:
		/* ### Should set number of cylinders for real device */
		dx = 0x3F01;   /* Allow head 0-63, 1 drive */
		cx = 0xFFE0;   /* Allow cyl 0-1023, sector 1-32 */
		return (1);

	case BEF_RESET:
		DPrintf(DBG_GEN, ("Network init: "));
		dump_regs(ax, bx, cx, dx, es);
		dev_init(info);

		ax = 0;
		return (1);

	}
	ax = 0x100;	/* Return "bad command" for status */
	return (-1);
}

/*
 * check_install() will return 1 if this device has already
 * been installed and will print a message. Otherwise it will
 * return 0.
 */
check_install()
{
	/*
	 * is_installed will return TRUE if there is already a device
	 * installed with the given "ident" string, otherwise FALSE.
	 * If TRUE, myfirstdev will be set to the first device number
	 * supported by the installed driver.  Otherwise it is set to
	 * the next available device number.
	 *
	 */
	if (is_installed(ident, &myfirstdev)) {
		printf("%s BIOS extension already installed as device 0x%x\n",
			ident, myfirstdev);
		return (1);
	} else
		return (0);
}

/*
 * dispatch
 * Boot Hill Functions. This is the new .bef interface
 * section for the Boot Hill project.
 */

int	MDB_loaded;	/* Global to flag driver was loaded by MDB */

dispatch(int func)
{
	int Rtn;

	if (check_install())
		Rtn = BEF_FAIL;
	else
		switch (func) {
		default:
			Rtn = BEF_FAIL;
			break;

		case BEF_PROBEINSTAL:
			/* ---- Old driver interface ---- */
			MDB_loaded = 1;
			Rtn = initmain();
			break;

		case BEF_LEGACYPROBE:
			MDB_loaded = 0;
			legacyprobe();
			Rtn = BEF_OK;
			break;

		case BEF_INSTALLONLY:
			MDB_loaded = 0;
			if (installonly() == BEF_OK)
				/* ---- shared stuff with initmain() ---- */
				Rtn = common_install();
			else
				Rtn = BEF_FAIL;
			break;
	}

	return (Rtn);
}

initmain()
{
	DPrintf(DBG_INIT, ("In initmain() of %s\n", ident));

	/* Determine whether my device is present */
	dev_find();
	if (devs == 0) {
		printf("No %s network cards found.\n", ident);
		printf("%s BIOS extension not install\n\n", ident);
		return (0);
	}
	printf("%s BIOS extension installed\n", ident);
	return (common_install());
}

/*
 * common_install -- as the name implies this routine is
 * shared between initmain (the old .bef interface) and
 * bh_install (the new Boot Hill interface for .befs).
 */
int
common_install()
{
	static unsigned short codesize, datasize, totalsize;
	static unsigned short codeseg, dataseg;
	extern newvect();

	/* Read the int13 vector into oldvect */
	getvec(0x13, &oldvect);

	/*
	 * We no longer relocate the .bef if loaded under Bootconf.
	 * just nstall ourself and return success instead.
	 */
	if (!MDB_loaded) {
		/* Set up for finding data from code */
		hidata(myds());
		/* Intercept int13 calls */
		setvec(0x13, newvect, mycs());
		return (mylastdev);
	}

	/* Calculate resident code size in bytes */
	codesize = ((myds() - mycs()) + 1) << 4;

	/* Calculate resident data size in bytes */
	datasize = (unsigned short)_end;

	DPrintf(DBG_INIT, ("code size 0x%x, data size 0x%x\n",
		codesize, datasize));

	/* Calculate total resident size in K */
	totalsize = (((codesize + 15) >> 4) + ((datasize + 15) >> 4) + 63) >> 6;

	/* ### Change reserve() to allow failure, test here */
	codeseg = reserve(totalsize);
	dataseg = codeseg + ((codesize + 15) >> 4);

	/* Set up for finding hi data from hi code */
	hidata(dataseg);

	/* Copy code and data parts into high memory */
	bcopy(MK_FP(codeseg, 0),  MK_FP(mycs(), 0), codesize);
	bcopy(MK_FP(dataseg, 0),  MK_FP(myds(), 0), datasize);

	/* Intercept int13 calls */
	setvec(0x13, newvect, codeseg);

	/* Return last device number to extended BIOS monitor */
	return (mylastdev);
}

/*
 * is_install -- Interrogate all installed BIOS extensions to determine
 * whether I am already installed.  Set *devp to device code for extension if
 * it is already present, otherwise next available device.  Return
 * 1 if device is present, 0 otherwise.
 */
static int
is_installed(char far *ident, int far *devp)
{
	int	dev;
	char	far *contstr,
		far *devstr,
		far *s1,
		*s2;

	for (dev = FIRSTDEV; dev != STOPDEV; dev++) {
		if (SKIPDEV(dev))
			continue;
		if (bef_ident(dev, &contstr, &devstr))
			break;
		for (s1 = contstr, s2 = ident; *s1 == *s2; s1++, s2++) {
			if (*s2 == 0) {
				break;
			}
		}
		if (*s1 || *s2) {
			continue;
		}
		*devp = dev;
		return (1);
	}
	*devp = dev;
	return (0);
}

void
net_dev(short index, ushort base_port, ushort irq_level,
	ushort base_seg, ushort mem_size)
{
	int i;
	unchar newdev;
	void strcpy();

	/*
	 * Start filling in details about device.  Details will be
	 * discarded unless the end of this routine is reached and
	 * "devs" incremented.
	 */
	dev_info[devs].dev_type = MDB_NET_CARD;
	dev_info[devs].version = 1;
	for (i = 0; i < 8; i++) {
		dev_info[devs].vid[i] = ident[i];
	}
	{
		static	char hexdigits[] = "0123456789ABCDEF";
		int	index = 0;
		int	ii;

		dev_info[devs].blank1 = ' ';
		dev_info[devs].pid[index++] = 'I';
		dev_info[devs].pid[index++] = '/';
		dev_info[devs].pid[index++] = 'O';
		dev_info[devs].pid[index++] = '=';
		if (base_port >= 0x1000) {
			ii = (base_port & 0xf000) >> 12;
			dev_info[devs].pid[index++] = hexdigits[ii];
		}
		ii = (base_port & 0x0f00) >> 8;
		dev_info[devs].pid[index++] = hexdigits[ii];
		ii = (base_port & 0x00f0) >> 4;
		dev_info[devs].pid[index++] = hexdigits[ii];
		ii = base_port & 0x000f;
		dev_info[devs].pid[index++] = hexdigits[ii];
		dev_info[devs].pid[index++] = ' ';
		dev_info[devs].pid[index++] = 'I';
		dev_info[devs].pid[index++] = 'R';
		dev_info[devs].pid[index++] = 'Q';
		dev_info[devs].pid[index++] = '=';
		if ((irq_level/10) > 0) {
			dev_info[devs].pid[index++] =
				(char)((irq_level/10)+'0');
			dev_info[devs].pid[index++] = (char)(irq_level-10+'0');
			dev_info[devs].pid[index++] = '\0';
		} else {
			dev_info[devs].pid[index++] = (char)(irq_level+'0');
			dev_info[devs].pid[index++] = '\0';
		}
	}

	dev_info[devs].base_port = base_port;
	dev_info[devs].MDBdev.net.index = index;
	dev_info[devs].MDBdev.net.irq_level = irq_level;
	dev_info[devs].MDBdev.net.mem_base = base_seg;
	dev_info[devs].MDBdev.net.mem_size = mem_size;

	/* Allocate BIOS code and advance next device pointer */
	if (devs < MAXDEVS) {

		if (mylastdev == 0) {
			newdev = myfirstdev;
		} else {
			newdev = mylastdev + 1;
			while (SKIPDEV(newdev)) {
				newdev++;
			}
		}
		if (newdev == STOPDEV) {
			printf(".  No BIOS device code available.\r\n");
			return;
		}
		mylastdev = newdev;
		dev_info[devs].bios_dev = mylastdev;

		devs++;
	}
}

void
net_dev_pci(unchar bus, ushort ven_id, ushort vdev_id, unchar dev, unchar func)
{
	dev_info[devs].pci_valid = 1;
	dev_info[devs].pci_bus = bus;
	dev_info[devs].pci_ven_id = ven_id;
	dev_info[devs].pci_vdev_id = vdev_id;
	dev_info[devs].pci_dev = dev;
	dev_info[devs].pci_func = func;
}

#ifdef notdef
int
net_dev_bootpath(char *path)
{
	char *s;
	int i = 0;

	/*
	 * copy string
	 */
	s = dev_info[devs].user_bootpath;
	do {
		if (++i == MAX_BOOTPATH_LEN) {
			printf("Max bootpath length exceeded\r\n");
			return (1);
		}
	} while (*s++ = *path++);
	return (0); /* successful */
}
#endif
