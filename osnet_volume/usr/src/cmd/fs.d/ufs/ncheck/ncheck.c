/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ncheck.c	1.25	97/07/25 SMI"	/* SVr4.0 1.7 */
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
 * Copyright (c) 1986,1987,1988,1989,1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */

/*
 * ncheck -- obtain file names from reading filesystem
 */

#define	MAXNINDIR	(MAXBSIZE / sizeof (daddr_t))

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_fsdir.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "roll_log.h"

union {
	struct	fs	sblk;
	char xxx[SBSIZE];	/* because fs is variable length */
} real_fs;
#define	sblock real_fs.sblk

struct	dinode	*itab;
unsigned itab_size;


struct 	dinode	*gip;

/* inode list */
struct ilist {
	ino_t	ino;
	u_short	mode;
	uid_t	uid;
	gid_t	gid;
} *ilist;
int ilist_size = 0;	/* size of ilist[] */
int ilist_index = 0;	/* current index for storing into ilist; */
#define	ILIST_SZ_INCR	1000	/* initial size, amount to incr sz of ilist */
#define	MAX_ILIST_INDEX()	(ilist_size - 1)

struct	htab
{
	ino_t	h_ino;
	ino_t	h_pino;
	int	h_name_index;		/* index into string table */
} *htab;
unsigned htab_size;		/* how much malloc'd for htab */

/*
 * string table: used to hold filenames.
 */
char *strngtab;
int strngloc;
int strngtab_size;
#define	STRNGTAB_INCR	(1024*16)	/* amount to grow strngtab */
#define	MAX_STRNGTAB_INDEX()	(strngtab_size - 1)
#define	AVG_PATH_LEN	30		/* average (?) length of name */

long hsize;

struct dirstuff {
	int loc;
	struct dinode *ip;
	char dbuf[MAXBSIZE];
};


int	aflg = 0;
int	sflg = 0;
int	iflg = 0; /* number of inodes being searched for */
int	mflg = 0;
int	fi;
ino_t	ino;
int	nhent;

int	nerror;

long	atol();
daddr_t	bmap();
void	bread(daddr_t bno, char *buf, int cnt);
void	check(char *file);
int	dotname(struct direct *dp);
offset_t llseek();
struct htab *lookup(ino_t i, int ef);
void	pass1(struct dinode *ip);
void	pass2(struct dinode *ip);
void	pass3(struct dinode *ip);
void	pname(ino_t i, int lev);
char 	*strcpy();
void	usage();
struct direct *dreaddir();
void extend_ilist();
int extend_strngtab(unsigned int size);
u_char *extend_tbl(u_char *tbl, unsigned int *current_size,
	unsigned int new_size);

extern int	optind;
extern char	*optarg;

char *subopts [] = {
#define	M_FLAG		0
	"m",
	NULL
	};

