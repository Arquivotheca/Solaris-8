/*
 * Copyright (c) 1996, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mkfs.c 1.15     99/12/04 SMI"

#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <locale.h>
#include <sys/fdio.h>
#include <sys/dktp/fdisk.h>
#include <sys/dkio.h>
#include <sys/sysmacros.h>
#include "mkfs_pcfs.h"
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_label.h>
#include <macros.h>

/*
 *	mkfs (for pcfs)
 *
 *	Install a boot block, FAT, and (if desired) the first resident
 *	of the new fs.
 *
 *	XXX -- floppy opens need O_NDELAY?
 */
#define	DEFAULT_LABEL "NONAME"

static char	*BootBlkFn = NULL;
static char	*DiskName = NULL;
static char	*FirstFn = NULL;
static char	*Label = NULL;
static char	Firstfileattr = 0x20;
static int	Outputtofile = 0;
static int	SunBPBfields = 0;
static int	GetFsParams = 0;
static int	Fatentsize = 0;
static int	Imagesize = 3;
static int	Notreally = 0;
static int	Verbose = 0;

static int	GetSize = 1;	/* Unless we're given as arg, must look it up */
static ulong_t	TotSize;	/* Total size of FS in # of sectors */
static int	GetSPC = 1;	/* Unless we're given as arg, must calculate */
static ulong_t	SecPerClust;	/* # of sectors per cluster */
static int	GetOffset = 1;	/* Unless we're given as arg, must look it up */
static ulong_t	RelOffset;	/* Relative start sector (hidden sectors) */
static int	GetSPT = 1;	/* Unless we're given as arg, must look it up */
static ushort_t	SecPerTrk;	/* # of sectors per track */
static int	GetTPC = 1;	/* Unless we're given as arg, must look it up */
static ushort_t	TrkPerCyl;	/* # of tracks per cylinder */
static int	GetResrvd = 1;	/* Unless we're given as arg, must calculate */
static ushort_t	Resrvd;		/* Number of reserved sectors */
static int	GetBPF = 1;	/* Unless we're given as arg, must calculate */
static int	BitsPerFAT;	/* Total size of FS in # of sectors */

static ulong_t	TotalClusters;	/* Computed total number of clusters */

/*
 * Unless we are told otherwise, we should use fdisk table for non-diskettes.
 */
static int	DontUseFdisk = 0;

/*
 * Function prototypes
 */
#ifndef i386
static void swap_pack_grabsebpb(struct _sun_bpb_extensions *sbpb,
	struct _boot_sector *bsp);
static void swap_pack_sebpbcpy(struct _boot_sector *bsp,
	struct _sun_bpb_extensions *sbpb);
static void swap_pack_grabbpb(struct _bios_param_blk *wbpb,
	struct _boot_sector *bsp);
static void swap_pack_bpbcpy(struct _boot_sector *bsp,
	struct _bios_param_blk *wbpb);
#endif

static uchar_t *build_rootdir(struct _bios_param_blk *wbpb, char *ffn, int fffd,
	ulong_t ffsize, pc_cluster16_t ffstart, ulong_t *rdirsize);
static uchar_t *build_fat(struct _bios_param_blk *wbpb, ulong_t bootblksize,
	ulong_t *fatsize, char *ffn, int *fffd, ulong_t *ffsize,
	pc_cluster16_t *ffstartclust);
static off64_t fill_bpb_sizes(struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb, struct ipart part[], int partno);

static char *stat_actual_disk(char *diskname, struct stat *info, char **suffix);

static void compare_existing_with_computed(int fd, char *suffix,
	struct _bios_param_blk *wbpb, struct _sun_bpb_extensions *sbpb,
	int *prtsize, int *prtspc, int *prtbpf, int *prtnsect,
	int *prtntrk, int *prtfdisk, int *prthidden, int *prtrsrvd,
	int *dashos);
static void print_reproducing_command(int fd, char *actualdisk, char *suffix,
	struct _bios_param_blk *wbpb, struct _sun_bpb_extensions *sbpb);
static void compute_file_area_size(struct _bios_param_blk *wbpb);
static void sanity_check_options(int argc, int optind);
static void compute_cluster_size(struct _bios_param_blk *wbpb);
static void find_fixed_details(int fd, struct _bios_param_blk *wbpb);
static void dirent_fname_fill(struct pcdir *dep, char *fn);
static void floppy_bpb_fillin(struct _bios_param_blk *wbpb,
	int diam, int hds, int spt);
