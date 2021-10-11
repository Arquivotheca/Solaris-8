/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fdisk.c	1.58	99/07/08 SMI"

/*
 * PROGRAM: fdisk(1M)
 * This program reads the partition table on the specified device and
 * also reads the drive parameters. The user can perform various
 * operations from a supplied menu or from the command line. Diagnostic
 * options are also available.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systeminfo.h>

#include <sys/dktp/fdisk.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>

#define	CLR_SCR "[1;1H[0J"
#define	CLR_LIN "[0K"
#define	HOME "[1;1H[0K[2;1H[0K[3;1H[0K[4;1H[0K[5;1H[0K" \
	"[6;1H[0K[7;1H[0K[8;1H[0K[9;1H[0K[10;1H[0K[1;1H"
#define	Q_LINE "[22;1H[0K[21;1H[0K[20;1H[0K"
#define	W_LINE "[12;1H[0K[11;1H[0K"
#define	E_LINE "[24;1H[0K[23;1H[0K"
#define	M_LINE "[13;1H[0K[14;1H[0K[15;1H[0K[16;1H[0K[17;1H" \
	"[0K[18;1H[0K[19;1H[0K[13;1H"
#define	T_LINE "[1;1H[0K"

/* XXX - should be in fdisk.h, used by sd as well */

/* max CHS values, unencoded */
#define	MAX_SECT	(63)
#define	MAX_CYL		(1022)
#define	MAX_HEAD	(254)

/* max CHS values, as they are encoded into bytes in table */
#define	LBA_MAX_SECT    (MAX_SECT | ((MAX_CYL & 0x300) >> 2))
#define	LBA_MAX_CYL	(MAX_CYL & 0xFF)
#define	LBA_MAX_HEAD    (MAX_HEAD)

/*
 * Support for fdisk(1M) on the SPARC platform
 *	In order to convert little endian values to big endian for SPARC,
 *	byte/short and long values must be swapped.
 *	These swapping macros will be used to access information in the
 *	mboot and ipart structures.
 */

#ifdef sparc
#define	les(val)	((((val)&0xFF)<<8)|(((val)>>8)&0xFF))
#define	lel(val)	(((unsigned)(les((val)&0x0000FFFF))<<16) | \
			    (les((unsigned)((val)&0xffff0000)>>16)))
#else
#define	les(val)	(val)
#define	lel(val)	(val)
#endif

#if defined(_SUNOS_VTOC_16)
#define	VTOC_OFFSET	512
#elif defined(_SUNOS_VTOC_8)
#define	VTOC_OFFSET	0
#else
#error No VTOC format defined.
#endif

char Usage[] = "Usage: fdisk\n"
"[ -A id:act:bhead:bsect:bcyl:ehead:esect:ecyl:rsect:numsect ]\n"
"[ -b masterboot ]\n"
"[ -D id:act:bhead:bsect:bcyl:ehead:esect:ecyl:rsect:numsect ]\n"
"[ -F fdisk_file ] [ -h ] [ -o offset ] [ -P fill_patt ] [ -s size ]\n"
"[ -S geom_file ] [ [ -v ] -W { creat_fdisk_file | - } ]\n"
"[ -w | r | d | n | I | B | g | G | R | t | T ] rdevice";

char Usage1[] = "    Partition options:\n"
"	-A id:act:bhead:bsect:bcyl:ehead:esect:ecyl:rsect:numsect\n"
"		Create a partition with specific attributes:\n"
"		id      = system id number (fdisk.h) for the partition type\n"
"		act     = active partition flag (0 is off and 128 is on)\n"
"		bhead   = beginning head for start of partition\n"
"		bsect   = beginning sector for start of partition\n"
"		bcyl    = beginning cylinder for start of partition\n"
"		ehead   = ending head for end of partition\n"
"		esect   = ending sector for end of partition\n"
"		ecyl    = ending cylinder for end of partition\n"
"		rsect   = sector number from start of disk for\n"
"			  start of partition\n"
"		numsect = partition size in sectors\n"
"	-b master_boot\n"
"		Use master_boot as the master boot file.\n"
"	-B	Create one Solaris partition that uses the entire disk.\n"
"	-D id:act:bhead:bsect:bcyl:ehead:esect:ecyl:rsect:numsect\n"
"		Delete a partition. See attribute definitions for -A.\n"
"	-F fdisk_file\n"
"		Use fdisk_file to initialize on-line fdisk table.\n"
"	-I	Forego device checks. Generate a file image of what would go\n"
"		on a disk using the geometry specified with the -S option.\n"
"	-n	Do not run in interactive mode.\n"
"	-R	Open the disk device as read-only.\n"
"	-t	Check and adjust VTOC to be consistent with fdisk table.\n"
"		VTOC slices exceeding the partition size will be truncated.\n"
"	-T	Check and adjust VTOC to be consistent with fdisk table.\n"
"		VTOC slices exceeding the partition size will be removed.\n"
"	-W fdisk_file\n"
"		Write on-disk table to fdisk_file.\n"
"	-W -	Write on-disk table to standard output.\n"
"	-v	Display virtual geometry. Must be used with the -W option.\n"
"    Diagnostic options:\n"
"	-d	Activate debug information about progress.\n"
"	-g	Write label geometry to standard output:\n"
"		PCYL		number of physical cylinders\n"
"		NCYL		number of usable cylinders\n"
"		ACYL		number of alternate cylinders\n"
"		BCYL		cylinder offset\n"
"		NHEADS		number of heads\n"
"		NSECTORS	number of sectors per track\n"
"		SECTSIZ		size of a sector in bytes\n"
"	-G	Write physical geometry to standard output (see -g).\n"
"	-h	Issue this verbose help message.\n"
"	-o offset\n"
"		Block offset from start of disk (default 0). Ignored if\n"
"		-P # specified.\n"
"	-P fill_patt\n"
"		Fill disk with pattern fill_patt. fill_patt can be decimal or\n"
"		hexadecimal and is used as number for constant long word\n"
"		pattern. If fill_patt is \"#\" then pattern of block #\n"
"		for each block. Pattern is put in each block as long words\n"
"		and fills each block (see -o and -s).\n"
"	-r	Read from a disk to stdout (see -o and -s).\n"
"	-s size	Number of blocks on which to perform operation (see -o).\n"
"	-S geom_file\n"
"		Use geom_file to set the label geometry (see -g).\n"
"	-w	Write to a disk from stdin (see -o and -s).";

char Ostr[] = "Other OS";
char Dstr[] = "DOS12";
char D16str[] = "DOS16";
char DDstr[] = "DOS-DATA";
char EDstr[] = "EXT-DOS";
char DBstr[] = "DOS-BIG";
char PCstr[] = "PCIX";
char Ustr[] = "UNIX System";
char SUstr[] = "Solaris";
char X86str[] = "x86 Boot";
char DIAGstr[] = "Diagnostic";
char IFSstr[] = "IFS: NTFS";
char AIXstr[] = "AIX Boot";
char AIXDstr[] = "AIX Data";
char OS2str[] = "OS/2 Boot";
char WINstr[] = "Win95 FAT32";
char EWINstr[] = "Ext Win95";
char FAT95str[] = "FAT16 LBA";
char EXTLstr[] = "EXT LBA";
char LINUXstr[] = "Linux";
char CPMstr[] = "CP/M";
char NOVstr[] = "Netware 3.x+";
char QNXstr[] = "QNX 4.x";
char QNX2str[] = "QNX part 2";
char QNX3str[] = "QNX part 3";
char LINNATstr[] = "Linux native";
char NTFSVOL1str[] = "NT volset 1";
char NTFSVOL2str[] = "NT volset 2";
char BSDstr[] = "BSD OS";
char NEXTSTEPstr[] = "NeXTSTEP";
char BSDIFSstr[] = "BSDI FS";
char BSDISWAPstr[] = "BSDI swap";
char Actvstr[] = "Active";
char NAstr[] = "      ";

/* All the user options and flags */
char *Dfltdev;			/* name of fixed disk drive */

/* Diagnostic options */
int	io_wrt;			/* write standard input to disk (-w) */
int	io_rd;			/* read from disk and write to stdout (-r) */
char	*io_fatt;		/* user supplied pattern (-P pattern) */
int	io_patt;		/* write a pattern to disk (-P pattern) */
int	io_lgeom;		/* get label geometry (-g) */
int	io_pgeom;		/* get drive physical geometry (-G) */
char	*io_sgeom = 0;		/* set label geometry (-S geom_file) */
int	io_readonly;		/* do not write to disk (-R) */

/* The -o offset and -s size options specify the area of the disk on */
/* which to perform the particular operation; i.e., -P, -r, or -w. */
int	io_offset;		/* offset sector (-o offset) */
int	io_size;		/* size in sectors (-s size) */

/* Partition table flags */
int	v_flag = 0;		/* virtual geometry-HBA flag (-v) */
int 	stdo_flag = 0;		/* stdout flag (-W -) */
int	io_fdisk;		/* do fdisk operation */
int	io_ifdisk;		/* interactive partition */
int	io_nifdisk;		/* non-interactive partition (-n) */

int	io_adjt;		/* check and adjust VTOC (truncate (-t)) */
int	io_ADJT;		/* check and adjust VTOC (delete (-T)) */
char	*io_ffdisk = 0;		/* name of input fdisk file (-F file) */
char	*io_Wfdisk = 0;		/* name of output fdisk file (-W file) */
char	*io_Afdisk = 0;		/* entry to add to partition table (-A) */
char	*io_Dfdisk = 0;		/* entry to delete from partition table (-D) */

char	*io_mboot = 0;		/* master boot record (-b boot_file) */

struct mboot BootCod;		/* buffer for master boot record */

int	io_wholedisk;		/* use whole disk for Solaris partition (-B) */
int	io_debug;		/* activate verbose mode (-d) */
int	io_image;		/* create image using supplied geometry (-I) */

struct mboot *Bootblk;		/* pointer to cut and paste sector zero */
char	*Bootsect;		/* pointer to sector zero buffer */
char	*Nullsect;
struct vtoc	disk_vtoc;	/* verify VTOC table */
int	vt_inval;
int 	no_virtgeom_ioctl = 0;	/* ioctl for virtual geometry failed */
int 	no_physgeom_ioctl = 0;	/* ioctl for physical geometry failed */

struct ipart	Table[FD_NUMPART];
struct ipart	Old_Table[FD_NUMPART];

/* Disk geometry information */
struct dk_geom	disk_geom;

int Dev;			/* fd for open device */
/* Physical geometry for the drive */
int	Numcyl;			/* number of cylinders */
int	heads;			/* number of heads */
int	sectors;		/* number of sectors per track */
int	acyl;			/* number of alternate sectors */

/* HBA (virtual) geometry for the drive */
int	hba_Numcyl;		/* number of cylinders */
int	hba_heads;		/* number of heads */
int	hba_sectors;		/* number of sectors per track */

int	sectsiz;		/* sector size */
int	drtype;			/* Type of drive; i.e., scsi, floppy, ... */

/* Load functions for fdisk table modification */
#define	LOADFILE	0	/* load fdisk from file */
#define	LOADDEL		1	/* delete an fdisk entry */
#define	LOADADD		2	/* add an fdisk entry */

#define	CBUFLEN 80
char s[CBUFLEN];

void	sanity_check_provided_device(char *devname, int fd);
int 	clear_vtoc(int);

/*
 * main
 * Process command-line options.
 */

