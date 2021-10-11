/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef _SYS_AUDIO_4231_DMA_H
#define	_SYS_AUDIO_4231_DMA_H

#pragma ident	"@(#)audio_4231_dma.h	1.1	99/05/26 SMI"

/*
 * This file contains platform-specific definitions for the various
 * DMA controllers used to support the CS4231 on SPARC/PPC/x86 platforms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * These are the registers for the APC DMA channel interface to the
 * 4231. One handle provides access the CODEC and the DMA engine's
 * registers.
 */

struct cs4231_apc {
	uint32_t 	dmacsr;		/* APC CSR */
	uint32_t	lpad[3];	/* PAD */
	uint32_t 	dmacva;		/* Captue Virtual Address */
	uint32_t 	dmacc;		/* Capture Count */
	uint32_t 	dmacnva;	/* Capture Next VAddress */
	uint32_t 	dmacnc;		/* Capture next count */
	uint32_t 	dmapva;		/* Playback Virtual Address */
	uint32_t 	dmapc;		/* Playback Count */
	uint32_t 	dmapnva;	/* Playback Next VAddress */
	uint32_t 	dmapnc;		/* Playback Next Count */
};
typedef struct cs4231_apc cs4231_apc_t;

#define	APC_HANDLE	state->cs_handles.cs_codec_hndl
#define	APC_DMACSR	state->cs_regs->apc.dmacsr
#define	APC_DMACVA	state->cs_regs->apc.dmacva
#define	APC_DMACC	state->cs_regs->apc.dmacc
#define	APC_DMACNVA	state->cs_regs->apc.dmacnva
#define	APC_DMACNC	state->cs_regs->apc.dmacnc
#define	APC_DMAPVA	state->cs_regs->apc.dmapva
#define	APC_DMAPC	state->cs_regs->apc.dmapc
#define	APC_DMAPNVA	state->cs_regs->apc.dmapnva
#define	APC_DMAPNC	state->cs_regs->apc.dmapnc

/*
 * APC CSR Register bit definitions
 */

#define	APC_RESET	0x00000001u	/* Reset the DMA engine, R/W */
#define	APC_CDMA_GO	0x00000004u	/* Capture DMA go, R/W */
#define	APC_PDMA_GO	0x00000008u	/* Playback DMA go, R/W */
#define	APC_LOOP_BACK	0x00000010u	/* Loopback, Capture to Play */
#define	APC_COD_PDWN	0x00000020u	/* CODEC power down, R/W */
#define	APC_C_ABORT	0x00000040u	/* Capture abort, R/W */
#define	APC_P_ABORT	0x00000080u	/* Play abort, R/W */
#define	APC_CXI_EN	0x00000100u	/* Capture expired int. enable, R/W */
#define	APC_CXI		0x00000200u	/* Capture expired interrupt, R/W */
#define	APC_CD		0x00000400u	/* Capture next VA dirty, R/O */
#define	APC_CX		0x00000800u	/* Capture expired (pipe empty), R/O */
#define	APC_PMI_EN	0x00001000u	/* Play pipe empty int. enable, R/W */
#define	APC_PD		0x00002000u	/* Playback next VA dirty, R/O */
#define	APC_PM		0x00004000u	/* Play pipe empty, R/O */
#define	APC_PMI		0x00008000u	/* Play pipe empty interrupt, R/W */
#define	APC_EIE		0x00010000u	/* Error interrupt enable, R/W */
#define	APC_CIE		0x00020000u	/* Capture interrupt enable, R/W */
#define	APC_PIE		0x00040000u	/* Playback interrupt enable, R/W */
#define	APC_IE		0x00080000u	/* Interrupt enable, R/W */
#define	APC_EI		0x00100000u	/* Error interrupt, R/W */
#define	APC_CI		0x00200000u	/* Capture interrupt, R/W */
#define	APC_PI		0x00400000u	/* Playback interrupt, R/W */
#define	APC_IP		0x00800000u	/* Interrupt Pending, R/O */
#define	APC_ID		0xff000000u	/* ID bits, set to 7E, R/O */

#define	APC_ID_VALUE	0x7E000000u	/* ID read from CSR */
#define	APC_CLEAR_RESET_VALUE	0x00

#define	APC_PINTR_MASK		(APC_PI|APC_PMI)
#define	APC_CINTR_MASK		(APC_CI|APC_CXI)
#define	APC_COMMON_MASK		(APC_IP|APC_EI)
#define	APC_PINTR_ENABLE	(APC_PIE|APC_PMI_EN)
#define	APC_CINTR_ENABLE	(APC_CIE|APC_CXI_EN)
#define	APC_COMMON_ENABLE	(APC_IE|APC_EIE)

#define	APC_PLAY_ENABLE		(APC_PINTR_MASK|APC_COMMON_MASK|\
			APC_PINTR_ENABLE|APC_COMMON_ENABLE|APC_PDMA_GO)

#define	APC_PLAY_DISABLE	(APC_PINTR_MASK|APC_PINTR_ENABLE|APC_PDMA_GO)

#define	APC_CAP_ENABLE		(APC_CINTR_MASK|APC_COMMON_MASK|\
			APC_CINTR_ENABLE|APC_COMMON_ENABLE|APC_CDMA_GO)

