/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989,1991,1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

#ident	"@(#)mkfs.c	1.67	99/10/12 SMI"	/* SVr4.0 1.9 */


/*
 * make file system for cylinder-group style file systems
 *
 * usage:
 *
 *    mkfs [-F FSType] [-V] [-G] [-M dirname] [-m] [options]
 *	[-o specific_options]  special size
 *	[nsect ntrack bsize fsize cpg	minfree	rps nbpi opt apc rotdelay
 *	  2     3      4     5     6	7	8   9	 10  11  12
 *	nrpos maxcontig]
 *	13    14
 *
 *  where specific_options are:
 *	N - no create
 *	nsect - The number of sectors per track
 *	ntrack - The number of tracks per cylinder
 *	bsize - block size
 *	fragsize - fragment size
 *	cgsize - The number of disk cylinders per cylinder group.
 * 	free - minimum free space
 *	rps - rotational speed (rev/sec).
 *	nbpi - number of data bytes per allocated inode
 *	opt - optimization (space, time)
 *	apc - number of alternates
 *	gap - gap size
 *	nrpos - number of rotational positions
 */

/*
 * The following constants set the defaults used for the number
 * of sectors/track (fs_nsect), and number of tracks/cyl (fs_ntrak).
 *
 *			NSECT		NTRAK
 *	72MB CDC	18		9
 *	30MB CDC	18		5
 *	720KB Diskette	9		2
 */

#define	DFLNSECT	32
#define	DFLNTRAK	16

/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DESBLKSIZE <= MAXBSIZE
 *	DEV_BSIZE <= DESFRAGSIZE <= DESBLKSIZE
 *	DESBLKSIZE / DESFRAGSIZE <= 8
 */
#define	DESBLKSIZE	8192
#define	DESFRAGSIZE	1024

/*
 * The maximum number of cylinders in a group depends upon how much
 * information can be stored on a single cylinder. The default is to
 * use 16 cylinders per group.  This is effectively tradition - it was
 * the largest value acceptable under SunOs 4.1
 */
#define	DESCPG		16	/* desired fs_cpg */

/*
 * MINFREE gives the minimum acceptable percentage of file system
 * blocks which may be free. If the freelist drops below this level
 * only the superuser may continue to allocate blocks. This may
 * be set to 0 if no reserve of free blocks is deemed necessary,
 * however throughput drops by fifty percent if the file system
 * is run at between 90% and 100% full; thus the default value of
 * fs_minfree is 10%. With 10% free space, fragmentation is not a
 * problem, so we choose to optimize for time.
 */
#define	MINFREE		10
#define	DEFAULTOPT	FS_OPTTIME

/*
 * ROTDELAY gives the minimum number of milliseconds to initiate
 * another disk transfer on the same cylinder. It is used in
 * determining the rotationally optimal layout for disk blocks
 * within a file; the default of fs_rotdelay is 4ms.  On a drive with
 * track readahead buffering this should be zero.
 */
#define	ROTDELAY	0

/*

 * DEF_MAXTRAX is the default maximum data in bytes that we can
 * transfer in a single request.  Traditionally, this was 56K, but
 * since the driver can tell us the maximum it supports, we now ask it
 * rather than making assumptions.  However, we need a default value
 * when parsing command-line options when given something truly bogus.
 * So, use 56K as an initial default.
 *
 * maxcontig is typically maxtrax/bsize.
 *
 */
#define	DEF_MAXTRAX	(56 * 1024)
#define	MAXCONTIG	(maxtrax / DESBLKSIZE)

/*
 * MAXBLKPG determines the maximum number of data blocks which are
 * placed in a single cylinder group. The default is one indirect
 * block worth of data blocks.
 */
#define	MAXBLKPG(bsize)	((bsize) / sizeof (daddr32_t))

/*
 * Each file system has a number of inodes statically allocated.
 * We allocate one inode slot per NBPI bytes, expecting this
 * to be far more than we will ever need.
 */
#define	NBPI		2048	/* Number Bytes Per Inode */

/*
 * Disks are assumed to rotate at 60HZ, unless otherwise specified.
 */
#define	DEFHZ		60

/*
 * Cylinder group related limits.
 *
 * For each cylinder we keep track of the availability of blocks at different
 * rotational positions, so that we can lay out the data to be picked
 * up with minimum rotational latency.  NRPOS is the number of rotational
 * positions which we distinguish.  With NRPOS 8 the resolution of our
 * summary information is 2ms for a typical 3600 rpm drive.
 */
#define	NRPOS		8	/* number distinct rotational positions */

/*
 * range_check "user_supplied" flag values.
 */
#define	RC_DEFAULT	0
#define	RC_KEYWORD	1
#define	RC_POSITIONAL	2

#ifndef	STANDALONE
#include	<stdio.h>
#include	<sys/mnttab.h>
#endif

#include	<stdlib.h>
#include	<unistd.h>
#include	<malloc.h>
#include	<string.h>
#include	<strings.h>
#include	<errno.h>
#include	<sys/param.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/sysmacros.h>
#include	<sys/vnode.h>
#include	<sys/fs/ufs_fsdir.h>
#include	<sys/fs/ufs_inode.h>
#include	<sys/fs/ufs_fs.h>
#include	<sys/fs/ufs_log.h>
#include	<sys/mntent.h>
#include	<sys/filio.h>
#include	<limits.h>
#include	<sys/int_const.h>
#include	"roll_log.h"

#define	bcopy(f, t, n)    (void) memcpy(t, f, n)
#define	bzero(s, n)	(void) memset(s, 0, n)
#define	bcmp(s, d, n)	memcmp(s, d, n)

#define	index(s, r)	strchr(s, r)
#define	rindex(s, r)	strrchr(s, r)

#include	<sys/stat.h>
#include	<ustat.h>
#include	<locale.h>
#include	<fcntl.h>
#include 	<sys/isa_defs.h>	/* for ENDIAN defines */
#include	<sys/vtoc.h>

#include	<sys/dkio.h>

extern offset_t	llseek();
extern char	*getfullblkname();
extern long	lrand48();

extern int	optind;
extern char	*optarg;


/*
 * The size of a cylinder group is calculated by CGSIZE. The maximum size
 * is limited by the fact that cylinder groups are at most one block.
 * Its size is derived from the size of the maps maintained in the
 * cylinder group and the (struct cg) size.
 */
#define	CGSIZE(fs) \
	/* base cg		*/ (sizeof (struct cg) + \
	/* blktot size	*/ (fs)->fs_cpg * sizeof (long) + \
	/* blks size	*/ (fs)->fs_cpg * (fs)->fs_nrpos * sizeof (short) + \
	/* inode map	*/ howmany((fs)->fs_ipg, NBBY) + \
	/* block map */ howmany((fs)->fs_cpg * (fs)->fs_spc / NSPF(fs), NBBY))

/*
 * We limit the size of the inode map to be no more than a
 * third of the cylinder group space, since we must leave at
 * least an equal amount of space for the block map.
 *
 * N.B.: MAXIpG must be a multiple of INOPB(fs).
 */
#define	MAXIpG(fs)	roundup((fs)->fs_bsize * NBBY / 3, INOPB(fs))

#define	UMASK		0755
#define	MAXINOPB	(MAXBSIZE / sizeof (struct dinode))
#define	POWEROF2(num)	(((num) & ((num) - 1)) == 0)
#define	MB		(1024*1024)

/*
 * Used to set the inode generation number. Since both inodes and dinodes
 * are dealt with, we really need a pointer to an icommon here.
 */
#define	IRANDOMIZE(icp)	(icp)->ic_gen = lrand48();

/*
 * Flags for number()
 */
#define	ALLOW_PERCENT	0x01	/* allow trailing `%' on number */
#define	ALLOW_MS1	0x02	/* allow trailing `ms', state 1 */
#define	ALLOW_MS2	0x04	/* allow trailing `ms', state 2 */
#define	ALLOW_END_ONLY	0x08	/* must be at end of number & suffixes */

/*
 * Forward declarations
 */
static void initcg(int cylno);
static void fsinit();
static int makedir(register struct direct *protodir, int entries);
static daddr32_t alloc(int size, int mode);
static void iput(register struct inode *ip);
static void rdfs(daddr32_t bno, int size, char *bf);
static void wtfs(daddr32_t bno, int size, char *bf);
static int isblock(struct fs *fs, unsigned char *cp, int h);
static void clrblock(struct fs *fs, unsigned char *cp, int h);
static void setblock(struct fs *fs, unsigned char *cp, int h);
static void usage();
static void dump_fscmd(char *fsys, int fsi);
static unsigned long number(long d_value, char *param, int flags);
static int match(char *s);
static char checkopt(char *optim);
static void range_check(long *varp, char *name, long minimum, long maximum,
    long def_val, int user_supplied);
static daddr32_t alloc(int size, int mode);
static long get_max_size(int fd);
static long get_max_track_size(int fd);

union {
	struct fs fs;
	char pad[SBSIZE];
} fsun;
#define	sblock	fsun.fs

struct	csum *fscs;

union {
	struct cg cg;
	char pad[MAXBSIZE];
} cgun;

#define	acg	cgun.cg
/*
 * Size of screen in cols in which to fit output
 */
#define	WIDTH	80

struct dinode zino[MAXBSIZE / sizeof (struct dinode)];

/*
 * file descriptors used for rdfs(fsi) and wtfs(fso).
 * Initialized to an illegal file descriptor number.
 */
int	fsi = -1;
int	fso = -1;

/*
 * The BIG parameter is machine dependent.  It should be a long integer
 * constant that can be used by the number parser to check the validity
 * of numeric parameters.
 */

#define	BIG		LONG_MAX

/* Used to indicate to number() that a bogus value should cause us to exit */
#define	NO_DEFAULT	LONG_MIN

/*
 * The *_flag variables are used to indicate that the user specified
 * the values, rather than that we made them up ourselves.  We can
 * complain about the user giving us bogus values.
 */

/* semi-constants */
long	sectorsize = DEV_BSIZE;		/* bytes/sector from param.h */
long	bbsize = BBSIZE;		/* boot block size */
long	sbsize = SBSIZE;		/* superblock size */

/* parameters */
long	fssize;				/* file system size in blocks */
long	cpg;				/* cylinders/cylinder group */
int	cpg_flag = RC_DEFAULT;
long	rotdelay = -1;			/* rotational delay between blocks */
int	rotdelay_flag = RC_DEFAULT;
long	maxcontig;			/* max contiguous blocks to allocate */
int	maxcontig_flag = RC_DEFAULT;
long	nsect = DFLNSECT;		/* sectors per track */
int	nsect_flag = RC_DEFAULT;
long	ntrack = DFLNTRAK;		/* tracks per cylinder group */
int	ntrack_flag = RC_DEFAULT;
long	bsize = DESBLKSIZE;		/* filesystem block size */
int	bsize_flag = RC_DEFAULT;
long	fragsize = DESFRAGSIZE; 	/* filesystem fragment size */
int	fragsize_flag = RC_DEFAULT;
long	minfree = MINFREE; 		/* fs_minfree */
int	minfree_flag = RC_DEFAULT;
long	rps = DEFHZ;			/* revolutions/second of drive */
int	rps_flag = RC_DEFAULT;
long	nbpi = NBPI;			/* number of bytes per inode */
int	nbpi_flag = RC_DEFAULT;
long	nrpos = NRPOS;			/* number of rotational positions */
int	nrpos_flag = RC_DEFAULT;
long	apc = 0;			/* alternate sectors per cylinder */
int	apc_flag = RC_DEFAULT;
char	opt = 't';			/* optimization style, `t' or `s' */

long	debug = 0;			/* enable debugging output */

/* global state */
int	Nflag;		/* do not write to disk */
int	mflag;		/* return the command line used to create this FS */
char	*fsys;
time_t	mkfstime;
char	*string;
long	maxtrax = DEF_MAXTRAX;		/* usually over-ridden */

/*
 * logging support
 */
int	ismdd;
int	islog;
int	islogok;

/*
 * growfs globals and forward references
 */
int		grow;
int		ismounted;
int		bdevismounted;
char		*directory;
long		grow_fssize;
long		grow_fs_size;
long		grow_fs_ncg;
daddr32_t		grow_fs_csaddr;
long		grow_fs_cssize;
int		grow_fs_clean;
struct csum	*grow_fscs;
daddr32_t		grow_sifrag;
int		test;
int		testforce;
daddr32_t		testfrags;
int		inlockexit;

void		lockexit();
void		randomgeneration();
void		checksummarysize();
void		checksblock();
void		growinit(char *);
void		checkdev();
void		checkmount();
struct dinode	*gdinode();
int		csfraginrange();
struct csfrag	*findcsfrag();
void		checkindirect(ino_t, daddr32_t *, daddr32_t, int);
void		addcsfrag();
void		delcsfrag();
void		checkdirect(ino_t, daddr32_t *, daddr32_t *, int);
void		findcsfragino();
void		fixindirect(daddr32_t, int);
void		fixdirect(caddr_t, daddr32_t, daddr32_t *, int);
void		fixcsfragino();
void		extendsummaryinfo();
int		notenoughspace();
void		unalloccsfragino();
void		unalloccsfragfree();
void		findcsfragfree();
void		copycsfragino();
void		rdcg();
void		wtcg();
void		flcg();
void		allocfrags();
void		alloccsfragino();
void		alloccsfragfree();
void		freefrags();
int		findfreerange();
void		resetallocinfo();
void		extendcg();
void		ulockfs();
void		wlockfs();
void		clockfs();
void		wtsb();

