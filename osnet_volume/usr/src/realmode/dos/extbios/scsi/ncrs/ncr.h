/*
 * Copyright (c) 1998, Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)ncr.h	1.9	98/06/17 SMI"
 */

/*
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */


/*
 * #define	NCR_DEBUG
 */
#if defined(__ppc)
#define	printf	prom_printf		/* vla fornow */
#endif

#ifdef DEBUG
#define STATIC
#else
#define STATIC static
#endif

typedef	unsigned char bool_t;
typedef unchar	u_char;
typedef ulong	u_long;
typedef	ulong	ioadr_t;
typedef unchar	u_char;
typedef char *	caddr_t;
typedef ushort	u_int;
typedef ushort	size_t;
typedef ushort	IOADDR;
#define NULL	0

#include <stdio.h>
#include "ncr_scsi.h"

#define drv_usecwait(period)    microseconds(period)
#define putsx(string,hexval)    puthex(hexval),putstr(" "),putstr(string), \
                                putstr("\r")
#define putsxx(string,hex1,hex2) puthex(hex1),putstr(" "),puthex(hex2), \
                                putstr(" "),putstr(string),putstr("\r")

extern ulong inl(ushort);
#include "intr.h"
#include "script.h"
#include "debug.h"

/*
 * If your HBA supports DMA or bus-mastering, you may have your own
 * scatter-gather list for physically non-contiguous memory in one
 * I/O operation; if so, there's probably a size for that list.
 * It must be placed in the ddi_dma_lim_t structure, so that the system
 * DMA-support routines can use it to break up the I/O request, so we
 * define it here.
 */

#define	NCR_MAX_DMA_SEGS	17

/*
 * Scatter-gather list structure defined by HBA hardware, for
 * loop in ncr_dmaget()
 */

typedef	struct NcrTableIndirect {	/* Table Indirect entries */
	ulong   count;		/* 24 bit count */
	paddr_t address;	/* 32 bit physical address */
} ncrti_t;

typedef	struct DataPointers {
	ncrti_t	nd_data[NCR_MAX_DMA_SEGS];	/* Pointers to data buffers */
	unchar	nd_left;		/* number of entries left to process */
	unchar	nd_num;			/* # of filled entries in S/G table */
	ushort	nd_pad;			/* pad to 8 byte multiple */
} ntd_t;



#ifdef SOLARIS
/*
 * Information setup to describe a request
 */
typedef	struct	ncr_ccb {
	struct scsi_cmd	 nc_cmd;	/* common packet wrapper */
	union scsi_cdb	 nc_cdb;	/* scsi command description block */
	struct ncr_ccb	*nc_linkp;	/* doneq linked list ptr */

	ncrti_t	nc_sg[NCR_MAX_DMA_SEGS]; /* scatter/gather structure */
	unchar	nc_num;			/* number of S/G list entries */
	unchar	nc_cdblen;		/* actual length of cdb */
	unchar	nc_type;		/* type of request */
	bool_t	nc_queued;		/* TRUE if on waitq */
} nccb_t;
#endif


/*
 * Information maintained about each (target,lun) the HBA serves.
 */

