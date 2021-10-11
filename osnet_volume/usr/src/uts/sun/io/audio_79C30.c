/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_79C30.c	1.49	99/11/03 SMI"

/*
 * AUDIO Chip driver - for AMD AM79C30A
 *
 * The heart of this driver is its ability to convert sound to a bit
 * stream and back to sound.  The chip itself supports lots of telephony
 * functions, but this driver doesn't.
 *
 * The basic facts:
 * 	- The chip has a built in 8000Hz DAC and ADC
 * 	- When it is active, it interrupts every 125us (8000 times a second).
 * 	- The digital representation is 8-bit u-law by default.
 *	  The high order bit is a sign bit, the low order seven bits
 *	  encode amplitude, and the entire 8 bits are inverted.
 * 	- The driver does not currently support 8-bit A-law encoding.
 * 	- We use the chip's Bb channel for both reading and writing.
 *
 *	NOTE: This driver depends on misc/diaudio
 */

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/file.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/systeminfo.h>
#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/audio_79C30.h>
#include <sys/audiodebug.h>

/*
 * External routine used with the fast trap handler.  We later check to
 * see if this symbol has the value 0 since it is weakly bound.  This may
 * be linker-dependent.
 */
#pragma	weak impl_setintreg_on
extern int impl_setintreg_on();


/*
 * Local routines
 */
static int audio_79C30_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int audio_79C30_attach(dev_info_t *, ddi_attach_cmd_t);
static int audio_79C30_detach(dev_info_t *, ddi_detach_cmd_t);
static int audio_79C30_identify(dev_info_t *);
static int audio_79C30_open(queue_t *, dev_t *, int, int, cred_t *);
static void audio_79C30_close(aud_stream_t *);
static aud_return_t audio_79C30_ioctl(aud_stream_t *, queue_t *, mblk_t *);
static aud_return_t audio_79C30_mproto(aud_stream_t *, mblk_t *);
static void audio_79C30_start(aud_stream_t *);
static void audio_79C30_stop(aud_stream_t *);
static uint_t audio_79C30_setflag(aud_stream_t *, enum aud_opflag, uint_t);
static aud_return_t audio_79C30_setinfo(aud_stream_t *, mblk_t *, int *);
static void audio_79C30_queuecmd(aud_stream_t *, aud_cmd_t *);
static void audio_79C30_flushcmd(aud_stream_t *);
static uint_t audio_79C30_intr4();
static void audio_79C30_chipinit(amd_unit_t *);
static void audio_79C30_loopback(amd_unit_t *, uint_t);
static uint_t audio_79C30_outport(struct aud_79C30_chip *, uint_t);
static uint_t audio_79C30_play_gain(struct aud_79C30_chip *, uint_t);
static uint_t audio_79C30_gx_gain(struct aud_79C30_chip *, uint_t, uint_t);


/*
 * Record and monitor gain use the same lookup table
 */
#define	audio_79C30_record_gain(chip, val)				\
	audio_79C30_gx_gain(chip, val, AUDIO_UNPACK_REG(AUDIO_MAP_GX))
#define	audio_79C30_monitor_gain(chip, val)				\
	audio_79C30_gx_gain(chip, val, AUDIO_UNPACK_REG(AUDIO_MAP_STG))

/*
 * Level 13 interrupt handler, either in C (below) or in
 * audio_79C30_intr.s
 */
extern uint_t audio_79C30_asmintr();
extern uint_t audio_79C30_cintr();


/*
 * Local declarations
 */
static boolean_t audio_speaker_present;	/* B_FALSE on Galaxy */
static amd_unit_t amd_unit;		/* driver soft state struct */
amd_unit_t *amd_units = &amd_unit;	/* for the assembler int. handler */
static struct aud_gainset *gertab;
static int ger_gains;
static int play_gains;

static ddi_softintr_t audio_79C30_softintr_id;
ulong_t audio_79C30_softintr_cookie; /* used by trap code */

static ddi_iblock_cookie_t audio_79C30_trap_cookie;

/*
 * This is the size of the STREAMS buffers we send up the read side
 */
int audio_79C30_bsize = AUD_79C30_DEFAULT_BSIZE;

/*
 * The hi-level lock is used to block level 13 interrupts
 */
kmutex_t audio_79C30_hilev;
#define	LOCK_HILEVEL()	mutex_enter(&audio_79C30_hilev)
#define	UNLOCK_HILEVEL() mutex_exit(&audio_79C30_hilev)

#ifdef AUDIOTRACE
#define	ASSERT_HILOCKED() ASSERT(MUTEX_HELD(&audio_79C30_hilev))
#else
#define	ASSERT_HILOCKED()
#endif


/*
 * Declare audio ops vector for AMD79C30 support routines
 */
static struct aud_ops audio_79C30_ops = {
	audio_79C30_close,
	audio_79C30_ioctl,
	audio_79C30_mproto,
	audio_79C30_start,
	audio_79C30_stop,
	audio_79C30_setflag,
	audio_79C30_setinfo,
	audio_79C30_queuecmd,
	audio_79C30_flushcmd
};


/*
 * Streams declarations
 */

static struct module_info audio_79C30_modinfo = {
	0x6175,			/* module ID number */
	"audioamd",		/* module name */
	0,			/* min packet size accepted */
	AUD_79C30_MAXPACKET,	/* max packet size accepted */
	AUD_79C30_HIWATER,	/* hi-water mark */
	AUD_79C30_LOWATER,	/* lo-water mark */
};

/*
 * Queue information structure for read queue
 */
static struct qinit audio_79C30_rinit = {
	audio_rput,		/* put procedure */
	audio_rsrv,		/* service procedure */
	audio_79C30_open,	/* called on startup */
	audio_close,		/* called on finish */
	NULL,			/* for 3bnet only */
	&audio_79C30_modinfo,	/* module information structure */
	NULL,			/* module statistics structure */
};

/*
 * Queue information structure for write queue
 */
static struct qinit audio_79C30_winit = {
	audio_wput,		/* put procedure */
	audio_wsrv,		/* service procedure */
	NULL,			/* called on startup */
	NULL,			/* called on finish */
	NULL,			/* for 3bnet only */
	&audio_79C30_modinfo,	/* module information structure */
	NULL,			/* module statistics structure */
};

static struct streamtab audio_79C30_str_info = {
	&audio_79C30_rinit,	/* qinit for read side */
	&audio_79C30_winit,	/* qinit for write side */
	NULL,			/* mux qinit for read */
	NULL,			/* mux qinit for write */
				/* list of modules to be pushed */
};

/*
 * AM79C30 gain coefficient tables
 *
 * The record and monitor levels can range from -18dB to +12dB.
 * The play level is the sum of both gr and ger and can range
 * from -28dB to +30dB.  Such low output levels are not particularly
 * useful, however, so the range supported by the driver is -13dB to +30dB.
 *
 * Further, since ger also amplifies the monitor path, it is held at +5dB
 * except at extremely high output levels.
 *
 * The gain tables have been chosen to give the most granularity in
 * the upper half of the dynamic range, since small changes at low
 * levels are virtually indistiguishable.  Since the maximum output
 * gains are only needed for extremely quiet recordings, less
 * granularity is provided at the extreme upper end as well.
 */

struct aud_gainset {
	unsigned char	coef[2];
};

/* This is the table for record, monitor, and 1st-stage play gains */
static struct aud_gainset grtab[] = {
	/* Infinite attenuation is treated as a special case */
	{0x08, 0x90},			/* infinite attenuation */

