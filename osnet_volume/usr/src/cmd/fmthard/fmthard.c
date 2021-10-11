/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 *	Portions of this source code were provided by International
 *	Computers Limited (ICL) under a development agreement with AT&T.
 */

#pragma ident	"@(#)fmthard.c	1.10	97/11/22 SMI"

/*
 *	Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

/*
 * Sun Microsystems version of fmthard:
 *
 * Supports the following arguments:
 *
 *	-i		Writes VTOC to stdout, rather than disk
 *	-q		Quick check: exit code 0 if VTOC ok
 *	-d <data>	Incremental changes to the VTOC
 *	-n <vname>	Change volume name to <vname>
 *	-s <file>	Read VTOC information from <file>, or stdin ("-")
 *	-u <state>	Reboot after writing VTOC, according to <state>:
 *				boot: AD_BOOT (standard reboot)
 *				firm: AD_IBOOT (interactive reboot)
 *
 * Features supported on x86:
 *
 *	-S		run in silent mode
 *	-I <file>	Use file for geometry and create inage in file not dev
 * 	-p <file>	Use file for partition boot
 *	-b <file>	Use file for priboot "bootblk"
 *
 * Note that fmthard cannot write a VTOC on an unlabeled disk.
 * You must use format or SunInstall for this purpose.
 * (NOTE: the above restriction only applies on Sparc systems).
 *
 * The primary motivation for fmthard is to duplicate the
 * partitioning from disk to disk:
 *
 *	prtvtoc /dev/rdsk/c0t0d0s2 | fmthard -s - /dev/rdsk/c0t1d0s2
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/open.h>
#include <sys/uadmin.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/isa_defs.h>

#if defined(_SUNOS_VTOC_16)
#include <sys/dklabel.h>
#endif

#include <macros.h>

#ifndef	SECSIZE
#define	SECSIZE			DEV_BSIZE
#endif	SECSIZE


/*
 * Fixed Partitions
 */
#define	P_BACKUP	2	/* whole disk partition */

/*
 * External functions.
 */
extern	int	read_vtoc(int, struct vtoc *);
extern	int	write_vtoc(int, struct vtoc *);

/*
 * Externals
 */
extern	char	*optarg;
extern	int	optind;
extern	int	errno;
extern	char	*sys_errlist[];

/*
 * Internal functions.
 */
extern	int	main(int, char **);
static	void	display(struct dk_geom *, struct vtoc *, char *);
static	void	insert(char *, struct vtoc *);
static	void	load(FILE *, struct dk_geom *, struct vtoc *);
static	void	usage(void);
static	void	validate(struct dk_geom *, struct vtoc *);
static	void	vread(int, struct vtoc *, char *);
static	void	vwrite(int, struct vtoc *, char *);

/*
 * Static variables.
 */
static char	*delta;		/* Incremental update */
static short	iflag;		/* Prints VTOC w/o updating */
static short	qflag;		/* Check for a formatted disk */
static short	uflag;		/* Exit to firmware after writing  */
				/* new vtoc and reboot. Used during */
				/* installation of core floppies */

#if defined(sparc)
static char	*uboot = "boot";

#elif defined(i386)
int	vt_inval;		/* there was no vtoc on disk */
static short	usilent;	/* be quite about work being done */
/*
 * XXX	This stuff is transitional - if we need to make it work on other
 *	platforms, then we should make the code do 'uname -i' to determine
 *	the appropriate platform name.  See fdisk.c for details.
 *
 *	(The 'real' way to install boot blocks is to use 'installboot')
 */
static char *uboot = "/usr/platform/i86pc/lib/fs/ufs/pboot";
				/* partition boot */
static char *pboot = "/usr/platform/i86pc/lib/fs/ufs/bootblk";
				/* bootblk */

#else
#error No platform defined.
#endif	/* various platform-specific definitions */

static char	*ufirm = "firm";
#if defined(_SUNOS_VTOC_16)
	char 		*io_sgeom;
	int		sectsiz;
	int		partoff;
	int		vtocsiz;
	struct	dk_label	dklabel;
	struct vtoc	disk_vtoc;
	struct dk_geom	disk_geom;
#endif	/* defined(_SUNOS_VTOC_16) */