typedef struct ncr_unit {
	/* DSA reg points here when this target is active */
	/* Table Indirect pointers for NCR SCRIPTS */
	struct	{
		unchar	nt_pad0;	/* currently unused */
		unchar	nt_sxfer;	/* SCSI transfer/period parms */
		unchar	nt_sdid;	/* SCSI destination ID for SELECT */
		unchar	nt_scntl3;	/* currently unused */
	} nt_selectparm;

	ncrti_t	nt_sendmsg;		/* pointer to sendmsg buffer */
	ncrti_t	nt_rcvmsg;		/* pointer to msginbuf */
	ncrti_t	nt_cmd;			/* pointer to cdb buffer */
	ncrti_t	nt_status;		/* pointer to status buffer */
	ncrti_t	nt_extmsg;		/* pointer to extended message buffer */
	ncrti_t	nt_syncin;		/* pointer to sync in buffer */
	ncrti_t	nt_syncout;		/* pointer to sync out buffer */
	ncrti_t	nt_widein;		/* pointer to wide in buffer */
	ncrti_t	nt_errmsg;		/* pointer to message reject buffer */
	ntd_t	nt_curdp;		/* current S/G data pointers */
	ntd_t	nt_savedp;		/* saved S/G data pointers */

#ifdef SOLARIS
	nccb_t	*nt_nccbp;		/* ccb for active request */
	nccb_t	*nt_waitq;		/* queue of pending requests */
	nccb_t	**nt_waitqtail;		/* queue tail ptr */

	struct	ncr_unit *nt_linkp;	/* wait queue link pointer */
#endif
	struct scsi_pkt	*nt_pktp;	/* pkt for active request */
	paddr_t	nt_dsa_physaddr;	/* physical addr of table indirects */

	/* these are the buffers the HBA actually does i/o to/from */
	union scsi_cdb	nt_cdb;		/* scsi command description block */
	/* keep these two together so HBA can transmit in single move */
	struct {
		unchar	ntu_id[1];	/* 1 byte identify msg */
		unchar	ntu_sync[5];	/* sdtr outbound message */
	} nt_id;
#define	nt_identify	nt_id.ntu_id
#define	nt_syncobuf	nt_id.ntu_sync
#define	nt_abortmsg	nt_id.ntu_sync[0]
	unchar	nt_msginbuf[1];		/* first byte of message in */
	unchar	nt_extmsgbuf[1];	/* length of extended message */
	unchar	nt_syncibuf[3];		/* room for sdtr inbound message */
	unchar	nt_wideibuf[2];		/* room for wdtr inbound message */
	unchar	nt_statbuf[1];		/* status byte */
	unchar	nt_errmsgbuf[1];	/* error message for target */

#ifdef SOLARIS
	ddi_dma_lim_t	nt_dma_lim;	/* per-target for sector size */
#endif
	unchar	nt_state;		/* current state of this target */
	unchar	nt_type;		/* type of request */

	bool_t	nt_goterror;		/* true if error occurred */
	unchar	nt_dma_status;		/* copy of DMA error bits */
	unchar	nt_scsi_status0;	/* copy of SCSI bus error bits */
	unchar	nt_scsi_status1;	/* copy of SCSI bus error bits */

	ushort	nt_target;		/* target number */
	ushort	nt_lun;			/* logical unit number */
	bool_t	nt_fastscsi;		/* true if > 5MB/sec, tp < 200ns */
	unchar	nt_sscfX10;		/* sync i/o clock divisor */

#ifdef SOLARIS
	unsigned 	nt_arq : 1;	/* auto-request sense enable */
	unsigned	nt_tagque : 1;	/* tagged queueing enable */
	unsigned	nt_resv : 6;
#endif

	ulong	nt_total_sectors;	/* total # of sectors on device */
} npt_t;

/*
 * The states a HBA to (target, lun) nexus can have are:
 */
#define	NPT_STATE_DONE		((unchar)0) /* processing complete */
#define	NPT_STATE_IDLE		((unchar)1) /* HBA is waiting for work */
#define	NPT_STATE_WAIT		((unchar)2) /* HBA is waiting for reselect */
#define	NPT_STATE_QUEUED	((unchar)3) /* target request is queued */
#define	NPT_STATE_DISCONNECTED	((unchar)4) /* disconnctd, wait for reconnect */
#define	NPT_STATE_ACTIVE	((unchar)5) /* this target is the active one */

/*
 * types of ccb requests stored in nc_type
 */
#define	NRQ_NORMAL_CMD		((unchar)0)	/* normal command */
#define	NRQ_ABORT		((unchar)1)	/* Abort message */
#define	NRQ_ABORT_TAG		((unchar)2)	/* Abort Tag message */
#define	NRQ_DEV_RESET		((unchar)3)	/* Bus Device Reset message */