void
main(argc, argv)
	int argc;
	char *argv[];
{
	long i, mincpc, mincpg, inospercg, ibpcl;
	long cylno, rpos, blk, j, warn = 0;
	long used, mincpgcnt, bpcg, maxcpg;
	long mapcramped, inodecramped;
	long postblsize, rotblsize, totalsbsize;
	FILE *mnttab;
	struct mnttab mntp;
	char *special;
	struct stat64 statarea;
	struct ustat ustatarea;
	struct dk_cinfo dkcinfo;
	char pbuf[sizeof (unsigned int) * 3 + 1];
	int width, plen;
	unsigned int num;
	int spc_flag = 0;
	int c, saverr;
	long max_fssize;
	long tmpmaxcontig = -1;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "F:bmo:VGM:T:t:")) != EOF) {
		switch (c) {

		case 'F':
			string = optarg;
			if (strcmp(string, "ufs") != 0)
				usage();
			break;

		case 'm':	/* return command line used to create this FS */
			mflag++;
			break;

		case 'o':
			/*
			 * ufs specific options.
			 */
			string = optarg;
			while (*string != '\0') {
				if (match("nsect=")) {
					nsect = number(DFLNSECT, "nsect", 0);
					nsect_flag = RC_KEYWORD;
				} else if (match("ntrack=")) {
					ntrack = number(DFLNTRAK, "ntrack", 0);
					ntrack_flag = RC_KEYWORD;
				} else if (match("bsize=")) {
					bsize = number(DESBLKSIZE, "bsize", 0);
					bsize_flag = RC_KEYWORD;
				} else if (match("fragsize=")) {
					fragsize = number(DESFRAGSIZE,
					    "fragsize", 0);
					fragsize_flag = RC_KEYWORD;
				} else if (match("cgsize=")) {
					cpg = number(DESCPG, "cgsize", 0);
					cpg_flag = RC_KEYWORD;
				} else if (match("free=")) {
					minfree = number(MINFREE, "free",
					    ALLOW_PERCENT);
					minfree_flag = RC_KEYWORD;
				} else if (match("maxcontig=")) {
					tmpmaxcontig =
					    number(MAXCONTIG, "maxcontig", 0);
					maxcontig_flag = RC_KEYWORD;
				} else if (match("nrpos=")) {
					nrpos = number(NRPOS, "nrpos", 0);
					nrpos_flag = RC_KEYWORD;
				} else if (match("rps=")) {
					rps = number(DEFHZ, "rps", 0);
					rps_flag = RC_KEYWORD;
				} else if (match("nbpi=")) {
					nbpi = number(NBPI, "nbpi", 0);
					nbpi_flag = RC_KEYWORD;
				} else if (match("opt=")) {
					opt = checkopt(string);
				} else if (match("apc=")) {
					apc = number(0, "apc", 0);
					apc_flag = RC_KEYWORD;
				} else if (match("gap=")) {
					rotdelay = number(ROTDELAY, "gap",
					    ALLOW_MS1);
					rotdelay_flag = RC_KEYWORD;
				} else if (match("debug=")) {
					debug = number(0, "debug", 0);
				} else if (match("N")) {
					Nflag++;
				} else if (*string == '\0') {
					break;
				} else {
					(void) fprintf(stderr, gettext(
						"illegal option: %s\n"),
						string);
					usage();
				}

				if (*string == ',') string++;
				if (*string == ' ') string++;
			}
			break;

		case 'V':
			{
				char	*opt_text;
				int	opt_count;

				(void) fprintf(stdout, gettext("mkfs -F ufs "));
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

		case 'b':	/* do nothing for this */
			break;

		case 'M':	/* grow the mounted file system */
			directory = optarg;

			/* FALLTHROUGH */
		case 'G':	/* grow the file system */
			grow = 1;
			break;

		case 'T':	/* For testing */
			testforce = 1;

			/* FALLTHROUGH */
		case 't':
			test = 1;
			string = optarg;
			testfrags = number(NO_DEFAULT, "testfrags", 0);
			break;

		case '?':
			usage();
			break;
		}
	}
	(void) time(&mkfstime);
	if (optind >= (argc - 1)) {
		if (optind > (argc - 1)) {
			(void) fprintf(stderr,
			    gettext("special not specified\n"));
		} else {
			(void) fprintf(stderr,
			    gettext("size not specified\n"));
		}
		usage();
	}
	argc -= optind;
	argv = &argv[optind];

	fsys = argv[0];
	fsi = open64(fsys, O_RDONLY);
	if (fsi < 0) {
		(void) fprintf(stderr, gettext("%s: cannot open\n"), fsys);
		lockexit(32);
	}

	max_fssize = get_max_size(fsi);

	/*
	 * Get and check positional arguments, if any.
	 */
	switch (argc - 1) {
	default:
		usage();
		/*NOTREACHED*/
	case 14:
		string = argv[14];
		tmpmaxcontig = number(MAXCONTIG, "maxcontig", 0);
		maxcontig_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 13:
		string = argv[13];
		nrpos = number(NRPOS, "nrpos", 0);
		nrpos_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 12:
		string = argv[12];
		rotdelay = number(ROTDELAY, "gap", ALLOW_MS1);
		rotdelay_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 11:
		string = argv[11];
		apc = number(0, "apc", 0);
		apc_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 10:
		opt = checkopt(argv[10]);
		/* FALLTHROUGH */
	case 9:
		string = argv[9];
		nbpi = number(NBPI, "nbpi", 0);
		nbpi_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 8:
		string = argv[8];
		rps = number(DEFHZ, "rps", 0);
		rps_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 7:
		string = argv[7];
		minfree = number(MINFREE, "free", ALLOW_PERCENT);
		minfree_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 6:
		string = argv[6];
		cpg = number(DESCPG, "cgsize", 0);
		cpg_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 5:
		string = argv[5];
		fragsize = number(DESFRAGSIZE, "fragsize", 0);
		fragsize_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 4:
		string = argv[4];
		bsize = number(DESBLKSIZE, "bsize", 0);
		bsize_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 3:
		string = argv[3];
		ntrack = number(DFLNTRAK, "ntrack", 0);
		ntrack_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 2:
		string = argv[2];
		nsect = number(DFLNSECT, "nsect", 0);
		nsect_flag = RC_POSITIONAL;
		/* FALLTHROUGH */
	case 1:
		string = argv[1];
		fssize = number(max_fssize, "size", 0);
	}

	maxtrax = get_max_track_size(fsi);

	if ((maxcontig_flag == RC_DEFAULT) || (tmpmaxcontig == -1)) {
		maxcontig = maxtrax / bsize;
	} else {
		maxcontig = tmpmaxcontig;
	}

	if (rotdelay == -1) {	/* default by newfs and mkfs */
		rotdelay = ROTDELAY;
	}

	if (cpg_flag == RC_DEFAULT) { /* If not explicity set, use default */
		cpg = DESCPG;
	}

	/*
	 * Now that we have the semi-sane args, either positional, via -o,
	 * or by defaulting, handle inter-dependencies and range checks.
	 */
	range_check(&fssize, "size", 1024, max_fssize, max_fssize, 1);
	range_check(&rotdelay, "gap", 0, 1000, ROTDELAY, rotdelay_flag);

	/*
	 * 32K based on max block size of 64K, and rotational layout
	 * test of nsect <= (256 * sectors/block).  Current block size
	 * limit is not 64K, but it's growing soon.
	 */
	range_check(&nsect, "nsect", 1, 32768, DFLNSECT, nsect_flag);
	range_check(&maxcontig, "maxcontig", 0, nsect,
	    MIN(nsect, maxtrax / bsize), maxcontig_flag);

	range_check(&apc, "apc", 0, nsect - 1, 0, apc_flag);
	range_check(&bsize, "bsize", MINBSIZE, MAXBSIZE, DESBLKSIZE,
	    bsize_flag);

	if (!POWEROF2(bsize)) {
		(void) fprintf(stderr,
		    gettext("block size must be a power of 2, not %d\n"),
		    bsize);
		bsize = DESBLKSIZE;
		(void) fprintf(stderr,
		    gettext("mkfs: bsize reset to default %ld\n"),
		    bsize);
	}

	range_check(&fragsize, "fragsize", sectorsize, bsize,
	    MAX(bsize / MAXFRAG, MIN(DESFRAGSIZE, bsize)), fragsize_flag);

	if ((bsize / MAXFRAG) > fragsize) {
		(void) fprintf(stderr, gettext(
"fragment size %d is too small, minimum with block size %d is %d\n"),
		    fragsize, bsize, bsize / MAXFRAG);
		(void) fprintf(stderr,
		    gettext("mkfs: fragsize reset to minimum %d\n"),
		    bsize / MAXFRAG);
		fragsize = bsize / MAXFRAG;
	}

	if (!POWEROF2(fragsize)) {
		(void) fprintf(stderr,
		    gettext("fragment size must be a power of 2, not %d\n"),
		    fragsize);
		fragsize = MAX(bsize / MAXFRAG, MIN(DESFRAGSIZE, bsize));
		(void) fprintf(stderr,
		    gettext("mkfs: fragsize reset to %ld\n"),
		    fragsize);
	}

	/* At this point, bsize must be >= fragsize, so no need to check it */

	if (bsize < PAGESIZE) {
		(void) fprintf(stderr, gettext(
		    "WARNING: filesystem block size (%ld) is smaller than "
		    "memory page size (%ld).\nResulting filesystem can not be "
		    "mounted on this system.\n\n"),
		    bsize, (long)PAGESIZE);
	}

	range_check(&rps, "rps", 1, 1000, DEFHZ, rps_flag);
	range_check(&minfree, "free", 0, 99, MINFREE, minfree_flag);
	range_check(&nrpos, "nrpos", 1, nsect, MIN(nsect, NRPOS), nrpos_flag);

	/*
	 * ntrack is the number of tracks per cylinder.
	 * The ntrack value must be between 1 and the total number of
	 * sectors in the file system.
	 */
	range_check(&ntrack, "ntrack", 1, fssize, DFLNTRAK, ntrack_flag);

	/*
	 * nbpi is variable, but 2MB seems a reasonable upper limit,
	 * as 4MB tends to cause problems (using otherwise-default
	 * parameters).  The true limit is where we end up with one
	 * inode per cylinder group.
	 */
	range_check(&nbpi, "nbpi", DEV_BSIZE, 2 * MB, NBPI, nbpi_flag);

	/*
	 * maxcpg is another variably-limited parameter.  Calculate
	 * the limit based on what we've got for its dependent
	 * variables.  Effectively, it's how much space is left in the
	 * superblock after all the other bits are accounted for.  We
	 * only fill in sblock fields so we can use MAXIpG.
	 *
	 * If the calculation of maxcpg is changed, update newfs as well.
	 */
	sblock.fs_bsize = bsize;
	sblock.fs_inopb = sblock.fs_bsize / sizeof (struct dinode);
	maxcpg = (bsize - sizeof (struct cg) - howmany(MAXIpG(&sblock), NBBY)) /
	    (sizeof (long) + nrpos * sizeof (short) + nsect / (MAXFRAG * NBBY));
	/*
	 * mincpg is variable in complex ways, so we really can't
	 * do a sane lower-end limit check at this point.
	 */
	range_check(&cpg, "cgsize", 1, maxcpg, MIN(maxcpg, DESCPG), cpg_flag);

	if (mflag) {
		dump_fscmd(fsys, fsi);
		lockexit(0);
	}

	/*
	 * get the controller info
	 */
	ismdd = 0;
	islog = 0;
	islogok = 0;
	if (ioctl(fsi, DKIOCINFO, &dkcinfo) == 0)
		/*
		 * if it is an MDD (disksuite) device
		 */
		if (dkcinfo.dki_ctype == DKC_MD) {
			ismdd++;
			/*
			 * check the logging device
			 */
			if (ioctl(fsi, _FIOISLOG, NULL) == 0) {
				islog++;
				if (ioctl(fsi, _FIOISLOGOK, NULL) == 0)
					islogok++;
			}
		}

	if (!Nflag) {
		special = getfullblkname(fsys);
		if (grow)
			checkdev(fsys, special);

		/*
		 * If we found the block device name,
		 * then check the mount table.
		 * if mounted, and growing write lock the file system
		 *
		 */
		if ((special != NULL) && (*special != '\0')) {
			mnttab = fopen(MNTTAB, "r");
			while ((getmntent(mnttab, &mntp)) == NULL) {
				if (grow) {
					checkmount(&mntp, special);
					continue;
				}
				if (strcmp(special, mntp.mnt_special) == 0) {
					(void) fprintf(stderr, gettext(
					    "%s is mounted, can't mkfs\n"),
					    special);
					exit(32);
				}
			}
			(void) fclose(mnttab);
		}
		if ((bdevismounted) && (ismounted == 0)) {
			(void) fprintf(stderr, gettext(
	    "can't check mount point; %s is mounted but not in mnttab(4)\n"),
			    special);
			lockexit(32);
		}
		if (directory) {
			if (ismounted == 0) {
				(void) fprintf(stderr,
				    gettext("%s is not mounted\n"),
				    special);
				lockexit(32);
			}
			wlockfs();
		}
		fso = (grow) ? open64(fsys, O_WRONLY) : creat64(fsys, 0666);
		if (fso < 0) {
			saverr = errno;
			(void) fprintf(stderr,
			    gettext("%s: cannot create: %s\n"),
			    fsys, strerror(saverr));
			lockexit(32);
		}
		if (!grow && stat64(fsys, &statarea) < 0) {
			saverr = errno;
			(void) fprintf(stderr, gettext("%s: cannot stat: %s\n"),
			    fsys, strerror(saverr));
			exit(32);
		}
		if (!grow && ((statarea.st_mode & S_IFMT) != S_IFBLK) &&
			((statarea.st_mode & S_IFMT) != S_IFCHR)) {
			(void) fprintf(stderr, gettext(
				"%s is not special device:%x, can't mkfs\n"),
				fsys, statarea.st_mode);
			exit(32);
		}
		if (!grow && ustat(statarea.st_rdev, &ustatarea) >= 0) {
			(void) fprintf(stderr,
				gettext("%s is mounted, can't mkfs\n"), fsys);
			exit(32);
		}
	} else {

		/*
		 * For the -N case, a file descriptor is needed for the llseek()
		 * in wtfs(). See the comment in wtfs() for more information.
		 *
		 * Get a file descriptor that's read-only so that this code
		 * doesn't accidentally write to the file.
		 */
		fso = open64(fsys, O_RDONLY);
		if (fso < 0) {
			saverr = errno;
			(void) fprintf(stderr, gettext("%s: cannot open: %s\n"),
			    fsys, strerror(saverr));
			lockexit(32);
		}
	}

	/*
	 * seed random # generator (for ic_generation)
	 */
	srand48((long)(time((time_t *)NULL) + getpid()));

	if (grow) {
		growinit(fsys);
		goto grow00;
	}

	/*
	 * Validate the given file system size.
	 * Verify that its last block can actually be accessed.
	 *
	 * Note: it's ok to use sblock for the data because it is overwritten
	 * by the rdfs() of the superblock in the code following this if-block.
	 *
	 * ToDo: Because the size checking is done in rdfs()/wtfs(), the
	 * error message for specifying an illegal size is very unfriendly.
	 * In the future, one could replace the rdfs()/wtfs() calls
	 * below with in-line calls to read() or write(). This allows better
	 * error messages to be put in place.
	 */
	rdfs(fssize - 1, sectorsize, (char *)&sblock);

	/*
	 * make the fs unmountable
	 */
	rdfs(SBOFF / sectorsize, sbsize, (char *)&sblock);
	sblock.fs_magic = -1;
	sblock.fs_clean = FSBAD;
	sblock.fs_state = FSOKAY - sblock.fs_time;
	wtfs(SBOFF / sectorsize, sbsize, (char *)&sblock);
	bzero(&sblock, sbsize);

	sblock.fs_nsect = nsect;
	sblock.fs_ntrak = ntrack;

	/*
	 * Validate specified/determined spc
	 * and calculate minimum cylinders per group.
	 */

	/*
	 * sectors/cyl = tracks/cyl * sectors/track
	 */
	sblock.fs_spc = sblock.fs_ntrak * sblock.fs_nsect;

grow00:
	if (apc_flag) {
		sblock.fs_spc -= apc;
	}
	/*
	 * Have to test for this separately from apc_flag, due to
	 * the growfs case....
	 */
	if (sblock.fs_spc != sblock.fs_ntrak * sblock.fs_nsect) {
		spc_flag = 1;
	}
	if (grow)
		goto grow10;

	sblock.fs_nrpos = nrpos;
	sblock.fs_bsize = bsize;
	sblock.fs_fsize = fragsize;
	sblock.fs_minfree = minfree;

grow10:
	if (nbpi < sblock.fs_fsize) {
		(void) fprintf(stderr, gettext(
		"warning: wasteful data byte allocation / inode (nbpi):\n"));
		(void) fprintf(stderr, gettext(
		    "%d smaller than allocatable fragment size of %d\n"),
		    nbpi, sblock.fs_fsize);
	}
	if (grow)
		goto grow20;

	if (opt == 's')
		sblock.fs_optim = FS_OPTSPACE;
	else
		sblock.fs_optim = FS_OPTTIME;

	sblock.fs_bmask = ~(sblock.fs_bsize - 1);
	sblock.fs_fmask = ~(sblock.fs_fsize - 1);
	/*
	 * Planning now for future expansion.
	 */
#if defined(_BIG_ENDIAN)
		sblock.fs_qbmask.val[0] = 0;
		sblock.fs_qbmask.val[1] = ~sblock.fs_bmask;
		sblock.fs_qfmask.val[0] = 0;
		sblock.fs_qfmask.val[1] = ~sblock.fs_fmask;
#endif
#if defined(_LITTLE_ENDIAN)
		sblock.fs_qbmask.val[0] = ~sblock.fs_bmask;
		sblock.fs_qbmask.val[1] = 0;
		sblock.fs_qfmask.val[0] = ~sblock.fs_fmask;
		sblock.fs_qfmask.val[1] = 0;
#endif
	for (sblock.fs_bshift = 0, i = sblock.fs_bsize; i > 1; i >>= 1)
		sblock.fs_bshift++;
	for (sblock.fs_fshift = 0, i = sblock.fs_fsize; i > 1; i >>= 1)
		sblock.fs_fshift++;
	sblock.fs_frag = numfrags(&sblock, sblock.fs_bsize);
	for (sblock.fs_fragshift = 0, i = sblock.fs_frag; i > 1; i >>= 1)
		sblock.fs_fragshift++;
	if (sblock.fs_frag > MAXFRAG) {
		(void) fprintf(stderr, gettext(
	"fragment size %d is too small, minimum with block size %d is %d\n"),
		    sblock.fs_fsize, sblock.fs_bsize,
		    sblock.fs_bsize / MAXFRAG);
		lockexit(32);
	}
	sblock.fs_nindir = sblock.fs_bsize / sizeof (daddr32_t);
	sblock.fs_inopb = sblock.fs_bsize / sizeof (struct dinode);
	sblock.fs_nspf = sblock.fs_fsize / sectorsize;
	for (sblock.fs_fsbtodb = 0, i = NSPF(&sblock); i > 1; i >>= 1)
		sblock.fs_fsbtodb++;

	/*
	 * Compute the super-block, cylinder group, and inode blocks.
	 * Note that these "blkno" are really fragment addresses.
	 * For example, on an 8K/1K (block/fragment) system, fs_sblkno is 16,
	 * fs_cblkno is 24, and fs_iblkno is 32. This is why CGSIZE is so
	 * important: only 1 FS block is allocated for the cg struct (fragment
	 * numbers 24 through 31).
	 */
	sblock.fs_sblkno =
	    roundup(howmany(bbsize + sbsize, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_cblkno = (daddr32_t)(sblock.fs_sblkno +
	    roundup(howmany(sbsize, sblock.fs_fsize), sblock.fs_frag));
	sblock.fs_iblkno = sblock.fs_cblkno + sblock.fs_frag;

	sblock.fs_cgoffset = roundup(
	    howmany(sblock.fs_nsect, NSPF(&sblock)), sblock.fs_frag);
	for (sblock.fs_cgmask = -1, i = sblock.fs_ntrak; i > 1; i >>= 1)
		sblock.fs_cgmask <<= 1;
	if (!POWEROF2(sblock.fs_ntrak))
		sblock.fs_cgmask <<= 1;
	/*
	 * Validate specified/determined spc
	 * and calculate minimum cylinders per group.
	 */

	for (sblock.fs_cpc = NSPB(&sblock), i = sblock.fs_spc;
	    sblock.fs_cpc > 1 && (i & 1) == 0;
	    sblock.fs_cpc >>= 1, i >>= 1)
		/* void */;
	mincpc = sblock.fs_cpc;

	/* if these calculations are changed, check dump_fscmd also */
	bpcg = sblock.fs_spc * sectorsize;
	inospercg = roundup(bpcg / sizeof (struct dinode), INOPB(&sblock));
	if (inospercg > MAXIpG(&sblock))
		inospercg = MAXIpG(&sblock);
	used = (sblock.fs_iblkno + inospercg / INOPF(&sblock)) * NSPF(&sblock);
	mincpgcnt = howmany(sblock.fs_cgoffset * (~sblock.fs_cgmask) + used,
	    sblock.fs_spc);
	mincpg = roundup(mincpgcnt, mincpc);
	/*
	 * Insure that cylinder group with mincpg has enough space
	 * for block maps
	 */
	sblock.fs_cpg = mincpg;
	sblock.fs_ipg = inospercg;
	mapcramped = 0;

	/*
	 * Make sure the cg struct fits within the file system block.
	 * Use larger block sizes until it fits
	 */
	while (CGSIZE(&sblock) > sblock.fs_bsize) {
		mapcramped = 1;
		if (sblock.fs_bsize < MAXBSIZE) {
			sblock.fs_bsize <<= 1;
			if ((i & 1) == 0) {
				i >>= 1;
			} else {
				sblock.fs_cpc <<= 1;
				mincpc <<= 1;
				mincpg = roundup(mincpgcnt, mincpc);
				sblock.fs_cpg = mincpg;
			}
			sblock.fs_frag <<= 1;
			sblock.fs_fragshift += 1;
			if (sblock.fs_frag <= MAXFRAG)
				continue;
		}

		/*
		 * Looped far enough. The fragment is now as large as the
		 * filesystem block!
		 */
		if (sblock.fs_fsize == sblock.fs_bsize) {
			(void) fprintf(stderr, gettext(
		    "There is no block size that can support this disk\n"));
			lockexit(32);
		}

		/*
		 * Try a larger fragment. Double the fragment size.
		 */
		sblock.fs_frag >>= 1;
		sblock.fs_fragshift -= 1;
		sblock.fs_fsize <<= 1;
		sblock.fs_nspf <<= 1;
	}
	/*
	 * Insure that cylinder group with mincpg has enough space for inodes
	 */
	inodecramped = 0;
	used *= sectorsize;
	inospercg = roundup((mincpg * bpcg - used) / nbpi, INOPB(&sblock));
	sblock.fs_ipg = inospercg;
	while (inospercg > MAXIpG(&sblock)) {
		inodecramped = 1;
		if (mincpc == 1 || sblock.fs_frag == 1 ||
		    sblock.fs_bsize == MINBSIZE)
			break;
		(void) fprintf(stderr,
		    gettext("With a block size of %d %s %d\n"),
		    sblock.fs_bsize, gettext("minimum bytes per inode is"),
		    (mincpg * bpcg - used) / MAXIpG(&sblock) + 1);
		sblock.fs_bsize >>= 1;
		sblock.fs_frag >>= 1;
		sblock.fs_fragshift -= 1;
		mincpc >>= 1;
		sblock.fs_cpg = roundup(mincpgcnt, mincpc);
		if (CGSIZE(&sblock) > sblock.fs_bsize) {
			sblock.fs_bsize <<= 1;
			break;
		}
		mincpg = sblock.fs_cpg;
		inospercg =
		    roundup((mincpg * bpcg - used) / nbpi, INOPB(&sblock));
		sblock.fs_ipg = inospercg;
	}
	if (inodecramped) {
		if (inospercg > MAXIpG(&sblock)) {
			(void) fprintf(stderr, gettext(
			    "Minimum bytes per inode is %d\n"),
			    (mincpg * bpcg - used) / MAXIpG(&sblock) + 1);
		} else if (!mapcramped) {
			(void) fprintf(stderr, gettext(
	    "With %d bytes per inode, minimum cylinders per group is %d\n"),
			    nbpi, mincpg);
		}
	}
	if (mapcramped) {
		(void) fprintf(stderr, gettext(
		    "With %d sectors per cylinder, minimum cylinders "
		    "per group is %d\n"),
		    sblock.fs_spc, mincpg);
	}
	if (inodecramped || mapcramped) {
		/*
		 * To make this at least somewhat comprehensible in
		 * the world of i18n, figure out what we're going to
		 * say and then say it all at one time.  The days of
		 * needing to scrimp on string space are behind us....
		 */
		if ((sblock.fs_bsize != bsize) &&
		    (sblock.fs_fsize != fragsize)) {
			(void) fprintf(stderr, gettext(
	    "This requires the block size to be changed from %d to %d\n"
	    "and the fragment size to be changed from %d to %d\n"),
			    bsize, sblock.fs_bsize,
			    fragsize, sblock.fs_fsize);
		} else if (sblock.fs_bsize != bsize) {
			(void) fprintf(stderr, gettext(
	    "This requires the block size to be changed from %d to %d\n"),
			    bsize, sblock.fs_bsize);
		} else if (sblock.fs_fsize != fragsize) {
			(void) fprintf(stderr, gettext(
	    "This requires the fragment size to be changed from %d to %d\n"),
			    fragsize, sblock.fs_fsize);
		} else {
			(void) fprintf(stderr, gettext(
	    "Unable to make filesystem fit with the given constraints\n"));
		}
		(void) fprintf(stderr, gettext(
		    "Please re-run mkfs with corrected parameters\n"));
		lockexit(32);
	}
	/*
	 * Calculate the number of cylinders per group
	 */
	sblock.fs_cpg = cpg;
	if (sblock.fs_cpg % mincpc != 0) {
		if (cpg_flag) {
			(void) fprintf(stderr, gettext(
			    "Cylinder groups must have a multiple of %d "
			    "cylinders with the given parameters\n"),
			    mincpc);
		} else {
			(void) fprintf(stderr, gettext(
			    "Warning: cylinder groups must have a multiple "
			    "of %d cylinders with the given parameters\n"),
			    mincpc);
		}
		sblock.fs_cpg = roundup(sblock.fs_cpg, mincpc);
		(void) fprintf(stderr, gettext("Rounded cgsize up to %d\n"),
		    sblock.fs_cpg);
		if (!cpg_flag)
			cpg = sblock.fs_cpg;
	}
	/*
	 * Must insure there is enough space for inodes
	 */
	/* if these calculations are changed, check dump_fscmd also */
	sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / nbpi,
		INOPB(&sblock));

	/*
	 * Slim down cylinders per group, until the inodes can fit.
	 */
	while (sblock.fs_ipg > MAXIpG(&sblock)) {
		inodecramped = 1;
		sblock.fs_cpg -= mincpc;
		sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / nbpi,
			INOPB(&sblock));
	}
	/*
	 * Must insure there is enough space to hold block map.
	 * Cut down on cylinders per group, until the cg struct fits in a
	 * filesystem block.
	 */
	while (CGSIZE(&sblock) > sblock.fs_bsize) {
		mapcramped = 1;
		sblock.fs_cpg -= mincpc;
		sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / nbpi,
			INOPB(&sblock));
	}
	sblock.fs_fpg = (sblock.fs_cpg * sblock.fs_spc) / NSPF(&sblock);
	if ((sblock.fs_cpg * sblock.fs_spc) % NSPB(&sblock) != 0) {
		(void) fprintf(stderr,
		gettext("newfs: panic (fs_cpg * fs_spc) %% NSPF != 0\n"));
		lockexit(32);
	}
	if (sblock.fs_cpg < mincpg) {
		(void) fprintf(stderr, gettext(
"With the given parameters, cgsize must be at least %d; please re-run mkfs\n"),
			mincpg);
		lockexit(32);
	} else if (sblock.fs_cpg != cpg && cpg_flag &&
		!mapcramped && !inodecramped) {
			lockexit(32);
	}
	sblock.fs_cgsize = fragroundup(&sblock, CGSIZE(&sblock));
