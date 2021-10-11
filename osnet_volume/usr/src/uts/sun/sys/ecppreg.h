/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_ECPPREG_H
#define	_SYS_ECPPREG_H

#pragma ident	"@(#)ecppreg.h	2.8	98/04/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Register definitions for the National Semiconductor PC87332VLJ
 * SuperI/O chip.
 */

/*
 * configuration registers
 */
struct config_reg {
	uint8_t index;
	uint8_t data;
};

/* index values for the configuration registers */
#define	FER	0x0	/* Function Enable Register */
#define	FAR	0x1	/* Function Address Register */
#define	PTR	0x2	/* Power and Test Register */
#define	FCR	0x3	/* Function Control Register */
#define	PCR	0x4	/* Printer Control Register */
#define	KRR	0x5	/* Keyboard and RTC control Register */
#define	PMC	0x6	/* Power Management Control register */
#define	TUP	0x7	/* Tape, UART, and Parallel port register */
#define	SID	0x8	/* Super I/O Identification register */

#define	SIO_LITE	0x40
#define	SIO_LITE_B	0x90
#define	SIO_REVA	0x1a
#define	SIO_REVB	0x1b

/* bit definitions for the FER register */
/* bit definitions for the FAR register */
/* bit definitions for the PTR register */
/* bit definitions for the FCR register */
/* bit definitions for the PCR register */
/* bit definitions for the PMC register */
/* bit definitions for the TUP register */
/* bit definitions for the SID register */

/*
 * parallel port interface registers - same for all 1284 modes.
 */
struct info_reg {
	union {
		uint8_t	datar;
		uint8_t	afifo;
	} ir;
	uint8_t dsr;
	uint8_t dcr;
};

/*
 * additional ECP mode registers.
 */
struct fifo_reg {
	union {
		uint8_t cfifo;
		uint8_t dfifo;
		uint8_t tfifo;
		uint8_t config_a;
	} fr;
	uint8_t config_b;
	uint8_t ecr;
};

/*
 * Values for the ECR field
 *
 * The ECR has 3 read-only bits - bits 0,1,2.  Bits 3,4,5,6,7 are read/write.
 * While writing to this register (ECPPIOC_SETREGS), bits 0,1,2 must be 0.
 * If not, ECPPIOC_SETREGS will return EINVAL.
 */

#define	ECPP_FIFO_EMPTY		0x01	/* 1 when FIFO empty */
#define	ECPP_FIFO_FULL		0x02	/* 1 when FIFO full  */
#define	ECPP_INTR_SRV		0x04

/*
 * When bit is 0, bit will be set to 1
 * and interrupt will be generated if
 * any of the three events occur:
 * (a) TC is reached while DMA enabled
 * (b) If DMA disabled & DCR5 = 0, 8 or more bytes free in FIFO,
 * (c) IF DMA disable & DCR5 = 1, 8 or more bytes to be read in FIFO.
 *
 * When this bit is 1, DMA & (a), (b), (c)
 * interrupts are disabled.
 */

#define	ECPP_DMA_ENABLE		0x08  /* DMA enable =1 */
#define	ECPP_INTR_MASK		0x10  /* intr-enable nErr mask=1 */
#define	ECR_mode_000		0x00  /* PIO CENTRONICS */
#define	ECR_mode_001		0x20  /* PIO NIBBLE */
#define	ECR_mode_010		0x40  /* DMA CENTRONICS */
#define	ECR_mode_011		0x60  /* DMA ECP */
#define	ECR_mode_110		0xc0  /* TDMA (TFIFO) */
#define	ECR_mode_111		0xe0  /* Config Mode */

/* Cheerio Ebus DMAC */

struct cheerio_dma_reg {
	uint32_t csr;	/* Data Control Status Register */
	uint32_t acr;	/* DMA Address Count Registers */
	uint32_t bcr;	/* DMA Byte Count Register */
};

/*
 * DMA Control and Status Register(DCSR) definitions.  See Cheerio spec
 * for more details
 */
#define	DCSR_INT_PEND 	0x00000001	/* 1= pport or dma interrupts */
#define	DCSR_ERR_PEND 	0x00000002	/* 1= host bus error detected */
#define	DCSR_INT_EN 	0x00000010	/* 1= enable sidewinder/ebus intr */
#define	DCSR_RESET  	0x00000080	/* 1= resets the DCSR */
#define	DCSR_WRITE  	0x00000100  	/* DMA direction; 1 = memory */
#define	DCSR_EN_DMA  	0x00000200  	/* 1= enable DMA */
#define	DCSR_CYC_PEND	0x00000400	/* 1 = DMA pending */
#define	DCSR_EN_CNT 	0x00002000	/* 1= enables byte counter */
#define	DCSR_TC		0x00004000  	/* 1= Terminal Count occurred */
#define	DCSR_CSR_DRAIN 	0x00000000 	/* 1= disable draining */
#define	DCSR_BURST_0    0x00040000 	/* Burst Size bit 0 */
#define	DCSR_BURST_1    0x00080000 	/* Burst Size bit 1 */
#define	DCSR_DIAG	0x00000000 	/* 1= diag enable */
#define	DCSR_TCI_DIS 	0x00800000	/* 1= TC won't cause interrupt */

#define	set_dmac_csr(pp, val)	ddi_put32(pp->d_handle, \
				((uint32_t *)&pp->dmac->csr), \
				((uint32_t)val))
#define	get_dmac_csr(pp)	ddi_get32(pp->d_handle, \
				(uint32_t *)&(pp->dmac->csr))

#define	set_dmac_acr(pp, val)	ddi_put32(pp->d_handle, \
				((uint32_t *)&pp->dmac->acr), \
				((uint32_t)val))

#define	get_dmac_acr(pp)	ddi_get32(pp->d_handle, \
				(uint32_t *)&pp->dmac->acr)

#define	set_dmac_bcr(pp, val)	ddi_put32(pp->d_handle, \
				((uint32_t *)&pp->dmac->bcr), \
				((uint32_t)val))

#define	get_dmac_bcr(pp)	ddi_get32(pp->d_handle, \
				((uint32_t *)&pp->dmac->bcr))


/*
 * Reset the DCSR by first setting the RESET bit to 1.  Poll the
 * DCSR_CYC_PEND bit to make sure there are no more pending DMA cycles.
 * If there are no more pending cycles, clear the RESET bit.
 */
#define	RESET_DMAC_CSR(pp) \
	{ \
		set_dmac_csr(pp, DCSR_RESET); \
		while (get_dmac_csr(pp) & DCSR_CYC_PEND); \
		set_dmac_csr(pp, 0); \
	}

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ECPPREG_H */
