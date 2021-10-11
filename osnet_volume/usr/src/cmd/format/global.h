
/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_GLOBAL_H
#define	_GLOBAL_H

#pragma ident	"@(#)global.h	1.24	98/01/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains global definitions and declarations.  It is intended
 * to be included by everyone.
 */
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/isa_defs.h>

#include <sys/dklabel.h>
#include <sys/vtoc.h>
#include <sys/dkio.h>

#include "hardware_structs.h"
#include "defect.h"
#include "io.h"

#include <sys/dktp/fdisk.h>


/*
 * These declarations are global state variables.
 */
struct	disk_info *disk_list;		/* list of found disks */
struct	ctlr_info *ctlr_list;		/* list of found ctlrs */
char	cur_menu;			/* current menu level */
char	last_menu;			/* last menu level */
char	option_msg;			/* extended message options */
char	diag_msg;			/* extended diagnostic msgs */
char	option_s;			/* silent mode option */
char	*option_f;			/* input redirect option */
char	*option_l;			/* log file option */
FILE	*log_file;			/* log file pointer */
char	*option_d;			/* forced disk option */
char	*option_t;			/* forced disk type option */
char	*option_p;			/* forced partition table option */
char	*option_x;			/* data file redirection option */
FILE	*data_file;			/* data file pointer */
char	*file_name;			/* current data file name */
					/* for useful error messages */
int	expert_mode;			/* enable for expert mode */
					/* commands */
int	need_newline;			/* for correctly formatted output */
int	dev_expert;			/* enable for developer mode */
					/* commands */

/*
 * These declarations are used for quick access to information about
 * the disk being worked on.
 */
int	cur_file;			/* file descriptor for current disk */
int	cur_flags;			/* flags for current disk */
struct	disk_info *cur_disk;		/* current disk */
struct	disk_type *cur_dtype;		/* current dtype */
struct	ctlr_info *cur_ctlr;		/* current ctlr */
struct	ctlr_type *cur_ctype;		/* current ctype */
struct	ctlr_ops *cur_ops;		/* current ctlr's ops vector */
struct	partition_info *cur_parts;	/* current disk's partitioning */
struct	defect_list cur_list;		/* current disk's defect list */
void	*cur_buf;			/* current disk's I/O buffer */
void	*pattern_buf;			/* current disk's pattern buffer */
int	pcyl;				/* # physical cyls */
int	ncyl;				/* # data cyls */
int	acyl;				/* # alt cyls */
int	nhead;				/* # heads */
int	phead;				/* # physical heads */
int	nsect;				/* # data sects/track */
int	psect;				/* # physical sects/track */
int	apc;				/* # alternates/cyl */
int	solaris_offset;			/* Solaris offset, this value is zero */
					/* for non-fdisk machines. */

#if defined(i386)
int	alts_fd;			/* alternate partition file desc */
#endif		/* defined(i386) */

#if defined(_SUNOS_VTOC_16)
int	bcyl;				/* # other cyls */
#endif		/* defined(_SUNOS_VTOC_16) */

struct	mboot boot_sec;			/* fdisk partition info */
int	xstart;				/* solaris partition start */
char	x86_devname[80];		/* saved device name for fdisk */
					/* information accesses */
struct	mctlr_list	*controlp;	/* master controller list ptr */


/*
 * These defines are used to manipulate the physical characteristics of
 * the current disk.
 */
#define	sectors(h)	((h) == nhead - 1 ? nsect - apc : nsect)
#define	spc()		((int)(nhead * nsect - apc))
#define	chs2bn(c, h, s)	((daddr_t)((c) * spc() + (h) * nsect + (s)))
#define	bn2c(bn)	((bn) / (int)spc())
#define	bn2h(bn)	(((bn) % (int)spc()) / (int)nsect)
#define	bn2s(bn)	(((bn) % (int)spc()) % (int)nsect)
#define	datasects()	(ncyl * spc())
#define	totalsects()	((ncyl + acyl) * spc())
#define	physsects()	(pcyl * spc())

/*
 * Macro to convert a device number into a partition number
 */
#define	PARTITION(dev)	(minor(dev) & 0x07)