grow20:
	/*
	 * Now have size for file system and nsect and ntrak.
	 * Determine number of cylinders and blocks in the file system.
	 */
	sblock.fs_size = fssize = dbtofsb(&sblock, fssize);
	sblock.fs_ncyl = fssize * NSPF(&sblock) / sblock.fs_spc;
	if (fssize * NSPF(&sblock) > sblock.fs_ncyl * sblock.fs_spc) {
		sblock.fs_ncyl++;
		warn = 1;
	}
	if (sblock.fs_ncyl < 1) {
		(void) fprintf(stderr, gettext(
			"file systems must have at least one cylinder\n"));
		lockexit(32);
	}
	if (grow)
		goto grow30;
	/*
	 * Determine feasability/values of rotational layout tables.
	 *
	 * The size of the rotational layout tables is limited by the size
	 * of the file system block, fs_bsize.  The amount of space
	 * available for tables is calculated as (fs_bsize - sizeof (struct
	 * fs)).  The size of these tables is inversely proportional to the
	 * block size of the file system. The size increases if sectors per
	 * track are not powers of two, because more cylinders must be
	 * described by the tables before the rotational pattern repeats
	 * (fs_cpc).
	 */
	sblock.fs_postblformat = FS_DYNAMICPOSTBLFMT;
	sblock.fs_sbsize = fragroundup(&sblock, sizeof (struct fs));
	sblock.fs_npsect = sblock.fs_nsect;
	sblock.fs_interleave = 1;
	if (sblock.fs_ntrak == 1) {
		sblock.fs_cpc = 0;
		goto next;
	}
	postblsize = sblock.fs_nrpos * sblock.fs_cpc * sizeof (short);
	rotblsize = sblock.fs_cpc * sblock.fs_spc / NSPB(&sblock);
	totalsbsize = sizeof (struct fs) + rotblsize;

	/* do static allocation if nrpos == 8 and fs_cpc == 16  */
	if (sblock.fs_nrpos == 8 && sblock.fs_cpc <= 16) {
		/* use old static table space */
		sblock.fs_postbloff = (char *)(&sblock.fs_opostbl[0][0]) -
		    (char *)(&sblock.fs_link);
		sblock.fs_rotbloff = &sblock.fs_space[0] -
		    (uchar_t *)(&sblock.fs_link);
	} else {
		/* use 4.3 dynamic table space */
		sblock.fs_postbloff = &sblock.fs_space[0] -
		    (uchar_t *)(&sblock.fs_link);
		sblock.fs_rotbloff = sblock.fs_postbloff + postblsize;
		totalsbsize += postblsize;
	}
	if (totalsbsize > sblock.fs_bsize ||
	    sblock.fs_nsect > (1 << NBBY) * NSPB(&sblock)) {
		(void) fprintf(stderr, gettext(
		    "Warning: insufficient space in super block for\n"
		    "rotational layout tables with nsect %ld, ntrack %ld, "
		    "and nrpos %ld.\nOmitting tables - file system "
		    "performance may be impaired.\n"),
		    sblock.fs_nsect, sblock.fs_ntrak, sblock.fs_nrpos);

		/*
		 * Setting fs_cpc to 0 tells alloccgblk() in ufs_alloc.c to
		 * ignore the positional layout table and rotational
		 * position table.
		 */
		sblock.fs_cpc = 0;
		goto next;
	}
	sblock.fs_sbsize = fragroundup(&sblock, totalsbsize);


	/*
	 * calculate the available blocks for each rotational position
	 */
	for (cylno = 0; cylno < sblock.fs_cpc; cylno++)
		for (rpos = 0; rpos < sblock.fs_nrpos; rpos++)
			fs_postbl(&sblock, cylno)[rpos] = -1;
	for (i = (rotblsize - 1) * sblock.fs_frag;
	    i >= 0; i -= sblock.fs_frag) {
		cylno = cbtocylno(&sblock, i);
		rpos = cbtorpos(&sblock, i);
		blk = fragstoblks(&sblock, i);
		if (fs_postbl(&sblock, cylno)[rpos] == -1)
			fs_rotbl(&sblock)[blk] = 0;
		else
			fs_rotbl(&sblock)[blk] =
			    fs_postbl(&sblock, cylno)[rpos] - blk;
		fs_postbl(&sblock, cylno)[rpos] = blk;
	}