int
main(int argc, char **argv)
{
	int		fd;
	int		c;
	char		*dfile;
	char		*vname;
	struct stat	statbuf;
#if defined(_SUNOS_VTOC_8)
	struct vtoc	disk_vtoc;
	struct dk_geom	disk_geom;
#endif	/* defined(_SUNOS_VTOC_8) */
	int		n;


	dfile = NULL;
	vname = NULL;
#if defined(sparc)
	while ((c = getopt(argc, argv, "d:u:in:qs:")) != EOF)

#elif defined(i386)
	while ((c = getopt(argc, argv, "d:u:iSn:qb:p:s:I:")) != EOF)

#else
#error No platform defined.
#endif
		switch (c) {
#if defined(i386)
		case 'p':
			uboot = optarg;
			break;
		case 'b':
			pboot = optarg;
			break;
#endif	/* defined(i386) */

#if defined(i386)
		case 'S':
			usilent++;
			break;
		case 'I':
			io_sgeom = optarg;
			break;
#endif	/* defined(i386) */

		case 'd':
			delta = optarg;
			break;
		case 'i':
			++iflag;
			break;
		case 'n':
			vname = optarg;
			break;
		case 'q':
			++qflag;
			break;
		case 's':
			dfile = optarg;
			break;
		case 'u':
			if (strcmp(uboot, optarg) == 0)
				++uflag;
			else if (strcmp(ufirm, optarg) == 0)
				uflag = 2;

			break;
		default:
			usage();
		}


	if (argc - optind != 1)
		usage();

#if defined(i386)
	if (io_sgeom == 0) {
		if (stat(argv[optind], (struct stat *)&statbuf) == -1) {
			(void) fprintf(stderr,
				"fmthard:  Cannot stat device %s\n",
				argv[optind]);
			exit(1);
		}

		if ((statbuf.st_mode & S_IFMT) != S_IFCHR) {
			(void) fprintf(stderr,
				"fmthard:  %s must be a raw device.\n",
				argv[optind]);
			exit(1);
		}
	}

	if ((fd = open(argv[optind], O_RDWR|O_CREAT|O_NDELAY)) < 0) {
#elif defined(sparc)
	if (stat(argv[optind], (struct stat *)&statbuf) == -1) {
		(void) fprintf(stderr, "fmthard:  Cannot stat device %s\n",
			argv[optind]);
		exit(1);
	}

	if ((statbuf.st_mode & S_IFMT) != S_IFCHR) {
		(void) fprintf(stderr, "fmthard:  %s must be a raw device.\n",
			argv[optind]);
		exit(1);
	}

	if ((fd = open(argv[optind], O_RDWR|O_NDELAY)) < 0) {
#else
#error No platform defined.
#endif
		(void) fprintf(stderr, "fmthard:  Cannot open device %s - %s\n",
			argv[optind], sys_errlist[errno]);
		exit(1);
	}

#if defined(i386)
	/*
	 * Get the geometry information for this disk from the driver
	 */
	if (io_sgeom) {
		char    line[256];
		FILE *fp;
		/* open the prototype file */
		if ((fp = fopen(io_sgeom, "r")) == NULL) {
			(void) fprintf(stderr,
				    "Fmthard: Cannot open file %s\n",
					io_sgeom);
			exit(1);
		}

		/* read a line from the file */
		while (fgets(line, sizeof (line) - 1, fp)) {
			if (line[0] == '\0' || line[0] == '\n' ||
					    line[0] == '*')
			continue;
			else {
				line[strlen(line)] = '\0';
				if (sscanf(line, "%d %d %d %d %d %d %d %d %d",
					    &disk_geom.dkg_pcyl,
					    &disk_geom.dkg_ncyl,
					    &disk_geom.dkg_acyl,
					    &disk_geom.dkg_bcyl,
					    &disk_geom.dkg_nhead,
					    &disk_geom.dkg_nsect,
					    &sectsiz,
					    &partoff,
					    &vtocsiz) != 9) {
					(void) fprintf(stderr,
						"Syntax error: \"%s\"\n",
							line);
					exit(1);
				}
				break;
			} /* else */
		} /* while (fgets(line, sizeof (line) - 1, fp)) */
		(void) fclose(fp);
	} else {
		if (ioctl(fd, DKIOCGGEOM, &disk_geom)) {
			(void) fprintf(stderr,
				    "%s: Cannot get disk geometry\n",
					argv[optind]);
			exit(1);
		}
		partoff = 0;
		vtocsiz = 2;
	}

#endif	/* defined(i386) */


	/*
	 * Read the vtoc on the disk
	 */
	vread(fd, &disk_vtoc, argv[optind]);

#if defined(sparc)
	/*
	 * Get the geometry information for this disk from the driver
	 */
	if (ioctl(fd, DKIOCGGEOM, &disk_geom)) {
		(void) fprintf(stderr, "%s: Cannot get disk geometry\n",
			argv[optind]);
		exit(1);
	}
#endif

#if defined(i386)
	if (delta && vt_inval) {
		exit(1);
	}
#endif	/* defined(i386) */


	/*
	 * Quick check for valid disk: 0 if ok, 1 if not
	 */
	if (qflag) {
		exit(disk_vtoc.v_sanity == VTOC_SANE ? 0 : 1);
	}

	/*
	 * Incremental changes to the VTOC
	 */
	if (delta) {
		insert(delta, &disk_vtoc);
		validate(&disk_geom, &disk_vtoc);
		vwrite(fd, &disk_vtoc, argv[optind]);
		exit(0);
	}

	/*
	 * Read new VTOC from stdin or data file
	 */
	if (dfile) {
		if (vname) {
			n = strlen(vname) + 1;
			n = min(n, LEN_DKL_VVOL);
			(void) memcpy(disk_vtoc.v_volume, vname, n);
		}
		if (strcmp(dfile, "-") == 0) {
			load(stdin, &disk_geom, &disk_vtoc);
		} else {
			FILE *fp;
			if ((fp = fopen(dfile, "r")) == NULL) {
				(void) fprintf(stderr, "Cannot open file %s\n",
					dfile);
				exit(1);
			}
			load(fp, &disk_geom, &disk_vtoc);
			(void) fclose(fp);
		}
	} else if (vname) {
		n = strlen(vname) + 1;
		n = min(n, LEN_DKL_VVOL);
		(void) memcpy(disk_vtoc.v_volume, vname, n);
	} else {
		usage();
	}


	/*
	 * Print the modified VTOC, rather than updating the disk
	 */
	if (iflag) {
		display(&disk_geom, &disk_vtoc, argv[optind]);
		exit(0);
	}

	/*
	 * Write the new VTOC on the disk
	 */
	validate(&disk_geom, &disk_vtoc);
	vwrite(fd, &disk_vtoc, argv[optind]);

	/*
	 * Shut system down after writing a new vtoc to disk
	 * This is used during installation of core floppies.
	 */
	if (uflag == 1)
		uadmin(A_REBOOT, AD_BOOT, 0);
	else if (uflag == 2)
		uadmin(A_REBOOT, AD_IBOOT, 0);

#if defined(i386)
	if (!usilent)
#endif	/* defined(i386) */
	(void) printf("fmthard:  New volume table of contents now in place.\n");

	return (0);
	/*NOTREACHED*/
}



/*
 * display ()
 *
 * display contents of VTOC without writing it to disk
 */
static void
display(struct dk_geom *geom, struct vtoc *vtoc, char *device)
{
	register int	i;
	register int	c;

	/*
	 * Print out the VTOC
	 */
	(void) printf("* %s default partition map\n", device);
	if (*vtoc->v_volume) {
		(void) printf("* Volume Name:  ");
		for (i = 0; i < LEN_DKL_VVOL; i++) {
			if ((c = vtoc->v_volume[i]) == 0)
				break;
			(void) printf("%c", c);
		}
		(void) printf("\n");
	}
	(void) printf("*\n");
	(void) printf("* Dimensions:\n");
	(void) printf("*     %d bytes/sector\n", SECSIZE);
	(void) printf("*      %d sectors/track\n", geom->dkg_nsect);
	(void) printf("*       %d tracks/cylinder\n", geom->dkg_nhead);
	(void) printf("*     %d cylinders\n", geom->dkg_pcyl);
	(void) printf("*     %d accessible cylinders\n", geom->dkg_ncyl);
	(void) printf("*\n");
	(void) printf("* Flags:\n");
	(void) printf("*   1:  unmountable\n");
	(void) printf("*  10:  read-only\n");
	(void) printf("*\n");
	(void) printf(
"\n* Partition    Tag     Flag	    First Sector    Sector Count\n");
	for (i = 0; i < V_NUMPAR; i++) {
		if (vtoc->v_part[i].p_size > 0)
			(void) printf(
"    %d		%d	0%x		%ld		%ld\n",
				i, vtoc->v_part[i].p_tag,
				vtoc->v_part[i].p_flag,
				vtoc->v_part[i].p_start,
				vtoc->v_part[i].p_size);
	}
	exit(0);
}


/*
 * insert()
 *
 * Insert a change into the VTOC.
 */
static void
insert(char *data, struct vtoc *vtoc)
{
	int	part;
	int	tag;
	uint	flag;
	daddr_t	start;
	long	size;

	if (sscanf(data, "%d:%d:%x:%ld:%ld",
	    &part, &tag, &flag, &start, &size) != 5) {
		(void) fprintf(stderr, "Delta syntax error on \"%s\"\n", data);
		exit(1);
	}
	if (part >= V_NUMPAR) {
		(void) fprintf(stderr,
			"Error in data \"%s\": No such partition %x\n",
			data, part);
		exit(1);
	}
	vtoc->v_part[part].p_tag = (ushort) tag;
	vtoc->v_part[part].p_flag = (ushort) flag;
	vtoc->v_part[part].p_start = start;
	vtoc->v_part[part].p_size = size;
}

/*
 * load()
 *
 * Load VTOC information from a datafile.
 */
static void
load(FILE *fp, struct dk_geom *geom, struct vtoc *vtoc)
{
	int	part;
	int	tag;
	uint	flag;
	daddr_t	start;
	long	size;
	char	line[256];
	int	i;
	long	nblks;
	long	fullsz;

	for (i = 0; i < V_NUMPAR; ++i) {
		vtoc->v_part[i].p_tag = 0;
		vtoc->v_part[i].p_flag = V_UNMNT;
		vtoc->v_part[i].p_start = 0;
		vtoc->v_part[i].p_size = 0;
	}
	/*
	 * initialize partition 2, by convention it corresponds to whole
	 * disk. It will be overwritten, if specified in the input datafile
	 */
	fullsz = geom->dkg_ncyl * geom->dkg_nsect * geom->dkg_nhead;
	vtoc->v_part[2].p_tag = V_BACKUP;
	vtoc->v_part[2].p_flag = V_UNMNT;
	vtoc->v_part[2].p_start = 0;
	vtoc->v_part[2].p_size = fullsz;

	nblks = geom->dkg_nsect * geom->dkg_nhead;

	while (fgets(line, sizeof (line) - 1, fp)) {
		if (line[0] == '\0' || line[0] == '\n' || line[0] == '*')
			continue;
		line[strlen(line) - 1] = '\0';
		if (sscanf(line, "%d %d %x %ld %ld",
		    &part, &tag, &flag, &start, &size) != 5) {
			(void) fprintf(stderr, "Syntax error: \"%s\"\n",
				line);
			exit(1);
		}
		if (part >= V_NUMPAR) {
			(void) fprintf(stderr,
				"No such partition %x: \"%s\"\n",
				part, line);
			exit(1);
		}
		if ((start % nblks) != 0 || (size % nblks) != 0) {
			(void) fprintf(stderr,
"Partition %d not aligned on cylinder boundary: \"%s\"\n",
					part, line);
			exit(1);
		}
		vtoc->v_part[part].p_tag = (ushort) tag;
		vtoc->v_part[part].p_flag = (ushort) flag;
		vtoc->v_part[part].p_start = start;
		vtoc->v_part[part].p_size = size;
	}
	for (part = 0; part < V_NUMPAR; part++) {
		vtoc->timestamp[part] = (time_t)0;
	}
}


static void
usage()
{
	(void) fprintf(stderr,
#if defined(sparc)
"Usage:	fmthard [ -i ] [ -n volumename ] [ -s datafile ] [ -d arguments] \
raw-device\n");

#elif defined(i386)
"Usage:	fmthard [ -i ] [ -S ] [-b bootblk] [-p pboot] [-I geom_file]  \
-n volumename | -s datafile  [ -d arguments] raw-device\n");

#else
#error No platform defined.
#endif
	exit(2);
}