	{0x7c, 0x8b},			/* -18.  dB */
	{0x35, 0x8b},			/* -17.  dB */
	{0x24, 0x8b},			/* -16.  dB */
	{0x23, 0x91},			/* -15.  dB */
	{0x2a, 0x91},			/* -14.  dB */
	{0x3b, 0x91},			/* -13.  dB */
	{0xf9, 0x91},			/* -12.  dB */
	{0xb6, 0x91},			/* -11.  dB */
	{0xa4, 0x91},			/* -10.  dB */
	{0x32, 0x92},			/*  -9.  dB */
	{0xaa, 0x92},			/*  -8.  dB */
	{0xb3, 0x93},			/*  -7.  dB */
	{0x91, 0x9f},			/*  -6.  dB */
	{0xf9, 0x9b},			/*  -5.  dB */
	{0x4a, 0x9a},			/*  -4.  dB */
	{0xa2, 0xa2},			/*  -3.  dB */
	{0xa3, 0xaa},			/*  -2.  dB */
	{0x52, 0xbb},			/*  -1.  dB */
	{0x08, 0x08},			/*   0.  dB */
	{0xb2, 0x4c},			/*   0.5 dB */
	{0xac, 0x3d},			/*   1.  dB */
	{0xe5, 0x2a},			/*   1.5 dB */
	{0x33, 0x25},			/*   2.  dB */
	{0x22, 0x22},			/*   2.5 dB */
	{0x22, 0x21},			/*   3.  dB */
	{0xd3, 0x1f},			/*   3.5 dB */
	{0xa2, 0x12},			/*   4.  dB */
	{0x1b, 0x12},			/*   4.5 dB */
	{0x3b, 0x11},			/*   5.  dB */
	{0xc3, 0x0b},			/*   5.5 dB */
	{0xf2, 0x10},			/*   6.  dB */
	{0xba, 0x03},			/*   6.5 dB */
	{0xca, 0x02},			/*   7.  dB */
	{0x1d, 0x02},			/*   7.5 dB */
	{0x5a, 0x01},			/*   8.  dB */
	{0x22, 0x01},			/*   8.5 dB */
	{0x12, 0x01},			/*   9.  dB */
	{0xec, 0x00},			/*   9.5 dB */
	{0x32, 0x00},			/*  10.  dB */
	{0x21, 0x00},			/*  10.5 dB */
	{0x13, 0x00},			/*  11.  dB */
	{0x11, 0x00},			/*  11.5 dB */
	{0x0e, 0x00}			/*  12.  dB */
};

/*
 * This is the table for 2nd-stage play gain.
 * Note that this gain stage affects the monitor volume also.
 * This gain table is for the sunergy classic that has an
 * additional op amp post 79c30 in the circuit.
 */
static struct aud_gainset postamp_gertab[] = {
	{0xaa, 0xaa},			/*  -10. dB */
	{0xbb, 0x9b},			/*  -9.5 dB */
	{0xac, 0x79},			/*  -9.  dB */
	{0x9a, 0x09},			/*  -8.5 dB */
	{0x99, 0x41},			/*  -8.  dB */
	{0x99, 0x31},			/*  -7.5 dB */
	{0xde, 0x9c},			/*  -7.  dB */
	{0xef, 0x9d},			/*  -6.5 dB */
	{0x9c, 0x74},			/*  -6.  dB */
	{0x9d, 0x54},			/*  -5.5 dB */
	{0xae, 0x6a},			/*  -5.  dB */
	{0xcd, 0xab},			/*  -4.5 dB */
	{0xdf, 0xab},			/*  -4.  dB */
	{0x29, 0x74},			/*  -3.5 dB */
	{0xab, 0x64},			/*  -3.  dB */
	{0xff, 0x6a},			/*  -2.5 dB */
	{0xbd, 0x2a},			/*  -2.  dB */
	{0xef, 0xbe},			/*  -1.5 dB */
	{0xce, 0x5c},			/*  -1.  dB */
	{0xcd, 0x75},			/*  -0.5 dB */
	{0x99, 0x00},			/*   0.  dB */
	{0xdd, 0x43},			/*   1.  dB */
	{0xef, 0x52},			/*   2.  dB */
	{0x42, 0x55},			/*   3.  dB */
	{0xdd, 0x31},			/*   4.  dB */
	{0x1f, 0x43},			/*   5.  dB */
	{0xdd, 0x40},			/*   6.  dB */
	{0x0f, 0x44},			/*   7.  dB */
	{0x1f, 0x31},			/*   8.  dB */
	{0xdd, 0x10},			/*   9.  dB */
	{0x0f, 0x41}			/*  10.  dB */
};

static struct aud_gainset std_gertab[] = {
	{0x1f, 0x43},			/*   5.  dB */
	{0x1f, 0x33},			/*   5.5 dB */
	{0xdd, 0x40},			/*   6.  dB */
	{0xdd, 0x11},			/*   6.5 dB */
	{0x0f, 0x44},			/*   7.  dB */
	{0x1f, 0x41},			/*   7.5 dB */
	{0x1f, 0x31},			/*   8.  dB */
	{0x20, 0x55},			/*   8.5 dB */
	{0xdd, 0x10},			/*   9.  dB */
	{0x11, 0x42},			/*   9.5 dB */
	{0x0f, 0x41},			/*  10.  dB */
	{0x1f, 0x11},			/*  10.5 dB */
	{0x0b, 0x60},			/*  11.  dB */
	{0xdd, 0x00},			/*  11.5 dB */
	{0x10, 0x42},			/*  12.  dB */
	{0x0f, 0x11},			/*  13.  dB */
	{0x00, 0x72},			/*  14.  dB */
	{0x10, 0x21},			/*  15.  dB */
	{0x00, 0x22},			/*  15.9 dB */
	{0x0b, 0x00},			/*  16.9 dB */
	{0x0f, 0x00}			/*  18.  dB */
};

#define	GR_GAINS	((sizeof (grtab) / sizeof (grtab[0])) - 1)


/*
 * Declare ops vectors for auto configuration.
 */
DDI_DEFINE_STREAM_OPS(audioamd_ops, audio_79C30_identify, nulldev,
    audio_79C30_attach, audio_79C30_detach, nodev, audio_79C30_getinfo,
    D_NEW | D_MP, &audio_79C30_str_info);

/*
 * Loadable module wrapper for SVr4 environment
 */
#include <sys/conf.h>
#include <sys/modctl.h>


extern struct mod_ops mod_driverops;

static struct modldrv audio_79C30_modldrv = {
	&mod_driverops,		/* Type of module */
	"AM79C30 audio driver",	/* Descriptive name */
	&audioamd_ops		/* Address of dev_ops */
};

static struct modlinkage audio_79C30_modlinkage = {
	MODREV_1,
	(void *)&audio_79C30_modldrv,
	NULL
};


int
_init()
{
	return (mod_install(&audio_79C30_modlinkage));
}


int
_fini()
{
	return (mod_remove(&audio_79C30_modlinkage));
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&audio_79C30_modlinkage, modinfop));
}


/*
 * Return the opaque device info pointer for a particular unit
 */
/*ARGSUSED*/
static int
audio_79C30_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	dev_t dev;
	int error;

	dev = (dev_t)arg;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (AMD_UNIT(dev) != 0 || amd_unit.dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)amd_unit.dip;
			error = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		/* Instance num is unit num (with minor number flags masked) */
		*result = (void *)AMD_UNIT(dev);
		error = DDI_SUCCESS;
		break;

	default:
		error = DDI_FAILURE;
		break;
	}
	return (error);
}


/*
 * Called from autoconf.c to locate device handlers
 */
static int
audio_79C30_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "audio") == 0)
		return (DDI_IDENTIFIED);

	return (DDI_NOT_IDENTIFIED);
}


/*
 * Attach to the device.
 */