/*
 * macro to get offset of npt_t members for compiling into the SCRIPT
 */
#define	NTOFFSET(label) ((long)&(((npt_t *)0)->label))

typedef struct ncr_blk {
#ifdef SOLARIS
	kmutex_t	 n_mutex;
	ddi_iblock_cookie_t n_iblock;
	dev_info_t	*n_dip;
#endif

	struct ncrops	*n_ops;		/* ptr to NCR SIOP ops table */

#ifdef SOLARIS
	npt_t	*n_pt[ NTARGETS_WIDE * NLUNS_PER_TARGET ];
					/* ptrs to target DSA structures */

	caddr_t	 n_ptsave;		/* save ptr to per target buffer */
	size_t	 n_ptsize;		/* save the size of the buffer */
#endif

	npt_t	*n_current;		/* ptr to active target's DSA */
	npt_t	*n_forwp;		/* ptr to front of the wait queue */
	npt_t	*n_backp;		/* ptr to back of the wait queue */
#ifdef SOLARIS
	npt_t	*n_hbap;		/* the HBA's target struct */
	nccb_t	*n_doneq;		/* queue of completed commands */
	nccb_t	**n_donetail;		/* queue tail ptr */

	ddi_acc_handle_t	n_handle;
	int     *n_regp;		/* this hba's regprop */
	int     n_reglen;		/* this hba's regprop reglen */

	int	n_reg;
	u_int	n_inumber;		/* interrupts property index */
	unchar	n_irq;			/* interrupt request line */

	int	n_sclock;		/* hba's SCLK freq. in MHz */
	int	n_speriod;		/* hba's SCLK period, in nsec. */
	int	n_minperiod[NTARGETS_WIDE]; /* minimum sync i/o period per target */
#endif
	unchar	n_scntl3;		/* 53c8xx hba's core clock divisor */
	unchar	n_syncstate;		/* sync i/o state flags */

	unchar	n_initiatorid;		/* this hba's target number and ... */
	IOADDR	n_ioaddr;		/* this hba's io port address */
	ushort	n_idmask;		/* ... its corresponding bit mask */
	unchar	n_iden_msg[1];		/* buffer for identify messages */
	unchar	n_state;		/* the HBA's current state */
	bool_t	n_is710;		/* TRUE if HBA is a 53c710 */
	bool_t	n_wide;			/* TRUE if HBA supports WIDE */
#ifdef SOLARIS
	bool_t	n_nodisconnect[NTARGETS_WIDE]; /* disable disconnects on target */
	unchar	n_disc_num;		/* number of disconnected devs */
	opaque_t n_cbthdl;		/* callback thread */
	bus_t   n_bustype;		/* bustype */
#endif

	unchar  n_compaq;		/* compaq */
	unchar  n_geomtype;		/* compaq geometry type */
} ncr_t;

#define	NSTATE_IDLE		((unchar)0) /* HBA is idle */
#define	NSTATE_ACTIVE		((unchar)1) /* HBA is processing a target */
#define	NSTATE_WAIT_RESEL	((unchar)2) /* HBA is waiting for reselect */

/*
 * states of the hba while negotiating synchronous i/o with a target
 */
#define	NSYNCSTATE(ncrp, nptp)	(ncrp)->n_syncstate[(nptp)->nt_target]
#define	NSYNC_SDTR_NOTDONE	((unchar)0) /* SDTR negotiation needed */
#define	NSYNC_SDTR_SENT		((unchar)1) /* waiting for target SDTR msg */
#define	NSYNC_SDTR_RCVD		((unchar)2) /* target waiting for SDTR msg */
#define	NSYNC_SDTR_REJECT	((unchar)3) /* send Message Reject to target */
#define	NSYNC_SDTR_DONE		((unchar)4) /* final state */


