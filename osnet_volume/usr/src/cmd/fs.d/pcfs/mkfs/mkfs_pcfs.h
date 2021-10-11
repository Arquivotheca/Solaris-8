/*
 * Copyright (c) 1996, 1998, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MKFS_PCFS_H
#define	_MKFS_PCFS_H

#pragma ident	"@(#)mkfs_pcfs.h	1.7	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	BPSEC		512	/* Assumed # of bytes per sector */

#define	OPCODE1		0xE9
#define	OPCODE2		0xEB
#define	BOOTSECSIG	0xAA55

#define	uppercase(c)	((c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 'A' : (c))

#define	FAT12_TYPE_STRING	"FAT12   "
#define	FAT16_TYPE_STRING	"FAT16   "

#define	FAT16_ENTSPERSECT	256

#ifndef	SUNIXOSBOOT
#define	SUNIXOSBOOT	190	/* Solaris UNIX boot partition */
#endif

/*
 *  A macro implementing a ceiling function for integer divides.
 */
#define	idivceil(dvend, dvsor) \
	((dvend)/(dvsor) + (((dvend)%(dvsor) == 0) ? 0 : 1))

/*
 *	MS-DOS Disk layout:
 *
 *	---------------------
 *	|    Boot sector    |
 *	|-------------------|
 *	|   Reserved area   |
 *	|-------------------|
 *	|	FAT #1      |
 *	|-------------------|
 *	|	FAT #2      |
 *	|-------------------|
 *	|   Root directory  |
 *	|-------------------|
 *	|                   |
 *	|     File area     |
 *	|___________________|
 */

#ifdef i386
#pragma	pack(1)
#endif
struct _orig_bios_param_blk {
	ushort_t bytes_sector;
	uchar_t	 sectors_per_cluster;
	ushort_t resv_sectors;
	uchar_t	 num_fats;
	ushort_t num_root_entries;
	ushort_t sectors_in_volume;
	uchar_t	 media;
	ushort_t sectors_per_fat;
	ushort_t sectors_per_track;
	ushort_t heads;
	ulong_t	 hidden_sectors;
	ulong_t	 sectors_in_logical_volume;
};
#ifdef i386
#pragma pack()
#endif

#ifdef i386
#pragma	pack(1)
#endif
struct _bpb_extensions {
	uchar_t phys_drive_num;
	uchar_t reserved;
	uchar_t ext_signature;
	ulong_t volume_id;
	uchar_t volume_label[11];
	uchar_t type[8];
};
#ifdef i386
#pragma pack()
#endif

#ifdef i386
#pragma	pack(1)
#endif
struct _sun_bpb_extensions {
	ushort_t bs_offset_high;
	ushort_t bs_offset_low;
};
#ifdef i386
#pragma pack()
#endif

/*
 *  32 Bit FATS have different fields after the original
 *  bpb fields.  Right now we don't make much use of this
 *  structure, but we will eventually.
 */
#ifdef i386
#pragma	pack(1)
#endif
struct _bpb32_extensions {
	ulong_t  big_sectors_per_fat;
	ushort_t ext_flags;
	uchar_t	 fs_vers_lo;
	uchar_t	 fs_vers_hi;
	ulong_t	 root_dir_clust;
	ushort_t fsinfosec;
	ushort_t backupboot;
	ushort_t reserved;
};
#ifdef i386
#pragma pack()
#endif

#ifdef i386
#pragma	pack(1)
#endif
struct _bios_param_blk {
	struct _orig_bios_param_blk bpb;
	struct _bpb_extensions	    ebpb;
};
#ifdef i386
#pragma pack()
#endif

#ifdef i386
#pragma	pack(1)
struct _boot_sector {
	uchar_t			   bs_jump_code[3];
	uchar_t			   bs_oem_name[8];
	struct _bios_param_blk	   bs_bpb;
	struct _sun_bpb_extensions bs_sun_bpb;
	uchar_t			   bs_bootstrap[444];
	ushort_t		   bs_signature;
};
#pragma pack()
#else
#define	ORIG_BPB_START_INDEX	9	/* index into filler field */
#define	BPB_32_START_INDEX	34	/* index into filler field */
struct _boot_sector {
	uchar_t	 bs_jump_code[2];
	uchar_t  bs_filler[60];
	uchar_t  bs_sun_bpb[4];
	uchar_t	 bs_bootstrap[444];
	ushort_t bs_signature;
};
#endif

typedef union _ubso {
	struct _boot_sector bs;
	struct mboot	    mb;
	uchar_t		    buf[BPSEC];
} boot_sector_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _MKFS_PCFS_H */