static void read_existing_bpb(int fd, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static void warn_funky_fatsize(void);
static void warn_funky_floppy(void);
static void dirent_time_fill(struct pcdir *dep);
static void parse_suboptions(char *optsstr);
static void set_fat_string(struct _bios_param_blk *wbpb, int fatsize);
static void partn_lecture(char *dn);
static void lookup_floppy(struct fd_char *fdchar, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static void label_volume(char *lbl, struct _bios_param_blk *wbpb);
static void mark_cluster(uchar_t *fatp, pc_cluster16_t clustnum,
	ushort_t value);
static void missing_arg(char *option);
static void dashm_bail(int fd);
static void write_rest(struct _bios_param_blk *wbpb, char *efn,
	int dfd, int sfd, int remaining);
static void write_fat(int fd, char *fn, char *lbl, char *ffn,
	struct _bios_param_blk *wbpb, struct _sun_bpb_extensions *sbpb);
static void bad_arg(char *option);
static void usage(void);

static int prepare_image_file(char *fn, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static int verify_bootblkfile(char *fn, boot_sector_t *bs,
	ulong_t *blkfilesize);
static int open_and_examine(char *dn, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static int verify_firstfile(char *fn, ulong_t *filesize);
static int powerofx_le_y(int x, int y, int value);
static int open_and_seek(char *dn, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static int warn_mismatch(char *desc, char *src, int expect, int assigned);
static int copy_bootblk(char *fn, boot_sector_t *bootsect,
	ulong_t *bootblksize);
static int parse_drvnum(char *pn);
static int seek_nofdisk(int fd, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static int ask_nicely(char *special);
static int seek_partn(int fd, char *pn, struct _bios_param_blk *wbpb,
	struct _sun_bpb_extensions *sbpb);
static int yes(void);

/*
 *  usage
 *
 *	Display usage message and exit.
 */
static
void
usage(void)
{
	(void) fprintf(stderr,
	    gettext("pcfs usage: mkfs [-F FSType] [-V] [-m] "
		"[-o specific_options] special\n"));

	(void) fprintf(stderr,
	    gettext(" -V: print this command line and return\n"
		" -m: dump command line used to create a FAT on this media\n"
		"\t(other options are ignored if this option is chosen).\n"
		" -o: pcfs_specific_options:\n"
		"\t'pcfs_specific_options' is a comma separated list\n"
		"\tincluding one or more of the following options:\n"
		"\t    N,v,r,h,s,b=label,B=filename,i=filename,\n"
		"\t    spc=n,fat=n,nsect=n,ntrack=n,nofdisk,size=n,\n"
		"\t    reserve=n,hidden=n\n\n"));

	(void) fprintf(stderr,
	    gettext("'Special' should specify a raw diskette "
			"or raw fixed disk device.  \"Fixed\"\n"
			"disks (which include high-capacity removable "
			"media such as Zip disks)\n"
			"may be further qualified with a logical "
			"drive specifier.\n"
			"Examples are: /dev/rdiskette and "
			"/dev/rdsk/c0t0d0p0:c\n"));
	exit(1);
}

static
int
yes(void)
{
	int i, b;

	i = b = getchar();
	while (b != '\n' && b != '\0' && b != EOF)
		b = getchar();
	return (i == 'y');
}

static
int
powerofx_le_y(int x, int y, int value)
{
	int ispower = 0;
	int pow = 1;

	do {
		if (pow == value) {
			ispower = 1;
			break;
		}
		pow *= x;
	} while (pow <= y);

	return (ispower);
}

static
int
ask_nicely(char *special)
{
	/*
	 * 4228473 - No way to non-interactively make a pcfs filesystem
	 *
	 *	If we don't have an input TTY, or we aren't really doing
	 *	anything, then don't ask questions.  Assume a yes answer
	 *	to any questions we would ask.
	 */
	if (Notreally || !isatty(fileno(stdin)))
		return (1);

	(void) printf(
	    gettext("Construct a new FAT file system on %s: (y/n)? "), special);
	(void) fflush(stdout);
	return (yes());
}

/*
 *  parse_drvnum
 *	Convert a partition name into a drive number.
 */
static
int
parse_drvnum(char *pn)
{
	int drvnum;

	/*
	 * Determine logical drive to seek after.
	 */
	if (strlen(pn) == 1 && *pn >= 'c' && *pn <= 'z') {
		drvnum = *pn - 'c' + 1;
	} else if (*pn >= '0' && *pn <= '9') {
		char *d;
		int v, m, c;

		v = 0;
		d = pn;
		while (*d && *d >= '0' && *d <= '9') {
			c = strlen(d);
			m = 1;
			while (--c)
				m *= 10;
			v += m * (*d - '0');
			d++;
		}

		if (*d || v > 24) {
			(void) fprintf(stderr,
			    gettext("%s: bogus logical drive specification.\n"),
			    pn);
			return (-1);
		}
		drvnum = v;
	} else if (strcmp(pn, "boot") == 0) {
		drvnum = 99;
	} else {
		(void) fprintf(stderr,
		    gettext("%s: bogus logical drive specification.\n"), pn);
		return (-1);
	}

	return (drvnum);
}

static
int
warn_mismatch(char *desc, char *src, int expect, int assigned)
{
	if (expect == assigned)
		return (assigned);

	/*
	 * 4228473 - No way to non-interactively make a pcfs filesystem
	 *
	 *	If we don't have an input TTY, or we aren't really doing
	 *	anything, then don't ask questions.  Assume a yes answer
	 *	to any questions we would ask.
	 */
	if (Notreally || !isatty(fileno(stdin))) {
		(void) printf(gettext("WARNING: User supplied %s is %d,"
			"\nbut value obtained from the %s is %d.\n"
			"Using user supplied value.\n"),
			desc, assigned, src, expect);
		return (assigned);
	}

	(void) printf(gettext("User supplied %s is %d."
		"\nThe value obtained from the %s is %d.\n"),
		desc, assigned, src, expect);

	(void) printf(
	    gettext("Continue with value given on command line (y/n)? "));
	(void) fflush(stdout);
	if (yes())
		return (assigned);
	else
		exit(2);
	/*NOTREACHED*/
}

static
off64_t
fill_bpb_sizes(struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb,
    struct ipart part[], int partno)
{
	off64_t usesect;
	ulong_t usesize;

	if (GetFsParams || GetSize) {
		usesize = ltohi(part[partno].numsect);
		if (Verbose) {
		    (void) printf(
			gettext("Partition size (from FDISK table) "
			    "= %d sectors.\n"), usesize);
		}
	} else {
		usesize = warn_mismatch(
		    gettext("length of partition (in sectors)"),
		    gettext("FDISK table"),
		    ltohi(part[partno].numsect), TotSize);
	}

	if (GetFsParams) {
		TotSize = usesize;
	} else {
		if (usesize > 0xffff)
			wbpb->bpb.sectors_in_volume = 0;
		else
			wbpb->bpb.sectors_in_volume = usesize;
		wbpb->bpb.sectors_in_logical_volume = usesize;
	}

	if (GetFsParams || GetOffset) {
		usesect = ltohi(part[partno].relsect);
	} else {
		usesect = warn_mismatch(
		    gettext("start sector of partition "
			"relative to beginning of disk"),
		    gettext("FDISK table"),
		    ltohi(part[partno].relsect), RelOffset);
	}
	wbpb->bpb.hidden_sectors = usesect;

	if (GetFsParams) {
		RelOffset = usesect;
	} else {
		sbpb->bs_offset_high = usesect >> 16;
		sbpb->bs_offset_low = usesect & 0xFFFF;
	}

	return (usesect * BPSEC);
}

/*
 *  seek_partn
 *
 *	Seek to the beginning of the partition where we need to install
 *	the new FAT.  Zero return for any error, but print error
 *	messages here.
 */
static
int
seek_partn(int fd, char *pn, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	struct ipart part[FD_NUMPART];
	struct mboot mb;
	ulong_t drvbasesec;
	off64_t bootsectseek;
	int drvnum;
	int rval = 0;
	int i;

	if ((drvnum = parse_drvnum(pn)) < 0)
		return (rval);

	if (read(fd, &mb, sizeof (mb)) != sizeof (mb)) {
		(void) fprintf(stderr,
		    gettext("Couldn't read a Master Boot Record?!\n"));
		return (rval);
	}

	if (ltohs(mb.signature) != BOOTSECSIG) {
		(void) fprintf(stderr,
		    gettext("Bad Sig on master boot record!\n"));
		return (rval);
	}

	/*
	 * Copy partition table into memory
	 */
	(void) memcpy(part, mb.parts, sizeof (part));

	/*
	 *  Wade through the fdisk table(s) looking for the specified
	 *  logical drive.
	 *
	 *  An fdisk table has up to four entries. Each entry describes
	 *  an actual file system, or points to another fdisk table.
	 *  The systid field describes if the entry is a file system or
	 *  a pointer to another fdisk table.  EXTDOS or FDISK_EXTLBA systids
	 *  indicate pointers to nested fdisk tables.
	 */
	if (drvnum == 99) {
		for (i = 0; i < FD_NUMPART; i++) {
			if (part[i].systid == SUNIXOSBOOT)
				break;
		}
		if (i == FD_NUMPART) {
			(void) fprintf(stderr,
			    gettext("No boot partition found on drive!\n"));
			return (rval);
		} else {
			bootsectseek = fill_bpb_sizes(wbpb, sbpb, part, i);
		}
	} else if (drvnum == 1) {
		for (i = 0; i < FD_NUMPART; i++) {
			if (part[i].systid == DOSOS12 ||
			    part[i].systid == DOSOS16 ||
			    part[i].systid == FDISK_WINDOWS ||
			    part[i].systid == FDISK_EXT_WIN ||
			    part[i].systid == FDISK_FAT95 ||
			    part[i].systid == DOSHUGE)
				break;
		}
		if (i == FD_NUMPART) {
			(void) fprintf(stderr,
			    gettext("No primary dos partition found "
				"on drive!\n"));
			return (rval);
		} else {
			bootsectseek = fill_bpb_sizes(wbpb, sbpb, part, i);
		}
	} else {
		struct mboot extmboot;
		uchar_t xsysid;
		off64_t nextseek, partbias, xnumsect;

		for (i = 0; i < FD_NUMPART; i++) {
			if (part[i].systid == EXTDOS ||
			    part[i].systid == FDISK_EXTLBA)
				break;
		}
		if (i == FD_NUMPART) {
			(void) fprintf(stderr,
			    gettext("No extended dos partition found "
				"on drive!\n"));
			return (rval);
		}

		partbias = nextseek = ltohi(part[i].relsect);
		xsysid = part[i].systid;
		xnumsect = ltohi(part[i].numsect);
		while (--drvnum &&
			(xsysid == EXTDOS || xsysid == FDISK_EXTLBA)) {
			if ((lseek64(fd, nextseek * BPSEC, SEEK_SET) < 0) ||
			    read(fd, &extmboot, sizeof (extmboot)) !=
			    sizeof (mb)) {
				perror(gettext("Could not read extended "
					"partition record?"));
				return (rval);
			}
			(void) memcpy(part, extmboot.parts, sizeof (part));
			xsysid = part[1].systid;
			drvbasesec = nextseek;
			nextseek = ltohi(part[1].relsect) + partbias;
		}
		bootsectseek = ltohi(part[0].relsect) + drvbasesec;
		if (drvnum) {
			(void) fprintf(stderr,
			    gettext("No such logical drive!\n"));
			return (rval);
		} else if (xnumsect < (bootsectseek - partbias)) {
			(void) fprintf(stderr,
			    gettext("Bogus extended partition info!\n"));
			return (rval);
		} else {
			(void) fill_bpb_sizes(wbpb, sbpb, part, 0);
			bootsectseek *= BPSEC;
		}
	}

	if (Verbose)
		(void) printf(gettext("Requested partition's offset: "
			"Sector %x.\n"), bootsectseek/BPSEC);

	if (bootsectseek == 0) {
		(void) fprintf(stderr,
		    gettext("Bogus FDISK entry? A file system starting\n"
			    "at sector 0 would collide with the"
			    " FDISK table!\n"));
		return (rval);
	}

	if (lseek64(fd, bootsectseek, SEEK_SET) < 0) {
		(void) fprintf(stderr, gettext("Partition %s: "), pn);
		perror("");
		return (rval);
	}

	return (++rval);
}

/*
 *  seek_nofdisk
 *
 *	User is asking us to trust them that they know best.
 *	We basically won't do much seeking here, the only seeking we'll do
 *	is if the 'hidden' parameter was given.
 */
static
int
seek_nofdisk(int fd, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	off64_t bootsectseek;
	int rval = 0;

	if (TotSize > 0xffff)
		wbpb->bpb.sectors_in_volume = 0;
	else
		wbpb->bpb.sectors_in_volume = (short)TotSize;
	wbpb->bpb.sectors_in_logical_volume = TotSize;

	bootsectseek = RelOffset * BPSEC;
	sbpb->bs_offset_high = RelOffset >> 16;
	sbpb->bs_offset_low = RelOffset & 0xFFFF;

	if (Verbose)
		(void) printf(gettext("Requested offset: Sector %x.\n"),
		    bootsectseek/BPSEC);

	if (lseek64(fd, bootsectseek, SEEK_SET) < 0) {
		(void) fprintf(stderr,
		    gettext("User specified start sector %d"), RelOffset);
		perror("");
		return (rval);
	}

	return (++rval);
}

/*
 * set_fat_string
 *
 *	Fill in the type string of the FAT
 */
static
void
set_fat_string(struct _bios_param_blk *wbpb, int fatsize)
{
	if (fatsize == 12) {
		(void) strncpy((char *)wbpb->ebpb.type, FAT12_TYPE_STRING,
			strlen(FAT12_TYPE_STRING));
	} else {
		(void) strncpy((char *)wbpb->ebpb.type, FAT16_TYPE_STRING,
			strlen(FAT16_TYPE_STRING));
	}
}

/*
 *  prepare_image_file
 *
 *	Open the file that will hold the image (as opposed to the image
 *	being written to the boot sector of an actual disk).
 */
static
int
prepare_image_file(char *fn, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	int fd;
	char zerobyte = '\0';

	if ((fd = open(fn, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
		perror(fn);
		exit(2);
	}

	if (Imagesize == 5) {
		/* Disk image of a 1.2M floppy */
		wbpb->bpb.sectors_in_volume = 2 * 80 * 15;
		wbpb->bpb.sectors_in_logical_volume = 2 * 80 * 15;
		wbpb->bpb.sectors_per_track = 15;
		wbpb->bpb.heads = 2;
		wbpb->bpb.media = 0xF9;
		wbpb->bpb.num_root_entries = 224;
		wbpb->bpb.sectors_per_cluster = 1;
		wbpb->bpb.sectors_per_fat = 7;
	} else {
		/* Disk image of a 1.44M floppy */
		wbpb->bpb.sectors_in_volume = 2 * 80 * 18;
		wbpb->bpb.sectors_in_logical_volume = 2 * 80 * 18;
		wbpb->bpb.sectors_per_track = 18;
		wbpb->bpb.heads = 2;
		wbpb->bpb.media = 0xF0;
		wbpb->bpb.num_root_entries = 224;
		wbpb->bpb.sectors_per_cluster = 1;
		wbpb->bpb.sectors_per_fat = 9;
	}

	/*
	 * Make a holey file, with length the exact
	 * size of the floppy image.
	 */
	if (lseek(fd, (wbpb->bpb.sectors_in_volume * BPSEC)-1, SEEK_SET) < 0) {
		(void) close(fd);
		perror(fn);
		exit(2);
	}

	if (write(fd, &zerobyte, 1) != 1) {
		(void) close(fd);
		perror(fn);
		exit(2);
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		(void) close(fd);
		perror(fn);
		exit(2);
	}

	Fatentsize = 12;  /* Size of fat entry in bits */
	set_fat_string(wbpb, Fatentsize);

	wbpb->ebpb.phys_drive_num = 0;

	sbpb->bs_offset_high = 0;
	sbpb->bs_offset_low = 0;

	return (fd);
}

/*
 *  partn_lecture
 *
 *	Give a brief sermon on dev_name user should pass to
 *	the program from the command line.
 *
 */
static
void
partn_lecture(char *dn)
{
	(void) fprintf(stderr,
		gettext("\nDevice %s was assumed to be a diskette.\n"
		    "A diskette specific operation failed on this device.\n"
		    "If the device is a hard disk, provide the name of "
		    "the full physical disk,\n"
		    "and qualify that name with a logical drive specifier.\n\n"
		    "Hint: the device is usually something similar to\n\n"
		    "/dev/rdsk/c0d0p0 or /dev/rdsk/c0t0d0p0 (x86)\n"
		    "/dev/rdsk/c0t5d0s2 (sparc)\n\n"
		    "The drive specifier is appended to the device name."
		    " For example:\n\n"
		    "/dev/rdsk/c0t5d0s2:c or /dev/rdsk/c0d0p0:boot\n\n"), dn);
}

static
void
warn_funky_floppy(void)
{
	(void) fprintf(stderr,
	    gettext("Use the 'nofdisk' option to create file systems\n"
		    "on non-standard floppies.\n\n"));
	exit(4);
}

static
void
warn_funky_fatsize(void)
{
	(void) fprintf(stderr,
	    gettext("Non-standard FAT size requested for floppy.\n"
		    "The 'nofdisk' option must be used to\n"
		    "override the 12 bit floppy default.\n\n"));
	exit(4);
}

static
void
floppy_bpb_fillin(struct _bios_param_blk *wbpb, int diam, int hds, int spt)
{
	switch (diam) {
	case 3:
		switch (hds) {
		case 2:
			switch (spt) {
			case 9:
				wbpb->bpb.media = 0xF9;
				wbpb->bpb.num_root_entries = 112;
				wbpb->bpb.sectors_per_cluster = 2;
				wbpb->bpb.sectors_per_fat = 3;
				break;
			case 18:
				wbpb->bpb.media = 0xF0;
				wbpb->bpb.num_root_entries = 224;
				wbpb->bpb.sectors_per_cluster = 1;
				wbpb->bpb.sectors_per_fat = 9;
				break;
			case 36:
				wbpb->bpb.media = 0xF0;
				wbpb->bpb.num_root_entries = 240;
				wbpb->bpb.sectors_per_cluster = 2;
				wbpb->bpb.sectors_per_fat = 9;
				break;
			default:
				(void) fprintf(stderr,
				    gettext("Unknown diskette parameters!  "
					"3.5'' diskette with %d heads "
					"and %d sectors/track.\n"), hds, spt);
				warn_funky_floppy();
			}
			break;
		case 1:
		default:
			(void) fprintf(stderr,
			    gettext("Unknown diskette parameters!  "
				"3.5'' diskette with %d heads "), hds);
			warn_funky_floppy();
		}
		break;
	case 5:
		switch (hds) {
		case 2:
			switch (spt) {
			case 15:
				wbpb->bpb.media = 0xF9;
				wbpb->bpb.num_root_entries = 224;
				wbpb->bpb.sectors_per_cluster = 1;
				wbpb->bpb.sectors_per_fat = 7;
				break;
			case 9:
				wbpb->bpb.media = 0xFD;
				wbpb->bpb.num_root_entries = 112;
				wbpb->bpb.sectors_per_cluster = 2;
				wbpb->bpb.sectors_per_fat = 2;
				break;
			case 8:
				wbpb->bpb.media = 0xFF;
				wbpb->bpb.num_root_entries = 112;
				wbpb->bpb.sectors_per_cluster = 1;
				wbpb->bpb.sectors_per_fat = 2;
				break;
			default:
				(void) fprintf(stderr,
				    gettext("Unknown diskette parameters!  "
					"5.25'' diskette with %d heads "
					"and %d sectors/track.\n"), hds, spt);
				warn_funky_floppy();
			}
			break;
		case 1:
			switch (spt) {
			case 9:
				wbpb->bpb.media = 0xFC;
				wbpb->bpb.num_root_entries = 64;
				wbpb->bpb.sectors_per_cluster = 1;
				wbpb->bpb.sectors_per_fat = 2;
				break;
			case 8:
				wbpb->bpb.media = 0xFE;
				wbpb->bpb.num_root_entries = 64;
				wbpb->bpb.sectors_per_cluster = 1;
				wbpb->bpb.sectors_per_fat = 1;
				break;
			default:
				(void) fprintf(stderr,
				    gettext("Unknown diskette parameters! "
					"5.25'' diskette with %d heads "
					"and %d sectors/track.\n"), hds, spt);
				warn_funky_floppy();
			}
			break;
		default:
			(void) fprintf(stderr,
			    gettext("Unknown diskette parameters! "
				"5.25'' diskette with %d heads."), hds);
			warn_funky_floppy();
		}
		break;
	default:
		(void) fprintf(stderr,
		    gettext("\nUnknown diskette type.  Only know about "
			"5.25'' and 3.5'' diskettes.\n"));
		warn_funky_floppy();
	}
}

/*
 *  lookup_floppy
 *
 *	Look up a media descriptor byte and other crucial BPB values
 *	based on floppy characteristics.
 */
static
void
lookup_floppy(struct fd_char *fdchar, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	ulong_t tsize;
	ulong_t cyls, spt, hds, diam;

	cyls = fdchar->fdc_ncyl;
	diam = fdchar->fdc_medium;
	spt = fdchar->fdc_secptrack;
	hds = fdchar->fdc_nhead;

	tsize = cyls * hds * spt;

	if (GetFsParams)
		TotSize = tsize;

	if (GetSize) {
		wbpb->bpb.sectors_in_logical_volume = tsize;
	} else {
		wbpb->bpb.sectors_in_logical_volume =
			warn_mismatch(
			    gettext("length of partition (in sectors)"),
			    gettext("FDIOGCHAR call"), tsize, TotSize);
	}
	wbpb->bpb.sectors_in_volume =
		(short)wbpb->bpb.sectors_in_logical_volume;

	if (GetSPT) {
		wbpb->bpb.sectors_per_track = spt;
	} else {
		wbpb->bpb.sectors_per_track =
			warn_mismatch(
			    gettext("sectors per track"),
			    gettext("FDIOGCHAR call"), spt, SecPerTrk);
		spt = wbpb->bpb.sectors_per_track;
	}

	if (GetTPC) {
		wbpb->bpb.heads = hds;
	} else {
		wbpb->bpb.heads =
			warn_mismatch(
			    gettext("number of heads"),
			    gettext("FDIOGCHAR call"), hds, TrkPerCyl);
		hds = wbpb->bpb.heads;
	}

	Fatentsize = 12;  /* Size of fat entry in bits */
	if (!GetBPF && BitsPerFAT != Fatentsize) {
		warn_funky_fatsize();
	}
	set_fat_string(wbpb, Fatentsize);

	wbpb->ebpb.phys_drive_num = 0;

	wbpb->bpb.hidden_sectors = 0;
	sbpb->bs_offset_high = 0;
	sbpb->bs_offset_low = 0;

	floppy_bpb_fillin(wbpb, diam, hds, spt);
}

/*
 *  compute_cluster_size
 *
 *	Compute an acceptable sectors/cluster value.
 *
 *	Values taken from a table found on p. 407 of "Windows 98
 *	Professional Reference", by Bruce A. Hallberg & Joe
 *	Casad. (ISBN 0-56205-786-3) I believe they've taken their
 *	table directly from the MSDN docs.
 */
static
void
compute_cluster_size(struct _bios_param_blk *wbpb)
{
	ulong_t volsize;
	ulong_t spc, spf;

	volsize = wbpb->bpb.sectors_in_volume ? wbpb->bpb.sectors_in_volume :
		wbpb->bpb.sectors_in_logical_volume;

	if (GetSPC) {
		if (volsize <= 0x10000) {		/* 32M */
			spc = 1;
		} else if (volsize <= 0x20000) {	/* 64M */
			spc = 2;
		} else if (volsize <= 0x40000) {	/* 128M */
			spc = 4;
		} else if (volsize <= 0x7f800) {	/* 255M */
			spc = 8;
		} else if (volsize <= 0xff800) {	/* 511M */
			spc = 16;
		} else if (volsize <= 0x1ff800) {	/* 1023M */
			spc = 32;
		} else if (volsize <= 0x3ff800) {	/* 2047M */
			spc = 64;
		} else if (volsize <= 0x7ff800) {	/* 4095M */
			spc = 128;
		} else {
			(void) fprintf(stderr,
			    gettext("Partition too large for a FAT!\n"));
			exit(4);
		}
	} else {
		spc = SecPerClust;
	}

	if (GetBPF) {
		Fatentsize = 16;
	} else {
		Fatentsize = BitsPerFAT;
		if (Fatentsize == 12 && volsize > DOS_F12MAXC * spc) {
			/*
			 * 4228473 No way to non-interactively make a
			 *	   pcfs filesystem
			 *
			 *	If we don't have an input TTY, or we aren't
			 *	really doing anything, then don't ask
			 *	questions.  Assume a yes answer to any
			 *	questions we would ask.
			 */
			if (Notreally || !isatty(fileno(stdin))) {
			    (void) printf(
				gettext("Volume too large for 12 bit FAT,"
				" increasing to 16 bit FAT size.\n"));
			    (void) fflush(stdout);
			    Fatentsize = 16;
			} else {
			    (void) printf(
				gettext("Volume too large for a 12 bit FAT.\n"
					"Increase to 16 bit FAT "
					"and continue (y/n)? "));
			    (void) fflush(stdout);
			    if (yes())
				Fatentsize = 16;
			    else
				exit(5);
			}
		}
	}
	wbpb->bpb.sectors_per_cluster = spc;
	set_fat_string(wbpb, Fatentsize);

	/* Assuming a 16 bit fat, compute a sector/fat figure */
	spf = idivceil(volsize, (FAT16_ENTSPERSECT * spc + 2));
	wbpb->bpb.sectors_per_fat = (ushort_t)spf;
}

static
void
find_fixed_details(int fd, struct _bios_param_blk *wbpb)
{
	struct dk_geom dginfo;

	/*
	 *  Look up the last remaining bits of info we need
	 *  that is specific to the hard drive using a disk ioctl.
	 */
	if (GetSPT || GetTPC) {
		if ((ioctl(fd, DKIOCG_VIRTGEOM, &dginfo)) == -1) {
		    if ((ioctl(fd, DKIOCG_PHYGEOM, &dginfo)) == -1) {
			if ((ioctl(fd, DKIOCGGEOM, &dginfo)) == -1) {
			    (void) close(fd);
			    perror(
				gettext("Drive geometry lookup (need "
				    "tracks/cylinder and/or sectors/track"));
			    exit(2);
			}
		    }
		}
	}

	wbpb->bpb.heads = (GetTPC ? dginfo.dkg_nhead : TrkPerCyl);
	wbpb->bpb.sectors_per_track = (GetSPT ? dginfo.dkg_nsect : SecPerTrk);

	if (Verbose) {
		if (GetTPC) {
		    (void) printf(
			gettext("DKIOCG determined number of heads = %d\n"),
			dginfo.dkg_nhead);
		}
		if (GetSPT) {
		    (void) printf(
			gettext("DKIOCG determined sectors per track = %d\n"),
			dginfo.dkg_nsect);
		}
	}

	/*
	 * XXX - MAY need an additional flag (or flags) to set media
	 * and physical drive number fields.  That in the case of weird
	 * floppies that have to go through 'nofdisk' route for formatting.
	 */
	wbpb->bpb.media = 0xF8;
	wbpb->bpb.num_root_entries = 512;
	wbpb->ebpb.phys_drive_num = 0x80;
	compute_cluster_size(wbpb);
}

static
char *
stat_actual_disk(char *diskname, struct stat *info, char **suffix)
{
	char *actualdisk;

	if (stat(diskname, info)) {
		/*
		 *  Device named on command line doesn't exist.  That
		 *  probably means there is a partition-specifying
		 *  suffix attached to the actual disk name.
		 */
		actualdisk = strtok(strdup(diskname), ":");
		if (*suffix = strchr(diskname, ':'))
			(*suffix)++;

		if (stat(actualdisk, info)) {
			perror(actualdisk);
			exit(2);
		}
	} else {
		actualdisk = strdup(diskname);
	}

	return (actualdisk);
}

static
void
compute_file_area_size(struct _bios_param_blk *wbpb)
{
	/*
	 * First we'll find total number of sectors in the file area...
	 */
	if (wbpb->bpb.sectors_in_volume > 0)
		TotalClusters = wbpb->bpb.sectors_in_volume;
	else
		TotalClusters = wbpb->bpb.sectors_in_logical_volume;
	TotalClusters -= wbpb->bpb.resv_sectors;
	TotalClusters -= wbpb->bpb.num_root_entries * sizeof (struct pcdir) /
	    BPSEC;
	TotalClusters -= 2 * wbpb->bpb.sectors_per_fat;
	/*
	 * Now change sectors to clusters
	 */
	TotalClusters = TotalClusters / wbpb->bpb.sectors_per_cluster;

	if (Verbose)
		(void) printf(gettext("Disk has a file area of %d "
		    "allocation units,\neach with %d sectors = %d "
		    "bytes.\n"), TotalClusters, wbpb->bpb.sectors_per_cluster,
		    TotalClusters * wbpb->bpb.sectors_per_cluster * BPSEC);
}

#ifndef i386
/*
 *  swap_pack_{bpb,sebpb}cpy
 *
 *	If not on an x86 we assume the structures making up the bpb
 *	were not packed and that longs and shorts need to be byte swapped
 *	(we've kept everything in host order up until now).  A new architecture
 *	might not need to swap or might not need to pack, in which case
 *	new routines will have to be written.  Of course if an architecture
 *	supports both packing and little-endian host order, it can follow the
 *	same path as the x86 code.
 */
static
void
swap_pack_bpbcpy(struct _boot_sector *bsp, struct _bios_param_blk *wbpb)
{
	uchar_t *fillp;

	fillp = (uchar_t *)&(bsp->bs_filler[ORIG_BPB_START_INDEX]);

	*fillp++ = getbyte(wbpb->bpb.bytes_sector, 1);
	*fillp++ = getbyte(wbpb->bpb.bytes_sector, 0);
	*fillp++ = wbpb->bpb.sectors_per_cluster;
	*fillp++ = getbyte(wbpb->bpb.resv_sectors, 1);
	*fillp++ = getbyte(wbpb->bpb.resv_sectors, 0);
	*fillp++ = wbpb->bpb.num_fats;
	*fillp++ = getbyte(wbpb->bpb.num_root_entries, 1);
	*fillp++ = getbyte(wbpb->bpb.num_root_entries, 0);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_volume, 1);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_volume, 0);
	*fillp++ = wbpb->bpb.media;
	*fillp++ = getbyte(wbpb->bpb.sectors_per_fat, 1);
	*fillp++ = getbyte(wbpb->bpb.sectors_per_fat, 0);
	*fillp++ = getbyte(wbpb->bpb.sectors_per_track, 1);
	*fillp++ = getbyte(wbpb->bpb.sectors_per_track, 0);
	*fillp++ = getbyte(wbpb->bpb.heads, 1);
	*fillp++ = getbyte(wbpb->bpb.heads, 0);
	*fillp++ = getbyte(wbpb->bpb.hidden_sectors, 3);
	*fillp++ = getbyte(wbpb->bpb.hidden_sectors, 2);
	*fillp++ = getbyte(wbpb->bpb.hidden_sectors, 1);
	*fillp++ = getbyte(wbpb->bpb.hidden_sectors, 0);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_logical_volume, 3);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_logical_volume, 2);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_logical_volume, 1);
	*fillp++ = getbyte(wbpb->bpb.sectors_in_logical_volume, 0);
	*fillp++ = wbpb->ebpb.phys_drive_num;
	*fillp++ = wbpb->ebpb.reserved;
	*fillp++ = wbpb->ebpb.ext_signature;
	*fillp++ = getbyte(wbpb->ebpb.volume_id, 3);
	*fillp++ = getbyte(wbpb->ebpb.volume_id, 2);
	*fillp++ = getbyte(wbpb->ebpb.volume_id, 1);
	*fillp++ = getbyte(wbpb->ebpb.volume_id, 0);

	(void) strncpy((char *)fillp, (char *)wbpb->ebpb.volume_label, 11);
	fillp += 11;
	(void) strncpy((char *)fillp, (char *)wbpb->ebpb.type, 8);
}

static
void
swap_pack_sebpbcpy(struct _boot_sector *bsp, struct _sun_bpb_extensions *sbpb)
{
	uchar_t *fillp;

	fillp = bsp->bs_sun_bpb;
	*fillp++ = getbyte(sbpb->bs_offset_high, 1);
	*fillp++ = getbyte(sbpb->bs_offset_high, 0);
	*fillp++ = getbyte(sbpb->bs_offset_low, 1);
	*fillp++ = getbyte(sbpb->bs_offset_low, 0);
}

static
void
swap_pack_grabbpb(struct _bios_param_blk *wbpb, struct _boot_sector *bsp)
{
	uchar_t *grabp;

	grabp = (uchar_t *)&(bsp->bs_filler[ORIG_BPB_START_INDEX]);

	((uchar_t *)&(wbpb->bpb.bytes_sector))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.bytes_sector))[0] = *grabp++;
	wbpb->bpb.sectors_per_cluster = *grabp++;
	((uchar_t *)&(wbpb->bpb.resv_sectors))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.resv_sectors))[0] = *grabp++;
	wbpb->bpb.num_fats = *grabp++;
	((uchar_t *)&(wbpb->bpb.num_root_entries))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.num_root_entries))[0] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_volume))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_volume))[0] = *grabp++;
	wbpb->bpb.media = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_per_fat))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_per_fat))[0] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_per_track))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_per_track))[0] = *grabp++;
	((uchar_t *)&(wbpb->bpb.heads))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.heads))[0] = *grabp++;
	((uchar_t *)&(wbpb->bpb.hidden_sectors))[3] = *grabp++;
	((uchar_t *)&(wbpb->bpb.hidden_sectors))[2] = *grabp++;
	((uchar_t *)&(wbpb->bpb.hidden_sectors))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.hidden_sectors))[0] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_logical_volume))[3] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_logical_volume))[2] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_logical_volume))[1] = *grabp++;
	((uchar_t *)&(wbpb->bpb.sectors_in_logical_volume))[0] = *grabp++;
	wbpb->ebpb.phys_drive_num = *grabp++;
	wbpb->ebpb.reserved = *grabp++;
	wbpb->ebpb.ext_signature = *grabp++;
	((uchar_t *)&(wbpb->ebpb.volume_id))[3] = *grabp++;
	((uchar_t *)&(wbpb->ebpb.volume_id))[2] = *grabp++;
	((uchar_t *)&(wbpb->ebpb.volume_id))[1] = *grabp++;
	((uchar_t *)&(wbpb->ebpb.volume_id))[0] = *grabp++;

	(void) strncpy((char *)wbpb->ebpb.volume_label, (char *)grabp, 11);
	grabp += 11;
	(void) strncpy((char *)wbpb->ebpb.type, (char *)grabp, 8);
}