next:
grow30:
	/*
	 * Compute/validate number of cylinder groups.
	 * Note that if an excessively large filesystem is specified
	 * (e.g., more than 16384 cylinders for an 8K filesystem block), it
	 * does not get detected until checksummarysize()
	 */
	sblock.fs_ncg = sblock.fs_ncyl / sblock.fs_cpg;
	if (sblock.fs_ncyl % sblock.fs_cpg)
		sblock.fs_ncg++;
	sblock.fs_dblkno = sblock.fs_iblkno + sblock.fs_ipg / INOPF(&sblock);
	i = MIN(~sblock.fs_cgmask, sblock.fs_ncg - 1);
	ibpcl = cgdmin(&sblock, i) - cgbase(&sblock, i);
	if (ibpcl >= sblock.fs_fpg) {
		(void) fprintf(stderr, gettext(
		    "inode blocks/cyl group (%d) >= data blocks (%d)\n"),
		    cgdmin(&sblock, i) - cgbase(&sblock, i) / sblock.fs_frag,
		    sblock.fs_fpg / sblock.fs_frag);
		if ((ibpcl < 0) || (sblock.fs_fpg < 0)) {
			(void) fprintf(stderr, gettext(
	    "number of cylinders per cylinder group (%d) must be decreased.\n"),
			    sblock.fs_cpg);
		} else {
			(void) fprintf(stderr, gettext(
	    "number of cylinders per cylinder group (%d) must be increased.\n"),
			    sblock.fs_cpg);
		}
		(void) fprintf(stderr, gettext(
"Note that cgsize may have been adjusted to allow struct cg to fit.\n"));
		lockexit(32);
	}
	j = sblock.fs_ncg - 1;
	if ((i = fssize - j * sblock.fs_fpg) < sblock.fs_fpg &&
	    cgdmin(&sblock, j) - cgbase(&sblock, j) > i) {
		(void) fprintf(stderr, gettext(
		    "Warning: inode blocks/cyl group (%d) >= data "
		    "blocks (%d) in last\n    cylinder group. This "
		    "implies %d sector(s) cannot be allocated.\n"),
		    (cgdmin(&sblock, j) - cgbase(&sblock, j)) / sblock.fs_frag,
		    i / sblock.fs_frag, i * NSPF(&sblock));
		sblock.fs_ncg--;
		sblock.fs_ncyl -= sblock.fs_ncyl % sblock.fs_cpg;
		sblock.fs_size = fssize = sblock.fs_ncyl * sblock.fs_spc /
		    NSPF(&sblock);
		warn = 0;
	}
	if (warn && !spc_flag) {
		(void) fprintf(stderr, gettext(
		    "Warning: %d sector(s) in last cylinder unallocated\n"),
		    sblock.fs_spc -
		    (fssize * NSPF(&sblock) - (sblock.fs_ncyl - 1)
		    * sblock.fs_spc));
	}
	/*
	 * fill in remaining fields of the super block
	 */

	/*
	 * The csum records are stored in cylinder group 0, starting at
	 * cgdmin, the first data block.
	 */
	sblock.fs_csaddr = cgdmin(&sblock, 0);
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof (struct csum));
	i = sblock.fs_bsize / sizeof (struct csum);
	sblock.fs_csmask = ~(i - 1);
	for (sblock.fs_csshift = 0; i > 1; i >>= 1)
		sblock.fs_csshift++;
	fscs = (struct csum *)calloc(1, sblock.fs_cssize);

	checksummarysize();
	if (grow) {
		bcopy((caddr_t)grow_fscs, (caddr_t)fscs, (int)grow_fs_cssize);
		extendsummaryinfo();
		goto grow40;
	}
	sblock.fs_magic = FS_MAGIC;
	sblock.fs_rotdelay = rotdelay;
	sblock.fs_maxcontig = maxcontig;
	sblock.fs_maxbpg = MAXBLKPG(sblock.fs_bsize);

	sblock.fs_rps = rps;
	sblock.fs_cgrotor = 0;
	sblock.fs_cstotal.cs_ndir = 0;
	sblock.fs_cstotal.cs_nbfree = 0;
	sblock.fs_cstotal.cs_nifree = 0;
	sblock.fs_cstotal.cs_nffree = 0;
	sblock.fs_fmod = 0;
	sblock.fs_ronly = 0;
	sblock.fs_time = mkfstime;
	sblock.fs_state = FSOKAY - sblock.fs_time;
	sblock.fs_clean = FSCLEAN;
grow40:

	/*
	 * Dump out summary information about file system.
	 */
	(void) fprintf(stderr, gettext(
	    "%s:\t%d sectors in %d cylinders of %d tracks, %d sectors\n"),
	    fsys, sblock.fs_size * NSPF(&sblock), sblock.fs_ncyl,
	    sblock.fs_ntrak, sblock.fs_nsect);
	(void) fprintf(stderr, gettext(
	    "\t%.1fMB in %d cyl groups (%d c/g, %.2fMB/g, %d i/g)\n"),
	    (float)sblock.fs_size * sblock.fs_fsize / MB, sblock.fs_ncg,
	    sblock.fs_cpg, (float)sblock.fs_fpg * sblock.fs_fsize / MB,
	    sblock.fs_ipg);
	/*
	 * Now build the cylinders group blocks and
	 * then print out indices of cylinder groups.
	 */
	(void) fprintf(stderr, gettext(
	    "super-block backups (for fsck -F ufs -o b=#) at:\n"));
	for (width = cylno = 0; cylno < sblock.fs_ncg; cylno++) {
		if ((grow == 0) || (cylno >= grow_fs_ncg))
			initcg(cylno);
		num = fsbtodb(&sblock, cgsblock(&sblock, cylno));
		(void) sprintf(pbuf, " %u,", num);
		plen = strlen(pbuf);
		if ((width + plen) > (WIDTH - 1)) {
			width = plen;
			(void) fprintf(stderr, "\n");
		} else {
			width += plen;
		}
		(void) fprintf(stderr, "%s", pbuf);
	}
	(void) fprintf(stderr, "\n");
	if (Nflag)
		lockexit(0);
	if (grow)
		goto grow50;

	/*
	 * Now construct the initial file system,
	 * then write out the super-block.
	 */
	fsinit();
grow50:
	/*
	 * write the superblock and csum information
	 */
	wtsb();

	/*
	 * extend the last cylinder group in the original file system
	 */
	if (grow) {
		extendcg(grow_fs_ncg-1);
		wtsb();
	}

	/*
	 * Write out the duplicate super blocks
	 */
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
		wtfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    sbsize, (char *)&sblock);

	/*
	 * set clean flag
	 */
	if (grow)
		sblock.fs_clean = grow_fs_clean;
	else
		sblock.fs_clean = FSCLEAN;
	sblock.fs_time = mkfstime;
	sblock.fs_state = FSOKAY - sblock.fs_time;
	wtfs(SBOFF / sectorsize, sbsize, (char *)&sblock);

	if (ismdd && islog && !islogok)
		(void) ioctl(fso, _FIOLOGRESET, NULL);

	if (fsync(fso) == -1) {
		saverr = errno;
		(void) fprintf(stderr,
		    gettext("mkfs: fsync failed on write disk: %s\n"),
		    strerror(saverr));
		/* we're just cleaning up, so keep going */
	}
	if (close(fsi) == -1) {
		saverr = errno;
		(void) fprintf(stderr,
		    gettext("mkfs: close failed on read disk: %s\n"),
		    strerror(saverr));
		/* we're just cleaning up, so keep going */
	}
	if (close(fso) == -1) {
		(void) fprintf(stderr,
		    gettext("mkfs: close failed on write disk: %s\n"),
		    strerror(saverr));
		/* we're just cleaning up, so keep going */
	}
	fsi = fso = -1;

#ifndef STANDALONE
	lockexit(0);
#endif
}

/*
 * Figure out how big the partition we're dealing with is.
 */
static long
get_max_size(int fd)
{
	struct vtoc vtoc;
	int index = read_vtoc(fd, &vtoc);

	if (index < 0) {
		switch (index) {
		case VT_ERROR:
			break;
		case VT_EIO:
			errno = EIO;
			break;
		case VT_EINVAL:
			errno = EINVAL;
		}
		perror(gettext("Can not determine partition size"));
		lockexit(32);
	}
	if (debug) {
		(void) fprintf(stderr,
		    "get_max_size: index = %d, p_size = %ld, dolimit = %d\n",
		    index, vtoc.v_part[index].p_size,
		    (vtoc.v_part[index].p_size < 0) ||
			(vtoc.v_part[index].p_size > INT_MAX));
	}
	/* ufs is limited to no more than int32_t sectors */
	if (vtoc.v_part[index].p_size > INT_MAX)
		return (INT_MAX);
	if (vtoc.v_part[index].p_size < 0)
		return (INT_MAX);
	return (vtoc.v_part[index].p_size);
}