main(argc, argv)
	int argc;
	char *argv[];
{
	long n;
	int	opt;
	char	*suboptions,	*value;
	int	suboption;
	char	*p;
	int	first = 0;

	extend_ilist();
	while ((opt = getopt(argc, argv, "ao:i:s")) != EOF) {
		switch (opt) {

		case 'a':
			aflg++;
			break;

		case 'o':
			/*
			 * ufs specific options.
			 */
			suboptions = optarg;
			while (*suboptions != '\0') {
				suboption = getsubopt(&suboptions,
					subopts, &value);
				switch (suboption) {

				case M_FLAG:
					mflg++;
					break;

				default:
					usage();
				}
			}
			break;

		case 'i':
			while ((p = (char *)strtok((first++ == 0 ? optarg : 0),
						    ", ")) != NULL) {
				if ((n = atoi(p)) == 0)
					break;
				ilist[iflg].ino = n;
				iflg++;
				ilist_index = iflg;
				if (iflg > MAX_ILIST_INDEX())
					extend_ilist();
			}
			break;

		case 's':
			sflg++;
			break;
#if 0
		case 'V':
			{
				int	opt_count;
				char	*opt_text;

				(void) fprintf(stdout, "ncheck -F ufs ");
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
#endif
		case '?':
			usage();
		}
	}
	argc -= optind;
	argv = &argv[optind];
	while (argc--) {
		check(*argv);
		argv++;
	}
	return (nerror);
}

void
check(file)
	char *file;
{
	register int i, j, c;

	fi = open64(file, 0);
	if (fi < 0) {
		(void) fprintf(stderr, "ncheck: cannot open %s\n", file);
		nerror++;
		return;
	}
	nhent = 0;
	(void) printf("%s:\n", file);
	sync();
	bread(SBLOCK, (char *)&sblock, SBSIZE);
	if (sblock.fs_magic != FS_MAGIC) {
		(void) printf("%s: not a ufs file system\n", file);
		nerror++;
		return;
	}

	/* If fs is logged, roll the log. */
	if (sblock.fs_logbno) {
		switch (rl_roll_log(file)) {
		case RL_SUCCESS:
			/*
			 * Reread the superblock.  Rolling the log may have
			 * changed it.
			 */
			bread(SBLOCK, (char *)&sblock, SBSIZE);
			break;
		case RL_SYSERR:
			(void) printf("Warning: cannot roll log for %s.  %s\n",
				file, strerror(errno));
			break;
		default:
			(void) printf("Warning: cannot roll log for %s.\n",
				file);
			break;
		}
	}

	itab = (struct dinode *)extend_tbl((u_char *)itab, &itab_size,
		(unsigned)(sblock.fs_ipg * sizeof (struct dinode)));
	if (itab == 0) {
		(void) fprintf(stderr,
			"ncheck: not enough memory for itab table\n");
		nerror++;
		return;
	}

	hsize = sblock.fs_ipg * sblock.fs_ncg - sblock.fs_cstotal.cs_nifree + 1;

	htab = (struct htab *)extend_tbl((u_char *)htab, &htab_size,
		(unsigned)(hsize * sizeof (struct htab)));
	if (htab == 0) {
		(void) fprintf(stderr,
			"ncheck: not enough memory for htab table\n");
		nerror++;
		return;
	}

	if (!extend_strngtab(AVG_PATH_LEN * hsize)) {
		(void) printf("not enough memory to allocate tables\n");
		nerror++;
		return;
	}
	strngloc = 0;

	ino = 0;
	for (c = 0; c < sblock.fs_ncg; c++) {
		bread(fsbtodb(&sblock, cgimin(&sblock, c)), (char *)itab,
		    (int)(sblock.fs_ipg * sizeof (struct dinode)));
		for (j = 0; j < sblock.fs_ipg; j++) {
			if (itab[j].di_smode != 0) {
				itab[j].di_mode = itab[j].di_smode;
				if (itab[j].di_suid != UID_LONG)
					itab[j].di_uid = itab[j].di_suid;
				if (itab[j].di_sgid != GID_LONG)
					itab[j].di_gid = itab[j].di_sgid;
				pass1(&itab[j]);
			}
			ino++;
		}
	}
	ilist[ilist_index++].ino = 0;
	if (ilist_index > MAX_ILIST_INDEX())
		extend_ilist();
	ino = 0;
	for (c = 0; c < sblock.fs_ncg; c++) {
		bread(fsbtodb(&sblock, cgimin(&sblock, c)), (char *)itab,
		    (int)(sblock.fs_ipg * sizeof (struct dinode)));
		for (j = 0; j < sblock.fs_ipg; j++) {

			if (itab[j].di_smode != 0) {
				itab[j].di_mode = itab[j].di_smode;
				pass2(&itab[j]);
			}
			ino++;
		}
	}
	ino = 0;
	for (c = 0; c < sblock.fs_ncg; c++) {
		bread(fsbtodb(&sblock, cgimin(&sblock, c)), (char *)itab,
		    (int)(sblock.fs_ipg * sizeof (struct dinode)));
		for (j = 0; j < sblock.fs_ipg; j++) {
			if (itab[j].di_smode != 0) {
				itab[j].di_mode = itab[j].di_smode;
				pass3(&itab[j]);
			}
			ino++;
		}
	}
	(void) close(fi);

	/*
	 * Clear those elements after inodes specified by "-i" out of
	 * ilist.
	 */
	for (i = iflg; i < ilist_index; i++) {
		ilist[i].ino = 0;
	}
	ilist_index = iflg;
}

void
pass1(ip)
	register struct dinode *ip;
{
	int i;

	if (mflg) {
		for (i = 0; i < iflg; i++)
			if (ino == ilist[i].ino) {
				ilist[i].mode = ip->di_mode;
				ilist[i].uid = ip->di_uid;
				ilist[i].gid = ip->di_gid;
			}
	}
	if ((ip->di_mode & IFMT) != IFDIR) {
		if (sflg == 0)
			return;
		if ((ip->di_mode & IFMT) == IFBLK ||
				(ip->di_mode & IFMT) == IFCHR ||
				ip->di_mode&(ISUID|ISGID)) {
			ilist[ilist_index].ino = ino;
			ilist[ilist_index].mode = ip->di_mode;
			ilist[ilist_index].uid = ip->di_uid;
			ilist[ilist_index].gid = ip->di_gid;
			if (++ilist_index > MAX_ILIST_INDEX())
				extend_ilist();
			return;
		}
	}
	(void) lookup(ino, 1);
}

void
pass2(ip)
	register struct dinode *ip;
{
	register struct direct *dp;
	struct dirstuff dirp;
	struct htab *hp;


	if ((ip->di_mode&IFMT) != IFDIR)
		return;
	dirp.loc = 0;
	dirp.ip = ip;
	gip = ip;
	for (dp = dreaddir(&dirp); dp != NULL; dp = dreaddir(&dirp)) {
		int nmlen;

		if (dp->d_ino == 0)
			continue;

		hp = lookup(dp->d_ino, 0);
		if (hp == 0)
			continue;

		if (dotname(dp))
			continue;
		hp->h_pino = ino;
		nmlen = strlen(dp->d_name);

		if (strngloc + nmlen + 1 > MAX_STRNGTAB_INDEX()) {
			if (!extend_strngtab(STRNGTAB_INCR)) {
				perror("ncheck: can't grow string table\n");
				exit(32);
			}
		}

		hp->h_name_index = strngloc;
		(void) strcpy(&strngtab[strngloc], dp->d_name);
		strngloc += nmlen + 1;
	}
}

void
pass3(ip)
	register struct dinode *ip;
{
	register struct direct *dp;
	struct dirstuff dirp;
	int k;

	if ((ip->di_mode&IFMT) != IFDIR)
		return;
	dirp.loc = 0;
	dirp.ip = ip;
	gip = ip;
	for (dp = dreaddir(&dirp); dp != NULL; dp = dreaddir(&dirp)) {
		if (aflg == 0 && dotname(dp))
			continue;

		if (sflg == 0 && iflg == 0)
			goto pr;
		for (k = 0; k < ilist_index && ilist[k].ino != 0; k++) {
			if (ilist[k].ino == dp->d_ino) {
				break;
			}
		}
		if (ilist[k].ino == 0)
			continue;
		if (mflg)
			(void) printf("mode %-6o uid %-5ld gid %-5ld ino ",
			    ilist[k].mode, ilist[k].uid, ilist[k].gid);
	pr:
		(void) printf("%-5u\t", dp->d_ino);
		pname(ino, 0);
		(void) printf("/%s", dp->d_name);
		if (lookup(dp->d_ino, 0))
			(void) printf("/.");
		(void) printf("\n");
	}
}

/*
 * get next entry in a directory.
 */
struct direct *
dreaddir(dirp)
	register struct dirstuff *dirp;
{
	register struct direct *dp;
	daddr_t lbn, d;

	for (;;) {

		if (dirp->loc >= (int)dirp->ip->di_size)
			return (NULL);
		if (blkoff(&sblock, dirp->loc) == 0) {

			lbn = lblkno(&sblock, dirp->loc);

			d = bmap(lbn);
			if (d == 0)
				return (NULL);

			bread(fsbtodb(&sblock, d), dirp->dbuf,
			    (int)dblksize(&sblock, dirp->ip, (int)lbn));

		}
		dp = (struct direct *)
		    (dirp->dbuf + blkoff(&sblock, dirp->loc));
		dirp->loc += dp->d_reclen;
		if (dp->d_ino == 0) {
			continue;
		}
		return (dp);
	}
}

dotname(dp)
	register struct direct *dp;
{

	if (dp->d_name[0] == '.') {
		if (dp->d_name[1] == 0 ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == 0))
			return (1);
	}
	return (0);
}

