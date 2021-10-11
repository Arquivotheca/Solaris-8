/*
 * Copyright (c) 1992, 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SBPRO_H
#define	_SYS_SBPRO_H

#pragma ident	"@(#)sbpro.h	1.30	99/05/04 SMI"

/*
 *	Header file defining all registers and ioctl commands for
 *	the Creative Labs Sound Blaster audio cards for Solaris/PC.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * offsets to registers for the Digital-to-Analog and Analog-to-Digital
 * converters (DSP chips)
 */
#define	DSP_RESET	0x06		/* DSP Reset			(w) */
#define	DSP_RDDATA	0x0a		/* DSP Read Data Port		(r) */
#define	DSP_WRDATA_CMD	0x0c		/* DSP Write Data or Command	(w) */
#define	DSP_WRSTATUS	0x0c		/* DSP Write Buffer Status	(r) */
#define	DSP_DATAAVAIL	0x0e		/* DSP Data Available Status	(r) */
#define	DSP_DMA16_ACK	0x0f		/* DSP 16bit DMA interrupt ack  (r) */

/* Mixer Chip */
#define	MIXER_ADDR	0x04		/* Mixer Chip Register Addr Port (w) */
#define	MIXER_DATA	0x05		/* Mixer Chip Data Port		(rw) */

/*
 * offsets to registers for CD-ROM control (not yet implemented)
 */
#define	CDROM_DATA	0x10		/* CD-ROM Data Register		(r) */
#define	CDROM_COMMAND	0x10		/* CD-ROM Command Register	(w) */
#define	CDROM_STATUS	0x11		/* CD-ROM Status Register	(r) */
#define	CDROM_RESET	0x12		/* CD-ROM Reset Register	(w) */
#define	CDROM_ENABLE	0x13		/* CD-ROM Enable Register	(w) */

/*
 * offsets to registers for the FM music chips (not yet implemented)
 */
#define	FM_STATUS_L	0x00		/* Left FM Music Status	Port	(r) */
#define	FM_ADDR_L	0x00		/* Left FM Music Register Addr	(w) */
#define	FM_DATA_L	0x01		/* Left FM Music Data Port	(w) */
#define	FM_STATUS_R	0x02		/* Right FM Music Status Port	(r) */
#define	FM_ADDR_R	0x02		/* Right FM Music Register Addr (w) */
#define	FM_DATA_R	0x03		/* Right FM Music Data Port	(w) */
#define	FM_STATUS	0x08		/* FM Music Status Port		(r) */
#define	FM_REGISTER	0x08		/* FM Music Register Port	(w) */
#define	FM_DATA		0x09		/* FM Music Data Port		(w) */

/*
 *	Values in various registers:
 */

/* in DSP_RESET */
#define	RESET_CMD	0x01		/* trigger DSP reset */

/* in DSP_DATAAVAIL */
#define	DATA_READY	0x80		/* Data Available from DSP_RDDATA */

/* in DSP_WRSTATUS */
#define	WR_BUSY		0x80		/* if set, DSP is still busy */

/* in DSP_RDDATA */
#define	READY		0xAA		/* after RESET_CMD->DSP_RESET */

/*
 *	Commands to the DSP chips:
 */
#define	DAC_DIRECT	0x10		/* Direct mode DAC (polled)	*/
#define	DAC_DMA_8	0x14		/* DMA mode 8-bit DAC		*/
#define	DAC_DMA_2	0x16		/* DMA mode 2-bit ADPCM DAC	*/
#define	DAC_DMA_2_REF	0x17		/* DMA mode 2-bit w/ref byte	*/
#define	DAC_DMA_8_AI	0x1C		/* Auto init DMA mode 8-bit DAC	*/

#define	ADC_DIRECT	0x20		/* Direct mode ADC (polled)	*/
#define	ADC_DMA		0x24		/* DMA mode ADC			*/
#define	ADC_DMA_AI	0x2C		/* DMA mode ADC Auto init	*/

#define	MIDI_READ_POLL	0x30		/* MIDI Read (polling mode)	*/
#define	MIDI_READ_INTR	0x31		/* MIDI Read (interrupt mode)	*/
#define	MIDI_UART_POLL	0x34		/* MIDI UART mode (polling)	*/
#define	MIDI_UART_INTR	0x35		/* MIDI UART mode (interrupt)	*/
#define	MIDI_WRITE_POLL	0x38		/* MIDI Write (polling mode)	*/