/*
 * These values define flags for the current disk (cur_flags).
 */
#define	DISK_FORMATTED		0x01	/* disk is formatted */
#define	LABEL_DIRTY		0x02	/* label has been scribbled */

/*
 * These flags are for the controller type flags field.
 */
#define	CF_NONE		0x0000		/* NO FLAGS */
#define	CF_BLABEL	0x0001		/* backup labels in funny place */
#define	CF_DEFECTS	0x0002		/* disk has manuf. defect list */
#define	CF_APC		0x0004		/* ctlr uses alternates per cyl */
#define	CF_SMD_DEFS	0x0008		/* ctlr does smd defect handling */

#define	CF_SCSI		0x0040		/* ctlr is for SCSI disks */
#define	CF_EMBEDDED	0x0080		/* ctlr is for embedded SCSI disks */

#define	CF_IPI		0x0100		/* ctlr is for IPI disks */
#define	CF_WLIST	0x0200		/* ctlt handles working list */
#define	CF_NOFORMAT	0x0400		/* Manufacture formatting only */
/*
 * This flag has been introduced only for SPARC ATA. Which has been approved
 * at that time with the agreement in the next fix it will be removed and the
 * format will be revamped with controller Ops structure not to  have
 * any operation to be NULL. As it makes things more modular.
 */
#define	CF_NOWLIST	0x0800		/* Ctlr doesnot handle working list */

#define	CF_OLD_DRIVER	0x8000		/* Old style driver(non-generic) */


/*
 * Do not require confirmation to extract defect lists on SCSI
 * and IPI drives, since this operation is instantaneous
 */
#define	CF_CONFIRM	(CF_SCSI|CF_IPI)

/*
 * Macros to make life easier
 */
#define	SMD		(cur_ctype->ctype_flags & CF_SMD_DEFS)
#define	SCSI		(cur_ctype->ctype_flags & CF_SCSI)
#define	EMBEDDED_SCSI	((cur_ctype->ctype_flags & (CF_SCSI|CF_EMBEDDED)) == \
				(CF_SCSI|CF_EMBEDDED))

#define	OLD_DRIVER	(cur_ctype->ctype_flags & CF_OLD_DRIVER)

/*
 * These flags are for the disk type flags field.
 */
#define	DT_NEED_SPEFS	0x01		/* specifics fields are uninitialized */

/*
 * These defines are used to access the ctlr specific
 * disk type fields (based on ctlr flags).
 */
#define	dtype_bps	dtype_specifics[0]	/* bytes/sector */
#define	dtype_dr_type	dtype_specifics[1]	/* drive type */
#define	dtype_dr_type_data dtype_specifics[2]	/* drive type in data file */

/*
 * These flags are for the disk info flags field.
 */
#define	DSK_LABEL	0x01		/* disk is currently labelled */
#define	DSK_LABEL_DIRTY	0x02		/* disk auto-sensed, but not */
					/* labeled yet. */
#define	DSK_AUTO_CONFIG	0x04		/* disk was auto-configured */
#define	DSK_RESERVED	0x08		/* disk is reserved by other host */
#define	DSK_UNAVAILABLE	0x10		/* disk not available, could be */
					/* currently formatting */

/*
 * These flags are used to control disk command execution.
 */
#define	F_NORMAL	0x00		/* normal operation */
#define	F_SILENT	0x01		/* no error msgs at all */
#define	F_ALLERRS	0x02		/* return any error, not just fatal */

/*
 * Directional parameter for the op_rdwr controller op.
 */
#define	DIR_READ	0
#define	DIR_WRITE	1

/*
 * These defines are the mode parameter for the checksum routines.
 */
#define	CK_CHECKSUM		0		/* check checksum */
#define	CK_MAKESUM		1		/* generate checksum */

/*
 * This is the base character for partition identifiers
 */
#define	PARTITION_BASE		'0'

/*
 * Base pathname for devfs names to be stripped from physical name.
 */
#define	DEVFS_PREFIX	"/devices"

#ifdef	__cplusplus
}
#endif

#endif	/* _GLOBAL_H */