/*
 * validate()
 *
 * Validate the new VTOC.
 */
static void
validate(struct dk_geom *geom, struct vtoc *vtoc)
{
	int	i;
	int	j;
	long	fullsz;
	long	endsect;
	daddr_t	istart;
	daddr_t	jstart;
	long	isize;
	long	jsize;
	long	nblks;

	nblks = geom->dkg_nsect * geom->dkg_nhead;

	fullsz = geom->dkg_ncyl * geom->dkg_nsect * geom->dkg_nhead;

#if defined(_SUNOS_VTOC_16)
	/* make the vtoc look sane - ha ha */
	vtoc->v_version = V_VERSION;
	vtoc->v_sanity = VTOC_SANE;
	vtoc->v_nparts = V_NUMPAR;
	if (sectsiz == 0)
		sectsiz = SECSIZE;
	if (vtoc->v_sectorsz == 0 || io_sgeom)
		vtoc->v_sectorsz = sectsiz;
#endif				/* defined(_SUNOS_VTOC_16) */

	for (i = 0; i < V_NUMPAR; i++) {
		if (vtoc->v_part[i].p_tag == V_BACKUP) {
			if (vtoc->v_part[i].p_size != fullsz) {
#if defined(i386)
				if (!usilent)
#endif
				(void) fprintf(stderr, "\
fmthard: Partition %d specifies the full disk and is not equal\n\
full size of disk.  The full disk capacity is %lu sectors.\n", i, fullsz);
#if defined(sparc)
			exit(1);
#endif
			}
		}
		if (vtoc->v_part[i].p_size == 0)
			continue;	/* Undefined partition */
		if ((vtoc->v_part[i].p_start % nblks) ||
				(vtoc->v_part[i].p_size % nblks)) {
			(void) fprintf(stderr, "\
fmthard: Partition %d not aligned on cylinder boundary \n", i);
				exit(1);
		}
		if (vtoc->v_part[i].p_start > fullsz ||
			vtoc->v_part[i].p_start +
				vtoc->v_part[i].p_size > fullsz) {
#if defined(i386)
			if (!usilent)
#endif
			(void) fprintf(stderr, "\
fmthard: Partition %d specified as %lu sectors starting at %lu\n\
\tdoes not fit. The full disk contains %lu sectors.\n",
				i, vtoc->v_part[i].p_size,
				vtoc->v_part[i].p_start, fullsz);
#if defined(sparc)
			exit(1);
#endif
		}

		if (vtoc->v_part[i].p_tag != V_BACKUP &&
		    vtoc->v_part[i].p_size != fullsz) {
			for (j = 0; j < V_NUMPAR; j++) {
				if (vtoc->v_part[j].p_tag == V_BACKUP)
					continue;
				if (vtoc->v_part[j].p_size == fullsz)
					continue;
				isize = vtoc->v_part[i].p_size;
				jsize = vtoc->v_part[j].p_size;
				istart = vtoc->v_part[i].p_start;
				jstart = vtoc->v_part[j].p_start;
				if ((i != j) &&
				    (isize != 0) && (jsize != 0)) {
					endsect = jstart + jsize -1;
					if ((jstart <= istart) &&
						(istart <= endsect)) {
#if defined(i386)
						if (!usilent)
#endif
						(void) fprintf(stderr, "\
fmthard: Partition %d overlaps partition %d. Overlap is allowed\n\
\tonly on partition on the full disk partition).\n",
						    i, j);
#if defined(sparc)
						exit(1);
#endif
					}
				}
			}
		}
	}
}


