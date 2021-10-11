/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_SBPRO_H
#define	_SYS_SBPRO_H

#ident "@(#)sbpro.h   1.7   97/03/11 SMI"

/*
 *	Header file defining all registers and ioctl commands for
 *	the Creative Labs Sound Blaster audio cards for Solaris/PC.
 */

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


#define	SB_MIN_IO 0x220
#define	SB_MAX_IO 0x280
#define	SB_IO_GAP 0x20
#define	SB_IO_LEN 0x14

#define	SB_IO_FMMUSIC 0x388
#define	SB_IO_FMMUSIC_LEN 0x4

/*
 * Interrupt controller ports (for IRQ probing):
 */
#define	PIC1_PORT	0x20	/* Master interrupt controller port */
#define	PIC2_PORT	0xA0	/* Slave interrupt controller port */
#define	READ_IRR	0x0A	/* Read interrupt request reg */
#define	READ_ISR	0x0B	/* Read interrupt status reg */
#define	IRQ5_MASK	0x20
#define	IRQ7_MASK	0x80
#define	IRQ9_MASK	0x2
#define	IRQ10_MASK	0x4

/*
 * First DMA (handles dma channels 0->3) controller definitions
 * Largely from common/sys/dma_i8237A.h
 */
#define	DMAC1_MASK	0x0A	/* Mask set/reset register */
#define	DMAC1_MODE	0x0B	/* Mode reg */
#define	DMAC1_CLFF	0x0C	/* Clear byte pointer first/last flip-flop */
#define	DMA1_ADDR_BASE_REG 0
#define	DMA1_CNT_BASE_REG 1

#define	DMA_0PAGE	0x87	/* Channel 0 address extension reg */
#define	DMA_1PAGE	0x83	/* Channel 1 address extension reg */
#define	DMA_2PAGE	0x81	/* Channel 2 address extension reg */
#define	DMA_3PAGE	0x82	/* Channel 3 address extension reg */

/*
 * DMA Mask bits.
 */
#define	DMA_SETMSK	4	/* Set mask bit */
#define	DMA_CLRMSK	0	/* Clear mask bit */

/*
 * Write-only Mode register bits.
 */
#define	DMAMODE_SINGLE  0x40	/* Select Single mode */
#define	DMAMODE_READ	0x04	/* Read Transfer */

/*
 * MPU-401 (SB-16 only) definitions
 */
#define	MPU_PORT_A 0x330
#define	MPU_PORT_B 0x300
#define	MPU_PORT_LEN 2
#define	MPU_DATA_PORT 0
#define	MPU_STATUS_PORT 1
#define	MPU_CMD_PORT 1
#define	MPU_RESET_CMD 0xff
#define	MPU_RESET_SUCCESS 0xfe
#define	MPU_STAT_OUTPUT_READY 0x40 /* bit clear if ready */
#define	MPU_STAT_INPUT_DATA 0x80 /* bit clear if data waiting */
#define	MPU_MAX_RETRIES 0xffff

#endif	/* _SYS_SBPRO_H */
