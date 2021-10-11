/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_GENERIC_MODE_H
#define	_SYS_SCSI_GENERIC_MODE_H

#pragma ident	"@(#)mode.h	1.18	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *
 * Defines and Structures for SCSI Mode Sense/Select data - generic
 *
 */

/*
 * Structures and defines common for all device types
 */

/*
 * Mode Sense/Select Header.
 *
 * Mode Sense/Select data consists of a header, followed by zero or more
 * block descriptors, followed by zero or more mode pages.
 *
 */

struct mode_header {
	uchar_t length;		/* number of bytes following */
	uchar_t medium_type;	/* device specific */
	uchar_t device_specific;	/* device specfic parameters */
	uchar_t bdesc_length;	/* length of block descriptor(s), if any */
};

#define	MODE_HEADER_LENGTH	(sizeof (struct mode_header))

/*
 * Block Descriptor. Zero, one, or more may normally follow the mode header.
 *
 * The density code is device specific.
 *
 * The 24-bit value described by blks_{hi, mid, lo} describes the number of
 * blocks which this block descriptor applies to. A value of zero means
 * 'the rest of the blocks on the device'.
 *
 * The 24-bit value described by blksize_{hi, mid, lo} describes the blocksize
 * (in bytes) applicable for this block descriptor. For Sequential Access
 * devices, if this value is zero, the block size will be derived from
 * the transfer length in I/O operations.
 *
 */

struct block_descriptor {
	uchar_t density_code;	/* device specific */
	uchar_t blks_hi;	/* hi  */
	uchar_t blks_mid;	/* mid */
	uchar_t blks_lo;	/* low */
	uchar_t reserved;	/* reserved */
	uchar_t blksize_hi;	/* hi  */
	uchar_t blksize_mid;	/* mid */
	uchar_t blksize_lo;	/* low */
};

#define	MODE_BLK_DESC_LENGTH	(sizeof (struct block_descriptor))
#define	MODE_PARAM_LENGTH 	(MODE_HEADER_LENGTH + MODE_BLK_DESC_LENGTH)

/*
 * Define a macro to take an address of a mode header to the address
 * of the nth (0..n) block_descriptor, or NULL if there either aren't any
 * block descriptors or the nth block descriptor doesn't exist.
 */

#define	BLOCK_DESCRIPTOR_ADDR(mhdr, bdnum) \
	((mhdr)->bdesc_length && ((unsigned)(bdnum)) < \
	((mhdr)->bdesc_length/(sizeof (struct block_descriptor)))) ? \
	((struct block_descriptor *)(((ulong_t)(mhdr))+MODE_HEADER_LENGTH+ \
	((bdnum) * sizeof (struct block_descriptor)))) : \
	((struct block_descriptor *)0)

/*
 * Mode page header. Zero or more Mode Pages follow either the block
 * descriptors (if any), or the Mode Header.
 *
 * The 'ps' bit must be zero for mode select operations.
 *
 */

struct mode_page {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	code	:6,	/* page code number */
			:1,	/* reserved */
		ps	:1;	/* 'Parameter Saveable' bit */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	ps	:1,	/* 'Parameter Saveable' bit */
			:1,	/* reserved */
		code	:6;	/* page code number */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	length;		/* length of bytes to follow */
	/*
	 * Mode Page specific data follows right after this...
	 */
};

/*
 * Define a macro to retrieve the first mode page. Could be more
 * general (for multiple mode pages).
 */

#define	MODE_PAGE_ADDR(mhdr, type)	\
	((type *)(((ulong_t)(mhdr))+MODE_HEADER_LENGTH+(mhdr)->bdesc_length))

/*
 * Page codes follow the following specification:
 *
 *	Code Value(s)		What
 *	----------------------------------------------------------------------
 *	0x00			Vendor Unique (does not require page format)
 *
 *	0x02, 0x09, 0x0A	pages for all Device Types
 *	0x1A, 0x1C
 *
 *	0x01, 0x03-0x08,	pages for specific Device Type
 *	0x0B-0x19, 0x1B,
 *	0x1D-0x1F
 *
 *	0x20-0x3E		Vendor Unique (requires page format)
 *
 *	0x3F			Return all pages (valid for Mode Sense only)
 *
 */

/*
 * Page codes and page length values (all device types)
 */

#define	MODEPAGE_DISCO_RECO	0x02
#define	MODEPAGE_CACHING	0x08
#define	MODEPAGE_PDEVICE	0x09
#define	MODEPAGE_CTRL_MODE	0x0A
#define	MODEPAGE_POWER_COND	0x1A
#define	MODEPAGE_INFO_EXCPT	0x1C

#define	MODEPAGE_ALLPAGES	0x3F

/*
 * Mode Select/Sense page structures (for all device types)
 */

/*
 * Disconnect/Reconnect Page
 */

struct mode_disco_reco {
	struct	mode_page mode_page;	/* common mode page header */
	uchar_t	buffer_full_ratio;	/* write, how full before reconnect? */
	uchar_t	buffer_empty_ratio;	/* read, how full before reconnect? */
	ushort_t bus_inactivity_limit;	/* how much bus quiet time for BSY- */
	ushort_t disconect_time_limit;	/* min to remain disconnected */
	ushort_t connect_time_limit;	/* min to remain connected */
	ushort_t max_burst_size;	/* max data burst size */
#if defined(_BIT_FIELDS_LTOH)
	uchar_t		dtdc	: 3,	/* data transfer disconenct control */
			dimm	: 1,	/* disconnect immediate */
			fastat	: 1,	/* fair for status */
			fawrt	: 1,	/* fair for write */
			fard	: 1,	/* fair for read */
			emdp	: 1;	/* enable modify data pointers */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t		emdp	: 1,	/* enable modify data pointers */
			fard	: 1,	/* fair for read */
			fawrt	: 1,	/* fair for write */
			fastat	: 1,	/* fair for status */
			dimm	: 1,	/* disconnect immediate */
			dtdc	: 3;	/* data transfer disconenct control */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	reserved;
	ushort_t first_burst_sz;	/* first burst size */
};

