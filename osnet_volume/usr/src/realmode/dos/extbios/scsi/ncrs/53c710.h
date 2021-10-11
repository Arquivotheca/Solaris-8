/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)53c710.h	1.4	97/07/21 SMI"
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
 * This file was derived from the 53c710.h file in the Solaris
 * driver.
 */


enum ncr53c710regs {		/* To access NCR 53C710 registers */
NREG_SCNTL0 =	0x00,	NREG_SCNTL1,	NREG_SDID,	NREG_SIEN,
NREG_SCID,		NREG_SXFER,	NREG_SODL,	NREG_SOCL,
NREG_SFBR,		NREG_SIDL,	NREG_SBDL,	NREG_SBCL,
NREG_DSTAT,		NREG_SSTAT0,	NREG_SSTAT1,	NREG_SSTAT2,
NREG_DSA,
NREG_CTEST0 =	0x14,	NREG_CTEST1,	NREG_CTEST2,	NREG_CTEST3,
NREG_CTEST4,		NREG_CTEST5,	NREG_CTEST6,	NREG_CTEST7,
NREG_TEMP,
NREG_DFIFO =	0x20,	NREG_ISTAT,	NREG_CTEST8,	NREG_LCRC,
NREG_DBC,						NREG_DCMD = 0x27,
NREG_DNAD,
NREG_DSP =	0x2c,
NREG_DSPS =	0x30,
NREG_SCRATCH =	0x34,
NREG_SCRATCH0 =	0x34,	NREG_SCRATCH1,	NREG_SCRATCH2, NREG_SCRATCH3,
NREG_DMODE,		NREG_DIEN,	NREG_DWT,	NREG_DCNTL,
NREG_ADDER
};

/* bit definitions for the DSTAT (DMA Status) register */
#define	NB_DSTAT_DFE	0x80	/* DMA FIFO empty */
#define	NB_DSTAT_RES	0x40	/* reserved */
#define	NB_DSTAT_BF	0x20	/* bus fault */
#define	NB_DSTAT_ABRT	0x10	/* Aborted */
#define	NB_DSTAT_SSI 	0x08	/* SCRIPT step interrupt */
#define	NB_DSTAT_SIR	0x04	/* SCRIPT interrupt instruction */
#define	NB_DSTAT_WTD	0x02	/* Watchdog timeout detected */
#define	NB_DSTAT_IID	0x01	/* Illegal instruction detected */

/* just the fatal DSTAT errors */
#define	NB_DSTAT_ERRORS		(NB_DSTAT_BF | NB_DSTAT_ABRT | NB_DSTAT_SSI \
				| NB_DSTAT_WTD | NB_DSTAT_IID)

/* Bit definitions for the SSTAT0 (SCSI Status Zero) register */
#define	NB_SSTAT0_MA	0x80	/* Initiator: Phase Mismatch, or */
				/* Target: ATN/ active */
#define	NB_SSTAT0_FCMP	0x40	/* Function complete */
#define	NB_SSTAT0_STO	0x20	/* SCSI bus timeout */
#define	NB_SSTAT0_SEL	0x10	/* Selected or reselected */
#define	NB_SSTAT0_SGE	0x08	/* SCSI gross error */
#define	NB_SSTAT0_UDC	0x04	/* Unexpected disconnect */
#define	NB_SSTAT0_RST	0x02	/* SCSI RST/ (reset) received */
#define	NB_SSTAT0_PAR	0x01	/* Parity error */




#define	NB_CTEST0_BTD		0x40	/* byte-to-byte timer disable */
#define	NB_CTEST0_GRP		0x20	/* generate rcv parity for passthru */
#define	NB_CTEST0_DDIR		0x01	/* data transfer direction */

#define	NB_CTEST8_CLF		0x04	/* clear dma and scsi fifos */
#define	NB_CTEST8_VMASK		0xf0	/* chip revision level */
#define	NB_CTEST8_VERSION	0x20	/* expected chip revision level */

#define	NB_DCNTL_CF		0xc0	/* clock frequency */
#define	NB_DCNTL_CF2		0x00	/* clock freq = sclk / 2 */
#define	NB_DCNTL_CF15		0x40	/* clock freq = sclk / 1.5 */
#define	NB_DCNTL_CF1		0x80	/* clock freq = sclk / 1 */
#define	NB_DCNTL_CF3		0xc0	/* clock freq = sclk / 3 */
#define	NB_DCNTL_COM		0x01	/* 53c700 compatibility */

#define	NB_SBCL_SSCF0		0x00	/* set by dcntl */
#define	NB_SBCL_SSCF1		0x01	/* clock freq = sclk / 1 */
#define	NB_SBCL_SSCF15		0x02	/* clock freq = sclk / 1.5 */
#define	NB_SBCL_SSCF2		0x03	/* clock freq = sclk / 2 */

#define	NB_DIEN_RES7		0x80	/* reserved bit */
#define	NB_DIEN_RES6		0x40	/* reserved bit */
#define	NB_DIEN_SSI		0x08	/* script step interrupt */

#define	NB_ISTAT_ABRT		0x80	/* abort operation */
#define	NB_ISTAT_RST		0x40	/* software reset */
#define	NB_ISTAT_SIGP		0x20	/* signal process */

#define	NB_SCNTL0_EPC		0x08	/* enable parity checking */
#define	NB_SCNTL0_EPG		0x04	/* enable parity generation */

#define	NB_SCNTL1_ESR		0x20	/* enable selection & reselection */
#define	NB_SCNTL1_CON		0x10	/* connected */
#define	NB_SCNTL1_RST		0x08	/* assert scsi reset signal */

#define	NB_SIEN_FCMP		0x40	/* function complete */
#define	NB_SIEN_SEL		0x10	/* selected or reselected */

#define	NB_SSTAT1_ILF		0x80	/* scsi input data latch full */
#define	NB_SSTAT1_ORF		0x40	/* scsi output data register full */
#define	NB_SSTAT1_OLF		0x20	/* scsi output data latch full */

#define	NB_SSTAT2_PHASE		0x07	/* current scsi phase */


/*
 * The following bits are supposed to be setup by the POST BIOS, don't
 * clobber them when resetting the chip
 */
#define	NB_SCNTL1_EXC		0x80	/* extra clock cycle of data setup */

#define	NB_CTEST0_EAN		0x10	/* enable active negation */
#define	NB_CTEST0_ERF		0x02	/* extend req/ack filtering */

#define	NB_CTEST4_MUX		0x80	/* host bus multiplex mode */

#define	NB_CTEST7_CDIS		0x80	/* cache burst disable */
#define	NB_CTEST7_SC		0x60	/* snoop control */
#define	NB_CTEST7_DFP		0x08	/* dma fifo parity */
#define	NB_CTEST7_EVP		0x04	/* even parity */
#define	NB_CTEST7_TT1		0x02	/* transfer type bit */
#define	NB_CTEST7_DIFF		0x01	/* differential mode */

#define	NB_CTEST8_FM		0x02	/* fetch pin mode */
#define	NB_CTEST8_SM		0x01	/* snoop pin modes */

#define	NB_DMODE_BL		0xc0	/* burst length */
#define	NB_DMODE_FC		0x30	/* function code */
#define	NB_DMODE_PD		0x08	/* program/data */
#define	NB_DMODE_FAM		0x04	/* fixed address mode */
#define	NB_DMODE_U0		0x02	/* user programmable transfer type */

#define	NB_DCNTL_CF		0xc0	/* clock frequency */
#define	NB_DCNTL_EA		0x20	/* enable ack */
#define	NB_DCNTL_FA		0x02	/* fast arbitration */