#define	SET_CONSTANT	0x40		/* Set Time Constant		*/
#define	SET_SRATE_OUTPUT 0x41		/* set sampling rate output */
#define	SET_SRATE_INPUT	0x42		/* set sampling rate input */
#define	SET_BLOCK_SIZE	0x48		/* Set Block Size		*/

#define	DAC_DMA_4	0x74		/* DMA-mode DAC 4-bit ADPCM	*/
#define	DAC_DMA_4_REF	0x75		/*   ""   ""   "" w/ref byte	*/
#define	DAC_DMA_26	0x76		/* DMA-mode DAC 2.6-bit ADPCM	*/
#define	DAC_DMA_26_REF	0x77		/*    ""   ""   "" w/ref byte	*/

#define	SILENCE_MODE	0x80		/* Select Silence Mode		*/

#define	DAC_DMA_FAST_AI	0x90		/* DMA high speed 8-bit DAC auto */
#define	DAC_DMA_FAST	0x91		/* DMA high speed 8-bit DAC mode */
#define	ADC_DMA_FAST_AI	0x98		/* DMA high speed 8-bit ADC auto */
#define	ADC_DMA_FAST	0x99		/* DMA high speed 8-bit ADC mode */

#define	RECORD_MONO	0xA0		/* disable stereo mode flip-flop */
#define	START_LEFT	0xA8		/* Start stereo record at left chan */

#define	SB_DMA_16BIT	0xB0		/* 16 bit DMA for 4.x dsp */
#define	SB_DMA_8BIT	0xC0		/* 8 bit DMA for 4.x dsp */
#define	SB_DMA_D_to_A	0x00		/* mask for D/A */
#define	SB_DMA_A_to_D	0x08		/* mask for A/D */
#define	SB_DMA_AI	0x04		/* auto initialize */
#define	SB_DMA_FIFO	0x02		/* turn on FIFO */
					/* 8bit DMA single cycle input */
#define	SB_8_SC_INPUT	(SB_DMA_8BIT | SB_DMA_A_to_D)
					/* 16bit DMA single cycle input */
#define	SB_16_SC_INPUT	(SB_DMA_16BIT | SB_DMA_A_to_D)
					/* 8bit DMA auto initialize input */
#define	SB_8_AI_INPUT	(SB_DMA_8BIT | SB_DMA_A_to_D | SB_DMA_AI | SB_DMA_FIFO)
					/* 16bit DMA auto initialize  input */
#define	SB_16_AI_INPUT	(SB_DMA_16BIT | SB_DMA_A_to_D | SB_DMA_AI | SB_DMA_FIFO)
					/* 8bit DMA single cycle output */
#define	SB_8_SC_OUTPUT	(SB_DMA_8BIT | SB_DMA_D_to_A)
					/* 16bit DMA single cycle output */
#define	SB_16_SC_OUTPUT	(SB_DMA_16BIT | SB_DMA_D_to_A)
					/* 8bit DMA auto initialize output */
#define	SB_8_AI_OUTPUT	(SB_DMA_8BIT | SB_DMA_D_to_A | SB_DMA_AI | SB_DMA_FIFO)
					/* 16bit DMA auto initialize output */
#define	SB_16_AI_OUTPUT	(SB_DMA_16BIT | SB_DMA_D_to_A | SB_DMA_AI | SB_DMA_FIFO)
#define	SB_8_MONO	0x00		/* 8bit mono, arg for DMA cmds */
#define	SB_8_ST		0x20		/* 8bit stereo */
#define	SB_16_MONO	0x10		/* 16bit mono */
#define	SB_16_ST	0x30		/* 16bit stereo */

#define	HALT_DMA	0xD0		/* Halt DMA		*/
#define	SPEAKER_ON	0xD1		/* Turn On Speaker	*/
#define	SPEAKER_OFF	0xD3		/* Turn Off Speaker	*/
#define	CONTINUE_DMA	0xD4		/* Continue DMA		*/
#define	HALT_DMA16	0xD5		/* Halt DMA 16bit	*/
#define	CONTINUE_DMA16	0xD6		/* Continue DMA		*/
#define	GET_SPEAKER	0xD8		/* Get Speaker Status	*/
#define	EXIT_AUTO_DMA16	0xD9		/* exit 16bit auto-init dma */
#define	EXIT_AUTO_DMA8	0xDA		/* exit 8bit  auto-init dma */