main(argc, argv)
int argc;
char *argv[];
{
	int c, i, j;
	int unixstart;
	int unixend;
	extern	int optind;
	extern	char *optarg;
	int	errflg = 0;
	int	diag_cnt = 0;
	struct	stat	statbuf;
	int openmode;
	int check_support_fdisk();

	setbuf(stderr, 0);	/* so all output gets out on exit */
	setbuf(stdout, 0);

	/* Process the options. */
	while ((c = getopt(argc, argv, "o:s:P:F:b:A:D:W:S:tTIhwvrndgGRB"))
	    != EOF) {
		switch (c) {

			case 'o':
				io_offset = strtoul(optarg, 0, 0);
				continue;
			case 's':
				io_size = strtoul(optarg, 0, 0);
				continue;
			case 'P':
				diag_cnt++;
				io_patt++;
				io_fatt = optarg;
				continue;
			case 'w':
				diag_cnt++;
				io_wrt++;
				continue;
			case 'r':
				diag_cnt++;
				io_rd++;
				continue;
			case 'd':
				io_debug++;
				continue;
			case 'I':
				io_image++;
				continue;
			case 'R':
				io_readonly++;
				continue;
			case 'S':
				diag_cnt++;
				io_sgeom = optarg;
				continue;
			case 'T':
				io_ADJT++;
			case 't':
				io_adjt++;
				continue;
			case 'B':
				io_wholedisk++;
				io_fdisk++;
				continue;
			case 'g':
				diag_cnt++;
				io_lgeom++;
				continue;
			case 'G':
				diag_cnt++;
				io_pgeom++;
				continue;
			case 'n':
				io_nifdisk++;
				io_fdisk++;
				continue;
			case 'F':
				io_fdisk++;
				io_ffdisk = optarg;
				continue;
			case 'b':
				io_mboot = optarg;
				continue;
			case 'W':
				/*
				 * If '-' is the -W argument, then write
				 * to standard output, otherwise write
				 * to the specified file.
				 */
				if (strncmp(optarg, "-", 1) == 0)
					stdo_flag = 1;
				else
					io_Wfdisk = optarg;
				io_fdisk++;
				continue;
			case 'A':
				io_fdisk++;
				io_Afdisk = optarg;
				continue;
			case 'D':
				io_fdisk++;
				io_Dfdisk = optarg;
				continue;
			case 'h':
				fprintf(stderr, "%s\n", Usage);
				fprintf(stderr, "%s\n", Usage1);
				exit(0);
			case 'v':
				v_flag = 1;
				continue;
			case '?':
				errflg++;
				break;
		}
		break;
	}

	if (io_image && io_sgeom && diag_cnt == 1) {
		diag_cnt = 0;
	}

	/* User option checking */

	/* By default, run in interactive mode */
	if (!io_fdisk && !diag_cnt && !io_nifdisk) {
		io_ifdisk++;
		io_fdisk++;
	}
	if (((io_fdisk || io_adjt) && diag_cnt) || (diag_cnt > 1)) {
		errflg++;
	}

	/* Was any error detected? */
	if (errflg || argc == optind) {
		fprintf(stderr, "%s\n", Usage);
		fprintf(stderr,
		    "\nDetailed help is available with the -h option.\n");
		exit(2);
	}

	/* Make sure the device specified is the raw device */
	if (!io_image) {
		if (stat(argv[optind], (struct stat *)&statbuf) == -1) {
			fprintf(stderr, "fdisk: Cannot stat device %s.\n",
			    argv[optind]);
			exit(1);
		}

		if ((statbuf.st_mode & S_IFMT) != S_IFCHR) {
			fprintf(stderr, "fdisk: %s must be a raw device.\n",
			    argv[optind]);
			exit(1);
		}
	}

	/* Get the name of the device special file and try to open it */
	Dfltdev = argv[optind];
	if (io_readonly)
		openmode = O_RDONLY;
	else
		openmode = O_RDWR|O_CREAT;

	if ((Dev = open(Dfltdev, openmode, 0666)) == -1) {
		fprintf(stderr, "fdisk: Cannot open device %s.\n", Dfltdev);
		exit(1);
	}

	/* Get the disk geometry */
	if (!io_image) {
		/* Get disk's HBA (virtual) geometry */
		errno = 0;
		if (ioctl(Dev, DKIOCG_VIRTGEOM, &disk_geom)) {

			/*
			 * If ioctl isn't implemented on this platform, then
			 * turn off flag to print out virtual geometry (-v),
			 * otherwise use the virtual geometry.
			 */

			if (errno == ENOTTY) {
				v_flag = 0;
				no_virtgeom_ioctl = 1;
			} else if (errno == EINVAL) {
				/*
				 * This means that the ioctl exists, but
				 * is invalid for this disk, meaning the
				 * disk doesn't have an HBA geometry
				 * (like, say, it's larger than 8GB).
				 */
				v_flag = 0;
				hba_Numcyl = hba_heads = hba_sectors = 0;
			} else {
				(void) fprintf(stderr,
				    "%s: Cannot get virtual disk geometry.\n",
				    argv[optind]);
				exit(1);
			}
		} else {
			/* save virtual geometry values obtained by ioctl */
			hba_Numcyl = disk_geom.dkg_ncyl;
			hba_heads = disk_geom.dkg_nhead;
			hba_sectors = disk_geom.dkg_nsect;
		}

		errno = 0;
		if (ioctl(Dev, DKIOCG_PHYGEOM, &disk_geom)) {
			if (errno == ENOTTY) {
				no_physgeom_ioctl = 1;
			} else {
				(void) fprintf(stderr,
				    "%s: Cannot get physical disk geometry.\n",
				    argv[optind]);
				exit(1);
			}

		} else if (no_virtgeom_ioctl) {
			/*
			 * Have physical geometry but not virtual
			 * geometry. Assign physical geometry values to
			 * HBA (virtual) geometry.
			 */

			hba_Numcyl = disk_geom.dkg_ncyl;
			hba_heads = disk_geom.dkg_nhead;
			hba_sectors = disk_geom.dkg_nsect;
		}
		/*
		 * Call DKIOCGGEOM if the ioctls for physical and virtual
		 * geometry fail. Get both from this generic call.
		 */
		if (no_virtgeom_ioctl && no_physgeom_ioctl) {
			errno = 0;
			if (ioctl(Dev, DKIOCGGEOM, &disk_geom)) {
				(void) fprintf(stderr,
				    "%s: Cannot get disk label geometry.\n",
				    argv[optind]);
				exit(1);
			}
			hba_Numcyl = disk_geom.dkg_ncyl;
			hba_heads = disk_geom.dkg_nhead;
			hba_sectors = disk_geom.dkg_nsect;
		}
		Numcyl = disk_geom.dkg_ncyl;
		heads = disk_geom.dkg_nhead;
		sectors = disk_geom.dkg_nsect;
		sectsiz = 512;
		acyl = disk_geom.dkg_acyl;

		if (io_debug) {
			fprintf(stderr, "Physical Geometry:\n");
			fprintf(stderr,
			    "  cylinders[%d] heads[%d] sectors[%d]\n"
			    "  sector size[%d] blocks[%d] mbytes[%d]\n",
			    Numcyl,
			    heads,
			    sectors,
			    sectsiz,
			    Numcyl*heads*sectors,
			    (Numcyl*heads*sectors*sectsiz)/1048576);
			fprintf(stderr, "Virtual (HBA) Geometry:\n");
			fprintf(stderr,
			    "  cylinders[%d] heads[%d] sectors[%d]\n"
			    "  sector size[%d] blocks[%d] mbytes[%d]\n",
			    hba_Numcyl,
			    hba_heads,
			    hba_sectors,
			    sectsiz,
			    hba_Numcyl*hba_heads*hba_sectors,
			    (hba_Numcyl*hba_heads*hba_sectors*sectsiz)/1048576);
		}
	}

	/* If user has requested a geometry report just do it and exit */
	if (io_lgeom) {
		if (ioctl(Dev, DKIOCGGEOM, &disk_geom)) {
			(void) fprintf(stderr,
			    "%s: Cannot get disk label geometry.\n",
			    argv[optind]);
			exit(1);
		}
		Numcyl = disk_geom.dkg_ncyl;
		heads = disk_geom.dkg_nhead;
		sectors = disk_geom.dkg_nsect;
		sectsiz = 512;
		acyl = disk_geom.dkg_acyl;
		printf("* Label geometry for device %s\n", Dfltdev);
		printf("* PCYL     NCYL     ACYL     BCYL     NHEAD NSECT"
		    " SECSIZ\n");
		printf("  %-8d %-8d %-8d %-8d %-5d %-5d %-6d\n",
		    Numcyl,
		    disk_geom.dkg_ncyl,
		    disk_geom.dkg_acyl,
		    disk_geom.dkg_bcyl,
		    heads,
		    sectors,
		    sectsiz);
		exit(0);
	} else if (io_pgeom) {
		if (ioctl(Dev, DKIOCG_PHYGEOM, &disk_geom)) {
			(void) fprintf(stderr,
			    "%s: Cannot get physical disk geometry.\n",
			    argv[optind]);
			exit(1);
		}
		printf("* Physical geometry for device %s\n", Dfltdev);
		printf("* PCYL     NCYL     ACYL     BCYL     NHEAD NSECT"
		    " SECSIZ\n");
		printf("  %-8d %-8d %-8d %-8d %-5d %-5d %-6d\n",
		    disk_geom.dkg_pcyl,
		    disk_geom.dkg_ncyl,
		    disk_geom.dkg_acyl,
		    disk_geom.dkg_bcyl,
		    disk_geom.dkg_nhead,
		    disk_geom.dkg_nsect,
		    sectsiz);
		exit(0);
	} else if (io_sgeom) {
		if (read_geom(io_sgeom)) {
			exit(1);
		} else if (!io_image) {
			exit(0);
		}
	}

	/* Allocate memory to hold three complete sectors */
	Bootsect = (char *)malloc(3 * sectsiz);
	if (Bootsect == NULL) {
		fprintf(stderr,
		    "fdisk: Unable to obtain enough buffer memory"
		    " (%d bytes).\n",
		    3*sectsiz);
		exit(1);
	}

	Nullsect = Bootsect + sectsiz;
	/* Zero out the "NULL" sector */
	for (i = 0; i < sectsiz; i++) {
		Nullsect[i] = 0;
	}

	/* Find out what the user wants done */
	if (io_rd) {		/* abs disk read */
		abs_read();	/* will not return */
	} else if (io_wrt && !io_readonly) {
		abs_write();	/* will not return */
	} else if (io_patt && !io_readonly) {
		fill_patt();	/* will not return */
	}


	/* This is the fdisk edit, the real reason for the program.	*/

	sanity_check_provided_device(Dfltdev, Dev);

	/* Get the new BOOT program in case we write a new fdisk table */
	mboot_read();

	/* Read from disk master boot */
	dev_mboot_read();

	/*
	 * Verify and copy the device's fdisk table. This will be used
	 * as the prototype mboot if the device's mboot looks invalid.
	 */
	Bootblk = (struct mboot *)Bootsect;
	copy_Bootblk_to_Table();

	/* save away a copy of Table in Old_Table for sensing changes */
	copy_Table_to_Old_Table();

	/* Load fdisk table from specified file (-F fdisk_file) */
	if (io_ffdisk) {
		/* Load and verify user-specified table parameters */
		load(LOADFILE, io_ffdisk);
	}

	/* Does user want to delete or add an entry? */
	if (io_Dfdisk) {
		load(LOADDEL, io_Dfdisk);
	}
	if (io_Afdisk) {
		load(LOADADD, io_Afdisk);
	}

	/* Check if there is no fdisk table */
	if (Table[0].systid == UNUSED || io_wholedisk) {
		if (io_ifdisk) {
			printf("No fdisk table exists. The default"
			    " partition for the disk is:\n\n");
			printf("  a 100%% \"SOLARIS System\" partition\n\n");
		}

		/* Ask user if he wants the single Solaris partition */
		if (!io_Afdisk && !io_wholedisk && io_ifdisk) {
			printf("Type \"y\" to accept the default partition,"
			    " otherwise type \"n\" to edit the\n"
			    "partition table.\n");
			gets(s);
			rm_blanks(s);
			while (!(((s[0] == 'y') || (s[0] == 'Y') ||
			    (s[0] == 'n') || (s[0] == 'N')) &&
			    (s[1] == 0))) {
				printf(" Please answer with \"y\" or \"n\": ");
				gets(s);
				rm_blanks(s);
			}
		}

		/* Edit the partition table as directed */
		if (s[0] == 'y' || s[0] == 'Y' || io_wholedisk) {
			/* Default scenario! */
			nulltbl();
			/* now set up UNIX System partition */
			Table[0].bootid = ACTIVE;
			Table[0].begsect = 1;
			unixstart = heads * sectors;
			unixend = Numcyl - 1;
			Table[0].relsect = lel(heads * sectors);
			Table[0].numsect = lel((long)((Numcyl-1) *
			    heads * sectors));
			Table[0].systid = SUNIXOS;   /* Solaris */
			if (hba_heads != 0 && hba_sectors != 0) {
				Table[0].beghead = 0;
				Table[0].begcyl = (char)(unixstart & 0x00ff);
				Table[0].begsect |= (char)((unixstart >> 2) &
				    0x0c0);
				Table[0].endhead = hba_heads - 1;
				Table[0].endsect = (hba_sectors & 0x3f) |
				    (char)((unixend >> 2) & 0x00c0);
				Table[0].endcyl = (char)(unixend & 0x00ff);
			} else {
				/* no HBA geom; mark max in both */
				Table[0].begcyl = Table[0].endcyl =
				    LBA_MAX_CYL;
				Table[0].beghead = Table[0].endhead =
				    LBA_MAX_HEAD;
				Table[0].begsect = Table[0].endsect =
				    LBA_MAX_SECT;
			}

			/* Copy the new table back to the sector buffer and */
			/* write it to disk */
			copy_Table_to_Bootblk();
			dev_mboot_write(0, Bootsect, sectsiz);

			/* If the VTOC table is wrong fix it */
			if (io_adjt)
				fix_slice();
			exit(0);
		}
	}

	/* Display complete fdisk table entries for debugging purposes */
	if (io_debug) {
		fprintf(stderr, "Partition Table Entry Values:\n");
		print_Table();
		if (io_ifdisk) {
			fprintf(stderr, "\n");
			fprintf(stderr, "Press Enter to continue.\n");
			gets(s);
		}
	}

	/* Interactive fdisk mode */
	if (io_ifdisk) {
		printf(CLR_SCR);
		disptbl();
		while (1) {
			stage0(argv[1]);
			copy_Bootblk_to_Table();
			disptbl();
		}
	}

	/* If user wants to write the table to a file, do it */
	if (io_Wfdisk)
		ffile_write(io_Wfdisk);
	else if (stdo_flag)
		ffile_write((char *)stdout);

	if (TableChanged() == 1) {
		copy_Table_to_Bootblk();
		dev_mboot_write(0, Bootsect, sectsiz);
	}

	/* If the VTOC table is wrong fix it (truncation only) */
	if (io_adjt)
		fix_slice();
	exit(0);
}

