/*
 * Copyright (c) 1991-1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DBRIVAR_H
#define	_SYS_DBRIVAR_H

#pragma ident	"@(#)dbrivar.h	1.47	97/10/31 SMI"

/*
 * This file describes the ATT T5900FC (DBRI) ISDN chip and declares
 * parameters and data structures used by the audio driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#if !defined(DBRI_NOSETPORT)
#define	DBRI_NOSETPORT		/* Make this the default for now */
#endif


/*
 * Driver constants
 */
#define	DBRI_IDNUM	(0x6175)
#define	DBRI_NAME	"dbri"
#define	DBRI_MINPSZ	(0)
#define	DBRI_MAXPSZ	(8188)
#define	DBRI_HIWAT	(57000)
#define	DBRI_LOWAT	(32000)
#define	DBRI_DEFAULT_GAIN (128)	/* gain initialization */
#define	DBRI_NUM_INTQS	(6)	/* number of interrupt queues */

#define	DBRI_MAX_BSIZE		(8176)
#define	DBRI_DEFAULT_BSIZE	(1024)
#define	DBRI_BSIZE_MODULO	(16)

/*
 * Device minor numbers, the clone device becomes either DBRI_AUDIO_RW or
 * DBRI_AUDIO_RO depending on the open flags. DBRI_AUDIOCTL has been
 * changed from 0x80 to allow more minor devices to be added without
 * kludges. This breaks compatibility with the previous audio.c.
 *
 * Note: When new channel types are added to dbri_conf.c they must also
 * be added here as a new minor device. A device special file also needs
 * to be made in the /dev directory. The minor device number in the
 * /dev/directory must be the number below << 2 as the unit number is
 * encoded in the bottom two bits.
 */
typedef	enum dbri_minor dbri_minor_t;
enum dbri_minor {
	DBRI_MINOR_NODEV = 0,

	DBRI_MINOR_AUDIO_RW = 1,
	DBRI_MINOR_AUDIO_RO = 2,
	DBRI_MINOR_AUDIOCTL = 3,

	DBRI_MINOR_TE_B1_RW = 4,
	DBRI_MINOR_TE_B1_RO = 5,
	DBRI_MINOR_TE_B2_RW = 6,
	DBRI_MINOR_TE_B2_RO = 7,
	DBRI_MINOR_TE_D = 8,
	DBRI_MINOR_TEMGT = 9,

	DBRI_MINOR_NT_B1_RW = 10,
	DBRI_MINOR_NT_B1_RO = 11,
	DBRI_MINOR_NT_B2_RW = 12,
	DBRI_MINOR_NT_B2_RO = 13,
	DBRI_MINOR_NT_D = 14,
	DBRI_MINOR_NTMGT = 15,

	DBRI_MINOR_DBRIMGT = 16,

	DBRI_MINOR_TE_D_TRACE = 37,
	DBRI_MINOR_NT_D_TRACE = 38
};


/*
 * We now have non-8-bit quantities for major and minor numbers.  The
 * encoding for the minor numbers is as follows:
 *
 *	...UUUUXccccccc
 *
 * Where:
 *   UUUU is the unit number (0..15)
 *   X is used to make a new number for self-clone opens
 *   ccccccc is the channel number (0..127)
 *
 * for a total of 12 bits used.
 */
#define	NCHANBITS	(8)
#define	NUNITBITS	(4)
#define	NCLONEBITS	(1)

#define	CHANOFFSET	(0)
#define	UNITOFFSET	(NCHANBITS)
#define	CLONEOFFSET	(UNITOFFSET + NUNITBITS)

#define	CHANMASK	(((1 << NCHANBITS) - 1) << CHANOFFSET)
#define	CLONEMASK	(((1 << NCLONEBITS) - 1) << CLONEOFFSET)
#define	UNITMASK	(((1 << NUNITBITS) - 1) << UNITOFFSET)

#define	MAKECHAN(x)	(((x) << CHANOFFSET) & CHANMASK)
#define	MAKECLONE(x)	(((x) << CLONEOFFSET) & CLONEMASK)
#define	MAKEUNIT(x)	(((x) << UNITOFFSET) & UNITMASK)
#define	GETCHAN(x)	(((x) & CHANMASK) >> CHANOFFSET)
#define	GETCLONE(x)	(((x) & CLONEMASK) >> CLONEOFFSET)
#define	GETUNIT(x)	(((x) & UNITMASK) >> UNITOFFSET)

#define	MINORDEV(dev)	GETCHAN(getminor(dev))
#define	MINORUNIT(dev)	GETUNIT(getminor(dev))
#define	dbrimakeminor(unit, min)	\
	(minor_t)(MAKEUNIT(unit) | MAKECHAN(min))
#define	dbrimakedev(maj, unit, min) \
	makedevice((maj), dbrimakeminor(unit, min))
#define	dbrimakeclonedev(maj, unit, min) \
	makedevice((maj), dbrimakeminor(unit, min) | MAKECLONE(1))


/*
 * DBRI channels
 */
typedef enum dbri_chan dbri_chan_t;
enum dbri_chan {
	DBRI_CHAN_INDIRECT = -4, /* Indirect data */
	DBRI_CHAN_FIXED = -3,	/* Fixed data */
	DBRI_CHAN_NONE = -2,	/* Invalid channel */
	DBRI_CHAN_HOST = -1,	/* Memory (DMA) Channel */

	/* TE channel defines */
	DBRI_TE_D_OUT = 0,
	DBRI_TE_D_IN,
	DBRI_TE_B1_OUT,
	DBRI_TE_B1_IN,
	DBRI_TE_B2_OUT,
	DBRI_TE_B2_IN,

