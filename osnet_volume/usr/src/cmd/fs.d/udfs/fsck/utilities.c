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
 * Copyright (c) 1986,1987,1988,1989,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */
/*
 * Copyright (c) 1980, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#pragma ident	"@(#)utilities.c	1.11	99/02/13 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <malloc.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/filio.h>
#include <sys/vnode.h>
#include <sys/mnttab.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfstab.h>
#include <sys/sysmacros.h>
#include <sys/fs/udf_volume.h>
#include "fsck.h"
#include <sys/lockfs.h>
#include <locale.h>

extern int32_t	verifytag(struct tag *, uint32_t, struct tag *, int);
extern char	*tagerrs[];
extern void	maketag(struct tag *, struct tag *);
extern char	*hasvfsopt(struct vfstab *, char *);
static struct bufarea *getdatablk(daddr_t, long);
static struct bufarea *getblk(struct bufarea *, daddr_t, long);

void	flush(int32_t, struct bufarea *);
int32_t	bread(int32_t, char *, daddr_t, long);
void	bwrite(int, char *, daddr_t, long);
static int32_t	getline(FILE *, char *, int32_t);

static long	diskreads, totalreads;	/* Disk cache statistics */
offset_t	llseek();
extern unsigned int largefile_count;

/*
 * An unexpected inconsistency occured.
 * Die if preening, otherwise just print message and continue.
 */
/* VARARGS1 */
void
pfatal(s, a1, a2, a3)
	char *s;
{

	if (preen) {
		(void) printf("%s: ", devname);
		(void) printf(s, a1, a2, a3);
		(void) printf("\n");
		(void) printf(
		    gettext("%s: UNEXPECTED INCONSISTENCY; RUN fsck "
			"MANUALLY.\n"), devname);
		exit(36);
	}
	(void) printf(s, a1, a2, a3);
}

/*
 * Pwarn just prints a message when not preening,
 * or a warning (preceded by filename) when preening.
 */
/* VARARGS1 */
void
pwarn(s, a1, a2, a3, a4, a5, a6)
	char *s;
{

	if (preen)
		(void) printf("%s: ", devname);
	(void) printf(s, a1, a2, a3, a4, a5, a6);
}


/* VARARGS1 */
void
errexit(s1, s2, s3, s4)
	char *s1;
{
	(void) printf(s1, s2, s3, s4);
	exit(39);
}

void
markbusy(daddr_t block, long count)
{
	register int i;

	count = roundup(count, secsize) / secsize;
	for (i = 0; i < count; i++, block++) {
		if ((unsigned)block > part_len) {
			pwarn(gettext("Block %lx out of range\n"), block);
			break;
		}
		if (testbusy(block))
			pwarn(gettext("Dup block %lx\n"), block);
		else {
			n_blks++;
			setbusy(block);
		}
	}
}

void
printfree()
{
	int i, startfree, endfree;

	startfree = -1;
	for (i = 0; i < part_len; i++) {
		if (!testbusy(i)) {
			if (startfree <= 0)
				startfree = i;
			endfree = i;
		} else if (startfree >= 0) {
			(void) printf("free: %x-%x\n", startfree, endfree - 1);
			startfree = -1;
		}
	}
	if (startfree >= 0) {
		(void) printf("free: %x-%x\n", startfree, endfree);
	}
}

struct bufarea *
getfilentry(uint32_t block, int len)
{
	struct bufarea *bp;
	struct file_entry *fp;
	int err;

	if (len > fsbsize) {
		(void) printf(gettext("File entry at %x is too long "
			"(%d bytes)\n"), block, len);
		len = fsbsize;
	}
	bp = getdatablk((daddr_t)(block + part_start), fsbsize);
	if (bp->b_errs) {
		bp->b_flags &= ~B_INUSE;
		return (NULL);
	}
	/* LINTED */
	fp = (struct file_entry *)bp->b_un.b_buf;
	err = verifytag(&fp->fe_tag, block, &fp->fe_tag, UD_FILE_ENTRY);
	if (err) {
		(void) printf(gettext("Tag error %s or bad file entry, "
			"tag=%d\n"), tagerrs[err], fp->fe_tag.tag_id);
		bp->b_flags &= ~B_INUSE;
		return (NULL);
	}
	return (bp);
}

