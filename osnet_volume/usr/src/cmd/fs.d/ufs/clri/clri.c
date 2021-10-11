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
 * Copyright (c) 1986,1987,1988,1989,1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

#ident	"@(#)clri.c	1.20	97/11/14 SMI"	/* SVr4.0 1.4 */

/*
 * clri filsys inumber ...
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>

#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>

#include "roll_log.h"

#define	ISIZE	(sizeof (struct dinode))
#define	NI	(MAXBSIZE/ISIZE)

static struct dinode buf[NI];

static union {
	char		dummy[SBSIZE];
	struct fs	sblk;
} sb_un;
#define	sblock sb_un.sblk

static int status;

static int read_sb(int fd, const char *dev);
static int isnumber(const char *s);

int
main(int argc, char *argv[])
{
	int		i, f;
	unsigned int	n;
	int		j;
	offset_t	off;
	int32_t		gen;
	time_t		t;
	int		sbrr;

	if (argc < 3) {
		(void) printf("ufs usage: clri filsys inumber ...\n");
		return (35);
	}
	f = open64(argv[1], 2);
	if (f < 0) {
		(void) printf("cannot open %s\n", argv[1]);
		return (35);
	}

	if ((sbrr = read_sb(f, argv[1])) != 0) {
		return (sbrr);
	}

	/* If fs is logged, roll the log. */
	if (sblock.fs_logbno) {
		switch (rl_roll_log(argv[1])) {
		case RL_SUCCESS:
			/*
			 * Reread the superblock.  Rolling the log may have
			 * changed it.
			 */
			if ((sbrr = read_sb(f, argv[1])) != 0) {
				return (sbrr);
			}
			break;
		case RL_SYSERR:
			(void) printf("Warning: Cannot roll log for %s.  %s.  "
				"Inodes will be cleared anyway.\n",
				argv[1], strerror(errno));
			break;
		default:
			(void) printf("Cannot roll log for %s.  "
				"Inodes will be cleared anyway.\n",
				argv[1]);
			break;
		}
	}

	if (sblock.fs_magic != FS_MAGIC) {
		(void) printf("bad super block magic number\n");
		return (35);
	}

	for (i = 2; i < argc; i++) {
		if (!isnumber(argv[i])) {
			(void) printf("%s: is not a number\n", argv[i]);
			status = 1;
			continue;
		}
		n = atoi(argv[i]);
		if (n == 0) {
			(void) printf("%s: is zero\n", argv[i]);
			status = 1;
			continue;
		}
		off = fsbtodb(&sblock, itod(&sblock, n));
		off *= DEV_BSIZE;
		(void) llseek(f, off, 0);
		if (read(f, (char *)buf, sblock.fs_bsize) != sblock.fs_bsize) {
			(void) printf("%s: read error\n", argv[i]);
			status = 1;
		}
	}
	if (status)
		return (status+31);

	/*
	 * Update the time in superblock, so fsck will check this filesystem.
	 */
	(void) llseek(f, (offset_t)(SBLOCK * DEV_BSIZE), 0);
	(void) time(&t);
	sblock.fs_time = (time32_t)t;
	if (write(f, &sblock, SBSIZE) != SBSIZE) {
		(void) printf("cannot update %s\n", argv[1]);
		return (35);
	}

	for (i = 2; i < argc; i++) {
		n = atoi(argv[i]);
		(void) printf("clearing %u\n", n);
		off = fsbtodb(&sblock, itod(&sblock, n));
		off *= DEV_BSIZE;
		(void) llseek(f, off, 0);
		(void) read(f, (char *)buf, sblock.fs_bsize);
		j = itoo(&sblock, n);
		gen = buf[j].di_gen;
		memset(&buf[j], 0, ISIZE);
		buf[j].di_gen = gen + 1;
		(void) llseek(f, off, 0);
		(void) write(f, (char *)buf, sblock.fs_bsize);
	}
	if (status)
		return (status+31);
	(void) close(f);
	return (0);
}

static int
isnumber(const char *s)
{
	int c;

	while ((c = *s++) != '\0')
		if (c < '0' || c > '9')
			return (0);
	return (1);
}

static int
read_sb(int fd, const char *dev)
{
	(void) llseek(fd, (offset_t)(SBLOCK * DEV_BSIZE), 0);
	if (read(fd, &sblock, SBSIZE) != SBSIZE) {
		(void) printf("cannot read %s\n", dev);
		return (35);
	} else {
		return (0);
	}
}