/*ARGSUSED*/
static int
audio_79C30_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	aud_stream_t *as, *output_as, *input_as;
	amd_unit_t *unitp = &amd_unit;
	ddi_iblock_cookie_t icl;
	ddi_idevice_cookie_t idc;
	struct aud_cmd *pool;
	uint_t instance;
	caddr_t regs;
	char name[16];		/* XXX - A Constant! */
	int i;

	switch (cmd) {
	case DDI_ATTACH:
		instance = ddi_get_instance(dip);
		if (instance != 0) {
			cmn_err(CE_WARN, "audioamd: multiple audio devices?");
			return (DDI_FAILURE);
		}

		ATRACEINIT();

		/*
		 * We support only one unit, so we use a static
		 * 'amd_unit_t' that contains generic audio state in an
		 * 'aud_state_t' plus device-specific data.
		 */

		unitp = &amd_unit;

		/*
		 * Allocate command list buffers, initialized below
		 */
		unitp->allocated_size = AUD_79C30_CMDPOOL * sizeof (aud_cmd_t);
		unitp->allocated_memory = kmem_zalloc(unitp->allocated_size,
		    KM_NOSLEEP);
		if (unitp->allocated_memory == NULL)
			return (DDI_FAILURE);

		/*
		 * Map in the registers for this device
		 */
		if (ddi_map_regs(dip, 0, &regs, 0,
		    sizeof (struct aud_79C30_chip)) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}

		/*
		 * Does this system have a built-in speaker?
		 */
		switch (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    "audio-speaker?", -1)) {
		case 1:
			audio_speaker_present = B_TRUE;
			break;

		case 0:
			audio_speaker_present = B_FALSE;
			break;

		default:
			/*
			 * Galaxy didn't have a built-in speaker.
			 */
			if (strcmp(platform, "SUNW,SPARCsystem-600") == 0 ||
			    strcmp(platform, "SUNW,Sun_4_600") == 0)
				audio_speaker_present = B_FALSE;
			else
				audio_speaker_present = B_TRUE;
			break;
		}

		/*
		 * This check is here to see if the system has built
		 * in post 79C30 amplification. If this is the case we
		 * want to "limit" the secondary gain so that we
		 * aren't amplifying a distorted signal.
		 */
		switch (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    "audio-postamp", -1)) {
		case 1:
			gertab = postamp_gertab;
			ger_gains = (sizeof (postamp_gertab) /
			    sizeof (postamp_gertab[0]));
			break;
		case 0:
		default:
			gertab = std_gertab;
			ger_gains = (sizeof (std_gertab) /
			    sizeof (std_gertab[0]));
			break;
		}

		play_gains = (GR_GAINS + ger_gains - 1);

		/*
		 * Identify the audio device and assign a unit number.
		 * Get the address of this unit's audio device state
		 * structure.
		 */
		unitp->dip = dip;
		unitp->distate.ddstate = (caddr_t)unitp;
		unitp->distate.monitor_gain = 0;
		unitp->distate.output_muted = B_FALSE;
		unitp->distate.hw_features =
		    AUDIO_HWFEATURE_DUPLEX|AUDIO_HWFEATURE_IN2OUT;
		unitp->distate.sw_features = 0;
		unitp->distate.sw_features_enabled = 0;
		unitp->distate.ops = &audio_79C30_ops;
		unitp->chip = (struct aud_79C30_chip *)regs;

		/*
		 * Set up pointers between audio streams
		 */
		unitp->control.as.control_as = &unitp->control.as;
		unitp->control.as.output_as = &unitp->output.as;
		unitp->control.as.input_as = &unitp->input.as;
		unitp->output.as.control_as = &unitp->control.as;
		unitp->output.as.output_as = &unitp->output.as;
		unitp->output.as.input_as = &unitp->input.as;
		unitp->input.as.control_as = &unitp->control.as;
		unitp->input.as.output_as = &unitp->output.as;
		unitp->input.as.input_as = &unitp->input.as;

		as = &unitp->control.as;
		output_as = as->output_as;
		input_as = as->input_as;

		ASSERT(as != NULL);
		ASSERT(output_as != NULL);
		ASSERT(input_as != NULL);

		/*
		 * Initialize the play stream
		 */
		output_as->type = AUDTYPE_DATA;
		output_as->mode = AUDMODE_AUDIO;
		output_as->signals_okay = B_FALSE;
		output_as->distate = &unitp->distate;
		output_as->info.gain = AUD_79C30_DEFAULT_PLAYGAIN;
		output_as->info.sample_rate = AUD_79C30_SAMPLERATE;
		output_as->info.channels = AUD_79C30_CHANNELS;
		output_as->info.precision = AUD_79C30_PRECISION;
		output_as->info.encoding = AUDIO_ENCODING_ULAW;
		output_as->info.minordev = AMD_MINOR_RW;
		output_as->info.balance = AUDIO_MID_BALANCE;
		output_as->info.buffer_size = 0;

		output_as->traceq = NULL;
		output_as->maxfrag_size = AUD_79C30_MAX_BSIZE;

		/*
		 * Set the default output port according to capabilities
		 */
		output_as->info.port = (audio_speaker_present) ?
		    AUDIO_SPEAKER : AUDIO_HEADPHONE;
		output_as->info.avail_ports = AUDIO_HEADPHONE;
		if (audio_speaker_present)
			output_as->info.avail_ports |= AUDIO_SPEAKER;
		output_as->info.mod_ports = output_as->info.avail_ports;

		/*
		 * Initialize the record stream (by copying play stream
		 * and correcting some values)
		 */
		input_as->type = AUDTYPE_DATA;
		input_as->mode = AUDMODE_AUDIO;
		input_as->signals_okay = B_FALSE;
		input_as->distate = &unitp->distate;
		input_as->info = output_as->info;
		input_as->info.gain = AUD_79C30_DEFAULT_RECGAIN;
		input_as->info.minordev = AMD_MINOR_RO;
		input_as->info.balance = AUDIO_MID_BALANCE;
		input_as->info.port = AUDIO_MICROPHONE;
		input_as->info.avail_ports = AUDIO_MICROPHONE;
		input_as->info.mod_ports = AUDIO_MICROPHONE;
		input_as->info.buffer_size = audio_79C30_bsize;

		input_as->traceq = NULL;
		input_as->maxfrag_size = AUD_79C30_MAX_BSIZE;

		/*
		 * Control stream info
		 */
		as->type = AUDTYPE_CONTROL;
		as->mode = AUDMODE_NONE;
		as->signals_okay = B_TRUE;
		as->distate = &unitp->distate;
		as->traceq = NULL;
		as->info.minordev = AMD_MINOR_CTL;

		/*
		 * Initialize virtual chained DMA command block free
		 * lists.  Reserve a couple of command blocks for record
		 * buffers.  Then allocate the rest for play buffers.
		 */
		pool = (aud_cmd_t *)unitp->allocated_memory;
		unitp->input.as.cmdlist.free = NULL;
		unitp->output.as.cmdlist.free = NULL;
		for (i = 0; i < AUD_79C30_CMDPOOL; i++) {
			struct aud_cmdlist *list;

			list = (i < AUD_79C30_RECBUFS) ?
			    &unitp->input.as.cmdlist :
			    &unitp->output.as.cmdlist;
			pool->next = list->free;
			list->free = pool++;
		}

		/*
		 * Set up the interrupt handlers for this device.  Set up
		 * a level 4 soft interrupt service routine, which is
		 * scheduled when there is real work to do.
		 */
		if (ddi_add_softintr(dip, DDI_SOFTINT_MED,
		    &audio_79C30_softintr_id, &icl, &idc, audio_79C30_intr4,
		    (caddr_t)0) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "audioamd: cannot add soft interrupt");
			goto freemem;
		}
		audio_79C30_softintr_cookie = idc.idev_softint;

		/*
		 * We only expect one hard interrupt address at level 13.
		 * We first try to set the fast interrupt handler, but if
		 * it is not supported, then we will try to use the
		 * normal DDI-compliant C version of the interrupt
		 * handler.  If that fails, then we quit.  We will also
		 * default to the C version of the handler if the routine
		 * impl_setintreg_on doesn't exist in the current kernel.
		 */
		if (impl_setintreg_on == NULL ||
		    ddi_add_fastintr(dip, (uint_t)0, &audio_79C30_trap_cookie,
		    (ddi_idevice_cookie_t *)0, audio_79C30_asmintr) !=
		    DDI_SUCCESS) {
			if (ddi_add_intr(dip, 0, &audio_79C30_trap_cookie,
			    (ddi_idevice_cookie_t *)0, audio_79C30_cintr,
			    (caddr_t)0) != DDI_SUCCESS) {
				cmn_err(CE_WARN,
				    "audioamd: bad interrupt specification");
				goto remsoftint;
			}
		}

		mutex_init(&audio_79C30_hilev, NULL, MUTEX_DRIVER,
		    (void *)audio_79C30_trap_cookie);

		mutex_init(&unitp->lock, NULL, MUTEX_DRIVER, (void *)icl);
		output_as->lock = input_as->lock = as->lock = &unitp->lock;

		cv_init(&unitp->output.as.cv, NULL, CV_DRIVER, NULL);
		cv_init(&unitp->control.as.cv, NULL, CV_DRIVER, NULL);

		/*
		 * Initialize the audio chip
		 */
		LOCK_HILEVEL();
		audio_79C30_chipinit(unitp);
		UNLOCK_HILEVEL();

		ddi_report_dev(dip);

		(void) strcpy(name, "sound,audio");
		if (ddi_create_minor_node(dip, name, S_IFCHR, instance,
		    DDI_NT_AUDIO, 0) == DDI_FAILURE) {
			goto remhardint;
		}

		(void) strcpy(name, "sound,audioctl");
		if (ddi_create_minor_node(dip, name, S_IFCHR,
		    instance | AMD_MINOR_CTL, DDI_NT_AUDIO, 0) ==
		    DDI_FAILURE) {
			goto remminornode;
		}
		/*
		 * We don't bother with the pm_* properties, since our
		 * devo_power is NULL anyhow
		 */
		unitp->cpr_state = AMD_RESUMED;
		return (DDI_SUCCESS);

	case DDI_RESUME:
		/*
		 * We will only resume to a closed state, so we don't need to
		 * mess with the chip other than to init it.
		 */
		as = &unitp->control.as;
		LOCK_AS(as);
		ASSERT(! (unitp->control.as.info.open ||
		    unitp->input.as.info.open || unitp->output.as.info.open));
		if (unitp->cpr_state == AMD_SUSPENDED) {
			unitp->cpr_state = AMD_RESUMED;
			audio_79C30_chipinit(unitp);
		}
		UNLOCK_AS(as);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/*
	 * Error cleanup handling
	 */
