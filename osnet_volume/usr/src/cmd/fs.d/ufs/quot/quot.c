/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)quot.c	1.24	98/10/22 SMI"	/* SVr4.0 1.7 */
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
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

/*
 * quot
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <sys/mnttab.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	ISIZ	(MAXBSIZE/sizeof (struct dinode))
union {
	struct fs u_sblock;
	char dummy[SBSIZE];
} sb_un;
#define	sblock sb_un.u_sblock
struct dinode itab[MAXBSIZE/sizeof (struct dinode)];

struct du {
	struct	du *next;
	long	blocks;
	long	blocks30;
	long	blocks60;
	long	blocks90;
	long	nfiles;
	uid_t	uid;
	char	*u_name;
} **du;

#define	UHASH 8209
int	ndu;
#define	HASH(u)	((u) % UHASH)
struct	du *duhashtbl[UHASH];

#define	TSIZE	2048
int	sizes[TSIZE];
offset_t overflow;

int	nflg;
int	fflg;
int	cflg;
int	vflg;
int	hflg;
int	aflg;
long	now;

unsigned	ino;

char	*malloc();
extern offset_t llseek();

extern int	optind;
extern char	*optarg;

static void usage();
static void quotall();
static void acct();
static void bread();
static void report();
static int getdev();
static int check();
static struct du *adduid();
static struct du *lookup();
static void sortprep();
static void cleanup();

void
usage()
{
	fprintf(stderr, "ufs usage: quot [-nfcvha] [filesystem ...]\n");
}

void
main(argc, argv)
	int argc;
	char *argv[];
{
	int	opt;
	int	i;

	if (argc == 1) {
		fprintf(stderr,
		    "ufs Usage: quot [-nfcvha] [filesystem ...]\n");
		exit(32);
	}

	now = time(0);
	while ((opt = getopt(argc, argv, "nfcvhaV")) != EOF) {
		switch (opt) {

		case 'n':
			nflg++; break;

		case 'f':
			fflg++; break;

		case 'c':
			cflg++; break;

		case 'v':
			vflg++; break;

		case 'h':
			hflg++; break;

		case 'a':
			aflg++; break;

		case 'V':		/* Print command line */
			{
				char		*opt_text;
				int		opt_count;

				(void) fprintf(stdout, "quot -F UFS ");
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

		case '?':
			usage();
			exit(32);
		}
	}

	if (aflg) {
		quotall();
	}
	for (i = optind; i < argc; i++) {
		if ((getdev(&argv[i]) == 0) &&
			(check(argv[i], (char *)NULL) == 0)) {
				report();
				cleanup();
		}
	}
	exit(0);
}

void
quotall()
{
	FILE *fstab;
	struct mnttab mntp;
	char *cp;

	extern char *getfullrawname();

	fstab = fopen(MNTTAB, "r");
	if (fstab == NULL) {
		fprintf(stderr, "quot: no %s file\n", MNTTAB);
		exit(32);
	}
	while (getmntent(fstab, &mntp) == NULL) {
		if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0)
			continue;

		if ((cp = getfullrawname(mntp.mnt_special)) == NULL)
			continue;

		if (*cp == '\0')
			continue;

		if (check(cp, mntp.mnt_mountp) == 0) {
			report();
			cleanup();
		}

		free(cp);
	}
	fclose(fstab);
}

int
check(file, fsdir)
	char *file;
	char *fsdir;
{
	FILE *fstab;
	int i, j, nfiles;
	struct du **dp;
	diskaddr_t iblk;
	int c, fd;


	/*
	 * Initialize tables between checks;
	 * because of the qsort done in report()
	 * the hash tables must be rebuilt each time.
	 */
	for (i = 0; i < TSIZE; i++)
		sizes[i] = 0;
	overflow = 0LL;
	ndu = 0;
	fd = open64(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "quot: ");
		perror(file);
		exit(32);
	}
	printf("%s", file);
	if (fsdir == NULL) {
		struct mnttab mntp;

		fstab = fopen(MNTTAB, "r");
		if (fstab == NULL) {
			fprintf(stderr, "quot: no %s file\n", MNTTAB);
			exit(32);
		}
		while (getmntent(fstab, &mntp) == NULL) {
			if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0)
				continue;
			if (strcmp(mntp.mnt_special, file) == 0) {
				fsdir = mntp.mnt_mountp;
				break;
			}
		}
	}
	if (fsdir != NULL && *fsdir != '\0')
		printf(" (%s)", fsdir);
	printf(":\n");
	sync();
	bread(fd, (diskaddr_t)SBLOCK, (char *)&sblock, SBSIZE);
	if (nflg) {
		if (isdigit(c = getchar()))
			ungetc(c, stdin);
		else while (c != '\n' && c != EOF)
			c = getchar();
	}
	nfiles = sblock.fs_ipg * sblock.fs_ncg;
	for (ino = 0; ino < nfiles; ) {
		iblk = (diskaddr_t)fsbtodb(&sblock, itod(&sblock, ino));
		bread(fd, iblk, (char *)itab, sblock.fs_bsize);
		for (j = 0; j < INOPB(&sblock) && ino < nfiles; j++, ino++) {
			if (ino < UFSROOTINO)
				continue;
			acct(&itab[j]);
		}
	}
	close(fd);
	return (0);
}

