/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)scsi.c	1.44	99/11/02 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework
 * ===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
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
 */

#include <types.h>
#include <bef.h>
#include <dev_info.h>
#include <befext.h>
#include <common.h>
#include <stdio.h> /* for correct printf debug */
#include "scsi.h"

/* #define DEBUG /* */
/* #define PAUSE /* */
/* #define TESTREAD /* */

/*
 * Function prototypes.
 */
static void	dump_scsi(short b, short s);
static void	scsi_bsize(DEV_INFO *info);
static int	check_install();
static int	common_install_scsi(int verbose);
static int	is_installed(char *ident, unsigned char *devp);
static int 	do_read(DEV_INFO *info, ushort *ax, ulong blocknum, 
			ushort numblocks, ushort bufofs, ushort bufseg);

#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define	DPrintf(f, x) if (debug_scsi & f) printf x; else
#else
#define	DPrintf(f, x)
#endif
#define	DBG_ERRS	0x0001	/* ... error messages */
#define	DBG_GEN		0x0002	/* ... general info */
#define	DBG_INIT	0x0004	/* ... initialization code */
#define	DBG_RESMAIN	0x0008	/* ... specific messages in resmain */
#define	DBG_VGEN	0x0010	/* ... reads dump 32 chars of the buffer */
#define	DBG_GENRES	(DBG_GEN | DBG_RESMAIN)
#define	DBG_GENINIT	(DBG_GEN | DBG_INIT)
#define	DBG_ALL		0xffff
#define	DBG_OFF		0x0000
int debug_scsi = DBG_ERRS | DBG_GEN | DBG_INIT;

#pragma comment(compiler)
#pragma comment(user, "scsi.c	1.44	99/11/02")

#define	BUFSIZE 2048	/* Largest supported physical block size */

extern char		ident[]; /* ... defined in each driver directory */
extern int		_end[];
unchar			myfirstdev, mylastdev;
long			oldvect;
struct inquiry_data	inqd;
struct readcap_data	readcap_data;
unchar			databuf[BUFSIZE];
int			init_dev_before = 0;

/*
 * We have no obvious maximum number of possible SCSI devices because
 * we do not know how many host adapters might be supported.  So we
 * choose a number that is a compromise between restricting the number
 * of devices and wasting space in the data segment.
 */
#define	MAXDEVS	128

ushort			devs;
extern DEV_INFO		dev_info[];

#ifdef PAUSE
#define	Pause() pause()
static void
pause() {printf("Press ENTER to proceed "); kbchar(); printf("\n"); }
#else
#define	Pause()
#endif