remminornode:
	ddi_remove_minor_node(dip, NULL);

remhardint:
	ddi_remove_intr(dip, (uint_t)0, audio_79C30_trap_cookie);
	mutex_destroy(&audio_79C30_hilev);
	mutex_destroy(&unitp->lock);
	cv_destroy(&unitp->control.as.cv);
	cv_destroy(&unitp->output.as.cv);

remsoftint:
	ddi_remove_softintr(audio_79C30_softintr_id);

freemem:
	/* Deallocate structures allocated above */
	kmem_free(unitp->allocated_memory, unitp->allocated_size);

unmapregs:
	ddi_unmap_regs(dip, 0, &regs, (off_t)0,
	    (off_t)sizeof (struct aud_79C30_chip));

	return (DDI_FAILURE);
}


/*
 * Detach from the device.
 */
/*ARGSUSED*/
static int
audio_79C30_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	/* XXX - only handles a single detach at present */

	amd_unit_t *unitp = &amd_unit;
	aud_stream_t *as;

	switch (cmd) {
	case DDI_DETACH:
		ddi_remove_minor_node(dip, NULL);

		mutex_destroy(&audio_79C30_hilev);
		mutex_destroy(&unitp->lock);

		cv_destroy(&unitp->control.as.cv);
		cv_destroy(&unitp->output.as.cv);

		ddi_remove_intr(dip, (uint_t)0, audio_79C30_trap_cookie);
		ddi_remove_softintr(audio_79C30_softintr_id);

		ddi_unmap_regs(dip, 0, (caddr_t *)&unitp->chip, (off_t)0,
		    (off_t)sizeof (struct aud_79C30_chip));

		kmem_free(unitp->allocated_memory, unitp->allocated_size);

		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		/*
		 * This driver has a very basic strategy for dealing with
		 * suspend/resume.
		 * If the device is opened, then it won't suspend.
		 */
		as = &unitp->control.as;
		LOCK_AS(as);
		if (unitp->control.as.info.open || unitp->input.as.info.open ||
		    unitp->output.as.info.open) {
			UNLOCK_AS(as);
			return (DDI_FAILURE);
		} else {
			unitp->cpr_state = AMD_SUSPENDED;
			UNLOCK_AS(as);
			return (DDI_SUCCESS);
		}

	default:
		return (DDI_FAILURE);
	}
}


/*
 * Device open routine: set device structure ptr and call generic routine
 */
/*ARGSUSED*/
static int
audio_79C30_open(queue_t *q, dev_t *dp, int oflag, int sflag, cred_t *credp)
{
	amd_unit_t *unitp = &amd_unit;
	aud_stream_t *as = NULL;
	minor_t minornum;
	int status;

	/*
	 * Check that device is legal:
	 * Base unit number must be valid.
	 * If not a clone open, must be the control device.
	 */
	if (AMD_UNIT(*dp) != 0)
		return (ENODEV);

	minornum = geteminor(*dp);

	/*
	 * Get the correct audio stream
	 */
	if (minornum == unitp->output.as.info.minordev || minornum == 0)
		as = &unitp->output.as;
	else if (minornum == unitp->input.as.info.minordev)
		as = &unitp->input.as;
	else if (minornum == unitp->control.as.info.minordev)
		as = &unitp->control.as;

	if (as == NULL)
		return (ENODEV);

	/*
	 * cpr_state is protected by the (common) lock
	 */
	LOCK_AS(as);
	while (unitp->cpr_state != AMD_RESUMED) {
		ASSERT(! (unitp->control.as.info.open ||
		    unitp->input.as.info.open || unitp->output.as.info.open));
		UNLOCK_AS(as);
		(void) ddi_dev_is_needed(unitp->dip, 0, 1);
		LOCK_AS(as);
	}
	ATRACE(audio_79C30_open, 'OPEN', as);

	if (as == as->control_as) {
		as->type = AUDTYPE_CONTROL;
	} else {
		as = (oflag & FWRITE) ? as->output_as : as->input_as;
		as->type = AUDTYPE_DATA;
	}

	if (ISDATASTREAM(as) && ((oflag & (FREAD|FWRITE)) == FREAD))
		as = as->input_as;

	if (ISDATASTREAM(as)) {
		minornum = as->info.minordev | AMD_CLONE_BIT;
		sflag = CLONEOPEN;
	} else {
		minornum = as->info.minordev;
	}

	status = audio_open(as, q, dp, oflag, sflag);
	if (status != 0)
		goto done;

	if (ISDATASTREAM(as) && (oflag & FREAD)) {
		/*
		 * Set input bufsize now, in case the value was patched
		 */
		as->input_as->info.buffer_size = audio_79C30_bsize;

		audio_process_input(as->input_as);
	}

	*dp = makedevice(getmajor(*dp), AMD_UNIT(*dp) | minornum);

done:
	UNLOCK_AS(as);
	return (status);
}