/*
 * Read the VTOC
 */
void
vread(int fd, struct vtoc *vtoc, char *devname)
{
	int	i;

#if defined(sparc)
	if ((i = read_vtoc(fd, vtoc)) < 0) {
		if (i == VT_EINVAL) {
			(void) fprintf(stderr, "%s: Invalid VTOC\n",
				devname);
		} else {
			(void) fprintf(stderr, "%s: Cannot read VTOC\n",
				devname);
		}
		exit(1);
	}
#elif defined(i386)
	if (io_sgeom) {
		if ((i = read_label(fd, vtoc)) < 0) {
			if (i == VT_EINVAL) {
				if (!usilent)
					(void) fprintf(stderr,
						"%s: Invalid VTOC\n",
						devname);
				vt_inval++;
				return;
			} else {
				(void) fprintf(stderr, "%s: Cannot read VTOC\n",
					devname);
				vt_inval++;
				return;
			}
		}
	} else {
		if ((i = read_vtoc(fd, vtoc)) < 0) {
			if (i == VT_EINVAL) {
				if (!usilent)
					(void) fprintf(stderr,
						"%s: Invalid VTOC\n",
						devname);
				vt_inval++;
				return;
			} else {
				(void) fprintf(stderr, "%s: Cannot read VTOC\n",
					devname);
				vt_inval++;
				return;
			}
		}
	}
#else
#error No platform defined.
#endif
}