#define	GET_DSP_VER	0xE1		/* Get DSP Version	*/

/*
 *	Values for the mixer registers
 */
#define	MIXER_RESET	0x00		/* reset register (write-only) */
#define	MIXER_VOICE	0x04		/* voice volume register */
#define	MIXER_MIC	0x0A		/* microphone mixing register */
#define	MIXER_INPUT	0x0C		/* input source & filter */
#define	MIXER_OUTPUT	0x0E		/* output filter & stereo/mono */
#define	MIXER_MASTER	0x22		/* master volume register */
#define	MIXER_FM	0x26		/* FM volume register */
#define	MIXER_CD	0x28		/* CD volume register */
#define	MIXER_LINE	0x2E		/* line-in volume register */
#define	MIXER_16_LEFT_MASTER	0x30	/* SB16 left MASTER volume */
#define	MIXER_16_RIGHT_MASTER	0x31	/* SB16 right MASTER volume */
#define	MIXER_16_LEFT_VOICE	0x32	/* SB16 left VOICE volume */
#define	MIXER_16_RIGHT_VOICE	0x33	/* SB16 right MASTER volume */
#define	MIXER_16_LEFT_FM	0x34	/* SB16 left MIDI volume */
#define	MIXER_16_RIGHT_FM	0x35	/* SB16 right MIDI volume */
#define	MIXER_16_LEFT_CD	0x36	/* SB16 left CD volume */
#define	MIXER_16_RIGHT_CD	0x37	/* SB16 right CD volume */
#define	MIXER_16_LEFT_LINE	0x38	/* SB16 left LINE volume */
#define	MIXER_16_RIGHT_LINE	0x39	/* SB16 right LINE volume */
#define	MIXER_16_MIC		0x3a	/* SB16 MIC volume register */
#define	MIXER_16_PCSPEAKER	0x3B	/* SB16 PC Speaker */
#define	MIXER_16_OUTPUT		0x3C
#define	MIXER_16_INPUT_LEFT	0x3D	/* SB16 Input device switch left */
#define	MIXER_16_INPUT_RIGHT	0x3E	/* SB16 Input device switch right */
#define	MIXER_16_INPUT_LEFT_GAIN 0x3F	/* SB16 LEFT INPUT GAIN REG */
#define	MIXER_16_INPUT_RIGHT_GAIN 0x40	/* SB16 RIGHT INPUT GAIN REG */
#define	MIXER_16_OUTPUT_LEFT_GAIN 0x41	/* SB16 LEFT OUTPUT GAIN REG */
#define	MIXER_16_OUTPUT_RIGHT_GAIN 0x42	/* SB16 RIGHT OUTPUT GAIN REG */
#define	MIXER_16_AGC		0x43	/* SB16 RIGHT BASE REG */
#define	MIXER_16_LEFT_TREBLE	0x44	/* SB16 LEFT TREBLE REG */
#define	MIXER_16_RIGHT_TREBLE	0x45	/* SB16 RIGHT TREBLE REG */
#define	MIXER_16_LEFT_BASS	0x46	/* SB16 LEFT BASE REG */
#define	MIXER_16_RIGHT_BASS	0x47	/* SB16 RIGHT BASE REG */
#define	MIXER_IRQ	0x80		/* MIXER IRQ register */
#define	MIXER_DMA	0x81		/* MIXER DMA register */
#define	MIXER_ISR	0x82		/* interrupt status register */

#define	INPUT_16_MIC		1	/* input switch mask for microphone */
#define	INPUT_16_RIGHT_CD	2	/* input switch mask for right cd */
#define	INPUT_16_LEFT_CD	4	/* input switch mask for left cd */
#define	INPUT_16_RIGHT_LINE	8	/* input switch mask for right line */
#define	INPUT_16_LEFT_LINE	16	/* input switch mask for left line */

