/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom.c	1.40	99/10/07 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/promimpl.h>
#include <sys/bootcmn.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/bootconf.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/bootdef.h>
#include <stdarg.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/ihandle.h>
#include <sys/salib.h>

#include "devtree.h"

static struct ihandle stdio  = { DEVT_SER, 0, 1, "<stdio>" };
struct ihandle *open_devices[MAXDEVOPENS] = { &stdio };

extern struct pri_to_secboot *realp;
extern struct dnode *active_node;
extern struct bootops bootops;
extern char  *new_root_type;
extern struct int_pb ic;
extern int BootDev;
extern int BootDevType;
extern int Oldstyleboot;

extern struct real_regs	*alloc_regs(void);
extern struct dnode *find_node(char *, struct dnode *);
extern ushort_t bcd_to_bin(ushort_t);
extern long strtol(char *, char **, int);
extern void free_regs(struct real_regs *);
extern void *memset(void *s, int c, size_t n);
extern int doint(void);
extern int doint_r(struct int_pb *ic);
extern int doint_asm();
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int getchar();
extern void putchar();
extern int reset_disk();
extern int goany(void);
extern void bootabort(void);
extern paddr_t map_mem(uint_t, uint_t, int);
extern int macaddr_net(struct ihandle *ihp, unsigned char *eap);
extern caddr_t rm_malloc(size_t size, uint_t align, caddr_t virt);
extern void rm_free(caddr_t, size_t);

void prom_panic(char *str);

int OpenCount = 0;

/* #define	DEBUG */

#ifdef DEBUG
#define	Dprintf(x)	printf x
#else
#define	Dprintf(x)
#endif

/*ARGSUSED*/
static int
read_stdin(struct ihandle *ihp, char *buf, int len)
{
	/*
	 *  Read from stdin:
	 *
	 *  Fills the specified "buf"fer from the input device until "len" bytes
	 *  are transferred or a newline is read.  Returns the number of char-
	 *  acters read.
	 */

	int cnt = 0;

	while (cnt < len) {
		/*
		 *  Read up to "len" bytes from standard input.  The "cnt"
		 *  register tells us how many bytes we've read already.
		 */
		cnt += 1;

		if ((*buf++ = getchar()) == '\n') {
			/*
			 *  If the next input character is a line terminator,
			 *  break loop and return a short read.
			 */
			break;
		}
	}

	return (cnt);
}

/*ARGSUSED*/
static int
write_stdout(struct ihandle *ihp, char *buf, int len)
{
	/*
	 *  Write to stdout:
	 *
	 *  Writes the contents of the specified "buf"fer to the output device.
	 *  Returns the number of bytes written ("len" argument).
	 */
	int cnt = len;

	while (cnt-- > 0) {
		/*
		 *  Use the low-level "putchar" routine to write the next byte.
		 */
		putchar(*buf++);
	}

	return (len);
}

/*
 * This algorithm is the same as in bootblk (bootio.c) and bootconf
 * (biosprim.c).  Change the other two when changing this one.
 */
