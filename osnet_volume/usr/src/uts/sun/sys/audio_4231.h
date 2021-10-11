/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_AUDIO_4231_H
#define	_SYS_AUDIO_4231_H

#pragma ident	"@(#)audio_4231.h	1.6	99/12/09 SMI"

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/audio_4231_dma.h>

/*
 * This file describes the Crystal 4231 CODEC chip and declares
 * parameters and data structures used by the audio driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver constants
 */
#define	CS4231_IDNUM		(0x6175)
#define	CS4231_NAME		"audiocs"
#define	CS4231_MINPACKET	(0)
#define	CS4231_MAXPACKET	(16*1024)
#define	CS4231_HIWATER		(AM_MAX_QUEUED_MSGS_SIZE)
#define	CS4231_LOWATER		(AM_MIN_QUEUED_MSGS_SIZE)
#define	CS4231_MOD_NAME		"CS4231 audio driver (mixer version)"
#define	CS4231_SUP_INST		(1)

/*
 * Values returned by the AUDIO_GETDEV ioctl()
 */
#define	CS_DEV_NAME		"SUNW,CS4231"
#define	CS_DEV_CONFIG_ONBRD1	"onboard1"
#define	CS_DEV_VERSION		"a"	/* SS5 				*/
#define	CS_DEV_VERSION_A	CS_DEV_VERSION
#define	CS_DEV_VERSION_B	"b"	/* Electron - internal loopback	*/
#define	CS_DEV_VERSION_C	"c"	/* Positron			*/
#define	CS_DEV_VERSION_D	"d"	/* PowerPC - Retired		*/
#define	CS_DEV_VERSION_E	"e"	/* x86 - Retired		*/
#define	CS_DEV_VERSION_F	"f"	/* Tazmo			*/
#define	CS_DEV_VERSION_G	"g"	/* Quark Audio Module		*/
#define	CS_DEV_VERSION_H	"h"	/* Darwin			*/

/*
 * Driver supported configuration information
 */
#define	CS4231_SAMPR5510	(5510)
#define	CS4231_SAMPR6620	(6620)
#define	CS4231_SAMPR8000	(8000)
#define	CS4231_SAMPR9600	(9600)
#define	CS4231_SAMPR11025	(11025)
#define	CS4231_SAMPR16000	(16000)
#define	CS4231_SAMPR18900	(18900)
#define	CS4231_SAMPR22050	(22050)
#define	CS4231_SAMPR27420	(27420)
#define	CS4231_SAMPR32000	(32000)
#define	CS4231_SAMPR33075	(33075)
#define	CS4231_SAMPR37800	(37800)
#define	CS4231_SAMPR44100	(44100)
#define	CS4231_SAMPR48000	(48000)

#define	CS4231_DEFAULT_SR	CS4231_SAMPR8000
#define	CS4231_DEFAULT_CH	AUDIO_CHANNELS_MONO
#define	CS4231_DEFAULT_PREC	AUDIO_PRECISION_8
#define	CS4231_DEFAULT_ENC	AUDIO_ENCODING_ULAW
#define	CS4231_DEFAULT_PGAIN	(127)
#define	CS4231_DEFAULT_RGAIN	(127)
#define	CS4231_DEFAULT_MONITOR_GAIN	(0)
#define	CS4231_DEFAULT_BAL	AUDIO_MID_BALANCE
#define	CS4231_BSIZE		(8*1024)

#ifdef _KERNEL

/*
 * Misc. defines
 */
#define	CS4231_MAX_CHANNELS		(200)		/* force max # chs */
#define	CS4231_REGS			(32)
#define	CS4231_NCOMPONENTS		(1)
#define	CS4231_COMPONENT		(0)
#define	CS4231_PWR_OFF			(0)
#define	CS4231_PWR_ON			(1)
#define	CS4231_TIMEOUT			(100000)
#define	CS4231_INTS			(50)
#define	CS4231_MIN_INTS			(10)
#define	CS4231_MAX_INTS			(2000)

