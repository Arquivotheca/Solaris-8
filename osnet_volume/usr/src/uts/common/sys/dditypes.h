/*
 * Copyright (c) by 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDITYPES_H
#define	_SYS_DDITYPES_H

#pragma ident	"@(#)dditypes.h	1.26	98/03/04 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM
/*
 * DMA types
 *
 * A DMA handle represent a "DMA object".  A DMA object is an abstraction
 * that represents the potential source or destination of DMA transfers to
 * or from a device.  The DMA object is the highest level description of
 * the source or destination and is not suitable for the actual transfer.
 *
 * Note, that we avoid the specific references to "mapping". The fact that
 * a transfer requires mapping is an artifact of the specific architectural
 * implementation.
 */
typedef	void *ddi_dma_handle_t;

/*
 * A dma window type represents a "DMA window".  A DMA window is a portion
 * of a dma object or might be the entire object. A DMA window has had system
 * resources allocated to it and is prepared to be transfered into or
 * out of. Examples of system resouces are DVMA mapping resources and
 * intermediate transfer buffer resources.
 *
 */
typedef	void *ddi_dma_win_t;


/*
 * A dma segment type represents a "DMA segment".  A dma segment is a
 * contiguous portion of a DMA window which is entirely addressable by the
 * device for a transfer operation.  One example where DMA segments are
 * required is where the system does not contain DVMA capability and
 * the object or window may be non-contiguous.  In this example the
 * object or window will be broken into smaller contiguous segments.
 * Another example is where a device or some intermediary bus adapter has
 * some upper limit on its transfer size (i.e. an 8-bit address register).
 * In this example the object or window will be broken into smaller
 * addressable segments.
 */
typedef	void *ddi_dma_seg_t;

/*
 * A DMA cookie contains DMA address information required to
 * program a DMA engine
 */
typedef struct {
	union {
		uint64_t	_dmac_ll;	/* 64 bit DMA address */
		uint32_t 	_dmac_la[2];    /* 2 x 32 bit address */
	} _dmu;
	size_t		dmac_size;	/* DMA cookie size */
	uint_t		dmac_type;	/* bus specific type bits */
} ddi_dma_cookie_t;

#define	dmac_laddress	_dmu._dmac_ll
#ifdef _LONG_LONG_HTOL
#define	dmac_notused    _dmu._dmac_la[0]
#define	dmac_address    _dmu._dmac_la[1]
#else
#define	dmac_address	_dmu._dmac_la[0]
#define	dmac_notused	_dmu._dmac_la[1]
#endif

/*
 * Interrupt types
 */
typedef void *ddi_iblock_cookie_t;	/* lock initialization type */
typedef union {
	struct {
		ushort_t	_idev_vector;	/* vector - bus dependent */
		ushort_t	_idev_priority;	/* priority - bus dependent */
	} idu;
	uint_t	idev_softint;	/* Soft interrupt register bit(s) */
} ddi_idevice_cookie_t;
#define	idev_vector	idu._idev_vector
#define	idev_priority	idu._idev_priority

/*
 * Other types
 */
typedef void *ddi_regspec_t;		/* register specification for now  */
typedef void *ddi_intrspec_t;		/* interrupt specification for now */
typedef void *ddi_softintr_t;		/* soft interrupt id */
typedef void *dev_info_t;		/* opaque device info handle */
typedef void *ddi_devmap_data_t;	/* Mapping cookie for devmap(9E) */
typedef void *ddi_mapdev_handle_t;	/* Mapping cookie for ddi_mapdev() */

/*
 * Device id type
 * NOTE: There is no struct ddi_devid, This is equivalent to a "void *",
 *	 but will cause an error if not casted to impl_devid_t.
 */
typedef struct ddi_devid *ddi_devid_t;	/* Opaque Device id */

/*
 * Define ddi_devmap_cmd types. This should probably be elsewhere.
 */
typedef enum {
	DDI_DEVMAP_VALIDATE = 0		/* Check mapping, but do nothing */
} ddi_devmap_cmd_t;

#endif	/* !_ASM */

#ifdef	_KERNEL
#ifndef _ASM

/*
 * Device Access Attributes
 */

typedef struct ddi_device_acc_attr {
	ushort_t devacc_attr_version;
	uchar_t devacc_attr_endian_flags;
	uchar_t devacc_attr_dataorder;
} ddi_device_acc_attr_t;

#define	DDI_DEVICE_ATTR_V0 	0x0001

/*
 * endian-ness flags
 */
#define	 DDI_NEVERSWAP_ACC	0x00
#define	 DDI_STRUCTURE_LE_ACC	0x01
#define	 DDI_STRUCTURE_BE_ACC	0x02

/*
 * Data ordering values
 */
#define	DDI_STRICTORDER_ACC	0x00
#define	DDI_UNORDERED_OK_ACC    0x01
#define	DDI_MERGING_OK_ACC	0x02
#define	DDI_LOADCACHING_OK_ACC  0x03
#define	DDI_STORECACHING_OK_ACC 0x04

/*
 * Data size
 */
#define	DDI_DATA_SZ01_ACC	1
#define	DDI_DATA_SZ02_ACC	2
#define	DDI_DATA_SZ04_ACC	4
#define	DDI_DATA_SZ08_ACC	8

/*
 * Data Access Handle
 */
#define	VERS_ACCHDL 			0x0001

typedef void *ddi_acc_handle_t;

typedef struct ddi_acc_hdl {
	int	ah_vers;		/* version number */
	void	*ah_bus_private;	/* bus private pointer */
	void 	*ah_platform_private; 	/* platform private pointer */
	dev_info_t *ah_dip;		/* requesting device */

	uint_t	ah_rnumber;		/* register number */
	caddr_t	ah_addr;		/* address of mapping */

	off_t	ah_offset;		/* offset of mapping */
	off_t	ah_len;			/* length of mapping */
	uint_t	ah_hat_flags;		/* hat flags used to map object */
	uint_t	ah_pfn;			/* physical page frame number */
	uint_t	ah_pnum;		/* number of contiguous pages */
	ulong_t	ah_xfermodes;		/* data transfer modes */
	ddi_device_acc_attr_t ah_acc;	/* device access attributes */
} ddi_acc_hdl_t;


/*
 * Device id types
 */
#define	DEVID_NONE		0
#define	DEVID_SCSI3_WWN		1
#define	DEVID_SCSI_SERIAL	2
#define	DEVID_FAB		3
#define	DEVID_ENCAP		4
#define	DEVID_MAXTYPE		4

/*
 * Kernel event cookies
 * Kernel event dispatch levels
 */
typedef uint_t ddi_eventcookie_t;
typedef enum {EPL_KERNEL, EPL_INTERRUPT, EPL_HIGHLEVEL} ddi_plevel_t;

#endif	/* !_ASM */

#define	PEEK_START		1
#define	POKE_START		2
#define	PEEK_FAULT		4
#define	POKE_FAULT		8
#define	NO_FAULT		10

#ifndef	_ASM
/* t_nofault data structure used to pass data to trap code */
typedef struct ddi_nofault_data {
	int op_type;			/* Operation performed */
	pfn_t pfn;			/* Phys pfn generating nofault */
	caddr_t pc;			/* PC for trap code to return to */
	label_t jmpbuf;			/* jmp buf for NO_FAULT calls */
	void *save_nofault;
} ddi_nofault_data_t;

#endif	/* !_ASM */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDITYPES_H */