/*
 * Write the VTOC
 */
void
vwrite(int fd, struct vtoc *vtoc, char *devname)
{
	int	i;

#if defined(sparc)
	if ((i = write_vtoc(fd, vtoc)) != 0) {
		if (i == VT_EINVAL) {
			(void) fprintf(stderr,
			"%s: invalid entry exists in vtoc\n",
				devname);
		} else {
			(void) fprintf(stderr, "%s: Cannot write VTOC\n",
				devname);
		}
		exit(1);
	}
#elif defined(i386)
	if (io_sgeom) {
		if ((i = write_label(fd, vtoc)) != 0) {
			if (i == VT_EINVAL) {
				(void) fprintf(stderr,
				"%s: invalid entry exists in vtoc\n",
					devname);
			} else {
				(void) fprintf(stderr,
					"%s: Cannot write VTOC\n",
					devname);
			}
			exit(1);
		}
	} else {
		if ((i = write_vtoc(fd, vtoc)) != 0) {
			if (i == VT_EINVAL) {
				(void) fprintf(stderr,
				"%s: invalid entry exists in vtoc\n",
					devname);
			} else {
				(void) fprintf(stderr,
					"%s: Cannot write VTOC\n",
					devname);
			}
			exit(1);
		}
	}
#if defined(i386)
{
		int mDev;
		struct  stat    statbuf;
		char *bbootptr, *pbootptr;
		int pbootsiz;

/* ***** read the partition boot record from file ***** */
		/* get memory buffer to hold partboot loader */
		bbootptr = (char *)malloc(512);
		if (bbootptr == NULL) {
			fprintf(stderr,
		"Fmthard: Unable to obtain %d bytes of Buffer memory.\n",
				512);
			exit(1);
		}

		if ((mDev = open(uboot, O_RDONLY, 0666)) == -1) {
			fprintf(stderr,
	"Fmthard: Partition boot file (%s) cannot be opened\n", uboot);
			exit(1);
		}

		/* read the partition boot program */
		if (read(mDev, bbootptr, 512) != 512) {
			fprintf(stderr,
	"Fmthard: partition boot file (%s) cannot be read\n", uboot);
			exit(1);
		}

		close(mDev);

/* ***** read the priboot (bootblk) from file ***** */
		/* find size of pri boot program */
		if (stat(pboot, (struct stat *)&statbuf) == -1) {
			(void) fprintf(stderr,
			"Fmthard:  Cannot stat priboot %s\n", pboot);
			exit(1);
		}
		pbootsiz = statbuf.st_size;

		/* get memory buffer to hold priboot loader */
		pbootptr = (char *)malloc(pbootsiz+(512-pbootsiz%512));
		if (pbootptr == NULL) {
			fprintf(stderr,
	"Fmthard: Unable to obtain %d bytes of Buffer memory.\n",
				    pbootsiz);
			exit(1);
		}

		if ((mDev = open(pboot, O_RDONLY, 0666)) == -1) {
			fprintf(stderr,
		"Fmthard: pri boot file (%s) cannot be opened\n", pboot);
			exit(1);
		}

		/* read the pri boot program */
		if (read(mDev, pbootptr, pbootsiz) != pbootsiz) {
			fprintf(stderr,
			"Fmthard: Pri boot file (%s) cannot be read\n", pboot);
			exit(1);
		}

		close(mDev);

/* *** Now that we read in the boot files write them out to device *** */
		/* write out part boot record first */
		if (lseek(fd, (partoff)*512, 0) == -1) {
			fprintf(stderr,
			"Fmthard: Error seeking on write partboot\n");
			exit(1);
		}
		if (write(fd, bbootptr, 512) != 512) {
			fprintf(stderr, "Fmthard: Error writing partboot\n");
			exit(1);
		}

		/* write out priboot (bootblk) record first */
		if (lseek(fd, (vtocsiz+1+partoff)*512, 0) == -1) {
			fprintf(stderr,
			"Fmthard: Error seeking on write priboot(bootblk)\n");
			exit(1);
		}
		if (write(fd, pbootptr, (pbootsiz + (512 - pbootsiz%512)))
			    != (pbootsiz + (512 - pbootsiz%512))) {
			fprintf(stderr,
				"Fmthard: Error writing priboot (bootblk)\n");
			exit(1);
		}
}
#endif	/* defined(i386) */

#else
#error No platform defined.
#endif
}


