/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _I386_SYS_IHANDLE_H
#define	_I386_SYS_IHANDLE_H

#pragma ident	"@(#)ihandle.h	1.16	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEVT_DSK 0x13	/* .. .. Disk device	*/
#define	DEVT_SER 0x14	/* .. .. Pseudo-Serial device	*/
#define	DEVT_NET 0xFB 	/* .. .. Network device		*/

/* Types of BIOS devices */
#define	BDT_UNKNOWN	0
#define	BDT_HARD	1
#define	BDT_FLOPPY	2
#define	BDT_CDROM	3

/*
 * How boot.bin recognizes diskette drives.
 * Note: adding the #define not changing the algorithm.  AFB 1/27/99.
 */
#define	IS_FLOPPY(x)	((x) < 2)

#define	INT13_RESET	0x00
#define	INT13_READ	0x02
#define	INT13_WRITE	0x03
#define	INT13_PARMS	0x08
#define	INT13_READTYPE	0x15
#define	INT13_CHGLINE	0x16
#define	INT13_CHKEXT	0x41
#define	INT13_EXTREAD	0x42
#define	INT13_EXTWRITE	0x43
#define	INT13_EXTPARMS	0x48
#define	INT13_EMULTERM	0x4B
#define	INT13_BEFIDENT	0xF8

/* Packet used for INT13_EXTREAD/WRITE */
typedef struct dev_pkt {
	unsigned char	size;
	unsigned char	dummy1;
	unsigned char	nblks;
	unsigned char	dummy2;
	unsigned long	bufp;
	unsigned long	lba_lo;
	unsigned long	lba_hi;
	unsigned long	bigbufp_lo;
	unsigned long	bigbufp_hi;
} dev_pkt_t;

/* Data returned from INT13_EXTPARMS call */
typedef struct int13_extparms_result_tag {
	unsigned short bufsize;
	unsigned short flags;
	unsigned long phys_numcyl;
	unsigned long phys_numhead;
	unsigned long phys_numspt;
	unsigned long long phys_numsect;
	unsigned short bps;
	void *edd_ptr;
} int13_extparms_result_t;

/* El Torito INT13_EMULTERM call specification packet */
typedef struct int13_emulterm_packet_tag {
	unsigned char size;
	unsigned char media_type;
	unsigned char bios_code;
	unsigned char ctlr_index;
	unsigned long image_lba;
	unsigned short dev_spec;
	unsigned short buffer_seg;
	unsigned short load_seg;
	unsigned short load_count;
	unsigned char cyl_lo;
	unsigned char sectors_plus_cyl_hi;
	unsigned char heads;
	unsigned char dummy_for_alignment_purposes;
} int13_emulterm_packet_t;

struct ihandle			/* I/O device descriptor */
{
	unsigned char    type;		/* .. Device type code:	*/

	unsigned char    unit;		/* .. Device number	*/
	unsigned short   usecnt;    /* .. Device open count	*/

	char    fstype[8];	/* .. File system type code	*/
	char	*pathnm;    /* .. Device name pointer		*/

	union {				/* Device dependent section:	*/
		struct {			/* .. Disks:	*/
			void *alt; /* .. .. Alternate sector info	*/
			long  siz;	/* .. .. Partition size		*/
			daddr_t  par;	/* .. .. Partition offset	*/
			unsigned int   cyl; /* .. .. Number of cylinders */
			unsigned long  bas; /* .. .. Base sector	*/
			unsigned short bps; /* .. .. Bytes per sector */
			unsigned short spt; /* .. .. Sectors per track */
			unsigned short spc; /* .. .. Sectors per cylinder */
			unsigned long  csize; /* .. .. Device cache size */
			short num;	/* .. .. Partition (slice) no.	*/
			unsigned char lbamode;	/* use LBA, not geom, to acc */
			/*
			 * We set flop_change_line to 1 if the device is
			 * a floppy AND has change line read capability
			 */
			unsigned char flop_change_line;
		} disk;
	} dev;
};

#define	MAXDEVOPENS	6	/* Max number of open devices		*/

extern struct ihandle *open_devices[];
#define	devp(fd) (open_devices[fd])

/*
 * We allow multiple buffers within the cache, primarily for use with
 * diskettes.  Set the maximum number based on the typical size of a
 * diskette track.
 */
#define	DISK_CACHESIZE		(63 * 512)
#define	CACHE_MAXBUFFERS	(DISK_CACHESIZE / (9 * 1024))

struct shared_cache_info {
	char *memp;		/* address of allocated memory */
	unsigned long csize;	/* size of allocated memory */
	short owner;		/* device code of present user of cache */
	int count;		/* number of buffers in cache */
	struct {
		char *cachep;
		unsigned long cfirst;
	} multi[CACHE_MAXBUFFERS];
};
#define	active_buffer	multi[0].cachep
#define	active_sector	multi[0].cfirst

#define	CACHE_EMPTY	((unsigned long)-1)

extern struct shared_cache_info cache_info;

extern int find_net(int *, int, char *);
extern int open_net(struct ihandle *);
extern void close_net(struct ihandle *);
extern int read_net(struct ihandle *, caddr_t, u_int, u_int);
extern int write_net(struct ihandle *, caddr_t, u_int, u_int);

extern int find_disk(int *, int, char *);
extern int open_disk(struct ihandle *);
extern void close_disk(struct ihandle *);
extern int read_disk(struct ihandle *, caddr_t, u_int, u_int);
extern int write_disk(struct ihandle *, caddr_t, u_int, u_int);

extern void invalidate_cache(struct ihandle *);
extern void setup_cache_buffers(struct ihandle *);
extern int get_cache_buffer(struct ihandle *, u_int);
extern void set_cache_buffer(u_int);

#ifdef	__cplusplus
}
#endif

#endif /* _I386_SYS_IHANDLE_H */
