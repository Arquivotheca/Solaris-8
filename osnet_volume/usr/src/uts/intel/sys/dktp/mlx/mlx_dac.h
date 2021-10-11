/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MLX_DAC_H
#define	_SYS_DKTP_MLX_DAC_H

#pragma ident	"@(#)mlx_dac.h	1.14	99/05/04 SMI"

/*
 * Mylex DAC960 Host Adapter Driver Header File.  Driver non-SCSI
 * private interfaces.
 */

#ifdef _KERNEL
#include <sys/dktp/mlx/mlx_dacioc.h>
#include <sys/dktp/cmpkt.h>
#include <sys/isa_defs.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convenience defines for mlx_dacioc_t type variables.
 * These are kept here and not in mlx_dacioc.h because these
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

/* Non-zero decimal values for ubuf_len field of mlx_dacioc_t in bytes */
#define	MLX_DACIOC_SIZE_UBUF_LEN	4
#define	MLX_DACIOC_CHKCONS_UBUF_LEN	808
#define	MLX_DACIOC_RBLD_UBUF_LEN	808
#define	MLX_DACIOC_GSTAT_UBUF_LEN	12
#define	MLX_DACIOC_RUNDIAG_UBUF_LEN	8192
#define	MLX_DACIOC_ENQUIRY_UBUF_LEN	100
#define	MLX_DACIOC_RBADBLK_UBUF_LEN	808
#define	MLX_DACIOC_RBLDSTAT_UBUF_LEN	12
#define	MLX_DACIOC_GREPTAB_UBUF_LEN	36
#define	MLX_DACIOC_SINFO_UBUF_LEN	64
#define	MLX_DACIOC_GEROR_UBUF_LEN	0	/* + NCHNS*MAX_TGT */
#define	MLX_DACIOC_LOADIMG_UBUF_LEN	0	/* count in REG[2] & REG[3] */
#define	MLX_DACIOC_STOREIMG_UBUF_LEN	0	/* count in REG[2] & REG[3] */
#define	MLX_DACIOC_RDCONFIG_UBUF_LEN	3364	/* + NCHNS*MAX_TGT*12 */
#define	MLX_DACIOC_WRCONFIG_UBUF_LEN	MLX_DACIOC_RDCONFIG_UBUF_LEN
#define	MLX_DACIOC_ADCONFIG_UBUF_LEN	MLX_DACIOC_RDCONFIG_UBUF_LEN
#define	MLX_DACIOC_RDNVRAM_UBUF_LEN	MLX_DACIOC_RDCONFIG_UBUF_LEN

/* Command Types */
#define	MLX_DAC_CTYPE0		0	/* Commands with No params */
#define	MLX_DAC_CTYPE1		1	/* Drive,block,count,physaddr */
#define	MLX_DAC_CTYPE2		2	/* Chan,target,state,physaddr */
#define	MLX_DAC_CTYPE4		4	/* Diagnostic Commands */
#define	MLX_DAC_CTYPE5		5	/* param,physaddr */
#define	MLX_DAC_CTYPE6		6	/* LOAD/STORE Image commands */
#define	MLX_DACIOC_CTYPE_GEN	255	/* Used only in MLX_DACIOC_GENERIC */

/* Type 0 Commands */
#define	MLX_DAC_FLUSH		0x0A	/* Flush */
#define	MLX_DAC_SETDIAG		0x31	/* Set Diagnostic Mode */

/* Type 1 Commands */
#define	MLX_DAC_READA		0x01	/* Read Ahead */
#define	MLX_DAC_READ_V1		0x02	/* Version 1 Read */
#define	MLX_DAC_READE		0x33	/* Read Extended */
#define	MLX_DAC_READ		0x36	/* Read recommended for v4.01 */
#define	MLX_DAC_SREAD_V1	0x82	/* Version 1 Scatter-Gather Read */
#define	MLX_DAC_SREADE		0xB3	/* Scatter-Gather Read Extended */
#define	MLX_DAC_SREAD		0xB6	/* Scatter-Gather Read for v4.01 */
#define	MLX_DAC_WRITE_V1	0x03	/* Version 1 Write */
#define	MLX_DAC_WRITEE		0x34	/* Write Extended */
#define	MLX_DAC_WRITE		0x37	/* Write recommended for v4.01 */
#define	MLX_DAC_SWRITE_V1	0x83	/* Version 1 Scatter-Gather write */
#define	MLX_DAC_SWRITEE		0xB4	/* Write with Scatter-Gather Extended */
#define	MLX_DAC_SWRITE		0xB7	/* Scatter-Gather Write for v4.01 */
#define	MLX_DAC_SIZE		0x08	/* Size of system drive */
#define	MLX_DAC_CHKCONS		0x0F	/* Check Consistency */