/*
 * read_geom
 * Read geometry from specified file (-S).
 */

read_geom(sgeom)
char	*sgeom;
{
	char	line[256];
	FILE *fp;

	/* open the prototype file */
	if ((fp = fopen(sgeom, "r")) == NULL) {
		(void) fprintf(stderr, "fdisk: Cannot open file %s.\n",
		    io_sgeom);
		return (1);
	}

	/* Read a line from the file */
	while (fgets(line, sizeof (line) - 1, fp)) {
		if (line[0] == '\0' || line[0] == '\n' || line[0] == '*')
			continue;
		else {
			line[strlen(line)] = '\0';
			if (sscanf(line, "%d %d %d %d %d %d %d",
			    &disk_geom.dkg_pcyl,
			    &disk_geom.dkg_ncyl,
			    &disk_geom.dkg_acyl,
			    &disk_geom.dkg_bcyl,
			    &disk_geom.dkg_nhead,
			    &disk_geom.dkg_nsect,
			    &sectsiz) != 7) {
				(void) fprintf(stderr,
				    "Syntax error:\n	\"%s\".\n",
				    line);
				return (1);
			}
			break;
		} /* else */
	} /* while (fgets(line, sizeof (line) - 1, fp)) { */

	if (!io_image) {
		if (ioctl(Dev, DKIOCSGEOM, &disk_geom)) {
			(void) fprintf(stderr,
			    "fdisk: Cannot set label geometry.\n");
			return (1);
		}
	} else {
		Numcyl = hba_Numcyl = disk_geom.dkg_ncyl;
		heads = hba_heads = disk_geom.dkg_nhead;
		sectors = hba_sectors = disk_geom.dkg_nsect;
		acyl = disk_geom.dkg_acyl;
	}

	fclose(fp);
	return (0);
}

/*
 * dev_mboot_read
 * Read the master boot sector from the device.
 */
dev_mboot_read()
{
	if (lseek(Dev, 0, 0) == -1) {
		fprintf(stderr,
		    "fdisk: Error seeking to partition table on %s.\n",
		    Dfltdev);
		if (!io_image)
			exit(1);
	}
	if (read(Dev, Bootsect, sectsiz) != sectsiz) {
		fprintf(stderr,
		    "fdisk: Error reading partition table from %s.\n",
		    Dfltdev);
		if (!io_image)
			exit(1);
	}
}

/*
 * dev_mboot_write
 * Write the master boot sector to the device.
 */
dev_mboot_write(int sect, char *buff, int bootsiz)
{
	int new_pt, old_pt;

	if (io_readonly)
		return (0);

	if (io_debug) {
		fprintf(stderr, "About to write fdisk table:\n");
		print_Table();
		if (io_ifdisk) {
			fprintf(stderr, "Press Enter to continue.\n");
			gets(s);
		}
	}

	/* look to see if Solaris partition changed in relsect/numsect */
	for (new_pt = 0; new_pt < FD_NUMPART; new_pt++) {
		if (Table[new_pt].systid != SUNIXOS)
			continue;
		for (old_pt = 0; old_pt < FD_NUMPART; old_pt++) {
		    if ((Old_Table[old_pt].systid == Table[new_pt].systid) &&
			(Old_Table[old_pt].relsect == Table[new_pt].relsect) &&
			(Old_Table[old_pt].numsect == Table[new_pt].numsect))
			    break;
		}

		/* if Solaris partition changed, clear the VTOC */
		if (old_pt == FD_NUMPART && Table[new_pt].begcyl != 0)
			clear_vtoc(new_pt);
		break;
	}

	/* write to disk drive */
	if (lseek(Dev, sect, 0) == -1) {
		fprintf(stderr,
		    "fdisk: Error seeking to master boot record on %s.\n",
		    Dfltdev);
		exit(1);
	}
	if (write(Dev, buff, bootsiz) != bootsiz) {
		fprintf(stderr,
		    "fdisk: Error writing master boot record to %s.\n",
		    Dfltdev);
		exit(1);
	}
}

/*
 * mboot_read
 * Read the prototype boot records from the files.
 */
mboot_read()
{
	int mDev, i;
	struct	stat	statbuf;
	struct ipart *part;

#if defined(i386) || defined(sparc)
	/*
	 * If the master boot file hasn't been specified, use the
	 * implementation architecture name to generate the default one.
	 */
	if (io_mboot == (char *)0) {
		/*
		 * Bug ID 1249035:
		 *	The mboot file must be delivered on all platforms
		 *	and installed in a non-platform-dependent
		 *	directory; i.e., /usr/lib/fs/ufs.
		 */
		io_mboot = "/usr/lib/fs/ufs/mboot";
	}

	/* First read in the master boot record */

	/* Open the master boot proto file */
	if ((mDev = open(io_mboot, O_RDONLY, 0666)) == -1) {
		fprintf(stderr,
		    "fdisk: Cannot open master boot file %s.\n",
		    io_mboot);
		exit(1);
	}

	/* Read the master boot program */
	if (read(mDev, &BootCod, sizeof (struct mboot)) != sizeof
	    (struct mboot)) {
		fprintf(stderr,
		    "fdisk: Cannot read master boot file %s.\n",
		    io_mboot);
		exit(1);
	}

	/* Is this really a master boot record? */
	if (les(BootCod.signature) != MBB_MAGIC) {
		fprintf(stderr,
		    "fdisk: Invalid master boot file %s.\n", io_mboot);
		fprintf(stderr, "Bad magic number: is %x, but should be %x.\n",
		    les(BootCod.signature), MBB_MAGIC);
		exit(1);
	}

	close(mDev);
#else
#error	fdisk needs to be ported to new architecture
#endif

	/* Zero out the partitions part of this record */
	part = (struct ipart *)BootCod.parts;
	for (i = 0; i < FD_NUMPART; i++, part++) {
		memset(part, 0, sizeof (struct ipart));
	}

}

/*
 * fill_patt
 * Fill the disk with user/sector number pattern.
 */
fill_patt()
{
	int	*buff_ptr, i, c;
	int	io_fpatt = 0;
	int	io_ipatt = 0;

	if (strncmp(io_fatt, "#", 1) != 0) {
		io_fpatt++;
		io_ipatt = strtoul(io_fatt, 0, 0);
		buff_ptr = (int *)Bootsect;
		for (i = 0; i < sectsiz; i += 4, buff_ptr++)
		    *buff_ptr = io_ipatt;
	}

	/*
	 * Fill disk with pattern based on block number.
	 * Write to the disk at absolute relative block io_offset
	 * for io_size blocks.
	 */
	while (io_size--) {
		buff_ptr = (int *)Bootsect;
		if (!io_fpatt) {
			for (i = 0; i < sectsiz; i += 4, buff_ptr++)
				*buff_ptr = io_offset;
		}
		/* Write the data to disk */
		if (lseek(Dev, sectsiz * io_offset++, 0) == -1) {
			fprintf(stderr, "fdisk: Error seeking on %s.\n",
				Dfltdev);
			exit(1);
		}
		if (write(Dev, Bootsect, sectsiz) != sectsiz) {
			fprintf(stderr, "fdisk: Error writing %s.\n",
				Dfltdev);
			exit(1);
		}
	} /* while (--io_size); */
}

/*
 * abs_read
 * Read from the disk at absolute relative block io_offset for
 * io_size blocks. Write the data to standard ouput (-r).
 */
abs_read() {
	int c;

	while (io_size--) {
		if (lseek(Dev, sectsiz * io_offset++, 0) == -1) {
			fprintf(stderr, "fdisk: Error seeking on %s.\n",
			    Dfltdev);
			exit(1);
		}
		if (read(Dev, Bootsect, sectsiz) != sectsiz) {
			fprintf(stderr, "fdisk: Error reading %s.\n",
			    Dfltdev);
			exit(1);
		}

		/* Write to standard ouptut */
		if ((c = write(1, Bootsect, (unsigned)sectsiz)) != sectsiz)
		{
			if (c >= 0) {
				if (io_debug)
				fprintf(stderr,
				    "fdisk: Output warning: %d of %d"
				    " characters written.\n",
				    c, sectsiz);
				exit(2);
			} else {
				perror("write error on output file.");
				exit(2);
			}
		} /* if ((c = write(1, Bootsect, (unsigned)sectsiz)) */
			/* != sectsiz) */
	} /* while (--io_size); */
	exit(0);
}

