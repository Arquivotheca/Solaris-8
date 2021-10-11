/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * biosprim.c -- routines to handle bios primary/boot device
 */

#ident "@(#)biosprim.c   1.62   99/08/18 SMI"

#include "types.h"
#include <bios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <biosmap.h>
#include <dos.h>

#include "menu.h"
#include "boot.h"
#include "biosprim.h"
#include "bop.h"
#include "debug.h"
#include "devdb.h"
#include <dev_info.h>
#include "enum.h"
#include "err.h"
#include "fdisk.h"
#include "probe.h"
#include "prop.h"
#include "vtoc.h"


#define	DISK_READ_RETRIES 10
#define	DISK_WRITE_RETRIES 3
#define	BEF_GETPARMS 8

/*
 * Module data
 */
bef_dev *bios_primaryp = NULL;
bef_dev *bios_boot_devp = NULL;
int bios_primary_failure = 0;
int bios_dev_failure = 0;
u_char bios_bootdev;
u_char booted_from_eltorito_cdrom;

static char Bios_primary_1k[1024];
static char *Bios_bootdev_1k;
static char biosprim_path[MAXLINE];
static u_short bios_dev_cyls;
static u_char bios_dev_heads, bios_dev_secs;
static u_char all_bios_drives_support_lba;

/*
 * Module local prototypes
 */
static void init_biosdev_data(void);
static void mark_media_if_lba(void);
static int drive_present(u_char dev);
static int write_magic_fdisk_entry(u_char dev);
static int has_fdisk_table(struct mboot *mbp);
static int disk_is_blank(struct mboot *mbp);
static u_char read_1k(u_char dev, char *buf);
static void reset_disk(u_char dev);
static u_char read_lba(u_char dev, long relblk, int nblks, char *buf);
static u_char read_sectors(u_char dev, int cyl, int head, int sect,
	int nsect, char *buf);
static u_char write_sectors(u_char dev, int cyl, int head, int sect,
	int nsect, char *buf);
static u_char read_disk(u_char dev, long relsect, int nsect, char *buf);

#define	BIOS_MEM_SIZE bdap->RealMemSize

/* Packet used for INT13_EXTREAD/WRITE */
typedef struct dev_pkt {
	unsigned char	size;
	unsigned char	dummy1;
	unsigned char	nblks;
	unsigned char	dummy2;
	unsigned long	bufp;
	unsigned long	lba_lo;
	unsigned long	lba_hi;
	unsigned long	bigbufp_lo;
	unsigned long	bigbufp_hi;
} dev_pkt_t;

/* El Torito INT13_EMULTERM call specification packet */
typedef struct int13_emulterm_packet_tag {
	unsigned char size;
	unsigned char media_type;
	unsigned char bios_code;
	unsigned char ctlr_index;
	unsigned long image_lba;
	unsigned short dev_spec;
	unsigned short buffer_seg;
	unsigned short load_seg;
	unsigned short load_count;
	unsigned char cyl_lo;
	unsigned char sectors_plus_cyl_hi;
	unsigned char heads;
	unsigned char dummy_for_alignment_purposes;
} int13_emulterm_packet_t;

/*
 * 1) set up bios_bootdev, Bios_bootdev_1k and bios_dev_{cyls, heads, sects}
 * for later use by check_biosdev() (by init_biosdev_data())
 *
 * 2) mark fdisk tables of hard disks accessible by BIOS to indicate
 * whether their BIOS supports LBA access or not, so that later
 * Solaris drivers can read the fdisk table and know if LBA is supported
 */

void
init_biosdev()
{
	init_biosdev_data();
	mark_media_if_lba();
}

/*
 * Save away the 1st 1k of the bios primary disk and the
 * the bios boot disk.
 * We can compare against other disks later in
 * order to determine boot/primary paths.
 *
 * Note, This has to be done prior to any befs getting loaded.
 * This is because the bef that handles the boot device, thinks its the
 * only driver for that device and can set up different modes that
 * the other driver doesn't know about. For example, SCSI disconnects
 * can be set by the bef, but the bios driver wouldn't expect.
 * This happened on a test machine.
 */