/* Type 2 Commands */
#define	MLX_DAC_RBLD		0x09	/* Rebuild SCSI Disk */
#define	MLX_DAC_START		0x10	/* Start Device */
#define	MLX_DAC_STOPC		0x13	/* Stop Channel */
#define	MLX_DAC_STARTC		0x12	/* Start Channel */
#define	MLX_DAC_GSTAT		0x14	/* Get Device State */
#define	MLX_DAC_RBLDA		0x16	/* Async Rebuild SCSI Disk */
#define	MLX_DAC_RESETC		0x1A	/* Reset Channel */

/*  Type 4 Commands */
#define	MLX_DAC_RUNDIAG		0x32	/* Run Diagnostic */

/* Type 5 Commands */
#define	MLX_DAC_OENQUIRY	0x05	/* old Enquire about system config */
#define	MLX_DAC_ENQUIRY		0x53	/* Enquire about system config */
#define	MLX_DAC_ENQUIRY2	0x1C	/* Enquire2 about system config */
#define	MLX_DAC_WRCONFIG	0x06	/* Write DAC960 Config */
#define	MLX_DAC_ORDCONFIG	0x07	/* old Read ROM Config */
#define	MLX_DAC_RDCONFIG	0x4E	/* Read ROM Config */
#define	MLX_DAC_RDCONFIG2	0x3D	/* Read ROM Config2 */
#define	MLX_DAC_RBADBLK		0x0B	/* Read Bad Block Table */
#define	MLX_DAC_RBLDSTAT	0x0C	/* Rebuild Status */
#define	MLX_DAC_GREPTAB		0x0E	/* Get Replacement Table */
#define	MLX_DAC_CREPTAB		0x30	/* Clear Replacement Table */
#define	MLX_DAC_GEROR		0x17	/* Get history of errors */
#define	MLX_DAC_ADCONFIG	0x18	/* Add Configuration */
#define	MLX_DAC_SINFO		0x19	/* Info about all system drives */
#define	MLX_DAC_RDNVRAM		0x38	/* Read NVRAM Config */

/* Type 6 Commands */
#define	MLX_DAC_LOADIMG		0x20	/* Get firmware Image */
#define	MLX_DAC_STOREIMG	0x21	/* Store firmware Image */
#define	MLX_DAC_PROGIMG		0x22	/* Program  Image */

/* Misc. */
#define	MLX_DAC_CHN_NUM		0xFF	/* virtual chn number */
#define	MLX_DAC_RESETC_SOFT		0
#define	MLX_DAC_RESETC_HARD		1

#ifdef _KERNEL
typedef struct mlx_dac_cmd {
	struct cmpkt cmpkt;
	char cdb;		/* target driver command */
	char scb;		/* controller status */
	ddi_dma_handle_t handle;
	ddi_dma_win_t dmawin;
	ddi_dma_seg_t dmaseg;
	struct mlx_ccb *ccb;
} mlx_dac_cmd_t;
#endif

typedef struct mlx_dac_unit {
	void *lkarg;
	uchar_t sd_num;		/* System Drive number */
} mlx_dac_unit_t;

#pragma	pack(1)

/* DAC960 Disk info */
typedef struct mlx_dac_disk {
	uchar_t chn;		/* channel number */
	uchar_t tgt;		/* SCSI id */
	uchar_t filler[2];
	ulong_t start_sect;	/* start sector for this System Drive */
	ulong_t nsect;		/* # of sectors */
} mlx_dac_disk_t;

/* DAC960 Arm info */
#define	MLX_DAC_MAX_DISK	4