static
void
swap_pack_grabsebpb(struct _sun_bpb_extensions *sbpb, struct _boot_sector *bsp)
{
	uchar_t *grabp;

	grabp = bsp->bs_sun_bpb;
	((uchar_t *)&(sbpb->bs_offset_high))[1] = *grabp++;
	((uchar_t *)&(sbpb->bs_offset_high))[0] = *grabp++;
	((uchar_t *)&(sbpb->bs_offset_low))[1] = *grabp++;
	((uchar_t *)&(sbpb->bs_offset_low))[0] = *grabp++;
}

static
void
swap_pack_grab32bpb(struct _bpb32_extensions *wbpb, struct _boot_sector *bsp)
{
	uchar_t *grabp;

	grabp = (uchar_t *)&(bsp->bs_filler[BPB_32_START_INDEX]);

	((uchar_t *)&(wbpb->big_sectors_per_fat))[3] = *grabp++;
	((uchar_t *)&(wbpb->big_sectors_per_fat))[2] = *grabp++;
	((uchar_t *)&(wbpb->big_sectors_per_fat))[1] = *grabp++;
	((uchar_t *)&(wbpb->big_sectors_per_fat))[0] = *grabp++;
	((uchar_t *)&(wbpb->ext_flags))[1] = *grabp++;
	((uchar_t *)&(wbpb->ext_flags))[0] = *grabp++;
	wbpb->fs_vers_lo = *grabp++;
	wbpb->fs_vers_hi = *grabp++;
	((uchar_t *)&(wbpb->root_dir_clust))[3] = *grabp++;
	((uchar_t *)&(wbpb->root_dir_clust))[2] = *grabp++;
	((uchar_t *)&(wbpb->root_dir_clust))[1] = *grabp++;
	((uchar_t *)&(wbpb->root_dir_clust))[0] = *grabp++;
	((uchar_t *)&(wbpb->fsinfosec))[1] = *grabp++;
	((uchar_t *)&(wbpb->fsinfosec))[0] = *grabp++;
	((uchar_t *)&(wbpb->backupboot))[1] = *grabp++;
	((uchar_t *)&(wbpb->backupboot))[0] = *grabp++;
	((uchar_t *)&(wbpb->reserved))[1] = *grabp++;
	((uchar_t *)&(wbpb->reserved))[0] = *grabp++;
}
#endif	/* ! i386 */

