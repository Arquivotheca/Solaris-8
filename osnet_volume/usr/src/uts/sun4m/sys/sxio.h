/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SXIO_H
#define	_SYS_SXIO_H

#pragma ident	"@(#)sxio.h	1.20	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Data types to be used for SX ioctl(2) commands.
 */

#define	SX_IOC	('S' << 8)

struct sx_set_pgbnds {		/* for SX_SET_PGREGS command */

	uint_t	sp_vaddr;	/*  Virtual address which corresponds to */
				/*  physically contiguous DRAM */
	uint_t	sp_len;		/* Length in bytes */
};

/*
 * ioctl(2) commands and data definitions for changing the register contents
 * of SX (Sun Pixel Arithmetic Memory) graphics accelerator register set.
 * No commands are provided to read/write those registers which do not affect
 * the system or compromise the system security. It is expected that the
 * user maps a page containing the SX registers.
 */

/*
 * mask out the sxified page-frame-number (i.e, keep lower 20 bits)
 */
#define	SXPF_TOPF(x)		(x & 0xFFFFF)

/* Set page bound registers */
#define	SX_SET_PGBNDS		(SX_IOC | 1)
#define	SX_SET_DIAG_MODE	(SX_IOC | 2)
#define	SX_RESET		(SX_IOC | 3)	/* Software reset of SX */
#define	SX_SETCLR_GO		(SX_IOC | 4)

/*
 * Arguments for the SX_GO command
 */
#define	SX_CLEAR_GO	0x01
#define	SX_SET_GO	0x02

#define	SX_SET_ERRMASK	(SX_IOC | 5)
#define	SX_GET_ERRCODE	(SX_IOC | 6)
#define	SX_VALID_VA		(SX_IOC | 7)

struct sx_valid_va {

	uint_t	sp_vaddr;	/* Range of user provided virtual address */
	uint_t	sp_len;		/* and length */
	uint_t	sp_base_vaddr;	/* Driver returns virtual address it maps */
	uint_t	sp_base_len;	/* which falls within this subrange */
};

#define	SX_GET_ORIGINAL_VA	(SX_IOC | 8)

struct sx_original_va {

	uint_t	sp_sx_vaddr;	/* User specified SX mapped virtual addr */
	uint_t	sp_orig_vaddr;	/* Driver returns corresponding original */
				/* virtual address */
};

/*
 * Error codes for SX generated errors.
 */

#define	SX_NO_ERR	0x0	/* No error */
#define	SX_ILL_ERR	0x1	/* Attempt to execute illegal instruction */
#define	SX_PGBNDS_ERR	0x2	/* Out of page bounds access */
#define	SX_DVRAM_ERR	0x03	/* Access out of D[V]RAM address space */
#define	SX_REG_ERR	0x04	/* Attempt to access illegal register */
#define	SX_EADDR_ERR	0x05	/* Misaligned data */
#define	SX_IQ_ERR	0x06	/* Illegal write to the instruction Q */

#define	SX_SET_ERR_REG	(SX_IOC | 9)
/*
 * Mapping cookies for SX register set. Provides a mapping to the SX
 * register set in privileged and non-privileged mode.
 * These must be page-aligned.
 */
#define	SX_REG_MAP	0x0
#define	SX_PRIV_REG_MAP	0x1000
#define	SX_PGBLE_MAP	0x2000
#define	SX_VRAM_MAP	0x3000
#define	SX_CMEM_MAP	0x4000

#define	SX_CACHE_CTRL	(SX_IOC | 10)


typedef struct sx_cachectrl {
	int	sc_cmd;		/* Uncache or Cache; see flags below */
	uint_t	sc_orig_vaddr;	/* User specified original virtual addr */
	uint_t	sc_len;		/* length in bytes of the address range */

} sx_cachectrl_t;

#define	SX_PREP_FORSX 0x1	/* Mark memory as uncached */
#define	SX_DONE_WITHSX 0x2	/* Mark memory as cached; done using SX */

#ifdef _KERNEL

#define	SX_UADDR_OFFSET	0x1000

/*
 * SX registers which must be saved during a SX context switch.
 */

struct sx_cntxt {

	uint_t	spc_csr;		/* Control/Status register */
	uint_t	spc_ser;		/* Spam Error Register */
	uint_t	spc_pg_lo;		/* lower page bounds register	*/
	uint_t	spc_pg_hi;		/* Upper page bounds regiser	*/
	uint_t	spc_planereg;		/* Plane register		*/
	uint_t	spc_ropreg;		/* Raster operations control register */
	uint_t	spc_iqcounter;		/* SX timer */
	uint_t	spc_diagreg;
	uint_t	spc_regfile[SX_NREGS];	/* SX register file */
};