	/* NT channel defines */
	DBRI_NT_D_OUT,
	DBRI_NT_D_IN,
	DBRI_NT_B1_OUT,
	DBRI_NT_B1_IN,
	DBRI_NT_B2_OUT,
	DBRI_NT_B2_IN,

	/* SBI channel defines */
	DBRI_AUDIO_OUT,
	DBRI_AUDIO_IN,

	/*
	 * Not user visible channels. (?)
	 * Used for data and control pipes dealing with device control.
	 */
	DBRI_AUDIOCTL,		/* actually MMCODEC control */
	DBRI_TEMGT,		/* generic ISDN control on TE */
	DBRI_NTMGT,		/* generic ISDN control on NT */
	DBRI_DBRIMGT,		/* DBRI specific control */

	DBRI_TE_D_TRACE,	/* TE D-channel trace stream */
	DBRI_NT_D_TRACE,	/* NT D-channel trace stream */

	DBRI_LAST_CHAN
	/* DO NOT PUT ANY ENTRIES AFTER THIS LAST DUMMY ENTRY */
};

/* Aliases ... */
#define	DBRI_CHAN_MEMORY	(DBRI_CHAN_HOST)

/* Constants */
#define	DBRI_NSTREAM	(DBRI_LAST_CHAN)	/* # of channels available */


/*
 * Data mode to be used by the pipe.  Note that this does not apply to
 * fixed pipes.
 */
typedef	enum dbri_mode dbri_mode_t;
enum dbri_mode {
	DBRI_MODE_UNKNOWN = -1,			/* mode predefined by def */
	DBRI_MODE_TRANSPARENT = DBRI_SDP_TRANSPARENT, /* Transparent mode */
	DBRI_MODE_HDLC = DBRI_SDP_HDLC,		/* HDLC mode */
	DBRI_MODE_HDLC_D = DBRI_SDP_TE_DCHAN,	/* HDLC-D mode */
	DBRI_MODE_SERIAL = DBRI_SDP_SERIAL,	/* Serial to serial */
	DBRI_MODE_FIXED = DBRI_SDP_FIXED	/* Fixed mode */
};


/*
 */
typedef enum dbri_clocking dbri_clocking_t;
enum dbri_clocking {
	DBRI_CLOCK_NONE,	/* No clocking specified */
	DBRI_CLOCK_DBRI,	/* TE or DBRI if TE not activated */
	DBRI_CLOCK_EXT		/* MMCODEC, ISDN SWITCH */
};


/*
 */
typedef struct dbri_chanmap dbri_chanmap_t;
struct dbri_chanmap {
	isdn_chan_t channel;	/* isdn port */
	dbri_chan_t out_channel; /* dbri transmit port */
	dbri_chan_t in_channel;	/* dbri receive port */
};


/*
 */
typedef struct dbri_chanformat dbri_chanformat_t;
struct dbri_chanformat {
	uint_t length;		/* physical bit length of time slot */
	uint_t llength;		/* logical bit length */
	dbri_mode_t mode;	/* pipe mode - default HDLC */
	int samplerate;		/* physical sample frames per second */
	int lsamplerate;	/* logical sample frames per second */
	int channels;		/* channels per sample frame */
	int numbufs;		/* Number of descriptors */
	int buffer_size;	/* Default buffer size */
	int encoding;		/* AUDIO_ENCODING */
#if 0
	dbri_clocking_t	clocking; /* XXX - Clock source */
#endif
}; /* dbri_chanformat_t */


/*
 */
typedef struct dbri_chantab dbri_chantab_t;
struct dbri_chantab {
	dbri_chan_t dchan;	/* dbri channel */
	isdn_chan_t ichan;	/* generic channel associated w/chan */

	uint_t bpipe;		/* base pipe of channel */
	uint_t cycle;		/* bit offset of time slot */
	int dir;		/* direction of this channel */
	dbri_chanformat_t *default_format; /* default format */
	dbri_chanformat_t **legal_formats; /* legal formats for channel */

	dbri_chan_t input_ch;	/* input data stream */
	dbri_chan_t output_ch;	/* output data stream */
	dbri_chan_t control_ch;	/* control data stream */

	boolean_t signals_okay;	/* can send signals up this dbri_stream */
	aud_streamtype_t audtype; /* audio type of stream */
	dbri_minor_t minordev;	/* dev minor #...well almost */
	isdn_interface_t isdntype; /* ISDN type of stream */

	char *name;		/* Name string for printfs */
	char *devname;		/* Name string for making dev nodes */
	char *config;		/* Configuration name string */
};


/*
 * Limit to ensure we can insert a JMP back to the head of list.  Note
 * that the JMP cmd is two words long.
 */
#define	DBRI_MAX_CMDS		(512)	/* max cmds in Q */
#define	DBRI_MAX_CMDSIZE	(3)	/* max # of words in a cmd */
#define	DBRI_END_CMDLIST	(DBRI_MAX_CMDS - DBRI_MAX_CMDSIZE - 2 - 1)


/*
 * typedefs for forward-referencing
 */
typedef struct dbri_stream dbri_stream_t;


/*
 * DBRI internal command queue structure
 *
 * XXX - should be in dbrireg.h
 */
typedef struct dbri_cmdq dbri_cmdq_t;
struct dbri_cmdq {
	uint_t cmdhp;		/* next command to add to list */
	uint_t cmd[DBRI_MAX_CMDS]; /* space for command list */
};


/*
 * Nice fluffy macro to sync a command queue in the given direction
 * (DDI_DMA_SYNC_FOR{DEV,CPU}). LEN is in words.
 */
#define	DBRI_SYNC_CMDQ(UNITP, LEN, DIR) \
	ddi_dma_sync((UNITP)->dma_handle, \
	    (off_t)(UNITP)->cmdqp - (off_t)(UNITP)->iopbkbase + \
	    (off_t)((UNITP)->cmdqp->cmdhp * sizeof (uint_t)), \
	    (LEN) * sizeof (uint_t), (DIR))


