/*
 * Copyright (c) 1989-92 by Sun Microsystems, Inc.
 */

#ifndef _SYS_AUDIO_79C30_H
#define	_SYS_AUDIO_79C30_H

#pragma ident	"@(#)audio_79C30.h	1.13	95/05/26 SMI"

/*
 * This file describes the AMD79C30A ISDN chip and declares
 * parameters and data structures used by the audio driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * Driver constants for the AM79C30
 */
#define	AUD_79C30_PRECISION	(8) /* Bits per sample unit */
#define	AUD_79C30_CHANNELS	(1) /* Channels per sample frame */
#define	AUD_79C30_SAMPLERATE	(8000) /* Sample frames per second */

/*
 * This is the size of the STREAMS buffers we initially send up the read side
 * and the maximum record buffer size that can be specified by the user.
 */
#define	AUD_79C30_DEFAULT_BSIZE	(256)	/* Record buffer_size */
#define	AUD_79C30_MAX_BSIZE	(65536)	/* Maximum fragment size */

/*
 * High and low water marks for STREAMS flow control
 */
#define	AUD_79C30_HIWATER	(8192 * 3)	/* ~3 seconds at 8kHz */
#define	AUD_79C30_LOWATER	(8192 * 2)	/* ~2 seconds at 8kHz */

/*
 * Buffer allocation
 */
#define	AUD_79C30_CMDPOOL	(7)	/* total command block buffer pool */
#define	AUD_79C30_RECBUFS	(4)	/* number of record command blocks */
#define	AUD_79C30_PLAYBUFS	(AUD_79C30_CMDPOOL - AUD_79C30_RECBUFS)

/*
 * The maximum packet size interacts with flow control in the following
 * manner:
 *
 *	Large writes are broken up to AUD_79C30_MAXPACKET size chunks.
 *	AUD_79C30_PLAYBUFS packets are queued to the device-specific
 *	queue at any one time.  Additional packets are left on the write
 *	queue and their accumulated size is used for hiwater calculations.
 *
 * So, the most data that can be queued to the device at any one time can
 * be calculated by:
 *
 *	(AUD_79C30_PLAYBUFS * AUD_79C30_MAXPACKET) +
 *	  [AUD_79C30_HIWATER rounded up to a AUD_79C30_MAXPACKET boundary
 */
#define	AUD_79C30_MAXPACKET	(2048)

/*
 * Default gain settings
 */
#define	AUD_79C30_DEFAULT_PLAYGAIN	(75)	/* play gain initialization */
#define	AUD_79C30_DEFAULT_RECGAIN	(128)	/* gain initialization */

/*
 * Values returned by the AUDIO_GETDEV ioctl
 */
#define	AMD_DEV_NAME	"SUNW,am79c30"
#define	AMD_DEV_VERSION	"a"
#define	AMD_DEV_CONFIG_ONBRD1	"onboard1"


/*
 * Macros for distinguishing between minor devices
 */
#define	AMD_UNITMASK	(0x0f)
#define	AMD_UNIT(dev)	((geteminor(dev)) & AMD_UNITMASK)
#define	AMD_ISCTL(dev)	(((geteminor(dev)) & AMD_MINOR_CTL) != 0)
#define	AMD_MINOR_RW	(0x20)
#define	AMD_MINOR_RO	(0x40)
#define	AMD_MINOR_CTL	(0x80)
#define	AMD_CLONE_BIT	(0x10)

/*
 * Possible values for cpr_state, used for checkpoint/resume
 */
#define	AMD_SUSPENDED	1
#define	AMD_RESUMED	2

/*
 * These are the registers for the ADM79C30 chip.
 *
 * These names are not very descriptive, but they match the names chosen
 * by AMD.
 */
struct aud_79C30_chip {
	volatile char cr;	/* Command Register */
	volatile char dr;	/* Data Register */
	volatile char dsr1;	/* D-channel Status Register 1 */
	volatile char der;	/* D-channel Error Register */
	volatile char dctb;	/* D-channel Transmit Buffer */
	volatile char bbtb;	/* Bb Transmit Buffer */
	volatile char bctb;	/* Bc Transmit Buffer */
	volatile char dsr2;	/* D-channel Status Register 2 */
};

#define	ir	cr		/* Interrupt Register */
#define	bbrb	bbtb		/* Bb Receive Buffer */
#define	bcrb	bctb		/* Bc Receive Buffer */


/*
 * Device-dependent audio stream which encapsulates the generic audio
 * stream
 */
typedef struct amd_stream {
	/*
	 * Generic audio stream.  This MUST be the first structure member
	 */
	aud_stream_t as;

	/*
	 * Pointer to current command
	 */
	aud_cmd_t *cmdptr;

	/*
	 * Current statistics
	 */
	uint_t samples;
	uchar_t active;
	uchar_t error;
} amd_stream_t;


