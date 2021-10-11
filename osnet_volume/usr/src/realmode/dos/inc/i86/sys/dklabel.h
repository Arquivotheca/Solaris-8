/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)dklabel.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		dklabel.h
 *
 *   Description:	The Solaris x86 implementation of the disk label.
 *
 */

#ifndef _SYS_DKLABEL_H
#define	_SYS_DKLABEL_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Miscellaneous defines
 */
#define	DKL_MAGIC	0xDABE		/* magic number */
#define	FKL_MAGIC	0xff		/* magic number for DOS floppies */
#define	NDKMAP		16		/* # of logical partitions */

#define	LEN_DKL_ASCII	128		/* length of dkl_asciilabel */
#define	LEN_DKL_VVOL	8		/* length of v_volume */
#define	DK_LABEL_SIZE	512		/* size of disk label */
#define	DK_LABEL_LOC	1		/* location of disk label	  */


/*
 * Format of a Sun disk label.
 *
 * sizeof (struct dk_label) should be 512 (the current sector size),
 * but should the sector size increase, this structure should remain
 * at the beginning of the sector.
 */

/* partition headers:  section 1 */

struct dk_map {
	daddr_t	dkl_cylno;		/*	starting cylinder */
	daddr_t	dkl_nblk;		/*	number of blocks;  if == 0, */
					/*	partition is undefined */
};

/* partition headers:  section 2,
 * brought over from AT&T SVr4 vtoc structure.
 */

struct dk_map2 {
	unsigned short	p_tag;		/*	ID tag of partition */
	unsigned short	p_flag;		/*	permission flag */
};

struct dkl_partition	{
	ushort	p_tag;			/* ID tag of partition 		*/
	ushort	p_flag;			/* permision flags 		*/
	daddr_t p_start;		/* start sector no of partition */
	long	p_size;			/* # of blocks in partition 	*/
};

struct dkl_vtoc {
	unsigned long v_bootinfo[3];	/* info needed by mboot (unsupported) */
	unsigned long v_sanity;		/* to verify vtoc sanity 	*/
	unsigned long v_version;	/* layout version 		*/
	char	v_volume[LEN_DKL_VVOL];	/* volume name 			*/
	ushort	v_sectorsz;		/* sector size in bytes 	*/
	ushort	v_nparts;		/* number of partitions 	*/
	unsigned long v_reserved[10];	/* free space 			*/
	struct dkl_partition v_part[NDKMAP]; /* partition headers 	*/
	time_t	timestamp[NDKMAP];	/* partition timestamp (unsupported) */
	char	v_asciilabel[LEN_DKL_ASCII];	/* for compatibility 	*/
};

/*
 * Definition of a disk's geometry
 */
struct dkl_geom {
	unsigned long	dkg_pcyl;	/* # of physical cylinders 	    */
	unsigned long	dkg_ncyl;	/* # of data cylinders 		    */
	unsigned short	dkg_acyl;	/* # of alternate cylinders 	    */
	unsigned short	dkg_bcyl;	/* cyl offset (for fixed head area) */
	unsigned long	dkg_nhead;	/* # of heads 			    */
	unsigned long	dkg_nsect;	/* # of data sectors per track 	    */
	unsigned short	dkg_intrlv;	/* interleave factor 		    */
	unsigned short	dkg_skew;	/* skew factor 			    */
	unsigned short	dkg_apc;	/* alternates per cyl (SCSI only)   */
	unsigned short	dkg_rpm;	/* revolutions per minute 	    */
	unsigned short	dkg_write_reinstruct;  /* # sectors to skip, writes */
	unsigned short	dkg_read_reinstruct;   /* # sectors to skip, reads  */
	unsigned short	dkg_extra[4];	/* for compatible expansion 	    */
};

/* define the amount of disk label padding needed to make
 * the entire structure occupy 512 bytes.
 */
#define	LEN_DKL_PAD	(DK_LABEL_SIZE - (sizeof (struct dkl_vtoc)) - \
				(sizeof (struct dkl_geom)) - \
				(2 * (sizeof (unsigned short))))

struct dk_label {
	struct  dkl_vtoc dkl_vtoc;	/* vtoc inclusions from AT&T SVr4 */
	struct	dkl_geom dkl_geom;	/* disk geometry		  */
	char		dkl_pad[LEN_DKL_PAD]; /* unused part of 512 bytes */
	/* */
	unsigned short	dkl_magic;	/* identifies this label format	  */
	unsigned short	dkl_cksum;	/* xor checksum of sector 	  */
};

struct fk_label {			/* DOS floppy label */
	u_char  fkl_type;
	u_char  fkl_magich;
	u_char  fkl_magicl;
	u_char  filler;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKLABEL_H */