#define	OUTPUT_ALLOFF		0	/* all output switches off */
#define	OUTPUT_16_MIC		1	/* output switch mask for microphone */
#define	OUTPUT_16_RIGHT_CD	2	/* output switch mask for right cd */
#define	OUTPUT_16_LEFT_CD	4	/* output switch mask for left cd */
#define	OUTPUT_16_RIGHT_LINE	8	/* output switch for right line */
#define	OUTPUT_16_LEFT_LINE	16	/* output switch for left line */

/* values in the MIXER_INPUT register */
#define	MIC		(0x00<<1)	/* microphone input */
#define	CD		(0x01<<1)	/* CD input */
#define	MIC_2		(0x02<<1)	/* alternate value for mic input */
#define	LINE		(0x03<<1)	/* line-level input */
#define	FLTR_LO		0x00		/* low filter */
#define	FLTR_HI		0x08		/* high filter */
#define	FLTR_NO		0x20		/* no filter */
#define	MAX_VOICE	0xff		/* MAX voice register value */
#define	VOICE_OFF	0x0		/* switch off voice, volume=0 */

/*
 * This is a "temporary" hack -- benh.
 * Once the jury is in on this ioctl()
 * "feature" we'll put it into the
 * <sys/audioio.h> header file.
 */
#define	AUDIO_RESET	_IO('A', 9)
#define	SET_MASTER 128

/* values in the MIXER_OUTPUT register */
#define	DNFI		0x20		/* do not filter output if 1 */
#define	VSTC		0x02		/* enable stereo output if 1 */

/* Bitmask definitions for setting up the SB16 DMA and IRQ registers */
#define	DMA_CHAN_5	0x20
#define	DMA_CHAN_6	0x40
#define	DMA_CHAN_7	0x80
#define	DMA_CHAN_0	0x01
#define	DMA_CHAN_1	0x02
#define	DMA_CHAN_3	0x08

/* interrupt status register ISR bits */
#define	IS_8DMA_MIDI	0x01		/* 8bit DMA or MIDI interrupt */
#define	IS_16DMA	0x02		/* 16bit DMA interrupt */
#define	IS_MPU401	0x04		/* MPU-401 interrupt */

/* SB Pro filter frequency boundaries */
#define	TOP_LOW_SAMPLE_RATE	18000	/* SBPRO low sample rate, Hz */
#define	TOP_MID_SAMPLE_RATE	36000	/* SBPRO high sample rate, Hz */

/*
 * MWSS and AD184x register definitions
 */
#define	MWSS_IRQSTAT		0	/* Interrupt status */
#define	AD184x_INDEX		4	/* Index into register bank */
#define	AD184x_MODE_CHANGE	0x40	/* Index bit to allow mode change */
#define	AD184x_DATA		5	/* Indexed register data */
#define	AD184x_STATUS		6	/* More interrupt status */

#define	LEFT_IN_REG		0x0	/* Left input select/gain */
#define	RIGHT_IN_REG		0x1	/* Right input select/gain */
#define	LEFT_OUT_REG		0x6	/* Left output volume */
#define	RIGHT_OUT_REG		0x7	/* Right output volume */
#define	FORMAT_REG		0x8	/* Sampling format/rate */
#define	CFG_REG			0x9	/* Record/Play enable */
#define	PIN_REG			0xa	/* Interrupt enable */
#define	TEST_INIT_REG		0xb	/* Test and Init */
#define	MON_LOOP_REG		0xd	/* Monitor Volume */
#define	COUNT_HIGH_REG		0xe	/* Interrupt countdown */
#define	COUNT_LOW_REG		0xf

#define	AD184x_INIT_AUDIO_GAIN	((AUDIO_MAX_GAIN * 3) / 5)
#define	AD184x_INIT_AUDIO_RGAIN	((AUDIO_MAX_GAIN) / 2)

#define	INTR_DONT_KNOW	DDI_INTR_UNCLAIMED


/*
 *	Audio Device state values:
 */
#define	AUDIO_IS_NOT_ACTIVE		0x00
#define	AUDIO_IS_ACTIVE			0x01

/*
 *	Initial settings for the Sound-Blaster card:
 */
#define	INIT_AUDIO_GAIN		(AUDIO_MAX_GAIN / 2)