static void
init_biosdev_data(void)
{
	union _REGS inregs, outregs;
	char *pp;
	int bootdev;

	bios_primary_failure = read_1k(0x80, Bios_primary_1k);
	if ((pp = read_prop("bios-boot-device", "options")) != NULL &&
			sscanf(pp, "%x", &bootdev) == 1) {
		bios_bootdev = (u_char)bootdev;
	} else {
		bios_bootdev = 0x80;
	}
	debug(D_FLOW, "init_biosdev: bios_bootdev is %x\n", bios_bootdev);
	if (bios_bootdev == 0x80) {
		bios_dev_failure = bios_primary_failure;
		Bios_bootdev_1k = Bios_primary_1k;
		if (bios_dev_failure)
			return;
	} else {
		Bios_bootdev_1k = (char *)malloc(1024);
		if (bios_dev_failure = read_1k(bios_bootdev, Bios_bootdev_1k))
			return;
	}

	/*
	 * Retrieve the bios boot device geometry before the realmode
	 * driver that will control this device is loaded.  We will
	 * write this information into the driver so that it can
	 * take over boot device functionality.  It is possible for
	 * the geometry call to fail (e.g. on an El Torito CDROM) in
	 * which case noone could have relied on the geometry so
	 * just set arbitrary numbers.
	 */
	inregs.h.ah = BEF_GETPARMS;
	inregs.h.dl = bios_bootdev;

	_int86(0x13, &inregs, &outregs);

	if (outregs.h.ah == 0) {
		bios_dev_cyls = (((u_short) outregs.h.cl & 0xc0) << 2) |
			outregs.h.ch;
		bios_dev_heads = outregs.h.dh;
		bios_dev_secs = outregs.h.cl & 0x3f;
	} else {
		bios_dev_cyls = 1023;
		bios_dev_heads = 254;
		bios_dev_secs = 63;
	}
	debug(D_FLOW, "bios boot device geom is %d:%d:%d\n",
	    bios_dev_cyls, bios_dev_heads, bios_dev_secs);
}

/*
 * Loop through all readable devices from 0x80 to HDUNIT_LIMIT
 * and try to mark all to indicate whether they support LBA or no
 */

#define	HDUNIT_LIMIT	0xFF

static void
mark_media_if_lba(void)
{
	u_char dev;

	/*
	 * First, check if all devices support lba; special case if so
	 */

	/* assume all support (should be true on new machines) */
	all_bios_drives_support_lba = 1;

	for (dev = 0x80; dev < HDUNIT_LIMIT; dev++) {
		if (drive_present(dev) && !supports_lba(dev)) {
			all_bios_drives_support_lba = 0;
			break;
		}
	}

	if (all_bios_drives_support_lba) {
		debug(D_LBA, "All drives support lba; not marking");
		return;
	}

	for (dev = 0x80; dev < HDUNIT_LIMIT; dev++) {
		if (drive_present(dev) && supports_lba(dev)) {
			if (!write_magic_fdisk_entry(dev)) {
				debug(D_LBA,
				    "can't write magic fdisk for drv 0x%x",
				    dev);
			}
		}
	}
}

/* Does the BIOS respond for device number dev? */
static int
drive_present(u_char dev)
{
	union _REGS inregs, outregs;

	inregs.h.ah = 0x8;	/* "get parms"; "reset" takes too long */
	inregs.h.dl = dev;
	_int86(0x13, &inregs, &outregs);
	if (!outregs.x.cflag && outregs.h.ah == 0) {
		debug(D_LBA, "drive 0x%x present\n", dev);
		return (TRUE);
	}

	return (FALSE);
}

/* Does the BIOS for drive dev support the LBA access functions? */
int
supports_lba(u_char dev)
{
	union _REGS inregs, outregs;

	inregs.h.ah = 0x41;
	inregs.x.bx = 0x55AA;
	inregs.x.cx = 0;
	inregs.h.dl = dev;
	_int86(0x13, &inregs, &outregs);
	if (!outregs.x.cflag && outregs.x.bx == 0xAA55 && (outregs.x.cx & 1)) {
		debug(D_LBA, "drive 0x%x supports LBA\n", dev);
		return (TRUE);
	}

	return (FALSE);
}