void
putfilentry(struct bufarea *bp)
{
	struct file_entry *fp;

	/* LINTED */
	fp = (struct file_entry *)bp->b_un.b_buf;
	maketag(&fp->fe_tag, &fp->fe_tag);
}


int32_t
reply(char *question)
{
	char line[80];

	if (preen)
		pfatal(gettext("INTERNAL ERROR: GOT TO reply()"));
	(void) printf("\n%s? ", question);
	if (nflag || fswritefd < 0) {
		(void) printf(gettext(" no\n\n"));
		iscorrupt = 1;		/* known to be corrupt */
		return (0);
	}
	if (yflag) {
		(void) printf(gettext(" yes\n\n"));
		return (1);
	}
	if (getline(stdin, line, sizeof (line)) == EOF)
		errexit("\n");
	(void) printf("\n");
	if (line[0] == 'y' || line[0] == 'Y')
		return (1);
	else {
		iscorrupt = 1;		/* known to be corrupt */
		return (0);
	}
}

int32_t
getline(FILE *fp, char *loc, int32_t maxlen)
{
	register n;
	register char *p, *lastloc;

	p = loc;
	lastloc = &p[maxlen-1];
	while ((n = getc(fp)) != '\n') {
		if (n == EOF)
			return (EOF);
		if (!isspace(n) && p < lastloc)
			*p++ = n;
	}
	*p = 0;
	return (p - loc);
}
/*
 * Malloc buffers and set up cache.
 */
void
bufinit()
{
	register struct bufarea *bp;
	long bufcnt, i;
	char *bufp;

	bufp = malloc((unsigned int)fsbsize);
	if (bufp == 0)
		errexit(gettext("cannot allocate buffer pool\n"));
	bufhead.b_next = bufhead.b_prev = &bufhead;
	bufcnt = MAXBUFSPACE / fsbsize;
	if (bufcnt < MINBUFS)
		bufcnt = MINBUFS;
	for (i = 0; i < bufcnt; i++) {
		bp = (struct bufarea *)malloc(sizeof (struct bufarea));
		bufp = malloc((unsigned int)fsbsize);
		if (bp == NULL || bufp == NULL) {
			if (i >= MINBUFS)
				break;
			errexit(gettext("cannot allocate buffer pool\n"));
		}
		bp->b_un.b_buf = bufp;
		bp->b_prev = &bufhead;
		bp->b_next = bufhead.b_next;
		bufhead.b_next->b_prev = bp;
		bufhead.b_next = bp;
		initbarea(bp);
	}
	bufhead.b_size = i;	/* save number of buffers */
	pbp = pdirbp = NULL;
}

/*
 * Manage a cache of directory blocks.
 */
static struct bufarea *
getdatablk(daddr_t blkno, long size)
{
	register struct bufarea *bp;

	for (bp = bufhead.b_next; bp != &bufhead; bp = bp->b_next)
		if (bp->b_bno == fsbtodb(blkno))
			goto foundit;
	for (bp = bufhead.b_prev; bp != &bufhead; bp = bp->b_prev)
		if ((bp->b_flags & B_INUSE) == 0)
			break;
	if (bp == &bufhead)
		errexit(gettext("deadlocked buffer pool\n"));
	(void) getblk(bp, blkno, size);
	/* fall through */
foundit:
	totalreads++;
	bp->b_prev->b_next = bp->b_next;
	bp->b_next->b_prev = bp->b_prev;
	bp->b_prev = &bufhead;
	bp->b_next = bufhead.b_next;
	bufhead.b_next->b_prev = bp;
	bufhead.b_next = bp;
	bp->b_flags |= B_INUSE;
	return (bp);
}