/*
 * Using the third level of gain control for an SB16, the
 * initial volume levels for play volume, record volume &
 * monitor_gain are set to 3/4's the max level, because at
 * this level it appears that most NORMAL levels of input
 * from CDs are recorded to the maximum amplitude before
 * entering into the red-zone where they may be clipped if
 * the amplitude is too high. This in turn would cause
 * distortion.
 */
#define	SB16_INIT_AUDIO_GAIN	((AUDIO_MAX_GAIN * 3) /4)
#define	SILENT_BYTE	0x80		/* Silent Byte for linear data */

#ifdef _KERNEL
/*
 *	All kernel data for each card is kept here.
 */
typedef struct {
	ushort_t	ioaddr;		/* card's base i/o address	*/
	int		dmachan8;	/* 8 bit DMA channel		*/
	int		dmachan16;	/* 16 bit DMA channel		*/
	int		nj_card;	/* Non-jumpered card		*/

	/* current state of device */
	int		dmachan;	/* current DMA channel used	*/
	uint_t		dsp_speed;	/* sampling rate set in DSP	*/
	uchar_t		dsp_fastmode;	/* currently executing in fast mode? */
	uchar_t		dsp_stereomode;	/* is DSP in stereo mode?	*/
	uchar_t		dsp_silent;	/* center amplitude value	*/
	int right_input_switch;	/* used on the SB16 card to maintain mask */
	int left_input_switch; 	/* used on the SB16 card to hold current mask */
	int output_switch;	/* SB16 card, active output ports */

	ushort_t	flags;
#define	W_BUSY		0x0001	/* output write is busy */
#define	R_BUSY		0x0002	/* input read is in progress */
#define	CLOSE_WAIT	0x0004	/* sleeping in close() while output drains */
#define	READING		0x0008	/* process open for reading from card (ADC) */
#define	WRITING		0x0010	/* process open for writing to card (DAC) */
#define	PAUSED_DMA	0x0020	/* DMA has been paused using HALT_DMA cmd */
#define	ATTACH_COMPLETE	0x0040	/* attach has been completed,allow interrupts */
#define	SB16		0x0080	/* is this an SB16 card */
#define	PAUSED_DMA8	0x0100	/* 8 bit DMA has been paused */
#define	PAUSED_DMA16	0x0200	/* 16 bit DMA has been paused */
#define	AUTO_DMA	0x0400	/* auto initialize dma */
#define	ENDING		0x0800	/* auto initialize dma */
#define	START_STEREO	0x1000 	/* Set if a single step required to start */
				/*    SBPRO stereo on play */
#define	SBPRO		0x2000	/* is this an SBPRO card ? */
#define	AD184x		0x4000	/* is this an AD184x chip ? */
#define	CBA1847		0x8000	/* is this a Compaq AD1847 ? */

	queue_t		*rdq;		/* read-side queue */
	queue_t		*wrq;		/* write-side queue */
	queue_t		*ctlrdq;	/* control device's read-side queue */
	queue_t		*ctlwrq;	/* control device's write-side queue */
	mblk_t		*wmsg;		/* current message being output */
	mblk_t		*paused_buffer;	/* buffer paused while playing */
	int		paused_eof;	/* eofs for paused buffer */

	/*
	 * each of the the buffers are a half of a single allocated buffer
	 * by ddi_iopb_alloc(), we change the buffer length (buflen)
	 * according to the sampling rate and buflim keeps the size of
	 * the allocated buffer
	 */
	uint_t		buflim;		/* actual length of allocated buffer */
	uchar_t		*buffers[2];	/* pointers to double buffers */
	uint_t		buflen;		/* length of each buffer */
	int		length[2];	/* length of data in buffer */
	uint_t		bufnum;		/* buffer currently being transferred */

	int		eofbuf[2];	/* number of eofs in buffer */

	int		sampbuf[2];	/* number of sample counts in buffer */

	/* DMA-specific data */
	ddi_dma_handle_t aidmahandle;	/* auto init DMA handle */
	ddi_dma_cookie_t dmacookie;	/* DMA cookie */
	int		dspbits;	/* sample precision */
	ddi_dma_lim_t	dma_limits;	/* dma limits for card */

	int		count;		/* transfer byte count */
	int		sampcount;	/* transfer sample count */

	/* kernel DDI/DKI interface things: */
	dev_info_t		*dip;	/* pointer to devinfo for this card */
	kmutex_t		mutex;	/* kernel mutex */
	kcondvar_t		cvopen;	/* kernel condition variable */
	kcondvar_t		cvclosewait;	/* kernel condition variable */
	ddi_iblock_cookie_t	iblock_cookie;
	ddi_idevice_cookie_t	idevice_cookie;

	audio_info_t	audio_info;	/* settings and info */

	audio_device_t sbpro_devtype;	/* getinfo structure */

	/* default device state values */
	int inputleftgain, inputrightgain, outputleftgain, outputrightgain;
	int bassleft, bassright, trebleleft, trebleright, agc;
	int sample_rate, channels, precision, encoding;
	int pcspeaker;

	/*
	 * To get the SBPRO to correctly enter stereo mode, we must use a single
	 * cycle DMA to output an extra "silent" byte at the beginning of the
	 * playback.
	 */
	ddi_dma_cookie_t	sbpro_stereo_byte_cookie;
	ddi_dma_handle_t	stereo_byte_dmahand;
} SBInfo;