/*
 * This is the control structure used by the AMD79C30-specific driver
 * routines.
 */
typedef struct {
	/*
	 * Device-independent state--- MUST be first structure member
	 */
	aud_state_t distate;

	/*
	 * Address of the unit's registers
	 */
	struct aud_79C30_chip *chip;

	/*
	 * Device-dependent audio stream strucutures
	 */
	amd_stream_t input;
	amd_stream_t output;
	amd_stream_t control;

	/*
	 * Pointers to per-unit allocated memory so we can free it
	 * at detach time
	 */
	caddr_t *allocated_memory;
	size_t allocated_size;

	/*
	 * OS-dependent info
	 */
	dev_info_t *amd_dev;
	dev_info_t *dip;
	kmutex_t lock;		/* lolevel lock */

	/*
	 * The loopback ioctl sets the device to a funny state, so we need
	 * to know if we have to re-initialize the chip when the user closes
	 * the device.
	 */
	boolean_t init_on_close;
	int	cpr_state;
} amd_unit_t;


/*
 * Macros to derive control struct and chip addresses from the audio struct
 */
#define	UNITP(as)	((amd_unit_t *)((as)->distate->ddstate))
#define	CSR(as)		(UNITP(as)->chip)

#endif /* _KERNEL */


/*
 * These defines facilitate access to the indirect registers.  One of
 * these values is written to the command register, then the appropriate
 * number of bytes are written/read to/from the data register.
 *
 * The indirection values are formed by OR'ing together two quantities.
 * The first three bits of the indirection value specifies the chip
 * subsystem (audio processor, multiplexor, etc.) to be accessed.  AMP
 * calles these three bits the DCF, or destination code field.  The last
 * five bits specify the register within the subsystem.  AMD calls these
 * bits the OCF, or Operation Code Field.
 *
 * Note that the INIT_INIT value differs from the data sheet.  The data
 * sheet is wrong -- this is right.
 */
#define	AUDIO_PACK(reg, length)	(((reg) << 8) | (length))
#define	AUDIO_UNPACK_REG(x)	(((x) >> 8) & 0xff)
#define	AUDIO_UNPACK_LENGTH(x)	((x) & 0xff)

#define	AUDIO_INIT	0x20
#define	AUDIO_INIT_INIT	AUDIO_PACK((AUDIO_INIT | 0x01), 1)

#define	AUDIO_MUX	0x40
#define	AUDIO_MUX_MCR1	AUDIO_PACK(AUDIO_MUX | 0x01, 1)
#define	AUDIO_MUX_MCR2	AUDIO_PACK(AUDIO_MUX | 0x02, 1)
#define	AUDIO_MUX_MCR3	AUDIO_PACK(AUDIO_MUX | 0x03, 1)
#define	AUDIO_MUX_MCR4	AUDIO_PACK(AUDIO_MUX | 0x04, 1)

#define	AUDIO_MAP	0x60
#define	AUDIO_MAP_X	AUDIO_PACK(AUDIO_MAP | 0x01, 16)
#define	AUDIO_MAP_R	AUDIO_PACK(AUDIO_MAP | 0x02, 16)
#define	AUDIO_MAP_GX	AUDIO_PACK(AUDIO_MAP | 0x03, 2)
#define	AUDIO_MAP_GR	AUDIO_PACK(AUDIO_MAP | 0x04, 2)
#define	AUDIO_MAP_GER	AUDIO_PACK(AUDIO_MAP | 0x05, 2)
#define	AUDIO_MAP_STG	AUDIO_PACK(AUDIO_MAP | 0x06, 2)
#define	AUDIO_MAP_FTGR	AUDIO_PACK(AUDIO_MAP | 0x07, 2)
#define	AUDIO_MAP_ATGR	AUDIO_PACK(AUDIO_MAP | 0x08, 2)
#define	AUDIO_MAP_MMR1	AUDIO_PACK(AUDIO_MAP | 0x09, 1)
#define	AUDIO_MAP_MMR2	AUDIO_PACK(AUDIO_MAP | 0x0a, 1)
#define	AUDIO_MAP_ALL	AUDIO_PACK(AUDIO_MAP | 0x0b, 46)

/*
 * These are minimum and maximum values for various registers, in normal
 * units, *not* as written to the register.  Most are in db, but the FTGR
 * is in Hz.  The FTGR values are derived from the smallest and largest
 * representable frequency and do not necessarily reflect the true
 * capabilities of the chip.
 */