/*
 * abs_write
 * Read the data from standard input. Write to the disk at
 * absolute relative block io_offset for io_size blocks (-w).
 */
abs_write()
{
	int c, i;

	while (io_size--) {
		int part_exit = 0;
		/* Read from standard input */
		if ((c = read(0, Bootsect, (unsigned)sectsiz)) != sectsiz) {
			if (c >= 0) {
				if (io_debug)
				fprintf(stderr,
				    "fdisk: WARNING: Incomplete read (%d of"
				    " %d characters read) on input file.\n",
				    c, sectsiz);
				/* Fill pattern to mark partial sector in buf */
				for (i = c; i < sectsiz; ) {
					Bootsect[i++] = 0x41;
					Bootsect[i++] = 0x62;
					Bootsect[i++] = 0x65;
					Bootsect[i++] = 0;
				}
				part_exit++;
			} else {
				perror("read error on input file.");
				exit(2);
			}

		}
		/* Write to disk drive */
		if (lseek(Dev, sectsiz * io_offset++, 0) == -1) {
			fprintf(stderr, "fdisk: Error seeking on %s.\n",
			    Dfltdev);
			exit(1);
		}
		if (write(Dev, Bootsect, sectsiz) != sectsiz) {
			fprintf(stderr, "fdisk: Error writing %s.\n",
			    Dfltdev);
			exit(1);
		}
		if (part_exit)
		exit(0);
	} /* while (--io_size); */
	exit(1);
}

/*
 * load
 * Load will either read the fdisk table from a file or add or
 * delete an entry (-A, -D, -F).
 */

load(funct, file)
int	funct;
char	*file;			/* Either file name or delete/add line */
{
	int	id;
	int	act;
	int	bhead;
	int	bsect;
	int	bcyl;
	int	ehead;
	int	esect;
	int	ecyl;
	int	rsect;
	int	numsect;
	char	line[256];
	int	i = 0;
	int	j;
	FILE *fp;

	switch (funct) {

	    case LOADFILE:

		/*
		 * Zero out the table before loading it, which will
		 * force it to be updated on disk later (-F
		 * fdisk_file).
		 */
		nulltbl();

		/* Open the prototype file */
		if ((fp = fopen(file, "r")) == NULL) {
			(void) fprintf(stderr,
			    "fdisk: Cannot open prototype partition file %s.\n",
			    file);
			exit(1);
		}

		/* Read a line from the file */
		while (fgets(line, sizeof (line) - 1, fp)) {
			if (pars_fdisk(line, &id, &act, &bhead, &bsect,
			    &bcyl, &ehead, &esect, &ecyl, &rsect, &numsect)) {
				continue;
			}

			/*
			 * Find an unused entry to use and put the entry
			 * in table
			 */
			if (insert_tbl(id, act, bhead, bsect, bcyl, ehead,
			    esect, ecyl, rsect, numsect) < 0) {
				(void) fprintf(stderr,
				    "fdisk: Error on entry \"%s\".\n",
				    line);
				exit(1);
			}
		} /* while (fgets(line, sizeof (line) - 1, fp)) { */

		if (verify_tbl()) {
			fprintf(stderr,
			    "fdisk: Cannot create partition because it"
			    " overlaps an existing partition\n"
			    "or is too big.\n");
			exit(1);
		}

		fclose(fp);
		return;

	    case LOADDEL:

		/* Parse the user-supplied deletion line (-D) */
		pars_fdisk(file, &id, &act, &bhead, &bsect, &bcyl, &ehead,
		    &esect, &ecyl, &rsect, &numsect);

		/* Find the exact entry in the table */
		for (i = 0; i < FD_NUMPART; i++) {
			if (Table[i].systid == id &&
			    Table[i].bootid == act &&
			    Table[i].beghead == bhead &&
			    Table[i].begsect == ((bsect & 0x3f) |
				(unsigned char)((bcyl>>2) & 0xc0)) &&
			    Table[i].begcyl == (unsigned char)(bcyl & 0xff) &&
			    Table[i].endhead == ehead &&
			    Table[i].endsect == ((esect & 0x3f) |
				(unsigned char)((ecyl>>2) & 0xc0)) &&
			    Table[i].endcyl == (unsigned char)(ecyl & 0xff) &&
			    Table[i].relsect == lel(rsect) &&
			    Table[i].numsect == lel(numsect)) {

				/*
				 * Found the entry. Now move rest of
				 * entries up toward the top of the
				 * table, leaving available entries at
				 * the end of the fdisk table.
				 */
				for (j = i; j < FD_NUMPART-1; j++) {
					Table[j].systid = Table[j+1].systid;
					Table[j].bootid = Table[j+1].bootid;
					Table[j].beghead = Table[j+1].beghead;
					Table[j].begsect = Table[j+1].begsect;
					Table[j].begcyl = Table[j+1].begcyl;
					Table[j].endhead = Table[j+1].endhead;
					Table[j].endsect = Table[j+1].endsect;
					Table[j].endcyl = Table[j+1].endcyl;
					Table[j].relsect = Table[j+1].relsect;
					Table[j].numsect = Table[j+1].numsect;
				}

				/*
				 * Mark the last entry as unused in case
				 * all table entries were in use prior
				 * to the deletion.
				 */

				Table[FD_NUMPART-1].systid = UNUSED;
				Table[FD_NUMPART-1].bootid = 0;
				return;
			}
		}
		fprintf(stderr,
		    "fdisk: Entry does not match any existing partition:\n"
		    "	\"%s\"\n",
		    file);
		exit(1);

	    case LOADADD:

		/* Parse the user-supplied addition line (-A) */
		pars_fdisk(file, &id, &act, &bhead, &bsect, &bcyl, &ehead,
		    &esect, &ecyl, &rsect, &numsect);

		/* Find unused entry for use and put entry in table */
		if (insert_tbl(id, act, bhead, bsect, bcyl, ehead, esect,
		    ecyl, rsect, numsect) < 0) {
			(void) fprintf(stderr,
			    "fdisk: Invalid entry could not be inserted:\n"
			    "	\"%s\"\n",
			    file);
			exit(1);
		}

		/* Make sure new entry does not overlap existing entry */
		if (verify_tbl()) {
			fprintf(stderr,
			    "fdisk: Cannot create partition, \"%s\",\n"
			    " because it overlaps an existing partition,\n"
			    "or is too big.\n", file);
			exit(1);
		}
	} /* switch funct */
}

/*
 * insert_tbl
 * 	Insert entry into fdisk table. Check all user-supplied values
 *	for the entry, but not the validity relative to other table
 *	entries!
 */
insert_tbl(id, act, bhead, bsect, bcyl, ehead, esect, ecyl, rsect, numsect)
int	id, act, bhead, bsect, bcyl, ehead, esect, ecyl, rsect, numsect;
{
	int i, k;

	/* Check for a full table */
	if (Table[3].systid != UNUSED) {
		fprintf(stderr, "fdisk: Partition table is full.\n");
		return (-1);
	}

	/*
	 * Do error checking of all those values.
	 * Note that a user-supplied value of zero means that the fdisk
	 * program should generate the value using a standard formula.
	 */

	/*
	 * This is a hack that will not check zero but check value
	 * against the drive size.
	 */

	if (rsect+numsect > (Numcyl * heads * sectors)) {
		fprintf(stderr,
		    "fdisk: Partition table exceeds the size of the disk.\n");
		return (-1);
	}

	/* If we have a drive with no HBA geometry, set BCHS/ECHS to MAX */

	if (hba_sectors == 0 || hba_heads == 0) {
		if (bcyl != MAX_CYL || ecyl != MAX_CYL ||
		    bhead != MAX_HEAD || ehead != MAX_HEAD ||
		    bsect != MAX_SECT || esect != MAX_SECT) {
			fprintf(stderr, "No HBA geometry for this drive; "
			    "overriding CHS values with maximum values.\n");
		}

		for (i = 0; i < FD_NUMPART; i++) {
			if (Table[i].systid != UNUSED)
				continue;
			Table[i].systid = id;
			Table[i].bootid = act;
			Table[i].begcyl = Table[i].endcyl = LBA_MAX_CYL;
			Table[i].beghead = Table[i].endhead = LBA_MAX_HEAD;
			Table[i].begsect = Table[i].endsect = LBA_MAX_SECT;
			Table[i].numsect = lel(numsect);
			Table[i].relsect = lel(rsect);
			return (i);
		}
		return (-1);
	}

	/* Have a real HBA geometry.  Do tedious validation. */

	if (bcyl) {
		if (bcyl != (rsect/(hba_sectors*hba_heads) & 0x3ff)) {
			fprintf(stderr,
			    "fdisk: Invalid beginning cylinder number.\n");
			return (-1);
		}
	} else
		bcyl = rsect/(hba_sectors*hba_heads);

	if (bhead) {
		if (bhead != ((rsect-(bcyl*hba_heads*hba_sectors)) /
		    hba_sectors)) {
			fprintf(stderr,
			    "fdisk: Invalid beginning head number.\n");
			return (-1);
		}
	} else
		bhead = ((rsect-(bcyl*hba_heads*hba_sectors))/hba_sectors);

	if (bsect) {
		if (bsect != (((rsect%hba_sectors)+1) & 0x3f)) {
			fprintf(stderr,
			    "fdisk: Invalid beginning sector number.\n");
			return (-1);
		}
	} else
		bsect = ((rsect%hba_sectors)+1);

	if (ecyl) {
		if (ecyl != ((((rsect+numsect)-1)/(hba_sectors*hba_heads)) &
		    0x3ff)) {
			fprintf(stderr,
			    "fdisk: Invalid ending cylinder number.\n");
			return (-1);
		}
	} else
		ecyl = ((rsect+numsect)-1)/(hba_sectors*hba_heads);

	if (ehead) {
		if (ehead != (((rsect+numsect)-1) -
		    (ecyl*hba_heads*hba_sectors))/hba_sectors) {
			fprintf(stderr,
			    "fdisk: Invalid ending head number.\n");
			return (-1);
		}
	} else
		ehead = ((((rsect+numsect)-1) -
		    (ecyl*hba_heads*hba_sectors))/hba_sectors);

	if (esect) {
		if (esect != (((((rsect+numsect)-1)%hba_sectors)+1) & 0x3f)) {
			fprintf(stderr,
			    "fdisk: Invalid ending sector number.\n");
			return (-1);
		}
	} else
		esect = (((rsect+numsect)-1)%hba_sectors)+1;

	/* Put our validated entry in the table */
	for (i = 0; i < FD_NUMPART; i++) {
		if (Table[i].systid == UNUSED) {
			Table[i].systid = id;
			Table[i].begsect = ((bsect & 0x3f) |
			    (unsigned char)((bcyl>>2) & 0xc0));
			Table[i].endsect = ((esect & 0x3f) |
			    (unsigned char)((ecyl>>2) & 0xc0));
			Table[i].begcyl = (unsigned char)(bcyl & 0xff);
			Table[i].endcyl = (unsigned char)(ecyl & 0xff);
			Table[i].beghead = bhead;
			Table[i].endhead = ehead;
			Table[i].numsect = lel(numsect);
			Table[i].relsect = lel(rsect);
			Table[i].bootid = act;
			return (i);
		}
		else
			continue;
	}
	return (-1);
}

/*
 * verify_tbl
 * Verify that no partition entries overlap or exceed the size of
 * the disk.
 */