int
is_eltorito_dev(unsigned char unit)
{
	int answer = 0;
	int error;
	unsigned int size;
	static int13_emulterm_packet_t *spec_packet;
	static dev_pkt_t *pktp;
	static int13_extparms_result_t *extparms;
	char *buf;
	struct int_pb intregs;
	static unsigned short eltorito_dev = 0;

	Dprintf(("is_eltorito_dev(%x)\n", unit));

	/*
	 * Allocate all the realmode buffer space that might be required on
	 * the first call and keep it permanently.  We might allocate
	 * memory we never use, but we are guaranteed to have everything
	 * available when memory is tight later.  The different buffers
	 * are never used simultaneously, so share one allocation.
	 *
	 * The call to invalidate_cache here is because the first call
	 * to is_eltorito_dev is often when the cache memory is allocated.
	 * It requires special alignment, so make it happen before the
	 * the pkt/spec_packet/extparms buffer to reduce the potential
	 * for fragmentation.
	 */
	if (pktp == NULL) {
		invalidate_cache(NULL);
		size = MAX(sizeof (*spec_packet), sizeof (*pktp));
		size = MAX(size, sizeof (*extparms));
		pktp = (dev_pkt_t *)rm_malloc(size, sizeof (long), 0);
		if (pktp == NULL)
			prom_panic("is_eltorito_dev: cannot allocate "
			    "realmode memory\n");
		spec_packet = (int13_emulterm_packet_t *)pktp;
		extparms = (int13_extparms_result_t *)pktp;
	}

	/* Assume not El Torito if standard diskette code */
	if (IS_FLOPPY(unit)) {
		Dprintf(("is_eltorito_dev: device %x is a floppy.\n", unit));
		return (0);
	}

	if (eltorito_dev != 0) {
		Dprintf(("is_eltorito_dev: device %x was already found%s "
			"to be El Torito.\n", unit,
			unit == eltorito_dev ? "" : " not"));
		return (unit == eltorito_dev);
	}

	(void) memset(spec_packet, 0, sizeof (*spec_packet));
	spec_packet->size = 0x13;
	spec_packet->bios_code = ~unit;
	spec_packet->image_lba = 0;

	intregs.intval = DEVT_DSK;
	intregs.ax = ((INT13_EMULTERM << 8) | 1);
	intregs.dx = unit;
	intregs.ds = segpart((unsigned long)spec_packet);
	intregs.si = offpart((unsigned long)spec_packet);

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
	if ((error = doint_r(&intregs)) != 0 && spec_packet->image_lba &&
			spec_packet->bios_code == unit) {
		if ((unit != 0 && unit != 0x80) ||
				(spec_packet->media_type & 0xF) != 0) {
			error = 0;
			Dprintf(("is_eltorito_dev: ignoring status call carry "
				"flag\n"));
		}
	}

	/*
	 * If AH was 0, the packet size return was reasonable,
	 * the El Torito device code matched our BIOS code and
	 * the LBA word is non-zero, assume the device is an
	 * El Torito CDROM.
	 */
	if (error == 0 && (intregs.ax & 0xFF00) == 0 &&
			spec_packet->size >= 0x13 &&
			spec_packet->bios_code == unit &&
			(spec_packet->media_type & 0xF) == 0 &&
			spec_packet->image_lba != 0) {
		Dprintf(("is_eltorito_dev: status call matches device\n"));
		answer = 1;
		eltorito_dev = unit;
	}

	if (answer)
		return (answer);

	/*
	 * El Torito devices booted under "no-emulation mode" are supposed
	 * to have device codes in the range 0x81 - 0xFF.  Assume the device
	 * is not El Torito if the code is not in this range.  Note that
	 * placing this test here allows us to tolerate an invalid device
	 * code so long as the BIOS implements the 4B01 call properly.
	 */
	if (unit < 0x81) {
		Dprintf(("is_eltorito_dev: rejecting code %x\n", unit));
		return (0);
	}

	/*
	 * El Torito devices always have LBA access with 2K block size.
	 * If the BIOS reported a larger block size, assume that the device
	 * is not El Torito.  The primary purpose of this test is to avoid
	 * trying to read from a device whose block size is too large for
	 * our buffer.  We do not reject devices that report block sizes
	 * smaller than 2K because we have seen at least one El Torito BIOS
	 * report a block size of 512.
	 */
	intregs.intval = DEVT_DSK;
	intregs.ax = INT13_CHKEXT << 8;
	intregs.bx = 0x55AA;
	intregs.dx = unit;

	if (!doint_r(&intregs) && intregs.bx == 0xAA55 && (intregs.cx & 1))  {
		unsigned short bps = 0;

		extparms->bufsize = 30;
		extparms->bps = 0;

		intregs.intval = DEVT_DSK;
		intregs.ax = INT13_EXTPARMS << 8;
		intregs.dx = unit;
		intregs.ds = segpart((unsigned long)extparms);
		intregs.si = offpart((unsigned long)extparms);

		if (!doint_r(&intregs) && ((intregs.ax & 0xFF00) == 0)) {
			bps = extparms->bps;
		}

		/*
		 * We could increase the permitted size to DISK_CACHESIZE
		 * if we find BIOSes that require it.  For now we assume
		 * that all BIOSes give the correct value or 512.
		 */
		if (bps > 2048) {
			Dprintf(("is_eltorito_dev: LBA size is %d\n", bps));
			return (0);
		}
	}

	/*
	 * Attempt an LBA read of block 0x11.  If it looks like an El Torito
	 * BRVD assume we are booted from an El Torito device.  Borrow
	 * the shared cache memory as a buffer.
	 */
	invalidate_cache(NULL);
	buf = (char *)cache_info.memp;	/* Borrow cache buffer */

	(void) memset((char *)pktp, 0, sizeof (*pktp));
	ic.intval = DEVT_DSK;
	ic.ax = INT13_EXTREAD << 8;
	ic.dx = unit;
	pktp->size = sizeof (*pktp);
	pktp->nblks = 1;
	pktp->bufp = mk_farp((ulong_t)buf);
	pktp->lba_lo = 0x11;
	pktp->lba_hi = 0;
	pktp->bigbufp_lo = pktp->bigbufp_hi = 0;
	ic.ds = segpart((unsigned long)pktp);
	ic.si = offpart((unsigned long)pktp);

	if (!doint()) {
		if (buf[0] == 0 && strncmp(buf + 1, "CD001", 5) == 0 &&
		    strncmp(buf + 7, "EL TORITO SPECIFICATION", 23) == 0) {
			Dprintf(("is_eltorito_dev: found BRVD\n"));
			answer = 1;
			eltorito_dev = unit;
		}
	}

	Dprintf(("is_eltorito_dev: device %s is%s El Torito\n", unit,
		answer ? "" : " not"));
	return (answer);
}