/*
 * XXX
 */
typedef struct dbri_primitives dbri_primitives_t;
struct dbri_primitives {
	uint_t berr;		/* detectible bit errors */
	uint_t ferr;		/* frame sync errors */
};


/*
 * Possible SBI Bus states
 */
typedef enum dbri_chi_state dbri_chi_state_t;
enum dbri_chi_state {
	CHI_NO_STATE,
	CHI_NO_DEVICES,
	CHI_IN_DATA_MODE,
	CHI_IN_CTRL_MODE,
	CHI_WAIT_DCB_LOW,
	CHI_WAIT_DCB_HIGH,
	CHI_GOING_TO_DATA_MODE,
	CHI_CODEC_CALIBRATION,
	CHI_GOING_TO_CTRL_MODE,
	CHI_POWERED_DOWN
};


typedef struct isdn_var isdn_var_t;
struct isdn_var {
	enum isdn_param_maint maint; /* Maintenance mode */
	enum isdn_param_asmb asmb; /* Activation State Machine type */
	uint_t norestart;	/* XXX - Enable for restart hack */
	uint_t power;		/* ISDN_PARAM_POWER_OFF or ..ON */
	uint_t enable;		/* if (enable && power) enable interface */
	int t101;		/* NT ASM */
	int t102;		/* NT ASM */
	int t103;		/* TE ASM */
	int t104;		/* TE ASM */
};


/*
 * Status for TE, NT, and SBI interfaces
 */

typedef struct dbri_chi_status dbri_chi_status_t;
struct dbri_chi_status {
	/*
	 * Current CHI state
	 */
	dbri_chi_state_t chi_state;

	/*
	 * Software copies of DBRI commands
	 */
	uint_t chi_cmd;		/* CHI command */
	uint_t cdec_cmd;	/* CDEC command */
	uint_t cdm_cmd;		/* CDM command */

	/*
	 * CHI State flags
	 */
	boolean_t chi_codec_rptd;
	boolean_t chi_wait_for_init;
	boolean_t chi_use_dummy_pipe;

	/*
	 * Current CHI/MMCODEC format
	 */
	dbri_clocking_t chi_dbri_clock;	/* next clock source */
	dbri_clocking_t chi_clock;	/* current clock source */
	uint_t chi_format_refcnt;

	/*
	 * Current I/O port configuration.
	 */
	boolean_t have_speaker;	/* hardware supports speaker */
	boolean_t have_linein;	/* hardware supports line-in */
	boolean_t have_lineout;	/* hardware supports line-out */
	boolean_t have_microphone; /* hardware supports microphone */
	boolean_t have_headphone;  /* hardware supports headphone */

	/*
	 * Misc.
	 */
	uint_t chi_nofs;	/* # of CHIL nofs interrupts */
	boolean_t pal_present;	/* B_TRUE if a PAL is present */
	int mmcodec_ts_start;	/* cycle of the CODEC */
	uint_t chi_dd_val;	/* CHI dummy data value */
	uint_t savedpal;
}; /* dbri_chi_status */

typedef struct dbri_bri_status dbri_bri_status_t;
struct dbri_bri_status {
	/*
	 * Interface type
	 */
	boolean_t is_te;	/* XXX - not used right now */

	/*
	 * Software copies of DBRI commands and state
	 */
	uint_t ntte_cmd;
	struct dbri_code_sbri sbri;	/* SBRI interrupt TE status */
	boolean_t fact;			/* force activation */
	int dbri_sanity_interval;	/* timer service interval, 0==disable */

	/*
	 * dbri_f - Accept an F bit which is not a bipolar violation
	 * after an errored frame.
	 */
	boolean_t dbri_f;

	/*
	 * dbri_nbf - Number of bad frames required to loose framing.
	 * NBF must be 2 or 3.
	 */
	int dbri_nbf;

	/*
	 * Misc.
	 */
	dbri_primitives_t primitives; /* primitives for TE */
	isdn_interface_info_t i_info; /* ISDN state and error information */
	isdn_var_t i_var;	/* driver's ISDN private variables */
}; /* dbri_bri_status */


typedef struct dbri_serial_status dbri_serial_status_t;
struct dbri_serial_status {
	dbri_bri_status_t te;
	dbri_bri_status_t nt;
	dbri_chi_status_t chi;
}; /* struct dbri_serial_status */

#define	DBRI_CHI_STATUS_P(unitp) (&(unitp)->ser_sts.chi)
#define	DBRI_TE_STATUS_P(unitp) (&(unitp)->ser_sts.te)
#define	DBRI_NT_STATUS_P(unitp) (&(unitp)->ser_sts.nt)


/*
 * Frame lengths
 */
#define	CHI_DATA_MODE_LEN	128
#define	CHI_CTRL_MODE_LEN	256


/*
 * Status for 16 long pipes and 16 short pipes
 */
typedef struct dbri_pipe dbri_pipe_t;
struct dbri_pipe {
	dbri_stream_t *ds;	/* stream associated w/ port */
	uint_t pipeno;		/* DBRI pipe number */
	dbri_mode_t mode;	/* SDP command mode */

	/*
	 * Software copies of DBRI commands
	 */
	uint_t sdp;		/* setup data pipe command bits */
	uint_t ssp;		/* set short pipe command bits */
	dbri_dts_cmd_t dts;	/* define time slot command bits */

	/*
	 * If ismonitor is true, then monitorpipep points to the
	 * underlying pipe.  If false, then monitorpipep points
	 * to the monitor pipe.
	 */
	boolean_t ismonitor;	/* If this pipe *is* the monitor pipe */
	dbri_pipe_t *monitorpipep; /* This pipe's monitor pipe */