int
resmain(ushort ax, ushort bx, ushort cx, ushort dx,
    ushort si, ushort di, ushort es, ushort ds)
{
	unchar req_dev = (dx & 0xFF);
	DEV_INFO *info;
	ushort dev;
	ulong	blocknum;
	ushort bufseg;
	ushort bufofs;
	ushort toread;
	ushort numblocks;
	ushort cylinder;
	unchar head, sector;
	int ret;
	extern int (*putvec)(int);
	struct ext_getparm_resbuf far *resbuffer;
	struct ext_dev_addr_pkt far *readpkt;

	/* -- make sure prints go through putc routine and not callback -- */
	putvec = 0;

	/* Find the dev_info slot for req_dev */
	for (dev = 0, info = dev_info; dev < devs; dev++, info++) {
		if (info->bios_dev == req_dev) {
			DPrintf(DBG_GENRES,
				("Device 0x%x, base 0x%04x, port 0x%04x\n",
					dev, info->bios_dev, info->base_port));
			break;
		}
		if (info->bdev >= 0x80 && req_dev == info->bdev) {
			DPrintf(DBG_GENRES, ("Handling boot device\n"));
			break;
		}
	}
	if (dev == devs) {
		return (0); /* pass on */
	}

	switch (ax >> 8) {
	case BEF_IDENT:
		/*
		 * For now return the vendor/product string in es:bx and
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

		DPrintf(DBG_GENRES,
			("BEF_IDENT: bx %02x, cx %02x, dx %02x es %02x\n",
				bx, cx, dx, es));

		return (1);

		/*
		 *[]----------------------------------------------------[]
		 * | Start of SCSI-specific code			|
		 *[]----------------------------------------------------[]
		 */
	case BEF_READ:

		bufofs = bx;

		/*
		 * Isolate BIOS-style cyl/head/sector, convert sector
		 * to be 0-based, check head and sector for limits and
		 * calculate SCSI 512-byte sector offset.
		 */
		cylinder = (((cx & 0xC0) << 2) | ((cx & 0xFF00) >> 8));
		head = (dx & 0xFF00) >> 8;
		sector = (cx & 0x3F) - 1;

		if (req_dev == info->bdev) {
			/*
			 * We must translate from the bios boot device
			 * geometry to the logical block address
			 */
			ulong ul = ((ulong) cylinder * (info->heads + 1) +
			    (ulong) head) * info->secs +
			    (ulong) sector; /* 1 based */

			blocknum = (long)ul;
		} else {
			/*
			 * Translate from the fixed geometry supplied
			 * in the BEF_GETPARMS call
			 * to the logical block address
			 */
			ulong ul = ((ulong) cylinder * DEFAULT_NUMHEADS +
				(ulong) head) * DEFAULT_NUMSECS +
				(ulong) sector; /* 1 based */

			blocknum = (long)ul;
		}

		numblocks = ax & 0xFF;
		
		return do_read(info, &ax, blocknum, numblocks, bufofs, es);

	case BEF_GETPARMS:

		if (req_dev == info->bdev) {
			dx = ((info->heads & 0xff) << 8) | 1; /* one drive */
			cx = ((info->cyls & 0xff) << 8) |
			    ((info->cyls & 0x300) >> 2) |
			    (info->secs & 0x3f);
		} else {
			/* Should set number of cylinders for real device */
			dx = ((DEFAULT_NUMHEADS - 1) << 8) | DEFAULT_NUMDRIVES;
			cx = ((DEFAULT_NUMCYLS - 1) << 6) | DEFAULT_NUMSECS;
		}
		ax = 0;	  /* good return */
		return (1);	  /* have handled int, don't chain */

	case BEF_RESET:
DPrintf(DBG_GENRES, ("BEF_RESET: ax %02x, bx %02x, cx %02x, dx %02x, es %02x\n",
		ax, bx, cx, dx, es));

		if (dev_init(info)) {
			ax = 0xBB00; /* Return "undefined error" for status */
			return (-1);
		} else {
			ax = 0;
			return (1);
		}
	
	case BEF_CHKEXT:

		/* Input:
		 *   AH = 41H
		 *   BX = 55AAH
		 *   DL = Drive number
		 *
		 * Output:
		 *
		 *  (Carry Clear)
		 *   AH = Extensions version
		 *     10h = 1.x
		 *     20h = 2.0/EDD-1.0
		 *     21h = 2.1/EDD-1.1
		 *     30h = EDD-3.0
		 *   AL = Internal Use only  (??)
		 *   BX = AA55H
		 *   CX = Interface support bit map
		 *     Bit	Description
		 *     -----	------------
		 *     0	1 - Fixed disk access subset
		 *     1	1 - Drive locking and ejecting subset
		 *     2	1 - Enhanced disk drive support subset
		 *     3-15	Reserved, must be 0
		 *
		 *   (Carry Set)
		 *    AH = Error code (01h = Invalid command)
		 */

DPrintf(DBG_GENRES, ("BEF_CHKEXT: ax %02x, bx %02x, cx %02x, dx %02x, es %02x\n",
		ax, bx, cx, dx, es));

		if (bx != 0x55AA) {
			ax = 0x0100 | (ax & 0xFF);
			return (-1);
		}

		/* EDD-1.1, "Fixed disk access", signature */
		ax = 0x2100 | (ax & 0x00FF);
		cx = 0x0001;
		bx = 0xAA55;

		return (1);

	case BEF_EXTREAD:

		if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE) 
			scsi_bsize(info);

		/* Get the packet address */
		readpkt = (struct ext_dev_addr_pkt far *) MK_FP(ds, si);
		
		bufofs = FP_OFF(readpkt->bufaddr);
		bufseg = FP_SEG(readpkt->bufaddr);

		if (readpkt->bufaddr == 0xFFFFFFFF || 
		    readpkt->lba_addr_hi != 0 ||
		    readpkt->numblocks > 0x7F) {
			/* We only handle 2^32, 4GB addrs, and 127 blocks */
			ax = 0x0100;
			return (-1);
		}

		/* 
		 * Address is already in LBA, so no conversion needed like
		 * for BEF_READ.
		 */

		blocknum = (long) readpkt->lba_addr_lo;
		numblocks = (ushort) readpkt->numblocks;

		ax = (dev_read(info, blocknum, numblocks, bufofs, bufseg)) << 8;
		if (ax) {
			readpkt->numblocks = 0;
			return (-1);
		}
		return (1);

	case BEF_EXTGETPARMS:

		resbuffer = (struct ext_getparm_resbuf far *) MK_FP(ds, si);

		if (resbuffer->bufsize < 26) {
			ax = 0x0100 | (ax & 0xFF);
			return (-1);
		} else if (resbuffer->bufsize < 30) {
			resbuffer->bufsize = 26;
		} else {
			resbuffer->bufsize = 30;
		}

		if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE) 
			scsi_bsize(info);

		/* Note: can't handle more than 2^32-1 blocks */
		resbuffer->num_secs_lo = 
		    (unsigned long)readcap_data.rdcd_lba[0] << 24 |
		    (unsigned long)readcap_data.rdcd_lba[1] << 16 |
		    (unsigned long)readcap_data.rdcd_lba[2] << 8 |
		    (unsigned long)readcap_data.rdcd_lba[3];

		/* don't increment if the answer was "0" */
		if (resbuffer->num_secs_lo != 0)
			resbuffer->num_secs_lo++;
		resbuffer->num_secs_hi = 0;

		/* Fake physical geometry by using logical hds and secs */
		if (info->heads == 0 || info->secs == 0) {
			resbuffer->heads = (ulong)DEFAULT_NUMHEADS;
			resbuffer->secs = (ulong)DEFAULT_NUMSECS;
		} else {
			resbuffer->heads = info->heads;
			resbuffer->secs = info->secs;
		}

		resbuffer->cyls = resbuffer->num_secs_lo / 
			(resbuffer->heads * resbuffer->secs);
			
		resbuffer->bytes_per_sec = info->MDBdev.scsi.bsize;

		if (resbuffer->bufsize == 30) {
			resbuffer->dpte = MK_FP(0xFFFF, 0xFFFF);
		}

		resbuffer->info_flags = 0x0000;

		/* If the drive is large enough, heads/secs/cyls not valid */

		if (resbuffer->num_secs_lo <= MAXBLOCKS_FOR_GEOM) {
			resbuffer->info_flags |= INFO_PHYSGEOM_VALID;
		}

		if ((info->MDBdev.scsi.pdt & INQD_PDT_DMASK) == INQD_PDT_ROM) {
			resbuffer->info_flags |= INFO_REMOVABLE;
		}

		/* XXX - do we care about other INFO_ fields? */

		ax = 0x0000 | (ax & 0xFF);
		return (1);

	case BEF_EXTWRITE:
	case BEF_EXTVERIFY:
	case BEF_EXTSEEK:

		/* These three need to be supported in order to claim 
		 * support for some of the other functions.  As a safety
		 * precaution, we put these here to return failure. 
		 * 
		 * An alternative approach would be to not include these
		 * and let the default case handle this error.
		 */

		/*FALLTHROUGH*/

	default:
		ax = 0x0100;	/* Return "bad command" for status */
		return (-1);
	}
}