static
void
dashm_bail(int fd)
{
	(void) fprintf(stderr,
		gettext("This media does not appear to be "
			"formatted with a FAT file system.\n"));
	(void) close(fd);
	exit(6);
}

/*
 *  read_existing_bpb
 *
 *	Grab the first sector, which we think is a bios parameter block.
 *	If it looks bad, bail.  Otherwise fill in the parameter struct
 *	fields that matter.
 */
static
void
read_existing_bpb(int fd, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	struct _bpb32_extensions check32;
	boot_sector_t ubpb;

	if (read(fd, ubpb.buf, BPSEC) < BPSEC) {
		perror(gettext("Read BIOS parameter block "
			"from previously formatted media"));
		(void) close(fd);
		exit(6);
	}

	if (ltohs(ubpb.mb.signature) != BOOTSECSIG) {
		dashm_bail(fd);
	}

#ifdef i386
	(void) memcpy(wbpb, &(ubpb.bs.bs_bpb), sizeof (*wbpb));
#else
	swap_pack_grabbpb(wbpb, &(ubpb.bs));
#endif
	if (SunBPBfields) {
#ifdef i386
		(void) memcpy(sbpb, &(ubpb.bs.bs_sun_bpb), sizeof (*sbpb));
#else
		swap_pack_grabsebpb(sbpb, &(ubpb.bs));
#endif
	}
	if (wbpb->bpb.bytes_sector != BPSEC) {
		(void) fprintf(stderr,
		    gettext("Bogus bytes per sector value.\n"));
		if (!powerofx_le_y(2, BPSEC * 8, wbpb->bpb.bytes_sector)) {
			(void) fprintf(stderr,
			    gettext("The device name may be missing a "
				    "logical drive specifier.\n"));
			(void) close(fd);
			exit(6);
		} else {
			(void) fprintf(stderr,
			    gettext("Do not know how to build FATs with a\n"
				    "non-standard sector size. Standard "
				    "size is %d bytes,\nyour sector size "
				    "is %d bytes.\n"), BPSEC,
				    wbpb->bpb.bytes_sector);
			(void) close(fd);
			exit(6);
		}
	}
	if (!(powerofx_le_y(2, 128, wbpb->bpb.sectors_per_cluster))) {
		(void) fprintf(stderr,
		    gettext("Bogus sectors per cluster value.\n"));
		(void) fprintf(stderr,
		    gettext("The device name may be missing a "
			"logical drive specifier.\n"));
		(void) close(fd);
		exit(6);
	}
	compute_file_area_size(wbpb);

	if (wbpb->bpb.sectors_per_fat == 0) {
#ifdef i386
		(void) memcpy(&check32, &(ubpb.bs.bs_bpb.ebpb),
			sizeof (check32));
#else
		swap_pack_grab32bpb(&check32, &(ubpb.bs));
#endif
		if ((check32.big_sectors_per_fat * BPSEC * 8 /
		    TotalClusters) > 16) {
			(void) fprintf(stderr,
				gettext("Do not know how to build "
				    "32 bit FATs (yet).\n"));
			(void) close(fd);
			exit(6);
		} else {
			dashm_bail(fd);
		}
	}
}

