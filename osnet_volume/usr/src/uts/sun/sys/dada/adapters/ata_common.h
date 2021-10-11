/*
 * Copyrigth (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ATA_COMMON_H
#define	_ATA_COMMON_H

#pragma ident	"@(#)ata_common.h	1.25	99/06/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/varargs.h>
#include <sys/dktp/dadkio.h>
#include <sys/dada/dada.h>
#include <sys/ddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/dada/adapters/ghd/ghd.h>

#define	ATA_DEBUG
#define	UNDEFINED	-1



/*
 * ac_flags (per-controller)
 */
#define	AC_GHD_INIT			0x02
#define	AC_ATAPI_INIT			0x04
#define	AC_DISK_INIT			0x08
#define	AC_SCSI_HBA_TRAN_ALLOC		0x1000
#define	AC_SCSI_HBA_ATTACH		0x2000
#define	AC_ATTACH_IN_PROGRESS		0x4000

/*
 * device types
 */
#define	ATA_DEV_NONE	0
#define	ATA_DEV_DISK	1
#define	ATA_DEV_ATAPI	2

/*
 * ad_flags (per-drive)
 */
#define	AD_ATAPI		0x01
#define	AD_DISK			0x02
#define	AD_MUTEX_INIT		0x04
#define	AD_ATAPI_OVERLAP	0x08
#define	AD_DSC_OVERLAP		0x10
#define	AD_NO_CDB_INTR		0x20


/*
 * generic return codes
 */
#define	SUCCESS		DDI_SUCCESS
#define	FAILURE		DDI_FAILURE

/*
 * returns from intr status routines
 */
#define	STATUS_PARTIAL		0x01
#define	STATUS_PKT_DONE		0x02
#define	STATUS_NOINTR		0x03

/*
 * max targets and luns
 */
#define	ATA_MAXTARG	4
#define	ATA_CHANNELS 	2
#define	ATA_MAXLUN	16

/*
 * Controller port address defaults
 */
#define	ATA_BASE0	0x1f0
#define	ATA_BASE1	0x170

/*
 * port offsets from base address ioaddr1
 */
#define	AT_DATA		0x00	/* data register 			*/
#define	AT_ERROR	0x01	/* error register (read)		*/
#define	AT_FEATURE	0x01	/* features (write)			*/
#define	AT_COUNT	0x02    /* sector count 			*/
#define	AT_SECT		0x03	/* sector number 			*/
#define	AT_LCYL		0x04	/* cylinder low byte 			*/
#define	AT_HCYL		0x05	/* cylinder high byte 			*/
#define	AT_DRVHD	0x06    /* drive/head register 			*/
#define	AT_STATUS	0x07	/* status/command register 		*/
#define	AT_CMD		0x07	/* status/command register 		*/

/*
 * port offsets from base address ioaddr2
 */
#define	AT_ALTSTATUS	0x02	/* alternate status (read)		*/
#define	AT_DEVCTL	0x02	/* device control (write)		*/
#define	AT_DRVADDR	0x07 	/* drive address (read)			*/

/*
 * Device control register
 */
#define	ATDC_NIEN    	0x02    /* disable interrupts 			*/
#define	ATDC_SRST	0x04	/* controller reset			*/
#define	ATDC_D3		0x08	/* Mysterious bit, must be set		*/

/*
 * Status bits from AT_STATUS register
 */
#define	ATS_BSY		0x80    /* controller busy 			*/
#define	ATS_DRDY	0x40    /* drive ready 				*/
#define	ATS_DWF		0x20    /* write fault 				*/
#define	ATS_DSC    	0x10    /* seek operation complete 		*/
#define	ATS_DRQ		0x08	/* data request 			*/
#define	ATS_CORR	0x04    /* ECC correction applied 		*/
#define	ATS_IDX		0x02    /* disk revolution index 		*/
#define	ATS_ERR		0x01    /* error flag 				*/

/*
 * Status bits from AT_ERROR register
 */
#define	ATE_AMNF	0x01    /* address mark not found		*/
#define	ATE_TKONF	0x02    /* track 0 not found			*/
#define	ATE_ABORT	0x04    /* aborted command			*/
#define	ATE_IDNF	0x10    /* ID not found				*/
#define	ATE_MC		0x20    /* Media chane				*/
#define	ATE_UNC		0x40	/* uncorrectable data error		*/
#define	ATE_BBK		0x80	/* bad block detected			*/