void
acct(ip)
	struct dinode *ip;
{
	struct du *dp;
	struct du **hp;
	long blks, frags, size;
	int n;
	static fino;

	ip->di_mode = ip->di_smode;
	if (ip->di_suid != UID_LONG) {
		ip->di_uid = ip->di_suid;
	}
	if ((ip->di_mode & IFMT) == 0)
		return;
	/*
	 * By default, take block count in inode.  Otherwise (-h),
	 * take the size field and estimate the blocks allocated.
	 * The latter does not account for holes in files.
	 */
	if (!hflg)
		size = ip->di_blocks / 2;
	else {
		blks = lblkno(&sblock, ip->di_size);
		frags = blks * sblock.fs_frag +
			numfrags(&sblock, dblksize(&sblock, ip, blks));
		/*
		 * Must cast to offset_t because for a large file,
		 * frags multiplied by sblock.fs_fsize will not fit in a long.
		 * However, when divided by 1024, the end result will fit in
		 * the 32 bit size variable (40 bit UFS).
		 */
	    size = (long)((offset_t)frags * (offset_t)sblock.fs_fsize / 1024);
	}
	if (cflg) {
		if ((ip->di_mode&IFMT) != IFDIR && (ip->di_mode&IFMT) != IFREG)
			return;
		if (size >= TSIZE) {
			overflow += (offset_t)size;
			size = TSIZE-1;
		}
		sizes[size]++;
		return;
	}
	dp = lookup(ip->di_uid);
	if (dp == NULL)
		return;
	dp->blocks += size;
#define	DAY (60 * 60 * 24)	/* seconds per day */
	if (now - ip->di_atime > 30 * DAY)
		dp->blocks30 += size;
	if (now - ip->di_atime > 60 * DAY)
		dp->blocks60 += size;
	if (now - ip->di_atime > 90 * DAY)
		dp->blocks90 += size;
	dp->nfiles++;
	while (nflg) {
		char *np;

		if (fino == 0)
			if (scanf("%d", &fino) <= 0)
				return;
		if (fino > ino)
			return;
		if (fino < ino) {
			while ((n = getchar()) != '\n' && n != EOF)
				;
			fino = 0;
			continue;
		}
		if (dp->u_name)
			printf("%.7s	", dp->u_name);
		else
			printf("%ld	", (long)ip->di_uid);
		while ((n = getchar()) == ' ' || n == '\t')
			;
		putchar(n);
		while (n != EOF && n != '\n') {
			n = getchar();
			putchar(n);
		}
		fino = 0;
		break;
	}
}

void
bread(fd, bno, buf, cnt)
	int		fd;
	diskaddr_t	bno;	/* Probably could be a daddr_t */
	char		*buf;
	int		cnt;
{
	int	ret;

	if (llseek(fd, (offset_t)(bno * DEV_BSIZE), SEEK_SET) < 0) {
		perror("llseek");
		exit(32);
	}

	if ((ret = read(fd, buf, cnt)) != cnt) {
		fprintf(stderr, "quot: read returns %d (cnt = %d)\n", ret, cnt);
		fprintf(stderr, "quot: read error at block %lld\n", bno);
		perror("read");
		exit(32);
	}
}

qcmp(p1, p2)
	struct du **p1, **p2;
{
	char *s1, *s2;

	if ((*p1)->blocks > (*p2)->blocks)
		return (-1);
	if ((*p1)->blocks < (*p2)->blocks)
		return (1);
	s1 = (*p1)->u_name;
	if (s1 == NULL)
		return (0);
	s2 = (*p2)->u_name;
	if (s2 == NULL)
		return (0);
	return (strcmp(s1, s2));
}