static int 
do_read(DEV_INFO *info, ushort *ax, ulong blocknum, ushort numblocks, 
    ushort bufofs, ushort bufseg)
{
	ushort	toread;
	ushort  blockoff;
	int	ret;
	union halves	bn_un;

#define	bn	bn_un.l
#define	bn_lo	bn_un.s[0]
#define	bn_hi	bn_un.s[1]

	bn = blocknum;		/* Cast to signed long? */

	/*
	 * If this is the first read attempt we will not yet
	 * have determined the block size for the device.
	 * Try to do so now.
	 */
	if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE) 
		scsi_bsize(info);

	/* So far we support only 512-byte and 2K blocks */
	switch (info->MDBdev.scsi.bsize) {
	case 512:
	case 2048:
		break;

	default:
		/* -- Block size is unsupported or undetermined -- */
		DPrintf(DBG_ERRS, ("Unsupported block size\n"));
		*ax = 0xBB00;
		return (-1);
		break;
	}

	/* Handle 512-byte block device */
	if (info->MDBdev.scsi.bsize == 512) {
		DPrintf(DBG_GENRES,
		    ("About to read %d sectors at %08lx into %x:%x\n",
		    numblocks, bn, bufseg, bufofs));
		if (ret = dev_read(info, bn, numblocks, bufofs, bufseg)) {
			*ax = (ret << 8);
			DPrintf(DBG_ERRS, ("dev_read failed\n"));
			return (-1);
		} else {
			*ax = 0;
			if (debug_scsi & DBG_VGEN)
				dump_scsi(bufofs, bufseg);
			Pause();
			return (1);
		}
	}

	/* Handle 2048-byte block device */
	blockoff = bn_lo & 3;
	lrshift(&bn_un, 2);
	for (toread = numblocks; toread > 0; ) {
		/*
		 * Read one or more complete blocks into user buffer
		 * if on block boundary and have at least one block
		 * to read.
		 */
		if (blockoff == 0 && toread >= 4) {
			numblocks = toread / 4;

DPrintf(DBG_GENRES, ("Read 0x%x 2K blocks at 0x%08lx into %04x:%04x\n",
		numblocks, bn, bufseg, bufofs));

			if (ret = dev_read(info, bn, numblocks, 
			    bufofs, bufseg)) {
				*ax = (ret << 8);
				return (-1);
			}

			bufofs += 2048 * numblocks;
			toread -= 4 * numblocks;
			bn += numblocks;
			continue;
		}

		/*
		 * Read a block into local buffer and copy some amount
		 * of it into user buffer.
		 */
		numblocks = 4 - blockoff;
		if (numblocks > toread)
			numblocks = toread;

DPrintf(DBG_GENRES, ("Copy %d sectors at 0x%08lx into %04x:%04x\n",
		numblocks, bn, bufseg, bufofs));

		if (ret = dev_read(info, bn, 1, (ushort)databuf,
		    myds())) { 
			*ax = (ret << 8);
			return (-1);
		}
		bcopy(MK_FP(bufseg, bufofs),
			MK_FP(myds(), FP_OFF(&databuf[blockoff << 9])),
			numblocks * 512);
		toread -= numblocks;
		bufofs += 512 * numblocks;
		blockoff += numblocks;
		if (blockoff == 4) {
			blockoff = 0;
			bn++;
		}
	}

	*ax = 0;
	return (1);
}

