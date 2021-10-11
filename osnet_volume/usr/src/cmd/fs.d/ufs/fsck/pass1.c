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
 * Copyright (c) 1986,1987,1988,1989,1996,1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
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

#ident	"@(#)pass1.c	1.30	99/08/12 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/mntent.h>

#define	bcopy(f, t, n)    memcpy(t, f, n)
#define	bzero(s, n)	memset(s, 0, n)
#define	bcmp(s, d, n)	memcmp(s, d, n)

#define	index(s, r)	strchr(s, r)
#define	rindex(s, r)	strrchr(s, r)

#include <sys/fs/ufs_fs.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_log.h>
#include "fsck.h"

/*
 * for each large file ( size > MAXOFF_T) this global counter
 * gets incremented here.
 */

extern unsigned int largefile_count;

static daddr_t badblk;
static daddr_t dupblk;
int pass1check();
struct dinode *getnextinode();

pass1()
{
	unsigned int c, i, j;
	struct dinode *dp;
	struct zlncnt *zlnp;
	int ndb, cgd;
	struct inodesc idesc;
	ino_t inumber, shadow;
	ino_t maxinumber;

	/*
	 * Set file system reserved blocks in used block map.
	 */
	for (c = 0; c < sblock.fs_ncg; c++) {
		cgd = cgdmin(&sblock, c);
		if (c == 0) {
			i = cgbase(&sblock, c);
			cgd += howmany(sblock.fs_cssize, sblock.fs_fsize);
		} else
			i = cgsblock(&sblock, c);
		for (; i < cgd; i++)
			setbmap(i);
	}
	/*
	 * Record log blocks
	 */
	if (islog && islogok && sblock.fs_logbno) {
		struct bufarea *bp;
		extent_block_t *ebp;
		extent_t *ep;
		daddr_t nfno, fno;
		int i, j, k;

		bp = getdatablk(dbtofsb(&sblock, sblock.fs_logbno),
							sblock.fs_bsize);
		ebp = (void *)bp->b_un.b_buf;
		ep = &ebp->extents[0];
		for (i = 0; i < ebp->nextents; ++i, ++ep) {
			fno = (daddr_t)(dbtofsb(&sblock, ep->pbno));
			nfno = (daddr_t)(dbtofsb(&sblock, ep->nbno));
			for (j = 0; j < nfno; ++j, ++fno)
				setbmap(fno);
		}
		bp->b_flags &= ~B_INUSE;

		fno = dbtofsb(&sblock, sblock.fs_logbno);
		for (j = 0; j < sblock.fs_frag; ++j, ++fno)
			setbmap(fno);
	}
	/*
	 * Find all allocated blocks.
	 */
	bzero((char *)&idesc, sizeof (struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = pass1check;
	inumber = 0;
	n_files = n_blks = 0;
	resetinodebuf();
	maxinumber = sblock.fs_ncg * sblock.fs_ipg;
	for (c = 0; c < sblock.fs_ncg; c++) {
		for (i = 0; i < sblock.fs_ipg; i++, inumber++) {
			if (inumber < UFSROOTINO)
				continue;
			dp = getnextinode(inumber);
			if ((dp->di_mode & IFMT) == 0) {
				/* mode and type of file is not set */
				if (bcmp((char *)dp->di_db, (char *)zino.di_db,
					NDADDR * sizeof (daddr_t)) ||
				    bcmp((char *)dp->di_ib, (char *)zino.di_ib,
					NIADDR * sizeof (daddr_t)) ||
				    dp->di_mode || dp->di_size) {
					pfatal("PARTIALLY ALLOCATED INODE I=%u",
						inumber);
					if (reply("CLEAR") == 1) {
						dp = ginode(inumber);
						clearinode(dp);
						inodirty();
					}
				}
				statemap[inumber] = USTATE;
				continue;
			}
			lastino = inumber;
			if (dp->di_size > (u_offset_t)UFS_MAXOFFSET_T) {
				if (debug)
					printf("bad size %llu:",
							dp->di_size);
				goto unknown;
			}
			if (!preen && (dp->di_mode & IFMT) == IFMT &&
			    reply("BAD MODE: MAKE IT A FILE") == 1) {
				dp = ginode(inumber);
				dp->di_size = (u_offset_t)sblock.fs_fsize;
				dp->di_mode = IFREG|0600;
				inodirty();
			}
			/* number of blocks is a 32 bit value */
			ndb = (int)howmany(dp->di_size,
						(u_offset_t)sblock.fs_bsize);
			if (dp->di_oeftflag == oEFT_MAGIC) {
				dp->di_oeftflag = 0; /* XXX migration aid */
				inodirty();
			}
			if (ndb < 0) {
				if (debug)
					printf("bad size %llu ndb %d:",
					dp->di_size, ndb);
				goto unknown;
			}
			if ((dp->di_mode & IFMT) == IFBLK ||
			    (dp->di_mode & IFMT) == IFCHR) {
			    for (j = 0; j < NDADDR; j++) {
				if (dp->di_db[j] != 0 &&
				    &dp->di_db[j] != &dp->di_ordev) {
					if (debug) {
printf("special file contains value %d at indirect addr[%d] - should be 0\n",
						dp->di_db[j], j);
					}
					goto unknown;
				}
			    }
			} else {
			    for (j = ndb; j < NDADDR; j++)
				if (dp->di_db[j] != 0) {
					if (debug) {
			    printf("bad direct addr[%lld]: 0x%x mode:%o\n",
						j, dp->di_db[j], dp->di_mode);
					}
					goto unknown;
				}
			}
			for (j = 0, ndb -= NDADDR; ndb > 0; j++)
				ndb /= NINDIR(&sblock);
			for (; j < NIADDR; j++)
				if (dp->di_ib[j] != 0) {
					if (debug) {
					printf("bad indirect addr: %d\n",
							dp->di_ib[j]);
					}
					goto unknown;
				}
			if (ftypeok(dp) == 0) {
				printf("mode: %o\n", dp->di_mode);
				goto unknown;
			}
			n_files++;
			lncntp[inumber] = dp->di_nlink;
			/*
			 * if errorlocked then open deleted files will
			 * manifest as di_nlink <= 0 and di_mode != 0
			 * so skip them; they're ok.
			 */
			if (dp->di_nlink <= 0 &&
					!(errorlocked && dp->di_mode == 0)) {
				zlnp = (struct zlncnt *)malloc(sizeof (*zlnp));
				if (zlnp == NULL) {
					pfatal("LINK COUNT TABLE OVERFLOW");
					if (reply("CONTINUE") == 0)
						errexit("");
				} else {
					zlnp->zlncnt = inumber;
					zlnp->next = zlnhead;
					zlnhead = zlnp;
				}
			}
			if ((dp->di_mode & IFMT) == IFDIR) {
				if (dp->di_size == 0)
					statemap[inumber] = DCLEAR;
				else
					statemap[inumber] = DSTATE;
				cacheino(dp, inumber);
			} else if ((dp->di_mode & IFMT) == IFSHAD) {
				if (dp->di_size == 0)
					statemap[inumber] = SCLEAR;
				else
					statemap[inumber] = SSTATE;
				cacheacl(dp, inumber);
			} else
				statemap[inumber] = FSTATE;
			badblk = dupblk = 0;
			idesc.id_number = inumber;
			idesc.id_fix = DONTKNOW;
			if (dp->di_size > (u_offset_t)MAXOFF_T) {
				largefile_count++;
				if (debug)
					printf("largefile: size=%lld,"
					    "count=%d\n", dp->di_size,
					    largefile_count);
			}
			(void) ckinode(dp, &idesc);
			idesc.id_entryno *= btodb(sblock.fs_fsize);
			if (dp->di_blocks != idesc.id_entryno) {
			pwarn("INCORRECT BLOCK COUNT I=%u (%ld should be %ld)",
				    inumber, dp->di_blocks, idesc.id_entryno);
				if (preen)
					printf(" (CORRECTED)\n");
				else if (reply("CORRECT") == 0)
					continue;
				dp = ginode(inumber);
				dp->di_blocks = idesc.id_entryno;
				inodirty();
			}
			if ((dp->di_mode & IFMT) == IFDIR)
				if (dp->di_blocks == 0)
					statemap[inumber] = DCLEAR;
			/*
			 * Check that the ACL is on a valid file type
			 */
			shadow = dp->di_shadow;
			if (shadow != 0) {
				if (acltypeok(dp) == 0) {
					pwarn("NON-ZERO ACL REFERENCE,  I=%ld",
					    inumber);
					if (preen)
						printf(" (CORRECTED)\n");
					else if (reply("CORRECT") == 0)
						continue;
					dp = ginode(inumber);
					dp->di_shadow = 0;
					inodirty();
				} else if ((shadow <= UFSROOTINO) ||
					(shadow > maxinumber)) {
				/*
				 * The shadow inode # must be realistic -
				 * either 0 or a real inode number.
				 */
					pwarn("BAD ACL REFERENCE I=%ld",
					    inumber);
					if (preen)
						printf(" (CORRECTED)\n");
					else if (reply("CORRECT") == 0)
						continue;
					dp = ginode(inumber);
					dp->di_shadow = 0;
					dp->di_mode &= IFMT;
					dp->di_smode = dp->di_mode;
					inodirty();
				} else {
					/*
					 * "register" this inode/shadow pair
					 */
					registershadowclient(shadow, inumber);
				}
			}
			continue;
	unknown:
			pfatal("UNKNOWN FILE TYPE I=%u", inumber);
			if ((dp->di_mode & IFMT) == IFDIR) {
				statemap[inumber] = DCLEAR;
				cacheino(dp, inumber);
			} else
				statemap[inumber] = FCLEAR;
			if (reply("CLEAR") == 1) {
				statemap[inumber] = USTATE;
				dp = ginode(inumber);
				clearinode(dp);
				inodirty();
			}
		}
	}
	freeinodebuf();
}

pass1check(idesc)
	register struct inodesc *idesc;
{
	int res = KEEPON;
	int anyout, nfrags;
	daddr_t blkno = idesc->id_blkno;
	register struct dups *dlp;
	struct dups *new;

	if ((anyout = chkrange(blkno, idesc->id_numfrags)) != 0) {
		blkerror(idesc->id_number, "BAD", blkno);
		if (++badblk >= MAXBAD) {
			pwarn("EXCESSIVE BAD BLKS I=%u",
				idesc->id_number);
			if (preen)
				printf(" (SKIPPING)\n");
			else if (reply("CONTINUE") == 0)
				errexit("");
			return (STOP);
		}
	}
	for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
		if (anyout && chkrange(blkno, 1)) {
			res = SKIP;
		} else if (!testbmap(blkno)) {
			n_blks++;
			setbmap(blkno);
		} else {
			blkerror(idesc->id_number, "DUP", blkno);
			if (++dupblk >= MAXDUP) {
				pwarn("EXCESSIVE DUP BLKS I=%u",
					idesc->id_number);
				if (preen)
					printf(" (SKIPPING)\n");
				else if (reply("CONTINUE") == 0)
					errexit("");
				return (STOP);
			}
			new = (struct dups *)malloc(sizeof (struct dups));
			if (new == NULL) {
				pfatal("DUP TABLE OVERFLOW.");
				if (reply("CONTINUE") == 0)
					errexit("");
				return (STOP);
			}
			new->dup = blkno;
			if (muldup == 0) {
				duplist = muldup = new;
				new->next = 0;
			} else {
				new->next = muldup->next;
				muldup->next = new;
			}
			for (dlp = duplist; dlp != muldup; dlp = dlp->next)
				if (dlp->dup == blkno)
					break;
			if (dlp == muldup && dlp->dup != blkno)
				muldup = new;
		}
		/*
		 * count the number of blocks found in id_entryno
		 */
		idesc->id_entryno++;
	}
	return (res);
}
