
#ident	"@(#)newfs.c	1.27	99/10/12 SMI"	/* from UCB 5.2 9/11/85 */

/*
 * newfs: friendly front end to mkfs
 *
 * Copyright (c) 1991,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fs/ufs_fs.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/sysmacros.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libintl.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/mkdev.h>

static unsigned int number(char *, char *, int, int);
static void getdiskbydev(char *);
static int  yes(void);
static int  notrand(char *);
static void usage();
static int readvtoc(int, char *, struct vtoc *);
static void exenv(void);
static struct fs *read_sb(char *);
/*PRINTFLIKE1*/
static void fatal(char *fmt, ...);

#define	EPATH "PATH=/usr/sbin:/sbin:"
#define	CPATH "/sbin"					/* an EPATH element */
#define	MB (1024 * 1024)
#define	GBSEC ((1024 * 1024 * 1024) / DEV_BSIZE)	/* sectors in a GB */
#define	MINFREESEC ((64 * 1024 * 1024) / DEV_BSIZE)	/* sectors in 64 MB */
#define	MINCPG (16)	/* traditional */
#define	MAXDENSITY (8 * 1024)	/* arbitrary */
#define	MINDENSITY (2 * 1024)	/* traditional */

/* For use with number() */
#define	NR_NONE		0
#define	NR_PERCENT	0x01

int	Nflag;			/* run mkfs without writing file system */
int	verbose;		/* show mkfs line before exec */
long	fssize;			/* file system size */
int	fsize;			/* fragment size */
int	bsize;			/* block size */
int	ntracks;		/* # tracks/cylinder */
int	nsectors;		/* # sectors/track */
int	cpg;			/* cylinders/cylinder group */
int	minfree = -1;		/* free space threshold */
int	rpm;			/* revolutions/minute of drive */
int	nrpos = 8;		/* # of distinguished rotational positions */
				/* 8 is the historical default */
int	density;		/* number of bytes per inode */
int	apc;			/* alternates per cylinder */
int 	rot = -1;		/* rotational delay (msecs) */
int 	maxcontig = -1;		/* maximum number of contig blocks */

char	device[MAXPATHLEN];
char	cmd[BUFSIZ];