static long
get_max_track_size(int fd)
{
	struct dk_cinfo ci;
	long track_size = 56 * 1024;	/* worst-case, traditional value */

	if (ioctl(fd, DKIOCINFO, &ci) == 0) {
		track_size = ci.dki_maxtransfer * DEV_BSIZE;
		if ((track_size < 0) || (track_size > MB))
			track_size = MB;
	}

	return (track_size);
}

/*
 * Initialize a cylinder group.
 */
static void
initcg(int cylno)
{
	daddr32_t cbase, d, dlower, dupper, dmax;
	long i;
	register struct csum *cs;

	/*
	 * Determine block bounds for cylinder group.
	 * Allow space for super block summary information in first
	 * cylinder group.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0)
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	cs = fscs + cylno;
	acg.cg_time = mkfstime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	if (cylno == sblock.fs_ncg - 1)
		acg.cg_ncyl = sblock.fs_ncyl % sblock.fs_cpg;
	else
		acg.cg_ncyl = sblock.fs_cpg;
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_ndblk = dmax - cbase;
	acg.cg_cs.cs_ndir = 0;
	acg.cg_cs.cs_nffree = 0;
	acg.cg_cs.cs_nbfree = 0;
	acg.cg_cs.cs_nifree = 0;
	acg.cg_rotor = 0;
	acg.cg_frotor = 0;
	acg.cg_irotor = 0;
	acg.cg_btotoff = &acg.cg_space[0] - (uchar_t *)(&acg.cg_link);
	acg.cg_boff = acg.cg_btotoff + sblock.fs_cpg * sizeof (long);
	acg.cg_iusedoff = acg.cg_boff +
		sblock.fs_cpg * sblock.fs_nrpos * sizeof (short);
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, NBBY);
	acg.cg_nextfreeoff = acg.cg_freeoff +
		howmany(sblock.fs_cpg * sblock.fs_spc / NSPF(&sblock), NBBY);
	for (i = 0; i < sblock.fs_frag; i++) {
		acg.cg_frsum[i] = 0;
	}
	bzero((caddr_t)cg_inosused(&acg), acg.cg_freeoff - acg.cg_iusedoff);
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; i < UFSROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	for (i = 0; i < sblock.fs_ipg / INOPF(&sblock); i += sblock.fs_frag) {
		randomgeneration();
		wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
		    sblock.fs_bsize, (char *)zino);
	}
	bzero((caddr_t)cg_blktot(&acg), acg.cg_boff - acg.cg_btotoff);
	bzero((caddr_t)cg_blks(&sblock, &acg, 0),
	    acg.cg_iusedoff - acg.cg_boff);
	bzero((caddr_t)cg_blksfree(&acg), acg.cg_nextfreeoff - acg.cg_freeoff);

	if (cylno > 0) {
		for (d = 0; d < dlower; d += sblock.fs_frag) {
			setblock(&sblock, cg_blksfree(&acg), d/sblock.fs_frag);
			acg.cg_cs.cs_nbfree++;
			cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
			cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
			    [cbtorpos(&sblock, d)]++;
		}
		sblock.fs_dsize += dlower;
	}
	sblock.fs_dsize += acg.cg_ndblk - dupper;
	if ((i = dupper % sblock.fs_frag) != 0) {
		acg.cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(&acg), dupper);
			acg.cg_cs.cs_nffree++;
		}
	}
	for (d = dupper; d + sblock.fs_frag <= dmax - cbase; ) {
		setblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag);
		acg.cg_cs.cs_nbfree++;
		cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
		cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
		    [cbtorpos(&sblock, d)]++;
		d += sblock.fs_frag;
	}
	if (d < dmax - cbase) {
		acg.cg_frsum[dmax - cbase - d]++;
		for (; d < dmax - cbase; d++) {
			setbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree++;
		}
	}
	sblock.fs_cstotal.cs_ndir += acg.cg_cs.cs_ndir;
	sblock.fs_cstotal.cs_nffree += acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree += acg.cg_cs.cs_nbfree;
	sblock.fs_cstotal.cs_nifree += acg.cg_cs.cs_nifree;
	*cs = acg.cg_cs;
	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
		sblock.fs_bsize, (char *)&acg);
}

/*
 * initialize the file system
 */
struct inode node;

#define	LOSTDIR
#ifdef LOSTDIR
#define	PREDEFDIR 3
#else
#define	PREDEFDIR 2
#endif

struct direct root_dir[] = {
	{ UFSROOTINO, sizeof (struct direct), 1, "." },
	{ UFSROOTINO, sizeof (struct direct), 2, ".." },
#ifdef LOSTDIR
	{ LOSTFOUNDINO, sizeof (struct direct), 10, "lost+found" },
#endif
};
#ifdef LOSTDIR
struct direct lost_found_dir[] = {
	{ LOSTFOUNDINO, sizeof (struct direct), 1, "." },
	{ UFSROOTINO, sizeof (struct direct), 2, ".." },
	{ 0, DIRBLKSIZ, 0, 0 },
};
#endif
char buf[MAXBSIZE];

static void
fsinit()
{
	int i;


	/*
	 * initialize the node
	 */
	node.i_atime = mkfstime;
	node.i_mtime = mkfstime;
	node.i_ctime = mkfstime;
#ifdef LOSTDIR
	/*
	 * create the lost+found directory
	 */
	(void) makedir(lost_found_dir, 2);
	for (i = DIRBLKSIZ; i < sblock.fs_bsize; i += DIRBLKSIZ) {
		bcopy(&lost_found_dir[2], &buf[i], DIRSIZ(&lost_found_dir[2]));
	}
	node.i_number = LOSTFOUNDINO;
	node.i_smode = node.i_mode = IFDIR | 0700;
	node.i_nlink = 2;
	node.i_size = sblock.fs_bsize;
	node.i_db[0] = alloc((int)node.i_size, node.i_mode);
	node.i_blocks = btodb(fragroundup(&sblock, (int)node.i_size));
	IRANDOMIZE(&node.i_ic);
	wtfs(fsbtodb(&sblock, node.i_db[0]), (int)node.i_size, buf);
	iput(&node);
#endif
	/*
	 * create the root directory
	 */
	node.i_number = UFSROOTINO;
	node.i_mode = node.i_smode = IFDIR | UMASK;
	node.i_nlink = PREDEFDIR;
	node.i_size = makedir(root_dir, PREDEFDIR);
	node.i_db[0] = alloc(sblock.fs_fsize, node.i_mode);
	/* i_size < 2GB because we are initializing the file system */
	node.i_blocks = btodb(fragroundup(&sblock, (int)node.i_size));
	IRANDOMIZE(&node.i_ic);
	wtfs(fsbtodb(&sblock, node.i_db[0]), sblock.fs_fsize, buf);
	iput(&node);
}

/*
 * construct a set of directory entries in "buf".
 * return size of directory.
 */
static int
makedir(register struct direct *protodir, int entries)
{
	char *cp;
	int i;
	ushort_t spcleft;

	spcleft = DIRBLKSIZ;
	for (cp = buf, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(&protodir[i]);
		bcopy(&protodir[i], cp, protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		spcleft -= protodir[i].d_reclen;
	}
	protodir[i].d_reclen = spcleft;
	bcopy(&protodir[i], cp, DIRSIZ(&protodir[i]));
	return (DIRBLKSIZ);
}

/*
 * allocate a block or frag
 */
static daddr32_t
alloc(int size, int mode)
{
	int i, frag;
	daddr32_t d;

	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		(void) fprintf(stderr, gettext("cg 0: bad magic number\n"));
		lockexit(32);
	}
	if (acg.cg_cs.cs_nbfree == 0) {
		(void) fprintf(stderr,
			gettext("first cylinder group ran out of space\n"));
		lockexit(32);
	}
	for (d = 0; d < acg.cg_ndblk; d += sblock.fs_frag)
		if (isblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag))
			goto goth;
	(void) fprintf(stderr,
	    gettext("internal error: can't find block in cyl 0\n"));
	lockexit(32);
goth:
	clrblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag);
	acg.cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	fscs[0].cs_nbfree--;
	if (mode & IFDIR) {
		acg.cg_cs.cs_ndir++;
		sblock.fs_cstotal.cs_ndir++;
		fscs[0].cs_ndir++;
	}
	cg_blktot(&acg)[cbtocylno(&sblock, d)]--;
	cg_blks(&sblock, &acg, cbtocylno(&sblock, d))[cbtorpos(&sblock, d)]--;
	if (size != sblock.fs_bsize) {
		frag = howmany(size, sblock.fs_fsize);
		fscs[0].cs_nffree += sblock.fs_frag - frag;
		sblock.fs_cstotal.cs_nffree += sblock.fs_frag - frag;
		acg.cg_cs.cs_nffree += sblock.fs_frag - frag;
		acg.cg_frsum[sblock.fs_frag - frag]++;
		for (i = frag; i < sblock.fs_frag; i++)
			setbit(cg_blksfree(&acg), d + i);
	}
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	return (d);
}

/*
 * Allocate an inode on the disk
 */
static void
iput(register struct inode *ip)
{
	struct dinode buf[MAXINOPB];
	daddr32_t d;

	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		(void) fprintf(stderr, gettext("cg 0: bad magic number\n"));
		lockexit(32);
	}
	acg.cg_cs.cs_nifree--;
	setbit(cg_inosused(&acg), ip->i_number);
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	sblock.fs_cstotal.cs_nifree--;
	fscs[0].cs_nifree--;
	if ((int)ip->i_number >= sblock.fs_ipg * sblock.fs_ncg) {
		(void) fprintf(stderr,
			gettext("fsinit: inode value out of range (%d).\n"),
			ip->i_number);
		lockexit(32);
	}
	d = fsbtodb(&sblock, itod(&sblock, (int)ip->i_number));
	rdfs(d, sblock.fs_bsize, (char *)buf);
	buf[itoo(&sblock, (int)ip->i_number)].di_ic = ip->i_ic;
	wtfs(d, sblock.fs_bsize, (char *)buf);
}

/*
 * read a block from the file system
 */
static void
rdfs(daddr32_t bno, int size, char *bf)
{
	int n, saverr;

	/*
	 * Note: the llseek() can succeed, even if the offset is out of range.
	 * It's not until the file i/o operation (the read()) that one knows
	 * for sure if the raw device can handle the offset.
	 */
	if (llseek(fsi, (offset_t)bno * sectorsize, 0) < 0) {
		saverr = errno;
		(void) fprintf(stderr,
		    gettext("seek error on sector %ld: %s\n"),
		    bno, strerror(saverr));
		lockexit(32);
	}
	n = read(fsi, bf, size);
	if (n != size) {
		saverr = errno;
		if (n == -1)
			(void) fprintf(stderr,
			    gettext("read error on sector %ld: %s\n"),
			    bno, strerror(saverr));
		else
			(void) fprintf(stderr, gettext(
			    "short read (%d of %d bytes) on sector %ld\n"),
			    n, size, bno);
		lockexit(32);
	}
}

/*
 * write a block to the file system
 */
static void
wtfs(daddr32_t bno, int size, char *bf)
{
	int n, saverr;

	if (fso == -1)
		return;

	/*
	 * Note: the llseek() can succeed, even if the offset is out of range.
	 * It's not until the file i/o operation (the write()) that one knows
	 * for sure if the raw device can handle the offset.
	 */
	if (llseek(fso, (offset_t)bno * sectorsize, 0) < 0) {
		saverr = errno;
		(void) fprintf(stderr,
		    gettext("seek error on sector %ld: %s\n"),
		    bno, strerror(saverr));
		lockexit(32);
	}
	if (Nflag)
		return;
	n = write(fso, bf, size);
	if (n != size) {
		saverr = errno;
		if (n == -1)
			(void) fprintf(stderr,
			    gettext("write error on sector %ld: %s\n"),
			    bno, strerror(saverr));
		else
			(void) fprintf(stderr, gettext(
			    "short write (%d of %d bytes) on sector %ld\n"),
			    n, size, bno);
		lockexit(32);
	}
}

/*
 * check if a block is available
 */
static int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	unsigned char mask;

	switch (fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
		(void) fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
		return (0);
	}
}

/*
 * take a block out of the map
 */
static void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
		(void) fprintf(stderr,
		    gettext("clrblock: bad fs_frag value %d\n"), fs->fs_frag);
		return;
	}
}

/*
 * put a block into the map
 */
static void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
		(void) fprintf(stderr,
		    gettext("setblock: bad fs_frag value %d\n"), fs->fs_frag);
		return;
	}
}

static void
usage()
{
	(void) fprintf(stderr,
	    gettext("ufs usage: mkfs [-F FSType] [-V] [-m] [-o options] "
		"special "			/* param 0 */
		"size(sectors) \\ \n"));	/* param 1 */
	(void) fprintf(stderr,
		"[nsect "			/* param 2 */
		"ntrack "			/* param 3 */
		"bsize "			/* param 4 */
		"fragsize "			/* param 5 */
		"cpg "				/* param 6 */
		"free "				/* param 7 */
		"rps "				/* param 8 */
		"nbpi "				/* param 9 */
		"opt "				/* param 10 */
		"apc "				/* param 11 */
		"gap "				/* param 12 */
		"nrpos "			/* param 13 */
		"maxcontig]\n");		/* param 14 */
	(void) fprintf(stderr,
		gettext(" -m : dump fs cmd line used to make this partition\n"
		" -V :print this command line and return\n"
		" -o :ufs options: :nsect=%d,ntrack=%d,bsize=%d,fragsize=%d\n"
		" -o :ufs options: :cgsize=%d,free=%d,rps=%d,nbpi=%d,opt=%c\n"
		" -o :ufs options: :apc=%d,gap=%d,nrpos=%d,maxcontig=%d\n"
"NOTE that all -o suboptions: must be separated only by commas so as to\n"
"be parsed as a single argument\n"),
		nsect, ntrack, bsize, fragsize, cpg, sblock.fs_minfree, rps,
		nbpi, opt, apc, (rotdelay == -1) ? 0 : rotdelay,
		sblock.fs_nrpos, maxcontig);
	lockexit(32);
}

