/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _PROTOCOLS_DUMPRESTORE_H
#define	_PROTOCOLS_DUMPRESTORE_H

#pragma ident	"@(#)dumprestore.h	1.16	98/07/29 SMI"	/* SVr4.0 1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * TP_BSIZE is the size of file blocks on the dump tapes.
 * Note that TP_BSIZE must be a multiple of DEV_BSIZE.
 *
 * NTREC is the number of TP_BSIZE blocks that are written
 * in each tape record. HIGHDENSITYTREC is the number of
 * TP_BSIZE blocks that are written in each tape record on
 * 6250 BPI or higher density tapes.  CARTRIDGETREC is the
 * number of TP_BSIZE blocks that are written in each tape
 * record on cartridge tapes.
 *
 * TP_NINDIR is the number of indirect pointers in a TS_INODE
 * or TS_ADDR record. Note that it must be a power of two.
 */
#define	TP_BSIZE	1024
#define	NTREC   	10
#define	HIGHDENSITYTREC	32
#define	CARTRIDGETREC	63
#define	TP_NINDIR	(TP_BSIZE/2)
#define	TP_NINOS	(TP_NINDIR / sizeof (long))
#define	LBLSIZE		16
#define	NAMELEN		64

#define	OFS_MAGIC   	(int)60011
#define	NFS_MAGIC   	(int)60012
#define	CHECKSUM	(int)84446

union u_data {
	char	s_addrs[TP_NINDIR];	/* 1 => data; 0 => hole in inode */
	int32_t	s_inos[TP_NINOS];	/* starting inodes on tape */
};

union u_shadow {
	struct s_nonsh {
		int32_t	c_level;		/* level of this dump */
		char	c_filesys[NAMELEN];	/* dumpped file system name */
		char	c_dev[NAMELEN];		/* name of dumpped device */
		char	c_host[NAMELEN];	/* name of dumpped host */
	} c_nonsh;
	char    c_shadow[1];
};

/* if you change anything here, be sure to change normspcl in byteorder.c */

union u_spcl {
	char dummy[TP_BSIZE];
	struct	s_spcl {
		int32_t	c_type;		    /* record type (see below) */
		time32_t c_date;	    /* date of previous dump */
		time32_t c_ddate;	    /* date of this dump */
		int32_t	c_volume;	    /* dump volume number */
		daddr32_t c_tapea;	    /* logical block of this record */
		ino32_t	c_inumber;	    /* number of inode */
		int32_t	c_magic;	    /* magic number (see above) */
		int32_t	c_checksum;	    /* record checksum */
		struct	dinode	c_dinode;   /* ownership and mode of inode */
		int32_t	c_count;	    /* number of valid c_addr entries */
		union	u_data c_data;	    /* see union above */
		char	c_label[LBLSIZE];   /* dump label */
		union	u_shadow c_shadow;  /* see union above */
		int32_t	c_flags;	    /* additional information */
		int32_t	c_firstrec;	    /* first record on volume */
		int32_t	c_spare[32];	    /* reserved for future uses */
	} s_spcl;
} u_spcl;
#define	spcl u_spcl.s_spcl
#define	c_addr c_data.s_addrs
#define	c_inos c_data.s_inos
#define	c_level c_shadow.c_nonsh.c_level
#define	c_filesys c_shadow.c_nonsh.c_filesys
#define	c_dev c_shadow.c_nonsh.c_dev
#define	c_host c_shadow.c_nonsh.c_host

/*
 * special record types
 */
#define	TS_TAPE 	1	/* dump tape header */
#define	TS_INODE	2	/* beginning of file record */
#define	TS_ADDR 	4	/* continuation of file record */
#define	TS_BITS 	3	/* map of inodes on tape */
#define	TS_CLRI 	6	/* map of inodes deleted since last dump */
#define	TS_END  	5	/* end of volume marker */
#define	TS_EOM		7	/* floppy EOM - restore compat w/ old dump */

/*
 * flag values
 */
#define	DR_NEWHEADER	1	/* new format tape header */
#define	DR_INODEINFO	2	/* header contains starting inode info */
#define	DR_REDUMP	4	/* dump contains recopies of active files */
#define	DR_TRUEINC	8	/* dump is a "true incremental"	*/
#define	DR_HASMETA	16	/* metadata in this header */



#define	DUMPOUTFMT	"%-32s %c %s"		/* for printf */
						/* name, incno, ctime(date) */
#define	DUMPINFMT	"%258s %c %128[^\n]\n"	/* inverse for scanf */

#ifdef __cplusplus
}
#endif

#endif	/* !_PROTOCOLS_DUMPRESTORE_H */