/*
 * Write a sentinel to the disk in the FDISK table to let Solaris driver
 * know that this drive's BIOS supports LBA, so that the Solaris driver
 * can create the lba-access-ok property.  The reason we can do this is
 * that Solaris ignores the CHS values anyway, and for partitions which
 * start or end past the 8GB boundary, the CHS values can't be correct
 * anyway...so we choose sentinels that seem to be the ones chosen by
 * Microsoft.
 *
 * 0) look for presence of fdisk or "blank" disk; if no fdisk and disk
 * doesn't appear blank, don't try to mark; we don't understand the
 * disk, and may corrupt data if we blindly write fdisk data
 *
 * 1) If marking, first look for a Solaris FDISK entry; if there is one,
 * make sure its starting and ending CHS values are the LBA_MAX values.
 *
 * 2) if there are no Solaris entries, look for a free entry; fail if
 * none available
 *
 * 3) mark a free entry with the LBA_MAX values.  This is risky, but
 * should be safe, as unallocated entries (those with systype == 0) are
 * supposed to be completely ignored....as long as this is a disk which
 * can take an fdisk table.
 */

#define	LBA_MAX_CYL	(1022 & 0xFF)
#define	LBA_MAX_HEAD	254
#define	LBA_MAX_SECT	(63 | ((1022 & 0x300) >> 2))

#define	CHS_IS_MAX(mbr, i) \
	(mbr.parts[i].begcyl == LBA_MAX_CYL && \
	mbr.parts[i].beghead == LBA_MAX_HEAD && \
	mbr.parts[i].begsect == LBA_MAX_SECT && \
	mbr.parts[i].endcyl == LBA_MAX_CYL && \
	mbr.parts[i].endhead == LBA_MAX_HEAD && \
	mbr.parts[i].endsect == LBA_MAX_SECT)

#define	SET_CHS_TO_MAX(mbr, i) \
	mbr.parts[i].begcyl  =  LBA_MAX_CYL;	\
	mbr.parts[i].beghead = LBA_MAX_HEAD;	\
	mbr.parts[i].begsect = LBA_MAX_SECT;	\
	mbr.parts[i].endcyl  =  LBA_MAX_CYL;	\
	mbr.parts[i].endhead = LBA_MAX_HEAD;	\
	mbr.parts[i].endsect = LBA_MAX_SECT;