/*
 *  compare_existing_with_computed
 *
 *	We use this function when we the user specifies the -m option.
 *	We compute and look up things like we would if they had asked
 *	us to make the fs, and compare that to what's already layed down
 *	in the existing fs.  If there's a difference we can tell them what
 *	options to specify in order to reproduce their existing layout.
 *	Note that they still may not get an exact duplicate, because we
 *	don't, for example, preserve their existing boot code.  We think
 *	we've got all the fields that matter covered, though.
 *
 *	XXX - We're basically ignoring sbpb at this point.  I'm unsure
 *	if we'll ever care about those fields, in terms of the -m option.
 */
static
void
compare_existing_with_computed(int fd, char *suffix,
    struct _bios_param_blk *wbpb, struct _sun_bpb_extensions *sbpb,
    int *prtsize, int *prtspc, int *prtbpf, int *prtnsect, int *prtntrk,
    int *prtfdisk, int *prthidden, int *prtrsrvd, int *dashos)
{
	struct _bios_param_blk compare;
	struct _sun_bpb_extensions compsun;
	struct dk_geom dginfo;
	struct fd_char fdchar;
	int fd_ioctl_worked = 0;
	/*
	 *  For all non-floppy cases we expect to find a 16-bit FAT
	 */
	int expectfatsize = 16;

	compare = *wbpb;
	compsun = *sbpb;

	if (!suffix) {
		if (ioctl(fd, FDIOGCHAR, &fdchar) != -1) {
			expectfatsize = 12;
			fd_ioctl_worked++;
		}
	}

	if (fd_ioctl_worked) {
#ifdef sparc
		fdchar.fdc_medium = 3;
#endif
		GetSize = GetSPT = GetSPC = GetTPC = GetBPF = 1;
		lookup_floppy(&fdchar, &compare, &compsun);
		if (compare.bpb.heads != wbpb->bpb.heads) {
			(*prtntrk)++;
			(*dashos)++;
		}
		if (compare.bpb.sectors_per_track !=
		    wbpb->bpb.sectors_per_track) {
			(*prtnsect)++;
			(*dashos)++;
		}
	} else {
		int dk_ioctl_worked = 1;

		if (!suffix) {
			(*prtfdisk)++;
			(*prtsize)++;
			*dashos += 2;
		}
		if ((ioctl(fd, DKIOCG_VIRTGEOM, &dginfo)) == -1) {
		    if ((ioctl(fd, DKIOCG_PHYGEOM, &dginfo)) == -1) {
			if ((ioctl(fd, DKIOCGGEOM, &dginfo)) == -1) {
				*prtnsect = *prtntrk = 1;
				*dashos += 2;
				dk_ioctl_worked = 0;
			}
		    }
		}
		if (dk_ioctl_worked) {
			if (dginfo.dkg_nhead != wbpb->bpb.heads) {
				(*prtntrk)++;
				(*dashos)++;
			}
			if (dginfo.dkg_nsect !=
			    wbpb->bpb.sectors_per_track) {
				(*prtnsect)++;
				(*dashos)++;
			}
		}
		GetBPF = GetSPC = 1;
		compute_cluster_size(&compare);
	}

	if (!*prtfdisk && TotSize != wbpb->bpb.sectors_in_volume &&
		TotSize != wbpb->bpb.sectors_in_logical_volume) {
		(*dashos)++;
		(*prtsize)++;
	}

	if (compare.bpb.sectors_per_cluster != wbpb->bpb.sectors_per_cluster) {
		(*dashos)++;
		(*prtspc)++;
	}

	if (compare.bpb.hidden_sectors != wbpb->bpb.hidden_sectors) {
		(*dashos)++;
		(*prthidden)++;
	}

	if (compare.bpb.resv_sectors != wbpb->bpb.resv_sectors) {
		(*dashos)++;
		(*prtrsrvd)++;
	}

	/*
	 * Compute approximate Fatentsize.  It's approximate because the
	 * size of the FAT may not be exactly a multiple of the number of
	 * clusters.  It should be close, though.
	 */
	Fatentsize = wbpb->bpb.sectors_per_fat * BPSEC * 8 / TotalClusters;
	if (Fatentsize <= 12)
		Fatentsize = 12;
	else
		Fatentsize = 16;
	if (Fatentsize != expectfatsize) {
		(*dashos)++;
		(*prtbpf)++;
	}
}

static
void
print_reproducing_command(int fd, char *actualdisk, char *suffix,
    struct _bios_param_blk *wbpb, struct _sun_bpb_extensions *sbpb)
{
	int needcomma = 0;
	int prthidden = 0;
	int prtrsrvd = 0;
	int prtfdisk = 0;
	int prtnsect = 0;
	int prtntrk = 0;
	int prtsize = 0;
	int prtbpf = 0;
	int prtspc = 0;
	int dashos = 0;
	int ll, i;

	compare_existing_with_computed(fd, suffix, wbpb, sbpb,
	    &prtsize, &prtspc, &prtbpf, &prtnsect, &prtntrk,
	    &prtfdisk, &prthidden, &prtrsrvd, &dashos);

	/*
	 *  Print out the command line they can use to reproduce the
	 *  file system.
	 */
	(void) printf("mkfs -F pcfs");

	ll = min(11, (int)strlen((char *)wbpb->ebpb.volume_label));
	/*
	 * First, eliminate trailing spaces. Now compare the name against
	 * our default label.  If there's a match we don't need to print
	 * any label info.
	 */
	i = ll;
	while (wbpb->ebpb.volume_label[--i] == ' ');
	ll = i;

	if (ll == strlen(DEFAULT_LABEL) - 1) {
		char cmpbuf[11];

		(void) strcpy(cmpbuf, DEFAULT_LABEL);
		for (i = ll; i >= 0; i--) {
			if (cmpbuf[i] !=
			    toupper((int)(wbpb->ebpb.volume_label[i]))) {
				break;
			}
		}
		if (i < 0)
			ll = i;
	}

	if (ll >= 0) {
		(void) printf(" -o ");
		(void) printf("b=\"");
		for (i = 0; i <= ll; i++) {
			(void) printf("%c", wbpb->ebpb.volume_label[i]);
		}
		(void) printf("\"");
		needcomma++;
	} else if (dashos) {
		(void) printf(" -o ");
	}

#define	NEXT_DASH_O	dashos--; needcomma++; continue

	while (dashos) {
		if (needcomma) {
			(void) printf(",");
			needcomma = 0;
		}
		if (prtfdisk) {
			(void) printf("nofdisk");
			prtfdisk--;
			NEXT_DASH_O;
		}
		if (prtsize) {
			(void) printf("size=%ld", wbpb->bpb.sectors_in_volume ?
			    wbpb->bpb.sectors_in_volume :
			    wbpb->bpb.sectors_in_logical_volume);
			prtsize--;
			NEXT_DASH_O;
		}
		if (prtnsect) {
			(void) printf("nsect=%d", wbpb->bpb.sectors_per_track);
			prtnsect--;
			NEXT_DASH_O;
		}
		if (prtspc) {
			(void) printf("spc=%d", wbpb->bpb.sectors_per_cluster);
			prtspc--;
			NEXT_DASH_O;
		}
		if (prtntrk) {
			(void) printf("ntrack=%d", wbpb->bpb.heads);
			prtntrk--;
			NEXT_DASH_O;
		}
		if (prtbpf) {
			(void) printf("fat=%d", Fatentsize);
			prtbpf--;
			NEXT_DASH_O;
		}
		if (prthidden) {
			(void) printf("hidden=%ld", wbpb->bpb.hidden_sectors);
			prthidden--;
			NEXT_DASH_O;
		}
		if (prtrsrvd) {
			(void) printf("reserve=%d", wbpb->bpb.resv_sectors);
			prtrsrvd--;
			NEXT_DASH_O;
		}
	}

	(void) printf(" %s%c%c\n", actualdisk,
	    suffix ? ':' : '\0', suffix ? *suffix : '\0');
}

/*
 *  open_and_examine
 *
 *	Open the requested 'dev_name'.  Seek to point where
 *	we'd expect to find boot sectors, etc., based on any ':partition'
 *	attachments to the dev_name.
 *
 *	Examine the fields of any existing boot sector and display best
 *	approximation of how this fs could be reproduced with this command.
 */
static
int
open_and_examine(char *dn, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	struct stat di;
	char *actualdisk = NULL;
	char *suffix = NULL;
	int fd;

	if (Verbose)
		(void) printf(gettext("Opening destination device/file.\n"));

	actualdisk = stat_actual_disk(dn, &di, &suffix);

	/*
	 *  Destination exists, now find more about it.
	 */
	if (!(S_ISCHR(di.st_mode))) {
		(void) fprintf(stderr,
		    gettext("\n%s: device name must be a "
			"character special device.\n"), actualdisk);
		exit(2);
	} else if ((fd = open(actualdisk, O_RDWR | O_EXCL)) < 0) {
		perror(actualdisk);
		exit(2);
	}

	/*
	 * Find appropriate partition if we were requested to do so.
	 */
	if (suffix && !(seek_partn(fd, suffix, wbpb, sbpb))) {
		(void) close(fd);
		exit(2);
	}

	read_existing_bpb(fd, wbpb, sbpb);
	print_reproducing_command(fd, actualdisk, suffix, wbpb, sbpb);

	return (fd);
}