/*
 * The following routine is a SCSI-specific resident routine
 */

static void
dump_scsi(short b, short s)
{
	int l, c;
	union {
		char far *cf;
		struct { short off, seg; } s;
	} x;

	x.s.off = b;
	x.s.seg = s;

	for (l = 4; l; l--) {
		for (c = 0; c < 16; c++)
			printf("%02x ", *x.cf++ & 0xff);
		printf("\n");
	}
}

static void
scsi_bsize(DEV_INFO *info)
{
	ushort i;

	info->MDBdev.scsi.bsize = UNKNOWN_BSIZE;

	/* First just try a straight SI_READCAP command */
	if (dev_readcap(info) == 0) {
		i = (readcap_data.rdcd_bl[3] | (readcap_data.rdcd_bl[2] << 8));
		if (readcap_data.rdcd_bl[0] == 0 &&
			readcap_data.rdcd_bl[1] == 0 && i <= BUFSIZE) {
			info->MDBdev.scsi.bsize = i;
		}
		return;
	}

	/* Give up if not a removable device */
	if ((info->MDBdev.scsi.dtq & INQD_DTQ_RMB) == 0) {
		return;
	}

	/*
	 * Simple SI_READCAP failed.  Try motor start, lock, readcap, stop,
	 * unlock sequence.  Ignore failure to start motor.
	 */
	for (i = 0; i < 10; i++) {
		if (dev_motor(info, 1) == 0) {
			break;
		}
	}

	dev_lock(info, 1);
	if (dev_readcap(info) == 0) {
		i = (readcap_data.rdcd_bl[3] | (readcap_data.rdcd_bl[2] << 8));
		if (readcap_data.rdcd_bl[0] == 0 &&
			readcap_data.rdcd_bl[1] == 0 && i <= BUFSIZE) {
			info->MDBdev.scsi.bsize = i;
		}
	}

	/*
	 * If we still do not know the blocksize it is probably because:
	 *		a) there is no medium in the drive or
	 *		b) the drive does not support the SX_READCAP command
	 *
	 * If the device is a CDROM drive, attempt a read using a blocksize
	 * of 2048 (the normal blocksize for such devices).  If the read
	 * appears to succeed, and changes the last word of the buffer,
	 * assume case b) and a blocksize of 2048.  Otherwise assume case a).
	 */
	if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE &&
		(info->MDBdev.scsi.pdt & INQD_PDT_DMASK) == INQD_PDT_ROM) {
		info->MDBdev.scsi.bsize = 2048;
		*(int *)&databuf[2046] = 0xAFB0;
		if (dev_read(info, 0L, 1, (ushort)databuf, myds()) ||
			*(int *)&databuf[2046] == 0xAFB0) {
			info->MDBdev.scsi.bsize = UNKNOWN_BSIZE;
		}
	}

	dev_lock(info, 0);
	dev_motor(info, 0);
}