void
report()
{
	int i;
	struct du **dp;
	int cnt;

	if (nflg)
		return;
	if (cflg) {
		long t = 0;

		for (i = 0; i < TSIZE - 1; i++)
			if (sizes[i]) {
				t += i*sizes[i];
				printf("%d	%d	%d\n", i, sizes[i], t);
			}
		if (sizes[TSIZE -1 ])
			printf("%d	%d	%lld\n", TSIZE - 1,
			    sizes[TSIZE - 1], overflow + (offset_t)t);
		return;
	}
	sortprep();
	qsort(du, ndu, sizeof (du[0]), qcmp);
	for (cnt = 0, dp = &du[0]; dp && cnt != ndu; dp++, cnt++) {
		char *cp;

		if ((*dp)->blocks == 0)
			return;
		printf("%5d\t", (*dp)->blocks);
		if (fflg)
			printf("%5d\t", (*dp)->nfiles);

		if ((*dp)->u_name)
			printf("%-8s", (*dp)->u_name);
		else
			printf("#%-8ld", (long)(*dp)->uid);
		if (vflg)
			printf("\t%5d\t%5d\t%5d",
			    (*dp)->blocks30, (*dp)->blocks60, (*dp)->blocks90);
		printf("\n");
	}
}



int
getdev(devpp)
	char **devpp;
{
	struct stat64 statb;
	FILE *fstab;
	struct mnttab mntp;
	char *cp;	/* Pointer to raw device name */

	extern char *getfullrawname();

	if (stat64(*devpp, &statb) < 0) {
		perror(*devpp);
		exit(32);
	}
	if ((statb.st_mode & S_IFMT) == S_IFCHR)
		return (0);
	if ((statb.st_mode & S_IFMT) == S_IFBLK) {
		/* If we can't get the raw name, keep the block name */
		if ((cp = getfullrawname(*devpp)) != NULL)
			*devpp = strdup(cp);
		return (0);
	}
	fstab = fopen(MNTTAB, "r");
	if (fstab == NULL) {
		fprintf(stderr, "quot: no %s file\n", MNTTAB);
		exit(32);
	}
	while (getmntent(fstab, &mntp) == NULL) {
		if (strcmp(mntp.mnt_mountp, *devpp) == 0) {
			if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0) {
				fprintf(stderr,
				    "quot: %s not ufs filesystem\n",
				    *devpp);
				exit(32);
			}
			/* If we can't get the raw name, use the block name */
			if ((cp = getfullrawname(mntp.mnt_special)) == NULL)
				cp = mntp.mnt_special;
			*devpp = strdup(cp);
			fclose(fstab);
			return (0);
		}
	}
	fclose(fstab);
	fprintf(stderr, "quot: %s doesn't appear to be a filesystem.\n",
	    *devpp);
	usage();
	exit(32);
}

static struct du *
lookup(uid_t uid)
{
	struct	passwd *pwp;
	struct	du *up;

	for (up = duhashtbl[uid % UHASH]; up != 0; up = up->next) {
		if (up->uid == uid)
			return (up);
	}

	pwp = getpwuid(uid);

	up = adduid(uid);
	if (up && pwp) {
		up->u_name = strdup(pwp->pw_name);
	}
	return (up);
}

static struct du *
adduid(uid_t uid)
{
	struct du *up, **uhp;

	up = (struct du *)calloc(1, sizeof (struct du));
	if (up == NULL) {
		(void) fprintf(stderr,
			"out of memory for du structures\n");
			exit(32);
	}

	uhp = &duhashtbl[uid % UHASH];
	up->next = *uhp;
	*uhp = up;
	up->uid = uid;
	up->u_name = NULL;
	ndu++;
	return (up);
}

static void
sortprep()
{
	struct du **dp, **tp, *ep;
	struct du **hp;
	int i, cnt = 0;

	dp = NULL;

	dp = (struct du **)calloc(ndu, sizeof (struct du **));
	if (dp == NULL) {
		(void) fprintf(stderr,
			"out of memory for du structures\n");
			exit(32);
	}

	for (hp = duhashtbl, i = 0; i != UHASH; i++) {
		if (hp[i] == NULL)
			continue;

		for (ep = hp[i]; ep; ep = ep->next) {
			dp[cnt++] = ep;
		}
	}
	du = dp;
}

static void
cleanup()
{
	int		i;
	struct du 	*ep, *next;

	/*
	 * Release memory from hash table and du
	 */

	if (du) {
		free(du);
		du = NULL;
	}


	for (i = 0; i != UHASH; i++) {
		if (duhashtbl[i] == NULL)
			continue;
		ep = duhashtbl[i];
		while (ep) {
			next = ep->next;
			if (ep->u_name) {
				free(ep->u_name);
			}
			free(ep);
			ep = next;
		}
		duhashtbl[i] = NULL;
	}
}