/*
 * Supported dma engines and the ops vector
 */
enum cs_dmae_types {APC_DMA, EB2_DMA};
typedef enum cs_dmae_types cs_dmae_types_e;

/*
 * Hardware registers
 */
struct cs4231_pioregs {
	uint8_t iar;		/* index address register */
	uint8_t pad1[3];		/* pad */
	uint8_t idr;		/* indexed data register */
	uint8_t pad2[3];		/* pad */
	uint8_t statr;		/* status register */
	uint8_t pad3[3];		/* pad */
	uint8_t piodr;		/* PIO data regsiter */
	uint8_t pad4[3];
};
typedef struct cs4231_pioregs cs4231_pioregs_t;

struct cs4231_eb2 {
	cs4231_eb2regs_t	*play;		/* play EB2 registers */
	cs4231_eb2regs_t	*record;	/* record EB2 registers */
	uint_t			auxio;		/* aux io - power down */
};
typedef struct cs4231_eb2 cs4231_eb2_t;

struct cs4231_regs {
	cs4231_pioregs_t	codec;		/* CS4231 CODEC registers */
	cs4231_apc_t		apc;		/* gets mapped with CODEC */
};
typedef struct cs4231_regs cs4231_regs_t;

#define	CS4231_IAR	state->cs_regs->codec.iar	/* Index Add. Reg. */
#define	CS4231_IDR	state->cs_regs->codec.idr	/* Index Data Reg. */
#define	CS4231_STATUS	state->cs_regs->codec.statr	/* Status Reg. */
#define	CS4231_PIODR	state->cs_regs->codec.piodr	/* PIO Data Reg. */

/*
 * Misc. state enumerations and structures
 */
enum cs_cd {NO_INTERNAL_CD, INTERNAL_CD_ON_AUX1, INTERNAL_CD_ON_AUX2};
typedef enum cs_cd cs_cd_e;

struct cs4231_handle {
	ddi_acc_handle_t	cs_codec_hndl;	/* CODEC handle, APC & EB2 */
	ddi_acc_handle_t	cs_eb2_play_hndl; /* EB2 only, play handle */
	ddi_acc_handle_t	cs_eb2_rec_hndl; /* EB2 only, record handle */
	ddi_acc_handle_t	cs_eb2_auxio_hndl; /* EB2 only, auxio handle */
};
typedef struct cs4231_handle cs4231_handle_t;

/*
 * CS_state_t - per instance state and operation data
 */