verify_tbl()
{
	int i, j, rsect, numsect;

	/* Make sure new entry does not overlap an existing entry */
	for (i = 0; i < FD_NUMPART-1; i++) {
		if (Table[i].systid != UNUSED) {
			rsect = lel(Table[i].relsect);
			numsect = lel(Table[i].numsect);
			if ((rsect + numsect) > (Numcyl * heads * sectors)) {
				return (-1);
			}
			for (j = i+1; j < FD_NUMPART; j++) {
				if (Table[j].systid != UNUSED) {
					int t_relsect = lel(Table[j].relsect);
					int t_numsect = lel(Table[j].numsect);
					if ((rsect >=
					    (t_relsect + t_numsect)) ||
					    ((rsect+numsect) <= t_relsect)) {
						continue;
					} else {
						return (-1);
					}
				}
			}
		}
	}
	if (Table[i].systid != UNUSED) {
		if ((lel(Table[i].relsect) + lel(Table[i].numsect)) >
		    (Numcyl * heads * sectors)) {
			return (-1);
		}
	}
	return (0);
}

/*
 * pars_fdisk
 * Parse user-supplied data to set up fdisk partitions
 * (-A, -D, -F).
 */
pars_fdisk(line, id, act, bhead, bsect, bcyl, ehead, esect, ecyl,
	rsect, numsect)
char *line;
char *id, *act, *bhead, *bsect, *bcyl, *ehead, *esect, *ecyl, *rsect;
char *numsect;
{
	int	i;
	if (line[0] == '\0' || line[0] == '\n' || line[0] == '*')
	    return (1);
	line[strlen(line)] = '\0';
	for (i = 0; i < strlen(line); i++) {
		if (line[i] == '\0') {
			break;
		} else if (line[i] == ':') {
			line[i] = ' ';
		}
	}
	if (sscanf(line, "%d %d %d %d %d %d %d %d %ld %ld",
	    id, act, bhead, bsect, bcyl, ehead, esect, ecyl,
	    rsect, numsect) != 10) {
		(void) fprintf(stderr, "Syntax error:\n	\"%s\".\n", line);
		exit(1);
	}
	return (0);
}

/*
 * stage0
 * Print out interactive menu and process user input.
 */
stage0(file)
char *file;
{
	dispmenu(file);
	while (1) {
		printf(Q_LINE);
		printf("Enter Selection: ");
		gets(s);
		rm_blanks(s);
		while (!((s[0] > '0') && (s[0] < '6') && (s[1] == 0))) {
			printf(E_LINE); /* Clear any previous error */
			printf("Enter a one-digit number between 1 and 5.");
			printf(Q_LINE);
			printf("Enter Selection: ");
			gets(s);
			rm_blanks(s);
		}
		printf(E_LINE);
		switch (s[0]) {
			case '1':
				if (pcreate() == -1)
					return;
				break;
			case '2':
				if (pchange() == -1)
					return;
				break;
			case '3':
				if (pdelete() == -1)
					return;
				break;
			case '4':
				/* update disk partition table, if changed */
				if (TableChanged() == 1) {
					copy_Table_to_Bootblk();
					dev_mboot_write(0, Bootsect, sectsiz);
				}
				/*
				 * If the VTOC table is wrong fix it
				 * (truncate only)
				 */
				if (io_adjt)
					fix_slice();
				close(Dev);
				exit(0);
			case '5':
				/*
				 * If the VTOC table is wrong fix it
				 * (truncate only)
				 */
				if (io_adjt)
					fix_slice();
				close(Dev);
				exit(0);
			default:
				break;
		}
		copy_Table_to_Bootblk();
		disptbl();
		dispmenu(file);
	}
}

/*
 * pcreate
 * Create partition entry in the table (interactive mode).
 */
pcreate()
{
	unsigned char tsystid = 'z';
	int i, j;
	int startcyl, endcyl;

	i = 0;
	while (1) {
		if (i == FD_NUMPART) {
			printf(E_LINE);
			printf("The partition table is full!\n");
			printf("You must delete a partition before creating"
			    " a new one.\n");
			return (-1);
		}
		if (Table[i].systid == UNUSED) {
			break;
		}
		i++;
	}

	j = 0;
	for (i = 0; i < FD_NUMPART; i++) {
		if (Table[i].systid != UNUSED) {
			j += lel(Table[i].numsect);
		}
		if (j >= Numcyl * heads * sectors) {
			printf(E_LINE);
			printf("There is no more room on the disk for"
			    " another partition.\n");
			printf("You must delete a partition before creating"
			    " a new one.\n");
			return (-1);
		}
	}
	while (tsystid == 'z') {
		printf(Q_LINE);
		printf("Select the partition type to create:\n");
		printf("   1=SOLARIS   2=UNIX        3=PCIXOS     4=Other\n");
		printf("   5=DOS12     6=DOS16       7=DOSEXT     8=DOSBIG\n");
		printf("   A=x86 Boot  B=Diagnostic  0=Exit? ");
		gets(s);
		rm_blanks(s);
		if (s[1] != 0) {
			printf(E_LINE);
			printf("Invalid selection, try again.");
			continue;
		}
		switch (s[0]) {
		case '0':		/* exit */
		    printf(E_LINE);
		    return (-1);
		case '1':		/* Solaris partition */
		    tsystid = SUNIXOS;
		    break;
		case '2':		/* UNIX partition */
		    tsystid = UNIXOS;
		    break;
		case '3':		/* PCIXOS partition */
		    tsystid = PCIXOS;
		    break;
		case '4':		/* OTHEROS System partition */
		    tsystid = OTHEROS;
		    break;
		case '5':
		    tsystid = DOSOS12; /* DOS 12 bit fat */
		    break;
		case '6':
		    tsystid = DOSOS16; /* DOS 16 bit fat */
		    break;
		case '7':
		    tsystid = EXTDOS;
		    break;
		case '8':
		    tsystid = DOSHUGE;
		    break;
		case 'a':		/* x86 Boot partition */
		case 'A':
		    tsystid = X86BOOT;
		    break;
		case 'b':		/* Diagnostic boot partition */
		case 'B':
		    tsystid = DIAGPART;
		    break;
		default:
		    printf(E_LINE);
		    printf("Invalid selection, try again.");
		    continue;
		}
	}
	printf(E_LINE);
	i = specify(tsystid);
	if (i == -1)
		return (-1);
	printf(E_LINE);
	printf(Q_LINE);

	printf("Should this become the active partition? If yes, it"
	    " will be activated\n");
	printf("each time the computer is reset or turned on.\n");
	printf("Please type \"y\" or \"n\". ");
	gets(s);
	rm_blanks(s);
	while ((s[1] != 0) && ((s[0] != 'y') && (s[0] != 'Y') &&
	    (s[0] != 'n') && (s[0] != 'N'))) {
	    printf(E_LINE);
	    printf(" Please answer with \"y\" or \"n\": ");
	    gets(s);
	    rm_blanks(s);
	}
	printf(E_LINE);
	if (s[0] == 'y' || s[0] == 'Y') {
	    for (j = 0; j < FD_NUMPART; j++)
		if (j == i) {
		    Table[j].bootid = ACTIVE;
		    printf(E_LINE);
		    printf("Partition %d is now the active partition.", j+1);
		}
		else
		    Table[j].bootid = 0;
	}
	else
	    Table[i].bootid = 0;
	return (1);
}

/*
 * specify
 * Query the user to specify the size of the new partition in
 * terms of percentage of the disk or by specifying the starting
 * cylinder and length in cylinders.
 */
specify(tsystid)
unsigned char tsystid;
{
	int	i, j,
		percent = -1;
	int	cyl, cylen, first_free, size_free;

	printf(Q_LINE);
	printf("Specify the percentage of disk to use for this partition\n");
	printf("(or type \"c\" to specify the size in cylinders). ");
	gets(s);
	rm_blanks(s);
	if (s[0] != 'c') {	/* Specify size in percentage of disk */
	    i = 0;
	    while (s[i] != '\0') {
		if (s[i] < '0' || s[i] > '9') {
		    printf(E_LINE);
		    printf("Invalid percentage value specified; retry"
			" the operation.");
		    return (-1);
		}
		i++;
		if (i > 3) {
		    printf(E_LINE);
		    printf("Invalid percentage value specified; retry"
			" the operation.");
		    return (-1);
		}
	    }
	    if ((percent = atoi(s)) > 100) {
		printf(E_LINE);
		printf("Percentage value is too large. The value must be"
		    " between 1 and 100;\nretry the operation.\n");
		return (-1);
	    }
	    if (percent < 1) {
		printf(E_LINE);
		printf("Percentage value is too small. The value must be"
		    " between 1 and 100;\nretry the operation.\n");
		return (-1);
	    }

	    cylen = (Numcyl * percent) / 100;
	    if ((percent < 100) && (((Numcyl * percent) % 10) > 5))
		cylen++;

	    /* Verify that the DOS12 partition does not exceed the maximum */
	    /* size of 32MB. */
	    if ((tsystid == DOSOS12) && ((long)((long)cylen*heads*sectors) >
		MAXDOS)) {
		int n;
		n = (int)(MAXDOS*100/(int)(heads*sectors)/Numcyl);
		printf(E_LINE);
		printf("Maximum size for a DOS partition is %d%%;"
		    " retry the operation.",
		    n <= 100 ? n : 100);
		return (-1);
	    }

	    for (i = 0; i < FD_NUMPART; i++) {
		    int last_ent = 0;

		    /* Find start of current check area */
		    if (i) { /* Not an empty table */
			    first_free = lel(Table[i-1].relsect) +
				lel(Table[i-1].numsect);
		    } else {
			    first_free = heads * sectors;
		    }

		    /* Determine size of current check area */
		    if (Table[i].systid == UNUSED) {
			    /* Special case hack for whole unused disk */
			    if (percent == 100 && i == 0)
				cylen--;
			    size_free = (Numcyl*heads*sectors) - first_free;
			    last_ent++;
		    } else {
			    if (i && ((lel(Table[i-1].relsect) +
				lel(Table[i-1].numsect)) !=
				lel(Table[i].relsect))) {
				    /* There is a hole in table */
				    size_free = lel(Table[i].relsect) -
					(lel(Table[i-1].relsect) +
					lel(Table[i-1].numsect));
			    } else if (i == 0) {
				    size_free = lel(Table[i].relsect) -
					heads*sectors;
			    } else {
				    size_free = 0;
			    }
		    }

		    if ((cylen*heads*sectors) <= size_free) {
			    /* We found a place to use */
			    break;
		    } else if (last_ent) {
			    size_free = 0;
			    break;
		    }
	    }
	    if (i < FD_NUMPART && size_free) {
		    printf(E_LINE);
		    if ((i = insert_tbl(tsystid, 0, 0, 0, 0, 0, 0, 0,
			first_free, cylen*heads*sectors)) < 0)  {
			    fprintf(stderr,
				"fdisk: Partition entry too big.\n");
			    return (-1);
		    }
	    } else {
		    printf(E_LINE);
		    fprintf(stderr, "fdisk: Partition entry too big.\n");
		    i = -1;
	    }
	    return (i);
	} else {	/* Specifying size in cylinders */

	    printf(E_LINE);
	    printf(Q_LINE);
	    printf("Enter starting cylinder number: ");
	    if ((cyl = getcyl()) == -1) {
		printf(E_LINE);
		printf("Invalid number; retry the operation.");
		return (-1);
	    }
	    if (cyl >= (unsigned int)Numcyl) {
		printf(E_LINE);
		printf("Cylinder %d is out of bounds, the maximum is %d.\n",
		    cyl, Numcyl - 1);
		return (-1);
	}
	    printf(Q_LINE);
	    printf("Enter partition size in cylinders: ");
	    if ((cylen = getcyl()) == -1) {
		printf(E_LINE);
		printf("Invalid number, retry the operation.");
		return (-1);
	    }

	    /* Verify that the DOS12 partition does not exceed the maximum */
	    /* size of 32MB. */
	    if ((tsystid == DOSOS12) &&
		((long)((long)cylen*heads*sectors) > MAXDOS)) {
		printf(E_LINE);
		printf("Maximum size for a %s partition is %ld cylinders;"
		    "\nretry the operation.",
		    Dstr, MAXDOS/(int)(heads*sectors));
		return (-1);
	    }

	    i = insert_tbl(tsystid, 0, 0, 0, 0, 0, 0, 0, cyl*heads*sectors,
		cylen*heads*sectors);

	    if (verify_tbl()) {
		printf(E_LINE);
		printf("fdisk: Cannot create partition because it"
		    " overlaps an existing partition\n"
		    "or is too big.\n");
		return (-1);
	    }

	    return (i);
	}
}