	/*
	 * A two-way connection can only be created by making
	 * two one-way connections.  otherpipep will point to
	 * the other pipe used to make a two-way connection
	 * (since given a pipe it is otherwise impossible to
	 * reconstruct how a connection was created).
	 */
	dbri_pipe_t *otherpipep; /* other pipe associated with a connection */

	struct {
		dbri_chan_t dchan;
		dbri_pipe_t *basepipep;
		uint_t cycle;
		uint_t length;
	} source, sink;

	/*
	 * Pipe's format
	 */
	dbri_chanformat_t format;

	/*
	 * refcnt is the number of times this pipe has been "linked" into
	 * DBRI's pipe list.
	 */
	int refcnt;
	boolean_t allocated;	/* unused pipe in pipe pool */
};

#define	PIPENO(pipep) (pipep->pipeno)


/*
 * Reserved pipes
 */
#define	DBRI_PIPE_TE_D_IN	(0)
#define	DBRI_PIPE_TE_D_OUT	(1)
#define	DBRI_PIPE_NT_D_OUT	(2)
#define	DBRI_PIPE_NT_D_IN	(3)
#define	DBRI_PIPE_LONGFREE	(4)	/* initial start of free list */
#define	DBRI_PIPE_CHI		(16)	/* base pipe for CHI */
/*
 * These next few dedicated short pipes are needed as they contain s/w
 * status.  They should never be used for any other purpose.  If any
 * additional short pipes are needed for status insert them before the
 * define for the freelist and modify the start of the freelist.
 */
#define	DBRI_PIPE_DM_T_5_8	(17)	/* Data Mode tx TS's 5-8 */
#define	DBRI_PIPE_DM_R_5_6	(18)	/* Data Mode rx TS's 5-6 */
#define	DBRI_PIPE_DM_R_7_8	(19)	/* Data Mode rx TS's 7-8 */
#define	DBRI_PIPE_CM_T_1_4	(20)	/* Cntrl Mode tx TS's 1-4 */
#define	DBRI_PIPE_CM_R_1_2	(21)	/* Cntrl Mode rx TS's 1-2 */
#define	DBRI_PIPE_CM_R_3_4	(22)	/* Cntrl Mode rx TS's 3-4 */
#define	DBRI_PIPE_SB_PAL_T	(23)	/* SBox ID PAL transmit */
#define	DBRI_PIPE_SB_PAL_R	(24)	/* SBox ID PAL receive */
#define	DBRI_PIPE_CM_R_7	(25)	/* To read MMCODEC Manu info */
#define	DBRI_PIPE_DUMMYDATA	(26)	/* To transmit dummy data */
#define	DBRI_PIPE_SHORTFREE	(27)	/* initial start of free list */
#define	DBRI_MAX_PIPES		(32)	/* total number of pipes */
					/* also used for invalid pipe */
#define	DBRI_BAD_PIPE	DBRI_MAX_PIPES	/* total number of pipes */

/*
 * DBRI pipe types
 */
#define	DBRI_LONGPIPE	(0)	/* Long pipe type */
#define	DBRI_SHORTPIPE	(1)	/* Short pipe type */

#define	ISPIPEINUSE(a)	((a)->refcnt != 0)
#define	ISHWCONNECTED(as) (ISDATASTREAM(as) && \
	(AsToDs(as)->pipep != NULL))
#define	ISTEDCHANNEL(as) ((PIPENO(AsToDs(as)->pipep) == DBRI_PIPE_TE_D_IN) || \
	(PIPENO(AsToDs(as)->pipep) == DBRI_PIPE_TE_D_OUT))
#define	ISNTDCHANNEL(as) ((PIPENO(AsToDs(as)->pipep) == DBRI_PIPE_NT_D_IN) || \
	(PIPENO(AsToDs(as)->pipep) == DBRI_PIPE_NT_D_OUT))
#define	ISXMITDIRECTION(as)			\
	(AsToDs(as)->pipep->sdp & DBRI_SDP_D)

#define	DBRI_DIRECTION_NONE	(0x0)	/* Not input or output */
#define	DBRI_DIRECTION_IN	(0x1)	/* input direction */
#define	DBRI_DIRECTION_OUT	(0x2)	/* output direction */


/*
 * Dbri message descriptor
 */
typedef struct dbri_cmd_md dbri_md_t;
struct dbri_cmd_md {
	union dbri_md _md;
};


/*
 * Each message descriptor MUST start on a 16-byte aligned address for
 * DBRI V5 silicon.  This is a hardware requirement!
 */
#define	DBRI_MD_ALIGN	(16)


/*
 * Nice fluffy macro to sync a message descriptor in the given direction
 * (DDI_DMA_SYNC_FOR{DEV,CPU}).
 */
#define	DBRI_SYNC_MD(UNITP, MDP, DIR) \
	ddi_dma_sync((UNITP)->dma_handle, \
	    (off_t)(MDP) - (off_t)(UNITP)->iopbkbase, \
	    sizeof (dbri_md_t), (DIR))

/*
 * Command list status and info within a specific stream
 */
typedef struct dbri_cmd	dbri_cmd_t;
struct dbri_cmd {
	/* DI Audio */
	aud_cmd_t cmd;		/* generic driver's generic command */

	/* DD Audio */
	dbri_cmd_t *nextio;	/* next IO command */
	dbri_md_t *md;		/* pointer to message descriptor */
	dbri_md_t *md2;		/* pointer to fake message descriptor */

	/* OS Dependent */
	ddi_dma_handle_t buf_dma_handle;
	ddi_dma_cookie_t buf_dma_cookie;
};

#define	txmd	md->_md.tx
#define	rxmd	md->_md.rx
#define	words	md->_md._words

#define	txmd2	md2->_md.tx
#define	rxmd2	md2->_md.rx
#define	words2	md2->_md._words