/*
 * Determine the type of the initial program load device, i.e. the
 * device from which boot.bin was loaded.
 */
int
prom_get_ipl_device_type(void)
{
	static int ipl_device_type = BDT_UNKNOWN;

	/*
	 * When booting with DevConf the boot device must be one of the
	 * supported firmware IPL devices (presently diskette, CDROM or
	 * hard drive).
	 *
	 * When booted old-style (which includes DevConf boot directly
	 * out of a Solaris partition but not out of a Solaris boot
	 * partition) assume it is a hard drive only if it is for the
	 * BIOS primary device.
	 */
	if (ipl_device_type == BDT_UNKNOWN) {
		if (IS_FLOPPY(BootDev))
			ipl_device_type = BDT_FLOPPY;
		else if (is_eltorito_dev(BootDev))
			ipl_device_type = BDT_CDROM;
		else if (Oldstyleboot && BootDev != 0x80)
			/*
			 * Oldstyleboot really means either pre-DevConf
			 * boot *or* boot from a hard drive.  If the
			 * device code is not 0x80 it must have been
			 * a pre-DevConf boot.
			 */
			ipl_device_type = BDT_UNKNOWN;
		else
			ipl_device_type = BDT_HARD;
	}
	return (ipl_device_type);
}

/*
 * Determine the name of the extend device based on the IPL device
 * type.
 *
 * Note that this routine does not get called when doing a hard drive
 * boot out of a Solaris partition because bootblk passes a non-null
 * realp which sets Oldstyleboot.  boot_compfs_mountroot sets the
 * extend device to FLOPPY0_NAME in that case.  Hard drive boot via
 * a Solaris boot partition *does* call this routine because it uses
 * strap.com which passes a null realp.
 *
 * XXX - The present algorithm will give the hard drive for a Solaris
 * boot partition.  Is that correct?  That's what the old code did.
 */
char *
prom_get_extend_name(void)
{
	char *extend_name;

	switch (prom_get_ipl_device_type()) {
	case BDT_CDROM:
		extend_name = FLOPPY0_NAME;
		break;
	default:
		extend_name = BOOT_DEV_NAME;
		break;
	}
	return (extend_name);
}