/*
 * Drive selectors for AT_DRVHD register
 */
#define	ATDH_LBA	0x40	/* addressing in LBA mode not chs 	*/
#define	ATDH_DRIVE0	0xa0    /* or into AT_DRVHD to select drive 0 	*/
#define	ATDH_DRIVE1	0xb0    /* or into AT_DRVHD to select drive 1 	*/

/*
 * Common ATA commands.
 */
#define	ATC_DIAG	0x90    /* diagnose command 			*/
#define	ATC_RECAL	0x10	/* restore cmd, bottom 4 bits step rate */
#define	ATC_FORMAT	0x50	/* format track command 		*/
#define	ATC_SET_FEAT	0xef	/* set features				*/
#define	ATC_IDLE_IMMED	0xe1	/* idle immediate			*/
#define	ATC_STANDBY_IM	0xe0	/* standby immediate			*/
#define	ATC_DOOR_LOCK	0xde	/* door lock				*/
#define	ATC_DOOR_UNLOCK	0xdf	/* door unlock				*/

/*
 * common bits and options for set features (ATC_SET_FEAT)
 */
#define	SET_TFER_MODE	3
#define	FC_PIO_MODE 	0x8		/* Flow control pio mode */

/*
 * PPC irq13 register
 */
#define	IDE_PRIMARY	(1 << 0)
#define	IDE_SECONDARY	(1 << 1)

/*
 * Identify Drive: common config bits
 */
#define	ATA_ID_REM_DRV  0x80

/*
 * Identify Drive: common capability bits
 */
#define	ATAC_DMA_SUPPORT	0x0100
#define	ATAC_LBA_SUPPORT	0x0200
#define	ATAC_IORDY_DISABLE	0x0400
#define	ATAC_IORDY_SUPPORT	0x0800
#define	ATAC_PIO_RESERVED	0x1000
#define	ATAC_STANDBYTIMER	0x2000


/*
 * National chip specific data
 */
#define	CH1LMODE	0x01
#define	CH2LMODE	0x04
#define	CH1MASK		0x0100
#define	CH2MASK		0x0200
#define	RESETCTL	0x0004


#define	VENDORID	0
#define	DEVICEID	0x0001
#define	REVISION	0x0008
#define	CMDW		0x0002
#define	PIF		0x0009
#define	CTRLW		0x0020
#define	CTRLB		0x0042
#define	CH1D1DR		0x0044
#define	CH1D1DW		0x0045
#define	CH1D2DR		0x0048
#define	CH1D2DW		0x0049
#define	CH2D1DR		0x004c
#define	CH2D1DW		0x004d
#define	CH2D2DR		0x0050
#define	CH2D2DW		0x0051

#define	NPIOR0	0x85
#define	NPIOW0	0x85
#define	NPIOR1	0xb9
#define	NPIOW1	0xb9
#define	NPIOR2	0xeb
#define	NPIOW2	0xeb
#define	NPIOR3	0xec
#define	NPIOW3	0xec
#define	NPIOR4	0xfe
#define	NPIOW4	0xfe
#define	NDMAR0	0x89
#define	NDMAW0	0x89
#define	NDMAR1	0xee
#define	NDMAW1	0xee
#define	NDMAR2	0xfe
#define	NDMAW2	0xfe

#define	PIOR0	0x6d
#define	PIOW0	0x6d
#define	PIOR1	0x57
#define	PIOW1	0x57
#define	PIOR2	0x43
#define	PIOW2	0x43
#define	PIOR3	0x32
#define	PIOW3	0x32
#define	PIOR4	0x3f
#define	PIOW4	0x3f
#define	DMAR0	0x87
#define	DMAW0	0x87
#define	DMAR1	0x31
#define	DMAW1	0x31
#define	DMAR2	0x3f
#define	DMAW2	0x3f

#define	NSVID   0x100b
#define	NSDID   0x2

#define	CMDVID	0x1095
#define	CMDDID	0x646



#define	DEFAULT_DCD_OPTIONS DCD_MULT_DMA_MODE2