static struct bufarea *
getblk(struct bufarea *bp, daddr_t blk, long size)
{
	daddr_t dblk;

	dblk = fsbtodb(blk);
	if (bp->b_bno == dblk)
		return (bp);
	flush(fswritefd, bp);
	diskreads++;
	bp->b_errs = bread(fsreadfd, bp->b_un.b_buf, dblk, size);
	bp->b_bno = dblk;
	bp->b_size = size;
	return (bp);
}

void
flush(int32_t fd, struct bufarea *bp)
{
	if (!bp->b_dirty)
		return;
	if (bp->b_errs != 0)
		pfatal(gettext("WRITING ZERO'ED BLOCK %d TO DISK\n"),
			bp->b_bno);
	bp->b_dirty = 0;
	bp->b_errs = 0;
	bwrite(fd, bp->b_un.b_buf, bp->b_bno, (long)bp->b_size);
}

static void
rwerror(char *mesg, daddr_t blk)
{

	if (preen == 0)
		(void) printf("\n");
	pfatal(gettext("CANNOT %s: BLK %ld"), mesg, blk);
	if (reply(gettext("CONTINUE")) == 0)
		errexit(gettext("Program terminated\n"));
}

void
ckfini()
{
	struct bufarea *bp, *nbp;
	int cnt = 0;

	for (bp = bufhead.b_prev; bp && bp != &bufhead; bp = nbp) {
		cnt++;
		flush(fswritefd, bp);
		nbp = bp->b_prev;
		free(bp->b_un.b_buf);
		free((char *)bp);
	}
	pbp = pdirbp = NULL;
	if (bufhead.b_size != cnt)
		errexit(gettext("Panic: lost %d buffers\n"),
			bufhead.b_size - cnt);
	if (debug)
		(void) printf("cache missed %ld of %ld (%ld%%)\n",
		    diskreads, totalreads,
		    totalreads ? diskreads * 100 / totalreads : 0);
	(void) close(fsreadfd);
	(void) close(fswritefd);
}

int32_t
bread(int fd, char *buf, daddr_t blk, long size)
{
	char *cp;
	int i, errs;
	offset_t offset = ldbtob(blk);
	offset_t addr;

	if (llseek(fd, offset, 0) < 0)
		rwerror(gettext("SEEK"), blk);
	else if (read(fd, buf, (int)size) == size)
		return (0);
	rwerror(gettext("READ"), blk);
	if (llseek(fd, offset, 0) < 0)
		rwerror(gettext("SEEK"), blk);
	errs = 0;
	bzero(buf, (int)size);
	pwarn(gettext("THE FOLLOWING SECTORS COULD NOT BE READ:"));
	for (cp = buf, i = 0; i < btodb(size); i++, cp += DEV_BSIZE) {
		addr = ldbtob(blk + i);
		if (llseek(fd, addr, SEEK_CUR) < 0 ||
		    read(fd, cp, (int)secsize) < 0) {
			(void) printf(" %ld", blk + i);
			errs++;
		}
	}
	(void) printf("\n");
	return (errs);
}

void
bwrite(int fd, char *buf, daddr_t blk, long size)
{
	int i, n;
	char *cp;
	offset_t offset = ldbtob(blk);
	offset_t addr;

	if (fd < 0)
		return;
	if (llseek(fd, offset, 0) < 0)
		rwerror(gettext("SEEK"), blk);
	else if (write(fd, buf, (int)size) == size) {
		fsmodified = 1;
		return;
	}
	rwerror(gettext("WRITE"), blk);
	if (llseek(fd, offset, 0) < 0)
		rwerror(gettext("SEEK"), blk);
	pwarn(gettext("THE FOLLOWING SECTORS COULD NOT BE WRITTEN:"));
	for (cp = buf, i = 0; i < btodb(size); i++, cp += DEV_BSIZE) {
		n = 0;
		addr = ldbtob(blk + i);
		if (llseek(fd, addr, SEEK_CUR) < 0 ||
		    (n = write(fd, cp, DEV_BSIZE)) < 0) {
			(void) printf(" %ld", blk + i);
		} else if (n > 0) {
			fsmodified = 1;
		}

	}
	(void) printf("\n");
}