#define	DTDC_DATADONE	0x01
					/*
					 * Target may not disconnect once
					 * data transfer is started until
					 * all data successfully transferred.
					 */

#define	DTDC_CMDDONE	0x03
					/*
					 * Target may not disconnect once
					 * data transfer is started until
					 * command completed.
					 */
/*
 * Caching Page
 */

struct mode_caching {
	struct	mode_page mode_page;	/* common mode page header */
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	rcd		: 1,	/* Read Cache Disable */
		mf		: 1,	/* Multiplication Factor */
		wce		: 1,	/* Write Cache Enable */
				: 5;	/* Reserved */
	uchar_t	write_ret_prio	: 4,	/* Write Retention Priority */
		dmd_rd_ret_prio	: 4;	/* Demand Read Retention Priority */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t			: 5,	/* Reserved */
		wce		: 1,	/* Write Cache Enable */
		mf		: 1,	/* Multiplication Factor */
		rcd		: 1;	/* Read Cache Disable */
	uchar_t	dmd_rd_ret_prio	: 4,	/* Demand Read Retention Priority */
		write_ret_prio	: 4;	/* Write Retention Priority */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	ushort_t pf_dsbl_trans_len;	/* Disable prefetch transfer length */
	ushort_t min_prefetch;		/* Minimum Prefetch */
	ushort_t max_prefetch;		/* Maximum Prefetch */
	ushort_t max_prefetch_ceiling;	/* Maximum Prefetch Ceiling */
};

/*
 * Peripheral Device Page
 */

struct mode_pdevice {
	struct	mode_page mode_page;	/* common mode page header */
	ushort_t if_ident;		/* interface identifier */
	uchar_t	reserved[4];		/* reserved */
	uchar_t	vendor_uniqe[1];	/* vendor unique data */
};

#define	PDEV_SCSI	0x0000		/* scsi interface */
#define	PDEV_SMD	0x0001		/* SMD interface */
#define	PDEV_ESDI	0x0002		/* ESDI interface */
#define	PDEV_IPI2	0x0003		/* IPI-2 interface */
#define	PDEV_IPI3	0x0004		/* IPI-3 interface */

/*
 * Control Mode Page
 *
 * Note:	This structure is incompatible with previous SCSI
 *		implementations. See <scsi/impl/mode.h> for an
 *		alternative form of this structure. They can be
 *		distinguished by the length of data returned
 *		from a MODE SENSE command.
 */

#define	PAGELENGTH_MODE_CONTROL_SCSI3	0x0A

struct mode_control_scsi3 {
	struct	mode_page mode_page;	/* common mode page header */
#if defined(_BIT_FIELDS_LTOH)
	uchar_t		rlec	: 1,	/* Report Log Exception bit */
			gltsd	: 1,	/* global logging target save disable */
				: 6;
	uchar_t		qdisable: 1,	/* Queue disable */
			que_err	: 1,	/* Queue error */
				: 2,
			que_mod : 4;    /* Queue algorithm modifier */
	uchar_t		eanp	: 1,	/* Enable AEN permission */
			uaaenp  : 1,	/* Unit attention AEN permission */
			raenp   : 1,	/* Ready AEN permission */
				: 1,
			bybths	: 1,	/* By both RESET signal */
			byprtm	: 1,	/* By port message */
			rac	: 1,	/* report a check */
			eeca	: 1;	/* enable extended contingent */
					/* allegiance (only pre-SCSI-3) */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t			: 6,
			gltsd	: 1,	/* global logging target save disable */
			rlec	: 1;	/* Report Log Exception bit */
	uchar_t		que_mod	: 4,	/* Queue algorithm modifier */
				: 2,
			que_err	: 1,	/* Queue error */
			qdisable: 1;	/* Queue disable */
	uchar_t		eeca	: 1,	/* enable extended contingent */
					/* allegiance (only pre-SCSI-3) */
			rac	: 1,	/* report a check */
			byprtm	: 1,	/* By port message */
			bybths	: 1,	/* By both RESET signal */
				: 1,
			raenp   : 1,	/* Ready AEN permission */
			uaaenp  : 1,	/* Unit attention AEN permission */
			eanp	: 1;	/* Enable AEN permission */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	reserved;
	ushort_t ready_aen_holdoff;	/* Ready AEN holdoff period */
	ushort_t busy_timeout;		/* Busy timeout period */
	uchar_t	reserved_2[2];
};

#define	CTRL_QMOD_RESTRICT	0x0
#define	CTRL_QMOD_UNRESTRICT	0x1

#ifdef	__cplusplus
}
#endif

/*
 * Include known generic device specific mode definitions and structures
 */

#include <sys/scsi/generic/dad_mode.h>

/*
 * Include implementation specific mode information
 */

#include <sys/scsi/impl/mode.h>

#endif	/* _SYS_SCSI_GENERIC_MODE_H */
