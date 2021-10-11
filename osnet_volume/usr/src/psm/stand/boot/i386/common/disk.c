/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)disk.c	1.60	99/10/07 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vtoc.h>
#include <sys/bootdef.h>
#include <sys/bootcmn.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/sysmacros.h>
#include <sys/ihandle.h>
#include <sys/salib.h>
#include <sys/promif.h>

extern caddr_t rm_malloc(size_t size, u_int align, caddr_t virt);
extern void rm_free(caddr_t, size_t);
extern int doint(void);
extern int doint_r(struct int_pb *ic);
extern int is_eltorito_dev(unsigned char unit);
extern void *memcpy(void *s1, void *s2, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int bd_getalts(struct ihandle *ihp, int prt);
extern void bd_freealts(struct ihandle *ihp);
extern int bd_readalts(struct ihandle *ihp, daddr_t off, int cnt);
extern long strtol(char *, char **, int);
extern void nosnarf_printf(char *fmt, ...);

extern int BootDev;
extern struct int_pb ic;
extern char *new_root_type;

static unsigned int io_conv(struct ihandle *ihp, unsigned char *device);
int reset_disk(struct ihandle *ihp);

#ifdef	notdef
/*
 * Turn this on if you want to enable debugging of the stand-alone pcfs.
 */
#define	DEBUG
#endif	/* notdef */

#ifdef DEBUG
void dump_ihp(struct ihandle *ihp);
#endif

/*
 * We maintain a single shared input cache for all "ihandle" devices.
 *
 * In the past we used to allocate a separate cache for each open
 * device.  Sometimes the attempt to mount the boot device via bootops
 * from bootconf would fail because there was not enough space for
 * the cache.  To prevent this problem, and to limit the memory used
 * for caches, we now allocate a single shared cache of 63 sectors.
 * This cache is large enough to handle any geometry-based device track.
 *
 * Before adding LBA support, track cache sizes were determined by
 * the device geometry which forced a limit of 63 sectors.  For LBA
 * devices cache size is totally arbitrary.
 *
 * The limit of 63 sectors is also designed to avoid any problems with
 * realmode drivers that were not designed to handle more than 63-sector
 * reads or were not tested with them.  ata.bef has been seen to fail
 * on larger reads.
 *
 * For devices with small enough tracks, we divide the shared cache
 * into multiple buffers to improve performance.
 *
 * To use the shared I/O cache buffer as a temporary buffer, call
 * invalidate_cache(NULL) then use up to cache_info.csize bytes at
 * cache_info.memp.  See net.c for an example.
 *
 * To use the multiple buffer support, call setup_cache_buffers to
 * divide the cache memory into buffers.  This call must be remade
 * every time another device could have used the cache.
 *
 * During a read operation, check for the presence of a sector in the
 * cache by calling get_cache_buffer which will activate the buffer
 * containing the requested sector, if found, and return 1.  The caller
 * can retrieve data from the active buffer using the buffer start
 * (cache_info.active_buffer), buffer size (ihp->dev.disk.csize) and
 * the address of the first sector in the buffer (cache_info.active_sector).
 *
 * If the requested sector is not found in the cache, get_cache_buffer
 * activates an empty buffer and returns 0.  The caller is responsible
 * for reading the data and can record the new contents for future cache
 * searches by calling set_cache_buffer.
 *
 * When a device is closed, or I/O bypasses the cache, call invalidate_cache
 * giving the device ihandle pointer as the argument to avoid keeping stale
 * data.
 *
 * If you wish to use read_blocks to bypass the cache, you must call
 * setup_cache_buffers for the device first because read_blocks uses the
 * active buffer not the whole cache.
 *
 * DO NOT make private copies of active_buffer because it can change for
 * devices where multiple buffers are maintained.
 */

static int DiskDebug = 0;
int SilentDiskFailures = 0;
struct shared_cache_info cache_info;

void
setup_cache_buffers(struct ihandle *ihp)
{
	int i;

	/*
	 * If we do not yet have a shared buffer, allocate it.
	 * Put it on a 32K boundary to prevent diskette DMA errors.
	 * Note that calling with a null handle guarantees that
	 * the cache has been allocated.
	 */
	if (cache_info.memp == 0) {
		cache_info.csize = DISK_CACHESIZE;
		cache_info.memp = rm_malloc(cache_info.csize, 0x8000, 0);
		if (cache_info.memp == 0)
			prom_panic("setup_cache_buffers: failed to allocate "
				"memory for shared cache.");
		cache_info.owner = -1;
	}

	if (ihp == NULL) {
		cache_info.owner = -1;
	} else if (cache_info.owner != ihp->unit) {
		if (ihp->type == DEVT_DSK) {
			cache_info.count = cache_info.csize /
						ihp->dev.disk.csize;
			if (cache_info.count == 0)
				prom_panic("setup_cache_buffers: shared "
					"cache too small.");
			if (cache_info.count > CACHE_MAXBUFFERS)
				cache_info.count = CACHE_MAXBUFFERS;
			for (i = 0; i < cache_info.count; i++) {
				cache_info.multi[i].cachep = cache_info.memp +
					i * ihp->dev.disk.csize;
				cache_info.multi[i].cfirst = CACHE_EMPTY;
			}
		}
		cache_info.owner = ihp->unit;
	}
}

void
invalidate_cache(struct ihandle *ihp)
{
	/*
	 * If called with a null handle, invalidate the cache no matter who
	 * owns it.  Otherwise just invalidate it if the owner matches.
	 */
	if (ihp == NULL || cache_info.owner == ihp->unit)
		cache_info.owner = -1;

	/*
	 * Make sure that the cache memory has been allocated so that
	 * invalidate_cache(NULL) can be used when borrowing the cache
	 * buffer.
	 */
	if (cache_info.memp == 0)
		setup_cache_buffers(NULL);
}

/*
 * get_cache_buffer - return 1 if the requested sector is within the cache,
 * otherwise 0.
 *
 * On return, the matching buffer or buffer to use will be the first
 * one in the table.
 *
 * Note that although we presently always align LBA reads on a "track"
 * boundary (where a "track" is whatever fits in the buffer) we do
 * not make that assumption in this routine.  In future we might decide
 * to align reads according to the first requested block rather than
 * a "track" boundary.
 */
int
get_cache_buffer(struct ihandle *ihp, u_int off)
{
	int i;
	int first_avail;
	int match;
	int prev;
	char *cachep;
	unsigned long cfirst;
	unsigned long csectors;

	setup_cache_buffers(ihp);

	csectors = ihp->dev.disk.csize / DEV_BSIZE;

	/*
	 * Scan the list of buffers, looking for the first empty slot,
	 * an entry that contains what we need or an entry that is
	 * the track before the one we want.
	 *
	 * If the first slot contains the match just return immediately
	 * because there is nothing more to do.
	 */
	match = prev = first_avail = CACHE_MAXBUFFERS;
	for (i = 0; i < cache_info.count; i++) {
		cfirst = cache_info.multi[i].cfirst;
		if (cfirst == CACHE_EMPTY) {
			if (i < first_avail)
				first_avail = i;
			continue;
		}
		if (off >= cfirst && off < cfirst + csectors) {
			if (i == 0) {
				return (1);
			}
			match = i;
		} else if (off >= cfirst + csectors &&
		    off < cfirst + 2 * csectors) {
			prev = i;
		}
	}

	/*
	 * Choose (in order of preference):
	 *	1) a matching slot
	 *	2) the first empty slot
	 *	3) a slot containing the previous track
	 *	4) the last slot
	 *
	 * The reason for reusing the slot containing the previous track
	 * is that sequential file reads are pretty common and the chances
	 * of revisiting previous tracks are low.  So it is better to
	 * keep other things (such as FATs or directories) which might
	 * reduce both the number of track reads and the amount of seeking.
	 */
	if (match == CACHE_MAXBUFFERS) {
		if (first_avail < CACHE_MAXBUFFERS) {
			match = first_avail;
		} else if (prev < CACHE_MAXBUFFERS) {
			match = prev;
			cache_info.multi[match].cfirst = CACHE_EMPTY;
		} else {
			match = cache_info.count - 1;
			cache_info.multi[match].cfirst = CACHE_EMPTY;
		}
	}

	/* Move the chosen slot to the top */
	if (match != 0) {
		cachep = cache_info.multi[match].cachep;
		cfirst = cache_info.multi[match].cfirst;
		for (i = match; i > 0; i--) {
			cache_info.multi[i].cachep =
				cache_info.multi[i - 1].cachep;
			cache_info.multi[i].cfirst =
				cache_info.multi[i - 1].cfirst;
		}
		cache_info.active_buffer = cachep;
		cache_info.active_sector = cfirst;
	}

	return (cache_info.active_sector == CACHE_EMPTY ? 0 : 1);
}

void
set_cache_buffer(u_int off)
{
	cache_info.active_sector = off;
}

static u_int
cachesize(struct ihandle *ihp)
{
	static int lba_cachesize = DISK_CACHESIZE;

	if (!IS_FLOPPY(ihp->unit) && ihp->dev.disk.lbamode) {
		/*
		 * For hard drives and CDROMs, when in LBA mode, use the LBA
		 * cache size, rounded down to a multiple of the block size.
		 */
		return ((lba_cachesize / ihp->dev.disk.bps) *
			ihp->dev.disk.bps);
	} else {
		/* everyone else gets the boring old cache */
		return (ihp->dev.disk.bps * ihp->dev.disk.spt);
	}
}

int
find_disk(int *table, int n, char *path)
{
	/* BEGIN CSTYLED */
	/*
	 *  Open disk unit:
	 *
	 *    The "boot-interface" property for disks consists of an array of
	 *    triplets (plus a leading "type" word).  These triplets are in-
	 *    terpreted as entries in a table with the following format:
	 *
	 *       +---------------+---------------+--------------------+
	 *       |  target addr  |  unit number  |  bios drive number |
	 *       +---------------+---------------+--------------------+
	 *
	 *    This routine extracts the target and lun from the device "path"
	 *    and returns the bios drive number from the "n"-entry
	 *    boot-interface "table" that matches these extracted values.
	 *    Columns containing -1 will match extracted value (including no
	 *    extracted value).
	 *
	 *    This routine returns -1 if it can't match the target/lun specified
	 *    in the path name.
	 */
	/* END CSTYLED */
	if (!(n % sizeof (int)) && (((n /= sizeof (int)) % 3) == 1)) {
		/*
		 *  The boot-interface property length is reasonable, get
		 *  ready to search the target/lun table.
		 */
		char *cp = strrchr(path, '/');
		int tar = -1, lun = -1;

		if (cp = strrchr((cp ? cp : path), '@')) {

			/*
			 * There's a unit address attached to the last
			 * component of the path name.  Extract the
			 * target and lun values from this addr.
			 * (NOTE: they're in hex, so use base 16 in
			 * strtol() call.) We're not very forgiving
			 * here:  If either value is missing the open
			 * will fail.
			 */

			tar = (int)strtol(cp+1, &cp, 16);
			if (!*cp || (*cp++ != ','))
				return (-1);
			lun = (int)strtol(cp, 0, 16);
		}

		for (n--; n > 0; (n -= 3, table += 3)) {
			/*
			 *  Now step thru the boot-interface table looking for
			 *  a target/lun pair that matches the values we
			 *  extracted from the unit address.
			 */
			if (((table[1] == -1) || (table[1] == tar)) &&
			    ((table[2] == -1) || (table[2] == lun))) {
				/*
				 *  This is it!  Return the bios drive number
				 *  field of the current boot-interface table
				 *  entry.
				 */
				return (table[3]);
			}
		}
	}

	return (-1);	/* bogus (or missing) unit address! */
}

void
check_lba_support(struct ihandle *ihp)
{
	struct int_pb intregs;

	/*
	 * For now we use LBA under either of two circumstances:
	 *
	 * 1) the BIOS reports LBA support for the device according to
	 * the EDD 3.0 standard, specifically the fixed disk access subset.
	 *
	 * 2) the BIOS reports that the device is an El Torito boot device.
	 * In this case we make some reasonable assumptions.
	 *
	 * This algorithm may need to be adjusted as we gain experience
	 * with BIOS behaviors.
	 */

	/* First look to see if the EDD fixed disk access subset is supported */
	intregs.intval = DEVT_DSK;
	intregs.ax = INT13_CHKEXT << 8;
	intregs.bx = 0x55AA;
	intregs.cx = 0;
	intregs.dx = ihp->unit;

	if (!doint_r(&intregs) && intregs.bx == 0xAA55 && (intregs.cx & 1))  {
		int13_extparms_result_t *extparmsp;
		/*
		 * We have seen a BIOS bug on NCR 4300 series machines
		 * which clears the byte at DS:0x74 during the Get Drive
		 * Parameters call.  We will set DS to the last paragraph
		 * boundary before the buffer, so using a union to force
		 * the buffer to be 128 bytes works around the problem.
		 */
		union {
			int13_extparms_result_t extparms;
			char ncr4300_buffer[128];
		} u;

		/* can do packet-mode LBA access */
		ihp->dev.disk.lbamode = 1;

		/* get disk size from Function 48h */

		/* Avoid realmode memory allocation if stack is in RM arena */
		if ((ulong)&u.extparms < TOP_RMMEM) {
			extparmsp = &u.extparms;
		} else {
			extparmsp = (int13_extparms_result_t *)
			    rm_malloc(sizeof (u), sizeof (long), 0);
			if (!extparmsp)
				prom_panic("check_lba_support: cannot get "
				    "extparms mem\n");
		}

		extparmsp->bufsize = 30;

		intregs.intval = DEVT_DSK;
		intregs.ax = INT13_EXTPARMS << 8;
		intregs.dx = ihp->unit;
		intregs.ds = segpart((unsigned long)extparmsp);
		intregs.si = offpart((unsigned long)extparmsp);

		if (!doint_r(&intregs) && ((intregs.ax & 0xFF00) == 0)) {
			/*
			 * BIOSes sometimes report a bogus size.  For
			 * UFS filesystems that does not matter because
			 * the size will be replaced later by the slice
			 * size.
			 *
			 * XXX Might need to implement similar protection
			 * for PCFS partitions.  So far we have seen bogus
			 * size only for El Torito devices.
			 */
			ihp->dev.disk.siz =
			    (unsigned long)extparmsp->phys_numsect *
			    (extparmsp->bps / DEV_BSIZE);
			ihp->dev.disk.bps = extparmsp->bps;
		} else {
			ihp->dev.disk.lbamode = 0;
		}
		if (extparmsp != &u.extparms)
			rm_free((caddr_t)extparmsp, sizeof (u));
	}

	/*
	 * Some El Torito BIOSes either do not implement the full
	 * EDD 3.0 fixed disk access subset or do not report that
	 * they support it in the check extensions present call.
	 * Others report incorrect values for the block size in
	 * the get drive parameters call.  So try using the El
	 * Torito emulation status call, if the device code is a
	 * valid no-emulation boot device code.
	 */
	if (ihp->unit > 0x80 && is_eltorito_dev(ihp->unit)) {
		/*
		 * The device is an El Torito CDROM which must support
		 * LBA mode with a blocksize of 2K.  Assign an arbitrary
		 * size which will be replaced by a number taken from the
		 * VTOC.
		 */
		ihp->dev.disk.lbamode = 1;
		ihp->dev.disk.bps = 2048;
		ihp->dev.disk.siz = 1000;
	}

	if (ihp->dev.disk.lbamode) {
		/* geometry is irrelevant */
		ihp->dev.disk.spc = ihp->dev.disk.spt =
			ihp->dev.disk.cyl = 0xFFFF;

		/*
		 * We have seen at least one hard disk BIOS return a
		 * size of 0 blocks for the boot device.  That is
		 * clearly bogus since we managed to boot from it,
		 * so set an arbitrary size that will be replaced
		 * once we read the VTOC.
		 */
		if (ihp->unit == BootDev && ihp->dev.disk.siz == 0)
			ihp->dev.disk.siz = 1000;
	}
}

int
open_disk(struct ihandle *ihp)
{
	/*
	 *  Open a disk device:
	 *
	 *  This routine issues a bios call to obtain disk geometry of the
	 *  specified "unit" and saves it in the indicated "ihandle" structure.
	 *  If necessary, we also search for the Solaris partition and set up
	 *  the slice offset indicated by the partition specifier (e.g, ":a")
	 *  in the "path" name.  Once we know the disk geometry, we can
	 *  initialize the corresponding sector buffers.
	 *
	 *  Returns 0 if it works, -1 if something goes wrong.
	 */
	int prt = strncmp(ihp->pathnm, BOOT_DEV_NAME, sizeof (BOOT_DEV_NAME)-1);
	char *path = ihp->pathnm;
	struct fdpt *fpp;
	int x;
	struct int_pb intregs;

	if (new_root_type && strcmp(new_root_type, "ufs")) {
		/* BEGIN CSTYLED */
		/*
		 *  Here's how "new_root_type" works:
		 *
		 *  1).  If it's null, it means that the caller doesn't know
		 *       what the file system type is and wants us to probe
		 *       for VTOCs and return the appropriate file type in
		 *       "new_root_type".
		 *
		 *  2).  If it points to a null string, it means that the
		 *       caller is doing a device-open and doesn't care what
		 *       the file system type is.
		 *
		 *  3).  If it points to anything else, it means the caller
		 *       expects to find a file system of the given type on
		 *       the device.
		 *
		 *  Now, if we happen to find a "ufs" file system on the disk,
		 *  we'll need to know what slice to open.  Right now, we're
		 *  defaulting to slice "a" (because "dev.disk.num" is zero),
		 *  but if the caller does not want a "ufs" file system, we'll
		 *  want to bypass the Solaris VTOC search.  We can do this by
		 *  setting "dev.disk.num" to -1.
		 */
		/* END CSTYLED */

		ihp->dev.disk.num = -1;
	} else
		ihp->dev.disk.num = 0;

	ihp->dev.disk.lbamode = 0;
	ihp->dev.disk.bps = DEV_BSIZE;

	if (!IS_FLOPPY(ihp->unit))
		check_lba_support(ihp);

	/*
	 * bd_readalts expects dividing the block size by DEV_BSIZE
	 * to yield a divisor suitable for converting from sectors to
	 * blocks.
	 *
	 * If the divisor would be 0, or if the number of LBA blocks is
	 * reported as 0, give an error.  We have seen this happen
	 * only for no media in a removable drive.  We may need to add
	 * further tests to give different error messages if we find
	 * other cases.  These error messages are seen by the end
	 * user, e.g. while trying to mount the boot device in response
	 * to choosing a device from the CA boot menu.
	 */
	if (ihp->dev.disk.bps < DEV_BSIZE ||
	    (ihp->dev.disk.lbamode && ihp->dev.disk.siz == 0)) {
		if (!SilentDiskFailures) {
			printf("%s: cannot open - no media in drive\n", path);
		}
		return (-1);
	}

	if (ihp->dev.disk.lbamode == 0) {
		intregs.intval = DEVT_DSK;	/* Get the disk geometry */
		intregs.ax = INT13_PARMS << 8;
		intregs.bx = intregs.cx = 0;
		intregs.dx = ihp->unit;

		/*
		 * We do not test for AH == 0 when determining whether
		 * the following call worked because older realmode drivers
		 * failed to clear AH on success.  The framework has since
		 * been corrected, but we want to support the older drivers.
		 */
		if (doint_r(&intregs)) {
			/*
			 * Bios doesn't recognize the device; assume we
			 * have a configuration error.
			 */
			if (!SilentDiskFailures) {
				printf("%s: can't open - "
				    "bios configuration error\n", path);
			}
			return (-1);
		}
		/*
		 * NOTE: This code assumes that the caller has cleared the
		 * "ihandle" struct to zero before calling us.  This
		 * happens automatically when the struct is allocated.
		 */

		x = intregs.cx & 0x3F;
		ihp->dev.disk.spt = (u_short)(x ? x : 32);
		ihp->dev.disk.spc = (u_short)
			(ihp->dev.disk.spt * (((intregs.dx >> 8) & 0xFF) + 1));
		ihp->dev.disk.cyl = ((intregs.cx >> 8) & 0xFF) +
			((intregs.cx << 2) & 0x300) + 1;
		ihp->dev.disk.siz = ihp->dev.disk.cyl * ihp->dev.disk.spc;
	}

	if (!IS_FLOPPY(ihp->unit)) {
		/*
		 * For hard disks and CDROMs we have to initialize the
		 * partition offset and size info.
		 */

		char *cp = strrchr(path, '/');

		if (!ihp->dev.disk.num &&
				(cp = strchr((cp ? cp : path), ':'))) {
			/*
			 *  Solaris slice number is given by the
			 *  partition modifier of the last component
			 *  of the path name (if any).
			 */

			if ((ihp->dev.disk.num = cp[1] - 'a') >= V_NUMPAR) {
				/*
				 *  If the partition number is bogus,
				 *  complain about it but default to
				 *  the 'a' partition.
				 */

				if (!SilentDiskFailures) {
					printf("%s: bogus partition,",
					    path);
					printf(" using \":a\"\n");
				}
				ihp->dev.disk.num = 0;
			}

		} else if (!new_root_type) {
			/*
			 *  If we're looking for a disk of "any" type,
			 *  but user hasn't specified a partition,
			 *  don't assume "ufs"!
			 */

			ihp->dev.disk.num = -1;
		}

	} else switch (intregs.bx & 0xFF) {
		/*
		 *  For floppy disks, device type information is
		 *  returned in registers but geometry must be lifted
		 *  from the nvram address returned in %di.
		 */

	default:	/* This particular floppy is not installed!  */
		if (!SilentDiskFailures) {
			printf("%s: device not installed\n", path);
		}
		return (-1);

	case 1:	/* 360K,  40 track,  5.25 inch */
	case 2:	/* 1.2M,  80 track,  5.25 inch */
	case 3:	/* 720K,  80 track,  3.50 inch */
	case 4:	/* 1.4M,  80 track,  3.50 inch */
	case 5: /* 2.88M, 80 track,  3.50 inch (obscure IBM drive */
	case 6: /* 2.88M, 80 track,  3.50 inch */

		fpp = (struct fdpt *)segoftop(intregs.es, intregs.di);

		ihp->dev.disk.spt = fpp->spt;
		ihp->dev.disk.bps = 128 << fpp->secsiz;
		ihp->dev.disk.spc = ihp->dev.disk.spt * 2;

		if (!prt || !new_root_type) {
			/*
			 *  If caller wants to know the file system
			 *  type, we can assume it's pcfs!
			 */
			new_root_type = "pcfs";

		} else if (prt && *new_root_type &&
				strcmp(new_root_type, "pcfs")) {
			/*
			 *  DOS file systems are all we support on
			 *  floppies, reject attempts to open anything
			 *  else!
			 */
			if (!SilentDiskFailures) {
				printf("%s: only supports pcfs "
					"file system type\n", path);
			}
			return (-1);
		}

		break;
	}

	/*
	 *  We used to send cachesize(ihp) as the size of the
	 *  track cache to allocate for the device.  That macro
	 *  calculates the track size = bps * spt. Unfortunately,
	 *  if we are using a floppy, the spt value may be modified
	 *  later (because the floppy might actually be written at
	 *  a lower density than the drive is capable of handling)
	 *  Then even later we'd free the cache using the cachesize
	 *  calculation and end up freeing too small of a buffer.
	 *
	 *  So now we calculate the max track size the drive is
	 *  capable of supporting and store the size in the device
	 *  ihandle structure.
	 */
	ihp->dev.disk.csize = cachesize(ihp);
	setup_cache_buffers(ihp);

	/*
	 * We now have a cache large enough to hold a full
	 * track's worth of data.  Mark the cache empty and
	 * use "bd_getalts" to read in the vtoc and set the
	 * alternate track map (if any).
	 */
	set_cache_buffer(CACHE_EMPTY);

	if (!IS_FLOPPY(ihp->unit) && bd_getalts(ihp, prt)) {
		/*
		 *  Mounting a hard disk is a bit tricky,
		 *  given that it may contain one of three
		 *  possible file system types: "pcfs",
		 *  "ufs", or "hsfs".  The "bd_getalts"
		 *  routine figures it all out and returns
		 *  zero if all is well.
		 */
		invalidate_cache(ihp);
		return (-1);
	}

	if (!IS_FLOPPY(ihp->unit)) {
		ihp->dev.disk.flop_change_line = 0;
	} else {
		/*
		 *  Check if the floppy has a change line.
		 */
		intregs.intval = DEVT_DSK;
		intregs.ax = INT13_READTYPE << 8;
		intregs.dx = ihp->unit;
		if (!doint_r(&intregs) && (intregs.ax & 0xff00) == 0x200) {
			ihp->dev.disk.flop_change_line = 1;
		} else {
			ihp->dev.disk.flop_change_line = 0;
		}
	}
	return (0);
}

void
close_disk(struct ihandle *ihp)
{
	/*
	 *  Close a disk device:
	 *
	 *  All we have to do here is free up the alternate track map and
	 *  the cache buffer we allocated at open.
	 */
	bd_freealts(ihp);
	invalidate_cache(ihp);
}

int
reset_disk(struct ihandle *ihp)
{
	ic.dx = ihp->unit;
	ic.intval = DEVT_DSK;
	ic.ax = INT13_RESET << 8;
	ic.cx = 0;
	return (doint());
}

#ifdef DEBUG
void
dump_ihp(struct ihandle *ihp)
{
	nosnarf_printf("ihp: 0x%x\n", ihp);
	nosnarf_printf("type 0x%x, unit %d, ref %d\n",
	    ihp->type, ihp->unit, ihp->usecnt);
	nosnarf_printf("fstype %s name %s\n",
	    ihp->fstype ? ihp->fstype : "NONE",
	    ihp->pathnm ? ihp->pathnm : "NONE");

	nosnarf_printf("alt %x %s siz %x par %x cyl %x bas %x \n"
	    "bps %x spt %x spc %x csize %x num %x\n",
	    ihp->dev.disk.alt,
	    ihp->dev.disk.lbamode ? "LBA" : "CHS",
	    ihp->dev.disk.siz,
	    ihp->dev.disk.par,
	    ihp->dev.disk.cyl,
	    ihp->dev.disk.bas,
	    ihp->dev.disk.bps,
	    ihp->dev.disk.spt,
	    ihp->dev.disk.spc,
	    ihp->dev.disk.csize,
	    ihp->dev.disk.num);
}
#endif /* DEBUG */

int
read_disk(struct ihandle *ihp, caddr_t buf, u_int len, u_int off)
{
	/*
	 * Read from disk:
	 *
	 * This routine reads "len" bytes into the specified "buf"fer,
	 * starting at slice-relative disk sector "off"set.  Note that "len"
	 * is given in bytes while "off" is given in 512-byte sectors.
	 *
	 * Routine may perform a short read if the "off"set is near the end of
	 * the partition/device.  The value returned is the number of bytes
	 * read (or -1 if there's an error).
	 */
	int j, k, bc = len;
	int csize_sectors;

	if (ihp->dev.disk.csize == 0)
		ihp->dev.disk.csize = cachesize(ihp);

	csize_sectors = ihp->dev.disk.csize / DEV_BSIZE;

	if (ihp->dev.disk.csize < ihp->dev.disk.bps)
		prom_panic("read_disk: cache is smaller than one block.\n");

	if ((off + ((bc + DEV_BSIZE - 1) / DEV_BSIZE)) >
			ihp->dev.disk.siz) {
		/*
		 *  Caller is trying to read past the end of the partition
		 *  (or disk, in the case of floppies).  Adjust the byte count
		 *  so that we only read up to the EOF mark.
		 */

		if ((bc = (int)(ihp->dev.disk.siz - off)) < 0) {
			/*
			 *  If adjusted length is negative, it means that the
			 *  caller has specified a bogus starting offset.
			 *  Deliver an error return!
			 */
			if (!SilentDiskFailures) {
				printf("%s: bogus sector number\n",
				    ihp->pathnm);
			}
			return (-1);
		}

		bc *= DEV_BSIZE;	/* Convert sectors to bytes */
	}

	while (bc > 0) {

		/*
		 *  Main data transfer loop.  Copy data from the track cache
		 *  until we fall off the end or reach the caller's byte count.
		 *  Each time we empty the cache, re-load it from disk.
		 */
		if (get_cache_buffer(ihp, off) == 0) {
			/*
			 *  If requested sector is not already in the cache,
			 *  read it in now.  We try to read an entire csize,
			 *  but if we're near the end of the partition/device,
			 *  we'll shorten this up a bit.  The fact that we've
			 *  already adjusted the "bc" count prevents us
			 *  from reading the unused portion of the cache when
			 *  this happens.
			 */
			daddr_t dad;

			/*
			 * Calculate sector address of start of aligned
			 * cache-size unit.  Size cache size is a multiple
			 * of the block size, it must also be on a block
			 * boundary within the slice.  Slices must start
			 * on block boundaries to make this work!
			 */
			dad = (daddr_t)(off / csize_sectors) * csize_sectors;

			/*
			 * Calculate the number of sectors to read by
			 * starting with the remaining sectors in the
			 * slice then reducing it if the result is more
			 * than a cacheful.
			 *
			 * Note that this algorithm assumes that slices
			 * end on block boundaries.  If that is not true,
			 * the number of sectors passed to bd_readalts
			 * when near the end of the slice will not be a
			 * multiple of the number of sectors in a block.
			 */
			j = ihp->dev.disk.siz - dad;
			if (j > csize_sectors)
				j = csize_sectors;

			if ((j = bd_readalts(ihp, ihp->dev.disk.par + dad,
					j)) != -1) {
				/*
				 *  The "bd_readalts" routine calls
				 *  "read_blocks" (see below) after first
				 *  remapping any bad sectors in the request.
				 *  If we get an I/O error, it will return the
				 *  address of the failing sector so we can
				 *  print an error message and bail out with a
				 *  short read.
				 */
				if (!SilentDiskFailures) {
					printf("%s: disk read error, ",
					    ihp->pathnm);
					printf("sector %d\n", j);
				}
				return (-1);
			}

			/*
			 * Mark the cache full from block dad.
			 * dad is in sectors from start of slice.
			 */
			set_cache_buffer((unsigned)dad);
		}

		/* set j to amount in cache from requested block to end */
		j = csize_sectors - (off - cache_info.active_sector);

		/*
		 * k = amount to copy to buf, either entire req or this
		 * cache's worth
		 */

		k = MIN(j * DEV_BSIZE, bc);

		(void) memcpy(buf, cache_info.active_buffer +
		    ((off - cache_info.active_sector) * DEV_BSIZE), k);

		off += j;
		buf += k;
		bc -= k;
	}

	return (len - bc);
}

/*
 * []------------------------------------------------------------[]
 *  | write_disk - This routine is called infrequently enough	|
 *  | that I'm taking the approach of simplistic code which	|
 *  | equals smaller code. This program is currently growing	|
 *  | without bounds and it must stop before we run out of	|
 *  | memory.							|
 *  |								|
 *  | The boot code does not support writing to UFS filesystems	|
 *  | so this routine does not need to go through the alternate |
 *  | sector handling code.					|
 *  |								|
 *  | Note: 'len' is in bytes and 'off' is in 512-byte sectors.	|
 * []------------------------------------------------------------[]
 */
int
write_disk(struct ihandle *ihp, caddr_t buf, u_int len, u_int off)
{
	int	cyl,
		sec,
		head,
		retry = RD_RETRY;
	u_int	bc = len;
	paddr_t	lowmem;
	dev_pkt_t *pktp = NULL;
	dev_pkt_t pkt;
	unsigned char allow_lba = 1;
	unsigned char device = ihp->unit;
	unsigned int bps = ihp->dev.disk.bps;
	unsigned int wlen;

	/*
	 * need to use low mem buffer because we don't know where buf
	 * is located.  Use the shared cache buffer because we will
	 * need to invalidate it anyway since we could be writing
	 * the same blocks.  Using the shared cache also eliminates
	 * the danger of running out of realmode memory while trying
	 * to write a file.
	 */
	invalidate_cache(ihp);
	lowmem = (paddr_t)cache_info.active_buffer;
	if ((ulong)&pkt < TOP_RMMEM)
		pktp = &pkt;

	len /= DEV_BSIZE;
	while (len) {

		wlen = 1;

		if (allow_lba && ihp->dev.disk.lbamode) {

			/*
			 * We are going to write one block at a time.
			 * If only part of the block is part of the
			 * write request we must read it into the buffer
			 * first.
			 */
			int block_factor = ihp->dev.disk.bps / DEV_BSIZE;
			int block = off / block_factor;
			int boff = off % block_factor;

			if (pktp == 0) {
				pktp = (dev_pkt_t *)rm_malloc(sizeof (*pktp),
				    sizeof (long), 0);
				if (!pktp)
					prom_panic("write_disk: cannot "
						"allocate pkt mem\n");
			}

			if (boff != 0 || len < block_factor) {
				ic.intval = DEVT_DSK;
				ic.ax = INT13_EXTREAD << 8;
				ic.dx = device;
				pktp->size = sizeof (*pktp);
				pktp->nblks = 1;
				pktp->bufp = mk_farp(lowmem);
				pktp->lba_lo = block;
				pktp->lba_hi = 0;
				pktp->bigbufp_lo = pktp->bigbufp_hi = 0;
				ic.ds = segpart((unsigned long)pktp);
				ic.si = offpart((unsigned long)pktp);
				if (doint()) {
					if ((ic.ax & 0xFF00) == 1)
						goto try_write_conversion;
					if (!SilentDiskFailures && DiskDebug) {
						printf("(LBA write "
						    "error: ax %x, mem 0x%x)",
						    ic.ax, lowmem);
					}
					goto write_fail;
				}
			}

			wlen = block_factor - boff;
			if (wlen < len)
				wlen = len;
			bcopy(buf, (caddr_t)lowmem + boff * DEV_BSIZE,
			    wlen * DEV_BSIZE);

			ic.intval = DEVT_DSK;
			ic.ax = INT13_EXTWRITE << 8;
			ic.dx = device;
			pktp->size = sizeof (*pktp);
			pktp->nblks = 1;
			pktp->bufp = mk_farp(lowmem);
			pktp->lba_lo = off;
			pktp->lba_hi = 0;
			pktp->bigbufp_lo = pktp->bigbufp_hi = 0;
			ic.ds = segpart((unsigned long)pktp);
			ic.si = offpart((unsigned long)pktp);
		} else {

			bcopy(buf, (caddr_t)lowmem, bps);

			cyl = off / ihp->dev.disk.spc;
			sec = off % ihp->dev.disk.spc;
			head = sec / ihp->dev.disk.spt;
			sec -= head * ihp->dev.disk.spt - 1;

			/* Write one sector at a time */
			ic.intval = DEVT_DSK;
			ic.ax = (INT13_WRITE << 8) | wlen;
			ic.dx = (head << 8) + device;
			ic.es = segpart(lowmem); ic.bx = offpart(lowmem);
			ic.cx = ((cyl & 0xFF) << 8) | ((cyl >> 2) & 0xC0) |
			    (sec & 0x3f);
		}

		if (doint()) {
			if (allow_lba && (ic.ax & 0xFF00) == 1 &&
			    ihp->dev.disk.lbamode) {
try_write_conversion:
				if (io_conv(ihp, &device)) {
					/*
					 * Sizes are already in sectors so
					 * unlike in read_blocks there is
					 * no size conversion here.
					 */
					allow_lba = 0;
					bps = DEV_BSIZE;
					continue;
				}
			}
			if (!SilentDiskFailures && DiskDebug) {
				printf("(Write error: ax %x, mem 0x%x)",
				    ic.ax, lowmem);
			}
			if (!retry--) {
write_fail:
				if (pktp && pktp != &pkt)
					rm_free((caddr_t)pktp, sizeof (*pktp));
				return (-1);
			}
		} else {
			len -= wlen;
			buf += wlen * DEV_BSIZE;
			off += wlen;
		}
	}

	if (pktp && pktp != &pkt)
		rm_free((caddr_t)pktp, sizeof (*pktp));

	invalidate_cache(ihp);

	return (bc);
}

/*
 * io_conv() is called to determine whether a failed LBA call
 * can be converted into a regular call via a realmode driver loaded
 * since the device was opened.
 */
static unsigned int
io_conv(struct ihandle *ihp, unsigned char *device)
{
	unsigned int x;
	struct bdev_info *bi;

	ic.intval = DEVT_DSK;
	ic.ax = INT13_BEFIDENT << 8;
	ic.dx = ihp->unit;
	if (doint() || ic.dx != 0xBEF1) {
		return (0);	/* Device not run by a realmode driver */
	}

	bi = (struct bdev_info *)mk_ea(ic.es, ic.bx);
	if (bi->bios_dev == ihp->unit || bi->bdev != ihp->unit) {
		return (0);	/* Device is not a special BIOS device */
	}

	/*
	 * BIOS device ihp->unit has been taken over by a realmode
	 * driver for device bi->bios_dev.  Use the normal device
	 * code and geometry because the BIOS device geometry might
	 * be unreliable.
	 */
	ic.intval = DEVT_DSK;	/* Get the disk geometry */
	ic.ax = INT13_PARMS << 8;
	ic.bx = ic.cx = 0;
	ic.dx = bi->bios_dev;

	if (doint()) {
		return (0);	/* Device not run by a realmode driver */
	}

	x = ic.cx & 0x3F;
	ihp->dev.disk.spt = (u_short)(x ? x : 32);
	ihp->dev.disk.spc = (u_short) (ihp->dev.disk.spt *
		(((ic.dx >> 8) & 0xFF) + 1));
	ihp->dev.disk.cyl = ((ic.cx >> 8) & 0xFF) +
		((ic.cx << 2) & 0x300) + 1;
	*device = bi->bios_dev;
	return (ihp->dev.disk.bps / DEV_BSIZE);
}

int
read_blocks(struct ihandle *ihp, daddr_t off, int cnt)
{
	/*
	 * Read disk blocks:
	 *
	 * This routine is called from bd_readalts as part of filling the
	 * disk cache for read_disk.  It is also used by other routines
	 * in ix_alts.c while initializing the disk.
	 *
	 * Read "cnt" blocks into the track cache, starting at block "off"
	 * relative to the start of the medium.
	 *
	 * Return -1 for success, or the failing block number if there's an
	 * unrecoverable read error.
	 *
	 * Allocate an LBA packet the first time we are called and keep it
	 * permanently.  This strategy ensures that the packet is available
	 * when needed, even if the first LBA use is after selecting a boot
	 * device.
	 */
	paddr_t buf = (paddr_t)cache_info.active_buffer;
	int retry = RD_RETRY;
	int x, n = cnt;
	static dev_pkt_t *pktp;
	unsigned char allow_lba = 1;
	unsigned char device = ihp->unit;
	unsigned int bps = ihp->dev.disk.bps;

	if (pktp == NULL) {
		pktp = (dev_pkt_t *)rm_malloc(sizeof (*pktp), sizeof (long), 0);
		if (pktp == NULL)
			prom_panic("read_blocks: cannot allocate LBA pkt "
			    "memory\n");
	}

	/*
	 * Mark the cache empty because all I/O is via the track cache
	 * even when the caller does not intend to fill the cache.
	 */
	set_cache_buffer(CACHE_EMPTY);

	while (n > 0) {
		/*
		 *  Main read loop.
		 *
		 *  Normally, we will only need a single iteration of this loop.
		 *  Multiple iterations occur when (a) there's an I/O error or
		 *  (b) the track cache straddles a 64KB boundary.
		 */

		if (IS_FLOPPY(device)) {
			/*
			 *  Floppy disks are unable to read a full track at a
			 *  time unless the request is aligned on a track
			 *  boundary.  If this isn't the case, truncate this
			 *  request to read up to the next track boundary and
			 *  read the rest on the next iteration of the loop.
			 */
			int q = ihp->dev.disk.spt - (off % ihp->dev.disk.spt);
			if (n > q) n = q;
		}


		if (allow_lba && ihp->dev.disk.lbamode) {
			(void) memset((char *)pktp, 0, sizeof (*pktp));
			ic.intval = DEVT_DSK;
			ic.ax = INT13_EXTREAD << 8;
			ic.dx = ihp->unit;
			pktp->size = sizeof (*pktp);
			pktp->nblks = (unsigned char)(n > 127 ? 127 : n);
			pktp->bufp = mk_farp(buf);
			pktp->lba_lo = off;
			pktp->lba_hi = 0;
			pktp->bigbufp_lo = pktp->bigbufp_hi = 0;
			ic.ds = segpart((unsigned long)pktp);
			ic.si = offpart((unsigned long)pktp);
		} else {

			int cyl = off / ihp->dev.disk.spc;
			int sec = off - (cyl * ihp->dev.disk.spc);
			int hed = sec / ihp->dev.disk.spt;

			sec -= ((hed * ihp->dev.disk.spt) - 1);

			if (cyl > 1023) {
				printf("read_blocks: can't read block"
				    " 0x%x, (cyl %d > 1023 and no LBA)\n",
				    off, cyl);
				return (off);
			}
			ic.intval = DEVT_DSK;
			ic.ax = (INT13_READ << 8) | n;
			ic.dx = (hed << 8) + device;
			ic.es = segpart(buf); ic.bx = offpart(buf);
			ic.cx = ((cyl & 0xFF) << 8) | ((cyl >> 2) & 0xC0) |
			    (sec & 0x3f);
		}

		if (doint() && (((x = ((ic.ax >> 8) & 0xFF)) != ECC_COR_ERR))) {
			if (allow_lba && x == 1 && ihp->dev.disk.lbamode) {
				/*
				 * An LBA device reported that LBA read is an
				 * invalid function.  That is probably because
				 * bootconf loaded a realmode driver that
				 * took over the device.  Test for a driver
				 * and convert the read if possible.
				 *
				 * Note that conversion lasts only for the
				 * active read, because the driver could be
				 * unloaded again in future.
				 *
				 * io_conv() attempts to set up to use
				 * the equivalent realmode driver device
				 * code for the BIOS device.  It returns
				 * 0 for failure, otherwise the number of
				 * sectors in an LBA block.
				 */
				unsigned int m;

				if ((m = io_conv(ihp, &device)) != 0) {
					allow_lba = 0;
					bps = DEV_BSIZE;
					if (m != 1) {
						off *= m;
						n *= m;
					}
					continue;
				}
			}
			/*
			 *  I/O error of some sort.  We're prepared to
			 *  retry these up to "FD_RETRY" times ...
			 */
			if (!retry--) {
				/*
				 *  ... after which we deliver the failing
				 *  sector number so that caller can include it
				 *  in an error message.
				 */
				return (off);
			} else if (IS_FLOPPY(ihp->unit)) {
				/*
				 *  If this is a floppy disk, retry the read
				 *  for a single sector.  This sometimes gives
				 *  better results than trying to re-read the
				 *  entire track (especially if there are two
				 *  or more bad sectors on the track).
				 */
				if (x & (I13_SEK_ERR+I13_TMO_ERR)) {
					/*
					 *  Our problem may be that the drive
					 *  hasn't come up to speed yet.  Issue
					 *  a "reset drive" command and see if
					 *  that helps any.
					 */
					(void) reset_disk(ihp);
				}

				if (n > 1) {
					/*
					 *  Our problem may be that we're
					 *  trying to read too many sectors
					 *  (the bios may not support full
					 *  track reads from floppy disk).
					 *  Let's cut the read size in half
					 *  and try again!
					 */
					n >>= 1;
					retry += 1;
				}
			}

		} else {
			/*
			 *  Read complete.  Update pointers & counters JIC we
			 *  take another trip thru the read loop.
			 */
			buf += (n * bps);
			retry = RD_RETRY;
			off += n;
			cnt -= n;
			n = cnt;
		}
	}

	return (-1);	/* NOTE: This is a successful return! */
}

int
is_floppy(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is a floppy disk.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    IS_FLOPPY(ihp->unit));
}

int
is_floppy0(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is floppy drive 0.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->unit == 0));
}

int
is_floppy1(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is floppy drive 1.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->unit == 1));
}

int
floppy_status_changed(int fd)
{
	/*
	 *  If the floppy has a 'change line' check to see if the
	 *  line's status has changed.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];
	int rv = 0;

	if ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->dev.disk.flop_change_line == 1)) {
		ic.intval = DEVT_DSK;
		ic.ax = INT13_CHGLINE << 8;
		ic.dx = ihp->unit;

		if (doint() && ((ic.ax & 0xff00) == 0x600)) {
			rv = 1;
		}
	}
	return (rv);
}
