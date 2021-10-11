
/*
 * Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
 */
/* #define DEBUG /*  */

#ident "@(#)ataprobe.h   1.1   97/10/01 SMI\n"

#include <stdio.h>

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
 * Definitions for ATA (IDE) controllers.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 */

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

#define	AT_IOADDR2	0x206	/* offset from ioaddr1 to ioaddr2	*/
/*
 * port offsets from base address ioaddr2
 */
#define	AT_DEVCTL	0x00	/* device control (write)		*/

/*	Device control register						*/
#define	AT_NIEN    	0x02    /* disable interrupts 			*/
#define	AT_SRST		0x04    /* controller reset			*/
#define	AT_DC3		0x08	/* must always be set			*/

/*
 * Status bits from AT_STATUS register
 */
#define	ATS_BSY		0x80    /* controller busy 			*/
#define	ATS_DRDY	0x40    /* drive ready 				*/
#define	ATS_DF		0x20    /* device fault				*/
#define	ATS_DSC    	0x10    /* seek operation complete 		*/
#define	ATS_DRQ		0x08    /* data request 			*/
#define	ATS_CORR	0x04    /* ECC correction applied 		*/
#define	ATS_IDX		0x02    /* disk revolution index 		*/
#define	ATS_ERR		0x01    /* error flag 				*/

/*
 * Drive selectors for AT_DRVHD register
 */
#define	ATDH_DRIVE0	0xa0    /* or into AT_DRVHD to select drive 0 	*/
#define	ATDH_DRIVE1	0xb0    /* or into AT_DRVHD to select drive 1 	*/
#define	ATDH_LBA	0x40	/* or into AT_DRVHD to enable LBA mode	*/

/*
 * Status bits from ATAPI Interrupt reason register (AT_COUNT) register
 */
#define	ATI_COD		0x01    /* Command or Data			*/
#define	ATI_IO		0x02    /* IO direction 			*/

/*
 * Commands
 */
#define	ATC_PI_PKT	0xa0	/* ATAPI packet command 		*/
#define	ATC_PI_ID_DEV	0xa1	/* ATAPI identify device		*/
#define	ATC_READPARMS	0xec    /* ATA identify device			*/
#define	ATC_RDSEC	0x20    /* read sector cmd			*/
#define	ATC_SOFT_RESET	0x08	/* ATAPI soft reset command		*/

/*
 * ATAPI
 */
#define	ATAPI_SIG_HI	0xeb		/* in high cylinder register	*/
#define	ATAPI_SIG_LO	0x14		/* in low cylinder register	*/


/*
 * Identify drive data
 */
struct ata_id {
/*  				   WORD					*/
/* 				   OFFSET COMMENT			*/
	ushort  ai_config;	  /*   0  general configuration bits 	*/
	ushort  ai_fixcyls;	  /*   1  # of fixed cylinders		*/
	ushort  ai_remcyls;	  /*   2  # of removable cylinders	*/
	ushort  ai_heads;	  /*   3  # of heads			*/
	ushort  ai_trksiz;	  /*   4  # of unformatted bytes/track 	*/
	ushort  ai_secsiz;	  /*   5  # of unformatted bytes/sector	*/
	ushort  ai_sectors;       /*   6  # of sectors/track		*/
	ushort  ai_resv1[3];      /*   7  "Vendor Unique"		*/
	char	ai_drvser[20];    /*  10  Serial number			*/
	ushort	ai_buftype;	  /*  20  Buffer type			*/
	ushort	ai_bufsz;	  /*  21  Buffer size in 512 byte incr  */
	ushort	ai_ecc;	  	  /*  22  # of ecc bytes avail on rd/wr */
	char	ai_fw[8];	  /*  23  Firmware revision		*/
	char	ai_model[40];     /*  27  Model #			*/
	ushort	ai_mult1;	  /*  47  Multiple command flags	*/
	ushort	ai_dwcap;	  /*  48  Doubleword capabilities	*/
	ushort	ai_cap;	  	  /*  49  Capabilities			*/
	ushort	ai_resv2;	  /*  50  Reserved			*/
	ushort	ai_piomode;	  /*  51  PIO timing mode		*/
	ushort	ai_dmamode;	  /*  52  DMA timing mode		*/
	ushort	ai_validinfo;     /*  53  bit0: wds 54-58, bit1: 64-70	*/
	ushort	ai_curcyls;	  /*  54  # of current cylinders	*/
	ushort	ai_curheads;	  /*  55  # of current heads		*/
	ushort	ai_cursectrk;     /*  56  # of current sectors/track	*/
	ushort	ai_cursccp[2];    /*  57  current sectors capacity	*/
	ushort	ai_mult2;	  /*  59  multiple sectors info		*/
	ushort	ai_addrsec[2];    /*  60  LBA only: no of addr secs	*/
	ushort	ai_sworddma;	  /*  62  single word dma modes		*/
	ushort	ai_dworddma;	  /*  63  double word dma modes		*/
	ushort	ai_advpiomode;    /*  64  advanced PIO modes supported	*/
	ushort	ai_minmwdma;      /*  65  min multi-word dma cycle info	*/
	ushort	ai_recmwdma;      /*  66  rec multi-word dma cycle info	*/
	ushort	ai_minpio;	  /*  67  min PIO cycle info		*/
	ushort	ai_minpioflow;    /*  68  min PIO cycle info w/flow ctl */
	ushort	ai_resv3[2];	  /* 69,70 reserved			*/
	ushort	ai_resv4[4];	  /* 71-74 reserved			*/
	ushort	ai_qdepth;	  /*  75  queue depth			*/
	ushort	ai_resv5[4];	  /* 76-79 reserved			*/
	ushort	ai_majorversion;  /*  80  major versions supported	*/
	ushort	ai_minorversion;  /*  81  minor version number supported*/
	ushort	ai_cmdset82;	  /*  82  command set supported		*/
	ushort	ai_cmdset83;	  /*  83  more command sets supported	*/
	ushort	ai_cmdset84;	  /*  84  more command sets supported	*/
	ushort	ai_features85;	  /*  85 enabled features		*/
	ushort	ai_features86;	  /*  86 enabled features		*/
	ushort	ai_features87;	  /*  87 enabled features		*/
	ushort	ai_ultradma;	  /*  88 Ultra DMA mode			*/
	ushort  ai_erasetime;	  /*  89 security erase time		*/
	ushort	ai_erasetimex;	  /*  90 enhanced security erase time	*/
	ushort	ai_padding1[35];  /* pad to 125 */
	ushort	ai_lastlun;	  /* 126 last LUN, as per SFF-8070i	*/
	ushort	ai_resv6;	  /* 127 reserved			*/
	ushort	ai_securestatus;  /* 128 security status		*/
	ushort  ai_vendor[31];	  /* 129-159 vendor specific		*/
	ushort	ai_padding2[96]; /* pad to 255 */
};