/* useful macro to make property lookup simple */
#define	GET_INT_PROP(devi, pname, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
			DDI_PROP_DONTPASS, (pname), (caddr_t)(pval), (plen)))

/*
 * flags/masks for error printing.
 * the levels are for severity
 */
#define	SBEP_L0		0	/* chatty as can be - for debug! */
#define	SBEP_L1		1	/* best for debug */
#define	SBEP_L2		2	/* minor errors - retries, etc. */
#define	SBEP_L3		3	/* major errors */
#define	SBEP_L4		4	/* catastrophic errors, don't mask! */
#define	SBEP_L5		5	/* Major error, print message and panic. */
#define	SBEP_LMAX	5	/* catastrophic errors, don't mask! */

#if defined(DEBUG) && !defined(lint)
#define	SBERRPRINT(l, m, args)	{\
	if (sbdebug && (l) >= sberrlevel &&\
	    (((m)&sberrmask) || (l) >= SBEP_L2))\
		cmn_err args;\
	if (l == SBEP_L5) {\
		cmn_err(CE_WARN, "SBPRO_PANIC!");\
		cmn_err args;\
		cmn_err(CE_PANIC, "sbpro: This panic is sponsored by SBPRO!");\
	}\
}
#else
#define	SBERRPRINT(l, m, args)	{ }
#endif	/* DEBUG */

#define	CHECK_IF_CONTROL(device_format)	{\
	if ((q == info->ctlrdq) || (q == info->ctlwrq)) {\
		SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_WARN,\
		    "sbpro_checkprinfo: %s cannot be changed on the "\
		    "Audio Control Device", device_format));\
		invalid++;\
	}\
}

#define	CHECK_CORRECT_SIDE(device_format)	{\
	if ((q == info->wrq && record_info)) {\
		SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_WARN,\
		    "sbpro_checkprinfo: Changing %s on Record side when "\
		    "open for Play", device_format));\
		invalid++;\
	}\
	if ((RD(q) == info->rdq && !record_info)) {\
		SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_WARN,\
		    "sbpro_checkprinfo: Changing %s on Play side when "\
		    "open for Record", device_format));\
		invalid++;\
	}\
}

/*
 * for each function, we can mask off its printing by clearing its bit in
 * the sberrmask.  Some functions (_init, _info) share a mask bit
 */