/*
 *  open_and_seek
 *
 *	Open the requested 'dev_name'.  Seek to point where
 *	we'll write boot sectors, etc., based on any ':partition'
 *	attachments to the dev_name.
 *
 *	By the time we are finished here, the entire BPB will be
 *	filled in, excepting the volume label.
 */
static
int
open_and_seek(char *dn, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	struct fd_char fdchar;
	struct stat di;
	char *actualdisk = NULL;
	char *suffix = NULL;
	int fd;

	if (Verbose)
		(void) printf(gettext("Opening destination device/file.\n"));

	/*
	 * We hold these truths to be self evident, all BPBs we create
	 * will have these values in these fields.
	 */
	wbpb->bpb.num_fats = 2;
	wbpb->bpb.bytes_sector = BPSEC;

	/*
	 * Assign or use supplied numbers for hidden and
	 * reserved sectors in the file system.
	 */
	if (GetResrvd)
		wbpb->bpb.resv_sectors = 1;
	else
		wbpb->bpb.resv_sectors = Resrvd;

	wbpb->ebpb.ext_signature = 0x29; /* Magic number for modern format */
	wbpb->ebpb.volume_id = 0;

	/*
	 * If all output goes to a simple file, call a routine to setup
	 * that scenario. Otherwise, try to find the device.
	 */
	if (Outputtofile)
		return (fd = prepare_image_file(dn, wbpb, sbpb));

	actualdisk = stat_actual_disk(dn, &di, &suffix);

	/*
	 * Sanity check.  If we've been provided a partition-specifying
	 * suffix, we shouldn't also have been told to ignore the
	 * fdisk table.
	 */
	if (DontUseFdisk && suffix) {
		(void) fprintf(stderr,
		    gettext("Using 'nofdisk' option precludes "
			    "appending logical drive\nspecifier "
			    "to the device name.\n"));
		exit(2);
	}

	/*
	 *  Destination exists, now find more about it.
	 */
	if (!(S_ISCHR(di.st_mode))) {
		(void) fprintf(stderr,
		    gettext("\n%s: device name must indicate a "
			"character special device.\n"), actualdisk);
		exit(2);
	} else if ((fd = open(actualdisk, O_RDWR | O_EXCL)) < 0) {
		perror(actualdisk);
		exit(2);
	}

	/*
	 * Find appropriate partition if we were requested to do so.
	 */
	if (suffix && !(seek_partn(fd, suffix, wbpb, sbpb))) {
		(void) close(fd);
		exit(2);
	}

	if (!suffix) {
		/*
		 * We have one of two possibilities.  Chances are we have
		 * a floppy drive.  But the user may be trying to format
		 * some weird drive that we don't know about and is supplying
		 * all the important values.  In that case, they should have set
		 * the 'nofdisk' flag.
		 *
		 * If 'nofdisk' isn't set, do a floppy-specific ioctl to
		 * get the remainder of our info. If the ioctl fails, we have
		 * a good idea that they aren't really on a floppy.  In that
		 * case, they should have given us a partition specifier.
		 */
		if (DontUseFdisk) {
			if (!(seek_nofdisk(fd, wbpb, sbpb))) {
				(void) close(fd);
				exit(2);
			}
			find_fixed_details(fd, wbpb);
		} else if (ioctl(fd, FDIOGCHAR, &fdchar) == -1) {
			if (errno == ENOTTY) {
				partn_lecture(actualdisk);
				(void) close(fd);
				exit(2);
			}
		} else {
#ifdef sparc
			fdchar.fdc_medium = 3;
#endif
			lookup_floppy(&fdchar, wbpb, sbpb);
		}
	} else {
		find_fixed_details(fd, wbpb);
	}

	return (fd);
}

/*
 * The following is a copy of MS-DOS 4.0 boot block.
 * It consists of the BIOS parameter block, and a disk
 * bootstrap program.
 *
 * The BIOS parameter block contains the right values
 * for the 3.5" high-density 1.44MB floppy format.
 *
 * This will be our default boot sector, if the user
 * didn't point us at a different one.
 *
 */
static
uchar_t DefBootSec[512] = {
	0xeb, 0x3c, 0x90, 	/* 8086 short jump + displacement + NOP */
	'M', 'S', 'D', 'O', 'S', '4', '.', '0',	/* OEM name & version */
	0x00, 0x02, 0x01, 0x01, 0x00,
	0x02, 0xe0, 0x00, 0x40, 0x0b,
	0xf0, 0x09, 0x00, 0x12, 0x00,
	0x02, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x29, 0x00, 0x00, 0x00, 0x00,
	'N', 'O', 'N', 'A', 'M', 'E', ' ', ' ', ' ', ' ', ' ',
	'F', 'A', 'T', '1', '2', ' ', ' ', ' ',
	0xfa, 0x33,
	0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16, 0x07,
	0xbb, 0x78, 0x00, 0x36, 0xc5, 0x37, 0x1e, 0x56,
	0x16, 0x53, 0xbf, 0x3e, 0x7c, 0xb9, 0x0b, 0x00,
	0xfc, 0xf3, 0xa4, 0x06, 0x1f, 0xc6, 0x45, 0xfe,
	0x0f, 0x8b, 0x0e, 0x18, 0x7c, 0x88, 0x4d, 0xf9,
	0x89, 0x47, 0x02, 0xc7, 0x07, 0x3e, 0x7c, 0xfb,
	0xcd, 0x13, 0x72, 0x7c, 0x33, 0xc0, 0x39, 0x06,
	0x13, 0x7c, 0x74, 0x08, 0x8b, 0x0e, 0x13, 0x7c,
	0x89, 0x0e, 0x20, 0x7c, 0xa0, 0x10, 0x7c, 0xf7,
	0x26, 0x16, 0x7c, 0x03, 0x06, 0x1c, 0x7c, 0x13,
	0x16, 0x1e, 0x7c, 0x03, 0x06, 0x0e, 0x7c, 0x83,
	0xd2, 0x00, 0xa3, 0x50, 0x7c, 0x89, 0x16, 0x52,
	0x7c, 0xa3, 0x49, 0x7c, 0x89, 0x16, 0x4b, 0x7c,
	0xb8, 0x20, 0x00, 0xf7, 0x26, 0x11, 0x7c, 0x8b,
	0x1e, 0x0b, 0x7c, 0x03, 0xc3, 0x48, 0xf7, 0xf3,
	0x01, 0x06, 0x49, 0x7c, 0x83, 0x16, 0x4b, 0x7c,
	0x00, 0xbb, 0x00, 0x05, 0x8b, 0x16, 0x52, 0x7c,
	0xa1, 0x50, 0x7c, 0xe8, 0x87, 0x00, 0x72, 0x20,
	0xb0, 0x01, 0xe8, 0xa1, 0x00, 0x72, 0x19, 0x8b,
	0xfb, 0xb9, 0x0b, 0x00, 0xbe, 0xdb, 0x7d, 0xf3,
	0xa6, 0x75, 0x0d, 0x8d, 0x7f, 0x20, 0xbe, 0xe6,
	0x7d, 0xb9, 0x0b, 0x00, 0xf3, 0xa6, 0x74, 0x18,
	0xbe, 0x93, 0x7d, 0xe8, 0x51, 0x00, 0x32, 0xe4,
	0xcd, 0x16, 0x5e, 0x1f, 0x8f, 0x04, 0x8f, 0x44,
	0x02, 0xcd, 0x19, 0x58, 0x58, 0x58, 0xeb, 0xe8,
	0xbb, 0x00, 0x07, 0xb9, 0x03, 0x00, 0xa1, 0x49,
	0x7c, 0x8b, 0x16, 0x4b, 0x7c, 0x50, 0x52, 0x51,
	0xe8, 0x3a, 0x00, 0x72, 0xe6, 0xb0, 0x01, 0xe8,
	0x54, 0x00, 0x59, 0x5a, 0x58, 0x72, 0xc9, 0x05,
	0x01, 0x00, 0x83, 0xd2, 0x00, 0x03, 0x1e, 0x0b,
	0x7c, 0xe2, 0xe2, 0x8a, 0x2e, 0x15, 0x7c, 0x8a,
	0x16, 0x24, 0x7c, 0x8b, 0x1e, 0x49, 0x7c, 0xa1,
	0x4b, 0x7c, 0xea, 0x00, 0x00, 0x70, 0x00, 0xac,
	0x0a, 0xc0, 0x74, 0x29, 0xb4, 0x0e, 0xbb, 0x07,
	0x00, 0xcd, 0x10, 0xeb, 0xf2, 0x3b, 0x16, 0x18,
	0x7c, 0x73, 0x19, 0xf7, 0x36, 0x18, 0x7c, 0xfe,
	0xc2, 0x88, 0x16, 0x4f, 0x7c, 0x33, 0xd2, 0xf7,
	0x36, 0x1a, 0x7c, 0x88, 0x16, 0x25, 0x7c, 0xa3,
	0x4d, 0x7c, 0xf8, 0xc3, 0xf9, 0xc3, 0xb4, 0x02,
	0x8b, 0x16, 0x4d, 0x7c, 0xb1, 0x06, 0xd2, 0xe6,
	0x0a, 0x36, 0x4f, 0x7c, 0x8b, 0xca, 0x86, 0xe9,
	0x8a, 0x16, 0x24, 0x7c, 0x8a, 0x36, 0x25, 0x7c,
	0xcd, 0x13, 0xc3, 0x0d, 0x0a, 0x4e, 0x6f, 0x6e,
	0x2d, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20,
	0x64, 0x69, 0x73, 0x6b, 0x20, 0x6f, 0x72, 0x20,
	0x64, 0x69, 0x73, 0x6b, 0x20, 0x65, 0x72, 0x72,
	0x6f, 0x72, 0x0d, 0x0a, 0x52, 0x65, 0x70, 0x6c,
	0x61, 0x63, 0x65, 0x20, 0x61, 0x6e, 0x64, 0x20,
	0x70, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e,
	0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x77, 0x68,
	0x65, 0x6e, 0x20, 0x72, 0x65, 0x61, 0x64, 0x79,
	0x0d, 0x0a, 0x00, 0x49, 0x4f, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x4d, 0x53,
	0x44, 0x4f, 0x53, 0x20, 0x20, 0x20, 0x53, 0x59,
	0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa
};

/*
 *  verify_bootblk
 *
 *	We were provided with the name of a file containing the bootblk
 *	to install.  Verify it has a valid boot sector as best we can. Any
 *	errors and we return a bad file descriptor.  Otherwise we fill up the
 *	provided buffer with the boot sector, return the file
 *	descriptor for later use and leave the file pointer just
 *	past the boot sector part of the boot block file.
 */
