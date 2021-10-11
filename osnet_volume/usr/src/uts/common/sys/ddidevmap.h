/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDIDEVMAP_H
#define	_SYS_DDIDEVMAP_H

#pragma ident	"@(#)ddidevmap.h	1.12	99/05/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#include <sys/mman.h>

struct devmap_info {
	size_t	length;		/* and this length */
	size_t	page_size;	/* pte page size selected by framework */
	size_t	offset;		/* optimal page size based on this offset */
	ushort_t valid_flag;	/* flag to indicate the validity of data */
	uchar_t	byte_order;	/* the  endian characteristics of the mapping */

	/*
	 * describes  order in which the CPU will reference data.
	 */
	uchar_t	data_order;
};

struct ddi_umem_cookie {
	size_t		size;		/* size of allocation */
	caddr_t		kvaddr;		/* cookie virtual address */
	kmutex_t	lock;
	kcondvar_t	cv;
	uint_t		type;		/* see below for umem_cookie type */
	/*
	 * Following 3 members are used for UMEM_LOCKED cookie type
	 */
	page_t		**pparray;	/* shadow list from as_pagelock */
	void		*procp;		/* user process owning backing store */
	enum seg_rw	s_flags;	/* flags used during pagelock/fault */
	int		locked;
};

typedef void * ddi_umem_cookie_t;
typedef struct as *ddi_as_handle_t;

/*
 * type of umem_cookie:
 *    pageable memory allocated from segkp segment driver
 *    non-pageable memory allocated form kmem_getpages()
 *    locked umem allocated from ddi_umem_lock
 */
#define	KMEM_PAGEABLE		0x100	/* un-locked kernel memory */
#define	KMEM_NON_PAGEABLE	0x200	/* locked kernel memeory */
#define	UMEM_LOCKED		0x400	/* locked user process memory */

typedef void *devmap_cookie_t;

struct devmap_callback_ctl {
	int	devmap_rev;		/* devmap_callback_ctl version number */
	int	(*devmap_map)(devmap_cookie_t dhp, dev_t dev, uint_t flags,
				offset_t off, size_t len, void **pvtp);
	int	(*devmap_access)(devmap_cookie_t dhp, void *pvtp, offset_t off,
				size_t len, uint_t type, uint_t rw);
	int	(*devmap_dup)(devmap_cookie_t dhp, void *pvtp,
				devmap_cookie_t new_dhp, void **new_pvtp);
	void	(*devmap_unmap)(devmap_cookie_t dhp, void *pvtp, offset_t off,
				size_t len, devmap_cookie_t new_dhp1,
				void **new_pvtp1, devmap_cookie_t new_dhp2,
				void **new_pvtp2);
};

struct devmap_softlock {
	ulong_t		id;	/* handle grouping id */
	dev_t		dev; /* Device to which we are mapping */
	struct		devmap_softlock	*next;
	kmutex_t	lock;
	kcondvar_t	cv;
	int		refcnt;	/* Number of threads with mappings */
	size_t		softlocked;
};

struct devmap_ctx {
	ulong_t		id; /* handle grouping id */
	dev_info_t	dip; /* Device info struct for tracing context */
	struct devmap_ctx *next;
	kmutex_t	lock;
	kcondvar_t	cv;
	int		refcnt; /* Number of threads with mappings */
	uint_t		oncpu; /* this context is running on a cpu */
	timeout_id_t	timeout; /* Timeout ID */
};

/*
 * Fault information passed to the driver fault handling routine.
 * The DEVMAP_LOCK and DEVMAP_UNLOCK are used by software
 * to lock and unlock pages for physical I/O.
 */
enum devmap_fault_type {
	DEVMAP_ACCESS,		/* invalid page */
	DEVMAP_PROT,		/* protection fault */
	DEVMAP_LOCK,		/* software requested locking */
	DEVMAP_UNLOCK		/* software requested unlocking */
};

/*
 * seg_rw gives the access type for a fault operation
 */
enum devmap_rw {
	DEVMAP_OTHER,		/* unknown or not touched */
	DEVMAP_READ,		/* read access attempted */
	DEVMAP_WRITE,		/* write access attempted */
	DEVMAP_EXEC,		/* execution access attempted */
	DEVMAP_CREATE		/* create if page doesn't exist */
};