static int
write_magic_fdisk_entry(u_char dev)
{
	int i;
	struct mboot mbr;

	/* read MBR */
	if (read_disk(dev, 0, 1, (char *)&mbr) != 0) {
		debug(D_LBA,
		    "write_magic_fdisk_entry: can't read fdisk table\n");
		return (FALSE);
	}

	/* If no fdisk table, and disk doesn't appear blank, fail to mark */
	if (!has_fdisk_table(&mbr) && !disk_is_blank(&mbr)) {
		debug(D_LBA, "write_magic_fdisk_entry: no fdisk table, "
		    "and disk not blank; fail\n");
		return (FALSE);
	}

	/*
	 * Either we have an fdisk table, or we're a "blank" disk.
	 * Handle "blank" first.
	 */

	if (disk_is_blank(&mbr)) {
		/* set first entry; we know it's empty */
		SET_CHS_TO_MAX(mbr, 0);
		debug(D_LBA, "write_magic_fdisk_entry: setting entry 0 "
		    "on blank disk\n");
		goto update_mbr;
	}

	/* Have an fdisk table.  Look for Solaris entry */
	for (i = 0; i < FD_NUMPART; i++) {
		if (mbr.parts[i].systid == SUNIXOS) {
			if (CHS_IS_MAX(mbr, i)) {
				debug(D_LBA,
				    "write_magic_fdisk_entry: found max mark"
				    " on Solaris partition %d\n", i);
				return (TRUE);
			} else {
				debug(D_LBA,
				    "write_magic_fdisk_entry: writing Solaris"
				    " partition %d\n", i);
				SET_CHS_TO_MAX(mbr, i);
				goto update_mbr;
			}
		}
	}

	/* No Solaris entry.  Look for an empty one. */
	for (i = 0; i < FD_NUMPART; i++) {
		if (mbr.parts[i].systid == 0) {
			if (CHS_IS_MAX(mbr, i)) {
				debug(D_LBA,
				    "write_magic_fdisk_entry: found max mark"
				    " on empty partition %d\n", i);
				return (TRUE);
			} else {
				debug(D_LBA,
				    "write_magic_fdisk_entry: writing empty"
				    " partition %d\n", i);
				SET_CHS_TO_MAX(mbr, i);
			}
			goto update_mbr;
		}
	}

	/* Can't find a partition to write it; return failure. */
	debug(D_LBA, "write_magic_fdisk_entry: can't find a partition\n");
	return (FALSE);

update_mbr:
	if (write_sectors(dev, 0, 0, 1, 1, (char *)&mbr) != 0) {
		debug(D_LBA,
		    "write_magic_fdisk_entry: can't write fdisk table\n");
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Try to figure out if we have an FDISK table.  Look for the signature,
 * then look for FAT strings at the FAT16 and FAT32 places; if we have
 * a sig but no FAT strings, assume it's an MBR.
 */

#define	FAT_STRING_OFFSET	0x36
#define	FAT32_STRING_OFFSET	0x52

static int
has_fdisk_table(struct mboot *mbp)
{
	char fat[] = "FAT";
	if (mbp->signature != MBB_MAGIC)
		return (FALSE);

	if (strncmp((u_char *)(mbp + FAT_STRING_OFFSET),
	    fat, strlen(fat)) == 0)
		return (FALSE);

	if (strncmp((u_char *)(mbp + FAT32_STRING_OFFSET),
	    fat, strlen(fat)) == 0)
		return (FALSE);

	return (TRUE);
}

/*
 * Look for entire sector all one byte pattern; if so, call this disk
 * "blank"
 */

static int
disk_is_blank(struct mboot *mbp)
{
	u_char byte_pattern;
	u_char *p;
	int i;

	p = (u_char *)mbp;
	byte_pattern = *p;

	for (i = 1; i < sizeof (*mbp); i++) {
		if (byte_pattern != *(p+i)) {
			return (FALSE);
		}
	}
	return (TRUE);
}

void
output_biosprim()
{
	if (all_bios_drives_support_lba) {
		out_bop("dev /\n");
		out_bop("setprop lba-access-ok\n");
		debug(D_LBA, "lba-access-ok set on /\n");
	}

	out_bop("dev /chosen\n");
	out_bop("setprop bios-primary ");
	out_bop(biosprim_path);
	out_bop("\n");
	debug(D_FLOW, "bios_primary is %s\n", biosprim_path);

	out_bop("dev /options\n");
	out_bop("setprop boot-device ");
	out_bop(biosprim_path);
	out_bop("\n");
}

/*
 * Read 1k from the beginning of the designated disk
 */
static u_char
read_1k(u_char dev, char *buf)
{
	u_char status1, status2;

	status1 = read_disk(dev, 0, 2, buf);
	if (status1 == 0) {
		return (0);
	}

	/*
	 * read failed, reset disk and retry
	 */
	reset_disk(dev);
	status2 = read_disk(dev, 0, 2, buf);
	if (status2 == 0) {
		return (0);
	}
	debug(D_ERR, "Bad disk read from device 0x%x", dev);
	debug(D_ERR, " - status1 0x%x, status2 0x%x\n", status1, status2);
	return (1);
}

static u_char
read_sectors(u_char dev, int cyl, int head, int sect, int nsect, char *buf)
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	u_char status;
	int iter;

	_segread(&segregs);
	for (iter = 0; iter < DISK_READ_RETRIES; iter++) {
		inregs.h.ah = 2; /* Read disk */
		inregs.h.al = (char)nsect;
		inregs.h.ch = cyl & 0xff;
		inregs.h.cl = (cyl >> 2) & 0xc0;
		inregs.h.cl |= sect & 0x3f;
		inregs.h.dh = (char)head;
		inregs.h.dl = dev;
		inregs.x.bx = _FP_OFF(buf);
		segregs.es = _FP_SEG(buf);
		_int86x(0x13, &inregs, &outregs, &segregs);
		status = outregs.h.ah;
		if (status == 0) {
			return (0); /* success */
		}
	}
	return (status);
}

static u_char
write_sectors(u_char dev, int cyl, int head, int sect, int nsect, char *buf)
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	u_char status;
	int iter;

	_segread(&segregs);
	for (iter = 0; iter < DISK_WRITE_RETRIES; iter++) {
		inregs.h.ah = 3; /* Write disk */
		inregs.h.al = (char)nsect;
		inregs.h.ch = cyl & 0xff;
		inregs.h.cl = (cyl >> 2) & 0xc0;
		inregs.h.cl |= sect & 0x3f;
		inregs.h.dh = (char)head;
		inregs.h.dl = dev;
		inregs.x.bx = _FP_OFF(buf);
		segregs.es = _FP_SEG(buf);
		_int86x(0x13, &inregs, &outregs, &segregs);
		status = outregs.h.ah;
		if (status == 0) {
			return (0); /* success */
		}
	}
	return (status);
}