typedef struct mlx_dac_arm {
	uchar_t ndisk;		/* # of disks on this arm: 0-3 */
	uchar_t filler[3];
	mlx_dac_disk_t disk[MLX_DAC_MAX_DISK];
} mlx_dac_arm_t;

/* System Drive info */
#define	MLX_DAC_MAX_ARM	8

typedef struct mlx_dac_sd {
	uchar_t	status;
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	raid	:7,
		cache	:1;
#else /* _BIT_FIELDS_HTOL */
	uchar_t	cache	:1,
		raid	:7;
#endif /* _BIT_FIELDS_HTOL */
	uchar_t	narm;		/* # of arms on the System Drive */
	uchar_t	filler;
	mlx_dac_arm_t arm[MLX_DAC_MAX_ARM];
} mlx_dac_sd_t;

/* Possible values for status field of mlx_dac_sd_t */
#define	MLX_DAC_ONLINE	0x03
#define	MLX_DAC_CRITICAL	0x04
#define	MLX_DAC_OFFLINE	0xFF

/* Possible values for raid.cache field of mlx_dac_sd_t */
#define	MLX_DAC_WRITE_THROUGH	0
#define	MLX_DAC_WRITE_BACK		1

/* DAC960 target (System Drive or non-System Drive) info */
typedef struct mlx_dac_tgt_info {
	uchar_t present;  /* 1 = dev configured at this address, 0 otherwise */
	uchar_t type;
	uchar_t cfg_state;
	uchar_t state;		/* The ONLY reliable field here! */
	uchar_t reserved1;
	uchar_t sync_per;	/* Read Only */
	uchar_t sync_off;	/* Read Only */
	uchar_t reserved2;
	ulong_t nsect;		/* configured size of this targ in sectors */
} mlx_dac_tgt_info_t;

/* Possible values for type field of mlx_dac_tgt_info_t */
#define	MLX_DAC_TGT_DISK(t)		(((t&0xf) << 4) == 0x10)
#define	MLX_DAC_TGT_TAPE(t)		(((t&0xf) << 4) == 0x20)
#define	MLX_DAC_TGT_10MHZ(t)		((t) & 0x20)
#define	MLX_DAC_TGT_WIDE(t)		((t) & 0x40)
#define	MLX_DAC_TGT_TAGQ(t)		((t) & 0x80)

/* Possible values for state field of mlx_dac_tgt_info_t */
#define	MLX_DAC_TGT_DEAD	0x0
#define	MLX_DAC_TGT_ONLINE	0x3
#define	MLX_DAC_TGT_STANDBY	0x10

/* System Drive Configuration Table preserved on DAC960 EEPROM */
#define	MLX_DAC_MAX_SD	8

typedef struct mlx_dac_conf {
	uchar_t nsd;			/* # of System Drives: 0-7 */
	uchar_t filler[3];
	mlx_dac_sd_t sd[MLX_DAC_MAX_SD];
	mlx_dac_tgt_info_t tgt_info[1];	/* 1st of [nchn][max_tgt] */
} mlx_dac_conf_t; /* used in MLX_DACIOC_WRCONFIG and MLX_DACIOC_RDCONFIG */

/* Information returned by  MLX_DAC_ENQUIRY command */
typedef struct mlx_dac_enquiry {
	ulong_t nsd;			/* Total Number of system drives */
	ulong_t sd_sz[MLX_DAC_MAX_SD]; 	/* sizes of system drives in sectors */
	ushort_t fw_age;		/* age of the firmware flash eep */
	uchar_t write_bk_err;		/* flag indicating write back error */
	uchar_t nfree_chg;		/* # of free entries in chg list */
	struct {
		uchar_t	version;
		uchar_t	release;
	} fw_num;			/* firmware release & version number */
	uchar_t stbyrbld;		/* standby drive being rebuilt to */
					/* replace a dead drive */
	uchar_t max_cmd;		/*  Number of Max iop Q tables */
	ulong_t noffline;		/*  Number of drives offline */
	ulong_t ncritical;		/* number of drives critical */
	ushort_t ndead;			/* Number of drives dead */
	ushort_t wonly;			/* no.of write only scsi devices */
	uchar_t info[44];
} mlx_dac_enquiry_t;			/* used in MLX_DACIOC_ENQUIRY */

#pragma	pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_MLX_DAC_H */