#if defined(_SUNOS_VTOC_16)

/* ********************************************* */
/* read_label from disk and copy vtoc to user  */
/* ********************************************* */
read_label(int fd, struct vtoc *vtoc)
{
	short   *sp;
	short   count;
	unsigned short  sum;
	struct dk_label *lbp = &dklabel;

	if (lseek(fd, DK_LABEL_LOC * 512, 0) == -1) {
		return (VT_EIO);
	}

	if (read(fd, lbp, sizeof (struct dk_label)) !=
					sizeof (struct dk_label)) {
		return (VT_EIO);
	}


/*	Check magic number of the label					*/
	if ((lbp->dkl_magic != DKL_MAGIC) ||
	    (((struct vtoc *)lbp)->v_sanity != VTOC_SANE) ||
	    (((struct vtoc *)lbp)->v_version != V_VERSION)) {
		return (VT_EINVAL);
	}

/*	Check the checksum of the label					*/
	sp = (short *)lbp;
	count = sizeof (struct dk_label) / sizeof (short);

	while (count--)  {
		sum ^= *sp++;
	}

	if (sum)
		return (VT_EINVAL);

	(void) memcpy((caddr_t)vtoc, (caddr_t)&(dklabel.dkl_vtoc),
			    sizeof (struct vtoc));
	return (V_NUMPAR);

}