/*
 * dispmenu
 * Display command menu (interactive mode).
 */
dispmenu(file)
char *file;
{
	printf(M_LINE);
	printf("SELECT ONE OF THE FOLLOWING:\n\n");
	printf("   1. Create a partition\n");
	printf("   2. Specify the active partition\n");
	printf("   3. Delete a partition\n");
	printf("   4. Exit (update disk configuration and exit)\n");
	printf("   5. Cancel (exit without updating disk configuration)\n");
}

/*
 * pchange
 * Change the ACTIVE designation of a partition.
 */
pchange()
{
	char s[80];
	int i, j;

	while (1) {
		printf(Q_LINE);
			{
			printf("Specify the partition number to boot from"
			    " (or specify 0 for none): ");
			}
		gets(s);
		rm_blanks(s);
		if ((s[1] != 0) || (s[0] < '0') || (s[0] > '4')) {
			printf(E_LINE);
			printf("Invalid response, please specify a number"
			    " between 0 and 4.\n");
		} else {
			break;
		}
	}
	if (s[0] == '0') {	/* No active partitions */
		for (i = 0; i < FD_NUMPART; i++) {
			if (Table[i].systid != UNUSED &&
			    Table[i].bootid == ACTIVE)
				Table[i].bootid = 0;
		}
		printf(E_LINE);
			printf("No partition is currently marked as active.");
		return (0);
	} else {	/* User has selected a partition to be active */
		i = s[0] - '1';
		if (Table[i].systid == UNUSED) {
			printf(E_LINE);
			printf("Partition does not exist.");
			return (-1);
		}
		/* a DOS-DATA or EXT-DOS partition cannot be active */
		else if ((Table[i].systid == DOSDATA) ||
		    (Table[i].systid == EXTDOS)) {
			printf(E_LINE);
			printf("DOS-DATA or EXT_DOS partitions cannot be"
			    " made active.\n");
			printf("Select another partition.");
			return (-1);
		}
		Table[i].bootid = ACTIVE;
		for (j = 0; j < FD_NUMPART; j++) {
			if (j != i)
			Table[j].bootid = 0;
		}
	}
	printf(E_LINE);
		{
		printf("Partition %d is now active. The system will start up"
		    " from this\n", i+1);
		printf("partition after the next reboot.");
		}
	return (1);
}

/*
 * pdelete
 * Remove partition entry from the table (interactive mode).
 */
pdelete()
{
	char s[80];
	int i, j;
	char pactive;

DEL1:	printf(Q_LINE);
	printf("Specify the partition number to delete"
	    " (or enter 0 to exit): ");
	gets(s);
	rm_blanks(s);
	if ((s[0] == '0')) {	/* exit delete command */
		printf(E_LINE);	/* clear error message */
		return (1);
	}
	/* Accept only a single digit between 1 and 4 */
	if (s[1] != 0 || (i = atoi(s)) < 1 || i > FD_NUMPART) {
		printf(E_LINE);
		printf("Invalid response, retry the operation.\n");
		goto DEL1;
	} else {		/* Found a digit between 1 and 4 */
		--i;	/* Structure begins with element 0 */
	}
	if (Table[i].systid == UNUSED) {
		printf(E_LINE);
		printf("Partition %d does not exist.", i+1);
		return (-1);
	}
	while (1) {
		printf(Q_LINE);
		printf("Are you sure you want to delete partition %d?"
		    " This will make all files and \n", i+1);
		printf("programs in this partition inaccessible (type"
		    " \"y\" or \"n\"). ");
		gets(s);
		rm_blanks(s);
		if ((s[1] != 0) || ((s[0] != 'y') && (s[0] != 'n'))) {
			printf(E_LINE);
			printf("Please answer with \"y\" or \"n\": ");
		} else break;
	}
	printf(E_LINE);
	if (s[0] != 'y' && s[0] != 'Y')
		return (1);
	if (Table[i].bootid != 0)
		pactive = 1;
	else
		pactive = 0;
	for (j = i; j < 3; j++) {
	    if (Table[j+1].systid == UNUSED) {
		Table[j].systid = UNUSED;
		break;
	    }
	    Table[j] = Table[j+1];
	}
	Table[j].systid = UNUSED;
	Table[j].numsect = 0;
	Table[j].relsect = 0;
	Table[j].bootid = 0;
	printf(E_LINE);
	printf("Partition %d has been deleted.", i+1);
	if (pactive)
	    printf(" This was the active partition.");
	return (1);
}

/*
 * rm_blanks
 * Remove blanks from strings of user responses.
 */
rm_blanks(s)
char *s;
{
	register int i, j;

	for (i = 0; i < CBUFLEN; i++) {
		if ((s[i] == ' ') || (s[i] == '\t'))
			continue;
		else
			/* Found first non-blank character of the string */
			break;
	}
	for (j = 0; i < CBUFLEN; j++, i++) {
		if ((s[j] = s[i]) == '\0') {
			/* Reached end of string */
			return;
		}
	}
}

/*
 * getcyl
 * Take the user-specified cylinder number and convert it from a
 * string to a decimal value.
 */
getcyl()
{
int slen, i, j;
unsigned int cyl;
	gets(s);
	rm_blanks(s);
	slen = strlen(s);
	j = 1;
	cyl = 0;
	for (i = slen-1; i >= 0; i--) {
		if (s[i] < '0' || s[i] > '9') {
			return (-1);
		}
		cyl += (j*(s[i]-'0'));
		j *= 10;
	}
	return (cyl);
}

/*
 * disptbl
 * Display the current fdisk table; determine percentage
 * of the disk used for each partition.
 */
disptbl()
{
	int i;
	unsigned int startcyl, endcyl, length, percent, remainder;
	char *stat, *type;
	unsigned char *t;

	printf(HOME);
	printf(T_LINE);
	printf("             Total disk size is %d cylinders\n", Numcyl);
	printf("             Cylinder size is %d (512 byte) blocks\n\n",
	    heads*sectors);
	printf("                                               Cylinders\n");
	printf("      Partition   Status    Type          Start   End   Length"
	    "    %%\n");
	printf("      =========   ======    ============  =====   ===   ======"
	    "   ===");
	for (i = 0; i < FD_NUMPART; i++) {
		if (Table[i].systid == UNUSED) {
			printf("\n");
			printf(CLR_LIN);
			continue;
		}
		if (Table[i].bootid == ACTIVE)
		    stat = Actvstr;
		else
		    stat = NAstr;
		switch (Table[i].systid) {
		case UNIXOS:
		    type = Ustr;
		    break;
		case SUNIXOS:
		    type = SUstr;
		    break;
		case X86BOOT:
		    type = X86str;
		    break;
		case DOSOS12:
		    type = Dstr;
		    break;
		case DOSOS16:
		    type = D16str;
		    break;
		case EXTDOS:
		    type = EDstr;
		    break;
		case DOSDATA:
		    type = DDstr;
		    break;
		case DOSHUGE:
		    type = DBstr;
		    break;
		case PCIXOS:
		    type = PCstr;
		    break;
		case DIAGPART:
		    type = DIAGstr;
		    break;
		case FDISK_IFS:
		    type = IFSstr;
		    break;
		case FDISK_AIXBOOT:
		    type = AIXstr;
		    break;
		case FDISK_AIXDATA:
		    type = AIXDstr;
		    break;
		case FDISK_OS2BOOT:
		    type = OS2str;
		    break;
		case FDISK_WINDOWS:
		    type = WINstr;
		    break;
		case FDISK_EXT_WIN:
		    type = EWINstr;
		    break;
		case FDISK_FAT95:
		    type = FAT95str;
		    break;
		case FDISK_EXTLBA:
		    type = EXTLstr;
		    break;
		case FDISK_LINUX:
		    type = LINUXstr;
		    break;
		case FDISK_CPM:
		    type = CPMstr;
		    break;
		case FDISK_NOVELL3:
		    type = NOVstr;
		    break;
		case FDISK_QNX4:
		    type = QNXstr;
		    break;
		case FDISK_QNX42:
		    type = QNX2str;
		    break;
		case FDISK_QNX43:
		    type = QNX3str;
		    break;
		case FDISK_LINUXNAT:
		    type = LINNATstr;
		    break;
		case FDISK_NTFSVOL1:
		    type = NTFSVOL1str;
		    break;
		case FDISK_NTFSVOL2:
		    type = NTFSVOL2str;
		    break;
		case FDISK_BSD:
		    type = BSDstr;
		    break;
		case FDISK_NEXTSTEP:
		    type = NEXTSTEPstr;
		    break;
		case FDISK_BSDIFS:
		    type = BSDIFSstr;
		    break;
		case FDISK_BSDISWAP:
		    type = BSDISWAPstr;
		    break;
		default:
		    type = Ostr;
		    break;
		}
		t = &Table[i].bootid;
		startcyl = lel(Table[i].relsect)/(heads*sectors);
		length = lel(Table[i].numsect) / (long)(heads * sectors);
		if (lel(Table[i].numsect) % (long)(heads * sectors))
			length++;
		endcyl = startcyl + length - 1;
		percent = length * 100 / Numcyl;
		if ((remainder = (length*100 % Numcyl)) != 0) {
			if ((remainder * 100 / Numcyl) > 50) {
				/* round up */
				percent++;
			}
			/* Else leave the percent as is since it's already */
			/* rounded down */
		}
		if (percent > 100)
			percent = 100;
		printf("\n          %d       %s    %-12.12s   %4d  %4d    %4d"
		    "    %3d", i+1, stat, type, startcyl, endcyl, length,
		    percent);
	}
	/* Print warning message if table is empty */
	if (Table[0].systid == UNUSED) {
		printf(W_LINE);
		printf("WARNING: no partitions are defined!");
	} else {
		/* Clear the warning line */
		printf(W_LINE);
	}
}

/*
 * print_Table
 * Write the detailed fdisk table to standard error for
 * the selected disk device.
 */
print_Table() {
	int i;

	fprintf(stderr,
	    "  SYSID ACT BHEAD BSECT BEGCYL   EHEAD ESECT ENDCYL   RELSECT"
	    "   NUMSECT\n");

	for (i = 0; i < FD_NUMPART; i++) {
		fprintf(stderr, "  %-5d ", Table[i].systid);
		fprintf(stderr, "%-3d ", Table[i].bootid);
		fprintf(stderr, "%-5d ", Table[i].beghead);
		fprintf(stderr, "%-5d ", Table[i].begsect);
/*		fprintf(stderr, "%-8d ", ((Table[i].begsect & 0xc0)<<2) + */
/*			Table[i].begcyl); */
		fprintf(stderr, "%-8d ", Table[i].begcyl);

		fprintf(stderr, "%-5d ", Table[i].endhead);
		fprintf(stderr, "%-5d ", Table[i].endsect);
/*		fprintf(stderr, "%-8d ", ((Table[i].endsect & 0xc0)<<2) + */
/*			Table[i].endcyl); */
		fprintf(stderr, "%-8d ", Table[i].endcyl);
		fprintf(stderr, "%-9d ", lel(Table[i].relsect));
		fprintf(stderr, "%-9d\n", lel(Table[i].numsect));

	}
}