#define	APC_CAP_DISABLE		(APC_CINTR_MASK|APC_CINTR_ENABLE|APC_CDMA_GO)

/*
 * These are the registers for the EBUS2 DMA channel interface to the
 * 4231. One struct per channel for playback and record, therefore there
 * individual handles for the CODEC and the two DMA engines.
 */

struct cs4231_eb2regs {
	uint32_t 	eb2csr;		/* Ebus 2 csr */
	uint32_t 	eb2acr;		/* ebus 2 Addrs */
	uint32_t 	eb2bcr;		/* ebus 2 counts */
};
typedef struct cs4231_eb2regs cs4231_eb2regs_t;

#define	EB2_CODEC_HNDL	state->cs_handles.cs_codec_hndl
#define	EB2_PLAY_HNDL	state->cs_handles.cs_eb2_play_hndl
#define	EB2_REC_HNDL	state->cs_handles.cs_eb2_rec_hndl
#define	EB2_AUXIO_HNDL	state->cs_handles.cs_eb2_auxio_hndl
#define	EB2_PLAY_CSR	state->cs_eb2_regs.play->eb2csr
#define	EB2_PLAY_ACR	state->cs_eb2_regs.play->eb2acr
#define	EB2_PLAY_BCR	state->cs_eb2_regs.play->eb2bcr
#define	EB2_REC_CSR	state->cs_eb2_regs.record->eb2csr
#define	EB2_REC_ACR	state->cs_eb2_regs.record->eb2acr
#define	EB2_REC_BCR	state->cs_eb2_regs.record->eb2bcr
#define	EB2_AUXIO_REG	state->cs_eb2_regs.auxio

/*
 * Audio auxio register definitions
 */
#define	EB2_AUXIO_COD_PDWN	0x00000001u	/* power down Codec */

/*
 * EBUS 2 CSR definitions
 */

#define	EB2_INT_PEND		0x00000001u	/* Interrupt pending, R/O */
#define	EB2_ERR_PEND		0x00000002u	/* Error interrupt, R/O */
#define	EB2_DRAIN		0x00000004u	/* FIFO being drained, R/O */
#define	EB2_INT_EN		0x00000010u	/* Enable interrupts, R/W */
#define	EB2_RESET		0x00000080u	/* Reset DMA engine, R/W */
#define	EB2_WRITE		0x00000100u	/* DMA direction (to mem) R/W */
#define	EB2_READ		0x00000000u	/* DMA direction (to dev) R/W */
#define	EB2_EN_DMA		0x00000200u	/* Enable DMA, R/W */
#define	EB2_CYC_PENDING		0x00000400u	/* DMA cycle pending, R/O */
#define	EB2_DIAG_RD_DONE	0x00000800u	/* Diag RD done, R/O */
#define	EB2_DIAG_WR_DONE	0x00001000u	/* Diag WR done, R/O */
#define	EB2_EN_CNT		0x00002000u	/* Enable byte count, R/W */
#define	EB2_TC			0x00004000u	/* Terminal count, R/W */
#define	EB2_DIS_CSR_DRN		0x00010000u	/* Dis. drain with W-CSR, R/W */
#define	EB2_16			0x00000000u 	/* 19,18 == 0,0, R/W */
#define	EB2_32			0x00040000u	/* 19,18 == 0,1, R/W */
#define	EB2_4			0x00080000u	/* 19,18 == 1,0, R/W */
#define	EB2_64			0x000C0000u	/* 19,18 == 1,1, R/W */
#define	EB2_DIAG_EN		0x00100000u	/* DMA diag. enable, R/W */
#define	EB2_DIS_ERR_PEND	0x00400000u	/* Disable Error int., R/W */
#define	EB2_TCI_DIS		0x00800000u	/* Disable TC int., R/W */
#define	EB2_EN_NEXT		0x01000000u	/* Next addr. enabled, R/W */
#define	EB2_DMA_ON		0x02000000u	/* DMA engine enabled, R/O */
#define	EB2_A_LOADED		0x04000000u	/* Address loaded, R/O */
#define	EB2_NA_LOADED		0x08000000u	/* Next add. loaded, R/O */
#define	EB2_DEV_ID		0xf0000000u	/* Device ID -0x0C, R/O */

#define	EB2_ID_VALUE		0xC0000000u	/* ID read from CSR */
#define	EB2_PCLEAR_RESET_VALUE	(EB2_READ|EB2_EN_NEXT)
#define	EB2_RCLEAR_RESET_VALUE	(EB2_WRITE|EB2_EN_NEXT)

#define	EB2_PINTR_MASK		(EB2_INT_EN)
#define	EB2_RINTR_MASK		(EB2_INT_EN)

#define	EB2_PLAY_ENABLE		(EB2_PINTR_MASK|EB2_EN_DMA|EB2_EN_CNT|EB2_64|\
					EB2_PCLEAR_RESET_VALUE)

#define	EB2_REC_ENABLE		(EB2_RINTR_MASK|EB2_EN_DMA|EB2_EN_CNT|EB2_64|\
					EB2_RCLEAR_RESET_VALUE)

#define	EB2_FIFO_DRAIN		(EB2_DRAIN|EB2_CYC_PENDING)

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUDIO_4231_DMA_H */