extern	char	*getfullrawname();

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, optopt;

	char *special, *name;
	char message[256];
	struct stat64 st;
	int status;
	int opt;
	struct fs *sbp;	/* Pointer to superblock (if present) */

	while ((opt = getopt(argc, argv, "vNs:C:d:t:o:a:b:f:c:m:n:r:i:")) !=
	    EOF) {
		switch (opt) {
		case 'v':
			verbose++;
			break;

		case 'N':
			Nflag++;
			break;

		case 's':
			fssize = number("fssize", optarg, NR_NONE, INT_MAX);
			if (fssize < 1024)
				fatal("%s: fssize must be at least 1024",
				    optarg);
			break;

		case 'C':
			maxcontig = number("maxcontig", optarg, NR_NONE,
			    56 * 1024 / 8192);   /* 56KB / bsize */
			if (maxcontig < 0 || maxcontig > (MB / 4096))
				fatal("%s: bad maxcontig", optarg);
			break;

		case 'd':
			rot = number("rotdelay", optarg, NR_NONE, 0);
			if (rot < 0 || rot > 1000)
				fatal("%s: bad rotational delay", optarg);
			break;

		case 't':
			ntracks = number("ntrack", optarg, NR_NONE, 16);
			if ((ntracks < 0) ||
			    (ntracks > INT_MAX))
				fatal("%s: bad total tracks", optarg);
			break;

		case 'o':
			if (strcmp(optarg, "space") == 0)
			    opt = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
			    opt = FS_OPTTIME;
			else
			    fatal("%s: bad optimization preference"
				" (options are `space' or `time')",
				optarg);
			break;

		case 'a':
			apc = number("apc", optarg, NR_NONE, 0);
			if (apc < 0 || apc > 32768) /* see mkfs.c */
				fatal("%s: bad alternates per cyl ", optarg);
			break;

		case 'b':
			bsize = number("bsize", optarg, NR_NONE, MAXBSIZE);
			if (bsize < MINBSIZE || bsize > MAXBSIZE)
				fatal("%s: bad block size", optarg);
			break;

		case 'f':
			fsize = number("fragsize", optarg, NR_NONE,
			    MAXBSIZE/8);
			/* xxx ought to test against bsize for uppper limit */
			if (fsize < DEV_BSIZE)
				fatal("%s: bad frag size", optarg);
			break;

		case 'c':
			cpg = number("cpg", optarg, NR_NONE, 16);
			if (cpg < 1)
				fatal("%s: bad cylinders/group", optarg);
			break;

		case 'm':
			minfree = number("minfree", optarg, NR_PERCENT, 10);
			if (minfree < 0 || minfree > 99)
				fatal("%s: bad free space %%", optarg);
			break;

		case 'n':
			nrpos = number("nrpos", optarg, NR_NONE, 8);
			if (nrpos <= 0)
				fatal("%s: bad number of rotational positions",
				    optarg);
			break;

		case 'r':
			rpm = number("rpm", optarg, NR_NONE, 3600);
			if (rpm < 0)
				fatal("%s: bad revs/minute", optarg);
			break;

		case 'i':
			/* xxx ought to test against fsize */
			density = number("nbpi", optarg, NR_NONE, 2048);
			if (density < DEV_BSIZE)
				fatal("%s: bad bytes per inode", optarg);
			break;

		default:
			usage();
			fatal("-%c: unknown flag", optopt);
		}
	}

	/* At this point, there should only be one argument left:	*/
	/* The raw-special-device itself. If not, print usage message.	*/
	if ((argc - optind) != 1) {
		usage();
		exit(1);
	}

	name = argv[optind];

	special = getfullrawname(name);
	if (special == NULL) {
		(void) fprintf(stderr, "newfs: malloc failed\n");
		exit(1);
	}

	if (*special == '\0') {
		if (strchr(name, '/') != NULL) {
			if (stat64(name, &st) < 0) {
				(void) fprintf(stderr, "newfs: %s: %s\n",
				    name, strerror(errno));
				exit(2);
			}
			fatal("%s: not a raw disk device", name);
		}
		(void) sprintf(device, "/dev/rdsk/%s", name);
		if ((special = getfullrawname(device)) == NULL) {
			(void) fprintf(stderr, "newfs: malloc failed\n");
			exit(1);
		}

		if (*special == '\0') {
			(void) sprintf(device, "/dev/%s", name);
			if ((special = getfullrawname(device)) == NULL) {
				(void) fprintf(stderr,
				    "newfs: malloc failed\n");
				exit(1);
			}
			if (*special == '\0')
				fatal("%s: not a raw disk device", name);
		}
	}

	/* note: getdiskbydev does not set apc */
	getdiskbydev(special);
	if (fssize < 0) {
		(void) sprintf(message,
		    "%s is too big.  Use \"newfs -s %ld %s\"",
		    special, LONG_MAX, special);
		fatal(message);
	}
	if (nsectors < 0)
		fatal("%s: no default #sectors/track", special);
	if (ntracks < 0)
		fatal("%s: no default #tracks", special);
	if (bsize < 0)
		fatal("%s: no default block size", special);
	if (fsize < 0)
		fatal("%s: no default frag size", special);
	if (rpm < 0)
		fatal("%s: no default revolutions/minute value", special);
	if (rpm < 60) {
		(void) fprintf(stderr, "Warning: setting rpm to 60\n");
		rpm = 60;
	}
	/* XXX - following defaults are both here and in mkfs */
	if (density <= 0) {
		if (fssize < GBSEC)
			density = MINDENSITY;
		else
			density = ((fssize + (GBSEC - 1)) /
						GBSEC) * MINDENSITY;
		if (density <= 0)
			density = MINDENSITY;
		if (density > MAXDENSITY)
			density = MAXDENSITY;
	}
	if (cpg <= 0) {
		/*
		 * maxcpg calculation adapted from mkfs
		 */
		long maxcpg, maxipg;

		maxipg = roundup(bsize * NBBY / 3,
		    bsize / sizeof (struct inode));
		maxcpg = (bsize - sizeof (struct cg) - howmany(maxipg, NBBY)) /
		    (sizeof (long) + nrpos * sizeof (short) +
			nsectors / (MAXFRAG * NBBY));
		cpg = (fssize / GBSEC) * 32;
		if (cpg > maxcpg)
			cpg = maxcpg;
		if (cpg <= 0)
			cpg = MINCPG;
	}
	if (minfree < 0) {
		minfree = ((float)MINFREESEC / fssize) * 100;
		if (minfree > 10)
			minfree = 10;
		if (minfree <= 0)
			minfree = 1;
	}