/*
 * Nice fluffy macro to sync a message descriptor in the given direction
 * (DDI_DMA_SYNC_FOR{DEV,CPU}).
 */
#define	DBRI_SYNC_BUF(DCP, SIZE, DIR) \
	ddi_dma_sync((DCP)->buf_dma_handle, 0, (SIZE), (DIR))


typedef struct dbri_zerobuf dbri_zerobuf_t;
struct dbri_zerobuf {
	uchar_t ulaw[4];
	uchar_t alaw[4];
	uchar_t linear[4];
};

/*
 * XXX
 */
typedef struct dbri_stat dbri_stat_t;
struct dbri_stat {
	isdn_chan_t channel;
	uint_t dma_underflow;
	uint_t dma_overflow;
	uint_t abort_error;
	uint_t crc_error;
	uint_t badbyte_error;
	uint_t crc_error_soft;
	uint_t recv_eol;
	uint_t recv_error_octets;
};


/*
 * Stream specific status info within a particular controller
 */
struct dbri_stream {
	/*
	 * Device-Independent portion... this MUST be first!
	 */
	aud_stream_t as;

	/*
	 * Device's command list
	 */
	dbri_cmd_t *cmdptr;	/* cmd chain list head */
	dbri_cmd_t *cmdlast;	/* last IO command in list */

	/*
	 * Per-stream device-dependent information
	 */
	dbri_pipe_t *pipep;	/* pipe associate with this stream */
	dbri_chantab_t *ctep;	/* chan tab entry for this stream */

	/*
	 * The per-stream dbri_format pointer points to a *default*
	 * format for the stream.  The *current* format should always
	 * be obtained through the dbri_stream's pipe pointer.
	 */
	dbri_chanformat_t *dfmt; /* From ISDN_SET_FORMAT or default */

	boolean_t audio_uflow;	/* audio over/under-flow */
	boolean_t last_flow_error; /* XXX - DBRI bug #3034 */

	uint_t samples;		/* number samples converted */
	struct timeval last_smpl_update;
	dbri_stat_t d_stats;	/* DBRI statistics */
	char config[MAX_AUDIO_DEV_LEN];

	struct isdn_io_stats iostats;
	enum isdn_iostate iostate;
};

#define	AsToDs(as) ((dbri_stream_t *)(as))
#define	DsToAs(ds) (&(ds)->as)
#define	DChanToDs(unitp, dchan) (&(unitp)->ioc[dchan])
#define	DChanToAs(unitp, dchan) (&(unitp)->ioc[dchan].as)
#define	DsToUnitp(ds)	((dbri_unit_t *)(ds)->as.distate->ddstate)
#define	AsToUnitp(ds)	((dbri_unit_t *)(as)->distate->ddstate)
#define	DsDisabled(ds)	((ds)->ctep == NULL ? B_TRUE : B_FALSE)


/*
 * This structure defines one endpoint of a DBRI connection
 */
typedef struct dbri_conn_endpoint dbri_conn_endpoint_t;
struct dbri_conn_endpoint {
	dbri_chan_t dchan;	/* DBRI channel */
	dbri_chantab_t *ctep;	/* DBRI channel table entry pointer */
};


/*
 * This structure contains all information necessary to create
 * a one-way connection.
 */
typedef struct dbri_conn_oneway dbri_conn_oneway_t;
struct dbri_conn_oneway {
	dbri_stream_t *ds;	/* Data stream if any (DBRI_CHAN_HOST) */
	dbri_pipe_t *pipep;
	dbri_conn_endpoint_t sink;
	dbri_conn_endpoint_t source;
	dbri_chanformat_t format;
};

/*
 * Structure containing enough information to complete an ISDN connection
 * request.
 */
typedef struct dbri_conn_request dbri_conn_request_t;
struct dbri_conn_request {
	boolean_t already_busy;	/* don't need to mark channels busy */
	boolean_t has_context;	/* open has it, ioctl don't */
	dbri_stream_t *ds;	/* controlling dbri_stream */
	boolean_t oneway_valid;
	dbri_conn_oneway_t oneway; /* primary direction */
	boolean_t otherway_valid;
	dbri_conn_oneway_t otherway; /* other direction if ISDN_PATH_TWOWAY */
	int errno;		/* 0 if success, otherwise contains error */
};


/*
 * DBRI Interrupt queue state structure
 */
typedef struct dbri_intr dbri_intr_t;
struct dbri_intr {
	dbri_intq_t *intq_bp;	/* interrupt queue base ptr */
	dbri_intq_t *curqp;	/* interrupt qptr to current qstruct */
	int off;		/* offset into qstruct */

	/* OS Dependent */
	ddi_dma_cookie_t dma_cookie;
};


/*
 * Nice fluffy macro to DMA synchronize the current interrupt queue for
 * the given DBRI unit (it only makes sense to sync for the CPU, not the
 * device, so the direction is not an argument).  This macro only syncs
 * the current word in the interrupt queue.  It MUST be called before
 * each interrupt queue entry is read!
 */
#define	DBRI_SYNC_CURINTQ(UNITP) \
	ddi_dma_sync((UNITP)->dma_handle, \
	    (off_t)(UNITP)->intq.curqp - (off_t)(UNITP)->iopbkbase + \
	    (off_t)((UNITP)->intq.off * sizeof (uint32_t)), \
	    sizeof (uint32_t), DDI_DMA_SYNC_FORCPU)


/*
 * Controller specific status and info on a per controller basis
 */
typedef struct dbri_unit dbri_unit_t;
struct dbri_unit {
	aud_state_t distate;	/* HW Independant chip State */

	/*
	 * Physical unit information
	 */
	char version;		/* DBRI version letter (SUNW,DBRI%c) */
	boolean_t isdn_disabled; /* ISDN interfaces disabled */