typedef struct sx_cntxt sx_cntxt_t;

struct sx_ctlregs {

	uint_t	spc_csr;		/* Control/Status register */
	uint_t	spc_ser;		/* Spam Error Register */
	uint_t	spc_pg_lo;		/* lower page bounds register	*/
	uint_t	spc_pg_hi;		/* Upper page bounds regiser	*/
	uint_t	spc_planereg;		/* Plane register		*/
	uint_t	spc_ropreg;		/* Raster operations control register */
	uint_t	spc_iqcounter;		/* SX timer */
	uint_t	spc_diagreg;
};

struct sx_regfile {

	uint_t	spc_regfile[SX_NREGS];	/* SX register file */
};

struct sx_proc {

	struct	as *spp_as;		/* The address space for this process */
	struct	proc	*spp_procp;	/* Pointer to the proc struct */
	struct 	sx_cntxt *spp_cntxtp;	/* Private register context save area */
	struct	sx_proc  *spp_next;
	struct	sx_proc  *spp_prev;
	struct	hat	*spp_hat;	/* SX HAT for this process */
	unsigned int spp_rss;		/* Size of SX accel'ed working set */
	unsigned short	spp_segcnt;	/* count of segments  on this proc */
	unsigned short	spp_errcode;	/* Record reason for SX errors here */
	unsigned short	spp_cntxtnum;	/* Context number of this process */
	unsigned short	spp_privflag;
};

typedef struct sx_proc sx_proc_t;

struct sx_cntxtinfo {		/* Information for each open of SX device */

	int	spc_state;	/* See flags below */
	struct	sx_proc *spc_sxprocp;
};

typedef	struct sx_cntxtinfo sx_cntxtinfo_t;

#define	SX_SPC_FREE	0x00	/* Entry in the clone device table is free */
				/* to be allocated to a SX context */
#define	SX_SPC_OPEN	0x01	/* Entry being used */
#define	SX_SPC_CLOSE	0x02	/* File descriptor closed but mappings exist */

/*
 * Reserve a page for SX context table. This should allow up to 512 processes
 * to access SX.
 */

#define	SX_MAXCNTXTS	(MMU_PAGESIZE / sizeof (sx_cntxtinfo_t))
#define	SX_MAXCNTXT_MASK	(SX_MAXCNTXTS -1)
/*
 * A process may request multiple mappings to D[V]RAM for SX memory
 * reference instructions but each of these mappings must be associated with a
 * SX register mapping i.e all SX memory reference instructions must
 * execute out of the context provided by the SX  registers. A process
 * cannot issue a request for a mapping to D[V]RAM for use in SX memory
 * reference instructions unless it has already requested a  mapping for SX
 * register set. To enforce these constraints each process requesting graphics
 * acceleration (i.e SX services) via requests for these mappings has to
 * open /dev/sx for each context. The driver clones each instance of the
 * open to provide redirection at the vnode level and provides  upto
 * SX_MAXCNTXTS SX contexts.
 */

/*
 * Minor numbers are used in an unusual way. Bits 0-5 of the minor
 * number encode the minor number of the "cloned" SX device providing upto
 * SX_MAXCNTXTS SX contexts.
 */
#define	SX_CLNUNIT(minor)	((minor & (SX_MAXCNTXT_MASK)))

/*
 * Per segment private data for all segments managed by the SX/MDI driver
 * XXX- 5.x version must have a hat field also.
 */

struct sx_seg_data {	/* Per segment private data	*/

	uint_t	sd_prot;	/* Protections for addresses mapped by seg */
	uint_t	sd_maxprot;	/* Max prot for addresses mapped by seg */
	caddr_t	sd_orig_vaddr; 	/* address mapped by some other segment */
				/* This segment and the other segment */
				/* will map different virtual addresses */
				/* to the same underlying phys memory */
	dev_t	sd_dev;		/* Device number of this device */
	struct vnode *sd_vp;	/* Shadow vnode of this dev entry */
	caddr_t	offset;		/* offset arg given to mmap(2)	*/
	uint_t	sd_valid;	/* This segment has at least one valid */
				/* translation set up */
	uint_t	sd_objtype;	/* Type of underlying memory i.e physically */
				/* contiguous DRAM, VRAM etc. SX registers */
	uint_t	sd_cntxtnum;	/* SX context number for this address space */
};

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SXIO_H */
