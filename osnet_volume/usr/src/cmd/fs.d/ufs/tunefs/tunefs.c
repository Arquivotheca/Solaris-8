/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tunefs.c	1.17	99/03/22 SMI"	/* SVr4.0 1.6	*/

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
 * Copyright (c) 1986,1987,1988,1989,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

/*
 * tunefs: change layout parameters to an existing file system.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ustat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mntent.h>

#define	bcopy(f, t, n)    memcpy(t, f, n)
#define	bzero(s, n)	memset(s, 0, n)
#define	bcmp(s, d, n)	memcmp(s, d, n)

#define	index(s, r)	strchr(s, r)
#define	rindex(s, r)	strrchr(s, r)

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/fs/ufs_fs.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mnttab.h>
#include <sys/vfstab.h>
#include <sys/ustat.h>
#include <sys/filio.h>
#include <sys/fs/ufs_filio.h>

extern offset_t llseek();

union {
	struct	fs sb;
	char pad[SBSIZE];
} sbun;
#define	sblock sbun.sb

int fi;
struct ustat ustatarea;
extern int	optind;
extern char	*optarg;

static void usage();
static void getsb();
static void bwrite();
static void fatal();
static int bread();
static int isnumber();

extern char *getfullrawname(), *getfullblkname();

static void
searchvfstab(char **specialp)
{
	FILE *vfstab;
	struct vfstab vfsbuf;
	char *blockspecial;

	blockspecial = getfullblkname(*specialp);
	if (blockspecial == NULL)
		blockspecial = *specialp;

	if ((vfstab = fopen(VFSTAB, "r")) == NULL) {
		fprintf(stderr, "%s: ", VFSTAB);
		perror("open");
	}
	while (getvfsent(vfstab, &vfsbuf) == NULL)
		if (strcmp(vfsbuf.vfs_fstype, MNTTYPE_UFS) == 0)
			if ((strcmp(vfsbuf.vfs_mountp, *specialp) == 0) ||
			    (strcmp(vfsbuf.vfs_special, *specialp) == 0) ||
			    (strcmp(vfsbuf.vfs_special, blockspecial) == 0) ||
			    (strcmp(vfsbuf.vfs_fsckdev, *specialp) == 0)) {
				*specialp = strdup(vfsbuf.vfs_special);
				return;
			}
	fclose(vfstab);
}

static void
searchmnttab(char **specialp, char **mountpointp)
{
	FILE *mnttab;
	struct mnttab mntbuf;
	char *blockspecial;

	blockspecial = getfullblkname(*specialp);
	if (blockspecial == NULL)
		blockspecial = *specialp;

	if ((mnttab = fopen(MNTTAB, "r")) == NULL)
		return;
	while (getmntent(mnttab, &mntbuf) == NULL)
		if (strcmp(mntbuf.mnt_fstype, MNTTYPE_UFS) == 0)
			if ((strcmp(mntbuf.mnt_mountp, *specialp) == 0) ||
			    (strcmp(mntbuf.mnt_special, blockspecial) == 0) ||
			    (strcmp(mntbuf.mnt_special, *specialp) == 0)) {
				*specialp = strdup(mntbuf.mnt_special);
				*mountpointp = strdup(mntbuf.mnt_mountp);
				return;
			}
	fclose(mnttab);
}

void
main(argc, argv)
	int argc;
	char *argv[];
{
	char *special, *name, *mountpoint = NULL;
	struct stat64 st;
	int i, mountfd;
	int Aflag = 0;
	char *chg[2];
	int	opt;
	struct fiotune fiotune;


	if (argc < 3)
		usage();
	special = argv[argc - 1];

	/*
	 * For performance, don't search mnttab unless necessary
	 */

	if (stat64(special, &st) >= 0) {
		/*
		 * If mounted directory, search mnttab for special
		 */
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			if (st.st_ino == UFSROOTINO)
				searchmnttab(&special, &mountpoint);
		/*
		 * If mounted device, search mnttab for mountpoint
		 */
		} else if ((st.st_mode & S_IFMT) == S_IFBLK ||
			    (st.st_mode & S_IFMT) == S_IFCHR) {
				if (ustat(st.st_rdev, &ustatarea) >= 0)
					searchmnttab(&special, &mountpoint);
		}
	}
	/*
	 * Doesn't appear to be mounted; take ``unmounted'' path
	 */
	if (mountpoint == NULL)
		searchvfstab(&special);

	if ((special = getfullrawname(special)) == NULL) {
		fprintf(stderr, "tunefs: malloc failed\n");
		exit(32);
	}

	if (*special == '\0') {
		fprintf(stderr, "tunefs: Could not find raw device for %s\n",
		    argv[argc -1]);
		exit(32);
	}

	if (stat64(special, &st) < 0) {
		fprintf(stderr, "tunefs: "); perror(special);
		exit(31+1);
	}

	/*
	 * If a mountpoint has been found then we will ioctl() the file
	 * system instead of writing to the file system's device
	 */
	/* ustat() ok because max number of UFS inodes can fit in ino_t */
	if (ustat(st.st_rdev, &ustatarea) >= 0) {
		if (mountpoint == NULL) {
			printf("%s is mounted, can't tunefs\n", special);
			exit(32);
		}
	} else
		mountpoint = NULL;

	if ((st.st_mode & S_IFMT) != S_IFBLK &&
	    (st.st_mode & S_IFMT) != S_IFCHR)
		fatal("%s: not a block or character device", special);
	getsb(&sblock, special);
	while ((opt = getopt(argc, argv, "o:m:e:d:a:AV")) != EOF) {
		switch (opt) {

		case 'A':
			Aflag++;
			continue;

		case 'a':
			name = "maximum contiguous block count";
			if (!isnumber(optarg))
				fatal("%s: %s must be >= 1", *argv, name);
			i = atoi(optarg);
			if (i < 1)
				fatal("%s: %s must be >= 1", *argv, name);
			fprintf(stdout, "%s changes from %d to %d\n",
				name, sblock.fs_maxcontig, i);
			sblock.fs_maxcontig = i;
			continue;

		case 'd':
			name =
			    "rotational delay between contiguous blocks";
			if (!isnumber(optarg))
				fatal("%s: bad %s", *argv, name);
			i = atoi(optarg);
			if (i < 0)
				fatal("%s: bad %s", *argv, name);
			fprintf(stdout,
				"%s changes from %dms to %dms\n",
				name, sblock.fs_rotdelay, i);
			sblock.fs_rotdelay = i;
			continue;

		case 'e':
			name =
			    "maximum blocks per file in a cylinder group";
			if (!isnumber(optarg))
				fatal("%s: %s must be >= 1", *argv, name);
			i = atoi(optarg);
			if (i < 1)
				fatal("%s: %s must be >= 1", *argv, name);
			fprintf(stdout, "%s changes from %d to %d\n",
				name, sblock.fs_maxbpg, i);
			sblock.fs_maxbpg = i;
			continue;

		case 'm':
			name = "minimum percentage of free space";
			if (!isnumber(optarg))
				fatal("%s: bad %s", *argv, name);
			i = atoi(optarg);
			if (i < 0 || i > 99)
				fatal("%s: bad %s", *argv, name);
			fprintf(stdout,
				"%s changes from %d%% to %d%%\n",
				name, sblock.fs_minfree, i);
			sblock.fs_minfree = i;
			continue;

		case 'o':
			name = "optimization preference";
			chg[FS_OPTSPACE] = "space";
			chg[FS_OPTTIME] = "time";
			if (strcmp(optarg, chg[FS_OPTSPACE]) == 0)
				i = FS_OPTSPACE;
			else if (strcmp(optarg, chg[FS_OPTTIME]) == 0)
				i = FS_OPTTIME;
			else
			fatal("%s: bad %s (options are `space' or `time')",
					optarg, name);
			if (sblock.fs_optim == i) {
				fprintf(stdout,
					"%s remains unchanged as %s\n",
					name, chg[i]);
				continue;
			}
			fprintf(stdout,
				"%s changes from %s to %s\n",
				name, chg[sblock.fs_optim], chg[i]);
			sblock.fs_optim = i;
			continue;

		case 'V':
			{
				char	*opt_text;
				int	opt_count;

				(void) fprintf(stdout, "df -F ufs ");
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

		default:
			usage();
		}
	}
	if ((argc - optind) != 1)
		usage();
	if (mountpoint) {
		mountfd = open(mountpoint, O_RDONLY);
		if (mountfd == -1) {
			perror(mountpoint);
			fprintf(stderr,
				"tunefs: can't tune %s\n", mountpoint);
			exit(32);
		}
		fiotune.maxcontig = sblock.fs_maxcontig;
		fiotune.rotdelay = sblock.fs_rotdelay;
		fiotune.maxbpg = sblock.fs_maxbpg;
		fiotune.minfree = sblock.fs_minfree;
		fiotune.optim = sblock.fs_optim;
		if (ioctl(mountfd, _FIOTUNE, &fiotune) == -1) {
			perror(mountpoint);
			fprintf(stderr,
				"tunefs: can't tune %s\n", mountpoint);
			exit(32);
		}
		close(mountfd);
	} else {
		bwrite(SBLOCK, (char *)&sblock, SBSIZE);

		if (Aflag)
			for (i = 0; i < sblock.fs_ncg; i++)
				bwrite(fsbtodb(&sblock, cgsblock(&sblock, i)),
				    (char *)&sblock, SBSIZE);
	}

	close(fi);
	exit(0);
}