/*
 * The following routines are used by the SCSI-specific code to avoid
 * the use of library routines.  They can be omitted from code for
 * other devices unless the same functions are required.
 */

/* long left shift */
void
llshift(union halves far *hp, ushort dist)
{
	hp->s[1] = ((hp->s[1] << dist) | (hp->s[0] >> (16 - dist)));
	hp->s[0] <<= dist;
}

/* long right shift */
void
lrshift(union halves far *hp, ushort dist)
{
	hp->s[0] = ((hp->s[0] >> dist) | (hp->s[1] << (16 - dist)));
	hp->s[1] >>= dist;
}

/* convert segment/offset pair to physical address */
long
longaddr(ushort offset, ushort segment)
{
	long answer = segment;

	llshift((union halves far *)&answer, 4);
	answer += offset;
	return (answer);
}

/*
 * End of SCSI-specific utility routines.
 */

DEV_INFO dev_info[MAXDEVS + 1];

/*
 * Code starting here is part of the general framework.  Code and data
 * before this point is part of the resident portion of the module.
 * Code and data after this point is part of the non-resident portion.
 * These limits are defined by the names "enddat" and "initmain" so
 * do not change anything between here and initmain.
 */

char enddat = 0;
ushort endloc = (ushort)&enddat;

/*
 * check_install() will return 1 if this device has already
 * been installed and will print a message. Otherwise it will
 * return 0.
 */
static int
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
	}
	return (0);
}

int	MDB_loaded;	/* Global to flag driver was loaded by MDB */

/*
 * dispatch
 * Boot Hill Functions. This is the new .bef interface
 * section for the Boot Hill project.
 */
int
dispatch(int func)
{
	int Rtn = BEF_FAIL;	/* --- default case */


	switch (func) {
	case BEF_PROBEINSTAL:
		/* ---- Old driver interface ---- */
		MDB_loaded = 1;
		if (!check_install())
			Rtn = initmain();
		break;

	case BEF_LEGACYPROBE:
		MDB_loaded = 0;
		legacyprobe();
		break;

	case BEF_INSTALLONLY:
		MDB_loaded = 0;
		is_installed(ident, &myfirstdev);
		if ((installonly() == BEF_OK) && (devs != 0))
			Rtn = common_install_scsi(0);
		break;
	}
	return (Rtn);
}

int
initmain()
{
	DPrintf(DBG_GENINIT, ("initmain() of %s\n", ident));

	/* Determine whether my device is present */
	dev_find();
	if (devs == 0) {
		printf("%s BIOS extension not installed.\r\n\n", ident);
		return (0);
	}

	return (common_install_scsi(1));
}

