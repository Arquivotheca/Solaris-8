/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _BOOTBLK_H
#define	_BOOTBLK_H

#ident	"@(#)bootblk.h	1.4	99/03/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys\types.h>

typedef union {
	struct {
		unsigned short offp;
		unsigned short segp;
		} s;
	char far *p;
	u_long l;
} seg_ptr;

extern int BootDbg;	/* global debug output switch */

#define	DBG_READ	1
#define	DBG_READD	2
#define	DBG_INT13	4
#define	DBG_FLOW	8
#define	DBG_BIOS	0x10
#define	DBG_DEVINFO	0x20
#define	DBG_VTOC	0x40
#define	DBG_FDISK	0x80
#define	DBG_COPY	0x100
#define	DBG_ALTS	0x200
#define	DBG_NONUMLOCK	0x400
#define	DBG_ASKDEV	0x800
#define	DBG_ELTORITO	0x1000
#define	DBG_FDBOOTBIN	0x8000
#define	DBG_ALWAYS	0xFFFF	// Special value for pause()

#ifdef DEBUG
#define	Dprintf(f, x)		if (BootDbg & (f)) printf x
#define	Dpause(f, s)		pause(f, s)
#else
#define	Dprintf(f, x)
#define	Dpause(f, s)
#endif

// Definitions for int13_packet() return value.
#define	BIOS_ERRMASK	0xFF
#define	BIOS_CARRY	0x100

#define	FP_TO_SEG(x)	((ushort)(((ulong)(x)) >> 16))
#define	FP_TO_OFF(x)	((ushort)(x))

#pragma pack(1)
struct bios_dev {
	unsigned short dev_type;
	short use_LBA;
	unsigned char BIOS_code;
	unsigned char drive_type;	/* for diskette drives */
	union {
		struct {
			unsigned short secPerBlk;
		} lba;
		struct {
			unsigned short cyl, nCyl, secPerCyl;
			unsigned char secPerTrk, sector, head, trkPerCyl;
		} func2;
	} un;
};
#pragma pack()

/* Simpler names for nested items in bios_dev */
#define	u_secPerBlk	un.lba.secPerBlk
#define	u_cyl		un.func2.cyl
#define	u_nCyl		un.func2.nCyl
#define	u_secPerCyl	un.func2.secPerCyl
#define	u_secPerTrk	un.func2.secPerTrk
#define	u_trkPerCyl	un.func2.trkPerCyl

/* Values for bios_dev dev_type */
#define	DT_UNKNOWN	0
#define	DT_HARD		1
#define	DT_FLOPPY	2
#define	DT_CDROM	3

extern struct bios_dev boot_dev;
extern struct pri_to_secboot *realp;
extern long relsect;

extern void c_fatal_err(char *);
extern void memcpy(char far *, char far *, ushort);
extern unsigned short read_sectors(struct bios_dev *, ulong,
	ushort, char far *);

extern void fs_init(struct partentry *);
extern int fs_cd(char *);
extern long fs_open(char *);
extern long fs_read(long, char far *, long);
extern long fs_seek(long, long, int);
extern long fs_close(long);

#ifdef	__cplusplus
}
#endif

#endif /* _BOOTBLK_H */