/*
 * action codes for interrupt decode routines in interrupt.c
 */
#define	NACTION_DONE		0x01	/* target request is done */
#define	NACTION_ERR		0x02	/* target's request got error */
#define	NACTION_DO_BUS_RESET	0x04	/* reset the SCSI bus */
#define	NACTION_SAVE_BCNT	0x08	/* save scatter/gather byte ptr */
#define	NACTION_SIOP_HALT	0x10	/* halt the current HBA program */
#define	NACTION_SIOP_REINIT	0x20	/* totally reinitialize the HBA */
#define	NACTION_SDTR		0x40	/* got SDTR interrupt */
#define	NACTION_SDTR_OUT	0x80	/* send SDTR message */
#define	NACTION_ACK		0x100	/* ack the last byte and continue */
#define	NACTION_CHK_INTCODE	0x200	/* SCRIPTS software interrupt */
#define	NACTION_INITIATOR_ERROR	0x400	/* send IDE message and then continue */
#define	NACTION_MSG_PARITY	0x800	/* send MPE error and then continue */
#define	NACTION_MSG_REJECT	0x1000	/* send MR and then continue */
#define	NACTION_BUS_FREE	0x2000	/* reselect error caused disconnect */
#define	NACTION_ABORT		0x4000	/* abort an invalid reconnect */
#define	NACTION_GOT_BUS_RESET	0x8000	/* detected a scsi bus reset */
#define	NACTION_GOT_MSGREJ	0x10000	/* receive MR */


/*
 * This structure defines which bits in which registers must be saved
 * during a chip reset. The saved bits were established by the
 * HBA's POST BIOS and are very hardware dependent.
 */
typedef	struct	regsave710 {
	unchar	nr_reg;		/* the register number */
	unchar	nr_bits;	/* the bit mask */
} nrs_t;


#define	ncr_save_regs(ncrp, np, rp, n) \
		ncr_saverestore((ncrp), (np), (rp), (n), TRUE)

#define	ncr_restore_regs(ncrp, np, rp, n) \
		ncr_saverestore((ncrp), (np), (rp), (n), FALSE)

/*
 * Handy constants
 */
#define	NCR_53c825	0x3
#define	NCR_53c875	0xf
#define NCR_53c875_95	0x1000
#define NCR_53c895	0xc

/* For returns from xxxcap() functions */

#define	FALSE		0
#define	TRUE		1
#define	UNDEFINED	-1

/*
 * Handy macros
 */
long longaddr(ushort offset, ushort segment);
#define NCR_KVTOP(vaddr)	longaddr((ushort)(vaddr), myds())

/* clear and set bits in an i/o register */
#define	ClrSetBits(ncrp, reg, clr, set) \
	outb((ncrp)->n_ioaddr + (reg), (inb((ncrp)->n_ioaddr + (reg)) \
			& ~(clr)) | (set))

#ifdef SOLARIS
/* Map (target, lun) to n_pt array index */
#define	TL2INDEX(target, lun)	((target) * NTARGETS_WIDE + (lun))

#define	SCMDP2NCCBP(cmdp)	((nccb_t *)((cmdp)->cmd_private))
#define	PKTP2NCCBP(pktp)	SCMDP2NCCBP(SCMD_PKTP(pktp))
#define	NCCBP2SCMDP(nccbp)	(&(nccbp)->nc_cmd)
#define	NCCBP2PKTP(nccbp)	(&NCCBP2SCMDP(nccbp)->cmd_pkt)

#define	NCR_BLKP(x)	(((struct ncr *)(x))->n_blkp)
#endif

/*
 * Debugging stuff
 */
#define	Byte0(x)		(x&0xff)
#define	Byte1(x)		((x>>8)&0xff)
#define	Byte2(x)		((x>>16)&0xff)
#define	Byte3(x)		((x>>24)&0xff)

/*
 * include all of the function prototype declarations
 */
#include "ncrops.h"
#include "ncrdefs.h"
