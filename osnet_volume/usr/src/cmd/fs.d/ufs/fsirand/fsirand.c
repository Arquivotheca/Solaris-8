/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fsirand.c	1.8	96/04/18 SMI"	/* SVr4.0 1.1	*/

/* ******************************************************************** */
/*			PROPRIETARY NOTICE (Combined)			*/

/* This source code is unpublished proprietary information		*/
/* constituting, or derived under license from AT&T's UNIX(r) System V. */
/* In addition, portions of such source code were derived from Berkeley */
/* 4.3 BSD under license from the Regents of the University of		*/
/* California.								*/



/*			Copyright Notice				*/

/* Notice of copyright on this source code product does not indicate	*/
/* publication.								*/

/* Copyright (c) 1984,1986,1987,1988,1989,1996  Sun Microsystems, Inc.	*/
/* All rights reserved.							*/

/* (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.			*/
/*		All rights reserved.					*/
/* ******************************************************************** */

/*
 * fsirand
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fs/ufs_fs.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>

extern int	errno;
extern offset_t	llseek();

extern long	lrand48();
extern void	srand48();

char fsbuf[SBSIZE];
struct dinode dibuf[8192/sizeof (struct dinode)];

static char	*strerror(/*int errnum*/);

void
main(argc, argv)
int	argc;
char	*argv[];
{
	struct fs *fs;
	int fd;
	char *dev;
	int bno;
	struct dinode *dip;
	int inum, imax;
	int i, n;
	offset_t seekaddr;
	int bsize;
	int pflag = 0;
	struct timeval timeval;

	argv++;
	argc--;
	if (argc > 0 && strcmp(*argv, "-p") == 0) {
		pflag++;
		argv++;
		argc--;
	}
	if (argc <= 0) {
		(void) fprintf(stderr, "Usage: fsirand [-p] special\n");
		exit(1);
	}
	dev = *argv;
	fd = open64(dev, pflag ? O_RDONLY : O_RDWR);
	if (fd == -1) {
		(void) fprintf(stderr, "fsirand: Cannot open %s: %s\n", dev,
		    strerror(errno));
		exit(1);
	}
	if (llseek(fd, (offset_t)SBLOCK * DEV_BSIZE, 0) == -1) {
		(void) fprintf(stderr,
		    "fsirand: Seek to superblock failed: %s\n",
		    strerror(errno));
		exit(1);
	}
	fs = (struct fs *) fsbuf;
	if ((n = read(fd, (char *) fs, SBSIZE)) != SBSIZE) {
		(void) fprintf(stderr,
		    "fsirand: Read of superblock failed: %s\n",
		    n == -1 ? strerror(errno) : "Short read");
		exit(1);
	}
	if (fs->fs_magic != FS_MAGIC) {
		(void) fprintf(stderr,
	"fsirand: Not a file system (bad magic number in superblock)\n");
		exit(1);
	}
	if (pflag) {
		(void) printf("fsid: %x %x\n", fs->fs_id[0], fs->fs_id[1]);
	} else {
		n = getpid();
		(void) gettimeofday(&timeval, (struct timezone *)NULL);
		srand48((long)(timeval.tv_sec + timeval.tv_usec + n));
	}
	bsize = INOPB(fs) * sizeof (struct dinode);
	inum = 0;
	imax = fs->fs_ipg * fs->fs_ncg;
	while (inum < imax) {
		bno = itod(fs, inum);
		seekaddr = (offset_t)fsbtodb(fs, bno) * DEV_BSIZE;
		if (llseek(fd, seekaddr, 0) == -1) {
			(void) fprintf(stderr,
			    "fsirand: Seek to %ld %ld failed: %s\n",
			    ((off_t *)&seekaddr)[0], ((off_t *)&seekaddr)[1],
			    strerror(errno));
			exit(1);
		}
		n = read(fd, (char *) dibuf, bsize);
		if (n != bsize) {
			(void) fprintf(stderr,
			    "fsirand: Read of ilist block failed: %s\n",
			    n == -1 ? strerror(errno) : "Short read");
			exit(1);
		}
		for (dip = dibuf; dip < &dibuf[INOPB(fs)]; dip++) {
			if (pflag) {
				(void) printf("ino %d gen %x\n", inum,
				    dip->di_gen);
			} else {
				dip->di_gen = lrand48();
			}
			inum++;
		}
		if (!pflag) {
			if (llseek(fd, seekaddr, 0) == -1) {
				(void) fprintf(stderr,
				    "fsirand: Seek to %ld %ld failed: %s\n",
				    ((off_t *)&seekaddr)[0],
				    ((off_t *)&seekaddr)[1],
				    seekaddr, strerror(errno));
				exit(1);
			}
			n = write(fd, (char *) dibuf, bsize);
			if (n != bsize) {
				(void) fprintf(stderr,
				"fsirand: Write of ilist block failed: %s\n",
				    n == -1 ? strerror(errno) : "Short write");
				exit(1);
			}
		}
	}
	if (!pflag) {
		(void) gettimeofday(&timeval, (struct timezone *)NULL);
		fs->fs_id[0] = timeval.tv_sec;
		fs->fs_id[1] = timeval.tv_usec + getpid();
		if (llseek(fd, (offset_t)SBLOCK * DEV_BSIZE, 0) == -1) {
			(void) fprintf(stderr,
			    "fsirand: Seek to superblock failed: %s\n",
			    strerror(errno));
			exit(1);
		}
		if ((n = write(fd, (char *) fs, SBSIZE)) != SBSIZE) {
			(void) fprintf(stderr,
			    "fsirand: Write of superblock failed: %s\n",
			    n == -1 ? strerror(errno) : "Short write");
			exit(1);
		}
	}
	for (i = 0; i < fs->fs_ncg; i++) {
		seekaddr = (offset_t)fsbtodb(fs, cgsblock(fs, i)) * DEV_BSIZE;
		if (llseek(fd,  seekaddr, 0) == -1) {
			(void) fprintf(stderr,
			"fsirand: Seek to alternate superblock failed: %s\n",
			    strerror(errno));
			exit(1);
		}
		if (pflag) {
			if ((n = read(fd, (char *) fs, SBSIZE)) != SBSIZE) {
				(void) fprintf(stderr,
			"fsirand: Read of alternate superblock failed: %s\n",
				    n == -1 ? strerror(errno) : "Short read");
				exit(1);
			}
			if (fs->fs_magic != FS_MAGIC) {
				(void) fprintf(stderr,
"fsirand: Not a valid file system (bad magic number in alternate superblock)\
\n");
				exit(1);
			}
		} else {
			if ((n = write(fd, (char *) fs, SBSIZE)) != SBSIZE) {
				(void) fprintf(stderr,
			"fsirand: Write of alternate superblock failed: %s\n",
				    n == -1 ? strerror(errno) : "Short write");
				exit(1);
			}
		}
	}
	exit(0);
	/* NOTREACHED */
}

static char *
strerror(errnum)
	int errnum;
{
	extern int sys_nerr;
	extern char *sys_errlist[];
	static char unknown_error[16+1];	/* "Error NNNNNNNNNN" + '\0' */

	if (errnum < 0 || errnum > sys_nerr) {
		(void) sprintf(unknown_error, "Error %d", errnum);
		return (unknown_error);
	} else
		return (sys_errlist[errnum]);
}