	dbri_reg_t *regsp;	/* addr of device registers */
	dbri_intr_t intq;	/* base of entire intq struct */
	dbri_cmdq_t *cmdqp;	/* DBRI chip command qptr */

	/*
	 * Software state
	 */
	dbri_stream_t ioc[DBRI_NSTREAM]; /* Per stream info */
	dbri_pipe_t ptab[DBRI_NPIPES]; /* pipe status table */
	dbri_serial_status_t ser_sts; /* serial status table */
	dbri_zerobuf_t *zerobuf;

	/*
	 * Memory usage information
	 */
	caddr_t iopbkbase;	/* allocated iopb space for kernel */
	uintptr_t iopbiobase;	/* allocated iopb space for io */
	size_t iopbsize;	/* size of allocated iopb space */
	ddi_dma_handle_t dma_handle; /* DMA mapping for IOPB space */

	caddr_t cmdbase;	/* base of allocated kernel memory */
	size_t cmdsize;		/* size of allocated kernel memory */

	/*
	 * Driver state
	 */
	boolean_t openinhibit;	/* FALSE if opens on device allowed */
	boolean_t initialized;	/* TRUE if chip initialized */
	boolean_t init_cmdq;	/* TRUE if cmdq already init */
	int keep_alive_running; /* Sanity reference count if NT|TE used */
	boolean_t suspended;	/* TRUE if driver suspended */

	/*
	 * Timeout state (id's returned by timeout)
	 */
	timeout_id_t tetimer_id;	/* dbri_te_timer() */
	timeout_id_t nttimer_t101_id;	/* dbri_nttimer_t101() */
	timeout_id_t nttimer_t102_id;	/* dbri_nttimer_t102() */
	timeout_id_t keepalive_id;	/* dbri_keepalive() */
	timeout_id_t mmcvolume_id;	/* mmcodec_volume_timer() */
	timeout_id_t mmcwatchdog_id;	/* mmcodec_watchdog() */
	timeout_id_t mmcdataok_id;	/* mmcodec_data_mode_ok() */
	timeout_id_t mmcstartup_id;	/* mmcodec_startup_audio() */

	/*
	 * SunDDI interface
	 */
	kmutex_t lock;		/* per-unit lock */
	int instance;
	dev_info_t *dip;
	ddi_iblock_cookie_t icookie;
	kcondvar_t audiocon_cv;
	int ddi_burstsizes_avail;
};

#define	DBRI_IOPBIOADDR(unitp, a) \
	((unitp)->iopbiobase + \
	    ((uintptr_t)(a) - (uintptr_t)(unitp)->iopbkbase))

#define	DBRI_IOPBKADDR(unitp, a) \
	((unitp)->iopbkbase + \
	    ((uintptr_t)(a) - (uintptr_t)(unitp)->iopbiobase))

#define	LOCK_UNIT(unitp)	mutex_enter(&(unitp)->lock)
#define	UNLOCK_UNIT(unitp)	mutex_exit(&(unitp)->lock)
#define	ASSERT_UNITLOCKED(unitp) \
	ASSERT(MUTEX_HELD(&(unitp)->lock))


/*
 * Macro to convert milliseconds into clock ticks for use by timeout.
 * There is guaranteed to be at least one tick.
 */

#define	TICKS(x) drv_usectohz((x) > 0 ? ((x) * 1000) : 1)

/*
 * Parameters passed back in AUDIO_GETDEV ioctl
 */
#define	DBRI_DEV_NAME	"SUNW,dbri"
#define	DBRI_DEV_SPKRBOX	"speakerbox"
#define	DBRI_DEV_ONBRD1		"onboard1"
#define	DBRI_DEV_ISDN_B		"isdn_b"

/*
 * dbri_dts action argument
 */
typedef enum dbri_dts_action dbri_dts_action_t;
enum dbri_dts_action {
	DBRI_DTS_NONE = 0,
	DBRI_DTS_INSERT,
	DBRI_DTS_MODIFY,
	DBRI_DTS_DELETE
};


/*
 * DBRI driver global variables
 */
extern ddi_dma_lim_t dbri_dma_limits;
extern struct as kas;

extern int Dbri_nstreams;
extern int Default_DBRI_nbf;
extern int Default_DBRI_f;
extern int Default_T101_timer;
extern int Default_T102_timer;
extern int Default_T103_timer;
extern int Default_T104_timer;
extern enum isdn_param_asmb Default_asmb;
extern int Default_power;
extern int Keepalive_timer;
extern boolean_t Dbri_panic;
extern void *Dbri_state;	/* SunDDI soft state pointer */
extern int Dbri_keepcnt;

/*
 * XXX - "reasons" for delayed execution. Maybe these are not needed.
 */
#define	DBRIEV_SBI_CONFIG	(1)


/*
 * DBRI-specific function prototypes
 */
#ifdef __STDC__
extern boolean_t dbri_find_oldnext(dbri_unit_t *, dbri_pipe_t *, int dir,
    dbri_pipe_t **, dbri_pipe_t **, dbri_pipe_t **);
extern void dbri_setup_sdp(dbri_unit_t *, dbri_pipe_t *,
    dbri_chantab_t *);
extern void dbri_setup_ntte(dbri_unit_t *, dbri_chan_t);
extern boolean_t dbri_disconnect_pipe(dbri_unit_t *, aud_stream_t *,
    dbri_pipe_t *);
extern dbri_pipe_t *dbri_pipe_allocate(dbri_unit_t *, dbri_conn_oneway_t *);
extern void dbri_pipe_free(dbri_pipe_t *);
extern boolean_t dbri_basepipe_init(dbri_unit_t *unitp, dbri_chan_t dc,
    int *error);
