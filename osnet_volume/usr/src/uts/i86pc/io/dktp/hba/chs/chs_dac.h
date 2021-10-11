/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_DKTP_CHS_DAC_H
#define	_SYS_DKTP_CHS_DAC_H

#pragma ident	"@(#)chs_dac.h	1.5	99/03/16 SMI"

/*
 * IBM RAID PCI Host Adapter Driver Header File.  Driver non-SCSI
 * private interfaces.
 */

#include "chs_dacioc.h"
#include <sys/dktp/cmpkt.h>
#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convenience defines for chs_dacioc_t type variables.
 * These are kept here and not in chs_dacioc.h because these
 * are not part of the public interfaces of the driver.
 */
#define	dacioc_drv		args.type1.drv
#define	dacioc_blk		args.type1.blk
#define	dacioc_cnt		args.type1.cnt
#define	dacioc_chn		args.type2.chn
#define	dacioc_tgt		args.type2.tgt
#define	dacioc_dev_state	args.type2.dev_state
#define	dacioc_test		args.type4.test
#define	dacioc_pass		args.type4.pass
#define	dacioc_chan   		args.type4.chan
#define	dacioc_param		args.param
#define	dacioc_count		args.type6.count
#define	dacioc_offset		args.type6.offset
#define	dacioc_gen_args		args.type_gen.gen_args
#define	dacioc_gen_args_len	args.type_gen.gen_args_len
#define	dacioc_xferaddr_reg	args.type_gen.xferaddr_reg

/* Non-zero decimal values for ubuf_len field of chs_dacioc_t in bytes */
#define	CHS_DACIOC_SIZE_UBUF_LEN	4
#define	CHS_DACIOC_CHKCONS_UBUF_LEN	808
#define	CHS_DACIOC_RBLD_UBUF_LEN	808
#define	CHS_DACIOC_GSTAT_UBUF_LEN	12
#define	CHS_DACIOC_RUNDIAG_UBUF_LEN	8192
#define	CHS_DACIOC_ENQUIRY_UBUF_LEN	100
#define	CHS_DACIOC_RBADBLK_UBUF_LEN	808
#define	CHS_DACIOC_RBLDSTAT_UBUF_LEN	12
#define	CHS_DACIOC_GREPTAB_UBUF_LEN	36
#define	CHS_DACIOC_SINFO_UBUF_LEN	64
#define	CHS_DACIOC_GEROR_UBUF_LEN	0	/* + NCHNS*MAX_TGT */
#define	CHS_DACIOC_LOADIMG_UBUF_LEN	0	/* count in REG[2] & REG[3] */
#define	CHS_DACIOC_STOREIMG_UBUF_LEN	0	/* count in REG[2] & */
							/* REG[3] */

#define	CHS_DACIOC_RDCONFIG_UBUF_LEN	3364	/* + NCHNS*MAX_TGT*12 */
#define	CHS_DACIOC_WRCONFIG_UBUF_LEN	CHS_DACIOC_RDCONFIG_UBUF_LEN
#define	CHS_DACIOC_ADCONFIG_UBUF_LEN	CHS_DACIOC_RDCONFIG_UBUF_LEN
#define	CHS_DACIOC_RDNVRAM_UBUF_LEN		CHS_DACIOC_RDCONFIG_UBUF_LEN

/* Command Types */
#define	CHS_DAC_CTYPE0		0	/* Commands with No params */
#define	CHS_DAC_CTYPE1		1	/* Drive,block,count,physaddr */
#define	CHS_DAC_CTYPE2		2	/* Chan,target,state,physaddr */
#define	CHS_DAC_CTYPE4		4	/* Diagnostic Commands */
#define	CHS_DAC_CTYPE5		5	/* param,physaddr */
#define	CHS_DAC_CTYPE6		6	/* LOAD/STORE Image commands */
#define	CHS_DACIOC_CTYPE_GEN	255	/* Used only in */
						/* CHS_DACIOC_GENERIC */

/* Type 0 Commands */
#define	CHS_DAC_FLUSH		0x0A	/* Flush */
#define	CHS_DAC_SETDIAG		0x31	/* Set Diagnostic Mode */
#define	CHS_DAC_NOOP		0xFF	/* No op */

/* Type 1 Commands */
#define	CHS_DAC_READA		0x01	/* Read Ahead */
#define	CHS_DAC_READ		0x02	/* Read */
#define	CHS_DAC_SREAD		0x82	/* Scatter-Gather Read */
#define	CHS_DAC_WRITE		0x03	/* Write */
#define	CHS_DAC_SWRITE		0x83	/* Write with Scatter-Gather */
#define	CHS_DAC_SIZE		0x08	/* Size of system drive */
#define	CHS_DAC_CHKCONS		0x0F	/* Check Consistency */