struct CS_state {
	kmutex_t		cs_lock;	/* state protection lock */
	kmutex_t		cs_swlock;	/* mixer apm software lock */
	uint_t			cs_flags;	/* flags */
	kstat_t			*cs_ksp;	/* kernel statistics */
	dev_info_t		*cs_dip;	/* used by cs4231_getinfo() */
	audio_info_t		cs_defaults;	/* deffault state for the dev */
	am_ad_info_t		cs_ad_info;	/* audio device info state */
	cs_dmae_types_e		cs_dma_engine;	/* dma engine for this h/w */
	struct cs4231_dma_ops	*cs_dma_ops;	/* dma engine ops vector */
	cs4231_regs_t		*cs_regs;	/* hardware registers */
	cs4231_eb2_t		cs_eb2_regs;	/* eb2 DMA registers */
	cs4231_handle_t		cs_handles;	/* hardware handles */
	audio_device_t		cs_dev_info;	/* device info strings */
	int			cs_suspended;	/* power management state */
	int			cs_powered;	/* device powered up? */
	int			cs_busy_cnt;	/* device busy count */
	int			cs_max_chs;	/* max channels to allocate */
	int			cs_max_p_chs;	/* max play chs to allocate */
	int			cs_max_r_chs;	/* max rec. chs to allocate */
	int			cs_chs;		/* channels allocated */
	int			cs_p_chs;	/* play chs allocated */
	int			cs_r_chs;	/* rec. chs allocated */
	ddi_dma_attr_t		*cs_dma_attr;	/* dma attributes */
	ddi_dma_handle_t	cs_ph[2];	/* play DMA handles */
	ddi_dma_handle_t	cs_ch[2];	/* capture DMA handles */
	ddi_dma_cookie_t	cs_pc[2];	/* play DMA cookies */
	ddi_dma_cookie_t	cs_cc[2];	/* capture DMA cookies */
	ddi_acc_handle_t	cs_pmh[2];	/* play DMA memory handles */
	ddi_acc_handle_t	cs_cmh[2];	/* capture DMA memory handles */
	size_t			cs_pml[2];	/* play DMA memory length */
	size_t			cs_cml[2];	/* capture DMA memory length */
	caddr_t			cs_pb[2];	/* play DMA buffers */
	caddr_t			cs_cb[2];	/* capture DMA buffers */
	int			cs_pcnt[2];	/* play count, in samples */
	int			cs_ccnt[2];	/* capture count, in bytes */
	int			cs_pbuf_toggle;	/* play DMA buffer toggle */
	int			cs_cbuf_toggle;	/* capture DMA buffer toggle */
	uint_t			play_sr;	/* play sample rate */
	uint_t			play_ch;	/* play channels */
	uint_t			play_prec;	/* play precision */
	uint_t			play_enc;	/* play encoding */
	uint_t			record_sr;	/* record sample rate */
	uint_t			record_ch;	/* record channels */
	uint_t			record_prec;	/* record precision */
	uint_t			record_enc;	/* record encoding */
	uint_t			output_muted;	/* output muted */
	cs_cd_e			cs_cd_input_line; /* AUX1, AUX2 or none */
	boolean_t		cs_revA;	/* B_TRUE if Rev A CODEC */
	boolean_t		cs_autocal;	/* auto calibrate if B_TRUE */
	uint8_t			cs_save[CS4231_REGS];	/* PM reg. storage */
};
typedef struct CS_state CS_state_t;

/* CS_state.flags defines */
#define	PDMA_ENGINE_INITALIZED	0x0001u	/* play DMA engine initialized */

#define	PLAY_DMA_HANDLE		state->cs_ph[state->cs_pbuf_toggle]
#define	PLAY_DMA_COOKIE		state->cs_pc[state->cs_pbuf_toggle]
#define	PLAY_DMA_BUF		state->cs_pb[state->cs_pbuf_toggle]
#define	PLAY_DMA_MEM_HANDLE	state->cs_pmh[state->cs_pbuf_toggle]
#define	PLAY_DMA_MEM_LENGTH	state->cs_pml[state->cs_pbuf_toggle]
#define	PLAY_COUNT		state->cs_pcnt[state->cs_pbuf_toggle]
#define	CAP_DMA_HANDLE		state->cs_ch[state->cs_cbuf_toggle]
#define	CAP_DMA_COOKIE		state->cs_cc[state->cs_cbuf_toggle]
#define	CAP_DMA_BUF		state->cs_cb[state->cs_cbuf_toggle]
#define	CAP_DMA_MEM_HANDLE	state->cs_cmh[state->cs_cbuf_toggle]
#define	CAP_DMA_MEM_LENGTH	state->cs_cml[state->cs_cbuf_toggle]
#define	CAP_COUNT		state->cs_ccnt[state->cs_cbuf_toggle]
#define	KIOP(X)			((kstat_intr_t *)(X->cs_ksp->ks_data))

/*
 * DMA ops vector definition
 */