/*
 * macros from old common hba code
 */
#define	ATA_INTPROP(devi, pname, pval, plen) \
	(ddi_prop_op(DDI_DEV_T_NONE, (devi), PROP_LEN_AND_VAL_BUF, \
	DDI_PROP_DONTPASS, (pname), (caddr_t)(pval), (plen)))


/*
 * per-controller data struct
 */
#define	CTL2DRV(cp, t, l)	(cp->ac_drvp[t][l])
struct	ata_controller {
	dev_info_t		*ac_dip;
	struct ata_controller	*ac_next;
	uint32_t		ac_flags;
	int			ac_simplex;	/* 1 if in simplex else 0 */
	int			ac_actv_chnl;	/* only valid if simplex == 1 */
						/* if no cmd pending -1 */
	int			ac_pending[ATA_CHANNELS];
	kmutex_t		ac_hba_mutex;

	struct ata_pkt		*ac_active[ATA_CHANNELS];  /* active packet */
	struct ata_pkt		*ac_overlap[ATA_CHANNELS]; /* overlap packet */
	ccc_t			*ac_cccp[ATA_CHANNELS];    /* dummy for debug */

	ddi_iblock_cookie_t	ac_iblock;

	struct ata_drive	*ac_drvp[ATA_MAXTARG][ATA_MAXLUN];
	int			ac_max_transfer; /* max transfer in sectors */
	void			*ac_atapi_tran;	/* for atapi module */
	void			*ac_ata_tran;	/* for dada module */
	int32_t			ac_dcd_options;

	ccc_t			ac_ccc[ATA_CHANNELS];	/* for GHD module */

	/* op. regs data access handle */
	ddi_acc_handle_t	ata_datap[ATA_CHANNELS];
	caddr_t			ata_devaddr[ATA_CHANNELS];

	/* operating regs data access handle */
	ddi_acc_handle_t	ata_datap1[ATA_CHANNELS];
	caddr_t			ata_devaddr1[ATA_CHANNELS];

	ddi_acc_handle_t	ata_conf_handle;
	caddr_t			ata_conf_addr;

	ddi_acc_handle_t	ata_cs_handle;
	caddr_t			ata_cs_addr;

	ddi_acc_handle_t	ata_prd_acc_handle[ATA_CHANNELS];
	ddi_dma_handle_t	ata_prd_dma_handle[ATA_CHANNELS];
	caddr_t			ac_memp[ATA_CHANNELS];

	/* port addresses associated with ioaddr1 */
	uint8_t *ioaddr1[ATA_CHANNELS];
	uint8_t	*ac_data[ATA_CHANNELS];		/* data register */
	uint8_t	*ac_error[ATA_CHANNELS];	/* error register (read) */
	uint8_t	*ac_feature[ATA_CHANNELS];	/* features (write) */
	uint8_t	*ac_count[ATA_CHANNELS];	/* sector count	*/
	uint8_t	*ac_sect[ATA_CHANNELS];		/* sector number */
	uint8_t	*ac_lcyl[ATA_CHANNELS];		/* cylinder low byte */
	uint8_t	*ac_hcyl[ATA_CHANNELS];		/* cylinder high byte */
	uint8_t	*ac_drvhd[ATA_CHANNELS];	/* drive/head register */
	uint8_t	*ac_status[ATA_CHANNELS];	/* status/command register */
	uint8_t	*ac_cmd[ATA_CHANNELS];		/* status/command register */

	/* port addresses associated with ioaddr2 */
	uint8_t	*ioaddr2[ATA_CHANNELS];
	uint8_t	*ac_altstatus[ATA_CHANNELS];	/* alternate status (read) */
	uint8_t	*ac_devctl[ATA_CHANNELS];	/* device control (write) */
	uint8_t	*ac_drvaddr[ATA_CHANNELS];	/* drive address (read)	*/

	ushort_t	ac_vendor_id;		/* Controller Vendor */
	ushort_t	ac_device_id;		/* Controller Type */
	uchar_t		ac_revision;		/* Controller Revision */
	uchar_t		ac_piortable[5];
	uchar_t		ac_piowtable[5];
	uchar_t		ac_dmartable[3];
	uchar_t		ac_dmawtable[3];