/*
 * Device-specific close routine, called from generic module.
 * Must be called with LOLEVEL lock held.
 */
static void
audio_79C30_close(aud_stream_t *as)
{
	amd_unit_t *unitp;

	ASSERT_ASLOCKED(as);
	unitp = UNITP(as);
	ASSERT(unitp == &amd_unit);

	/*
	 * Reset status bits.  The device will already have been stopped.
	 */
	if (as == as->output_as) {
		unitp->output.samples = 0;
		unitp->output.error = B_FALSE;
	} else {
		unitp->input.samples = 0;
		unitp->input.error = B_FALSE;
	}

	/*
	 * Reset to u-law on last close
	 */
	if ((as == as->output_as && as->input_as->readq == NULL) ||
	    (as == as->input_as && as->output_as->readq == NULL)) {
		LOCK_HILEVEL();
		as->output_as->info.encoding = AUDIO_ENCODING_ULAW;
		as->input_as->info.encoding = AUDIO_ENCODING_ULAW;
		unitp->chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
		unitp->chip->dr = (AUDIO_MMR1_BITS_u_LAW |
		    AUDIO_MMR1_BITS_LOAD_GR | AUDIO_MMR1_BITS_LOAD_GER |
		    AUDIO_MMR1_BITS_LOAD_GX | AUDIO_MMR1_BITS_LOAD_STG);
		UNLOCK_HILEVEL();
	}

	/*
	 * If a user process mucked up the device, reset it when fully
	 * closed
	 */
	if (unitp->init_on_close && !as->output_as->info.open &&
	    !as->input_as->info.open) {
		LOCK_HILEVEL();
		audio_79C30_chipinit(unitp);
		unitp->init_on_close = B_FALSE;
		UNLOCK_HILEVEL();
	}

	ATRACE(audio_79C30_close, 'CLOS', as);
}


/*
 * Process ioctls not already handled by the generic audio handler.
 *
 * If AUDIO_CHIP is defined, we support ioctls that allow user processes
 * to muck about with the device registers.
 * Must be called with LOLEVEL lock held.
 */
static aud_return_t
audio_79C30_ioctl(aud_stream_t *as, queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	aud_return_t change;
	caddr_t uaddr;
	int loop;
	audio_device_t *devtp;

	ASSERT_ASLOCKED(as);
	change = AUDRETURN_NOCHANGE; /* detect device state change */

	iocp = (struct iocblk *)(void *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	default:
	einval:
		/* NAK the request */
		audio_ack(q, mp, EINVAL);
		goto done;

	case AUDIO_GETDEV:	/* return device type */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;
		freemsg(mp->b_cont);
		mp->b_cont = allocb(sizeof (audio_device_t), BPRI_MED);
		if (mp->b_cont == NULL) {
			audio_ack(q, mp, ENOSR);
			goto done;
		}

		devtp = (audio_device_t *)(void *)mp->b_cont->b_rptr;
		mp->b_cont->b_wptr += sizeof (audio_device_t);
		(void) strcpy(devtp->name, AMD_DEV_NAME);
		(void) strcpy(devtp->version, AMD_DEV_VERSION);
		(void) strcpy(devtp->config, AMD_DEV_CONFIG_ONBRD1);

		audio_copyout(q, mp, uaddr, sizeof (audio_device_t));
		break;

	case AUDIO_DIAG_LOOPBACK: /* set/clear loopback mode */
		loop = *(int *)(void *)mp->b_cont->b_rptr; /* true to enable */
		UNITP(as)->init_on_close = B_TRUE; /* reset device later */

		LOCK_HILEVEL();
		audio_79C30_loopback(UNITP(as), loop);
		UNLOCK_HILEVEL();

		/* Acknowledge the request and we're done */
		audio_ack(q, mp, 0);

		change = AUDRETURN_CHANGE;
		break;
	}

done:
	return (change);
} /* audio_79C30_ioctl */


/*
 * audio_79C30_mproto - handle synchronous M_PROTO messages
 *
 * This driver does not suppport any M_PROTO message, but we must free
 * the message.
 */
/*ARGSUSED*/
static aud_return_t
audio_79C30_mproto(aud_stream_t *as, mblk_t *mp)
{
	if (mp != NULL)
		freemsg(mp);
	return (AUDRETURN_NOCHANGE);
} /* audio_79C30_mproto */


/*
 * The next routine is used to start reads or writes.  If there is a
 * change of state, enable the chip.  If there was already i/o active in
 * the desired direction, or if i/o is paused, don't bother enabling the
 * chip.  Must be called with LOLEVEL lock held.
 */
static void
audio_79C30_start(aud_stream_t *as)
{
	amd_stream_t *amds;
	int idle;
	int pause;

	ASSERT_ASLOCKED(as);
	ATRACE(audio_79C30_start, '  AS', as);

	if (as == as->output_as) {
		amds = &UNITP(as)->output;
		idle = !(UNITP(as)->input.active);
	} else {
		amds = &UNITP(as)->input;
		idle = !(UNITP(as)->output.active);
	}
	pause = as->info.pause;

	/* If already active, paused, or nothing queued to the device, done */
	if (amds->active || pause || (amds->cmdptr == NULL))
		return;

	amds->active = B_TRUE;

	if (idle) {
		ATRACE(audio_79C30_start,
		    (as == as->output_as ? 'SOUT' : 'SINP'), as);
		CSR(as)->cr = AUDIO_UNPACK_REG(AUDIO_INIT_INIT);
		CSR(as)->dr = AUDIO_INIT_BITS_ACTIVE;
	}
}


/*
 * The next routine is used to stop reads or writes.  All we do is turn
 * off the active bit.  If there is currently no i/o active in the other
 * direction, then the interrupt routine will disable the chip.  Must be
 * called with LOLEVEL lock held.
 */
static void
audio_79C30_stop(aud_stream_t *as)
{
	ASSERT_ASLOCKED(as);
	ATRACE(audio_79C30_stop, '  AS', as);

	LOCK_HILEVEL();
	if (as == as->output_as)
		UNITP(as)->output.active = B_FALSE;
	else
		UNITP(as)->input.active = B_FALSE;
	UNLOCK_HILEVEL();
}


/*
 * Get or set a particular flag value.  Must be called with LOLEVEL lock
 * held.
 */
static uint_t
audio_79C30_setflag(aud_stream_t *as, enum aud_opflag op, uint_t val)
{
	amd_stream_t *amds;

	ASSERT_ASLOCKED(as);

	amds = (as == as->output_as) ? &UNITP(as)->output : &UNITP(as)->input;

	switch (op) {
	case AUD_ERRORRESET:	/* read/reset error flag atomically */
		LOCK_HILEVEL();
		val = amds->error;
		amds->error = B_FALSE;
		UNLOCK_HILEVEL();
		break;

	case AUD_ACTIVE:	/* GET only */
		val = amds->active;
		break;
	}

	return (val);
}


/*
 * Get or set device-specific information in the audio state structure.
 * Must be called with LOLEVEL lock held.
 */