struct cs4231_dma_ops {
	char	*dma_device;
	int	(*cs_dma_map_regs)(dev_info_t *, CS_state_t *, size_t, size_t);
	void	(*cs_dma_unmap_regs)(CS_state_t *);
	void	(*cs_dma_reset)(CS_state_t *);
	int	(*cs_dma_add_intr)(CS_state_t *);
	void	(*cs_dma_rem_intr)(dev_info_t *);
	int 	(*cs_dma_p_start)(CS_state_t *);
	void	(*cs_dma_p_pause)(CS_state_t *);
	void	(*cs_dma_p_restart)(CS_state_t *);
	void	(*cs_dma_p_stop)(CS_state_t *);
	int	(*cs_dma_r_start)(CS_state_t *);
	void	(*cs_dma_r_stop)(CS_state_t *);
	void	(*cs_dma_power)(CS_state_t *, int);
};
typedef struct cs4231_dma_ops cs4231_dma_ops_t;

extern cs4231_dma_ops_t cs4231_apcdma_ops;
extern cs4231_dma_ops_t cs4231_eb2dma_ops;

#define	CS4231_DMA_MAP_REGS(DIP, S, P, C)	\
				((S)->cs_dma_ops->cs_dma_map_regs)(DIP, S, P, C)
#define	CS4231_DMA_UNMAP_REGS(S)	((S)->cs_dma_ops->cs_dma_unmap_regs)(S)
#define	CS4231_DMA_RESET(S)		((S)->cs_dma_ops->cs_dma_reset)(S)
#define	CS4231_DMA_ADD_INTR(S)		((S)->cs_dma_ops->cs_dma_add_intr)(S)
#define	CS4231_DMA_REM_INTR(DIP, S)	((S)->cs_dma_ops->cs_dma_rem_intr)(DIP)
#define	CS4231_DMA_START_PLAY(S)	((S)->cs_dma_ops->cs_dma_p_start)(S)
#define	CS4231_DMA_PAUSE_PLAY(S)	((S)->cs_dma_ops->cs_dma_p_pause)(S)
#define	CS4231_DMA_RESTART_PLAY(S)	((S)->cs_dma_ops->cs_dma_p_restart)(S)
#define	CS4231_DMA_STOP_PLAY(S)		((S)->cs_dma_ops->cs_dma_p_stop)(S)
#define	CS4231_DMA_START_RECORD(S)	((S)->cs_dma_ops->cs_dma_r_start)(S)
#define	CS4231_DMA_STOP_RECORD(S)	((S)->cs_dma_ops->cs_dma_r_stop)(S)
#define	CS4231_DMA_POWER(S, L)		((S)->cs_dma_ops->cs_dma_power)(S, L)

/*
 * Useful bit twiddlers
 */
#define	REG_SELECT(handle, addr, reg) {					\
	int		__x;						\
	uint8_t		__T;						\
	for (__x = 0; __x < CS4231_RETRIES; __x++) {			\
		ddi_put8((handle), (uint8_t *)(addr), (reg));		\
		__T = ddi_get8((handle), (uint8_t *)(addr));		\
		if (__T == reg) break;					\
		drv_usecwait(1000);					\
	}								\
	if (__x == CS4231_RETRIES) {					\
		cmn_err(CE_NOTE, "audiocs: couldn't set register "	\
			"(%d 0x%02x 0x%02x)", __LINE__, __T, reg);	\
		cmn_err(CE_CONT, "audio may not work correctly until "	\
			"it is stopped and restarted");			\
	}								\
}

#define	CS4231_RETRIES		10

#define	DDI_PUT8(handle, addr, val) {					\
	int		__x;						\
	uint8_t		__T;						\
	for (__x = 0; __x < CS4231_RETRIES; __x++) {			\
		ddi_put8((handle), (uint8_t *)(addr), (val));		\
		__T = ddi_get8((handle), (uint8_t *)(addr));		\
		if (__T == val) break;					\
		drv_usecwait(1000);					\
	}								\
	if (__x == CS4231_RETRIES) {					\
		cmn_err(CE_NOTE, "audiocs: couldn't set value "		\
			"(%d 0x%02x 0x%02x)", __LINE__, __T, val);	\
		cmn_err(CE_CONT, "audio may not work correctly until "	\
			"it is stopped and restarted");			\
	}								\
}

