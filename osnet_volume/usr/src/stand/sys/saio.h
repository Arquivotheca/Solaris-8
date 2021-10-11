/*
 * Copyright (c) 1985-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * header file for standalone I/O package
 */

#ident	"@(#)saio.h	1.14	98/01/24 SMI" /* from SunOS 4.1 */

#include <sys/types.h>

/*
 * This table entry describes a device.  It exists in the PROM.  A pointer to
 * it is passed in "bootparam".  It can be used to locate ROM subroutines for
 * opening, reading, and writing the device.  NOTE: When using this interface,
 * only ONE device can be open at any given time.  In other words, it is not
 * possible to open a tape and a disk at the same time.
 */

struct boottab {
	char b_dev[2];		/* Two character device name.		*/
	int (*b_probe)();	/* probe(): "-1" or controller number.	*/
	int (*b_boot)();	/* boot(bp): "-1" or start address.	*/
	int (*b_open)();	/* open(iobp): "-"1 or "0".		*/
	int (*b_close)();	/* close(iobp): "-"1 or "0".		*/
	int (*b_strategy)();	/* strategy(iobp, rw): "-1" or "0".	*/
	char *b_desc;		/* Printable string describing device.	*/
	struct devinfo *b_devinfo; /* Information to configure device.	*/
};

enum MAPTYPES { /* Page map entry types. */
	MAP_MAINMEM,
	MAP_OBIO,
	MAP_MBMEM,
	MAP_MBIO
};

/*
 * This table gives information about the resources needed by a device.
 */
struct devinfo {
	unsigned int d_devbytes;   /* Bytes occupied by device in IO space.  */
	unsigned int d_dmabytes;   /* Bytes needed by device in DMA memory.  */
	unsigned int d_localbytes; /* Bytes needed by device for local info. */
	unsigned int d_stdcount;   /* How many standard addresses.  */
	unsigned long *d_stdaddrs; /* The vector of standard addresses. */
	enum MAPTYPES d_devtype;   /* What map space device is in.  */
	unsigned int d_maxiobytes; /* Size to break big I/O's into. */
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
	struct boottab *si_boottab;	/* Points to boottab entry if any */
	char	*si_devdata;		/* Device-specific data pointer */
	int	si_ctlr;		/* Controller number or address */
	int	si_unit;		/* Unit number within controller */
	daddr_t	si_boff;		/* Partition number within unit */
	daddr_t	si_cyloff;	/* used by PC abs offset of unix slice */
	off_t	si_offset;
	daddr_t	si_bn;			/* Block number to R/W */
	char	*si_ma;			/* Memory address to R/W */
	int	si_cc;			/* Character count to R/W */
	struct	saif *si_sif;		/* interface pointer */
	char 	*si_devaddr;		/* Points to mapped in device */
	char	*si_dmaaddr;		/* Points to allocated DMA space */
};


#define	F_READ	01
#define	F_WRITE	02
#define	F_ALLOC	04
#define	F_FILE	010
#define	F_EOF	020	/* EOF on device */
#define	F_AJAR	040	/* Descriptor is "ajar:" stopped but not closed. */

/*
 * request codes. Must be the same as F_XXX above
 */
#define	READ	F_READ
#define	WRITE	F_WRITE

/*
 * how many files can be open at once.
 */
#define	NFILES	6

/*
 * Ethernet interface descriptor
 */
struct saif {
	int	(*sif_xmit)();		/* transmit packet */
	int	(*sif_poll)();		/* check for and receive packet */
	int	(*sif_reset)();		/* reset interface */
	int	(*sif_macaddr)();	/* MAC address/ ethernet address */
	int	(*sif_prstats)();	/* print cumulative statistics */
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
	RES_BOOTSCRATCH,	/* Memory <4MB used only by boot. */
	RES_CHILDVIRT		/* Virt anywhere, phys > 4MB */
};