/*
 * copy_Table_to_Old_Table
 * Copy Table into Old_Table. The function only copies the systid,
 * numsect, relsect, and bootid values because they are the only
 * ones compared when determining if Table has changed.
 */
copy_Table_to_Old_Table()
{
	int i;
	for (i = 0; i < FD_NUMPART; i++)  {
	    Old_Table[i].systid = Table[i].systid;
	    Old_Table[i].numsect = Table[i].numsect;
	    Old_Table[i].relsect = Table[i].relsect;
	    Old_Table[i].bootid = Table[i].bootid;
	}
}

/*
 * nulltbl
 * Zero out the systid, numsect, relsect, and bootid values in the
 * fdisk table.
 */
nulltbl()
{
	int i;

	for (i = 0; i < FD_NUMPART; i++)  {
	    Table[i].systid = UNUSED;
	    Table[i].numsect = lel(UNUSED);
	    Table[i].relsect = lel(UNUSED);
	    Table[i].bootid = 0;
	}
}

/*
 * copy_Bootblk_to_Table
 * Copy the bytes from the boot record to an internal "Table".
 * All unused are padded with zeros starting at offset 446.
 */
copy_Bootblk_to_Table()
{
	int i, j;
	char *bootptr;
	unsigned char *tbl_ptr;
	int tblpos;
	void fill_ipart(char *, struct ipart *);
	struct ipart iparts[FD_NUMPART];

	/* Get an aligned copy of the partition tables */
	memcpy(iparts, Bootblk->parts, sizeof (iparts));
	tbl_ptr = &Table[0].bootid;
	bootptr = (char *)iparts;	/* Points to start of partition table */
	if (les(Bootblk->signature) != MBB_MAGIC)  {
		/* Signature is missing */
		nulltbl();
		memcpy(Bootblk->bootinst, &BootCod, BOOTSZ);
		return;
	}
	/*
	 * When the DOS fdisk command deletes a partition, it is not
	 * recognized by the old algorithm.  The algorithm that
	 * follows looks at each entry in the Bootrec and copies all
	 * those that are valid.
	 */
	j = 0;
	for (i = 0; i < FD_NUMPART; i++) {
		if (iparts[i].systid == 0) {
			/* Null entry */
			bootptr += sizeof (struct ipart);
		} else {
			(void) fill_ipart(bootptr, &Table[j]);
			j++;
			bootptr += sizeof (struct ipart);
		}
	}
	for (i = j; i < FD_NUMPART; i++) {
		Table[i].systid = UNUSED;
		Table[i].numsect = lel(UNUSED);
		Table[i].relsect = lel(UNUSED);
		Table[i].bootid = 0;

	}
	/* For now, always replace the bootcode with ours */
	memcpy(Bootblk->bootinst, &BootCod, BOOTSZ);
	copy_Table_to_Bootblk();
}

/*
 * fill_ipart
 * Initialize ipart structure values.
 */
void
fill_ipart(char *bootptr, struct ipart *partp)
{
#ifdef sparc
	/* Packing struct ipart for Sparc */
	partp->bootid = getbyte(&bootptr);
	partp->beghead = getbyte(&bootptr);
	partp->begsect = getbyte(&bootptr);
	partp->begcyl = getbyte(&bootptr);
	partp->systid = getbyte(&bootptr);
	partp->endhead = getbyte(&bootptr);
	partp->endsect = getbyte(&bootptr);
	partp->endcyl = getbyte(&bootptr);
	partp->relsect = getlong(&bootptr);
	partp->numsect = getlong(&bootptr);
#else
	*partp = *(struct ipart *)bootptr;
#endif
}

/*
 * getbyte, getshort, getlong
 * 	Get a byte, a short, or a long (SPARC only).
 */
#ifdef sparc
getbyte(uchar_t **bp)
{
	int	b;

	b = **bp;
	*bp = *bp + 1;
	return (b);
}

getshort(uchar_t **bp)
{
	int	b;

	b = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	return (b);
}

getlong(uchar_t **bp)
{
	int	b, bh, bl;

	bh = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	bl = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;

	b = (bh << 16) | bl;
	return (b);
}
#endif

/*
 * copy_Table_to_Bootblk
 * Copy the table into the 512 boot record. Note that the unused
 * entries will always be the last ones in the table and they are
 * marked with 100 in sysind. The the unused portion of the table
 * is padded with zeros in the bytes after the used entries.
 */
copy_Table_to_Bootblk()
{
	struct ipart *boot_ptr, *tbl_ptr;

	boot_ptr = (struct ipart *)Bootblk->parts;
	tbl_ptr = (struct ipart *)&Table[0].bootid;
	for (; tbl_ptr < (struct ipart *)&Table[FD_NUMPART].bootid;
	    tbl_ptr++, boot_ptr++) {
	    if (tbl_ptr->systid == UNUSED)
		memset(boot_ptr, 0, sizeof (struct ipart));
	    else
		memcpy(boot_ptr, tbl_ptr, sizeof (struct ipart));
	}
	Bootblk->signature = les(MBB_MAGIC);
}

/*
 * TableChanged
 * 	Check for any changes in the partition table.
 */
TableChanged()
{
	int i, changed;

	changed = 0;
	for (i = 0; i < FD_NUMPART; i++) {
	    if (memcmp(&Old_Table[i], &Table[i], sizeof (Table[0])) != 0) {
		/* Partition table changed, write back to disk */
		changed = 1;
	    }
	}

	return (changed);
}

/*
 * ffile_write
 * 	Display contents of partition table to standard output or
 *	another file name without writing it to the disk (-W file).
 */
ffile_write(file)
char	*file;
{
	register int	i;
	register int	c;
	FILE *fp;

	/*
	 * If file isn't standard output, then it's a file name.
	 * Open file and write it.
	 */
	if (file != (char *)stdout) {
	    if ((fp = fopen(file, "w")) == NULL) {
		(void) fprintf(stderr, "fdisk: Cannot open output file %s.\n",
		    file);
		exit(1);
	    }
	}
	else
	    fp = stdout;

	/*
	 * Write the fdisk table information
	 */
	fprintf(fp, "\n* %s default fdisk table\n", Dfltdev);
	fprintf(fp, "* Dimensions:\n");
	fprintf(fp, "*   %4d bytes/sector\n", sectsiz);
	fprintf(fp, "*   %4d sectors/track\n", sectors);
	fprintf(fp, "*   %4d tracks/cylinder\n", heads);
	fprintf(fp, "*   %4d cylinders\n", Numcyl);
	fprintf(fp, "*\n");
	/* Write virtual (HBA) geometry, if required	*/
	if (v_flag) {
		fprintf(fp, "* HBA Dimensions:\n");
		fprintf(fp, "*   %4d bytes/sector\n", sectsiz);
		fprintf(fp, "*   %4d sectors/track\n", hba_sectors);
		fprintf(fp, "*   %4d tracks/cylinder\n", hba_heads);
		fprintf(fp, "*   %4d cylinders\n", hba_Numcyl);
		fprintf(fp, "*\n");
	}
	fprintf(fp, "* systid:\n");
	fprintf(fp, "*    1: DOSOS12\n");
	fprintf(fp, "*    2: PCIXOS\n");
	fprintf(fp, "*    4: DOSOS16\n");
	fprintf(fp, "*    5: EXTDOS\n");
	fprintf(fp, "*    6: DOSBIG\n");
	fprintf(fp, "*    7: FDISK_IFS\n");
	fprintf(fp, "*    8: FDISK_AIXBOOT\n");
	fprintf(fp, "*    9: FDISK_AIXDATA\n");
	fprintf(fp, "*   10: FDISK_0S2BOOT\n");
	fprintf(fp, "*   11: FDISK_WINDOWS\n");
	fprintf(fp, "*   12: FDISK_EXT_WIN\n");
	fprintf(fp, "*   14: FDISK_FAT95\n");
	fprintf(fp, "*   15: FDISK_EXTLBA\n");
	fprintf(fp, "*   18: DIAGPART\n");
	fprintf(fp, "*   65: FDISK_LINUX\n");
	fprintf(fp, "*   82: FDISK_CPM\n");
	fprintf(fp, "*   86: DOSDATA\n");
	fprintf(fp, "*   98: OTHEROS\n");
	fprintf(fp, "*   99: UNIXOS\n");
	fprintf(fp, "*  101: FDISK_NOVELL3\n");
	fprintf(fp, "*  119: FDISK_QNX4\n");
	fprintf(fp, "*  120: FDISK_QNX42\n");
	fprintf(fp, "*  121: FDISK_QNX43\n");
	fprintf(fp, "*  130: SUNIXOS\n");
	fprintf(fp, "*  131: FDISK_LINUXNAT\n");
	fprintf(fp, "*  134: FDISK_NTFSVOL1\n");
	fprintf(fp, "*  135: FDISK_NTFSVOL2\n");
	fprintf(fp, "*  165: FDISK_BSD\n");
	fprintf(fp, "*  167: FDISK_NEXTSTEP\n");
	fprintf(fp, "*  183: FDISK_BSDIFS\n");
	fprintf(fp, "*  184: FDISK_BSDISWAP\n");
	fprintf(fp, "*  190: X86BOOT\n");
	fprintf(fp, "*\n");
	fprintf(fp,
	    "\n* Id    Act  Bhead  Bsect  Bcyl    Ehead  Esect  Ecyl"
	    "    Rsect    Numsect\n");
	for (i = 0; i < FD_NUMPART; i++) {
		if (Table[i].systid != UNUSED)
			fprintf(fp,
			    "  %-5d %-4d %-6d %-6d %-7d %-6d %-6d %-7d %-8d"
			    " %-8d\n",
			    Table[i].systid,
			    Table[i].bootid,
			    Table[i].beghead,
			    Table[i].begsect & 0x3f,
			    ((Table[i].begcyl & 0xff) | ((Table[i].begsect &
				0xc0) << 2)),
			    Table[i].endhead,
			    Table[i].endsect & 0x3f,
			    ((Table[i].endcyl & 0xff) | ((Table[i].endsect &
				0xc0) << 2)),
			    lel(Table[i].relsect),
			    lel(Table[i].numsect));
	}
	if (fp != stdout)
		fclose(fp);
}

/*
 * fix_slice
 * 	Read the VTOC table on the Solaris partition and check that no
 *	slices exist that extend past the end of the Solaris partition.
 *	If no Solaris partition exists, nothing is done.
 */