#ifdef i386	/* Bug 1170182 */
	if (ntracks > 32 && (ntracks % 16) != 0) {
		ntracks -= (ntracks % 16);
	}
#endif
	/*
	 * Confirmation
	 */
	if (isatty(fileno(stdin)) && !Nflag) {
		/*
		 * If we can read a valid superblock, report the mount
		 * point on which this filesystem was last mounted.
		 */
		if (((sbp = read_sb(special)) != 0) &&
		    (*sbp->fs_fsmnt != '\0')) {
			(void) printf("newfs: %s last mounted as %s\n",
			    special, sbp->fs_fsmnt);
		}
		(void) printf("newfs: construct a new file system %s: (y/n)? ",
		    special);
		(void) fflush(stdout);
		if (!yes())
			exit(0);
	}
	/*
	 * If alternates-per-cylinder is ever implemented:
	 * need to get apc from dp->d_apc if no -a switch???
	 */
	(void) sprintf(cmd,
	"mkfs -F ufs %s%s %ld %d %d %d %d %d %d %d %d %s %d %d %d %d",
	    Nflag ? "-o N " : "", special,
	    fssize, nsectors, ntracks, bsize, fsize, cpg, minfree, rpm/60,
	    density, opt == FS_OPTSPACE ? "s" : "t", apc, rot, nrpos,
	    maxcontig);
	if (verbose) {
		(void) printf("%s\n", cmd);
		(void) fflush(stdout);
	}
	exenv();
	if (status = system(cmd))
		exit(status >> 8);
	if (Nflag)
		exit(0);
	(void) sprintf(cmd, "/usr/sbin/fsirand %s", special);
	if (notrand(special) && (status = system(cmd)) != 0)
		(void) fprintf(stderr, "%s: failed, status = %d\n",
		    cmd, status);
	return (0);
}

static void
exenv(void)
{
	char *epath;				/* executable file path */
	char *cpath;				/* current path */

	if ((cpath = getenv("PATH")) == NULL) {
		(void) fprintf(stderr, "newfs: no PATH in env\n");
		/*
		 * Background: the Bourne shell interpolates "." into
		 * the path where said path starts with a colon, ends
		 * with a colon, or has two adjacent colons.  Thus,
		 * the path ":/sbin::/usr/sbin:" is equivalent to
		 * ".:/sbin:.:/usr/sbin:.".  Now, we have no cpath,
		 * and epath ends in a colon (to make for easy
		 * catenation in the normal case).  By the above, if
		 * we use "", then "." becomes part of path.  That's
		 * bad, so use CPATH (which is just a duplicate of some
		 * element in EPATH).  No point in opening ourselves
		 * up to a Trojan horse attack when we don't have to....
		 */
		cpath = CPATH;
	}
	if ((epath = malloc(strlen(EPATH) + strlen(cpath) + 1)) == NULL) {
		(void) fprintf(stderr, "newfs: malloc failed\n");
		exit(1);
	}
	(void) strcpy(epath, EPATH);
	(void) strcat(epath, cpath);
	if (putenv(epath) < 0) {
		(void) fprintf(stderr, "newfs: putenv failed\n");
		exit(1);
	}
}

static int
yes(void)
{
	int	i, b;

	i = b = getchar();
	while (b != '\n' && b != '\0' && b != EOF)
		b = getchar();
	return (i == 'y');
}

/*
 * xxx Caller must run fmt through gettext(3) for us, if we ever
 * xxx go the i18n route....
 */
static void
fatal(char *fmt, ...)
{
	va_list pvar;

	(void) fprintf(stderr, "newfs: ");
	va_start(pvar, fmt);
	(void) vfprintf(stderr, fmt, pvar);
	va_end(pvar);
	(void) putc('\n', stderr);
	exit(10);
}