int
prom_open(char *path)
{
	/*
	 *  Open a device:
	 *
	 *  Returns the device handle (file descriptor), or 0 if it fails.
	 *  If the device we're looking for is a disk, we also probe for
	 *  the geometry (and maybe the partition info) and save this stuff
	 *  in the ihandle struct gets bound to the file descriptor.
	 */
	int OpenError = OpenCount;
	struct ihandle *ihp;
	char *cp, *xp = 0;
	int j, k = 0;

	int dev_type = BootDevType;
	int dev_addr = BootDev;
	int dev_part = 0;

	Dprintf(("prom_open(%s)\n", path ? path : ""));
	if (realp != 0) {
		/*
		 *  If we were booted by the old (pre 2.5) "blueboot", the
		 *  default device type and address are stored in the
		 *  "bootfrom" field of primary/secondary boot communication
		 *  structure.
		 */
		Dprintf(("prom_open: before adj, BootDev = %x, dev_part %d, "
			"dev_type %x\n", BootDev, dev_part, dev_type));
		BootDev = dev_addr = realp->bootfrom.ufs.boot_dev;
		dev_part = realp->bootfrom.ufs.root_slice;
		BootDevType = dev_type = (realp->F8.dev_type == MDB_NET_CARD) ?
		    DEVT_NET : DEVT_DSK;

		realp = 0;	/* We don't need this any more! */
		path = 0;	/* Don't care what the path was */
		Dprintf(("prom_open: after adj, BootDev = %x, dev_part %d, "
			"dev_type %x\n", BootDev, dev_part, dev_type));
	}

	if (!path || !*path) {
		/*
		 *  An incomplete pathname implies the boot device.  Use a
		 *  dummy path name for this device which is slightly different
		 *  from the BOOT_DEV_NAME.  This lets us have two file
		 *  descriptors open on the device, one for the DOS file
		 *  system, one for the ufs system.
		 */
		static char dummy_path[sizeof (BOOT_DEV_NAME)+4];

		(void) sprintf(dummy_path, "%s:a", BOOT_DEV_NAME);
		path = dummy_path;

		/*
		 * When booting from CDROM, ask get_vtoc() to report
		 * the FS type.  See comment in disk.c open_disk()
		 * for a description of new_root_type usage.
		 */
		if (prom_get_ipl_device_type() == BDT_CDROM)
			new_root_type = NULL;
	}

	for (cp = strrchr(path, 0); cp; xp = cp) {
		/*
		 *  Search for the "boot-interface" property for this device.
		 *  This property tells us how to perform I/O to the
		 *  corresponding device and may be attached to any node in the
		 *  device path.  We have to search for it in a bottom-up
		 *  fashion (i.e, looking at the rightmost pathname component
		 *  first).
		 */
		struct dnode *dnp;
		int n;

		*cp = '\0';
		dnp = find_node(path, active_node);

		if (dnp &&
		    ((n = bgetproplen(&bootops, "boot-interface",
		    dnp->dn_nodeid)) >= (int)sizeof (int))) {
			/*
			 *  We have a "boot-interface" property, but we don't
			 *  know whether or not it's legal.  The first word of
			 *  the property value gives the device type ...
			 */
			int *ip = (int *)bkmem_alloc(n);

			if (xp) *xp = '/';
			if (ip == (int *)0) {
				printf("can't get bytes for boot-interface\n");
				return (0);
			}
			(void) bgetprop(&bootops, "boot-interface", (caddr_t)ip,
			    n, dnp->dn_nodeid);

			switch (dev_type = *ip) {
				/*
				 *  Parse remainder of the the "boot-interface"
				 *  property according to the device type code
				 *  appearing in the first word.
				 */
				case DEVT_DSK:
				    dev_addr = find_disk(ip, n, path);
				    break;
				case DEVT_NET:
				    dev_addr = find_net(ip, n, path);
				    break;
				default:
				    dev_addr = -1;
				    break;
			}

			/*
			 *  Free up the property buffer and check result of
			 *  device parse.  If "dev_addr" is -1, it means the
			 *  parse fails and we should deliver an error.
			 */
			bkmem_free((char *)ip, n);
			if (dev_addr == -1) goto erx;
			OpenError = 0;
			break;

		/*
		 *  Below are two new special kluge cases.  We want to be
		 *  able to open floppy devices that weren't the boot device.
		 */
		} else if (strcmp(path, FLOPPY0_NAME) == 0) {
			OpenError = 0;
			dev_type = DEVT_DSK;
			dev_addr = 0;
			dev_part = 0;
			break;
		} else if (strcmp(path, FLOPPY1_NAME) == 0) {
			OpenError = 0;
			dev_type = DEVT_DSK;
			dev_addr = 1;
			dev_part = 0;
			break;
		}

		cp = strrchr(path, '/'); /* Back up to next path component & */
		if (xp) *xp = '/';	 /* .. replace the slash we removed, */
	}

	if (OpenError) {
		/*
		 *  If this is the very first open (or we're explictly opening
		 *  the boot device), we can use the default device type and
		 *  device addr (as provided by the boot loader) if we can't
		 *  find a boot-interface property for the indicated device.
		 *  If this is NOT the first call to prom_open, however, we
		 *  fail if we can't find a boot-interface property.
		 */

erx:		printf("%s: can't open - invalid boot interface\n", path);
		return (0);
	}

	for (j = 1; j < MAXDEVOPENS; j++) {
		/*
		 *  Search the list of open devices for an unused entry to
		 *  associate with the open device.  Note that we skip the
		 *  first entry in the "open_devices" table (which is reserved
		 *  for stdin/stdout).
		 */
		if ((ihp = open_devices[j]) == 0) {
			/*
			 *  If this ihandle pointer is unused, we can use it
			 *  for a new ihandle struct (if we're forced to
			 *  allocate one).
			 */
			k = j;

		} else if (((strcmp(path, ihp->pathnm) == 0) &&
		    (!new_root_type ||
		    (strcmp(new_root_type, ihp->fstype) == 0))) ||
		    (new_root_type && (dev_type == ihp->type) &&
		    (dev_addr == ihp->unit) &&
		    (strcmp(new_root_type, ihp->fstype) == 0))) {
			/*
			 *  We already have an ihandle open on the requested
			 *  device.  All we have to do is increment the use
			 *  count and return its table index.
			 */
			if (!new_root_type) new_root_type = ihp->fstype;
			ihp->usecnt += 1;
			return (j);
		}
	}

	if (k && (ihp = (struct ihandle *)bkmem_alloc(sizeof (*ihp)))) {
		/*
		 *  We found an unused ihandle entry.  Allocate an "ihandle"
		 *  struct and use the device-dependent "open" routines to
		 *  initialize its contents.  Start by allocating a buffer
		 *  to hold the path name.
		 */

		if (ihp->pathnm = bkmem_alloc(strlen(path)+1)) {
			/*
			 *  We successfully allocated a pathname buffer, copy
			 *  the path name into it and set up the rest of the
			 *  ihandle struct.
			 */
			ihp->usecnt = 1;
			ihp->type = (unsigned char)dev_type;
			ihp->unit = (unsigned char)dev_addr;
			(void) strcpy(ihp->pathnm, path);

			switch (dev_type) {

			case DEVT_DSK:
				ihp->dev.disk.num = dev_part;
				if (open_disk(ihp)) k = 0;
				break;

			case DEVT_NET:
				if (open_net(ihp)) k = 0;
				break;

			default:
				k = 0;
				break;
			}

			if (k > 0) {
				/*
				 *  Device-specific initialization is
				 *  complete.  Plant a ptr to the new ihandle
				 *  structure and return its index.
				 */
				if ((strcmp(path, BOOT_DEV_NAME) != 0) &&
				    (strcmp(path, FLOPPY0_NAME) != 0))
					OpenCount++;
				(void) strcpy(ihp->fstype, new_root_type);
				open_devices[k] = ihp;
				return (k);
			} else {
				bkmem_free(ihp->pathnm, strlen(path)+1);
				bkmem_free((char *)ihp,
				    sizeof (struct ihandle));
				return (0);
			}
		}

		printf("%s: can't open - out of memory\n", path);
		bkmem_free((char *)ihp, sizeof (struct ihandle));
		return (0);
	}

	printf("%s: can't open - %s\n", path,
	    k ? "no memory" : "too many open devices");
	return (0);
}