void
pname(i, lev)
	ino_t i;
	int lev;
{
	register struct htab *hp;

	if (i == UFSROOTINO)
		return;

	if ((hp = lookup(i, 0)) == 0) {
		(void) printf("???");
		return;
	}
	if (lev > 10) {
		(void) printf("...");
		return;
	}
	pname(hp->h_pino, ++lev);
	(void) printf("/%s", &(strngtab[hp->h_name_index]));

}

struct htab *
lookup(i, ef)
	ino_t i;
	int ef;
{
	register struct htab *hp;

	for (hp = &htab[(int)i%hsize]; hp->h_ino; ) {
		if (hp->h_ino == i)
			return (hp);
		if (++hp >= &htab[hsize])
			hp = htab;
	}

	if (ef == 0)
		return (0);
	if (++nhent >= hsize) {
		(void) fprintf(stderr, "ncheck: hsize of %ld is too small\n",
									hsize);
		exit(32);
	}
	hp->h_ino = i;
	return (hp);
}

void
bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
	int cnt;
{
	register i;
	int got;

	if (llseek(fi, (offset_t)bno * DEV_BSIZE, 0) == -1) {
		(void) fprintf(stderr, "ncheck: lseek error %ld\n",
							bno * DEV_BSIZE);

		for (i = 0; i < cnt; i++) {
			buf[i] = 0;
		}

		return;
	}

	got = read((int)fi, buf, cnt);

	if (got != cnt) {
		(void) fprintf(stderr,
			"ncheck: read error %d (wanted %d got %ld\n", cnt, got,
									bno);
		for (i = 0; i < cnt; i++)
			buf[i] = 0;
	}
}