#define	OR_SET_BYTE(handle, addr, val)					\
	DDI_PUT8((handle), (uint8_t *)(addr),				\
		(ddi_get8((handle), (uint8_t *)(addr)) | (uint8_t)(val)));

#define	OR_SET_WORD(handle, addr, val)					\
	ddi_put32((handle), (uint_t *)(addr),				\
		(ddi_get32((handle), (uint_t *)(addr)) | (uint_t)(val)));

#define	AND_SET_BYTE(handle, addr, val)					\
	DDI_PUT8((handle), (uint8_t *)(addr),				\
		(ddi_get8((handle), (uint8_t *)(addr)) & (uint8_t)(val)));

#define	AND_SET_WORD(handle, addr, val)					\
	ddi_put32((handle), (uint_t *)(addr),				\
		(ddi_get32((handle), (uint_t *)(addr)) & (uint_t)(val)));

/*
 * CS4231 Register Set Definitions
 */
/* Index Address Register */
#define	IAR_ADDRESS_MASK	0x1f	/* mask for index addresses, R/W */
#define	IAR_TRD			0x20	/* Transfer Request Disable, R/W */
#define	IAR_MCE			0x40	/* Mode Change Enable, R/W */
#define	IAR_INIT		0x80	/* 4231 init cycle, R/O */

/* Status Register */
#define	STATUS_INT		0x01	/* Interrupt status, R/O */
#define	STATUS_PRDY		0x02	/* Playback Data Ready */
#define	STATUS_PLR		0x04	/* Playback Left/Right sample */
#define	STATUS_PUL		0x08	/* Playback Upper/Lower byte */
#define	STATUS_SER		0x10	/* Sample Error, see Index 24 */
#define	STATUS_CRDY		0x20	/* Capture Data Ready */
#define	STATUS_CLR		0x40	/* Capture Left/Right sample */
#define	STATUS_CUL		0x80	/* Capture Upper/Lower byte */
#define	STATUS_RESET		0x00	/* Reset the status register */

/* Index 00 - Left ADC Input Control, Modes 1&2 */
#define	LADCI_REG		0x00	/* Left ADC Register */
#define	LADCI_GAIN_MASK		0x0f	/* Left gain mask, 1.5 dB/step */
#define	LADCI_LMGE		0x20	/* Left Mic Gain Enable, 20 dB stage */
#define	LADCI_LLINE		0x00	/* Left Line in enable */
#define	LADCI_LAUX1		0x40	/* Left AUX1 in enable */
#define	LADCI_LMIC		0x80	/* Left MIC in enable */
#define	LADCI_LLOOP		0xc0	/* Left Loopback enable */
#define	LADCI_IN_MASK		0xc0	/* Left input mask */

/* Index 01 - Right ADC Input Control, Modes 1&2 */
#define	RADCI_REG		0x01	/* Right ADC Register */
#define	RADCI_GAIN_MASK		0x0f	/* Right gain mask, 1.5 dB/step */
#define	RADCI_RMGE		0x20	/* Right Mic Gain Enable, 20 dB stage */
#define	RADCI_RLINE		0x00	/* Right Line in enable */
#define	RADCI_RAUX1		0x40	/* Right AUX1 in enable */
#define	RADCI_RMIC		0x80	/* Right MIC in enable */
#define	RADCI_RLOOP		0xc0	/* Right Loopback enable */
#define	RADCI_IN_MASK		0xc0	/* Right input mask */

/* Index 02 - Left Aux #1 Input Control, Modes 1&2 */
#define	LAUX1_REG		0x02	/* Left Aux#1 Register */
#define	LAUX1_GAIN_MASK		0x1f	/* Left Aux#1 gain mask, 1.5 dB/step */
#define	LAUX1_LX1M		0x80	/* Left Aux#1 mute */
#define	LAUX1_UNITY_GAIN	0x08	/* Left Aux#1 unity gain */