static void
getdiskbydev(char *disk)
{
	int partno;
	struct dk_geom g;
	struct dk_cinfo ci;
	int fd;
	struct vtoc vtoc;

	if ((fd = open64(disk, 0)) < 0) {
		perror(disk);
		exit(1);
	}
	if (ioctl(fd, DKIOCGGEOM, &g))
		fatal("%s: Unable to read Disk geometry", disk);
	partno = readvtoc(fd, disk, &vtoc);
	if (partno > (int)vtoc.v_nparts)
		fatal("%s: can't figure out file system partition", disk);
	if (fssize == 0)
		fssize = (int)vtoc.v_part[partno].p_size;
	if (nsectors == 0)
		nsectors = g.dkg_nsect;
	if (ntracks == 0)
		ntracks = g.dkg_nhead;
	if (bsize == 0)
		bsize = 8192;
	if (fsize == 0)
		fsize = 1024;
	if (rpm == 0)
		rpm = ((int)g.dkg_rpm <= 0) ? 3600: g.dkg_rpm;
	/*
	 * Adjust maxcontig by the device's maxtransfer.
	 *    BUT, as a safeguard, don't let it exceed a MB
	 */
	if (maxcontig == -1 && ioctl(fd, DKIOCINFO, &ci) == 0) {
		maxcontig = ci.dki_maxtransfer * DEV_BSIZE;
		if (maxcontig < 0 || maxcontig > MB)
			maxcontig = MB;
		maxcontig /= bsize;
		if (maxcontig > nsectors)
			maxcontig = nsectors;
	}
	(void) close(fd);
}

/*
 * readvtoc()
 *
 * Read a partition map.
 */
static int
readvtoc(
	int		fd,	/* opened device */
	char		*name,	/* name of disk device */
	struct vtoc	*vtoc)
{
	int	retval;

	if ((retval = read_vtoc(fd, vtoc)) >= 0)
		return (retval);

	switch (retval) {
	case VT_ERROR:
		(void) fprintf(stderr,
		    "newfs: %s: %s\n", name, strerror(errno));
		exit(10);
		/*NOTREACHED*/
	case VT_EIO:
		fatal("%s: I/O error accessing VTOC", name);
		/*NOTREACHED*/
	case VT_EINVAL:
		fatal("%s: Invalid field in VTOC", name);
		/*NOTREACHED*/
	default:
		fatal("%s: unknown error accessing VTOC", name);
		/*NOTREACHED*/
	}
}

/*
 * read_sb(char * rawdev) - Attempt to read the superblock from a raw device
 *
 * Returns:
 *	0 :
 *		Could not read a valid superblock for a variety of reasons.
 *		Since 'newfs' handles any fatal conditions, we're not going
 *		to make any guesses as to why this is failing or what should
 *		be done about it.
 *
 *	struct fs *:
 *		A pointer to (what we think is) a valid superblock. The
 *		space for the superblock is static (inside the function)
 *		since we will only be reading the values from it.
 */

struct fs *
read_sb(char *fsdev)
{
	static struct fs	sblock;
	struct stat64		statb;
	int			dskfd;
	char			*bufp = NULL;
	int			bufsz = 0;

	if (stat64(fsdev, &statb) < 0)
		return (0);

	if ((dskfd = open64(fsdev, O_RDONLY)) < 0)
		return (0);

	/*
	 * We need a buffer whose size is a multiple of DEV_BSIZE in order
	 * to read from a raw device (which we were probably passed).
	 */
	bufsz = ((sizeof (sblock) / DEV_BSIZE) + 1) * DEV_BSIZE;
	if ((bufp = malloc(bufsz)) == NULL) {
		(void) close(dskfd);
		return (0);
	}

	if (llseek(dskfd, (offset_t)SBOFF, SEEK_SET) < 0 ||
	    read(dskfd, bufp, bufsz) < 0) {
		(void) close(dskfd);
		free(bufp);
		return (0);
	}
	(void) close(dskfd);	/* Done with the file */

	(void) memcpy(&sblock, bufp, sizeof (sblock));
	free(bufp);	/* Don't need this anymore */

	if (sblock.fs_magic != FS_MAGIC ||
	    sblock.fs_ncg < 1 || sblock.fs_cpg < 1)
		return (0);

	if (sblock.fs_ncg * sblock.fs_cpg < sblock.fs_ncyl ||
	    (sblock.fs_ncg - 1) * sblock.fs_cpg >= sblock.fs_ncyl)
		return (0);

	if (sblock.fs_sbsize < 0 || sblock.fs_sbsize > SBSIZE)
		return (0);

	return (&sblock);
}