extern boolean_t dbri_basepipe_fini(dbri_unit_t *unitp, dbri_chan_t dc);
extern boolean_t dbri_pipe_insert(dbri_unit_t *, dbri_pipe_t *);
extern boolean_t dbri_pipe_remove(dbri_unit_t *, dbri_pipe_t *);
extern dbri_pipe_t *dbri_findshortpipe(dbri_unit_t *, dbri_chan_t,
    dbri_chan_t);
extern boolean_t dbri_is_dbri_audio_chan(dbri_chan_t);
extern dbri_chan_t dbri_base_p_to_ch(uint_t);
extern boolean_t dbri_isdn_connection_create(dbri_stream_t *,
    isdn_conn_req_t *, aud_return_t *, int *);
extern boolean_t dbri_isdn_connection_destroy(dbri_stream_t *,
    isdn_conn_req_t *, aud_return_t *, int *);
extern boolean_t dbri_isdn_set_format(dbri_stream_t *, isdn_format_req_t *,
    aud_return_t *, int *);
extern boolean_t dbri_isdn_get_format(dbri_stream_t *, isdn_format_req_t *,
    int *);
extern boolean_t dbri_isdn_get_config(dbri_stream_t *, isdn_conn_tab_t *);
extern boolean_t dbri_set_format(dbri_unit_t *, dbri_chan_t,
    dbri_chanformat_t *);
extern boolean_t dbri_set_default_format(dbri_stream_t *);
extern boolean_t dbri_mmcodec_connect(dbri_unit_t *, dbri_chan_t);
extern void dbri_mmcodec_disconnect(dbri_unit_t *, dbri_chan_t);
extern void dbri_bri_down(dbri_unit_t *, isdn_interface_t);
extern int dbri_bri_func(dbri_unit_t *, isdn_interface_t, int (*)(),
    void *);
extern void dbri_bri_up(dbri_unit_t *, isdn_interface_t);
extern void dbri_chil_intr(dbri_unit_t *, dbri_intrq_ent_t);
extern boolean_t dbri_chan_activated(dbri_stream_t *, dbri_chan_t,
    dbri_chan_t);
extern void dbri_command(dbri_unit_t *, dbri_chip_cmd_t);
extern boolean_t dbri_stream_connection_create(dbri_stream_t *, int,
    int *);
extern void dbri_config_queue(dbri_stream_t *);
extern void dbri_disable_nt(dbri_unit_t *);
extern void dbri_disable_te(dbri_unit_t *);
extern void dbri_dump_state(dbri_unit_t *);
extern void dbri_enable_nt(dbri_unit_t *);
extern void dbri_enable_te(dbri_unit_t *);
extern void dbri_error_msg(dbri_stream_t *, uchar_t);
extern void dbri_fxdt_intr(dbri_unit_t *, dbri_intrq_ent_t);
extern void dbri_hold_f3(dbri_unit_t *);
extern void dbri_hold_g1(dbri_unit_t *);
extern void dbri_exit_g1(dbri_unit_t *);
extern void dbri_initchip(dbri_unit_t *);
extern void dbri_remove_timeouts(dbri_unit_t *);
extern uint_t dbri_inport(dbri_unit_t *, uint_t);
extern uint_t dbri_intr(caddr_t arg);
extern boolean_t dbri_isdn_get_param(aud_stream_t *, isdn_param_t *,
    aud_return_t *, int *);
extern boolean_t dbri_isdn_set_param(aud_stream_t *, isdn_param_t *,
    aud_return_t *, int *);
extern uint_t dbri_monitor_gain(dbri_unit_t *, uint_t);
extern void dbri_nt_mph_deactivate_req(dbri_unit_t *);
extern void dbri_nt_ph_activate_req(aud_stream_t *);
extern void dbri_nttimer_t101(void *);
extern void dbri_nttimer_t102(void *);
extern int dbri_open(queue_t *, dev_t *, int, int, cred_t *);
extern uint_t dbri_outport(dbri_unit_t *, uint_t);
extern uchar_t dbri_output_muted(dbri_unit_t *, uchar_t);
extern void dbri_panic(dbri_unit_t *, const char *);
extern uint_t dbri_play_gain(dbri_unit_t *, uint_t, uchar_t);
extern dbri_chanmap_t *dbri_port_table_entry(isdn_chan_t);
extern int dbri_power(dbri_unit_t *, isdn_interface_t, uint_t);
extern void dbri_primitive_mph_ai(aud_stream_t *);
extern void dbri_primitive_mph_di(aud_stream_t *);
extern void dbri_primitive_mph_ei1(aud_stream_t *);
extern void dbri_primitive_mph_ei2(aud_stream_t *);
extern void dbri_primitive_mph_ii_c(aud_stream_t *);
extern void dbri_primitive_mph_ii_d(aud_stream_t *);
extern void dbri_primitive_ph_ai(aud_stream_t *);
extern void dbri_primitive_ph_di(aud_stream_t *);
extern uint_t dbri_record_gain(dbri_unit_t *, uint_t, uchar_t);
extern void dbri_setup_dts(dbri_unit_t *, dbri_pipe_t *,
    dbri_chantab_t *);
extern void dbri_start(aud_stream_t *);
extern void dbri_stop(aud_stream_t *);
extern void dbri_te_ph_activate_req(aud_stream_t *);
extern void dbri_te_unplug(dbri_unit_t *);
extern boolean_t dbri_stream_connection_destroy(dbri_stream_t *);
extern uint_t get_aud_pipe_cycle(dbri_unit_t *, dbri_pipe_t *, dbri_chan_t);
extern uint_t get_aud_pipe_length(dbri_unit_t *, dbri_pipe_t *,
    dbri_chan_t);
extern void mmcodec_setup_ctrl_mode(dbri_unit_t *);
extern int mmcodec_check_audio_config(dbri_unit_t *, uint_t, uint_t,
    uint_t, uint_t);