static aud_return_t
audio_79C30_setinfo(aud_stream_t *as, mblk_t *mp, int *error)
{
	amd_unit_t *unitp;
	struct aud_79C30_chip *chip;
	audio_info_t *ip;
	uint_t sample_rate, channels, precision, encoding;

	ASSERT_ASLOCKED(as);

	unitp = UNITP(as);
	ASSERT(unitp == &amd_unit);

	/*
	 * Set device-specific info into device-independent structure
	 */
	as->output_as->info.samples = unitp->output.samples;
	as->input_as->info.samples = unitp->input.samples;
	as->output_as->info.active = unitp->output.active;
	as->input_as->info.active = unitp->input.active;

	/*
	 * If getinfo, 'mp' is NULL...we're done
	 */
	if (mp == NULL)
		return (AUDRETURN_NOCHANGE);

	ip = (audio_info_t *)(void *)mp->b_cont->b_rptr;

	chip = unitp->chip;

	/*
	 * ENTER CRITICAL: from this point on, do not return, but rather
	 * "goto done;".
	 *
	 * Load chip registers atomically
	 */
	LOCK_HILEVEL();

	/*
	 * If any new value matches the current value, there
	 * should be no need to set it again here.
	 * However, it's work to detect this so don't bother.
	 */
	if (Modify(ip->play.gain)) {
		as->output_as->info.gain = audio_79C30_play_gain(chip,
		    ip->play.gain);
		if (as->distate->output_muted)
			as->distate->output_muted = B_FALSE;
	}

	if (Modify(ip->record.gain)) {
		as->input_as->info.gain = audio_79C30_record_gain(chip,
		    ip->record.gain);
	}

	if (Modify(ip->monitor_gain)) {
		as->distate->monitor_gain = audio_79C30_monitor_gain(chip,
		    ip->monitor_gain);
	}

	if (Modifyc(ip->output_muted)) {
		if (ip->output_muted) {
			(void) audio_79C30_play_gain(chip, (uint_t)0);
			as->distate->output_muted = B_TRUE;
		} else {
			(void) audio_79C30_play_gain(chip,
			    as->output_as->info.gain);
			as->distate->output_muted = B_FALSE;
		}
	}

	if (Modify(ip->record.buffer_size)) {
		if (((as != as->input_as) &&
		    (as == as->output_as) && !(as->openflag & FREAD)) ||
		    (ip->record.buffer_size <= 0) ||
		    (ip->record.buffer_size > AUD_79C30_MAX_BSIZE)) {
			ATRACE(audio_79C30_setinfo, 'OOPS', 1);
			*error = EINVAL;
		} else {
			as->input_as->info.buffer_size =
			    ip->record.buffer_size;
		}
	}

	if (Modify(ip->play.port)) {
		switch (ip->play.port) {
		case AUDIO_SPEAKER:
			/* If there is no speaker available, ignore port */
			if (!audio_speaker_present) {
				ATRACE(audio_79C30_setinfo, 'OOPS', 2);
				*error = EINVAL;
				break;
			}
			/*FALLTHROUGH*/

		case AUDIO_HEADPHONE:
			as->output_as->info.port = audio_79C30_outport(chip,
			    ip->play.port);
			break;

		default:
			ATRACE(audio_79C30_setinfo, 'OOPS', 3);
			*error = EINVAL;
			break;
		}
	}

	/*
	 * Set the sample counters atomically, returning the old values.
	 */
	if (Modify(ip->play.samples) || Modify(ip->record.samples)) {
		if (as->output_as->info.open) {
			as->output_as->info.samples = unitp->output.samples;
			if (Modify(ip->play.samples))
				unitp->output.samples = ip->play.samples;
		}

		if (as->input_as->info.open) {
			as->input_as->info.samples = unitp->input.samples;
			if (Modify(ip->record.samples))
				unitp->input.samples = ip->record.samples;
		}
	}

	if (Modify(ip->play.sample_rate))
		sample_rate = ip->play.sample_rate;
	else if (Modify(ip->record.sample_rate))
		sample_rate = ip->record.sample_rate;
	else
		sample_rate = as->info.sample_rate;

	if (Modify(ip->play.channels))
		channels = ip->play.channels;
	else if (Modify(ip->record.channels))
		channels = ip->record.channels;
	else
		channels = as->info.channels;

	if (Modify(ip->play.precision))
		precision = ip->play.precision;
	else if (Modify(ip->record.precision))
		precision = ip->record.precision;
	else
		precision = as->info.precision;

	if (Modify(ip->play.encoding))
		encoding = ip->play.encoding;
	else if (Modify(ip->record.encoding))
		encoding = ip->record.encoding;
	else
		encoding = as->info.encoding;

	/*
	 * If setting to the current format, do not do anything.  Otherwise
	 * check and see if this is a valid format.
	 */
	if ((sample_rate == as->info.sample_rate) &&
	    (channels == as->info.channels) &&
	    (precision == as->info.precision) &&
	    (encoding == as->info.encoding)) {
		goto done;
	} else if ((sample_rate != 8000) ||
	    (channels != 1) ||
	    (precision != 8) ||
	    ((encoding != AUDIO_ENCODING_ULAW) &&
	    (encoding != AUDIO_ENCODING_ALAW))) {
		ATRACE(audio_79C30_setinfo, 'OOPS', 4);
		*error = EINVAL;
		goto done;
	}

	if (Modify(ip->play.encoding)) {
		/*
		 * If a process wants to modify the play format,
		 * another process can not have it open for recording.
		 */
		if (as->input_as->info.open &&
		    as->output_as->info.open &&
		    (as->input_as->readq != as->output_as->readq)) {
			ATRACE(audio_79C30_setinfo, 'OOPS', 5);
			*error = EBUSY;
			goto playdone;
		}

		/*
		 * Control stream cannot modify the play format
		 */
		if ((as != as->output_as) && (as != as->input_as)) {
			ATRACE(audio_79C30_setinfo, 'OOPS', 6);
			*error = EINVAL;
			goto playdone;
		}

		switch (ip->play.encoding) {
		case AUDIO_ENCODING_ULAW:
			as->output_as->info.encoding = AUDIO_ENCODING_ULAW;
			as->input_as->info.encoding = AUDIO_ENCODING_ULAW;

			/* Tell the chip to accept the gain registers */
			chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
			chip->dr = (AUDIO_MMR1_BITS_u_LAW |
			    AUDIO_MMR1_BITS_LOAD_GR |
			    AUDIO_MMR1_BITS_LOAD_GER |
			    AUDIO_MMR1_BITS_LOAD_GX |
			    AUDIO_MMR1_BITS_LOAD_STG);
			break;

		case AUDIO_ENCODING_ALAW:
			as->output_as->info.encoding = AUDIO_ENCODING_ALAW;
			as->input_as->info.encoding = AUDIO_ENCODING_ALAW;

			/* Tell the chip to accept the gain registers */
			chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
			chip->dr = (AUDIO_MMR1_BITS_A_LAW |
			    AUDIO_MMR1_BITS_LOAD_GR |
			    AUDIO_MMR1_BITS_LOAD_GER |
			    AUDIO_MMR1_BITS_LOAD_GX |
			    AUDIO_MMR1_BITS_LOAD_STG);
			break;

		default:
			ATRACE(audio_79C30_setinfo, 'OOPS', 7);
			*error = EINVAL;
			break;
		}
	playdone:;
	}

	if (Modify(ip->record.encoding)) {
		/*
		 * If a process wants to modify the record format,
		 * another process can not have it open for recording.
		 */
		if (as->input_as->info.open &&
		    as->output_as->info.open &&
		    (as->input_as->readq != as->output_as->readq)) {
			ATRACE(audio_79C30_setinfo, 'OOPS', 8);
			*error = EBUSY;
			goto recdone;
		}

		/*
		 * Control stream cannot modify the record format
		 */
		if ((as != as->output_as) && (as != as->input_as)) {
			ATRACE(audio_79C30_setinfo, 'OOPS', 9);
			*error = EINVAL;
			goto recdone;
		}

		switch (ip->record.encoding) {
		case AUDIO_ENCODING_ULAW:
			as->input_as->info.encoding = AUDIO_ENCODING_ULAW;
			as->output_as->info.encoding = AUDIO_ENCODING_ULAW;

			/* Tell the chip to accept the gain registers */
			chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
			chip->dr = (AUDIO_MMR1_BITS_u_LAW |
			    AUDIO_MMR1_BITS_LOAD_GR |
			    AUDIO_MMR1_BITS_LOAD_GER |
			    AUDIO_MMR1_BITS_LOAD_GX |
			    AUDIO_MMR1_BITS_LOAD_STG);
			break;

		case AUDIO_ENCODING_ALAW:
			as->input_as->info.encoding = AUDIO_ENCODING_ALAW;
			as->output_as->info.encoding = AUDIO_ENCODING_ALAW;

			/* Tell the chip to accept the gain registers */
			chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
			chip->dr = (AUDIO_MMR1_BITS_A_LAW |
			    AUDIO_MMR1_BITS_LOAD_GR |
			    AUDIO_MMR1_BITS_LOAD_GER |
			    AUDIO_MMR1_BITS_LOAD_GX |
			    AUDIO_MMR1_BITS_LOAD_STG);
			break;


		default:
			ATRACE(audio_79C30_setinfo, 'OOPS', 10);
			*error = EINVAL;
			break;
		}
	recdone:;
	}

done:
	UNLOCK_HILEVEL();
	return (AUDRETURN_CHANGE);
}


