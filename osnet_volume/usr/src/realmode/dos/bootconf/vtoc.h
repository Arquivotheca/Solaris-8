/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VTOC_H
#define	_VTOC_H

#ident "@(#)vtoc.h   1.4   97/08/27 SMI"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		vtoc.h
 *
 *   Description:	The Solaris x86 implementation of the vtoc.
 *
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Note:  the VTOC is not implemented fully, nor in the manner
 *	that AT&T implements it.  AT&T puts the vtoc structure
 *	into a sector, usually the second sector (pdsector is first).
 *
 *	Sun incorporates the tag, flag, version, and volume vtoc fields into
 *	its Disk Label, which already has some vtoc-equivalent fields.
 *	Upon reading the vtoc with read_vtoc(), the following exceptions
 *	occur:
 *		v_bootinfo [all]	returned as zero
 *		v_sanity		returned as VTOC_SANE
 *						if Disk Label was sane
 *		v_sectorsz		returned as 512
 *		v_reserved [all]	retunred as zero
 *		timestamp [all]		returned as zero
 *
 *	See  dklabel.h, read_vtoc(), and write_vtoc().
 */

#define	V_NUMPAR 	16		/* The number of partitions */
					/* (from dkio.h) */

#define	VTOC_SANE	0x600DDEEE	/* Indicates a sane VTOC */
#define	V_VERSION	0x01		/* layout version number */

/*
 * Partition identification tags
 */
#define	V_UNASSIGNED	0x00		/* unassigned partition */
#define	V_BOOT		0x01		/* Boot partition */
#define	V_ROOT		0x02		/* Root filesystem */
#define	V_SWAP		0x03		/* Swap filesystem */
#define	V_USR		0x04		/* Usr filesystem */
#define	V_BACKUP	0x05		/* full disk */
#define	V_STAND		0x06		/* Stand partition */
#define	V_VAR		0x07		/* Var partition */
#define	V_HOME		0x08		/* Home partition */
#define	V_ALTSCTR	0x09		/* Alternate sector partition */
#define	V_CACHE		0x0a		/* Cache (cachefs) partition */

/*
 * Partition permission flags
 */
#define	V_UNMNT		0x01		/* Unmountable partition */
#define	V_RONLY		0x10		/* Read only */

/*
 * error codes for reading & writing vtoc
 */
#define	VT_ERROR	(-2)		/* errno supplies specific error */
#define	VT_EIO		(-3)		/* I/O error accessing vtoc */
#define	VT_EINVAL	(-4)		/* illegal value in vtoc or request */

#pragma pack(1)

struct partition	{
	unsigned short	p_tag;		/* ID tag of partition */
	unsigned short	p_flag;		/* permision flags */
	unsigned long	p_start;	/* start sector no of partition */
	long		p_size;		/* # of blocks in partition */
};

struct vtoc {
	unsigned long v_bootinfo[3];	/* info needed by mboot (unsupported) */
	unsigned long v_sanity;		/* to verify vtoc sanity */
	unsigned long v_version;	/* layout version */
	char v_volume[8];		/* volume name */
	unsigned short v_sectorsz;	/* sector size in bytes */
	unsigned short v_nparts;	/* number of partitions */
	unsigned long v_reserved[10];	/* free space */
	struct partition v_part[V_NUMPAR]; /* partition headers */
	time_t timestamp[V_NUMPAR];	/* partition timestamp (unsupported) */
	char v_asciilabel[128];		/* for compatibility */
};

#pragma pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _VTOC_H */