extern int mmcodec_getdev(dbri_unit_t *);
extern void mmcodec_reset(dbri_unit_t *);
extern void mmcodec_set_audio_config(dbri_stream_t *);
extern void mmcodec_startup_audio(void *);
extern dbri_chantab_t *dbri_dchan_to_ctep(dbri_chan_t);
extern dbri_chantab_t *dbri_ichan_to_ctep(isdn_chan_t, int);
extern dbri_chantab_t *dbri_chantab_first(void);
extern dbri_chantab_t *dbri_chantab_next(dbri_chantab_t *);
extern dbri_chan_t dbri_itod_chan(isdn_chan_t, int);
extern boolean_t dbri_isowner(dbri_stream_t *, isdn_chan_t);
extern boolean_t dbri_format_eq(dbri_chanformat_t *, isdn_format_t *);
extern dbri_chanformat_t *dbri_itod_fmt(dbri_unit_t *, dbri_chantab_t *,
    isdn_format_t *);
extern boolean_t dbri_dtoi_fmt(dbri_chanformat_t *, isdn_format_t *);
extern boolean_t dbri_dts(dbri_unit_t *, dbri_pipe_t *, dbri_dts_action_t);
extern boolean_t dbri_isvalidformat(dbri_chan_t, isdn_format_t *);
extern dbri_pipe_t *dbri_basepipe(dbri_unit_t *, dbri_chan_t);
extern void dbri_keep_alive(void *);
extern void mmcodec_insert_dummydata(dbri_unit_t *);

#else  /* !__STDC__ */

extern boolean_t dbri_find_oldnext();
extern void dbri_setup_sdp();
extern void dbri_setup_ntte();
extern boolean_t dbri_disconnect_pipe();
extern dbri_pipe_t *dbri_pipe_allocate();
extern void dbri_pipe_free();
extern boolean_t dbri_basepipe_init();
extern boolean_t dbri_basepipe_fini();
extern boolean_t dbri_pipe_insert();
extern boolean_t dbri_pipe_remove();
extern dbri_pipe_t *dbri_findshortpipe();
extern boolean_t dbri_is_dbri_audio_chan();
extern dbri_chan_t dbri_base_p_to_ch();
extern boolean_t dbri_isdn_connection_create();
extern boolean_t dbri_isdn_connection_destroy();
extern boolean_t dbri_isdn_set_format();
extern boolean_t dbri_isdn_get_format();
extern boolean_t dbri_isdn_get_config();
extern boolean_t dbri_set_format();
extern boolean_t dbri_set_default_format();
extern boolean_t dbri_mmcodec_connect();
extern void dbri_mmcodec_disconnect();
extern void dbri_bri_down();
extern int dbri_bri_func();
extern void dbri_bri_up();
extern void dbri_chil_intr();
extern boolean_t dbri_chan_activated();
extern void dbri_command();
extern boolean_t dbri_stream_connection_create();
extern void dbri_config_queue();
extern void dbri_disable_nt();
extern void dbri_disable_te();
extern void dbri_dump_state();
extern void dbri_enable_nt();
extern void dbri_enable_te();
extern void dbri_error_msg();
extern void dbri_fxdt_intr();
extern void dbri_hold_f3();
extern void dbri_hold_g1();
extern void dbri_exit_g1();
extern void dbri_initchip();
extern void dbri_remove_timeouts();
extern uint_t dbri_inport();
extern uint_t dbri_intr();
extern boolean_t dbri_isdn_get_param();
extern boolean_t dbri_isdn_set_param();
extern uint_t dbri_monitor_gain();
extern void dbri_nt_mph_deactivate_req();
extern void dbri_nt_ph_activate_req();
extern void dbri_nttimer_t101();
extern void dbri_nttimer_t102();
extern int dbri_open();
extern uint_t dbri_outport();
extern uchar_t dbri_output_muted();
extern void dbri_panic();
extern uint_t dbri_play_gain();
extern dbri_chanmap_t *dbri_port_table_entry();
extern int dbri_power();
extern void dbri_primitive_mph_ai();
extern void dbri_primitive_mph_di();
extern void dbri_primitive_mph_ei1();
extern void dbri_primitive_mph_ei2();
extern void dbri_primitive_mph_ii_c();
extern void dbri_primitive_mph_ii_d();
extern void dbri_primitive_ph_ai();
extern void dbri_primitive_ph_di();
extern uint_t dbri_record_gain();
extern void dbri_setup_dts();
extern void dbri_start();
extern void dbri_stop();
extern void dbri_te_ph_activate_req();
extern void dbri_te_unplug();
extern boolean_t dbri_stream_connection_destroy();
extern uint_t get_aud_pipe_cycle();
extern uint_t get_aud_pipe_length();
extern void mmcodec_setup_ctrl_mode();
extern int mmcodec_check_audio_config();
extern int mmcodec_getdev();
extern void mmcodec_reset();
extern void mmcodec_set_audio_config();
extern void mmcodec_startup_audio();
extern dbri_chantab_t *dbri_dchan_to_ctep();
extern dbri_chantab_t *dbri_ichan_to_ctep();
extern dbri_chantab_t *dbri_chantab_first();
extern dbri_chantab_t *dbri_chantab_next();
extern dbri_chan_t dbri_itod_chan();
extern boolean_t dbri_isowner();
extern boolean_t dbri_format_eq();
extern dbri_chanformat_t *dbri_itod_fmt();
extern boolean_t dbri_dtoi_fmt();
extern boolean_t dbri_dts();
extern boolean_t dbri_isvalidformat();
extern dbri_pipe_t *dbri_basepipe();
extern void dbri_keep_alive();
extern void mmcodec_insert_dummydata();

#endif /* __STDC__ */

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DBRIVAR_H */