/* Index 03 - Right Aux #1 Input Control, Modes 1&2 */
#define	RAUX1_REG		0x03	/* Right Aux#1 Register */
#define	RAUX1_GAIN_MASK		0x1f	/* Right Aux#1 gain mask, 1.5 dB/step */
#define	RAUX1_RX1M		0x80	/* Right Aux#1 mute */
#define	RAUX1_UNITY_GAIN	0x08	/* Right Aux#1 unity gain */

/* Index 04 - Left Aux #2 Input Control, Modes 1&2 */
#define	LAUX2_REG		0x04	/* Left Aux#2 Register */
#define	LAUX2_GAIN_MASK		0x1f	/* Left Aux#2 gain mask, 1.5 dB/step */
#define	LAUX2_LX2M		0x80	/* Left Aux#2 mute */
#define	LAUX2_UNITY_GAIN	0x08	/* Left Aux#2 unity gain */

/* Index 05 - Right Aux #2 Input Control, Modes 1&2 */
#define	RAUX2_REG		0x05	/* Right Aux#2 Register */
#define	RAUX2_GAIN_MASK		0x1f	/* Right Aux#2 gain mask, 1.5 dB/step */
#define	RAUX2_RX2M		0x80	/* Right Aux#2 mute */
#define	RAUX2_UNITY_GAIN	0x08	/* Right Aux#2 unity gain */

/* Index 06 - Left DAC Output Control, Modes 1&2 */
#define	LDACO_REG		0x06	/* Left DAC Register */
#define	LDACO_ATTEN_MASK	0x3f	/* Left attenuation mask, 1.5 dB/setp */
#define	LDACO_LDM		0x80	/* Left mute */
#define	LDACO_MID_GAIN		0x11	/* Left DAC mid gain */

/* Index 07 - Right DAC Output Control, Modes 1&2 */
#define	RDACO_REG		0x07	/* Right DAC Register */
#define	RDACO_ATTEN_MASK	0x3f	/* Right atten. mask, 1.5 dB/setp */
#define	RDACO_RDM		0x80	/* Right mute */
#define	RDACO_MID_GAIN		0x11	/* Right DAC mid gain */

/* Index 08 - Sample Rate and Data Format, Mode 2 only */
#define	FSDF_REG		0x08	/* Sample Rate & Data Format Register */
#define	FS_5510			0x01	/* XTAL2, Freq. Divide #0 */
#define	FS_6620			0x0f	/* XTAL2, Freq. Divide #7 */
#define	FS_8000			0x00	/* XTAL1, Freq. Divide #0 */
#define	FS_9600			0x0e	/* XTAL2, Freq. Divide #7 */
#define	FS_11025		0x03	/* XTAL2, Freq. Divide #1 */
#define	FS_16000		0x02	/* XTAL1, Freq. Divide #1 */
#define	FS_18900		0x05	/* XTAL2, Freq. Divide #2 */
#define	FS_22050		0x07	/* XTAL2, Freq. Divide #3 */
#define	FS_27420		0x04	/* XTAL1, Freq. Divide #2 */
#define	FS_32000		0x06	/* XTAL1, Freq. Divide #3 */
#define	FS_33075		0x0d	/* XTAL2, Freq. Divide #6 */
#define	FS_37800		0x09	/* XTAL2, Freq. Divide #4 */
#define	FS_44100		0x0b	/* XTAL2, Freq. Divide #5 */
#define	FS_48000		0x0c	/* XTAL1, Freq. Divide #6 */
#define	PDF_STEREO		0x10	/* Stereo Playback */
#define	PDF_MONO		0x00	/* Mono Playback */
#define	PDF_LINEAR8		0x00	/* Linear, 8-bit unsigned */
#define	PDF_ULAW8		0x20	/* u-Law, 8-bit companded */
#define	PDF_LINEAR16LE		0x40	/* Linear, 16-bit signed, little end. */
#define	PDF_ALAW8		0x60	/* A-Law, 8-bit companded */
#define	PDF_ADPCM4		0xa0	/* ADPCM, 4-bit, IMA compatible */
#define	PDB_LINEAR16BE		0xc0	/* Linear, 16-bit signed, big endian */