#define	SBEM_IDEN 0x00000001	/* sbpro_identify */
#define	SBEM_PROB 0x00000002	/* sbpro_probe */
#define	SBEM_ATTA 0x00000004	/* sbpro_attach */
#define	SBEM_REST 0x00000008	/* sbpro_reset */
#define	SBEM_OPEN 0x00000010	/* sbpro_open */
#define	SBEM_CLOS 0x00000020	/* sbpro_close */
#define	SBEM_WPUT 0x00000100	/* sbpro_wput */
#define	SBEM_WSRV 0x00000200	/* sbpro_wsrv */
#define	SBEM_STRT 0x00000400	/* sbpro_start */
#define	SBEM_DCMD 0x00001000	/* dsp_command */
#define	SBEM_DDAT 0x00002000	/* dsp_data */
#define	SBEM_DWDA 0x00004000	/* dsp_writedata */
#define	SBEM_DRDA 0x00008000	/* dsp_readdata */
#define	SBEM_IOCT 0x00010000	/* sbpro_ioctl */
#define	SBEM_IOCD 0x00020000	/* sbpro_iocdata */
#define	SBEM_SPRI 0x00040000	/* sbpro_setprinfo */
#define	SBEM_SINF 0x00080000	/* sbpro_setinfo */
#define	SBEM_MIXR 0x00100000	/* setmixer */
#define	SBEM_INTR 0x01000000	/* sbpro_intr */
#define	SBEM_SSIG 0x02000000	/* sbpro_sendsig */
#define	SBEM_DMA  0x04000000	/* dsp_dmahalt and dsp_dmacont */
#define	SBEM_MODS 0x08000000	/* _init, _info, _fini */
#define	SBEM_ALL  0xFFFFFFFF	/* all */

#endif	/* _KERNEL */

#ifdef COMMENT
/*
 * Convert to 8-bit unsigned linear to u-law
 *
 * See below for u-law format.
 */
unsigned char
linear_to_ulaw(linbyte)
unsigned char linbyte;
{
	static int exp_lut[128] = {
	    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
	};
	int sign = 0, exponent, mantissa, sample;
	unsigned char ulawbyte;

	sample = (int)linbyte - 0x80;	/* convert excess-128 to signed */
	if (sample < 0) {
		sign = 0x80;
		sample = -sample;	/* magnitude */
	}
	if (sample > 0x7f)
		sample = 0x7f;		/* in case was 0x80 */

	exponent = exp_lut[sample];
	sample = sample << 5;

	/* add another 1/2 of the least significant bit */
	if (sample)
		sample += 0x10;

	mantissa = (sample >> exponent) & 0x0F;	/* drop high bit */
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
	return (ulawbyte);
}
#endif /* COMMENT */

static  unsigned char   raw_to_ulaw[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
	0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
	0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05,
	0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07,
	0x07, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09,
	0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f,
	0x0f, 0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13,
	0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x16, 0x17,
	0x17, 0x18, 0x18, 0x19, 0x19, 0x1a, 0x1a, 0x1b,
	0x1b, 0x1c, 0x1c, 0x1d, 0x1d, 0x1e, 0x1e, 0x1f,
	0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
	0x2f, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c,
	0x3e, 0x41, 0x45, 0x49, 0x4d, 0x53, 0x5b, 0x67,
	0xff, 0xe7, 0xdb, 0xd3, 0xcd, 0xc9, 0xc5, 0xc1,
	0xbe, 0xbc, 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0,
	0xaf, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa8,
	0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0,
	0x9f, 0x9f, 0x9e, 0x9e, 0x9d, 0x9d, 0x9c, 0x9c,
	0x9b, 0x9b, 0x9a, 0x9a, 0x99, 0x99, 0x98, 0x98,
	0x97, 0x97, 0x96, 0x96, 0x95, 0x95, 0x94, 0x94,
	0x93, 0x93, 0x92, 0x92, 0x91, 0x91, 0x90, 0x90,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8a, 0x8a, 0x8a, 0x8a,
	0x89, 0x89, 0x89, 0x89, 0x88, 0x88, 0x88, 0x88,
	0x87, 0x87, 0x87, 0x87, 0x86, 0x86, 0x86, 0x86,
	0x85, 0x85, 0x85, 0x85, 0x84, 0x84, 0x84, 0x84,
	0x83, 0x83, 0x83, 0x83, 0x82, 0x82, 0x82, 0x82,
	0x81, 0x81, 0x81, 0x81, 0x80, 0x80, 0x80, 0x80
};

#ifdef COMMENT
/*
 * Convert u-law to 8-bit unsigned linear
 *
 * Don't consider this definitive, as I don't have the spec.
 *
 * u-law format is complement of:
 *   bit:	7   6   5   4   3   2   1   0
 *   contents:	S   E   E   E.M M   M   M   M
 * where:
 *   S is the sign bit,
 *   E is the exponent (power of 2)
 *   M is the mantissa (normalized with high bit assumed 1)
 *   . indicates the mantissa's binary radix point
 */