/*ARGSUSED*/
static void
dump_fscmd(char *fsys, int fsi)
{
	long used, bpcg, inospercg;
	long nbpi;

	bzero((char *)&sblock, sizeof (sblock));
	rdfs(SBLOCK, SBSIZE, (char *)&sblock);

	if (sblock.fs_magic != FS_MAGIC)
	    (void) fprintf(stderr, gettext(
		"[not currently a valid file system - bad superblock]\n"));

	/*
	 * Compute a reasonable nbpi value.
	 * The algorithm for "used" is copied from code
	 * in main() verbatim.
	 * The nbpi equation is taken from main where the
	 * fs_ipg value is set for the last time.  The INOPB(...) - 1
	 * is used to account for the roundup.
	 * The problem is that a range of nbpi values map to
	 * the same file system layout.  So it is not possible
	 * to calculate the exact value specified when the file
	 * system was created.  So instead we determine the top
	 * end of the range of values.
	 */
	bpcg = sblock.fs_spc * sectorsize;
	inospercg = roundup(bpcg / sizeof (struct dinode), INOPB(&sblock));
	if (inospercg > MAXIpG(&sblock))
		inospercg = MAXIpG(&sblock);
	used = (sblock.fs_iblkno + inospercg / INOPF(&sblock)) * NSPF(&sblock);
	used *= sectorsize;
	nbpi = (sblock.fs_cpg * bpcg - used) /
	    (sblock.fs_ipg - (INOPB(&sblock) - 1));

	(void) fprintf(stderr, gettext("mkfs -F ufs -o "), fsys);
	(void) fprintf(stderr, "nsect=%d,ntrack=%d,",
	    sblock.fs_nsect, sblock.fs_ntrak);
	(void) fprintf(stderr, "bsize=%d,fragsize=%d,cgsize=%d,free=%d,",
	    sblock.fs_bsize, sblock.fs_fsize, sblock.fs_cpg, sblock.fs_minfree);
	(void) fprintf(stderr, "rps=%d,nbpi=%ld,opt=%c,apc=%d,gap=%d,",
	    sblock.fs_rps, nbpi, (sblock.fs_optim == FS_OPTSPACE) ? 's' : 't',
	    (sblock.fs_ntrak * sblock.fs_nsect) - sblock.fs_spc,
	    sblock.fs_rotdelay);
	(void) fprintf(stderr, "nrpos=%d,maxcontig=%d ",
	    sblock.fs_nrpos, sblock.fs_maxcontig);
	(void) fprintf(stderr, "%s %d\n", fsys,
	    fsbtodb(&sblock, sblock.fs_size));

	bzero((char *)&sblock, sizeof (sblock));
}

/* number ************************************************************* */
/*									*/
/* Convert a numeric string arg to binary				*/
/*									*/
/* Args:	d_value - default value, if have parse error		*/
/*		param - the name of the argument, for error messages	*/
/*		flags - parser state and what's allowed in the arg	*/
/* Global arg:  string - pointer to command arg				*/
/*									*/
/* Valid forms: 123 | 123k | 123*123 | 123x123				*/
/*									*/
/* Return:	converted number					*/
/*									*/
/* ******************************************************************** */

static unsigned long
number(long d_value, char *param, int flags)
{
	char *cs;
	long n, t;
	long cut = BIG / 10;    /* limit to avoid overflow */
	int minus = 0;

	cs = string;
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
		case 'k':
			if (flags & ALLOW_END_ONLY)
				goto bail_out;
			if (n > (BIG / 1024))
				goto overflow;
			n *= 1024;
			continue;

		case '*':
		case 'x':
			if (flags & ALLOW_END_ONLY)
				goto bail_out;
			string = cs;
			t = number(d_value, param, flags);
			if (n > (BIG / t))
				goto overflow;
			n *= t;
			cs = string + 1; /* adjust for -- below */

			/* recursion has read rest of expression */
			/* FALLTHROUGH */

		case ',':
		case '\0':
			cs--;
			string = cs;
			return (n);

		case '%':
			if (flags & ALLOW_END_ONLY)
				goto bail_out;
			if (flags & ALLOW_PERCENT) {
				flags &= ~ALLOW_PERCENT;
				flags |= ALLOW_END_ONLY;
				continue;
			}
			goto bail_out;

		case 'm':
			if (flags & ALLOW_END_ONLY)
				goto bail_out;
			if (flags & ALLOW_MS1) {
				flags &= ~ALLOW_MS1;
				flags |= ALLOW_MS2;
				continue;
			}
			goto bail_out;

		case 's':
			if (flags & ALLOW_END_ONLY)
				goto bail_out;
			if (flags & ALLOW_MS2) {
				flags &= ~ALLOW_MS2;
				flags |= ALLOW_END_ONLY;
				continue;
			}
			goto bail_out;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
overflow:
			(void) fprintf(stderr,
			    gettext("mkfs: value for %s overflowed\n"),
			    param);
			while ((*cs != '\0') && (*cs != ','))
				cs++;
			string = cs;
			return (BIG);

		default:
bail_out:
			(void) fprintf(stderr, gettext(
			    "mkfs: bad numeric arg for %s: \"%s\"\n"),
			    param, string);
			while ((*cs != '\0') && (*cs != ','))
				cs++;
			string = cs;
			if (d_value != NO_DEFAULT) {
				(void) fprintf(stderr,
				    gettext("mkfs: %s reset to default %ld\n"),
				    param, d_value);
				return (d_value);
			}
			lockexit(2);

		}
	} /* never gets here */
}

/* match ************************************************************** */
/*									*/
/* Compare two text strings for equality				*/
/*									*/
/* Arg:	 s - pointer to string to match with a command arg		*/
/* Global arg:  string - pointer to command arg				*/
/*									*/
/* Return:	1 if match, 0 if no match				*/
/*		If match, also reset `string' to point to the text	*/
/*		that follows the matching text.				*/
/*									*/
/* ******************************************************************** */

static int
match(char *s)
{
	register char *cs;

	cs = string;
	while (*cs++ == *s) {
		if (*s++ == '\0') {
			goto true;
		}
	}
	if (*s != '\0') {
		return (0);
	}

true:
	cs--;
	string = cs;
	return (1);
}

/*
 * GROWFS ROUTINES
 */

/* ARGSUSED */
void
lockexit(exitstatus)
	int	exitstatus;
{
	/*
	 * flush the dirty cylinder group
	 */
	if (inlockexit == 0) {
		inlockexit = 1;
		flcg();
	}
	/*
	 * make sure the file system is unlocked before exiting
	 */
	if (inlockexit == 1) {
		inlockexit = 2;
		ulockfs();
	}

	exit(exitstatus);
}

void
randomgeneration()
{
	int		 i;
	struct dinode	*dp;

	/*
	 * always perform fsirand(1) function... newfs will notice that
	 * the inodes have been randomized and will not call fsirand itself
	 */
	for (i = 0, dp = zino; i < sblock.fs_inopb; ++i, ++dp)
		IRANDOMIZE(&dp->di_ic);
}

/*
 * Check the size of the summary information.
 * Fields in sblock are not changed in this function.
 *
 * For an 8K filesystem block, the maximum number of cylinder groups is 16384.
 *     MAXCSBUFS {32}  *   8K  {FS block size}
 *                         divided by (sizeof csum) {16}
 *
 * Note that MAXCSBUFS is not used in the kernel; as of Solaris 2.6 build 32,
 * this is the only place where it's referenced.
 */
void
checksummarysize()
{
	daddr32_t	dmax;
	daddr32_t	dmin;
	long	cg0frags;
	long	cg0blocks;
	longlong_t	maxncg;
	longlong_t	maxfrags;
	long	fs_size;
	long maxfs_blocks; /* filesystem blocks for maximum filesystem size */

	/*
	 * compute the maximum summary info size
	 */
	dmin = cgdmin(&sblock, 0);
	dmax = cgbase(&sblock, 0) + sblock.fs_fpg;
	fs_size = (grow) ? grow_fs_size : sblock.fs_size;
	if (dmax > fs_size)
		dmax = fs_size;
	cg0frags  = dmax - dmin;
	cg0blocks = cg0frags / sblock.fs_frag;
	cg0frags = cg0blocks * sblock.fs_frag;
	maxncg   = (longlong_t)cg0blocks *
	    (longlong_t)(sblock.fs_bsize / sizeof (struct csum));

	maxfs_blocks = dbtofsb(&sblock, INT_MAX);

	/*
	 * Limit maxncg to the maximum possible with the given
	 * parameters. The maximum possible is INT_MAX sectors.
	 * We convert INT_MAX sectors to file system fragment block size
	 * (given by maxfs_blocks above) and then divide it by
	 * fragments per group, to find maxncg necessary.
	 */

	if (maxncg > ((longlong_t)maxfs_blocks / (longlong_t)sblock.fs_fpg) + 1)
		maxncg = ((longlong_t)maxfs_blocks /
		    (longlong_t)sblock.fs_fpg) + 1;

	maxfrags = maxncg * (longlong_t)sblock.fs_fpg;

	/*
	 * Limit maxfrags to that possible
	 * for a filesystem of size INT_MAX sectors.
	 */

	if (maxfrags > maxfs_blocks)
		maxfrags = maxfs_blocks;


	/*
	 * remember for later processing in extendsummaryinfo()
	 */
	if (test)
		grow_sifrag = dmin + (cg0blocks * sblock.fs_frag);
	if (testfrags == 0)
		testfrags = cg0frags;
	if (testforce)
		if (testfrags > cg0frags) {
			(void) fprintf(stderr,
				gettext("Too many test frags (%d); try %d\n"),
				testfrags, cg0frags);
			lockexit(32);
		}

	/*
	 * if summary info is too large (too many cg's) tell the user and exit
	 */
	if ((longlong_t)sblock.fs_size > maxfrags) {
		(void) fprintf(stderr, gettext(
		    "Too many cylinder groups with %u sectors;\n    try "
		    "increasing cgsize, or decreasing fssize to %llu\n"),
		    fsbtodb(&sblock, sblock.fs_size),
		    fsbtodb(&sblock, maxfrags));
		lockexit(32);
	}
}

void
checksblock()
{
	/*
	 * make sure this is a file system
	 */
	if (sblock.fs_magic != FS_MAGIC) {
		(void) fprintf(stderr,
			gettext("Bad superblock; magic number wrong\n"));
		lockexit(32);
	}
	if (sblock.fs_ncg < 1) {
		(void) fprintf(stderr,
		    gettext("Bad superblock; ncg out of range\n"));
		lockexit(32);
	}
	if (sblock.fs_cpg < 1) {
		(void) fprintf(stderr,
		    gettext("Bad superblock; cpg out of range\n"));
		lockexit(32);
	}
	if (sblock.fs_ncg * sblock.fs_cpg < sblock.fs_ncyl ||
	    (sblock.fs_ncg - 1) * sblock.fs_cpg >= sblock.fs_ncyl) {
		(void) fprintf(stderr,
		    gettext("Bad superblock; ncyl out of range\n"));
		lockexit(32);
	}
	if (sblock.fs_sbsize <= 0 || sblock.fs_sbsize > sblock.fs_bsize) {
		(void) fprintf(stderr, gettext(
			"Bad superblock; superblock size out of range\n"));
		lockexit(32);
	}
}

/*
 * Roll the embedded log, if any, and set up the global variables
 * islog, islogok, and ismdd.
 */
static void
logsetup(char *devstr)
{
	void		*buf, *ud_buf;
	extent_block_t	*ebp;
	ml_unit_t	*ul;
	ml_odunit_t	*ud;


	if (sblock.fs_logbno && rl_roll_log(devstr) != RL_SUCCESS)
		return;

	/* Logging UFS may be enabled */
	if (sblock.fs_logbno) {
		islog = islogok = 0;

		++islog;

		/* log is not okay; check the fs */
		if ((FSOKAY != (sblock.fs_state + sblock.fs_time)) ||
		    (sblock.fs_clean != FSLOG))
			return;

		/* get the log allocation block */
		buf = (void *)malloc(DEV_BSIZE);
		if (buf == (void *) NULL)
			return;
		ud_buf = (void *)malloc(DEV_BSIZE);
		if (ud_buf == (void *) NULL) {
			free(buf);
			return;
		}
		rdfs((daddr32_t)sblock.fs_logbno, DEV_BSIZE, buf);
		ebp = (extent_block_t *)buf;

		/* log allocation block is not okay; check the fs */
		if (ebp->type != LUFS_EXTENTS) {
			free(buf);
			free(ud_buf);
			return;
		}

		/* get the log state block(s) */
		rdfs((daddr32_t)ebp->extents[0].pbno, DEV_BSIZE, ud_buf);
		ud = (ml_odunit_t *)ud_buf;
		ul = (ml_unit_t *)malloc(sizeof (*ul));
		ul->un_ondisk = *ud;

		/* log state is okay */
		if ((ul->un_chksum == ul->un_head_ident + ul->un_tail_ident) &&
		    (ul->un_version == LUFS_VERSION_LATEST) &&
		    (ul->un_badlog == 0))
			++islogok;
		free(ud_buf);
		free(buf);
		free(ul);
	}
}