fix_slice()
{
	int	i;
	int	ret;
	int	numsect;
	if (io_image)
		return (0);
	for (i = 0; i < FD_NUMPART; i++)
		if (Table[i].systid == SUNIXOS) {
			/*
			 * Only the size matters (not starting point), since
			 * VTOC entries are relative to the start of
			 * the partition.
			 */
			numsect = lel(Table[i].numsect);
			break;
		}
	if (i >= FD_NUMPART) {
		if (!io_nifdisk)
			(void) fprintf(stderr,
			    "fdisk: No Solaris partition found - VTOC not"
			    " checked.\n");
			return (1);
	}
	if ((ret = readvtoc()) == 2)
			exit(1);		/* Failed to read the VTOC */
	else if (ret != 1) {
		for (i = 0; i < V_NUMPAR; i++) {
			/* Special case for slice two (entire disk) */
			if (i == 2) {
				if (disk_vtoc.v_part[i].p_start != 0) {
					(void) fprintf(stderr,
					    "slice %d starts at %d, is not at"
					    " start of partition",
					    i, disk_vtoc.v_part[i].p_start);
					if (!io_nifdisk) {
					    printf(" adjust ?:");
					    if (yesno())
						disk_vtoc.v_part[i].p_start = 0;
					} else {
					    disk_vtoc.v_part[i].p_start = 0;
					    (void) fprintf(stderr,
						" adjusted!\n");
					}

				}
				if (disk_vtoc.v_part[i].p_size != numsect) {
					(void) fprintf(stderr,
					    "slice %d size %d does not cover"
					    " complete partition",
					    i, disk_vtoc.v_part[i].p_size);
					if (!io_nifdisk) {
					    printf(" adjust ?:");
					    if (yesno())
						disk_vtoc.v_part[i].p_size =
						    numsect;
					} else {
					    disk_vtoc.v_part[i].p_size =
						numsect;
					    (void) fprintf(stderr,
						" adjusted!\n");
					}
				}
				if (disk_vtoc.v_part[i].p_tag != V_BACKUP) {
				    (void) fprintf(stderr,
					"slice %d tag was %d should be %d",
					i, disk_vtoc.v_part[i].p_tag,
					V_BACKUP);
				    if (!io_nifdisk) {
					printf(" fix ?:");
					    if (yesno())
						disk_vtoc.v_part[i].p_tag =
						    V_BACKUP;
				    } else {
					disk_vtoc.v_part[i].p_tag = V_BACKUP;
					(void) fprintf(stderr, " fixed!\n");
					}
				}
			} else {
				if (io_ADJT) {
				    if (disk_vtoc.v_part[i].p_start > numsect ||
					disk_vtoc.v_part[i].p_start +
					disk_vtoc.v_part[i].p_size > numsect) {
					    (void) fprintf(stderr,
						"slice %d (start %d, end %d) is"
						" larger than the partition",
						i, disk_vtoc.v_part[i].p_start,
						disk_vtoc.v_part[i].p_start +
						disk_vtoc.v_part[i].p_size);
					    if (!io_nifdisk) {
						printf(" remove ?:");
						if (yesno()) {
						    disk_vtoc.v_part[i].p_size =
							0;
						    disk_vtoc.v_part[i].p_start
							= 0;
						    disk_vtoc.v_part[i].p_tag =
							0;
						    disk_vtoc.v_part[i].p_flag =
							0;
						}
					    } else {
						disk_vtoc.v_part[i].p_size = 0;
						disk_vtoc.v_part[i].p_start = 0;
						disk_vtoc.v_part[i].p_tag = 0;
						disk_vtoc.v_part[i].p_flag = 0;
						(void) fprintf(stderr,
						    " removed!\n");
						}
					}
				} else {
				    if (disk_vtoc.v_part[i].p_start >
					numsect) {
					(void) fprintf(stderr,
					    "slice %d (start %d) is larger"
					    " than the partition",
					    i, disk_vtoc.v_part[i].p_start);
					if (!io_nifdisk) {
					    printf(" remove ?:");
					    if (yesno()) {
						disk_vtoc.v_part[i].p_size = 0;
						disk_vtoc.v_part[i].p_start =
						    0;
						disk_vtoc.v_part[i].p_tag = 0;
						disk_vtoc.v_part[i].p_flag = 0;
					    }
					} else {
					    disk_vtoc.v_part[i].p_size = 0;
					    disk_vtoc.v_part[i].p_start = 0;
					    disk_vtoc.v_part[i].p_tag = 0;
					    disk_vtoc.v_part[i].p_flag = 0;
					    (void) fprintf(stderr,
						" removed!\n");
					    }
					} else if (disk_vtoc.v_part[i].p_start
					    + disk_vtoc.v_part[i].p_size >
					    numsect) {
					    (void) fprintf(stderr,
						"slice %d (end %d) is larger"
						" than the partition",
						i,
						disk_vtoc.v_part[i].p_start +
						disk_vtoc.v_part[i].p_size);
					    if (!io_nifdisk) {
						printf(" adjust ?:");
						if (yesno()) {
						    disk_vtoc.v_part[i].p_size
							= numsect;
						}
					    } else {
						disk_vtoc.v_part[i].p_size =
						    numsect;
						(void) fprintf(stderr,
						    " adjusted!\n");
						}
					}
				}
			}
		}
	}
#if 1		/* bh for now */
	/* Make the VTOC look sane - ha ha */
	disk_vtoc.v_version = V_VERSION;
	disk_vtoc.v_sanity = VTOC_SANE;
	disk_vtoc.v_nparts = V_NUMPAR;
	if (disk_vtoc.v_sectorsz == 0)
		disk_vtoc.v_sectorsz = NBPSCTR;
#endif

	/* Write the VTOC back to the disk */
	if (!io_readonly)
		writevtoc();
}

/*
 * yesno
 * Get yes or no answer. Return 1 for yes and 0 for no.
 */

yesno()
{
	char	s[80];

	for (;;) {
		gets(s);
		rm_blanks(s);
		if ((s[1] != 0) || ((s[0] != 'y') && (s[0] != 'n'))) {
			printf(E_LINE);
			printf("Please answer with \"y\" or \"n\": ");
			continue;
		}
		if (s[0] == 'y')
			return (1);
		else
			return (0);
	}
}

/*
 * readvtoc
 * 	Read the VTOC from the Solaris partition of the device.
 */
readvtoc()
{
	int i;
	if ((i = read_vtoc(Dev, &disk_vtoc)) < 0) {
		if (i == VT_EINVAL) {
			(void) fprintf(stderr, "fdisk: Invalid VTOC.\n");
			vt_inval++;
			return (1);
		} else {
			(void) fprintf(stderr, "fdisk: Cannot read VTOC.\n");
			return (2);
		}
	}
	return (0);
}

/*
 * writevtoc
 * 	Write the VTOC to the Solaris partition on the device.
 */
writevtoc()
{
	int	i;
	if ((i = write_vtoc(Dev, &disk_vtoc)) != 0) {
		if (i == VT_EINVAL) {
			(void) fprintf(stderr,
			    "fdisk: Invalid entry exists in VTOC.\n");
		} else {
			(void) fprintf(stderr, "fdisk: Cannot write VTOC.\n");
		}
		return (1);
	}
	return (0);
}

/*
 * clear_vtoc
 * 	Clear the VTOC from the Solaris partition on the device.
 */
int
clear_vtoc(int new_pt)
{
	struct dk_label disk_label;
	int pcyl, ncyl, backup_block, solaris_offset, count;

	memset(&disk_label, 0, sizeof (struct dk_label));

	if (lseek(Dev, (lel(Table[new_pt].relsect) * sectsiz) +
	    VTOC_OFFSET, 0) == -1) {
		fprintf(stderr, "fdisk: Error seeking to label on %s.\n",
		    Dfltdev);
		return (0);
	}

	write(Dev, &disk_label, sizeof (struct dk_label));

	/* Clear backup label */

	pcyl = lel(Table[new_pt].numsect) / (heads * sectors);
	solaris_offset = lel(Table[new_pt].relsect);
	ncyl = pcyl - acyl;

	backup_block = ((ncyl + acyl - 1) *
	    (heads * sectors)) + ((heads - 1) * sectors) + 1;

	for (count = 1; count < 6; count++) {
		if (lseek(Dev, (solaris_offset + backup_block) * 512, 0)
		    == -1) {
			fprintf(stderr,
			    "fdisk: Error seeking to backup label on %s.\n",
			    Dfltdev);
			return (0);
		}

		write(Dev, &disk_label, sizeof (struct dk_label));

		backup_block += 2;
	}

	return (0);
}

void
get_yn_answer(void)
{
	gets(s);
	rm_blanks(s);
	while (!(((s[0] == 'y') || (s[0] == 'Y') ||
	    (s[0] == 'n') || (s[0] == 'N')) && (s[1] == 0))) {
		printf(" Please answer with \"y\" or \"n\": ");
		gets(s);
		rm_blanks(s);
	}
}

#define	FDISK_STANDARD_LECTURE \
	"Fdisk is normally used with the device that " \
	"represents the entire fixed disk.\n" \
	"(For example, /dev/rdsk/c0d0p0 on x86 or " \
	"/dev/rdsk/c0t5d0s2 on sparc).\n"

#define	FDISK_LECTURE_NOT_SECTOR_ZERO \
	"The device does not appear to include absolute\n" \
	"sector 0 of the PHYSICAL disk " \
	"(the normal location for an fdisk table).\n"

#define	FDISK_LECTURE_NOT_FULL \
	"The device does not appear to encompass the entire PHYSICAL disk.\n"

#define	FDISK_LECTURE_NO_VTOC \
	"Unable to find a volume table of contents.\n" \
	"Cannot verify the device encompasses the full PHYSICAL disk.\n"

#define	FDISK_LECTURE_NO_GEOM \
	"Unable to get geometry from device.\n" \
	"Cannot verify the device encompasses the full PHYSICAL disk.\n"

#define	FDISK_SHALL_I_CONTINUE \
	"Are you sure you want to continue? (y/n) "

/*
 *  lecture_and_query
 *	Called when a sanity check fails.  This routine gives a warning
 *	specific to the check that fails, followed by a generic lecture
 *	about the "right" device to supply as input.  Then, if appropriate,
 *	it will prompt the user on whether or not they want to continue.
 *	Inappropriate times for prompting are when the user has selected
 *	non-interactive mode or read-only mode.
 */
int
lecture_and_query(char *warning, char *devname)
{
	if (io_nifdisk)
		return (0);

	fprintf(stderr, "WARNING: Device %s: \n", devname);
	fprintf(stderr, "%s", warning);
	fprintf(stderr, FDISK_STANDARD_LECTURE);
	fprintf(stderr, FDISK_SHALL_I_CONTINUE);
	get_yn_answer();
	return (s[0] == 'n' || s[0] == 'N');
}

void
sanity_check_provided_device(char *devname, int fd)
{
	struct vtoc v;
	struct dk_geom d;
	struct part_info pi;
	long totsize;
	char *p;
	int idx = -1;

	/*
	 *  First try the PARTINFO ioctl.  If it works, we will be able
	 *  to tell if they've specified the full disk partition by checking
	 *  to see if they've specified a partition that starts at sector 0.
	 */
	if (ioctl(fd, DKIOCPARTINFO, &pi) != -1) {
		if (pi.p_start != 0) {
			if (lecture_and_query(FDISK_LECTURE_NOT_SECTOR_ZERO,
			    devname)) {
				(void) close(fd);
				exit(1);
			}
		}
	} else {
		if ((idx = read_vtoc(fd, &v)) < 0) {
			if (lecture_and_query(FDISK_LECTURE_NO_VTOC, devname)) {
				(void) close(fd);
				exit(1);
			}
			return;
		}
		if (ioctl(fd, DKIOCGGEOM, &d) == -1) {
			perror(devname);
			if (lecture_and_query(FDISK_LECTURE_NO_GEOM, devname)) {
				(void) close(fd);
				exit(1);
			}
			return;
		}
		totsize = d.dkg_ncyl * d.dkg_nhead * d.dkg_nsect;
		if (v.v_part[idx].p_size != totsize) {
			if (lecture_and_query(FDISK_LECTURE_NOT_FULL,
			    devname)) {
				(void) close(fd);
				exit(1);
			}
		}
	}
}