/*
 * Read the UFS file system on the raw device SPECIAL.  If it does not
 * appear to be a UFS file system, return non-zero, indicating that
 * fsirand should be called (and it will spit out an error message).
 * If it is a UFS file system, take a look at the inodes in the first
 * cylinder group.  If they appear to be randomized (non-zero), return
 * zero, which will cause fsirand to not be called.  If the inode generation
 * counts are all zero, then we must call fsirand, so return non-zero.
 */

#define	RANDOMIZED	0
#define	NOT_RANDOMIZED	1

static int
notrand(char *special)
{
	long fsbuf[SBSIZE / sizeof (long)];
	struct dinode dibuf[MAXBSIZE/sizeof (struct dinode)];
	struct fs *fs;
	struct dinode *dip;
	offset_t seekaddr;
	int bno, inum;
	int fd;

	fs = (struct fs *)fsbuf;
	if ((fd = open64(special, 0)) == -1)
		return (NOT_RANDOMIZED);
	if (llseek(fd, (offset_t)SBLOCK * DEV_BSIZE, 0) == -1 ||
	    read(fd, (char *)fs, SBSIZE) != SBSIZE ||
	    fs->fs_magic != FS_MAGIC) {
		(void) close(fd);
		return (NOT_RANDOMIZED);
	}

	/* looks like a UFS file system; read the first cylinder group */
	bsize = INOPB(fs) * sizeof (struct dinode);
	inum = 0;
	while (inum < fs->fs_ipg) {
		bno = itod(fs, inum);
		seekaddr = (offset_t)fsbtodb(fs, bno) * DEV_BSIZE;
		if (llseek(fd, seekaddr, 0) == -1 ||
		    read(fd, (char *)dibuf, bsize) != bsize) {
			(void) close(fd);
			return (NOT_RANDOMIZED);
		}
		for (dip = dibuf; dip < &dibuf[INOPB(fs)]; dip++) {
			if (dip->di_gen != 0) {
				(void) close(fd);
				return (RANDOMIZED);
			}
			inum++;
		}
	}
	(void) close(fd);
	return (NOT_RANDOMIZED);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "usage: newfs [ -v ] [ mkfs-options ] raw-special-device\n");
	(void) fprintf(stderr, "where mkfs-options are:\n");
	(void) fprintf(stderr,
	    "\t-N do not create file system,\njust print out parameters");
	(void) fprintf(stderr, "\t-s file system size (sectors)\n");
	(void) fprintf(stderr, "\t-b block size\n");
	(void) fprintf(stderr, "\t-f frag size\n");
	(void) fprintf(stderr, "\t-t tracks/cylinder\n");
	(void) fprintf(stderr, "\t-c cylinders/group\n");
	(void) fprintf(stderr, "\t-m minimum free space %%\n");
	(void) fprintf(stderr,
	    "\t-o optimization preference (`space' or `time')");
	(void) fprintf(stderr, "\t-r revolutions/minute\n");
	(void) fprintf(stderr, "\t-i number of bytes per inode\n");
	(void) fprintf(stderr, "\t-a number of alternates per cylinder\n");
	(void) fprintf(stderr, "\t-C maxcontig\n");
	(void) fprintf(stderr, "\t-d rotational delay\n");
	(void) fprintf(stderr, "\t-n number of rotational positions\n");
}

/*
 * Error-detecting version of atoi(3).  Adapted from mkfs' number().
 */
static unsigned int
number(char *param, char *value, int flags, int def_value)
{
	char *cs;
	int n;
	int cut = INT_MAX / 10;    /* limit to avoid overflow */
	int minus = 0;

	cs = value;
	if (*cs == '-') {
		minus = 1;
		cs += 1;
	}
	if ((*cs < '0') || (*cs > '9')) {
		goto bail_out;
	}
	n = 0;
	while ((*cs >= '0') && (*cs <= '9') && (n <= cut)) {
		n = n*10 + *cs++ - '0';
	}
	if (minus)
	    n = -n;
	for (;;) {
		switch (*cs++) {
		case '\0':
			return (n);

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			(void) fprintf(stderr,
			    "newfs: value for %s overflowed, using %d\n",
			    param, def_value);
			return (def_value);

		case '%':
			if (flags & NR_PERCENT)
				break;
			/* FALLTHROUGH */

		default:
bail_out:
			fatal("bad numeric arg for %s: \"%s\"\n",
			    param, value);

		}
	}
	/* NOTREACHED */
}