/*
 * This routine is called whenever a new command is added to the cmd chain.
 * Since the virtual dma controller simply uses the driver's cmd chain,
 * all we have to do is make sure that the virtual controller has the
 * start address right.
 * Must be called with LOLEVEL lock held.
 */
/*ARGSUSED*/
static void
audio_79C30_queuecmd(aud_stream_t *as, aud_cmd_t *cmdp)
{
	amd_stream_t *amds;

	ASSERT_ASLOCKED(as);
	ATRACE(audio_79C30_queuecmd, '  AS', as);
	ATRACE(audio_79C30_queuecmd, ' CMD', cmdp);

	if (as == as->output_as)
		amds = &UNITP(as)->output;
	else
		amds = &UNITP(as)->input;

	/*
	 * If the virtual controller command list is NULL, then the interrupt
	 * routine is probably disabled.  In the event that it is not,
	 * setting a new command list below is safe at low priority.
	 */
	if (amds->cmdptr == NULL) {
		ATRACE(audio_79C30_queuecmd, 'NULL', as->cmdlist.head);
		amds->cmdptr = as->cmdlist.head;
		if (!amds->active)
			audio_79C30_start(as); /* go, if not paused */
	}
}


/*
 * Flush the device's notion of queued commands.
 * Must be called with LOLEVEL lock held.
 */
static void
audio_79C30_flushcmd(aud_stream_t *as)
{
	ASSERT_ASLOCKED(as);

	LOCK_HILEVEL();
	if (as == as->output_as)
		UNITP(as)->output.cmdptr = NULL;
	else
		UNITP(as)->input.cmdptr = NULL;
	UNLOCK_HILEVEL();
}


/*
 * This is the interrupt routine used by the level 4 interrupt.
 * It is scheduled by the level 13 interrupt routine, simply calls out
 * to the generic audio driver routines to process finished play and
 * record buffers.
 */
/*ARGSUSED*/
static uint_t
audio_79C30_intr4(arg)
	caddr_t arg;
{
	aud_stream_t *as;

	/* XXX - for now, there's only one audio device */
	amd_unit_t *unitp = &amd_unit;

	as = &unitp->control.as;

	/* Lock generic audio driver code */
	LOCK_AS(as);

	/* Process Record events: call out if finished cmd block or error */
	if (((as->input_as->cmdlist.head != NULL) &&
	    as->input_as->cmdlist.head->done) ||
	    UNITP(as)->input.error) {
		ATRACE(audio_79C30_intr4, 'rint', as->input_as);
		audio_process_input(as->input_as);
	}

	/* Process Play events: call out if finished cmd block or error */
	if (((as->output_as->cmdlist.head != NULL) &&
	    as->output_as->cmdlist.head->done) ||
	    UNITP(as)->output.error) {
		ATRACE(audio_79C30_intr4, 'tint', as->output_as);
		audio_process_output(as->output_as);
	}
	UNLOCK_AS(as);

	return (DDI_INTR_CLAIMED);
}


/*
 * Initialize the audio chip to a known good state.
 *
 * The audio outputs are usually high impedance.  This causes a problem
 * with the current hardware, since the high impedance line picks up
 * lots of noise.  The lines are always high impedance if the chip is
 * idle (as on reset or after setting the INIT register to 0).
 * We can lower the impedance of one output, and hence the noise, by
 * making the chip active and selecting that output.  However, the
 * other output will be floating, and hence noisy.
 */
static void
audio_79C30_chipinit(amd_unit_t *unitp)
{
	struct aud_79C30_chip *chip;

	chip = unitp->chip;
	ASSERT(chip != NULL);

	/* Make the chip inactive and turn off the INT pin. */
	chip->cr = AUDIO_UNPACK_REG(AUDIO_INIT_INIT);
	chip->dr = AUDIO_INIT_BITS_ACTIVE | AUDIO_INIT_BITS_INT_DISABLED;

	/*
	 * Set up the multiplexer.  We use the Bb port for both reads
	 * and writes.  We also enable interrupts for the port.  Note
	 * that we also have to enable the INT pin using the INIT
	 * register to get an interrupt.
	 */
	chip->cr = AUDIO_UNPACK_REG(AUDIO_MUX_MCR1);
	chip->dr = AUDIO_MUX_PORT_BA | (AUDIO_MUX_PORT_BB << 4);

	chip->cr = AUDIO_UNPACK_REG(AUDIO_MUX_MCR2);
	chip->dr = 0;

	chip->cr = AUDIO_UNPACK_REG(AUDIO_MUX_MCR3);
	chip->dr = 0;

	chip->cr = AUDIO_UNPACK_REG(AUDIO_MUX_MCR4);
	chip->dr = AUDIO_MUX_MCR4_BITS_INT_ENABLE;

	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_FTGR);
	chip->dr = 0;
	chip->dr = 0;

	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_ATGR);
	chip->dr = 0;
	chip->dr = 0;

	/* Init the gain registers */
	unitp->output.as.info.gain = audio_79C30_play_gain(chip,
	    unitp->output.as.info.gain);
	unitp->input.as.info.gain = audio_79C30_record_gain(chip,
	    unitp->input.as.info.gain);

	(void) audio_79C30_monitor_gain(chip, unitp->distate.monitor_gain);
	(void) audio_79C30_outport(chip, unitp->output.as.info.port);

	/* Tell the chip to accept the gain registers */
	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
	chip->dr = AUDIO_MMR1_BITS_LOAD_GR | AUDIO_MMR1_BITS_LOAD_GER |
	    AUDIO_MMR1_BITS_LOAD_GX | AUDIO_MMR1_BITS_LOAD_STG;
}


/*
 * Set or clear internal loopback for diagnostic purposes.
 * Must be called with HILEVEL lock held.
 */