#define	AUDIO_MAP_GX_MIN	0
#define	AUDIO_MAP_GX_MAX	12
#define	AUDIO_MAP_GR_MIN	-12
#define	AUDIO_MAP_GR_MAX	0
#define	AUDIO_MAP_GER_MIN	-10
#define	AUDIO_MAP_GER_MAX	18
#define	AUDIO_MAP_STG_MIN	-18
#define	AUDIO_MAP_STG_MAX	0
#define	AUDIO_MAP_FTGR_MIN	16
#define	AUDIO_MAP_FTGR_MAX	3999
#define	AUDIO_MAP_ATGR_MIN	-18
#define	AUDIO_MAP_ATGR_MAX	0

/*
 * These are the bit assignment in the INIT register.
 *
 * There are several ways to specify dividing the clock by 2.  Since
 * there appears to be no difference between them, only one way is
 * defined.
 */
#define	AUDIO_INIT_BITS_IDLE		0x00
#define	AUDIO_INIT_BITS_ACTIVE		0x01
#define	AUDIO_INIT_BITS_NOMAP		0x20

#define	AUDIO_INIT_BITS_INT_ENABLED	0x00
#define	AUDIO_INIT_BITS_INT_DISABLED	0x04

#define	AUDIO_INIT_BITS_CLOCK_DIVIDE_2	0x00
#define	AUDIO_INIT_BITS_CLOCK_DIVIDE_1	0x08
#define	AUDIO_INIT_BITS_CLOCK_DIVIDE_4	0x10
#define	AUDIO_INIT_BITS_CLOCK_DIVIDE_3	0x20

#define	AUDIO_INIT_BITS_RECEIVE_ABORT	0x40
#define	AUDIO_INIT_BITS_TRANSMIT_ABORT	0x80

/*
 * The MUX (Multiplexor) connects inputs and outputs.  The MUX has three
 * registers.  Each register can specify two ports in the high and low
 * order nibbles.
 *
 * Here is an example.  To connect ports B1 and Ba, you write the
 * following word to a MUX register:
 *
 * 	AUDIO_MUX_PORT_B1 | (AUDIO_MUX_PORT_BA << 4)
 *
 * Connections are bidirectional, so it doesn't matter which port is
 * which nibble.
 */
#define	AUDIO_MUX_PORT_NONE	0x00
#define	AUDIO_MUX_PORT_B1	0x01	/* line interface unit */
#define	AUDIO_MUX_PORT_B2	0x02	/* line interface unit */
#define	AUDIO_MUX_PORT_BA	0x03	/* main audio processor */
#define	AUDIO_MUX_PORT_BB	0x04	/* microprocessor interface */
#define	AUDIO_MUX_PORT_BC	0x05	/* microprocessor interface */
#define	AUDIO_MUX_PORT_BD	0x06	/* sp channel 1 */
#define	AUDIO_MUX_PORT_BE	0x07	/* sp channel 2 */
#define	AUDIO_MUX_PORT_BF	0x08	/* sp channel 3 */

#define	AUDIO_MUX_MCR4_BITS_INT_ENABLE	0x08
#define	AUDIO_MUX_MCR4_BITS_INT_DISABLE	0x00
#define	AUDIO_MUX_MCR4_BITS_REVERSE_BB	0x10
#define	AUDIO_MUX_MCR4_BITS_REVERSE_BC	0x20

/*
 * These are the bit assignments for the mode registers MMR1 and MMR2.
 */
#define	AUDIO_MMR1_BITS_A_LAW		0x01
#define	AUDIO_MMR1_BITS_u_LAW		0x00
#define	AUDIO_MMR1_BITS_LOAD_GX		0x02
#define	AUDIO_MMR1_BITS_LOAD_GR		0x04
#define	AUDIO_MMR1_BITS_LOAD_GER	0x08
#define	AUDIO_MMR1_BITS_LOAD_X		0x10
#define	AUDIO_MMR1_BITS_LOAD_R		0x20
#define	AUDIO_MMR1_BITS_LOAD_STG	0x40
#define	AUDIO_MMR1_BITS_LOAD_DLB	0x80

#define	AUDIO_MMR2_BITS_AINA		0x00
#define	AUDIO_MMR2_BITS_AINB		0x01
#define	AUDIO_MMR2_BITS_EAR		0x00
#define	AUDIO_MMR2_BITS_LS		0x02
#define	AUDIO_MMR2_BITS_DTMF		0x04
#define	AUDIO_MMR2_BITS_TONE		0x08
#define	AUDIO_MMR2_BITS_RINGER		0x10
#define	AUDIO_MMR2_BITS_HIGH_PASS	0x20
#define	AUDIO_MMR2_BITS_AUTOZERO	0x40

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUDIO_79C30_H */
