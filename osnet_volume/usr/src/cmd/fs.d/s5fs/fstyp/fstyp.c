/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
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
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ident "@(#)fstyp.c	1.2	98/07/20 SMI"

/*
 * fstyp
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/errno.h>
#include <sys/fs/s5_fs.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <stdio.h>
#include <sys/mnttab.h>

int	vflag = 0;		/* verbose output */
int	errflag = 0;
extern	int	optind;
char	*cbasename;
char	*special;
char	*fstype;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	c;

	cbasename = argv[0];
	while ((c = getopt (argc, argv, "v")) != EOF) {
		switch (c) {

		case 'v':		/* dump super block */
			vflag++;
			break;

		case '?':
			errflag++;
		}
	}
	if (errflag) {
		usage ();
		exit (31+1);
	}
	if (argc < optind) {
		usage ();
		exit (31+1);
	}
	special = argv[optind];
	dumpfs (special);
}


usage ()
{
	(void) fprintf (stderr, "s5fs usage: fstyp [-v] special\n");
}

union {
	struct filsys fs;
	char pad[BBSIZE];
} fsun;
#define	afs	fsun.fs


dumpfs(name)
	char *name;
{
	int c, i, j, k, size;

	close(0);
	if (open(name, 0) != 0) {
		perror(name);
		return;
	}
	lseek(0, SBLOCK * DEV_BSIZE, 0);
	if (read(0, &afs, SBSIZE) != SBSIZE) {
		perror(name);
		return;
	}
	if (afs.s_magic != FsMAGIC)
		exit (31+1);
	printf ("%s\n", "s5fs");
	if (!vflag)
		exit (0);
	printf("magic\t0x%x\t\ttime\t%s", afs.s_magic, ctime(&afs.s_time));
	printf("state\t0x%x\t\tfmod\t%d\n",afs.s_state,afs.s_fmod);
	printf("fsize\t%d\t\ttfree\t%d\t\ttinode\t%d\n",afs.s_fsize,afs.s_tfree,
		afs.s_tinode);
	if ( afs.s_bshift == Fs1b){
		printf("bsize\t%d\t",512);
	} else if ( afs.s_bshift == Fs2b){
		printf("bsize\t%d\t",1024);
	} else if (afs.s_bshift == Fs4b ){
		printf("bsize\t%d\t",2048);
	} else {
		printf("bsize\t%s\t","BAD");
	}
	printf("\tisize\t%d\t\tfname\t%s\n",afs.s_isize,afs.s_fname);
	printf("nfree\t%d\t\tninode\t%d\t\tfpack\t%s\n",
	    afs.s_nfree, afs.s_ninode, afs.s_fpack);
	if (afs.s_nfree) {
		printf("Free block list:\n\t\t");
		for (i=0; i<afs.s_nfree; i++) {
			printf("%d\t",afs.s_free[i]);
			if (((i+1) % 6) == 0)
			{
				printf("\n\t\t");
			}
		}
	}
	printf("\n");
	if (afs.s_ninode) {
		printf("Free inode list:\n\t\t");
		for (i=0; i<afs.s_ninode; i++) {
			printf("%d\t",afs.s_inode[i]);
			if (((i+1) % 6) == 0)
				printf("\n\t\t");
		}
	}
	printf("\n");
	close(0);
	exit(0);
}