daddr_t
bmap(i)
	daddr_t i;
{
	daddr_t ibuf[MAXNINDIR];

	if (i < NDADDR)
		return (gip->di_db[i]);
	i -= NDADDR;
	if (i > NINDIR(&sblock)) {
		(void) fprintf(stderr, "ncheck: %lu - huge directory\n", ino);
		return ((daddr_t)0);
	}

	bread(fsbtodb(&sblock, gip->di_ib[0]), (char *)ibuf, sizeof (ibuf));

	return (ibuf[i]);
}

void
usage()
{
	(void) fprintf(stderr,
		/*CSTYLED*/
		"ufs usage: ncheck [-F ufs] [generic options] [-a -i #list -s] [-o m] special\n");
	exit(32);
}


/*
 * Extend or create the inode list;
 * this is used to contains the list of inodes we've been
 * asked to check using the "-i" flag and to hold the
 * inode numbers of files which we detect as being
 * blk|char|setuid|setgid ("-s" flag support).
 * Preserves contents.
 */
void
extend_ilist()
{
	ilist_size += ILIST_SZ_INCR;
	ilist = (struct ilist *)realloc(ilist,
		(ilist_size * sizeof (struct ilist)));

	if (ilist == NULL) {
		perror("ncheck: not enough memory to grow ilist\n");
		exit(32);
	}
}

/*
 * Extend or create the string table.
 * Preserves contents.
 * Return non-zero for success.
 */
int
extend_strngtab(size)
	unsigned int size;
{
	strngtab_size += size;
	strngtab = (char *)realloc(strngtab, strngtab_size);

	return ((int)strngtab);
}

/*
 * Extend or create a table, throwing away previous
 * contents.
 * Return null on failure.
 */
u_char *
extend_tbl(tbl, current_size, new_size)
	u_char *tbl;
	unsigned int *current_size;	/* current size */
	unsigned int new_size;		/* bytes required */
{
	/*
	 * if we've already allocated tbl,
	 * but it is too small, free it.
	 * we don't realloc because we are throwing
	 * away its contents.
	 */

	if (tbl && (*current_size < new_size)) {
		free(tbl);
		tbl = NULL;
	}

	if (tbl == NULL) {
		tbl = (u_char *)malloc(new_size);
		if (tbl == 0)
			return ((u_char *)0);

		*current_size = new_size;
	}
	(void) memset(tbl, 0, new_size);

	return (tbl);
}
