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
 * Copyright (c) 1986,1987,1988,1989,1996 by Sun Microsystems, Inc.
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

#ident	"@(#)pass5.c	1.21	96/04/18 SMI"	/* SVr4.0 1.2 */

#include <sys/param.h>
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
#include "fsck.h"

pass5()
{
	int c, blk, frags, basesize, sumsize, mapsize, savednrpos;
	int savedinterleave, savednpsect;
	register struct fs *fs = &sblock;
	register struct cg *cg = &cgrp;
	daddr_t dbase, dmax;
	register daddr_t d;
	register long i, j;
	struct csum *cs;
	time_t now;
	struct csum cstotal;
	struct inodesc idesc;
	char buf[MAXBSIZE];
	register struct cg *newcg = (struct cg *)buf;
	struct ocg *ocg = (struct ocg *)buf;

	bzero((char *)newcg, (int)fs->fs_cgsize);
	newcg->cg_niblk = fs->fs_ipg;
	switch ((int)fs->fs_postblformat) {

	case FS_42POSTBLFMT:
		basesize = (char *)(&ocg->cg_btot[0]) - (char *)(&ocg->cg_link);
		sumsize = &ocg->cg_iused[0] - (char *)(&ocg->cg_btot[0]);
		mapsize = &ocg->cg_free[howmany(fs->fs_fpg, NBBY)] -
			(u_char *)&ocg->cg_iused[0];
		ocg->cg_magic = CG_MAGIC;
		savednrpos = fs->fs_nrpos;
		fs->fs_nrpos = 8;
		fs->fs_trackskew = 0;
		savedinterleave = fs->fs_interleave;
		fs->fs_interleave = 1;
		if ((fs->fs_npsect < 0) || (fs->fs_npsect > fs->fs_spc)) {
			/* Migration aid from fs_state */
			fs->fs_npsect = fs->fs_nsect;
		}
		savednpsect = fs->fs_npsect;
		fs->fs_npsect = fs->fs_nsect;
		break;

	case FS_DYNAMICPOSTBLFMT:
		newcg->cg_btotoff =
			&newcg->cg_space[0] - (u_char *)(&newcg->cg_link);
		newcg->cg_boff =
			newcg->cg_btotoff + fs->fs_cpg * sizeof (long);
		newcg->cg_iusedoff = newcg->cg_boff +
			fs->fs_cpg * fs->fs_nrpos * sizeof (short);
		newcg->cg_freeoff =
			newcg->cg_iusedoff + howmany(fs->fs_ipg, NBBY);
		newcg->cg_nextfreeoff = newcg->cg_freeoff +
			howmany(fs->fs_cpg * fs->fs_spc / NSPF(fs),
				NBBY);
		newcg->cg_magic = CG_MAGIC;
		basesize = &newcg->cg_space[0] - (u_char *)(&newcg->cg_link);
		sumsize = newcg->cg_iusedoff - newcg->cg_btotoff;
		mapsize = newcg->cg_nextfreeoff - newcg->cg_iusedoff;
		break;

	default:
		errexit("UNKNOWN ROTATIONAL TABLE FORMAT %d\n",
			fs->fs_postblformat);
	}

	bzero((char *)&idesc, sizeof (struct inodesc));
	idesc.id_type = ADDR;
	bzero((char *)&cstotal, sizeof (struct csum));
	(void) time(&now);
	j = blknum(fs, fs->fs_size + fs->fs_frag - 1);
	for (i = fs->fs_size; i < j; i++)
		setbmap(i);
	for (c = 0; c < fs->fs_ncg; c++) {
		getblk(&cgblk, cgtod(fs, c), fs->fs_cgsize);
		if (!cg_chkmagic(cg))
			pfatal("CG %d: BAD MAGIC NUMBER\n", c);
		dbase = cgbase(fs, c);
		dmax = dbase + fs->fs_fpg;
		if (dmax > fs->fs_size)
			dmax = fs->fs_size;
		if (now > cg->cg_time)
			newcg->cg_time = cg->cg_time;
		else
			newcg->cg_time = now;
		newcg->cg_cgx = c;
		if (c == fs->fs_ncg - 1)
			newcg->cg_ncyl = fs->fs_ncyl % fs->fs_cpg;
		else
			newcg->cg_ncyl = fs->fs_cpg;
		newcg->cg_niblk = sblock.fs_ipg;
		newcg->cg_ndblk = dmax - dbase;
		newcg->cg_cs.cs_ndir = 0;
		newcg->cg_cs.cs_nffree = 0;
		newcg->cg_cs.cs_nbfree = 0;
		newcg->cg_cs.cs_nifree = fs->fs_ipg;
		if ((cg->cg_rotor >= 0) && (cg->cg_rotor < newcg->cg_ndblk))
			newcg->cg_rotor = cg->cg_rotor;
		else
			newcg->cg_rotor = 0;
		if ((cg->cg_frotor >= 0) && (cg->cg_frotor < newcg->cg_ndblk))
			newcg->cg_frotor = cg->cg_frotor;
		else
			newcg->cg_frotor = 0;
		if ((cg->cg_irotor >= 0) && (cg->cg_irotor < newcg->cg_niblk))
			newcg->cg_irotor = cg->cg_irotor;
		else
			newcg->cg_irotor = 0;
		bzero((char *)&newcg->cg_frsum[0], sizeof (newcg->cg_frsum));
		bzero((char *)&cg_blktot(newcg)[0], sumsize + mapsize);
		if (fs->fs_postblformat == FS_42POSTBLFMT)
			ocg->cg_magic = CG_MAGIC;
		j = fs->fs_ipg * c;
		for (i = 0; i < fs->fs_ipg; j++, i++) {
			switch (statemap[j]) {

			case USTATE:
				break;

			case DSTATE:
			case DCLEAR:
			case DFOUND:
				newcg->cg_cs.cs_ndir++;
				/* fall through */

			case FSTATE:
			case FCLEAR:
			case SSTATE:
			case SCLEAR:
				newcg->cg_cs.cs_nifree--;
				setbit(cg_inosused(newcg), i);
				break;

			default:
				if (j < (int)UFSROOTINO)
					break;
				errexit("BAD STATE %d FOR INODE I=%d",
				    statemap[j], j);
			}
		}
		if (c == 0)
			for (i = 0; i < UFSROOTINO; i++) {
				setbit(cg_inosused(newcg), i);
				newcg->cg_cs.cs_nifree--;
			}
		for (i = 0, d = dbase;
		    d < dmax;
		    d += fs->fs_frag, i += fs->fs_frag) {
			frags = 0;
			for (j = 0; j < fs->fs_frag; j++) {
				if (testbmap(d + j))
					continue;
				setbit(cg_blksfree(newcg), i + j);
				frags++;
			}
			if (frags == fs->fs_frag) {
				newcg->cg_cs.cs_nbfree++;
				j = cbtocylno(fs, i);
				cg_blktot(newcg)[j]++;
				cg_blks(fs, newcg, j)[cbtorpos(fs, i)]++;
			} else if (frags > 0) {
				newcg->cg_cs.cs_nffree += frags;
				blk = blkmap(fs, cg_blksfree(newcg), i);
				fragacct(fs, blk, newcg->cg_frsum, 1);
			}
		}
/*
 *		for (frags = d; d < dmax; d++) {
 *			if (getbmap(d))
 *				continue;
 *			setbit(newcg->cg_free, d - dbase);
 *			newcg->cg_cs.cs_nffree++;
 *		}
 *		if (frags != d) {
 *			blk = blkmap(&sblock, newcg->cg_free, (frags - dbase));
 *			fragacct(&sblock, blk, newcg->cg_frsum, 1);
 *		}
 */
		cstotal.cs_nffree += newcg->cg_cs.cs_nffree;
		cstotal.cs_nbfree += newcg->cg_cs.cs_nbfree;
		cstotal.cs_nifree += newcg->cg_cs.cs_nifree;
		cstotal.cs_ndir += newcg->cg_cs.cs_ndir;

		cs = &fs->fs_cs(fs, c);
		if (bcmp((char *)&newcg->cg_cs, (char *)cs,
		    sizeof (*cs)) != 0 &&
		    dofix(&idesc, "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
			bcopy((char *)&newcg->cg_cs, (char *)cs, sizeof (*cs));
			sbdirty();
		}
		if (cvtflag) {
			bcopy((char *)newcg, (char *)cg, (int)fs->fs_cgsize);
			cgdirty();
			continue;
		}
		if ((bcmp((char *)newcg, (char *)cg, basesize) != 0 ||
		    bcmp((char *)&cg_blktot(newcg)[0],
			    (char *)&cg_blktot(cg)[0], sumsize) != 0) &&
		    dofix(&idesc, "SUMMARY INFORMATION BAD")) {
			bcopy((char *)newcg, (char *)cg, basesize);
			bcopy((char *)&cg_blktot(newcg)[0],
			    (char *)&cg_blktot(cg)[0], sumsize);
			cgdirty();
		}
		if (bcmp(cg_inosused(newcg),
			    cg_inosused(cg), mapsize) != 0 &&
		    dofix(&idesc, "BLK(S) MISSING IN BIT MAPS")) {
			bcopy(cg_inosused(newcg), cg_inosused(cg), mapsize);
			cgdirty();
		}

		cs = &sblock.fs_cs(&sblock, c);
		if (bcmp((char *)&newcg->cg_cs, (char *)cs,
		    sizeof (*cs)) != 0 &&
		    dofix(&idesc, "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
	/*
	 *		bcopy((char *)&newcg->cg_cs, (char *)cs, sizeof (*cs));
	 *		sbdirty();
	 */
		}

	}
	if (fs->fs_postblformat == FS_42POSTBLFMT) {
		fs->fs_nrpos = savednrpos;
		fs->fs_interleave = savedinterleave;
		fs->fs_npsect = savednpsect;
	}
	if ((fflag || !(islog && islogok)) &&
	    bcmp((char *)&cstotal, (char *)&fs->fs_cstotal,
	    sizeof (struct csum)) != 0 &&
	    dofix(&idesc, "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
		bcopy((char *)&cstotal, (char *)&fs->fs_cstotal,
			sizeof (struct csum));
		fs->fs_ronly = 0;
		fs->fs_fmod = 0;
		sbdirty();
	}
}