static int
common_install_scsi(int verbose)
{
	unsigned short codesize, datasize, totalsize;
	unsigned short codeseg, dataseg;
	int i;
	extern newvect();
#ifdef TESTREAD
	static char testbuf[512] = { 0 };
	extern int bef_read0();
#endif

	/*
	 * We no longer re-locate the .bef if we are running under
	 * bootconf rather than under MDB.  Just install ourself and
	 * return success.
	 */
	if (!MDB_loaded) {
		dataseg = myds();
		/* Read the int13 vector into oldvect */
		getvec(0x13, &oldvect);
		for (i = 0; i < devs; i++) {
			init_dev(dev_info[i].base_port, dataseg);
		}
		/* Set up for finding data from code */
		hidata(dataseg);
		/* Intercept int13 calls */
		setvec(0x13, newvect, mycs());
		return ((unsigned short)mylastdev);
	}

	/* Read the int13 vector into oldvect */
	getvec(0x13, &oldvect);

	/* Calculate resident code size in bytes */
	codesize = ((myds() - mycs()) + 1) << 4;

	/* Calculate resident data size in bytes */
	datasize = (unsigned short)_end;

	DPrintf(DBG_GENINIT, ("codesize 0x%x, datasize 0x%x\n",
		codesize, datasize));

	/* Calculate total resident size in K */
	totalsize = (((codesize + 15) >> 4) + ((datasize + 15) >> 4) + 63) >> 6;

	if (memsize() < totalsize) {
		printf("Not enough memory to install %s bef\n", ident);
		return (0);
	}
	codeseg = reserve(totalsize);
	dataseg = codeseg + ((codesize + 15) >> 4);

	/* Set up for finding hi data from hi code */
	hidata(dataseg);

	if (init_dev_before) {
		/*
		 * Currently only the ncrs bef sets init_dev_before
		 * and this bit of code was the only difference between
		 * its local scsi.c and this one. So special case in
		 * order to have just one scsi framework file.
		 */
		/* Do board-specific installation */
		for (i = 0; i < devs; i++) {
			init_dev(dev_info[i].base_port, dataseg);
		}
		/* Copy code and data parts into high memory */
		bcopy(MK_FP(codeseg, 0), MK_FP(mycs(), 0), codesize);
		bcopy(MK_FP(dataseg, 0), MK_FP(myds(), 0), datasize);
	} else {
		/* Copy code and data parts into high memory */
		bcopy(MK_FP(codeseg, 0), MK_FP(mycs(), 0), codesize);
		bcopy(MK_FP(dataseg, 0), MK_FP(myds(), 0), datasize);
		/* Do board-specific installation */
		for (i = 0; i < devs; i++) {
			init_dev(dev_info[i].base_port, dataseg);
		}
	}


	/* Intercept int13 calls */
	setvec(0x13, newvect, codeseg);

#ifdef TESTREAD
	{
		unsigned char dev;

		for (dev = myfirstdev; dev <= mylastdev; dev++) {
			if (SKIPDEV(dev))
				continue;
			printf("Test read of device 0x%x ... %s.\n", dev
			bef_read0(dev, 1, testbuf) ? "succeeded" : "failed");
		}
		Pause();
	}
#endif
	if (verbose) {
		printf("%s BIOS extension installed at %x\n\n", ident, codeseg);
	}

	/* Return last device number to extended BIOS monitor */
	return ((unsigned short)mylastdev);
}

/*
 * Interrogate all installed BIOS extensions to determine whether I
 * am already installed.  Set *devp to device code for extension if
 * it is already present, otherwise next available device.  Return
 * 1 if device is present, 0 otherwise.
 */