unsigned char
ulaw_to_linear(ulawbyte)
unsigned char ulawbyte;
{
	unsigned char byte, result;
	int sign, exponent, mantissa, sample, sampleX256;

	/* extract the bits */
	byte = ~ulawbyte;
	sign = (byte & 0x80);
	exponent = (byte >> 4) & 0x07;
	mantissa = byte & 0x0F;

	/* if number nonzero, add in the hidden mantissa bit */
	if (mantissa || exponent)
		mantissa |= 0x10;

	/* left align and add another 1/2 of the least significant bit */
	mantissa = (mantissa << 3) | 0x4;

	/* multiply mantissa by 2^exponent */
	sampleX256 = mantissa << exponent;

	sampleX256 += 0x7f;	/* round (1/2 rounds down) */
	sample = sampleX256 >> 8;
	if (sign != 0)
		sample = -sample;
	result = sample + 0x80;	/* convert signed value to excess 128 */
	return (result);
}
#endif	/* COMMENT */

static  unsigned char   ulaw_to_raw[256] = {
	0x02, 0x06, 0x0a, 0x0e, 0x12, 0x16, 0x1a, 0x1e,
	0x22, 0x26, 0x2a, 0x2e, 0x32, 0x36, 0x3a, 0x3e,
	0x41, 0x43, 0x45, 0x47, 0x49, 0x4b, 0x4d, 0x4f,
	0x51, 0x53, 0x55, 0x57, 0x59, 0x5b, 0x5d, 0x5f,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x70, 0x71, 0x71, 0x72, 0x72, 0x73, 0x73, 0x74,
	0x74, 0x75, 0x75, 0x76, 0x76, 0x77, 0x77, 0x78,
	0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x7a, 0x7a,
	0x7a, 0x7a, 0x7b, 0x7b, 0x7b, 0x7b, 0x7c, 0x7c,
	0x7c, 0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7d,
	0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7e,
	0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80,
	0xfe, 0xfa, 0xf6, 0xf2, 0xee, 0xea, 0xe6, 0xe2,
	0xde, 0xda, 0xd6, 0xd2, 0xce, 0xca, 0xc6, 0xc2,
	0xbf, 0xbd, 0xbb, 0xb9, 0xb7, 0xb5, 0xb3, 0xb1,
	0xaf, 0xad, 0xab, 0xa9, 0xa7, 0xa5, 0xa3, 0xa1,
	0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98,
	0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
	0x90, 0x8f, 0x8f, 0x8e, 0x8e, 0x8d, 0x8d, 0x8c,
	0x8c, 0x8b, 0x8b, 0x8a, 0x8a, 0x89, 0x89, 0x88,
	0x88, 0x88, 0x87, 0x87, 0x87, 0x87, 0x86, 0x86,
	0x86, 0x86, 0x85, 0x85, 0x85, 0x85, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x80
};

#ifdef OLD_TABLE
static  unsigned char   ulaw_to_raw[256] = {
	0xbe, 0xbc, 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0,
	0xae, 0xac, 0xaa, 0xa8, 0xa6, 0xa4, 0xa2, 0xa0,
	0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98,
	0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
	0x90, 0x8f, 0x8f, 0x8e, 0x8e, 0x8d, 0x8d, 0x8c,
	0x8c, 0x8b, 0x8b, 0x8a, 0x8a, 0x89, 0x89, 0x88,
	0x88, 0x88, 0x87, 0x87, 0x87, 0x87, 0x86, 0x86,
	0x86, 0x86, 0x85, 0x85, 0x85, 0x85, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x80,
	0x43, 0x45, 0x47, 0x49, 0x4b, 0x4d, 0x4f, 0x51,
	0x53, 0x55, 0x57, 0x59, 0x5b, 0x5d, 0x5f, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
	0x71, 0x72, 0x72, 0x73, 0x73, 0x74, 0x74, 0x75,
	0x75, 0x76, 0x76, 0x77, 0x77, 0x78, 0x78, 0x79,
	0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7a, 0x7b,
	0x7b, 0x7b, 0x7b, 0x7c, 0x7c, 0x7c, 0x7c, 0x7d,
	0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e,
	0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};
#endif	/* !OLD_TABLE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SBPRO_H */