static
int
verify_bootblkfile(char *fn, boot_sector_t *bs, ulong_t *blkfilesize)
{
	struct stat fi;
	int bsfd = -1;

	if (stat(fn, &fi)) {
		perror(fn);
	} else if (fi.st_size < BPSEC) {
		(void) fprintf(stderr,
		    gettext("%s: Too short to be a boot sector.\n"), fn);
	} else if ((bsfd = open(fn, O_RDONLY)) < 0) {
		perror(fn);
	} else if (read(bsfd, bs->buf, BPSEC) < BPSEC) {
		(void) close(bsfd);
		bsfd = -1;
		perror(gettext("Boot block read"));
	} else {
		if ((bs->bs.bs_jump_code[0] != OPCODE1 &&
		    bs->bs.bs_jump_code[0] != OPCODE2) ||
		    ltohs(bs->bs.bs_signature) != BOOTSECSIG) {
			(void) close(bsfd);
			bsfd = -1;
			(void) fprintf(stderr,
			    gettext("Boot block (%s) bogus.\n"), fn);
		}
		*blkfilesize = fi.st_size;
	}
	return (bsfd);
}

/*
 *  verify_firstfile
 *
 *	We were provided with the name of a file to be the first file
 *	installed on the disk.  We just need to verify it exists and
 *	find out how big it is.  If it doesn't exist, we print a warning
 *	message about how the file wasn't found.  We don't exit fatally,
 *	though, rather we return a size of 0 and the FAT will be built
 *	without installing any first file.  They can then presumably
 *	install the correct first file by hand.
 */
static
int
verify_firstfile(char *fn, ulong_t *filesize)
{
	struct stat fi;
	int fd = -1;

	*filesize = 0;
	if (stat(fn, &fi) || (fd = open(fn, O_RDONLY)) < 0) {
		perror(fn);
		(void) fprintf(stderr,
		    gettext("Could not access requested file.  It will not\n"
			    "be installed in the new file system.\n"));
	} else {
		*filesize = fi.st_size;
	}

	return (fd);
}

/*
 *  label_volume
 *
 *	Fill in BPB with volume label.
 */
static
void
label_volume(char *lbl, struct _bios_param_blk *wbpb)
{
	int ll, i;

	/* Put a volume label into our BPB. */
	if (!lbl)
		lbl = DEFAULT_LABEL;

	ll = min(11, (int)strlen(lbl));
	for (i = 0; i < ll; i++) {
		wbpb->ebpb.volume_label[i] = toupper(lbl[i]);
	}
	for (; i < 11; i++) {
		wbpb->ebpb.volume_label[i] = ' ';
	}
}

static
int
copy_bootblk(char *fn, boot_sector_t *bootsect, ulong_t *bootblksize)
{
	int bsfd = -1;

	if (Verbose && fn)
		(void) printf(gettext("Request to install boot "
		    "block file %s.\n"), fn);
	else if (Verbose)
		(void) printf(gettext("Request to install DOS boot block.\n"));

	/*
	 *  If they want to install their own boot block, sanity check
	 *  that block.
	 */
	if (fn) {
		bsfd = verify_bootblkfile(fn, bootsect, bootblksize);
		if (bsfd < 0) {
			exit(3);
		}
		*bootblksize = roundup(*bootblksize, BPSEC);
	} else {
		(void) memcpy(bootsect, DefBootSec, BPSEC);
		*bootblksize = BPSEC;
	}

	return (bsfd);
}

/*
 *  mark_cluster
 *
 *	This routine fills a FAT entry with the value supplied to it as an
 *	argument.  The fatp argument is assumed to be a pointer to the FAT's
 *	0th entry.  The clustnum is the cluster entry that should be updated.
 *	The value is the new value for the entry.
 */
static
void
mark_cluster(uchar_t *fatp, pc_cluster16_t clustnum, ushort_t value)
{
	uchar_t *ep;
	ulong_t idx;

	idx = (Fatentsize == 16) ? clustnum * 2 : clustnum + clustnum/2;
	ep = fatp + idx;

	if (Fatentsize == 16) {
		*ep = value & 0xff;
		ep++;
		*ep = (value >> 8) & 0xff;
	} else {
		if (clustnum & 1) {
			*ep = (*ep & 0x0f) | ((value << 4) & 0xf0);
			ep++;
			*ep = (value >> 4) & 0xff;
		} else {
			*ep++ = value & 0xff;
			*ep = (*ep & 0xf0) | ((value >> 8) & 0x0f);
		}
	}
}

static
uchar_t *
build_fat(struct _bios_param_blk *wbpb, ulong_t bootblksize,
    ulong_t *fatsize, char *ffn, int *fffd, ulong_t *ffsize,
    pc_cluster16_t *ffstartclust)
{
	pc_cluster16_t cn;
	uchar_t *fatp;
	ushort_t numclust, numsect;
	int  remclust;

	/* Alloc space for a FAT and then null it out. */
	if (Verbose) {
		(void) printf(gettext("BUILD FAT.\n%d sectors per fat.\n"),
		    wbpb->bpb.sectors_per_fat);
	}
	*fatsize = BPSEC * wbpb->bpb.sectors_per_fat;
	if (!(fatp = (uchar_t *)malloc(*fatsize))) {
		perror(gettext("FAT table alloc"));
		exit(4);
	} else {
		(void) memset(fatp, 0, *fatsize);
	}

	/* Build in-memory FAT */
	*fatp = wbpb->bpb.media;
	*(fatp + 1) = 0xFF;
	*(fatp + 2) = 0xFF;

	if (Fatentsize == 16)
		*(fatp + 3) = 0xFF;

	/*
	 * Keep track of clusters used.
	 */
	remclust = TotalClusters;

	/*
	 * Get info on first file to install, if any.
	 */
	if (ffn)
		*fffd = verify_firstfile(ffn, ffsize);

	/*
	 * Compute number of clusters to preserve for bootblk overage.
	 * Remember that we already wrote the first sector of the boot block.
	 * These clusters are marked BAD to prevent them from being deleted
	 * or used.  The first available cluster is 2, so we always offset
	 * the clusters.
	 */
	numsect = idivceil((bootblksize - BPSEC), BPSEC);
	numclust = idivceil(numsect, wbpb->bpb.sectors_per_cluster);

	if (Verbose && numclust)
		(void) printf(gettext("Hiding %d excess bootblk cluster(s).\n"),
		    numclust);
	for (cn = 0; cn < numclust; cn++)
		mark_cluster(fatp, cn + 2, PCF_BADCLUSTER);
	remclust -= numclust;

	/*
	 * Compute and preserve number of clusters for first file.
	 */
	if (*fffd >= 0) {
		*ffstartclust = cn + 2;
		numsect = idivceil(*ffsize, BPSEC);
		numclust = idivceil(numsect, wbpb->bpb.sectors_per_cluster);

		if (numclust > remclust) {
			(void) fprintf(stderr,
				gettext("Requested first file too large to be\n"
					"installed in the new file system.\n"));
			(void) close(*fffd);
			*fffd = -1;
			return (fatp);
		}

		if (Verbose)
			(void) printf(gettext("Reserving %d first file "
			    "cluster(s).\n"), numclust);
		for (cn = 0; (int)cn < (int)(numclust-1); cn++)
			mark_cluster(fatp, *ffstartclust + cn,
			    *ffstartclust + cn + 1);
		mark_cluster(fatp, *ffstartclust + cn, PCF_LASTCLUSTER);
	}

	return (fatp);
}

static
void
dirent_time_fill(struct pcdir *dep)
{
	struct  timeval tv;
	struct	tm	*tp;
	ushort_t	dostime;
	ushort_t	dosday;

	(void) gettimeofday(&tv, (struct timezone *)0);
	tp = localtime(&tv.tv_sec);
	/* get the time & day into DOS format */
	dostime = tp->tm_sec / 2;
	dostime |= tp->tm_min << 5;
	dostime |= tp->tm_hour << 11;
	dosday = tp->tm_mday;
	dosday |= (tp->tm_mon + 1) << 5;
	dosday |= (tp->tm_year - 80) << 9;
	dep->pcd_mtime.pct_time = htols(dostime);
	dep->pcd_mtime.pct_date = htols(dosday);
}

static
void
dirent_fname_fill(struct pcdir *dep, char *fn)
{
	char *fname, *fext;
	int nl, i;

	if (fname = strrchr(fn, '/')) {
		fname++;
	} else {
		fname = fn;
	}

	if (fext = strrchr(fname, '.')) {
		fext++;
	} else {
		fext = "";
	}

	fname = strtok(fname, ".");

	nl = min(PCFNAMESIZE, (int)strlen(fname));
	for (i = 0; i < nl; i++) {
		dep->pcd_filename[i] = toupper(fname[i]);
	}
	for (; i < PCFNAMESIZE; i++) {
		dep->pcd_filename[i] = ' ';
	}

	nl = min(PCFEXTSIZE, (int)strlen(fext));
	for (i = 0; i < nl; i++) {
		dep->pcd_ext[i] = toupper(fext[i]);
	}
	for (; i < PCFEXTSIZE; i++) {
		dep->pcd_ext[i] = ' ';
	}
}

static
uchar_t *
build_rootdir(struct _bios_param_blk *wbpb, char *ffn, int fffd,
    ulong_t ffsize, pc_cluster16_t ffstart, ulong_t *rdirsize)
{
	struct pcdir *entry;

	/*
	 * Build a root directory.  It will be empty if we don't have
	 * a first file we are installing.
	 */
	*rdirsize = wbpb->bpb.num_root_entries * sizeof (struct pcdir);
	if (!(entry = (struct pcdir *)malloc(*rdirsize))) {
		perror(gettext("Root directory allocation"));
		exit(4);
	} else {
		(void) memset((char *)entry, 0, *rdirsize);
	}

	/* Create directory entry for first file, if there is one */
	if (fffd >= 0) {
		dirent_fname_fill(entry, ffn);
		entry->pcd_attr = Firstfileattr;
		dirent_time_fill(entry);
		entry->pcd_scluster_lo = htols(ffstart);
		entry->pcd_size = htoli(ffsize);
	}

	return ((uchar_t *)entry);
}

/*
 * write_rest
 *
 *	Write all the bytes from the current file pointer to end of file
 *	in the source file out to the destination file.  The writes should
 *	be padded to whole clusters with 0's if necessary.
 */