void
growinit(char *devstr)
{
	int	i;
	char	buf[DEV_BSIZE];

	/*
	 * Read and verify the superblock
	 */
	rdfs(SBOFF / sectorsize, sbsize, (char *)&sblock);
	checksblock();
	if (sblock.fs_postblformat != FS_DYNAMICPOSTBLFMT) {
		(void) fprintf(stderr,
			gettext("old file system format; can't growfs\n"));
		lockexit(32);
	}

	/*
	 * can't shrink a file system
	 */
	grow_fssize = fsbtodb(&sblock, sblock.fs_size);
	if (fssize < grow_fssize) {
		(void) fprintf(stderr,
			gettext("%d sectors < current size of %d sectors\n"),
			fssize, grow_fssize);
		lockexit(32);
	}

	logsetup(devstr);

	/*
	 * can't growfs when logging device has errors
	 */
	if ((islog && !islogok) ||
	    ((FSOKAY == (sblock.fs_state + sblock.fs_time)) &&
	    (sblock.fs_clean == FSLOG && !islog))) {
		(void) fprintf(stderr,
			gettext("logging device has errors; can't growfs\n"));
		lockexit(32);
	}

	/*
	 * make sure device is big enough
	 */
	rdfs((daddr32_t)fssize - 1, DEV_BSIZE, buf);
	wtfs((daddr32_t)fssize - 1, DEV_BSIZE, buf);

	/*
	 * read current summary information
	 */
	grow_fscs = (struct csum *)(malloc((unsigned)sblock.fs_cssize));
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize) {
		rdfs(fsbtodb(&sblock,
			sblock.fs_csaddr + numfrags(&sblock, i)),
			(int)(sblock.fs_cssize - i < sblock.fs_bsize ?
			sblock.fs_cssize - i : sblock.fs_bsize),
			((caddr_t)grow_fscs) + i);
	}
	/*
	 * save some current size related fields from the superblock
	 * These are used in extendsummaryinfo()
	 */
	grow_fs_size	= sblock.fs_size;
	grow_fs_ncg	= sblock.fs_ncg;
	grow_fs_csaddr	= sblock.fs_csaddr;
	grow_fs_cssize	= sblock.fs_cssize;

	/*
	 * save and reset the clean flag
	 */
	if (FSOKAY == (sblock.fs_state + sblock.fs_time))
		grow_fs_clean = sblock.fs_clean;
	else
		grow_fs_clean = FSBAD;
	sblock.fs_clean = FSBAD;
	sblock.fs_state = FSOKAY - sblock.fs_time;
	wtfs(SBOFF / sectorsize, sbsize, (char *)&sblock);
}

void
checkdev(rdev, bdev)
	char	*rdev;
	char	*bdev;
{
	struct stat64	statarea;
	struct ustat	ustatarea;

	if (stat64(bdev, &statarea) < 0) {
		(void) fprintf(stderr, gettext("can't check mount point; "));
		(void) fprintf(stderr, gettext("can't stat %s\n"), bdev);
		lockexit(32);
	}
	if ((statarea.st_mode & S_IFMT) != S_IFBLK) {
		(void) fprintf(stderr, gettext(
		    "can't check mount point; %s is not a block device\n"),
		    bdev);
		lockexit(32);
	}
	if (ustat(statarea.st_rdev, &ustatarea) >= 0)
		bdevismounted = 1;

	if (stat64(rdev, &statarea) < 0) {
		(void) fprintf(stderr, gettext("can't stat %s\n"), rdev);
		lockexit(32);
	}
	if ((statarea.st_mode & S_IFMT) != S_IFCHR) {
		(void) fprintf(stderr,
			gettext("%s is not a character device\n"), rdev);
		lockexit(32);
	}
}

void
checkmount(mntp, bdevname)
	struct mnttab	*mntp;
	char		*bdevname;
{
	struct stat64	statdir;
	struct stat64	statdev;

	if (strcmp(bdevname, mntp->mnt_special) == 0) {
		if (stat64(mntp->mnt_mountp, &statdir) == -1) {
			(void) fprintf(stderr, gettext("can't stat %s\n"),
				mntp->mnt_mountp);
			lockexit(32);
		}
		if (stat64(mntp->mnt_special, &statdev) == -1) {
			(void) fprintf(stderr, gettext("can't stat %s\n"),
				mntp->mnt_special);
			lockexit(32);
		}
		if (statdir.st_dev != statdev.st_rdev) {
			(void) fprintf(stderr, gettext(
				"%s is not mounted on %s; mnttab(4) wrong\n"),
				mntp->mnt_special, mntp->mnt_mountp);
			lockexit(32);
		}
		ismounted = 1;
		if (directory) {
			if (strcmp(mntp->mnt_mountp, directory) != 0) {
				(void) fprintf(stderr,
				gettext("%s is mounted on %s, not %s\n"),
					bdevname, mntp->mnt_mountp, directory);
				lockexit(32);
			}
		} else {
			if (grow)
				(void) fprintf(stderr, gettext(
					"%s is mounted on %s; can't growfs\n"),
					bdevname, mntp->mnt_mountp);
			else
				(void) fprintf(stderr,
					gettext("%s is mounted, can't mkfs\n"),
					bdevname);
			lockexit(32);
		}
	}
}

struct dinode	*dibuf		= 0;
daddr32_t		 difrag		= 0;
struct dinode *
gdinode(ino)
	ino_t	ino;
{
	/*
	 * read the block of inodes containing inode number ino
	 */
	if (dibuf == 0)
		dibuf = (struct dinode *)malloc((unsigned)sblock.fs_bsize);
	if (itod(&sblock, ino) != difrag) {
		difrag = itod(&sblock, ino);
		rdfs(fsbtodb(&sblock, difrag), (int)sblock.fs_bsize,
			(char *)dibuf);
	}
	return (dibuf + (ino % INOPB(&sblock)));
}

/*
 * structure that manages the frags we need for extended summary info
 *	These frags can be:
 *		free
 *		data  block
 *		alloc block
 */
struct csfrag {
	struct csfrag	*next;		/* next entry */
	daddr32_t		 ofrag;		/* old frag */
	daddr32_t		 nfrag;		/* new frag */
	long		 cylno;		/* cylno of nfrag */
	long		 frags;		/* number of frags */
	long		 size;		/* size in bytes */
	ino_t		 ino;		/* inode number */
	long		 fixed;		/* Boolean - Already fixed? */
};
struct csfrag	*csfrag;		/* state unknown */
struct csfrag	*csfragino;		/* frags belonging to an inode */
struct csfrag	*csfragfree;		/* frags that are free */

daddr32_t maxcsfrag	= 0;		/* maximum in range */
daddr32_t mincsfrag	= 0x7fffffff;	/* minimum in range */

int
csfraginrange(frag)
	daddr32_t	frag;
{
	return ((frag >= mincsfrag) && (frag <= maxcsfrag));
}

struct csfrag *
findcsfrag(frag, cfap)
	daddr32_t		frag;
	struct csfrag	**cfap;
{
	struct csfrag	*cfp;

	if (!csfraginrange(frag))
		return (NULL);

	for (cfp = *cfap; cfp; cfp = cfp->next)
		if (cfp->ofrag == frag)
			return (cfp);
	return (NULL);
}

void
checkindirect(ino, fragsp, frag, level)
	ino_t	ino;
	daddr32_t	*fragsp;
	daddr32_t	 frag;
	int	 level;
{
	int			i;
	int			ne	= sblock.fs_bsize / sizeof (daddr32_t);
	daddr32_t			fsb[MAXBSIZE / sizeof (daddr32_t)];

	if (frag == 0)
		return;

	rdfs(fsbtodb(&sblock, frag), (int)sblock.fs_bsize, (char *)fsb);

	checkdirect(ino, fragsp, fsb, sblock.fs_bsize / sizeof (daddr32_t));

	if (level)
		for (i = 0; i < ne && *fragsp; ++i)
			checkindirect(ino, fragsp, fsb[i], level-1);
}

void
addcsfrag(ino, frag, cfap)
	ino_t		ino;
	daddr32_t		frag;
	struct csfrag	**cfap;
{
	struct csfrag	*cfp;

	/*
	 * establish a range for faster checking in csfraginrange()
	 */
	if (frag > maxcsfrag)
		maxcsfrag = frag;
	if (frag < mincsfrag)
		mincsfrag = frag;

	/*
	 * if this frag belongs to an inode and is not the start of a block
	 *	then see if it is part of a frag range for this inode
	 */
	if (ino && (frag % sblock.fs_frag))
		for (cfp = *cfap; cfp; cfp = cfp->next) {
			if (ino != cfp->ino)
				continue;
			if (frag != cfp->ofrag + cfp->frags)
				continue;
			cfp->frags++;
			cfp->size += sblock.fs_fsize;
			return;
		}
	/*
	 * allocate a csfrag entry and link on specified anchor
	 */
	cfp = (struct csfrag *)calloc(1, sizeof (struct csfrag));
	cfp->ino	= ino;
	cfp->ofrag	= frag;
	cfp->frags	= 1;
	cfp->size	= sblock.fs_fsize;
	cfp->next	= *cfap;
	*cfap		= cfp;
}

void
delcsfrag(frag, cfap)
	daddr32_t		frag;
	struct csfrag	**cfap;
{
	struct csfrag	*cfp;
	struct csfrag	**cfpp;

	/*
	 * free up entry whose beginning frag matches
	 */
	for (cfpp = cfap; *cfpp; cfpp = &(*cfpp)->next) {
		if (frag == (*cfpp)->ofrag) {
			cfp = *cfpp;
			*cfpp = (*cfpp)->next;
			free((char *)cfp);
			return;
		}
	}
}

void
checkdirect(ino, fragsp, db, ne)
	ino_t	ino;
	daddr32_t	*fragsp;
	daddr32_t	*db;
	int	 ne;
{
	int	 i;
	int	 j;
	int	 found;
	daddr32_t	 frag;

	/*
	 * scan for allocation within the new summary info range
	 */
	for (i = 0; i < ne && *fragsp; ++i) {
		if (frag = *db++) {
			found = 0;
			for (j = 0; j < sblock.fs_frag && *fragsp; ++j) {
				if (found || (found = csfraginrange(frag))) {
					addcsfrag(ino, frag, &csfragino);
					delcsfrag(frag, &csfrag);
				}
				++frag;
				--(*fragsp);
			}
		}
	}
}

void
findcsfragino()
{
	int		 i;
	int		 j;
	daddr32_t		 frags;
	struct dinode	*dp;

	/*
	 * scan all old inodes looking for allocations in the new
	 * summary info range.  Move the affected frag from the
	 * generic csfrag list onto the `owned-by-inode' list csfragino.
	 */
	for (i = UFSROOTINO; i < grow_fs_ncg*sblock.fs_ipg && csfrag; ++i) {
		dp = gdinode((ino_t)i);
		switch (dp->di_mode & IFMT) {
			case IFSHAD	:
			case IFLNK 	:
			case IFDIR 	:
			case IFREG 	: break;
			default		: continue;
		}

		frags   = dbtofsb(&sblock, dp->di_blocks);

		checkdirect((ino_t)i, &frags, &dp->di_db[0], NDADDR+NIADDR);
		for (j = 0; j < NIADDR && frags; ++j)
			checkindirect((ino_t)i, &frags, dp->di_ib[j], j);
	}
}

void
fixindirect(frag, level)
	daddr32_t		frag;
	int		level;
{
	int			 i;
	int			 ne	= sblock.fs_bsize / sizeof (daddr32_t);
	daddr32_t			fsb[MAXBSIZE / sizeof (daddr32_t)];

	if (frag == 0)
		return;

	rdfs(fsbtodb(&sblock, frag), (int)sblock.fs_bsize, (char *)fsb);

	fixdirect((caddr_t)fsb, frag, fsb, ne);

	if (level)
		for (i = 0; i < ne; ++i)
			fixindirect(fsb[i], level-1);
}

void
fixdirect(bp, frag, db, ne)
	caddr_t		 bp;
	daddr32_t		 frag;
	daddr32_t		*db;
	int		 ne;
{
	int	 i;
	struct csfrag	*cfp;

	for (i = 0; i < ne; ++i, ++db) {
		if (*db == 0)
			continue;
		if ((cfp = findcsfrag(*db, &csfragino)) == NULL)
			continue;
		*db = cfp->nfrag;
		cfp->fixed = 1;
		wtfs(fsbtodb(&sblock, frag), (int)sblock.fs_bsize, bp);
	}
}

void
fixcsfragino()
{
	int		 i;
	struct dinode	*dp;
	struct csfrag	*cfp;

	for (cfp = csfragino; cfp; cfp = cfp->next) {
		if (cfp->fixed)
			continue;
		dp = gdinode((ino_t)cfp->ino);
		fixdirect((caddr_t)dibuf, difrag, dp->di_db, NDADDR+NIADDR);
		for (i = 0; i < NIADDR; ++i)
			fixindirect(dp->di_ib[i], i);
	}
}