/* Type 2 Commands */
#define	CHS_DAC_RBLD		0x09	/* Rebuild SCSI Disk */
#define	CHS_DAC_START		0x10	/* Start Device */
#define	CHS_DAC_STOPC		0x13	/* Stop Channel */
#define	CHS_DAC_STARTC		0x12	/* Start Channel */
#define	CHS_DAC_GSTAT		0x14	/* Get Device State */
#define	CHS_DAC_RBLDA		0x16	/* Async Rebuild SCSI Disk */
#define	CHS_DAC_RESETC		0x1A	/* Reset Channel */

/*  Type 4 Commands */
#define	CHS_DAC_RUNDIAG		0x32	/* Run Diagnostic */

/* Type 5 Commands */
#define	CHS_DAC_ENQUIRY		0x05	/* Enquire system config */
#define	CHS_DAC_WRCONFIG		0x06	/* Write Config */
#define	CHS_DAC_RDCONFIG		0x07	/* Read ROM Config */
#define	CHS_DAC_RBADBLK		0x0B	/* Read Bad Block Table */
#define	CHS_DAC_RBLDSTAT		0x0C	/* Rebuild Status */
#define	CHS_DAC_GREPTAB		0x0E	/* Get Replacement Table */
#define	CHS_DAC_GEROR		0x17	/* Get history of errors */
#define	CHS_DAC_ADCONFIG		0x18	/* Add Configuration */
#define	CHS_DAC_SINFO		0x19	/* Info about all system drvs */
#define	CHS_DAC_RDNVRAM		0x38	/* Read NVRAM Config */

/* Type 6 Commands */
#define	CHS_DAC_LOADIMG		0x20	/* Get firmware Image */
#define	CHS_DAC_STOREIMG		0x21	/* Store firmware Image */
#define	CHS_DAC_PROGIMG		0x22	/* Program  Image */

/* Misc. */
#define	CHS_DAC_CHN_NUM		0xFF	/* virtual chn number */
#define	CHS_DAC_RESETC_SOFT		0
#define	CHS_DAC_RESETC_HARD		1


typedef struct chs_dac_cmd {
	struct cmpkt cmpkt;
	char cdb;		/* target driver command */
	char scb;		/* controller status */
	ddi_dma_handle_t handle;
	ddi_dma_win_t dmawin;
	ddi_dma_seg_t dmaseg;
	struct chs_ccb *ccb;
} chs_dac_cmd_t;

typedef struct chs_dac_unit {
	void *lkarg;
	uchar_t sd_num;	/* System Drive number */
} chs_dac_unit_t;

#pragma	pack(1)

/* Possible values for status field of chs_dac_sd_t */
#define	CHS_DAC_ONLINE	0x03
#define	CHS_DAC_CRITICAL	0x04
#define	CHS_DAC_OFFLINE	0x02
#define	CHS_DAC_FREE	0x00
#define	CHS_DAC_LDM		0x05
#define	CHS_DAC_SYS		0x06

/* Possible values for cache field of raidcache (bit 7) logical_drive */
#define	CHS_DAC_WRITE_THROUGH	0
#define	CHS_DAC_WRITE_BACK		1


/* Possible values for Params field of devstate */
#define	CHS_DAC_TGT_DISK(t)		((t&0x1f) == 0x00)
#define	CHS_DAC_TGT_TAPE(t)		((t&0x1f) == 0x01)
#define	CHS_DAC_TGT_10MHZ(t)		((t) & 0x20)
#define	CHS_DAC_TGT_WIDE(t)		((t) & 0x40)
#define	CHS_DAC_TGT_TAGQ(t)		((t) & 0x80)

/* Possible values for state field of devstat */
#define	CHS_DAC_TGT_EMP(s)		((s & 0x8f) == 0x0)
#define	CHS_DAC_TGT_DHS(s)		((s & 0x8f) == 0x4)
#define	CHS_DAC_TGT_DDD(s)		((s & 0x8f) == 0x8)
#define	CHS_DAC_TGT_RDY(s)		((s & 0x8f) == 0x81)
#define	CHS_DAC_TGT_HSP(s)		((s & 0x8f) == 0x85)
#define	CHS_DAC_TGT_RBL(s)		((s & 0x8f) == 0x8B)
#define	CHS_DAC_TGT_SHS(s)		((s & 0x8f) == 0x05)
#define	CHS_DAC_TGT_ONLINE(s)	((s & 0x8f) == 0x89)
#define	CHS_DAC_TGT_STANDBY(s)	((s & 0x8f) == 0x01)