void
usage()
{
	fprintf(stderr, "ufs usage: tunefs tuneup-options special-device\n");
	fprintf(stderr, "where tuneup-options are:\n");
	fprintf(stderr, "\t-a maximum contiguous blocks\n");
	fprintf(stderr, "\t-d rotational delay between contiguous blocks\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-m minimum percentage of free space\n");
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	exit(31+2);
}

void
getsb(fs, file)
	struct fs *fs;
	char *file;
{

	fi = open64(file, O_RDWR);
	if (fi < 0) {
		fprintf(stderr, "Cannot open ");
		perror(file);
		exit(31+3);
	}
	if (bread(SBLOCK, (char *)fs, SBSIZE)) {
		fprintf(stderr, "Bad super block ");
		perror(file);
		exit(31+4);
	}
	if (fs->fs_magic != FS_MAGIC) {
		fprintf(stderr, "%s: bad magic number\n", file);
		exit(31+5);
	}
}

void
bwrite(blk, buf, size)
	char *buf;
	daddr_t blk;
	int size;
{
	if (llseek(fi, (offset_t)blk * DEV_BSIZE, 0) < 0) {
		perror("FS SEEK");
		exit(31+6);
	}
	if (write(fi, buf, size) != size) {
		perror("FS WRITE");
		exit(31+7);
	}
}

int
bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
{
	int	i;

	if (llseek(fi, (offset_t)bno * DEV_BSIZE, 0) < 0) {
		fprintf(stderr, "bread: ");
		perror("llseek");
		return (1);
	}
	if ((i = read(fi, buf, cnt)) != cnt) {
		perror("read");
		for (i = 0; i < sblock.fs_bsize; i++)
			buf[i] = 0;
		return (1);
	}
	return (0);
}

/* VARARGS1 */
void
fatal(fmt, arg1, arg2)
	char *fmt, *arg1, *arg2;
{
	fprintf(stderr, "tunefs: ");
	fprintf(stderr, fmt, arg1, arg2);
	putc('\n', stderr);
	exit(31+10);
}


int
isnumber(s)
	char *s;
{
	register c;

	while (c = *s++)
		if (c < '0' || c > '9')
			return (0);
	return (1);
}
