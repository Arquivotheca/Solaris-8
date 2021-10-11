
/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_HARDWARE_STRUCTS_H
#define	_HARDWARE_STRUCTS_H

#pragma ident	"@(#)hardware_structs.h	1.14	96/11/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/isa_defs.h>

#include <sys/dktp/fdisk.h>
#include <sys/dklabel.h>

/*
 * This file contains definitions of data structures pertaining to disks
 * and controllers.
 */

/*
 * This structure describes a specific disk.  These structures are in a
 * linked list because they are malloc'd as disks are found during the
 * initial search.
 */
struct disk_info {
	struct dk_cinfo		disk_dkinfo;	/* controller config info */
	struct disk_type	*disk_type;	/* ptr to physical info */
	struct partition_info	*disk_parts;	/* ptr to partition info */
	struct ctlr_info	*disk_ctlr;	/* ptr to disk's ctlr */
	struct disk_info	*disk_next;	/* ptr to next disk */
	struct ipart		fdisk_part;	/* fdisk partition info */
	int			disk_flags;	/* misc gotchas */
	char			*disk_name;	/* name of the disk */
	char			*disk_path;	/* pathname to device */
	char			*devfs_name;	/* devfs name for device */
	char			v_volume[LEN_DKL_VVOL];
						/* volume name from label */
						/* (no terminating null) */
};

#define	NSPECIFICS	8

/*
 * This structure describes a type (model) of drive.  It is malloc'd
 * and filled in as the data file is read and when a type 'other' disk
 * is selected.  The link is used to make a list of all drive types
 * supported by a ctlr type.
 */
struct disk_type {
	char	*dtype_asciilabel;		/* drive identifier */
	int	dtype_flags;			/* flags for disk type */
	u_long	dtype_options;			/* flags for options */
	u_int	dtype_fmt_time;			/* format time */
	u_int	dtype_bpt;			/* # bytes per track */
	int	dtype_ncyl;			/* # of data cylinders */
	int	dtype_acyl;			/* # of alternate cylinders */
	int	dtype_pcyl;			/* # of physical cylinders */
	int	dtype_nhead;			/* # of heads */
	int	dtype_phead;			/* # of physical heads */
	int	dtype_nsect;			/* # of data sectors/track */
	int	dtype_psect;			/* # physical sectors/track */
	int	dtype_rpm;			/* rotations per minute */
	int	dtype_cyl_skew;			/* cylinder skew */
	int	dtype_trk_skew;			/* track skew */
	int	dtype_trks_zone;		/* # tracks per zone */
	int	dtype_atrks;			/* # alt. tracks  */
	int	dtype_asect;			/* # alt. sectors */
	int	dtype_cache;			/* cache control */
	int	dtype_threshold;		/* cache prefetch threshold */
	int	dtype_read_retries;		/* read retries */
	int	dtype_write_retries;		/* write retries */
	int	dtype_prefetch_min;		/* cache min. prefetch */
	int	dtype_prefetch_max;		/* cache max. prefetch */
	u_int	dtype_specifics[NSPECIFICS];	/* ctlr specific drive info */
	struct	chg_list	*dtype_chglist;	/* mode sense/select */
						/* change list - scsi */
	struct	partition_info	*dtype_plist;	/* possible partitions */
	struct	disk_type	*dtype_next;	/* ptr to next drive type */
	/*
	 * Added so that we can print a useful diagnostic if
	 * inconsistent definitions found in multiple files.
	 */
	char	*dtype_filename;		/* filename where defined */
	int	dtype_lineno;			/* line number in file */
};

/*
 * This structure describes a specific ctlr.  These structures are in
 * a linked list because they are malloc'd as ctlrs are found during
 * the initial search.
 */
struct ctlr_info {
	char	ctlr_cname[DK_DEVLEN+1];	/* name of ctlr */
	char	ctlr_dname[DK_DEVLEN+1];	/* name of disks */
	u_short	ctlr_flags;			/* flags for ctlr */
	short	ctlr_num;			/* number of ctlr */
	int	ctlr_addr;			/* address of ctlr */
	u_int	ctlr_space;			/* bus space it occupies */
	int	ctlr_prio;			/* interrupt priority */
	int	ctlr_vec;			/* interrupt vector */
	struct	ctlr_type *ctlr_ctype;		/* ptr to ctlr type info */
	struct	ctlr_info *ctlr_next;		/* ptr to next ctlr */
};

/*
 * This structure describes a type (model) of ctlr.  All supported ctlr
 * types are built into the program statically, they cannot be added by
 * the user.
 */
struct ctlr_type {
	u_short	ctype_ctype;			/* type of ctlr */
	char	*ctype_name;			/* name of ctlr type */
	struct	ctlr_ops *ctype_ops;		/* ptr to ops vector */
	int	ctype_flags;			/* flags for gotchas */
	struct	disk_type *ctype_dlist;		/* list of disk types */
};

/*
 * This structure is the operation vector for a controller type.  It
 * contains pointers to all the functions a controller type can support.
 */
struct ctlr_ops {
	int	(*op_rdwr)();		/* read/write - mandatory */
	int	(*op_ck_format)();	/* check format - mandatory */
	int	(*op_format)();		/* format - mandatory */
	int	(*op_ex_man)();		/* get manufacturer's list - optional */
	int	(*op_ex_cur)();		/* get current list - optional */
	int	(*op_repair)();		/* repair bad sector - optional */
	int	(*op_create)();		/* create original manufacturers */
					/* defect list. - optional */
	int	(*op_wr_cur)();		/* write current list - optional */
};

/*
 * This structure describes a specific partition layout.  It is malloc'd
 * when the data file is read and whenever the user creates his own
 * partition layout.  The link is used to make a list of possible
 * partition layouts for each drive type.
 */
struct partition_info {
	char	*pinfo_name;			/* name of layout */
	struct	dk_map pinfo_map[NDKMAP];	/* layout info */
	struct	dk_vtoc vtoc;			/* SVr4 vtoc additions */
	struct	partition_info *pinfo_next;	/* ptr to next layout */
	char	*pinfo_filename;		/* filename where defined */
	int	pinfo_lineno;			/* line number in file */
};


/*
 * This structure describes a change to be made to a particular
 * SCSI mode sense page, before issuing a mode select on that
 * page.  This changes are specified in format.dat, and one
 * such structure is created for each specification, linked
 * into a list, in the order specified.
 */
struct chg_list {
	int		pageno;		/* mode sense page no. */
	int		byteno;		/* byte within page */
	int		mode;		/* see below */
	int		value;		/* desired value */
	struct chg_list	*next;		/* ptr to next */
};

/*
 * Change list modes
 */
#define	CHG_MODE_UNDEFINED	(-1)		/* undefined value */
#define	CHG_MODE_SET		0		/* set bits by or'ing */
#define	CHG_MODE_CLR		1		/* clr bits by and'ing */
#define	CHG_MODE_ABS		2		/* set absolute value */

/*
 * This is the structure that creates a dynamic list of controllers
 * that we know about.  This structure will point to the items that
 * use to be statically created in the format program but will now allow
 * dynamic creation of the list so that we can do 3'rd party generic
 * disk/controller support.
 */

struct mctlr_list {
	struct mctlr_list *next;
	struct ctlr_type  *ctlr_type;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _HARDWARE_STRUCTS_H */