static void
reset_disk(u_char dev)
{
	u_char status;
	union _REGS inregs, outregs;

	inregs.h.ah = 0; /* Reset disk */
	inregs.h.dl = dev;
	_int86(0x13, &inregs, &outregs);
	status = outregs.h.ah;
	if (status == 0) {
		debug(D_ERR, "bad disk reset: device 0x%x, status 0x%x\n",
			dev, status);
	}
}

static u_char
read_lba(u_char dev, long relblk, int nblks, char *buf)
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	dev_pkt_t read_pkt;
	char *pkt = (char *)&read_pkt;

	read_pkt.size = 0x10;
	read_pkt.dummy1 = 0;
	read_pkt.nblks = (unsigned char)nblks;
	read_pkt.dummy2 = 0;
	read_pkt.bufp = (long)buf;
	read_pkt.lba_lo = relblk;
	read_pkt.lba_hi = 0;
	read_pkt.bigbufp_lo = 0;
	read_pkt.bigbufp_hi = 0;

	_segread(&segregs);
	inregs.h.ah = 0x42;
	inregs.h.dl = dev;
	/* Use pkt in next 2 lines because compiler objects to &read_pkt */
	inregs.x.si = _FP_OFF(pkt);
	segregs.ds = _FP_SEG(pkt);
	_int86x(0x13, &inregs, &outregs, &segregs);

	return (outregs.h.ah);
}

/*
 * This algorithm is a copy of the one in bootblk in bootio.c.  There
 * is also a copy in boot.bin disk.c.
 */