/*
 * Identify Drive: ai_config bits
 */

#define	ATA_ID_REM_DRV  0x80
#define	ATAPI_ID_CFG_PKT_SZ   0x3
#define	ATAPI_ID_CFG_PKT_12B  0x0
#define	ATAPI_ID_CFG_PKT_16B  0x1
#define	ATAPI_ID_CFG_DRQ_TYPE 0x60
#define	ATAPI_ID_CFG_DRQ_MICRO 0x00
#define	ATAPI_ID_CFG_DRQ_INTR 0x20
#define	ATAPI_ID_CFG_DRQ_FAST 0x40
#define	ATAPI_ID_CFG_DRQ_RESV 0x60
#define	ATAPI_ID_CFG_DEV_TYPE 0x0f00
#define	ATAPI_ID_CFG_DEV_SHFT 8

#define	ATAC_ATA_TYPE_MASK	0x8001
#define	ATAC_ATA_TYPE		0x0000
#define	ATAC_ATAPI_TYPE_MASK	0xc000
#define	ATAC_ATAPI_TYPE		0x8000


/*
 * Identify Drive: ai_cap bits
 */

#define	ATAC_DMA_SUPPORT	0x0100
#define	ATAC_LBA_SUPPORT	0x0200
#define	ATAC_IORDY_DISABLE	0x0400
#define	ATAC_IORDY_SUPPORT	0x0800
#define	ATAC_PIO_RESERVED	0x1000
#define	ATAC_STANDBYTIMER	0x2000


extern unsigned int debuglevel;

#define	DERR	0x0001	/* errors */
#define	DENT	0x0002	/* function entry points */
#define	DINIT	0x0004	/* initialisation */
#define	DIO	0x0008	/* IO */
#define	DFRAME	0x0010	/* Frame work */
#define	DALL	0xFFFF	/* All */

#ifdef DEBUG

#define	DEB_STR(level, str) \
	if (debuglevel & level) { \
		printf(str); \
	}

#define	DEB_HEX(level, num) \
	if (debuglevel & level) { \
		printf("%x", num); \
	}

#define	DEB_PAU(level, str) /* pause */ \
	if (debuglevel & level) { \
		printf(str); \
	}

#else /* !DEBUG */

#define	DEB_STR(level, str)
#define	DEB_HEX(level, num)
#define	DEB_PAU(level, num)

#endif /* DEBUG */

#ifdef DEBUG
#define	DPRINTF(level, fmt)	if (debuglevel & (level)) printf fmt 
#else
#define	DPRINTF(level, fmt)
#endif

#define	NBPSCTR 512

#pragma comment(compiler)

#define ATA_DELAY_400NSEC(port)	\
	((void)(inb(port), inb(port), inb(port), inb(port)))