#define	CHS_DAC_MAX_SD	8


#define	VIPER_MAX_TGT	15
#define	VIPER_MAX_CHN	3


typedef struct {
	uchar_t board_disc[8];
	uchar_t processor[8];
	uchar_t NoChanType;
	uchar_t NoHostIntType;
	uchar_t Compression;
	uchar_t NvramType;
	ulong_t ulNvramSize;
} hwdisk;



typedef struct {
	uchar_t chn;
	uchar_t tgt;
	ushort_t reserved;
	ulong_t ulstartSect;
	ulong_t ul_nosects;
}chs_chunk_t;

#define	CHS_MAX_CHUNKS	16

typedef struct {
	ushort_t userfield;
	uchar_t state;
	uchar_t raidcache;
	uchar_t NoChunkUnits;
	uchar_t stripsize;
	uchar_t params;
	uchar_t Reserved;
	ulong_t ulLogDrvSize;
	chs_chunk_t chunk[CHS_MAX_CHUNKS];
} logical_drive;


typedef struct {
	uchar_t Initiator;
	uchar_t Params;
	uchar_t miscflags;
	uchar_t state;
	ulong_t ulBlockCount;
	uchar_t DeviceID[28];
} devstate;

typedef union {
	devstate device;
} chs_dac_tgt_info_t;

/* System Drive Configuration Table preserved on IBM RAID PCI EEPROM */
typedef struct chs_dac_conf_viper {
	uchar_t nsd;
	uchar_t Date[3];
	uchar_t init_id[4];
	uchar_t host_id[12];
	uchar_t time_sign[8];
	struct {
		uint_t usCfgDrvUpdateCnt :16;
		uint_t concurDrvStartCnt :4;
		uint_t startupdelay	:4;
		uint_t auto_reaarrange	:1;
		uint_t cd_boot		:1;
		uint_t cluster 		:1;
		int reserved		:5;
	} useroptions;
	ushort_t userfield;
	uchar_t  RebuildRate;
	uchar_t  Reserve;
	hwdisk	  hardware_disc;
	logical_drive log_drv[CHS_DAC_MAX_SD];
	devstate dev[1]; /* dev[nchn][MAX_TGT +1]; */
} chs_dac_conf_viper_t;




typedef union {
	struct chs_dac_conf_viper viper;
} chs_dac_conf_t;

typedef	ushort_t deadinfo;

/* Information returned by  CHS_DAC_ENQUIRY command */

typedef struct chs_dac_enquiry_viper {
	uchar_t nsd;			/* Total Number of logical drives */
	uchar_t MiscFlag;
	uchar_t SLTFlag;
	uchar_t BSTFlag;
	uchar_t PwrChgcnt;
	uchar_t WrongAdrCnt;
	uchar_t UnidentCnt;
	uchar_t NVramDevChgCnt;
	uchar_t CodeBlkVersion[8];
	uchar_t BootBlkVersion[8];
	ulong_t  sd_sz[CHS_DAC_MAX_SD];	/* sizes of system drives */
					/* in sectors */
	uchar_t ConCurrentCmdCount;
	uchar_t MaxPhysicalDevices;
	ushort_t usFlashRepgmCount;
	uchar_t DefuncDiskCount;
	uchar_t RebuildFlag;
	uchar_t OffLineLogDrvCount;
	uchar_t CriticalDrvCOunt;
	ushort_t usConfigUpdateCount;
	uchar_t BlkFlag;
	uchar_t res;
	deadinfo AddrDeadDisk[1];
/*	deadinfo AddrDeadDisk[VIPER_MAX_TGT * VIPER_MAX_CHN]; */

	/* Note that if get adapter status command is sent wth desc field */
	/* not equal to 0 we would get additional VPD info and devstate	*/
	/* which are not needed at this time so we won't define it */

} chs_dac_enquiry_viper_t;		/* used in CHS_DACIOC_ENQUIRY */


typedef  union chs_dac_enquiry  {
	struct chs_dac_enquiry_viper viper;
} chs_dac_enquiry_t;



#define	CHS_VENDORNAME_SZ	8
/* a uniform structure used to get some info for logical drives */
typedef struct chs_ld {
	char	 vendorname[CHS_VENDORNAME_SZ];
	uchar_t state;
	ulong_t  size;
	uchar_t raid;
} chs_ld_t;



#pragma	pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CHS_DAC_H */