/* Index 09 - Interface Configuration, Mode 1&2 */
#define	INTC_REG		0x09	/* Interrupt Configuration Register */
#define	INTC_PEN		0x01	/* Playback enable */
#define	INTC_CEN		0x02	/* Capture enable */
#define	INTC_SDC		0x04	/* Single DMA channel */
#define	INTC_DDC		0x00	/* Dual DMA channels */
#define	INTC_ACAL		0x08	/* Auto-Calibrate Enable */
#define	INTC_PPIO		0x40	/* Playback vi PIO */
#define	INTC_PDMA		0x00	/* Playback vi DMA */
#define	INTC_CPIO		0x80	/* Capture vi PIO */
#define	INTC_CDMA		0x00	/* Capture vi DMA */

/* Index 10 - Pin Control, Mode 1&2 */
#define	PC_REG			0x0a	/* Pin Control Register */
#define	PC_IEN			0x02	/* Interrupt Enable */
#define	PC_DEN			0x04	/* Dither Enable */
#define	PC_XCTL0		0x40	/* External control 0 */
#define	LINE_OUT_MUTE		0x40	/* Line Out Mute */
#define	PC_XCTL1		0x80	/* External control 1 */
#define	HEADPHONE_MUTE		0x80	/* Headphone Mute */

/* Index 11 - Error Status and Initialization, Mode 1&2 */
#define	ESI_REG			0x0b	/* Error Status & Init. Register */
#define	ESI_ORL_MASK		0x03	/* Left ADC Overrange */
#define	ESI_ORR_MASK		0x0c	/* Right ADC Overrange */
#define	ESI_DRS			0x10	/* DRQ status */
#define	ESI_ACI			0x20	/* Auto-Calibrate In Progress */
#define	ESI_PUR			0x40	/* Playback Underrun */
#define	ESI_COR			0x80	/* Capture Overrun */

/* Index 12 - Mode and ID, Modes 1&2 */
#define	MID_REG			0x0c	/* Mode and ID Register */
#define	MID_ID_MASK		0x0f	/* CODEC ID */
#define	MID_MODE2		0x40	/* Mode 2 enable */

/* Index 13 - Loopback Control, Modes 1&2 */
#define	LC_REG			0x0d	/* Loopback Control Register */
#define	LC_LBE			0x01	/* Loopback Enable */
#define	LC_ATTEN_MASK		0xfc	/* Loopback attenuation mask */
#define	LC_OFF			0x00	/* Loopback off */

/* Index 14 - Playback Upper Base, Mode 2 only */
#define	PUB_REG			0x0e	/* Playback Upper Base Register */

/* Index 15 - Playback Lower Base, Mode 2 only */
#define	PLB_REG			0x0f	/* Playback Lower Base Register */

/* Index 16 - Alternate Feature Enable 1, Mode 2 only */
#define	AFE1_REG		0x10	/* Alternate Feature Enable 1 Reg */
#define	AFE1_DACZ		0x01	/* DAC Zero */
#define	AFE1_TE			0x40	/* Timer Enable */
#define	AFE1_OLB		0x80	/* Output Level Bit, 1=2.8Vpp, 0=2Vpp */

/* Index 17 - Alternate Feature Enable 2, Mode 2 only */
#define	AFE2_REG		0x11	/* Alternate Feature Enable 2 Reg */
#define	AFE2_HPF		0x01	/* High Pass Filter - DC blocking */

