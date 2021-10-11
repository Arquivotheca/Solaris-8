/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)saio.h	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		saio.h
 *
 *   Description:	contains data structures used by the standalone I/O
 *			system.
 *
 */

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
 * header file for standalone I/O package
 */

#include <sys/types.h>

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

/*
 * This table entry describes a device.  It exists in the PROM.  A pointer to
 * it is passed in "bootparam".  It can be used to locate ROM subroutines for
 * opening, reading, and writing the device.  NOTE: When using this interface,
 * only ONE device can be open at any given time.  In other words, it is not
 * possible to open a tape and a disk at the same time.
 */

struct boottab {
  char           b_dev[2];        /* Two character device name.          */
  long            (_FAR_ *b_probe)();    /* probe(): "-1" or controller number. */
  long            (_FAR_ *b_boot)();     /* boot(bp): "-1" or start address.    */
  long            (_FAR_ *b_open)();     /* open(iobp): "-"1 or "0".            */
  long            (_FAR_ *b_close)();    /* close(iobp): "-"1 or "0".           */
  long            (_FAR_ *b_strategy)(); /* strategy(iobp, rw): "-1" or "0".    */
  char           _FAR_ *b_desc;         /* Printable string describing device. */
  struct devinfo _FAR_ *b_devinfo;      /* Information to configure device.    */
};

enum MAPTYPES { /* Page map entry types. */
  MAP_MAINMEM,
  MAP_OBIO,
  MAP_MBMEM,
  MAP_MBIO,
  MAP_VME16A16D,
  MAP_VME16A32D,
  MAP_VME24A16D,
  MAP_VME24A32D,
  MAP_VME32A16D,
  MAP_VME32A32D
};

/*
 * This table gives information about the resources needed by a device.
 */
struct devinfo {
  unsigned long      d_devbytes;   /* Bytes occupied by device in IO space.  */
  unsigned long      d_dmabytes;   /* Bytes needed by device in DMA memory.  */
  unsigned long      d_localbytes; /* Bytes needed by device for local info. */
  unsigned long      d_stdcount;   /* How many standard addresses.           */
  unsigned long     _FAR_ *d_stdaddrs;  /* The vector of standard addresses.      */
  enum     MAPTYPES d_devtype;    /* What map space device is in.           */
  unsigned long      d_maxiobytes; /* Size to break big I/O's into.          */
};



/*
 * io block: the structure passed to or from the device drivers.
 * 
 * Includes pointers to the device
 * in use, a pointer to device-specific data (iopb's or device
 * state information, typically), cells for the use of seek, etc.
 * NOTE: expand at end to preserve compatibility with PROMs
 */
struct saioreq {
	char	si_flgs;
	struct boottab _FAR_ *si_boottab;	/* Points to boottab entry if any */
	char	_FAR_ *si_devdata;		/* Device-specific data pointer */
	long	si_ctlr;		/* Controller number or address */
	long	si_unit;		/* Unit number within controller */
	daddr_t	si_boff;		/* Partition number within unit */
	/* synonymous with our Solaris slice (max 8 per system) */

	daddr_t	si_cyloff;
	/* this is not used anywhere; interpreted to mean the start of
	 * this partition from the beginning of the disk (fdisk info)
	 * units used for this field are in 512-byte sectors, i.e.,
	 * physical disk blocks */

	off_t	si_offset;
	daddr_t	si_bn;			/* Block number to R/W */
	char	_FAR_ *si_ma;			/* Memory address to R/W */
	long	si_cc;			/* Character count to R/W */
	struct	saif _FAR_ *si_sif;		/* interface pointer */
	char 	_FAR_ *si_devaddr;		/* Points to mapped in device */
	char	_FAR_ *si_dmaaddr;		/* Points to allocated DMA space */
};


#define F_READ	01
#define F_WRITE	02
#define F_ALLOC	04
#define F_FILE	010
#define	F_EOF	020	/* EOF on device */
#define F_AJAR	040	/* Descriptor is "ajar:" stopped but not closed. */

/*
 * request codes. Must be the same as F_XXX above
 */
#define	READ	F_READ
#define	WRITE	F_WRITE

/*
 * how many files can be open at once.
 */
#define NFILES	6

/*
 * Ethernet interface descriptor
 */
struct saif {
	long	(_FAR_ *sif_xmit)();		/* transmit packet */
	long	(_FAR_ *sif_poll)();		/* check for and receive packet */
	long	(_FAR_ *sif_reset)();		/* reset interface */
	long	(_FAR_ *sif_macaddr)();	/* MAC address/ ethernet address */
	long	(_FAR_ *sif_prstats)();	/* print cumulative statistics */
};


/*  FIXME Clean this up when doing sunmon work */

/*
 * Types of resources that can be allocated by resalloc().
 */
enum RESOURCES { 
	RES_MAINMEM,		/* Main memory, accessible to CPU */
	RES_RAWVIRT,		/* Raw addr space that can be mapped */
	RES_DMAMEM,		/* Memory acc. by CPU and by all DMA I/O */
	RES_DMAVIRT,		/* Raw addr space accessible by DMA I/O */
	RES_PHYSICAL,		/* Physical address */
	RES_VIRTALLOC,		/* Virtual addresses used */
        RES_BOOTSCRATCH,        /* Memory <4MB used only by boot. */
        RES_CHILDVIRT		/* Virt anywhere, phys > 4MB */
};