static int
is_eltorito_device(u_char dev)
{
	int answer = 0;
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	int13_emulterm_packet_t spec_packet;
	char *p;

	debug(D_FLOW, "Checking dev %x for boot CDROM\n", dev);

	/*
	 * Issue an INT 13 function 4B, subfunction 1
	 * which returns El Torito disk emulation status without
	 * terminating emulation.
	 */
	memset(&spec_packet, 0, sizeof (spec_packet));
	spec_packet.size = 0x13;
	spec_packet.bios_code = ~dev;
	_segread(&segregs);
	inregs.h.ah = 0x4B;
	inregs.h.al = 1;
	inregs.h.dl = dev;
	inregs.x.si = (unsigned short)((unsigned long)&spec_packet & 0xFFFF);
	segregs.ds = (unsigned short)((unsigned long)&spec_packet >> 16);
	_int86x(0x13, &inregs, &outregs, &segregs);

	/*
	 * The El Torito spec is not clear about what the carry flag
	 * means for this call.  So if it returns CF set, examine the
	 * buffer to determine whether it looks like the call worked.
	 * Firstly we require that the boot device code matches.
	 * Secondly we require the LBA address of the bootstrap or
	 * image to be non-zero.  Thirdly we require the boot device
	 * not to be one of the standard device codes for bootable
	 * diskette or hard drive unless the CDROM is booted using an
	 * emulated image.
	 */
	if (outregs.x.cflag && spec_packet.image_lba != 0 &&
			spec_packet.bios_code == dev) {
		if ((dev != 0 && dev != 0x80) ||
					(spec_packet.media_type & 0xF) != 0) {
			outregs.x.cflag = 0;
			debug(D_FLOW, "Ignoring CARRY flag from INT 13 "
				"function 4B01\n");
		}
	}

	if (outregs.x.cflag == 0 && spec_packet.size >= 0x13 &&
			spec_packet.bios_code == dev) {
		debug(D_FLOW, "Dev %x is the boot CDROM per INT 13 function "
			"4B01\n", dev);
		return (1);
	}

	/*
	 * El Torito devices booted under "no-emulation mode" are supposed
	 * to have device codes in the range 0x81 - 0xFF.  Assume the device
	 * is not El Torito if the code is not in this range.  Note that
	 * placing this test here allows us to tolerate an invalid device
	 * code so long as the BIOS implements the 4B01 call properly.
	 */
	if (dev < 0x81)
		return (0);

	/*
	 * El Torito devices always have LBA access with 2K block size.
	 * If the BIOS reported a larger block size, assume that the device
	 * is not El Torito.  The primary purpose of this test is to avoid
	 * trying to read from a device whose block size is too large for
	 * our buffer.  We do not reject devices that report block sizes
	 * smaller than 2K because we have seen at least one El Torito BIOS
	 * report a block size of 512.
	 */
	inregs.h.ah = 0x41; /* check extensions present */
	inregs.x.bx = 0x55AA;
	inregs.h.dl = dev;
	_int86(0x13, &inregs, &outregs);

	if (outregs.x.cflag == 0 && outregs.x.bx == 0xAA55 &&
			(outregs.x.cx & 1)) {
		unsigned short param_pkt[13];
		unsigned short *param_pktp = param_pkt;

		param_pkt[0] = sizeof (param_pkt);
		param_pkt[12] = 0;
		_segread(&segregs);
		inregs.h.ah = 0x48; /* get drive parameters */
		inregs.h.dl = dev;
		inregs.x.si = _FP_OFF(param_pktp);
		segregs.ds = _FP_SEG(param_pktp);
		_int86x(0x13, &inregs, &outregs, &segregs);

		if (outregs.x.cflag == 0 && outregs.h.ah == 0 &&
				param_pkt[12] > 2048) {
			debug(D_FLOW, "Dev %x reports block size %x\n",
				param_pkt[12]);
			return (0);
		}
	}

	/*
	 * Attempt an LBA read of block 0x11.  If it looks like an El Torito
	 * BRVD assume we are booted from an El Torito device.
	 */
	p = malloc(2048);
	if (read_lba(dev, 0x11, 1, p) == 0) {
		if (p[0] == 0 && strncmp(p + 1, "CD001", 5) == 0 &&
		    strncmp(p + 7, "EL TORITO SPECIFICATION", 23) == 0) {
			answer = 1;
			debug(D_FLOW, "Block 11 contains BRVD");
		}
		else
			debug(D_FLOW, "Block 11 is not BRVD");
	}
	else
		debug(D_FLOW, "Block 11 read failed");
	free(p);

	return (answer);
}

/*
 * Read the given relative sector from the device indicated.
 * bootconf reads only sectors near the start of the device.
 * For disks that means that all reads can use the normal
 * BIOS read (AH=2) function.  For CDROMS we need to use
 * LBA reads because normal BIOS reads tend not to do what
 * we want.  For example some of them read relative to the
 * bootstrap location as if they were emulating a hard drive
 * at that location.
 */
static u_char
read_disk(u_char dev, long relsect, int nsect, char *buf)
{
	long secspercyl, remainder;
	long heads, secs;
	int rdcyl, rdhead, rdsect;
	union _REGS inregs, outregs;
#define	NO_DEV	0xFF00
	static int cdrom_dev = NO_DEV;

	if (cdrom_dev == NO_DEV && is_eltorito_device(dev)) {
		cdrom_dev = dev;
		if (cdrom_dev == bios_bootdev)
			booted_from_eltorito_cdrom = 1;
	}

	if (dev == cdrom_dev) {
		char *cdbuf = malloc(2048);
		int n = nsect;
		long r = relsect;
		char *p = buf;
		u_char ret = 0;
		int c;

		if (cdbuf == 0)
			fatal("Could not allocate CDROM read buffer\n");

		while (n > 0) {
			if (n < 4 || (r & 3) != 0) {
				if ((ret = read_lba(dev, r >> 2, 1,
						cdbuf)) != 0) {
					goto lba_done;
				}
				c = 4 - (r & 3);
				if (n < c)
					c = n;
				memcpy(p, cdbuf + (r & 3) * 512, c * 512);
			} else {
				c = (n & ~3);
				if ((ret = read_lba(dev, r >> 2, n >> 2,
						p)) != 0) {
					goto lba_done;
				}
			}
			n -= c;
			r += c;
			p += (c * 512);
		}
lba_done:
		free(cdbuf);
		return (ret);
	}

	inregs.h.ah = BEF_GETPARMS;
	inregs.h.dl = dev;

	_int86(0x13, &inregs, &outregs);

	if (outregs.h.ah)
		return (1);
	heads = outregs.h.dh;
	secs = outregs.h.cl & 0x3f;

	/*
	 * heads is maximum head number (zero based)
	 */
	secspercyl = (heads + 1) * secs;
	rdcyl = (int)(relsect/secspercyl);
	remainder = relsect % secspercyl;
	rdhead = remainder / secs;
	remainder = remainder % secs;
	rdsect = (int)remainder + 1;
	return (read_sectors(dev, rdcyl, rdhead, rdsect, nsect, buf));
}