int
prom_close(int fd)
{
	/*
	 *  Close a device file:
	 *
	 *  Returns 0 if it works, -1 otherwise (e.g. if the indicated device
	 *  wasn't open).
	 */

	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  If file descriptor is valid, perform close processing
		 *  according to device type.  If the device isn't open,
		 *  device type code in the corresponding ihandle struct
		 *  will be null!
		 */
		if (!fd || --(ihp->usecnt)) {
			/*
			 *  If device is still in use (or if device is
			 *  stdin/stdout), simply decrement the use count
			 *  and return.
			 */
			return (0);
		}

		switch (ihp->type) {
			/*
			 *  Perform device-dependent close processing (if any)
			 *  before freeing up the ihandle struct.
			 */
			case DEVT_DSK:	close_disk(ihp); break;
			case DEVT_NET:	close_net(ihp);  break;
		}

		bkmem_free(ihp->pathnm, strlen(ihp->pathnm)+1);
		bkmem_free((char *)ihp, sizeof (*ihp));
		open_devices[fd] = 0;
		return (0);
	}

	return (-1);	/* Bogus file descriptor!	*/
}

/*ARGSUSED*/
int
prom_seek(int fd, unsigned long long offset)
{
	int	high = (offset >> 32) & 0xffffffff;

	/*
	 *  Seek to given offset:
	 *
	 *  We don't need seek as a separate operation; Read/write calls are
	 *  always relative to the "startblk" argument.  All we do here is val-
	 *  idate the file descriptor and make sure the "high" order 32 bits of
	 *  the seek offset are null.
	 */
	return (-((fd < 0) || (fd >= MAXDEVOPENS) || !devp(fd) || high));
}