static void
audio_79C30_loopback(amd_unit_t *unitp, uint_t loop)
{
	int encoding;

	ASSERT_HILOCKED();

	switch (unitp->input.as.info.encoding) {
	case AUDIO_ENCODING_ULAW:
	default:
		encoding = AUDIO_MMR1_BITS_A_LAW;
		break;

	case AUDIO_ENCODING_ALAW:
		encoding = AUDIO_MMR1_BITS_u_LAW;
		break;
	}

	unitp->chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR1);
	unitp->chip->dr = encoding | AUDIO_MMR1_BITS_LOAD_GR |
	    AUDIO_MMR1_BITS_LOAD_GER | AUDIO_MMR1_BITS_LOAD_GX |
	    AUDIO_MMR1_BITS_LOAD_STG | (loop ? AUDIO_MMR1_BITS_LOAD_DLB : 0);
}


/*
 * Set output port to external jack or built-in speaker.
 * Must be called with HILEVEL lock held.
 */
static uint_t
audio_79C30_outport(struct aud_79C30_chip *chip, uint_t val)
{
	ASSERT_HILOCKED();

	/* AINB is the analog input port; LS is the built-in speaker */
	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_MMR2);
	chip->dr = AUDIO_MMR2_BITS_AINB |
	    (val == AUDIO_SPEAKER ? AUDIO_MMR2_BITS_LS : 0);

	return (val);
}



/*
 * Convert play gain to chip values and load them.
 * Keep the 2nd stage gain at its lowest value if possible,
 * so that monitor gain isn't affected.
 * Return the closest appropriate gain value.
 * Must be called with HILEVEL lock held.
 */
static uint_t
audio_79C30_play_gain(struct aud_79C30_chip *chip, uint_t val)
{
	int gr;
	int ger;

	ASSERT_HILOCKED();
	ger = 0;		/* assume constant 2nd stage gain */
	if (val == 0) {
		gr = -1;	/* first gain entry is infinite attenuation */
	} else {
		/* Scale gain range to available values */
		gr = ((val * play_gains) + (play_gains / 2)) /
		    (AUDIO_MAX_GAIN + 1);

		/* Scale back to full range for return value */
		if (val != AUDIO_MAX_GAIN)
			val = ((gr * AUDIO_MAX_GAIN) + (AUDIO_MAX_GAIN / 2)) /
			    play_gains;

		/* If gr is off scale, increase 2nd stage gain */
		if (gr >= GR_GAINS) {
			ger = gr - GR_GAINS + 1;
			gr = GR_GAINS - 1;
		}
	}

	/* Load output gain registers */
	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_GR);
	chip->dr = grtab[++gr].coef[0];
	chip->dr = grtab[gr].coef[1];
	chip->cr = AUDIO_UNPACK_REG(AUDIO_MAP_GER);
	chip->dr = gertab[ger].coef[0];
	chip->dr = gertab[ger].coef[1];

	return (val);
}


/*
 * Convert record and monitor gain to chip values and load them.
 * Return the closest appropriate gain value.
 * Must be called with HILEVEL lock held.
 */
static uint_t
audio_79C30_gx_gain(struct aud_79C30_chip *chip, uint_t val, uint_t reg)
{
	int gr;

	ASSERT_HILOCKED();
	if (val == 0) {
		gr = -1;	/* first gain entry is infinite attenuation */
	} else {
		/* Scale gain range to available values */
		gr = ((val * GR_GAINS) + (GR_GAINS / 2)) /
		    (AUDIO_MAX_GAIN + 1);

		/* Scale back to full range for return value */
		if (val != AUDIO_MAX_GAIN)
			val = ((gr * AUDIO_MAX_GAIN) + (AUDIO_MAX_GAIN / 2)) /
			    GR_GAINS;
	}

	/* Load gx or stg registers */
	chip->cr = (char)reg;
	chip->dr = grtab[++gr].coef[0];
	chip->dr = grtab[gr].coef[1];
	return (val);
}


/*
 * Level 13 intr handler implements a pseudo DMA device for the AMD79C30.
 *
 * NOTE: This routine has a number of unreferenced labels corresponding
 * to branch points in the assembly language routine
 * (audio_79C30_intr.s).
 */
uint_t
audio_79C30_cintr()
{
	amd_unit_t *unitp;
	struct aud_79C30_chip *chip;
	aud_cmd_t *cmdp;
	int int_active;

#define	Interrupt	1
#define	DidSomething	2
#define	Active		4

	/* Acquire spin lock */
	LOCK_HILEVEL();

	/*
	 * Figure out which chip interrupted.
	 * Since we only have one chip, we punt and assume device zero.
	 *
	 * XXX - how would we differentiate between chips??
	 */
	unitp = &amd_units[0];
	chip = unitp->chip;
	int_active = chip->ir;		/* clear interrupt condition */

	if (int_active & 16)
		int_active = Active;
	else
		goto checkintr;

	/*
	 * Process record IO
	 */
	if (unitp->input.active) {
		cmdp = unitp->input.cmdptr;

recskip:
		/*
		 * A normal buffer must have at least 1 sample of data.
		 * A zero length buffer must have had the skip flag set.
		 * A skipped (possibly zero length) command block is used
		 * to synchronize the audio stream.
		 */
		while (cmdp != NULL && (cmdp->skip || cmdp->done)) {
			cmdp->done = B_TRUE;
			cmdp = cmdp->next;
			unitp->input.cmdptr = cmdp;
			int_active |= Interrupt;
		}

		/*
		 * Check for flow error
		 */
		if (cmdp == NULL) {
			/* Flow error condition */
recnull:
			unitp->input.error = B_TRUE;
			unitp->input.active = B_FALSE;
			int_active |= Interrupt;
			goto play;
		}

recactive:
		/*
		 * Transfer record data
		 */
		unitp->input.samples++;		/* bump sample count */
		*cmdp->data++ = chip->bbrb;
		int_active |= DidSomething;	/* note device active */

		/* Notify driver of end of buffer condition */
		if (cmdp->data >= cmdp->enddata) {
			cmdp->done = B_TRUE;
			unitp->input.cmdptr = cmdp->next;
recintr:
			int_active |= Interrupt;
		}
	}

play:
	if (unitp->output.active) {
		cmdp = unitp->output.cmdptr;

playskip:
		/*
		 * Ignore null and non-data buffers
		 */
		while (cmdp != NULL && (cmdp->skip || cmdp->done)) {
			cmdp->done = B_TRUE;
			cmdp = cmdp->next;
			unitp->output.cmdptr = cmdp;
			int_active |= Interrupt;
		}

		/*
		 * Check for flow error
		 */
		if (cmdp == NULL) {
			/* Flow error condition */
playnull:
			unitp->output.error = B_TRUE;
			unitp->output.active = B_FALSE;
			int_active |= Interrupt;
			goto done;
		}

playactive:
		/*
		 * Transfer play data
		 */
		unitp->output.samples++;
		chip->bbrb = *cmdp->data++;
		int_active |= DidSomething;

		/*
		 * Notify driver of end of buffer condition
		 */
		if (cmdp->data >= cmdp->enddata) {
			cmdp->done = B_TRUE;
			unitp->output.cmdptr = cmdp->next;
playintr:
			int_active |= Interrupt;
		}
	}

done:
checkactive:
	/*
	 * If no IO is active, shut down device interrupts
	 */
	if (!(int_active & DidSomething)) {
		chip->cr = AUDIO_UNPACK_REG(AUDIO_INIT_INIT);
		chip->dr = AUDIO_INIT_BITS_ACTIVE |
		    AUDIO_INIT_BITS_INT_DISABLED;
	}

checkintr:

	/* Release spin lock */
	UNLOCK_HILEVEL();

	/*
	 * Schedule a lower priority interrupt, if necessary
	 *
	 * NOTE: call impl_setintreg_on(audio)79C30_sofintr_cookie) in
	 * the assembly version.
	 */
	if (int_active & Interrupt)
		ddi_trigger_softintr(audio_79C30_softintr_id);
reti:
	return ((int_active & Active) ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}