void
catch()
{
	ckfini();
	exit(37);
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit()
{
	extern returntosingle;

	(void) printf(gettext("returning to single-user after filesystem "
		"check\n"));
	returntosingle = 1;
	(void) signal(SIGQUIT, SIG_DFL);
}

/*
 * determine whether an inode should be fixed.
 */
/* ARGSUSED1 */
int32_t
dofix(struct inodesc *idesc, char *msg)
{

	switch (idesc->id_fix) {

	case DONTKNOW:
		pwarn(msg);
		if (preen) {
			(void) printf(gettext(" (SALVAGED)\n"));
			idesc->id_fix = FIX;
			return (ALTERED);
		}
		if (reply(gettext("SALVAGE")) == 0) {
			idesc->id_fix = NOFIX;
			return (0);
		}
		idesc->id_fix = FIX;
		return (ALTERED);

	case FIX:
		return (ALTERED);

	case NOFIX:
		return (0);

	default:
		errexit(gettext("UNKNOWN INODESC FIX MODE %d\n"),
			idesc->id_fix);
	}
	/* NOTREACHED */
}

/*
 * Check to see if unraw version of name is already mounted.
 * Since we do not believe /etc/mnttab, we stat the mount point
 * to see if it is really looks mounted.
 */
mounted(char *name)
{
	int found = 0;
	struct mnttab mnt;
	FILE *mnttab;
	struct stat device_stat, mount_stat;
	char *blkname, *unrawname();
	int err;

	mnttab = fopen(MNTTAB, "r");
	if (mnttab == NULL) {
		(void) printf(gettext("can't open %s\n"), MNTTAB);
		return (0);
	}
	blkname = unrawname(name);
	while ((getmntent(mnttab, &mnt)) == NULL) {
		if (strcmp(mnt.mnt_fstype, MNTTYPE_UDFS) != 0) {
			continue;
		}
		if (strcmp(blkname, mnt.mnt_special) == 0) {
			err = stat(mnt.mnt_mountp, &mount_stat);
			err |= stat(mnt.mnt_special, &device_stat);
			if (err < 0)
				continue;
			if (device_stat.st_rdev == mount_stat.st_dev) {
				(void) strncpy(mnt.mnt_mountp, mountpoint,
					sizeof (mountpoint));
				if (hasmntopt(&mnt, MNTOPT_RO) != 0)
					found = 2;	/* mounted as RO */
				else
					found = 1; 	/* mounted as R/W */
			}
			break;
		}
	}
	(void) fclose(mnttab);
	return (found);
}

/*
 * Check to see if name corresponds to an entry in vfstab, and that the entry
 * does not have option ro.
 */
writable(char *name)
{
	int rw = 1;
	struct vfstab vfsbuf;
	FILE *vfstab;
	char *blkname, *unrawname();

	vfstab = fopen(VFSTAB, "r");
	if (vfstab == NULL) {
		(void) printf(gettext("can't open %s\n"), VFSTAB);
		return (1);
	}
	blkname = unrawname(name);
	if ((getvfsspec(vfstab, &vfsbuf, blkname) == 0) &&
	    (vfsbuf.vfs_fstype != NULL) &&
	    (strcmp(vfsbuf.vfs_fstype, MNTTYPE_UDFS) == 0) &&
	    (hasvfsopt(&vfsbuf, MNTOPT_RO))) {
		rw = 0;
	}
	(void) fclose(vfstab);
	return (rw);
}

/*
 * print out clean info
 */
void
printclean()
{
	char	*s;

	switch (lvintp->lvid_int_type) {

	case LVI_CLOSE:
		s = gettext("clean");
		break;

	case LVI_OPEN:
		s = gettext("active");
		break;

	default:
		s = gettext("unknown");
	}

	if (preen)
		pwarn(gettext("is %s.\n"), s);
	else
		(void) printf("** %s is %s.\n", devname, s);
}
