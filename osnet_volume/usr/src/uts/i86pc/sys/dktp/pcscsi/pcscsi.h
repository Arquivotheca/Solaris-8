/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)pcscsi.h	97/02/25 SMI"


#ifndef _SYS_DKTP_PCSCSI_H
#define	_SYS_DKTP_PCSCSI_H


#ifdef	__cplusplus
extern "C" {
#endif


/* ------------------------------------------------------------------------ */
/*	Global options and includes	*/

/*
 * The following defines affect pcscsi_impl_dma.h, included below,
 * and the code in pcscsi_impl_dmaget.c.
 */

/*
 * pcscsi does not have a physical limit on the size of the s/g table
 * so use 32, which is the convention in Solaris.
 * (pcscsi_impl_dma defaults to this value).
 */
/* Not needed for pcscsi.  */
/* #define	MAX_SG_LIST_ENTRIES	32   */


/*
 * pcssci needs to keep the physical addresses of the start of each DMA
 * segment for the Portability Layer function ScsiPortGetVirtualAddress.
 */
/* Defined by Makefile.rules	- needed by pcscsi.	*/
/* #define	SAVE_DMA_SEG_VIRT_ADDRS   */


/* pcscsi (as implemented) always wants a SG list	*/
/* Removes dmaget single-block-transfer code */
/* *Defined by Makefile.rules*	- needed by pcscsi.	*/
/* #define	SG_ALWAYS   */


/* pcscsi is little-endian, so don't define BIG_ENDIAN_HW	*/
/* #define	BIG_ENDIAN_HW	*/


/*
 * pcscsi currently does *not* do ARQ internally.
 * Implementation was begun, but not completed.
 */
/* #define	ARQ   */


/*
 * Number of requests which can be queued for a give target/lun.
 * The driver always attempts to send a request, and only if that fails
 * will it queue the request.
 * If the queue is full for this unit, the driver kicks the request back with
 * TRAN_BUSY.
 * NOTE defining this turns on some #ifdefs in the driver.
 * NOTE also that if the queue size is increased beyond 1, ARQ must be
 * implemented and turned on, or sense info on errors will be lost.
 */
/*#define	UNIT_QUEUE_SIZE		1   */


/*
 * Driver is constructed so that target completion routines can be either
 * run down asynchronously (via softintrs), or synchronously from the
 * ISR.
 */
/* #define	SYNCHRONOUS_TGT_COMPLETIONS   */


/*
 * Retry timout intervals for sending a reqeust to the AMD core.
 * Negative = Don't retry
 * 0 = Retry until success (or forever)
 * Postive = usecs to wait until giving up
 */
#define	NO_RETRY		 -1
#define	RETRY_FOREVER		0

#ifdef SYNCHRONOUS_TGT_COMPLETIONS
/*
 * NOTE if NORMAL_RETRY is anything other than NO_RETRY, 
 * you must not have target completion routines being run down 
 * synchronously from the ISR.  If you do, the i/o system may hang in a
 * cv_timedwait on the stack from the ISR.
 */
#define	NORMAL_RETRY		NO_RETRY
#else	/* SYNCHRONOUS_TGT_COMPLETIONS	*/
#define	NORMAL_RETRY		5000000		/* usec ( 5 seconds)	*/
#endif	/* SYNCHRONOUS_TGT_COMPLETIONS	*/

#define	POLLED_RETRY_TIMEOUT	10000000	/* usec (10 seconds)	*/
#define	ABORT_RETRY_TIMEOUT	20000000	/* usec (20 seconds)	*/
#define	RESET_RETRY_TIMEOUT	60000000	/* usec (60 seconds)	*/


/*
 * Misc defines.
 */
#define	DEFAULT_INITIATOR_ID	7

#define	MAX_TARGETS		8	/* No wide support, so only 8 */
#define	MAX_LUNS_PER_TARGET	8
/*
 * Tag queueing not implemented, so this could be set to 1 (not 0!)
 * to save a bit of space and time.
 * Note the driver contains arrays sized by this value - so the size
 * must be -at least- 1.
 * Tag queueing is not fully implemented - but - the driver currently uses
 * entry 0 in an array of size MAX_QUEUE_TAGS_PER_LUN for a request to
 * that target/lun.  This probably will need to be changed to implement
 * tag queueing correctly (for untagged queueing).
 */
#define	MAX_QUEUE_TAGS_PER_LUN	32


/* ------------------------------------------------------------------------ */
/*
 * Global function prototypes
 * (Funtions called FROM the portability layer but DEFINED in pcscsi.c
 */

void
pcscsi_request_completion(
	struct pcscsi_blk *pcscsi_blk_p,
	PSCSI_REQUEST_BLOCK srb_p);

void
pcscsi_start_next_request(
	struct pcscsi_blk *pcscsi_blk_p,
	uchar_t target, uchar_t lun);

#ifdef	UNIT_QUEUE_SIZE
struct pcscsi_ccb *
pcscsi_dequeue_request(int target, int lun, struct pcscsi_blk *blk_p);
#endif	/* UNIT_QUEUE_SIZE	*/


#ifdef PCSCSI_DEBUG
void
pcscsi_debug(uint funcs, uint granularity, char *message);
#endif /* PCSCSI_DEBUG */

/* ------------------------------------------------------------------------ */
/*
 * #defines
 */

#ifdef PCSCSI_DEBUG
/*
 * The routines/execution paths for which debug reporting can be turned on:
 * (for pcscsig_debug_gran)
 */
#define	DBG_PROBE		0x0001  /* Display HBA Probe data */
#define	DBG_ATTACH		0x0002  /* Display Attach data	*/
#define	DBG_TGT_INIT		0x0004  /* Display Tgt Attach data */
#define	DBG_TGT_PROBE		0x0008  /* Display Tgt Probe data */
#define	DBG_TRAN_INIT_PKT	0x0010  /* ...etc... */
#define	DBG_PKTALLOC		0x0020  /* */
#define	DBG_DMAGET		0x0040  /* */
#define	DBG_TRANSPORT		0x0080  /* */
#define	DBG_INTR		0x0100  /* */
#define	DBG_PKT_COMPLETION	0x0200  /* */
#define	DBG_PKT_CHK_ERRS	0x0400  /* */
#define	DBG_SOFTINT		0x0800  /* */
#define	DBG_ABORT		0x1000  /* */
#define	DBG_RESET		0x2000  /* */

#define	DBG_PORTABILITY		0x4000	/* Portability Layer happenings */
#define	DBG_CORE 		0x8000  /* AMD core(& ggmini_solaris.c) */

/* For all of the above, granularity of debug reporting.		*/
#define	DBG_ENTRY		0x0001  /* Function names on entry	*/
#define	DBG_RESULTS		0x0002  /* What happened, status	*/
#define	DBG_VERBOSE 		0x0004  /* Everything			*/

#endif /* PCSCSI_DEBUG */

#define	MAX_SCB_LEN		1	/* bytes */

#define	NO_TARGET		0xff	/* Used in pcscsi_start_next_request */
#define	NO_LUN			0xff	/* ...and ScsiPortNotification.	*/

#define	PCSCSI_BUSY_WAIT_USECS	25	/* Used in portability.c (for core) */


/* Note following values are the same for the AM53C974, AM53C974A, AM79C974. */
#define	AMD_VENDOR_ID		0x1022	/* AMD's PCI vendor id		*/
#define	PCSCSI_DEVICE_ID	0x2020	/* PCSCSI chip PCI device id	*/

/*
 * Start of PCI config space SCSI scratch regs, relative to the beginning of
 * the device's config space.
 */
#define	PCSCSI_SCRATCH_REGS_BASE	0x40	/* (ggmini_solaris.c)	*/



/* ------------------------------------------------------------------------ */
/*
 * Macros.
 */

/*	Kernel-virtual-address to physical-address macro.		*/
#define	PCSCSI_KVTOP(vaddr) (HBA_KVTOP((vaddr), pcscsig_pgshf, pcscsig_pgmsk))

#define	SDEV2MASTERGLUE(sd)	(TRAN2MASTERGLUE(SDEV2TRAN(sd)))
#define	SDEV2CLONEGLUE(sd)	(TRAN2CLONEGLUE(SDEV2TRAN(sd)))

#define	TRAN2MASTERGLUE(hba)	((struct pcscsi_glue *)(hba)->tran_hba_private)
#define	TRAN2CLONEGLUE(hba)	((struct pcscsi_glue *)(hba)->tran_tgt_private)
#define	TRAN2BLK(hba)		((TRAN2CLONEGLUE(hba))->pg_blk_p)
#define	TRAN2UNIT(hba)		((TRAN2CLONEGLUE(hba))->pg_unit_p)

#define	PKT2GLUE(pktp)		(TRAN2CLONEGLUE(PKT2TRAN(pktp)))
#define	PKT2UNIT(pktp)		(TRAN2UNIT(PKT2TRAN(pktp)))
#define	PKT2BLK(pktp)		(TRAN2BLK(PKT2TRAN(pktp)))
#define	PKT2CCB(pkt)		((struct pcscsi_ccb *)(pkt->pkt_ha_private))

#define	CCB2PKT(ccb)		((ccb->ccb_pkt_p))
#define	CCB2BLK(ccb)		(PKT2BLK(CCB2PKT(ccb)))
#define	CCB2TRAN(ccb)		(PKT2TRAN(CCB2PKT(ccb)))
#define	CCB2UNIT(ccb)		(TRAN2UNIT(CCB2TRAN(ccb)))
#define	CCB2UNITDIP(ccb)	(CCB2TRAN(ccb)->tran_sd->sd_dev)


#define	ADDR2UNIT(ap)		(TRAN2UNIT(ADDR2TRAN(ap)))
#define	ADDR2BLK(ap)		(TRAN2BLK(ADDR2TRAN(ap)))


#define	PCSCSI_DIP(pcscsi_glue)		(((pcscsi_glue)->pg_blk_p)->pb_dip)

/* used to get pcscsi_blk from pcscsi struct from upper layers */
#define	PCSCSI_BLKP(X) (((struct pcscsi_glue *)(X))->pg_blk_p)

#define	SCBP(pkt)	((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)	((*(pkt)->pkt_scbp) & STATUS_MASK)


/* ------------------------------------------------------------------------ */
/*
 * Typedefs.
 */

typedef enum {
	SCSI_CMD,
	CMD_ABORT,
	TARGET_ABORT_ALL,
	TARGET_RESET,
	BUS_RESET,
	HBA_INTERNAL_ARQ
} scsi_cmd_type_t;


/* ------------------------------------------------------------------------ */
/*
 * Driver-specific structure definitions.
 */

struct pcscsi_glue {
	scsi_hba_tran_t		*pg_hbatran_p;	/* Master or clone */
	struct pcscsi_blk	*pg_blk_p;
	struct pcscsi_unit	*pg_unit_p;	/* Null in master */
};


struct  pcscsi_blk {

	/*
	 * The following stuff is used by the Solaris part of the driver only.
	 */

	dev_info_t 		*pb_dip;	/* HBA dev_info 	*/

	/* Access handle to PCI config space for this device instance.  */
	ddi_acc_handle_t	pb_pci_config_handle;

	int			pb_initiator_id;	/* HBA SCSI ID	*/

	kmutex_t 		pb_core_mutex;	/* Mutex for strcts, core, hw */

	/*
	 * Flag that indicates that the AMD core code is ready to
	 * accept the next SCSI request (note this does *NOT* mean the
	 * last request is complete - only that the core is now in a
	 * state where it can start another command).
	 */
	boolean_t		pb_core_ready_for_next;
	kcondvar_t		pb_wait_for_core_ready;
	int			pb_send_requests_waiting; /* ...to be sent */

	ulong_t			pb_ioaddr;	/* IO address base used	*/
	uchar_t			pb_intr;	/* IRQ used		*/
	void			*pb_iblock_cookie; /* Interrupt cookie	*/
	uchar_t			pb_targetid;	/* Controller SCSI ID	*/

	ddi_dma_lim_t		*pb_dma_lim_p;	/* Per-driver DMA lims struct */


	boolean_t		pb_arq_enabled;	/* Driver ARQ flag	*/

	/*
	 * Array of pointers to all possible unit structs.
	 * Need this to get to LUN-specific info stored there for the
	 * core code.
	 */
	struct pcscsi_unit	*pb_unit_structs[MAX_TARGETS]
						[MAX_LUNS_PER_TARGET];

	short			pb_active_units;  /* Total unit structs */

	struct pcscsi_ccb	*pb_polling_ccb; /* pkt we're polling for */

	u_int			pb_last_rr_target; /* For round-robin */
	u_int			pb_last_rr_lun;	/* dequeuing of ccbs	*/

	ddi_softintr_t		pb_soft_int_id;	/* Soft int - tgt compl. */
	struct pcscsi_ccb	*pb_tgt_compl_q_head; /* Pkts needng tgt cmpl */

#ifdef PCSCSI_DEBUG
	ulong			pb_pending_comps; /* Pktcomp callbk list cnt */
#endif	/* PCSCSI_DEBUG	*/



	/*
	 * Following block of stuff needed exclusively for/by the
	 * Portability Layer (glue between AMD core code and Solaris).
	 */

	/*
	 * Flag (set in pcscsi_init_properties) which determines whether
	 * we'll let the core code do its Compaq-specific things.
	 * Examined in ScsiPortInitialize (in portability.c) to set/clear
	 * a bit a core struct.  Used nowhere else.
	 */
	boolean_t		pb_disable_compaq_specific;

	/* Replacements for PCI config space 'SCSI scratch' regs	*/
	ushort_t		pb_scratch_regs[8]; /* 8 16-bit registers */

	/* Physically contiguous DMAable buffer for the core's internal use. */
	caddr_t			pb_tempbuf_p;		/* Virtual address */
	paddr_t			pb_tempbuf_physaddr;	/* Physical address */
	int			pb_tempbuf_length;



	/*
	 * Following block of stuff needed exclusively for/by the AMD core
	 * code.
	 */

	HW_INITIALIZATION_DATA		pb_core_HwInitializationData;
	PSPECIFIC_DEVICE_EXTENSION	pb_core_DeviceExtension_p;
	PPORT_CONFIGURATION_INFORMATION pb_core_PortConfigInfo_p;

	/*
	 * This is really an array of ACCESS_RANGES, but we'll deal
	 * with it as a pointer as we don't know the number of
	 * elements.  Used only by the core code.
	 */
	ACCESS_RANGE			(*pb_core_AccessRanges_p)[];

	/*
	 * Saves the SpecificLuExtensionSize for use in tran_tgt_init to
	 * allocate the struct for each (active) target.
	 */
	ULONG				pb_core_SpecificLuExtensionSize;

	/*
	 * Saves the SrbExtensionSize for use in the pcscsi_ccb_alloc
	 * routine, to allocate and link this struct to each new Srb.
	 */
	ULONG				pb_core_SrbExtensionSize;

	/*
	 * These are just pointers to functions in the AMD core code.
	 *
	 * This info is really static, but putting them here saves
	 * having to create globals so ScsiPortInitialize (in
	 * portability.c) can get to them.
	 */
	PHW_INITIALIZE			pb_AMDInitializeAdapter;
	PHW_STARTIO			pb_AMDStartIo;
	PHW_INTERRUPT			pb_AMDInterruptServiceRoutine;
	PHW_DMA_STARTED			pb_AMDDmaStarted;
	PHW_FIND_ADAPTER		pb_AMDFindAdapter;
	PHW_RESET_BUS			pb_AMDResetScsiBus;
};



struct pcscsi_unit {

	short			pu_refcnt;		/* tran_tgt_init cnt  */
	short			pu_active_ccb_cnt;	/* on this target/lun */

	ddi_dma_lim_t		pu_lim;			/* Per-LUN DMA lims   */

	int			pu_total_sectors;	/* capacity (sectors) */

	unsigned		pu_arq		: 1;	/* Enable ARQ	*/
	unsigned		pu_tagque	: 1;	/* En tagged queueing */
	unsigned		pu_nodisconnect	: 1;	/* Disallow tgt disc. */
	unsigned		pu_resv		: 5;	/* Alignment filler   */

	/*
	 * SRBs active on this target/LUN, by queue tag.
	 * NOTE as tag queueing is not currently implemented,
	 * only element 0 of this array will be used.
	 */
	struct pcscsi_ccb	*pu_active_ccbs[MAX_QUEUE_TAGS_PER_LUN];

#ifdef UNIT_QUEUE_SIZE
	/*
	 * Array of ccbs wating to be sent to the HBA (for this target/lun)..
	 */
	struct pcscsi_ccb	*pu_ccb_queue_head;
	struct pcscsi_ccb	*pu_ccb_queue_tail;
	int			pu_ccb_queued_cnt;
#endif	/* UNIT_QUEUE_SIZE	*/


	/*
	 * The following block of stuff needed for the Portability Layer only.
	 */

	/* Pointer to the SpecificLuExtension for this target/lun	*/
	PSPECIFIC_LOGICAL_UNIT_EXTENSION	pu_SpecificLuExtension;
};



struct pcscsi_ccb {
	struct pcscsi_blk	*ccb_blk_p;	/* Back ptr to blk */
	struct scsi_address	*ccb_ap;	/* Ptr to scsi_address of tgt */
	struct scsi_pkt		*ccb_pkt_p;	/* ptr to pkt */
	scsi_cmd_type_t		ccb_cmd_type;	/* Normal cmd, abort, ARQ... */
	int			ccb_flags;	/* Options for this ccb	*/

	uchar_t			*ccb_cdb_p;	/* ptr to SCSI CDB	*/
	int			ccb_cdb_len;	/* Length of cdb	*/
	uchar_t			*ccb_scb_p;	/* ptr to SCSI Status Blk */
	int			ccb_scb_len;	/* Length of cdb	*/
	opaque_t		ccb_hw_request_p; /* Hw-specific xfer structs */

	dma_blk_t		*ccb_dma_p;	/* DMA resources struct	*/

	struct pcscsi_ccb	*ccb_next_ccb;  /* Link to next in queue    */
};



/* ------------------------------------------------------------------------ */
#ifdef	__cplusplus
}
#endif

#endif  /* _SYS_DKTP_PCSCSI_H */