/*
 * Checks if the bef device handles the bios primary or the bios boot device.
 * Returns 0 if not or already found, and 1 on success for bios boot device.
 */
int
check_biosdev(bef_dev *bdp)
{
	char *buf;

	debug(D_FLOW, "check_biosdev: checking %s\n", bdp->name);

	if (!bdp->installed || bdp->info->dev_type != MDB_SCSI_HBA) {
		debug(D_FLOW, "check_biosdev: not installed or not HBA\n");
		return (0);
	}

	/*
	 * Normally we want to check only hard drives.  If booting
	 * from CDROM, we also need to check CDROM drives.  The only
	 * CDROM drive we care about is the boot device, so skip this
	 * device if we have already identified the boot device.
	 *
	 * Note: it is desirable to avoid unnecessary reads from
	 * CDROM devices other than an active boot device because
	 * some HBA/device combinations can be very slow to indicate
	 * failure when trying to read an empty drive.
	 */
	if (bdp->info->MDBdev.scsi.pdt != INQD_PDT_DA) {
		if (bdp->info->MDBdev.scsi.pdt != INQD_PDT_ROM ||
		    booted_from_eltorito_cdrom == 0 ||
		    bios_boot_devp) {
			debug(D_FLOW, "check_biosdev: wrong device type\n");
			return (0);
		}
	}

	if ((bios_primary_failure || bios_primaryp) &&
	    (bios_dev_failure || bios_boot_devp)) {
		debug(D_FLOW, "check_biosdev: previously identified or bad\n");
		return (0);
	}

	/*
	 * We have to check if this device
	 * matches the bios primary (device 0x80).
	 * We can't use geometry comparisons because the
	 * bef framework (scsi.c) always lies by returning
	 * a fixed geometry. So we just compare sectors
	 * up to a reasonable limit - the 1st 1KB.
	 */
	if ((buf = malloc(1024)) == NULL) {
		MemFailure();
	}
	if (read_1k(bdp->info->bios_dev, buf)) {
		debug(D_ERR, "read from 0x%x failed\n", bdp->info->bios_dev);
		free(buf);
		debug(D_FLOW, "check_biosdev: read_1k failed\n");
		return (0);
	}
	if (bios_primaryp == NULL && !bios_primary_failure) {
		if (memcmp(buf, Bios_primary_1k, 1024) == 0) {

			char *p;

			debug(D_FLOW, "check_biosdev: found a BIOS primary "
				"1K match\n");

			bios_primaryp = bdp;
			get_path_from_bdp(bdp, biosprim_path, 0);
			/* clip off slice letter */
			if ((p = strrchr(biosprim_path, ':')) != NULL)
				*p = '\0';
		}
	}
	if (bios_boot_devp == NULL && !bios_dev_failure) {
		if (memcmp(buf, Bios_bootdev_1k, 1024) == 0) {
			debug(D_FLOW, "check_biosdev: found a BIOS boot dev "
				"1K match\n");
			bios_boot_devp = bdp;
			/*
			 * Now set the boot device geometry in this bef.
			 * Also set bdev in the BEF to bios_bootdev;
			 * this makes resmain() in the driver start
			 * handling requests for the BIOS drivenum
			 * as well (so it would handle 0x10 *and* 0x80,
			 * for instance, after this point).
			 */
			bdp->info_orig->cyls = bios_dev_cyls;
			bdp->info_orig->heads = bios_dev_heads;
			bdp->info_orig->secs = bios_dev_secs;

			bdp->info_orig->bdev = bios_bootdev;

			free(buf);
			return (1);
		}
	}
	free(buf);
	debug(D_FLOW, "check_biosdev: done\n");
	return (0);
}