int
prom_devreset(int fd)
{
	/*
	 *  Reset device:
	 *
	 *  Currently only implemented for disk devices.
	 *
	 */
	struct ihandle *ihp = devp(fd);

	if (ihp->type == DEVT_DSK)
		return (reset_disk(ihp));
	else
		return (0);
}

/*ARGSUSED*/
int
prom_read(int fd, caddr_t buf, uint_t len, uint_t startblk, char devtype)
{
	/*
	 *  Read from a device:
	 *
	 *  The real work is done in one of the device-specific "read" modules,
	 *  all we have to do is figure out which one to call!
	 *
	 *  startblk is in 512-byte sectors.
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to select a read method.
		 *
		 *  NOTE: Stdin/stdout (fd zero) is the only "serial" device
		 *  that's currently supported.
		 */
		switch (ihp->type) {
		case DEVT_DSK:
			return (read_disk(ihp, buf, len, startblk));
		case DEVT_NET:
			return (read_net(ihp, buf, len, startblk));
		case DEVT_SER:
			if (!fd)
				return (read_stdin(ihp, buf, len));
			break;
		default:
			return (-1);
		}
	}

	return (-1);	/* Bogus file descriptor	*/
}

/*ARGSUSED*/
int
prom_write(int fd, caddr_t buf, uint_t len, uint_t startblk, char devtype)
{
	/*
	 *  Write to a device:
	 *
	 *  As with read, all we do here is call a device-dependent routine
	 *  based on the "type" field of the ihandle struct bound to the
	 *  caller's file descriptor.
	 */

	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to select a write method.
		 *
		 *  NOTE: Stdin/stdout (fd zero) is the only "serial" device
		 *  that's currently supported.
		 */
		switch (ihp->type) {

		case DEVT_DSK:
			return (write_disk(ihp, buf, len, startblk));
		case DEVT_NET:
			return (write_net(ihp, buf, len, startblk));
		case DEVT_SER:
			if (!fd)
				return (write_stdout(ihp, buf, len));
		default:
			return (-1);
		}
	}

	return (-1);	/* Bogus file descriptor	*/
}

unsigned
prom_gettime(void)
{
	/*
	 *  Read system timer:
	 *
	 *  Return milliseconds since last time counter was reset.
	 *  The timer ticks 18.2 times per second or approximately
	 *  55 milliseconds per tick.
	 *
	 *  The counter will be reset to zero by the bios after 24 hours
	 *  or 1,573,040 ticks. The first read after a counter
	 *  reset will flag this condition in the %al register.
	 *  Unfortunately, it is hard to take advantage of this
	 *  fact because some broken bioses will return bogus
	 *  counter values if the counter is in the process of
	 *  updating. We protect against this race by reading the
	 *  counter until we get consecutive identical readings.
	 *  By doing so, we lose the counter reset bit. To make this
	 *  highly unlikely, we reset the counter to zero on the
	 *  first call and assume 24 hours is enough time to get this
	 *  machine booted.
	 *
	 *  An attempt is made to provide a unique number on each
	 *  call by adding 1 millisecond if the 55 millisecond counter
	 *  hasn't changed. If this happens more than 54 times, we
	 *  return the same value until the next real tick.
	 *
	 */
	static unsigned lasttime = 0;
	static short fudge = 0;
	unsigned ticks, mills, first, tries;

	if (lasttime == 0) {
		/*
		 * initialize counter to zero so we don't have to
		 * worry about 24 hour wrap.
		 */
		(void) memset(&ic, 0, sizeof (ic));
		ic.ax = 0x0100;
		ic.intval = 0x1A;
		(void) doint();
	}
	tries = 0;
	do {
		/*
		 * Loop until we trust the counter value.
		 */
		(void) memset(&ic, 0, sizeof (ic));
		ic.intval = 0x1A;
		(void) doint();
		first = (ic.cx << 16) + (ic.dx & 0xFFFF);
		(void) memset(&ic, 0, sizeof (ic));
		ic.intval = 0x1A;
		(void) doint();
		ticks = (ic.cx << 16) + (ic.dx & 0xFFFF);
	} while (first != ticks && ++tries < 10);
	if (tries == 10)
		printf("prom_gettime: BAD BIOS TIMER\n");

	mills = ticks*55;
	if (mills > lasttime) {
		fudge = 0;
	} else {
		fudge += (fudge < 54) ? 1 : 0;
	}
	mills += fudge;
	lasttime = mills;
	return (mills);
}