static
void
write_rest(struct _bios_param_blk *wbpb, char *efn,
    int dfd, int sfd, int remaining)
{
	char buf[BPSEC];
	ushort_t numsect, numclust;
	ushort_t wnumsect, s;
	int doneread = 0;
	int rstat;

	/*
	 * Compute number of clusters required to contain remaining bytes.
	 */
	numsect = idivceil(remaining, BPSEC);
	numclust = idivceil(numsect, wbpb->bpb.sectors_per_cluster);

	wnumsect = numclust * wbpb->bpb.sectors_per_cluster;
	for (s = 0; s < wnumsect; s++) {
		if (!doneread) {
			if ((rstat = read(sfd, buf, BPSEC)) < 0) {
				perror(efn);
				doneread = 1;
				rstat = 0;
			} else if (rstat == 0) {
				doneread = 1;
			}
			(void) memset(&(buf[rstat]), 0, BPSEC - rstat);
		}
		if (write(dfd, buf, BPSEC) != BPSEC) {
			(void) fprintf(stderr, gettext("Copying "));
			perror(efn);
		}
	}
}

static
void
write_fat(int fd, char *fn, char *lbl, char *ffn, struct _bios_param_blk *wbpb,
    struct _sun_bpb_extensions *sbpb)
{
	pc_cluster16_t ffsc;
	boot_sector_t bootsect;
	uchar_t *fatp, *rdirp;
	ulong_t bootblksize, fatsize, rdirsize, ffsize;
	int bsfd = -1;
	int fffd = -1;

	compute_file_area_size(wbpb);

	bsfd = copy_bootblk(fn, &bootsect, &bootblksize);
	label_volume(lbl, wbpb);

	/* Copy our BPB into bootsec structure */
#ifdef i386
	(void) memcpy(&(bootsect.bs.bs_bpb), wbpb, sizeof (*wbpb));
#else
	swap_pack_bpbcpy(&(bootsect.bs), wbpb);
#endif

	/* Copy SUN BPB extensions into bootsec structure */
	if (SunBPBfields)
#ifdef i386
		(void) memcpy(&(bootsect.bs.bs_sun_bpb), sbpb, sizeof (*sbpb));
#else
		swap_pack_sebpbcpy(&(bootsect.bs), sbpb);
#endif

	/* Write boot sector */
	if (!Notreally) {
		if (write(fd, bootsect.buf, sizeof (bootsect.buf)) != BPSEC) {
			perror(gettext("Boot sector write"));
			exit(4);
		}
	}

	if (Verbose)
		(void) printf(gettext("Building FAT.\n"));
	fatp = build_fat(wbpb, bootblksize, &fatsize,
	    ffn, &fffd, &ffsize, &ffsc);

	/* Write FAT */
	if (Verbose)
		(void) printf(gettext("Writing FAT(s). %d bytes times %d.\n"),
		    fatsize, wbpb->bpb.num_fats);
	if (!Notreally) {
		int nf, wb;
		for (nf = 0; nf < (int)wbpb->bpb.num_fats; nf++)
			if ((wb = write(fd, fatp, fatsize)) != fatsize) {
				perror(gettext("FAT write"));
				exit(4);
			} else {
				if (Verbose)
					(void) printf(
					    gettext("Wrote %d bytes\n"), wb);
			}
	}

	free(fatp);

	if (Verbose)
		(void) printf(gettext("Building root directory.\n"));
	rdirp = build_rootdir(wbpb, ffn, fffd, ffsize, ffsc, &rdirsize);

	if (Verbose)
		(void) printf(gettext("Writing root directory. %d bytes.\n"),
		    rdirsize);
	if (!Notreally) {
		if (write(fd, rdirp, rdirsize) != rdirsize) {
			perror(gettext("Root directory write"));
			exit(4);
		}
	}

	free(rdirp);

	/*
	 * Now write anything that needs to be in the file space.
	 */
	if (bootblksize > BPSEC) {
		if (Verbose)
			(void) printf(gettext("Writing remainder of "
				"boot block.\n"));
		if (!Notreally)
			write_rest(wbpb, fn, fd, bsfd, bootblksize - BPSEC);
	}

	if (fffd >= 0) {
		if (Verbose)
			(void) printf(gettext("Writing first file.\n"));
		if (!Notreally)
			write_rest(wbpb, ffn, fd, fffd, ffsize);
	}
}

static
char *LegalOpts[] = {
#define	NFLAG 0
	"N",
#define	VFLAG 1
	"v",
#define	RFLAG 2
	"r",
#define	HFLAG 3
	"h",
#define	SFLAG 4
	"s",
#define	SUNFLAG 5
	"S",
#define	LABFLAG 6
	"b",
#define	BTRFLAG 7
	"B",
#define	INITFLAG 8
	"i",
#define	SZFLAG 9
	"size",
#define	SECTFLAG 10
	"nsect",
#define	TRKFLAG 11
	"ntrack",
#define	SPCFLAG 12
	"spc",
#define	BPFFLAG 13
	"fat",
#define	FFLAG 14
	"f",
#define	DFLAG 15
	"d",
#define	NOFDISKFLAG 16
	"nofdisk",
#define	RESRVFLAG 17
	"reserve",
#define	HIDDENFLAG 18
	"hidden",
	NULL
};

static
void
bad_arg(char *option)
{
	(void) fprintf(stderr,
		gettext("Unrecognized option %s.\n"), option);
	usage();
	exit(2);
}

static
void
missing_arg(char *option)
{
	(void) fprintf(stderr,
		gettext("Option %s requires a value.\n"), option);
	usage();
	exit(3);
}

static
void
parse_suboptions(char *optsstr)
{
	char *value;
	int c;

	while (*optsstr != '\0') {
		switch (c = getsubopt(&optsstr, LegalOpts, &value)) {
		case NFLAG:
			Notreally++;
			break;
		case VFLAG:
			Verbose++;
			break;
		case RFLAG:
			Firstfileattr |= 0x01;
			break;
		case HFLAG:
			Firstfileattr |= 0x02;
			break;
		case SFLAG:
			Firstfileattr |= 0x04;
			break;
		case SUNFLAG:
			SunBPBfields = 1;
			break;
		case LABFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				Label = value;
			}
			break;
		case BTRFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				BootBlkFn = value;
			}
			break;
		case INITFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				FirstFn = value;
			}
			break;
		case SZFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				TotSize = atoi(value);
				GetSize = 0;
			}
			break;
		case SECTFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				SecPerTrk = atoi(value);
				GetSPT = 0;
			}
			break;
		case TRKFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				TrkPerCyl = atoi(value);
				GetTPC = 0;
			}
			break;
		case SPCFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				SecPerClust = atoi(value);
				GetSPC = 0;
			}
			break;
		case BPFFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				BitsPerFAT = atoi(value);
				GetBPF = 0;
			}
			break;
		case NOFDISKFLAG:
			DontUseFdisk = 1;
			break;
		case RESRVFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				Resrvd = atoi(value);
				GetResrvd = 0;
			}
			break;
		case HIDDENFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				RelOffset = atoi(value);
				GetOffset = 0;
			}
			break;
		case FFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				DiskName = value;
				Outputtofile = 1;
			}
			break;
		case DFLAG:
			if (value == NULL) {
				missing_arg(LegalOpts[c]);
			} else {
				Imagesize = atoi(value);
			}
			break;
		default:
			bad_arg(value);
			break;
		}
	}
}

static
void
sanity_check_options(int argc, int optind)
{
	if (GetFsParams) {
		if (argc - optind != 1)
			usage();
		return;
	}

	if (DontUseFdisk && GetOffset) {
		/* Set default relative offset of zero */
		RelOffset = 0;
	}

	if (Outputtofile && (argc - optind)) {
		usage();
	} else if (Outputtofile && !DiskName) {
		usage();
	} else if (!Outputtofile && (argc - optind != 1)) {
		usage();
	} else if (SunBPBfields && !BootBlkFn) {
		(void) fprintf(stderr,
		    gettext("Use of the 'S' option requires that\n"
			    "the 'B=' option also be used.\n\n"));
		usage();
	} else if (Firstfileattr != 0x20 && !FirstFn) {
		(void) fprintf(stderr,
		    gettext("Use of the 'r', 'h', or 's' options requires\n"
			    "that the 'i=' option also be used.\n\n"));
		usage();
	} else if (!GetOffset && !DontUseFdisk) {
		(void) fprintf(stderr,
		    gettext("Use of the 'hidden' option requires that\n"
			    "the 'nofdisk' option also be used.\n\n"));
		usage();
	} else if (DontUseFdisk && GetSize) {
		(void) fprintf(stderr,
		    gettext("Use of the 'nofdisk' option requires that\n"
			    "the 'size=' option also be used.\n\n"));
		usage();
	} else if (!GetBPF && BitsPerFAT != 12 && BitsPerFAT != 16) {
		(void) fprintf(stderr,
		    gettext("Invalid Bits/Fat value.  "
			    "Only 12 and 16 bit FATs are supported.\n"));
		exit(2);
	} else if (!GetSPC && (!(powerofx_le_y(2, 128, SecPerClust) ||
	    (SecPerClust < 1) || (SecPerClust > 128)))) {
		(void) fprintf(stderr,
		    gettext("Invalid Sectors/Cluster value.  Must be a "
			    "power of 2 between 1 and 128.\n"));
		exit(2);
	} else if (!GetResrvd && (Resrvd < 1 || Resrvd > 0xffff)) {
		(void) fprintf(stderr,
		    gettext("Invalid number of reserved sectors.  "
			"Must be at least 1 but\nno larger than 65535."));
		exit(2);
	} else if (Imagesize != 3 && Imagesize != 5) {
		usage();
	}
}

void
main(int argc, char **argv)
{
	struct _bios_param_blk dskparamblk;
	struct _sun_bpb_extensions sunblk;
	char *string;
	int  fd;
	int  c;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "F:Vmo:")) != EOF) {
		switch (c) {
		case 'F':
			string = optarg;
			if (strcmp(string, "pcfs") != 0)
				usage();
			break;
		case 'V':
			{
				char	*opt_text;
				int	opt_count;

				(void) fprintf(stdout,
				    gettext("mkfs -F pcfs "));
				for (opt_count = 1; opt_count < argc;
								opt_count++) {
					opt_text = argv[opt_count];
					if (opt_text)
					    (void) fprintf(stdout, " %s ",
								opt_text);
				}
				(void) fprintf(stdout, "\n");
			}
			break;
		case 'm':
			GetFsParams++;
			break;
		case 'o':
			string = optarg;
			parse_suboptions(string);
			break;
		}
	}

	sanity_check_options(argc, optind);

	if (!Outputtofile)
		DiskName = argv[optind];

	(void) memset(&dskparamblk, 0, sizeof (dskparamblk));
	(void) memset(&sunblk, 0, sizeof (sunblk));

	if (GetFsParams) {
		fd = open_and_examine(DiskName, &dskparamblk, &sunblk);
	} else {
		fd = open_and_seek(DiskName, &dskparamblk, &sunblk);
		if (ask_nicely(DiskName))
			write_fat(fd, BootBlkFn, Label, FirstFn,
				&dskparamblk, &sunblk);
	}
	(void) close(fd);
	exit(0);
}