/* ************************************* */
/* write vtoc/label			*/
/* ************************************* */
write_label(int fd, struct vtoc *vtoc)
{
	short	*sp;
	short	sum;
	int	i;
	struct dk_label *lbp = &dklabel;

	lbp->dkl_magic = DKL_MAGIC;

	(void) memcpy((caddr_t)&(lbp->dkl_vtoc), (caddr_t)vtoc, sizeof (*vtoc));

	lbp->dkl_pcyl = disk_geom.dkg_pcyl;
	lbp->dkl_ncyl = disk_geom.dkg_ncyl;
	lbp->dkl_acyl = disk_geom.dkg_acyl;
	lbp->dkl_nhead = disk_geom.dkg_nhead;
	lbp->dkl_nsect = disk_geom.dkg_nsect;
	lbp->dkl_intrlv = disk_geom.dkg_intrlv;
	lbp->dkl_apc = disk_geom.dkg_apc;
	lbp->dkl_rpm = disk_geom.dkg_rpm;
	lbp->dkl_write_reinstruct = disk_geom.dkg_write_reinstruct;
	lbp->dkl_read_reinstruct = disk_geom.dkg_read_reinstruct;



	/*
	 * Construct checksum for the new disk label
	 */
	sum = 0;
	sp = (short *)lbp;
	for (i = sizeof (struct dk_label)/sizeof (short) - 1; i > 0; i--) {
		sum ^= *sp++;
	}
	lbp->dkl_cksum = sum;

	if (lseek(fd, DK_LABEL_LOC * 512, 0) == -1) {
		return (VT_EIO);
	}

	if (write(fd, lbp, sizeof (struct dk_label)) !=
					sizeof (struct dk_label)) {
		return (VT_EIO);
	}
	return (V_NUMPAR);
}
#endif	/* defined(_SUNOS_VTOC_16) */