typedef struct devmap_handle {

	/*
	 * physical offset at the beginning of mapping.
	 */
	offset_t	dh_roff;

	/*
	 * user offset at the beginning of mapping.
	 */
	offset_t	dh_uoff;
	size_t		dh_len;		/* length of mapping */
	dev_t		dh_dev;		/* dev_t for this mapping */
	caddr_t		dh_kvaddr;  /* kernel mapping of device address */
	caddr_t		dh_uvaddr;  /* user mapping of device address */

	/*
	 * used to protect dh_cv for making the call to
	 * devmap_access single threaded.
	 */
	kmutex_t	dh_lock;

	/*
	 * to sync. faults for remap and unlocked kvaddr.
	 */
	kcondvar_t		dh_cv;
	struct seg		*dh_seg; /* segment created for this mapping */
	void			*dh_pvtp; /* device mapping private data */
	struct devmap_handle	*dh_next;
	struct devmap_softlock	*dh_softlock;
	struct devmap_ctx	*dh_ctx;
	ddi_umem_cookie_t	dh_cookie; /* kmem cookie */

	/*
	 * protection flag possible for attempted mapping.
	 */
	uint_t		dh_prot;

	/*
	 * Maximum protection flag for attempted mapping.
	 */
	uint_t		dh_maxprot;

	/*
	 * mmu level corresponds to the Max page size can be use for
	 * the mapping.
	 */
	uint_t		dh_mmulevel;
	uint_t		dh_flags;   /* see defines below */
	pfn_t		dh_pfn;		/* pfn corresponds to dh_reg_off */
	uint_t		dh_hat_attr;
	clock_t		dh_timeout_length;
	struct devmap_callback_ctl dh_callbackops;
} devmap_handle_t;

#endif	/* _KERNEL */

/*
 * define for devmap_rev
 */
#define	DEVMAP_OPS_REV 1

/*
 * defines for devmap_*_setup flag
 */
#define	DEVMAP_DEFAULTS			0x00
#define	DEVMAP_MAPPING_INVALID		0x01 	/* mapping is invalid */
#define	DEVMAP_ALLOW_REMAP		0x02	/* allow remap */
#define	DEVMAP_USE_PAGESIZE		0x04	/* use pagesize for mmu load */
#define	DEVMAP_UNLOAD_PENDING		0x08	/* unload is in progress. */

/*
 * defines for dh_flags
 */
#define	DEVMAP_SETUP_DONE		0x100	/* mapping setup is done */
#define	DEVMAP_LOCK_INITED		0x200	/* locks are initailized */
#define	DEVMAP_FAULTING			0x400   /* faulting */
#define	DEVMAP_LOCKED			0x800	/* dhp is locked. */
#define	DEVMAP_FLAG_LARGE		0x1000  /* cal. optimal pgsize */
#define	DEVMAP_FLAG_KPMEM		0x2000  /* kernel pageable memory */
#define	DEVMAP_FLAG_KMEM		0x4000  /* kernel non-pageable memory */
#define	DEVMAP_FLAG_DEVMEM		0x8000  /* device memory */

/*
 * Flags to pass to ddi_umem_alloc and ddi_umem_iosetup
 */
#define	DDI_UMEM_SLEEP		0x0
#define	DDI_UMEM_NOSLEEP	0x01
#define	DDI_UMEM_PAGEABLE	0x02

/*
 * Flags to pass to ddi_umem_lock to indicate expected access pattern
 * DDI_UMEMLOCK_READ implies the memory being locked will be read
 * (e.g., data read from memory is written out to the disk or network)
 * DDI_UMEMLOCK_WRITE implies the memory being locked will be written
 * (e.g., data from the disk or network is written to memory)
 * Both flags may be set in the call to ddi_umem_lock,
 * Note that this corresponds to the VM subsystem definition of read/write
 * and also correspond to the prots set in devmap
 * When doing I/O, B_READ/B_WRITE are used which have exactly the opposite
 * meaning. Be careful when using it both for I/O and devmap
 */
#define	DDI_UMEMLOCK_READ	0x01
#define	DDI_UMEMLOCK_WRITE	0x02

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDIDEVMAP_H */