	uchar_t		ac_reset_done;
	uchar_t		ac_suspended;
	uint32_t	ac_saved_dmac_address[ATA_CHANNELS];
	uint32_t	ac_polled_finish;
	uint32_t	ac_polled_count;
	uint32_t	ac_intr_unclaimed;
};

/*
 * per-drive data struct
 */
struct	ata_drive {

	struct ata_controller	*ad_ctlp; 	/* pointer back to ctlr */
	gtgt_t			*ad_gtgtp;
	struct dcd_identify	ad_id;  	/* IDENTIFY DRIVE data */

	uint_t			ad_flags;
	int			ad_channel;
	uchar_t			ad_targ;	/* target */
	uchar_t			ad_lun;		/* lun */
	uchar_t			ad_drive_bits;

	/*
	 * Used by atapi side only
	 */
	uchar_t			ad_cdb_len;	/* Size of ATAPI CDBs */

#ifdef DSC_OVERLAP_SUPPORT
	struct ata_pkt 		*ad_tur_pkt;	/* TUR pkt for DSC overlap */
#endif

	/*
	 * Used by disk side only
	 */
	struct dcd_device	ad_device;
	struct dcd_address	ad_address;
	struct dcd_identify	ad_inquiry;
	int32_t			ad_dcd_options;

	uchar_t			ad_dmamode;
	uchar_t			ad_piomode;

	uchar_t			ad_rd_cmd;
	uchar_t			ad_wr_cmd;

	ushort_t		ad_acyl;
	ushort_t		ad_bioscyl;	/* BIOS: number of cylinders */
	ushort_t		ad_bioshd;	/* BIOS: number of heads */
	ushort_t		ad_biossec;	/* BIOS: number of sectors */
	ushort_t		ad_phhd;	/* number of phys heads */
	ushort_t		ad_phsec;	/* number of phys sectors */
	short			ad_block_factor;
	short			ad_bytes_per_block;
	uchar_t			ad_cur_disk_mode; /* Current disk Mode */
	uchar_t			ad_run_ultra;
	ushort_t		ad_invalid;	/* Whether the device exits */
};

/*
 * The following are the defines for cur_disk_mode
 */
#define	PIO_MODE	0x01
#define	DMA_MODE	0x02


/*
 * Normal DMA capability bits
 * From quantum fireball drive
 */
#define	DMA_MODE0	0x0001
#define	DMA_MODE1	0x0002
#define	DMA_MODE2	0x0004

/*
 * Ultra dma capablity bits
 * from quantum fireball drive
 */
#define	UDMA_MODE0	0x0001
#define	UDMA_MODE1	0x0002
#define	UDMA_MODE2	0x0004


/*
 * ata common packet structure
 */
#define	AP_ATAPI		0x001	/* device is atapi */
#define	AP_ERROR		0x002	/* normal error */
#define	AP_TRAN_ERROR		0x004	/* transport error */
#define	AP_READ			0x008	/* read data */
#define	AP_WRITE		0x010	/* write data */
#define	AP_ABORT		0x020	/* packet aborted */
#define	AP_TIMEOUT		0x040	/* packet timed out */
#define	AP_BUS_RESET		0x080	/* bus reset */
#define	AP_DEV_RESET		0x100		/* device reset */
#define	AP_POLL			0x200	/* polling packet */
#define	AP_ATAPI_OVERLAP	0x400	/* atapi overlap enabled */
#define	AP_FREE			0x1000	/* packet is free! */
#define	AP_FATAL		0x2000	/* There is a fatal error */
#define	AP_DMA			0x4000	/* DMA operation required */

/*
 * (struct ata_pkt *) to (gcmd_t *)
 */
#define	APKT2GCMD(apktp)	(&apktp->ap_gcmd)

/*
 * (gcmd_t *) to (struct ata_pkt *)
 */
#define	GCMD2APKT(gcmdp)	((struct ata_pkt  *)gcmdp->cmd_private)

/*
 * (gtgt_t *) to (struct ata_controller *)
 */
#define	GTGTP2ATAP(gtgtp)	((struct ata_controller *)GTGTP2HBA(gtgtp))

/*
 * (gtgt_t *) to (struct ata_drive *)
 */