/* Index 18 - Left Line Input Control, Mode 2 only */
#define	LLIC_REG		0x12	/* Left Line Input Control Register */
#define	LLIC_MIX_GAIN_MASK	0x1f	/* Left Mix Gain Mask, 1.5 dB/step */
#define	LLIC_LLM		0x80	/* Left Line Mute */
#define	LLIC_UNITY_GAIN		0x08	/* Left unit gain */

/* Index 19 - Right Line Input Control, Mode 2 only */
#define	RLIC_REG		0x13	/* Right Line Input Control Register */
#define	RLIC_MIX_GAIN_MASK	0x1f	/* Right Mix Gain Mask, 1.5 dB/step */
#define	RLIC_RLM		0x80	/* Right Line Mute */
#define	RLIC_UNITY_GAIN		0x08	/* Right unit gain */

/* Index 20 - Timer Lower Byte, Mode 2 only */
#define	TLB_REG			0x14	/* Timer Lower Byte Register */

/* Index 21 - Timer Upper Byte, Mode 2 only */
#define	TUB_REG			0x15	/* Timer Upper Byte Register */

/* Index 22 and 23 are reserved */

/* Index 24 - Alternate Feature Status, Mode 2 only */
#define	AFS_REG			0x18	/* Alternate Feature Status Registure */
#define	AFS_PU			0x01	/* Playback Underrun */
#define	AFS_PO			0x02	/* Playback Overrun */
#define	AFS_CO			0x04	/* Capture Overrun */
#define	AFS_CU			0x08	/* Capture Underrun */
#define	AFS_PI			0x10	/* Playback Interrupt */
#define	AFS_CI			0x20	/* Capture Interrupt */
#define	AFS_TI			0x40	/* Timer Interrupt */
#define	AFS_RESET_STATUS	0x00	/* Reset the status register */

/* Index 25 - Version and ID, Mode 2 only */
#define	VID_REG			0x19	/* Version and ID Register */
#define	VID_CID_MASK		0x07	/* Chip ID Mask */
#define	VID_VERSION_MASK	0xe0	/* Version number Mask */
#define	VID_A			0x20	/* Version A */
#define	VID_CDE			0x80	/* Versions C, D or E */

/* Index 26 - Mono I/O Control, Mode 2 only */
#define	MIOC_REG		0x1a	/* Mono I/O Control Register */
#define	MIOC_MI_ATTEN_MASK	0x0f	/* Mono In Attenuation Mask */
#define	MIOC_MOM		0x40	/* Mono Out Mute */
#define	MONO_SPKR_MUTE		0x40	/* Mono (internal) speaker mute */
#define	MIOC_MIM		0x80	/* Mono In Mute */

/* Index 27 is reserved */

/* Index 28 - Capture Data Format, Mode 2 only */
#define	CDF_REG			0x1c	/* Capture Date Foramt Register */
#define	CDF_STEREO		0x10	/* Stereo Capture */
#define	CDF_MONO		0x00	/* Mono Capture */
#define	CDF_LINEAR8		0x00	/* Linear, 8-bit unsigned */
#define	CDF_ULAW8		0x20	/* u-Law, 8-bit companded */
#define	CDF_LINEAR16LE		0x40	/* Linear, 16-bit signed, little end. */
#define	CDF_ALAW8		0x60	/* A-Law, 8-bit companded */
#define	CDF_ADPCM4		0xa0	/* ADPCM, 4-bit, IMA compatible */
#define	CDB_LINEAR16BE		0xc0	/* Linear, 16-bit signed, big endian */

/* Index 29 is reserved */

/* Index 30 - Capture Upper Base, Mode 2 only */
#define	CUB_REG			0x1e	/* Capture Upper Base Register */

/* Index 31 - Capture Lower Base, Mode 2 only */
#define	CLB_REG			0x1f	/* Capture Lower Base Register */

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_AUDIO_4231_H */
