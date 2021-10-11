/*
 * Copyright 1994 Sun Microsystems, Inc. All Rights Reserved
 */

/* PRAGMA_PLACEHOLDER */

/*
 * Definitions for IDE (ATA) controllers
 */

/* 
 * Call this to find out if any ATA drives are installed; returns
 * a bitmask of drives available (2^dnum means drive dnum is present)
 */
int ata_find(void);

/* check for primary I/O address */
#define	ATA_PRIMARY_IO1	0x1F0
#define	ATA_PRIMARY_IO2	0x3F6

/* check for secondary I/O address, some bios'es support this. */
#define	ATA_SECONDARY_IO1	0x170
#define	ATA_SECONDARY_IO2	0x376

/*
 * port offsets from base address ioaddr1
 */
#define AT_DATA		0x00	/* data register 			*/
#define AT_ERROR	0x01	/* error register (read)		*/
#define AT_FEATURE	0x01	/* features (write)			*/
#define AT_COUNT	0x02    /* sector count 			*/
#define AT_SECT		0x03	/* sector number 			*/
#define AT_LCYL		0x04	/* cylinder low byte 			*/
#define AT_HCYL		0x05	/* cylinder high byte 			*/
#define AT_DRVHD	0x06    /* drive/head register 			*/
#define AT_STATUS	0x07	/* status/command register 		*/
#define AT_CMD		0x07	/* status/command register 		*/

/*
 * port offsets from base address ioaddr2
 */
#define AT_ALTSTATUS	0x00	/* alternate status (read)		*/
#define AT_DEVCTL	0x00	/* device control (write)		*/

/*	Device control register						*/
#define AT_EXTRAHDS	0x08    /* access high disk heads 		*/
#define AT_NIEN    	0x02    /* disable interrupts 			*/
#define AT_SRST     	0x04    /* controller reset			*/

/*
 * Status bits from AT_STATUS register
 */
#define ATS_BSY		0x80    /* controller busy 			*/
#define ATS_DRDY	0x40    /* drive ready 				*/
#define ATS_DF		0x20    /* write fault 				*/
#define ATS_DSC    	0x10    /* seek operation complete 		*/
#define ATS_DRQ      	0x08    /* data request 			*/
#define ATS_CORR	0x04    /* ECC correction applied 		*/
#define ATS_IDX		0x02    /* disk revolution index 		*/
#define ATS_ERR		0x01    /* error flag 				*/

/*
 * Status bits from AT_ERROR register
 */
#define ATE_AMNF	0x01    /* address mark not found		*/
#define ATE_TKONF	0x02    /* track 0 not found			*/
#define ATE_ABORT	0x04    /* aborted command			*/
#define ATE_IDNF	0x10    /* ID not found				*/
#define ATE_UNC		0x40	/* uncorrectable data error		*/
#define ATE_BBK		0x80	/* bad block detected			*/

/*
 * Drive selectors for AT_DRVHD register
 */
#define ATDH_DRIVE0	0xa0    /* or into AT_DRVHD to select drive 0 	*/
#define ATDH_DRIVE1	0xb0    /* or into AT_DRVHD to select drive 1 	*/

/*
 * ATA commands. 
 */
#define ATC_DIAG	0x90    /* diagnose command 			*/
#define ATC_RECAL	0x10	/* restore cmd, bottom 4 bits step rate */
#define ATC_SEEK	0x70    /* seek cmd, bottom 4 bits step rate 	*/
#define ATC_RDVER	0x40	/* read verify cmd			*/
#define ATC_RDSEC	0x20    /* read sector cmd			*/
#define ATC_RDLONG	0x23    /* read long without retry		*/
#define ATC_WRSEC	0x30    /* write sector cmd			*/
#define ATC_SETMULT	0xc6	/* set multiple mode			*/	
#define ATC_RDMULT	0xc4	/* read multiple			*/
#define ATC_WRMULT	0xc5	/* write multiple			*/
#define ATC_FORMAT	0x50	/* format track command 		*/
#define ATC_SETPARAM	0x91	/* set parameters command 		*/
#define ATC_READPARMS	0xec    /* Read Parameters command 		*/
#define ATC_READDEFECTS	0xa0    /* Read defect list			*/
#define ATC_PI_SRESET	0x08	/* ATAPI soft reset                     */
#define ATC_PI_ID_DEV	0xa1	/* ATAPI identify device                */
#define ATC_PI_PKT	0xa0	/* ATAPI packet command                 */
				/* conflicts with ATC_READDEFECTS !     */

/*
 * Low bits for Read/Write commands...
 */
#define ATCM_ECCRETRY	0x01    /* Enable ECC and RETRY by controller 	*/
				/* enabled if bit is CLEARED!!! 	*/
#define ATCM_LONGMODE	0x02    /* Use Long Mode (get/send data & ECC) 	*/
				/* enabled if bit is SET!!! 		*/

/*
 * direction bits
 * for ac_direction
 */
#define AT_NO_DATA	0		/* No data transfer */
#define AT_OUT		1		/* for writes */
#define AT_IN		2		/* for reads */

/*
 * status bits for ab_ctl_status
 */
#define ATA_ONLINE	0
#define ATA_OFFLINE	1