int
prom_getmacaddr(int fd, unsigned char *eap)
{
	/*
	 *  Obtain machine's ethernet address:
	 *
	 *  This operation is only legal for network devices!
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd > 0) && (fd < MAXDEVOPENS) && ihp && (ihp->type == DEVT_NET)) {
		/*
		 *  File descriptor is legit, and this is a real network device.
		 *  Call macaddr_net() to do the real work!
		 */
		return (macaddr_net(ihp, eap));
	}

	return (-1);
}

caddr_t
prom_alloc(caddr_t virthint, uint_t size, int align)
{	/* Use virtual memory mapper to allocate memory */

	return ((caddr_t)map_mem((uint_t)virthint, size, align));
}

void
prom_panic(char *str)
{	/* Print panic string, then blow up! */

	printf("prom_panic: %s\n", str);
	bootabort();
}

#ifdef XXX
void
prom_enter_mon()
{	/* There is no monitor; Wait for keystroke instead */

	(void) goany();
}
#endif

char *
prom_bootargs()
{	/* For now, we're just returning a constant */

	return ("kernel/unix");
}

/*
 * Defines for accessing BIOS real time clock.
 */
#define	BIOS_REALTIME_CLK_INT		0x1A
#define	BIOS_REALTIME_GETTIME_FN	0x2
#define	BIOS_REALTIME_GETDATE_FN	0x4

void
prom_rtc_time(ushort_t *hours, ushort_t *mins, ushort_t *secs)
{
	/*
	 * Read time from the Realtime clock.
	 */
	struct real_regs *rr;
	struct real_regs regs;

	/* Avoid realmode memory allocation if stack is in RM arena */
	if ((ulong)&regs < TOP_RMMEM) {
		rr = &regs;
	} else if (!(rr = alloc_regs()))
		prom_panic("No low memory for RTC timestamp");

	AH(rr) = BIOS_REALTIME_GETTIME_FN;
	(void) doint_asm(BIOS_REALTIME_CLK_INT, rr);

	if (rr->eflags & CARRY_FLAG) {
		printf("No RTC timer!?");
		*hours = *mins = *secs = 0;
	} else {
		*hours = bcd_to_bin((ushort_t)(CH(rr)));
		*mins  = bcd_to_bin((ushort_t)(CL(rr)));
		*secs  = bcd_to_bin((ushort_t)(DH(rr)));
	}

	if (rr != &regs)
		free_regs(rr);
}

void
prom_rtc_date(ushort_t *year, ushort_t *month, ushort_t *day)
{
	/*
	 * Read date from the Realtime clock.
	 */
	struct real_regs *rr;
	struct real_regs regs;

	/* Avoid realmode memory allocation if stack is in RM arena */
	if ((ulong)&regs < TOP_RMMEM) {
		rr = &regs;
	} else if (!(rr = alloc_regs()))
		prom_panic("No low memory for RTC datestamp");

	AH(rr) = BIOS_REALTIME_GETDATE_FN;
	(void) doint_asm(BIOS_REALTIME_CLK_INT, rr);

	if (rr->eflags & CARRY_FLAG) {
		printf("No RTC date!?");
		*year = *month = *day = 0;
	} else {
		*year = bcd_to_bin(CX(rr));
		*month = bcd_to_bin((ushort_t)(DH(rr)));
		*day = bcd_to_bin((ushort_t)(DL(rr)));
	}

	if (rr != &regs)
		free_regs(rr);
}

#ifdef	HAS_CACHEFS
int
prom_devicetype(int fd, char *devtype)
{
	/*
	 *  Determine if device is "network" or "block"
	 *
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to check device type
		 *
		 */
		if ((ihp->type == DEVT_DSK) && (strcmp(devtype, "block") == 0))
			return (1);

		if ((ihp->type == DEVT_NET) &&
		    (strcmp(devtype, "network") == 0))
			return (1);

		return (0);	/* device type is not "devtype" */
	}

	return (0);		/* bad file descriptor	*/
}
#endif	/* HAS_CACHEFS */
