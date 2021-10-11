/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)53c8xx.h	1.3	97/07/21 SMI"
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
 * This file was derived from the 53c8xx.h file in the Solaris
 * driver.
 */


enum ncr53c8xxregs {		/* To access NCR 53C8xx registers */
NREG_SCNTL0 =	0x00,	NREG_SCNTL1,	NREG_SCNTL2,	NREG_SCNTL3,
NREG_SCID,		NREG_SXFER,	NREG_SDID,	NREG_GPREG,
NREG_SFBR,		NREG_SOCL,	NREG_SSID,	NREG_SBCL,
NREG_DSTAT,		NREG_SSTAT0,	NREG_SSTAT1,	NREG_SSTAT2,
NREG_DSA,
NREG_ISTAT =	0x14,
NREG_CTEST0 =	0x18,	NREG_CTEST1,	NREG_CTEST2,	NREG_CTEST3,
NREG_TEMP,
NREG_DFIFO =	0x20,	NREG_CTEST4,	NREG_CTEST5,	NREG_CTEST6,
NREG_DBC,						NREG_DCMD = 0x27,
NREG_DNAD,
NREG_DSP =	0x2c,
NREG_DSPS =	0x30,
NREG_SCRATCHA =	0x34,
NREG_SCRATCHA0 = 0x34,	NREG_SCRATCHA1,	NREG_SCRATCHA2, NREG_SCRATCHA3,
NREG_DMODE,		NREG_DIEN,	NREG_DWT,	NREG_DCNTL,
NREG_ADDER,

NREG_SIEN0 =	0x40,	NREG_SIEN1,	NREG_SIST0,	NREG_SIST1,
NREG_SLPAR,		NREG_RESERVED,	NREG_MACNTL,	NREG_GPCNTL,
NREG_STIME0,		NREG_STIME1,	NREG_RESPID,
NREG_STEST0 = 0x4c, 	NREG_STEST1,	NREG_STEST2,	NREG_STEST3,
NREG_SIDL,
NREG_SODL = 0x54,
NREG_SBDL = 0x58,
NREG_SCRATCHB = 0x5c,
NREG_SCRATCHB0 = 0x5c,	NREG_SCRATCHB1,	NREG_SCRATCHB2, NREG_SCRATCHB3
};

/* bit definitions for the ISTAT (Interrupt Status) register */
#define	NB_ISTAT_ABRT		0x80	/* abort operation */
#define	NB_ISTAT_SRST		0x40	/* software reset */
#define	NB_ISTAT_SIGP		0x20	/* signal process */

/* bit definitions for the DSTAT (DMA Status) register */
#define	NB_DSTAT_DFE	0x80	/* DMA FIFO empty */
#define	NB_DSTAT_MDPE	0x40	/* master data parity error */
#define	NB_DSTAT_BF	0x20	/* bus fault */
#define	NB_DSTAT_ABRT	0x10	/* aborted */
#define	NB_DSTAT_SSI 	0x08	/* SCRIPT step interrupt */
#define	NB_DSTAT_SIR	0x04	/* SCRIPT interrupt instruction */
#define	NB_DSTAT_RES	0x02	/* reserved */
#define	NB_DSTAT_IID	0x01	/* illegal instruction detected */

/* just the unexpected fatal DSTAT errors */
#define	NB_DSTAT_ERRORS		(NB_DSTAT_MDPE | NB_DSTAT_BF | NB_DSTAT_ABRT \
				| NB_DSTAT_SSI | NB_DSTAT_IID)

/* Bit definitions for the SIST0 (SCSI Interrupt Status Zero) register */
#define	NB_SIST0_MA	0x80	/* initiator: Phase Mismatch, or */
				/* target: ATN/ active */
#define	NB_SIST0_CMP	0x40	/* function complete */
#define	NB_SIST0_SEL	0x20	/* selected */
#define	NB_SIST0_RSL	0x10	/* reselected */
#define	NB_SIST0_SGE	0x08	/* SCSI gross error */
#define	NB_SIST0_UDC	0x04	/* unexpected disconnect */
#define	NB_SIST0_RST	0x02	/* SCSI RST/ (reset) received */
#define	NB_SIST0_PAR	0x01	/* parity error */

/* Bit definitions for the SIST1 (SCSI Interrupt Status One) register */
#define	NB_SIST1_STO	0x04	/* selection or reselection time-out */
#define	NB_SIST1_GEN	0x02	/* general purpose timer expired */
#define	NB_SIST1_HTH	0x01	/* handshake-to-handshake timer expired */

/* just the fatal SIST1 errors */
#define	NB_SIST1_ERRORS		NB_SIST1_STO