void
extendsummaryinfo()
{
	int		i;
	int		localtest	= test;
	long		frags;
	daddr32_t		oldfrag;
	daddr32_t		newfrag;

	/*
	 * if no-write (-N), don't bother
	 */
	if (Nflag)
		return;

again:
	flcg();
	/*
	 * summary info did not change size -- do nothing unless in test mode
	 */
	if (grow_fs_cssize == sblock.fs_cssize)
		if (!localtest)
			return;

	/*
	 * build list of frags needed for additional summary information
	 */
	oldfrag = howmany(grow_fs_cssize, sblock.fs_fsize) + grow_fs_csaddr;
	newfrag = howmany(sblock.fs_cssize, sblock.fs_fsize) + grow_fs_csaddr;
	for (i = oldfrag, frags = 0; i < newfrag; ++i, ++frags)
		addcsfrag((ino_t)0, (daddr32_t)i, &csfrag);
	sblock.fs_dsize -= (newfrag - oldfrag);

	/*
	 * In test mode, we move more data than necessary from
	 * cylinder group 0.  The lookup/allocate/move code can be
	 * better stressed without having to create HUGE file systems.
	 */
	if (localtest)
		for (i = newfrag; i < grow_sifrag; ++i) {
			if (frags >= testfrags)
				break;
			frags++;
			addcsfrag((ino_t)0, (daddr32_t)i, &csfrag);
		}

	/*
	 * move frags to free or inode lists, depending on owner
	 */
	findcsfragfree();
	findcsfragino();

	/*
	 * if not all frags can be located, file system must be inconsistent
	 */
	if (csfrag) {
		(void) fprintf(stderr, gettext(
			"File system may be inconsistent; see fsck(1)\n"));
		lockexit(32);
	}

	/*
	 * allocate the free frags
	 */
	alloccsfragfree();
	/*
	 * allocate extra space for inode frags
	 */
	alloccsfragino();

	/*
	 * not enough space
	 */
	if (notenoughspace()) {
		unalloccsfragfree();
		unalloccsfragino();
		if (localtest && !testforce) {
			localtest = 0;
			goto again;
		}
		(void) fprintf(stderr, gettext("Not enough free space\n"));
		lockexit(32);
	}

	/*
	 * copy the data from old frags to new frags
	 */
	copycsfragino();

	/*
	 * fix the inodes to point to the new frags
	 */
	fixcsfragino();

	/*
	 * We may have moved more frags than we needed.  Free them.
	 */
	rdcg((long)0);
	for (i = newfrag; i <= maxcsfrag; ++i)
		setbit(cg_blksfree(&acg), i-cgbase(&sblock, 0));
	wtcg();

	flcg();
}

int
notenoughspace()
{
	struct csfrag	*cfp;

	for (cfp = csfragino; cfp; cfp = cfp->next)
		if (cfp->nfrag == 0)
			return (1);
	return (0);
}

void
unalloccsfragino()
{
	struct csfrag	*cfp;

	while ((cfp = csfragino) != NULL) {
		if (cfp->nfrag)
			freefrags(cfp->nfrag, cfp->frags, cfp->cylno);
		delcsfrag(cfp->ofrag, &csfragino);
	}
}

void
unalloccsfragfree()
{
	struct csfrag	*cfp;

	while ((cfp = csfragfree) != NULL) {
		freefrags(cfp->ofrag, cfp->frags, cfp->cylno);
		delcsfrag(cfp->ofrag, &csfragfree);
	}
}

void
findcsfragfree()
{
	struct csfrag	*cfp;
	struct csfrag	*cfpnext;

	/*
	 * move free frags onto the free-frag list
	 */
	rdcg((long)0);
	for (cfp = csfrag; cfp; cfp = cfpnext) {
		cfpnext = cfp->next;
		if (isset(cg_blksfree(&acg), cfp->ofrag - cgbase(&sblock, 0))) {
			addcsfrag(cfp->ino, cfp->ofrag, &csfragfree);
			delcsfrag(cfp->ofrag, &csfrag);
		}
	}
}

void
copycsfragino()
{
	struct csfrag	*cfp;
	char		buf[MAXBSIZE];

	/*
	 * copy data from old frags to newly allocated frags
	 */
	for (cfp = csfragino; cfp; cfp = cfp->next) {
		rdfs(fsbtodb(&sblock, cfp->ofrag), (int)cfp->size, buf);
		wtfs(fsbtodb(&sblock, cfp->nfrag), (int)cfp->size, buf);
	}
}

long	curcylno	= -1;
int	cylnodirty	= 0;
void
rdcg(cylno)
	long	cylno;
{
	if (cylno != curcylno) {
		flcg();
		curcylno = cylno;
		rdfs(fsbtodb(&sblock, cgtod(&sblock, curcylno)),
			(int)sblock.fs_cgsize, (char *)&acg);
	}
}

void
flcg()
{
	if (cylnodirty) {
		resetallocinfo();
		wtfs(fsbtodb(&sblock, cgtod(&sblock, curcylno)),
			(int)sblock.fs_cgsize, (char *)&acg);
		cylnodirty = 0;
	}
	curcylno = -1;
}

void
wtcg()
{
	cylnodirty = 1;
}

void
allocfrags(frags, fragp, cylnop)
	long	frags;
	daddr32_t	*fragp;
	long	*cylnop;
{
	int	 i;
	int	 j;
	long	 bits;
	long	 bit;

	/*
	 * Allocate a free-frag range in an old cylinder group
	 */
	for (i = 0, *fragp = 0; i < grow_fs_ncg; ++i) {
		if (((fscs+i)->cs_nffree < frags) && ((fscs+i)->cs_nbfree == 0))
			continue;
		rdcg((long)i);
		bit = bits = 0;
		while (findfreerange(&bit, &bits)) {
			if (frags <= bits)  {
				for (j = 0; j < frags; ++j)
					clrbit(cg_blksfree(&acg), bit+j);
				wtcg();
				*cylnop = i;
				*fragp  = bit + cgbase(&sblock, i);
				return;
			}
			bit += bits;
		}
	}
}

void
alloccsfragino()
{
	struct csfrag	*cfp;

	/*
	 * allocate space for inode frag ranges
	 */
	for (cfp = csfragino; cfp; cfp = cfp->next) {
		allocfrags(cfp->frags, &cfp->nfrag, &cfp->cylno);
		if (cfp->nfrag == 0)
			break;
	}
}

void
alloccsfragfree()
{
	struct csfrag	*cfp;

	/*
	 * allocate the free frags needed for extended summary info
	 */
	rdcg((long)0);

	for (cfp = csfragfree; cfp; cfp = cfp->next)
		clrbit(cg_blksfree(&acg), cfp->ofrag - cgbase(&sblock, 0));

	wtcg();
}

void
freefrags(frag, frags, cylno)
	daddr32_t	frag;
	long	frags;
	long	cylno;
{
	int	i;

	/*
	 * free frags
	 */
	rdcg(cylno);
	for (i = 0; i < frags; ++i) {
		setbit(cg_blksfree(&acg), (frag+i) - cgbase(&sblock, cylno));
	}
	wtcg();
}

int
findfreerange(bitp, bitsp)
	long	*bitp;
	long	*bitsp;
{
	long	 bit;

	/*
	 * find a range of free bits in a cylinder group bit map
	 */
	for (bit = *bitp, *bitsp = 0; bit < acg.cg_ndblk; ++bit)
		if (isset(cg_blksfree(&acg), bit))
			break;

	if (bit >= acg.cg_ndblk)
		return (0);

	*bitp  = bit;
	*bitsp = 1;
	for (++bit; bit < acg.cg_ndblk; ++bit, ++(*bitsp)) {
		if ((bit % sblock.fs_frag) == 0)
			break;
		if (isclr(cg_blksfree(&acg), bit))
			break;
	}
	return (1);
}

void
resetallocinfo()
{
	long	cno;
	long	bit;
	long	bits;

	/*
	 * Compute the free blocks/frags info and update the appropriate
	 * inmemory superblock, summary info, and cylinder group fields
	 */
	sblock.fs_cstotal.cs_nffree -= acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree -= acg.cg_cs.cs_nbfree;

	acg.cg_cs.cs_nffree = 0;
	acg.cg_cs.cs_nbfree = 0;

	bzero((caddr_t)acg.cg_frsum, sizeof (acg.cg_frsum));
	bzero((caddr_t)cg_blktot(&acg), (int)(acg.cg_iusedoff-acg.cg_btotoff));

	bit = bits = 0;
	while (findfreerange(&bit, &bits)) {
		if (bits == sblock.fs_frag) {
			acg.cg_cs.cs_nbfree++;
			cno = cbtocylno(&sblock, bit);
			cg_blktot(&acg)[cno]++;
			cg_blks(&sblock, &acg, cno)[cbtorpos(&sblock, bit)]++;
		} else {
			acg.cg_cs.cs_nffree += bits;
			acg.cg_frsum[bits]++;
		}
		bit += bits;
	}

	*(fscs + acg.cg_cgx) = acg.cg_cs;

	sblock.fs_cstotal.cs_nffree += acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree += acg.cg_cs.cs_nbfree;
}

void
extendcg(cylno)
	long	cylno;
{
	int	i;
	daddr32_t	dupper;
	daddr32_t	cbase;
	daddr32_t	dmax;

	/*
	 * extend the cylinder group at the end of the old file system
	 * if it was partially allocated becase of lack of space
	 */
	flcg();
	rdcg(cylno);

	dupper = acg.cg_ndblk;
	if (cylno == sblock.fs_ncg - 1)
		acg.cg_ncyl = sblock.fs_ncyl % sblock.fs_cpg;
	else
		acg.cg_ncyl = sblock.fs_cpg;
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	acg.cg_ndblk = dmax - cbase;

	for (i = dupper; i < acg.cg_ndblk; ++i)
		setbit(cg_blksfree(&acg), i);

	sblock.fs_dsize += (acg.cg_ndblk - dupper);

	wtcg();
	flcg();
}

struct lockfs	lockfs;
int		lockfd;
int		islocked;
int		lockfskey;
char		lockfscomment[128];

void
ulockfs()
{
	/*
	 * if the file system was locked, unlock it before exiting
	 */
	if (islocked == 0)
		return;

	/*
	 * first, check if the lock held
	 */
	lockfs.lf_flags = LOCKFS_MOD;
	if (ioctl(lockfd, _FIOLFSS, &lockfs) == -1) {
		perror(directory);
		lockexit(32);
	}

	if (LOCKFS_IS_MOD(&lockfs)) {
		(void) fprintf(stderr,
			gettext("FILE SYSTEM CHANGED DURING GROWFS!\n"));
		(void) fprintf(stderr,
			gettext("   See lockfs(1), umount(1), and fsck(1)\n"));
		lockexit(32);
	}
	/*
	 * unlock the file system
	 */
	lockfs.lf_lock  = LOCKFS_ULOCK;
	lockfs.lf_flags = 0;
	lockfs.lf_key   = lockfskey;
	clockfs();
	if (ioctl(lockfd, _FIOLFS, &lockfs) == -1) {
		perror(directory);
		lockexit(32);
	}
}

void
wlockfs()
{

	/*
	 * if no-write (-N), don't bother
	 */
	if (Nflag)
		return;
	/*
	 * open the mountpoint, and write lock the file system
	 */
	if ((lockfd = open64(directory, O_RDONLY)) == -1) {
		perror(directory);
		lockexit(32);
	}
	lockfs.lf_lock  = LOCKFS_WLOCK;
	lockfs.lf_flags = 0;
	lockfs.lf_key   = 0;
	clockfs();
	if (ioctl(lockfd, _FIOLFS, &lockfs) == -1) {
		perror(directory);
		lockexit(32);
	}
	islocked = 1;
	lockfskey = lockfs.lf_key;
}

void
clockfs()
{
	time_t	t;
	char	*ct;

	(void) time(&t);
	ct = ctime(&t);
	ct[strlen(ct)-1] = '\0';

	(void) sprintf(lockfscomment, "%s -- mkfs pid %ld", ct, getpid());
	lockfs.lf_comlen  = strlen(lockfscomment)+1;
	lockfs.lf_comment = lockfscomment;
}

/*
 * Write the csum records and the superblock
 */
void
wtsb()
{
	long	i;

	/*
	 * write summary information
	 */
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize)
		wtfs(fsbtodb(&sblock, sblock.fs_csaddr + numfrags(&sblock, i)),
			(int)(sblock.fs_cssize - i < sblock.fs_bsize ?
			sblock.fs_cssize - i : sblock.fs_bsize),
			((char *)fscs) + i);

	/*
	 * write superblock
	 */
	sblock.fs_time = mkfstime;
	wtfs(SBOFF / sectorsize, sbsize, (char *)&sblock);
}

/*
 * Verify that the optimization selection is reasonable, and advance
 * the global "string" appropriately.
 */
static char
checkopt(optim)
	char	*optim;
{
	char	opt;
	int	limit = strcspn(optim, ",");

	switch (limit) {
	case 0:	/* missing indicator (have comma or nul) */
		(void) fprintf(stderr, gettext(
		    "mkfs: missing optimization flag reset to `t' (time)\n"));
		opt = 't';
		break;

	case 1: /* single-character indicator */
		opt = *optim;
		if ((opt != 's') && (opt != 't')) {
			(void) fprintf(stderr, gettext(
		    "mkfs: bad optimization value `%c' reset to `t' (time)\n"),
			    opt);
			opt = 't';
		}
		break;

	default: /* multi-character indicator */
		(void) fprintf(stderr, gettext(
	    "mkfs: bad optimization value `%*.*s' reset to `t' (time)\n"),
		    limit, limit, optim);
		opt = 't';
		break;
	}

	string += limit;

	return (opt);
}

/*
 * Verify that a value is in a range.  If it is not, resets it to
 * its default value if one is supplied, exits otherwise.
 *
 * When testing, can compare user_supplied to RC_KEYWORD or RC_POSITIONAL.
 */
static void
range_check(long *varp, char *name, long minimum, long maximum,
    long def_val, int user_supplied)
{
	if ((*varp < minimum) || (*varp > maximum)) {
		if (user_supplied != RC_DEFAULT) {
			(void) fprintf(stderr, gettext(
	    "mkfs: bad value for %s: %ld must be between %ld and %ld\n"),
			    name, *varp, minimum, maximum);
		}
		if (def_val != NO_DEFAULT) {
			if (user_supplied) {
				(void) fprintf(stderr,
				    gettext("mkfs: %s reset to default %ld\n"),
				    name, def_val);
			}
			*varp = def_val;
			return;
		}
		lockexit(2);
		/*NOTREACHED*/
	}
}