#define	GTGTP2ATADRVP(gtgtp)	((struct ata_drive *)GTGTP2TARGET(gtgtp))

/*
 * (struct ata_pkt *) to (struct ata_drive *)
 */
#define	APKT2DRV(apktp)		(GTGTP2ATADRVP(GCMDP2GTGTP(APKT2GCMD(apktp))))


/*
 * (struct hba_tran *) to (struct ata_controller *)
 */
#define	TRAN2ATAP(tranp)	((struct ata_controller *)TRAN2HBA(tranp))


#define	ADDR2CTLP(ap)		((struct ata_controller *) \
					TRAN2CTL(ADDR2TRAN(ap)))

#define	PKT2APKT(pkt)		(GCMD2APKT(PKTP2GCMDP(pkt)))

struct ata_pkt {
	gcmd_t			ap_gcmd;	/* GHD command struct */

	uint32_t		ap_flags;	/* packet flags */
	caddr_t			ap_v_addr;	/* I/O buffer address */


	/* preset values for task file registers */
	int			ap_chno;
	int			ap_targ;
	uchar_t			ap_sec;
	uchar_t			ap_count;
	uchar_t			ap_lwcyl;
	uchar_t			ap_hicyl;
	uchar_t			ap_hd;
	uchar_t			ap_cmd;

	/* saved status and error registers for error case */

	uchar_t			ap_status;
	uchar_t			ap_error;

	uint32_t		ap_addr;
	size_t			ap_cnt;

	/* disk/atapi callback routines */

	int			(*ap_start)(struct ata_controller *ata_ctlp,
					struct ata_pkt *ata_pktp);
	int			(*ap_intr)(struct ata_controller *ata_ctlp,
					struct ata_pkt *ata_pktp);
	void			(*ap_complete)(struct ata_pkt *ata_pktp,
					int do_callback);

	/* Used by disk side */

	char			ap_cdb;		/* disk command */
	char			ap_scb;		/* status after disk cmd */
	int32_t			ap_bytes_per_block; /* blk mode factor */

	/* Used by atapi side */

	uchar_t			ap_cdb_len;  /* length of SCSI CDB (in bytes) */
	uchar_t			ap_cdb_pad;	/* padding after SCSI CDB */
						/* (in shorts) */

	uint_t			ap_count_bytes;	/* Indicates the */
						/* count bytes for xfer */
	caddr_t			ap_buf_addr;
};

typedef struct ata_pkt ata_pkt_t;

/*
 * debugging
 */
#define	ADBG_FLAG_ERROR		0x0001
#define	ADBG_FLAG_WARN		0x0002
#define	ADBG_FLAG_TRACE		0x0004
#define	ADBG_FLAG_INIT		0x0008
#define	ADBG_FLAG_TRANSPORT	0x0010

#ifdef	ATA_DEBUG
#define	ADBG_FLAG_CHK(flag, fmt)
#else	/* !ATA_DEBUG */
#define	ADBG_FLAG_CHK(flag, fmt)
#endif	/* !ATA_DEBUG */


/*
 * Always print "real" error messages on non-debugging kernels
 */
#ifdef	ATA_DEBUG
#define	ADBG_ERROR(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_ERROR, fmt)
#else
#define	ADBG_ERROR(fmt)	ghd_err fmt
#endif

/*
 * ... everything else is conditional on the ATA_DEBUG preprocessor symbol
 */
#define	ADBG_WARN(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_WARN, fmt)
#define	ADBG_TRACE(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_TRACE, fmt)
#define	ADBG_INIT(fmt)		ADBG_FLAG_CHK(ADBG_FLAG_INIT, fmt)
#define	ADBG_TRANSPORT(fmt)	ADBG_FLAG_CHK(ADBG_FLAG_TRANSPORT, fmt)

/*
 * public function prototypes
 */
int ata_wait(ddi_acc_handle_t handle, uint8_t  *port, ushort_t onbits,
		ushort_t offbits, int usec_delay, int iterations);
int ata_set_feature(struct ata_drive *ata_drvp, uchar_t feature, uchar_t value);
void ata_write_config(struct ata_drive *ata_drvp);
void ata_write_config1(struct ata_drive *ata_drvp);

#ifdef	__cplusplus
}
#endif

#endif /* _ATA_COMMON_H */