/* Miscellaneous other bits that have to be fiddled */
#define	NB_SCNTL0_EPC		0x08	/* enable parity checking */

#define	NB_SCNTL1_CON		0x10	/* connected */
#define	NB_SCNTL1_RST		0x08	/* assert scsi reset signal */

#define	NB_SCID_RRE		0x40	/* enable response to reselection */
#define	NB_SCID_ENC		0x07	/* encoded chip scsi id */

#define	NB_SSID_VAL		0x80	/* scsi id valid bit */
#define	NB_SSID_ENCID		0x07	/* encoded destination scsi id */

#define	NB_SSTAT0_ILF		0x80	/* scsi input data latch full */
#define	NB_SSTAT0_ORF		0x40	/* scsi output data register full */
#define	NB_SSTAT0_OLF		0x20	/* scsi output data latch full */

#define	NB_SSTAT1_FF		0xf0	/* scsi fifo flags */
#define	NB_SSTAT1_PHASE		0x07	/* current scsi phase */

#define	NB_CTEST2_DDIR		0x80	/* data transfer direction */

#define	NB_CTEST3_VMASK		0xf0	/* chip revision level */
#define	NB_CTEST3_VERSION	0x10	/* expected chip revision level */
#define	NB_CTEST3_CLF		0x04	/* clear dma fifo */

#define	NB_CTEST4_MPEE		0x08	/* master parity error enable */

#define	NB_DIEN_MDPE		0x40	/* master data parity error */
#define	NB_DIEN_BF		0x20	/* bus fault */
#define	NB_DIEN_ABRT		0x10	/* aborted */
#define	NB_DIEN_SSI		0x08	/* SCRIPT step interrupt */
#define	NB_DIEN_SIR		0x04	/* SCRIPT interrupt instruction */
#define	NB_DIEN_IID		0x01	/* Illegal instruction detected */

#define	NB_DCNTL_COM		0x01	/* 53c700 compatibility */

#define	NB_SIEN0_MA		0x80	/* Initiator: Phase Mismatch, or */
					/* Target: ATN/ active */
#define	NB_SIEN0_CMP		0x40	/* function complete */
#define	NB_SIEN0_SEL		0x20	/* selected */
#define	NB_SIEN0_RSL		0x10	/* reselected */
#define	NB_SIEN0_SGE		0x08	/* SCSI gross error */
#define	NB_SIEN0_UDC		0x04	/* unexpected disconnect */
#define	NB_SIEN0_RST		0x02	/* SCSI reset condition */
#define	NB_SIEN0_PAR		0x01	/* SCSI parity error */

#define	NB_SIEN1_STO		0x04	/* selection or reselection time-out */
#define	NB_SIEN1_GEN		0x02	/* general purpose timer expired */
#define	NB_SIEN1_HTH		0x01	/* handshake-to-handshake timer */
					/* expired */

#define	NB_STIME0_SEL		0x0f	/* selection time-out bits */
#define	NB_STIME0_409		0x0d	/* 409.6 ms */

#define	NB_STEST3_DSI		0x10	/* disable single initiator response */
#define	NB_STEST3_CSF		0x02	/* clear SCSI FIFO */


/*
 * The following bits are supposed to be setup by the POST BIOS, don't
 * clobber them when resetting the chip
 */
#define	NB_SCNTL1_EXC		0x80	/* extra clock cycle of data setup */

#define	NB_SCNTL3_SCF		0x70	/* synch. clock conversion factor */
#define	NB_SCNTL3_SCF1		0x10	/* SCLK / 1 */
#define	NB_SCNTL3_SCF15		0x20	/* SCLK / 1.5 */
#define	NB_SCNTL3_SCF2		0x30	/* SCLK / 2 */
#define	NB_SCNTL3_SCF3		0x00	/* SCLK / 3 */
#define	NB_SCNTL3_CCF		0x07	/* clock conversion factor */
#define	NB_SCNTL3_CCF1		0x01	/* SCLK / 1 */
#define	NB_SCNTL3_CCF15		0x02	/* SCLK / 1.5 */
#define	NB_SCNTL3_CCF2		0x03	/* SCLK / 2 */
#define	NB_SCNTL3_CCF3		0x00	/* SCLK / 3 */
#define NB_SCNTL3_EWS		0x08	/* Enable Wide SCSI */

#define	NB_CTEST4_BDIS		0x80	/* burst disable */

#define	NB_DMODE_BL		0xc0	/* burst length */

#define	NB_DCNTL_IRQM		0x08	/* IRQ mode */

#define	NB_STEST1_SCLK		0x80	/* disable external SCSI clock */

#define	NB_STEST2_EXT		0x02	/* extend SREQ/SACK filtering */

#define	NB_STEST3_TE		0x80	/* tolerANT enable */