static int
is_installed(char *ident, unsigned char *devp)
{
	int dev;
	char far *contstr;
	char far *devstr;
	char far *s1;
	char *s2;

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

unchar
next_MDB_code()
{
	unchar newdev;

	if (mylastdev == 0) {
		newdev = myfirstdev;
	} else {
		newdev = mylastdev + 1;
		while (SKIPDEV(newdev)) {
			newdev++;
		}
	}
	if (newdev == STOPDEV) {
		return (0);
	}
	mylastdev = newdev;
	return (mylastdev);
}

/*
 * End of generic code.  Remaining code in this file is SCSI-specific.
 */

void
scsi_dev(ushort base_port, ushort targid, ushort lun)
{
	int ret;
	ushort blksize;
	register int i;

	/*
	 * Start filling in details about device.  Details will be
	 * discarded unless the end of this routine is reached and
	 * "devs" incremented.
	 */
	DPrintf(DBG_GEN, ("Examining target %d, lun %d\n", targid, lun));
	ret = dev_sense(base_port, targid, lun);

	DPrintf(DBG_GEN, ("dev_sense(%x)\n", ret));

	if (dev_inquire(base_port, targid, lun))
		return;

	dev_info[devs].base_port = base_port;
	dev_info[devs].MDBdev.scsi.targ = targid;
	dev_info[devs].MDBdev.scsi.lun = lun;
	for (i = 0; i < 8; i++) {
		dev_info[devs].vid[i] = inqd.inqd_vid[i];
	}
	dev_info[devs].blank1 = ' ';
	for (i = 0; i < 16; i++) {
		dev_info[devs].pid[i] = inqd.inqd_pid[i];
	}
	dev_info[devs].blank2 = ' ';
	for (i = 0; i < 4; i++) {
		dev_info[devs].prl[i] = inqd.inqd_prl[i];
	}
	dev_info[devs].term = 0;
	dev_info[devs].MDBdev.scsi.pdt = inqd.inqd_pdt;
	dev_info[devs].MDBdev.scsi.dtq = inqd.inqd_dtq;
	dev_info[devs].version = MDB_VERS_MISC_FLAGS;

	/* Describe the device */
	DPrintf(DBG_GEN, ("Base %04x, Targ %d, lun %d: %s\n",
		dev_info[devs].base_port,
		dev_info[devs].MDBdev.scsi.targ,
		dev_info[devs].MDBdev.scsi.lun,
		dev_info[devs].vid));
	Pause();

	/*
	 * Set the block size to "unknown".  The first read will
	 * determine the blocksize.
	 */
	dev_info[devs].MDBdev.scsi.bsize = UNKNOWN_BSIZE;

	/* mark this MDB device as a SCSI HBA */
	dev_info[devs].dev_type = MDB_SCSI_HBA;

	for (i = 0; i < 8; i++) {
		dev_info[devs].hba_id[i] = ident[i];
	}

	/*
	 * The dev_info table contains MAXDEVS + 1 entries.  We never
	 * increment devs such that the final slot is used.  Do not
	 * assign a BIOS code for entries in the final slot because
	 * it can be reused for any future requests.
	 */
	if (devs < MAXDEVS) {
		/* determine BIOS device code */
		dev_info[devs].bios_dev = next_MDB_code();
		if (dev_info[devs].bios_dev == 0) {
			putstr(".  No BIOS device code available.\r\n");
			return;
		}
		devs++;

		/* Expand resident portion of data segment for new device */
		endloc = (ushort)(dev_info + devs);
	}

	DPrintf(DBG_GEN, ("Bios dev code 0x%x\n", dev_info[devs].bios_dev));
}

void
scsi_dev_pci(ushort base_port, ushort targid, ushort lun,
	unchar bus, ushort ven_id, ushort vdev_id, unchar dev, unchar func)
{
	dev_info[devs].pci_valid = 1;
	dev_info[devs].pci_bus = bus;
	dev_info[devs].pci_ven_id = ven_id;
	dev_info[devs].pci_vdev_id = vdev_id;
	dev_info[devs].pci_dev = dev;
	dev_info[devs].pci_func = func;

	scsi_dev(base_port, targid, lun);
}

int
scsi_dev_bootpath(ushort base_port, ushort targid, ushort lun, char far *path)
{
	char *s;
	int i = 0;
	/*
	 * copy string
	 */
	s = dev_info[devs].user_bootpath;
	do {
		if (++i == MAX_BOOTPATH_LEN) {
			putstr("Max bootpath length exceeded\r\n");
			return (1);
		}
	} while (*s++ = *path++);

	scsi_dev(base_port, targid, lun);
	return (0); /* successful */
}

/* Return a pointer to the last defined device, or null if none */
struct bdev_info *
scsi_dev_info(void)
{
	return (devs == 0 ? 0 : &dev_info[devs - 1]);
}
