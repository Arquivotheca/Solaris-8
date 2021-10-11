/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_4231.c	1.12	99/11/02 SMI"

/*
 * audiocs Audio Driver
 *
 * This Audio Driver controls the Crystal CS4231 Codec used on many SPARC
 * platforms. It does not support the CS4231 on Power PCs or x86 PCs. It
 * does support two different DMA engines, the APC and EB2. The code for
 * those DMA engines is split out and a well defined, but private, interface
 * is used to control those DMA engines.
 *
 * For some reason setting the CS4231's registers doesn't always succeed.
 * Therefore every time we set a register we always read it back to make
 * sure it was set. If not we wait a little while and then try again. This
 * is all taken care of in the routines cs4231_put8() and cs4231_reg_select()
 * and the macros OR_SET_BYTE() and AND_SET_BYTE(). We don't worry about
 * the status register because it is cleared by writing anything to it.
 * So it doesn't matter what the value written is.
 *
 * This driver uses the mixer Audio Personality Module to implement audio(7I)
 * and mixer(7I) semantics. Unfortunately this is a single stream Codec,
 * forcing the mixer to do sample rate conversion.
 *
 * This driver supports suspending and resuming. A suspend just stops playing
 * and recording. The play DMA buffers end up getting thrown away, but when
 * you shut down the machine there is a break in the audio anyway, so they
 * won't be missed and it isn't worth the effort to save them. When we resume
 * we always start playing and recording. If they aren't needed they get
 * shut off by the mixer.
 *
 * Power management uses the new paradigm introduced in Solaris 8. However,
 * since this driver is also used in earlier releases it may be compiled
 * without the -DNEW_PM. This results in no power managment at all.
 *
 * Power management uses one component for the device. pm_busy_component()
 * is called once each time cs4231_ad_setup() is called and pm_idle_component()
 * is called once each time cs4231_ad_teardown() is called.  We also call
 * pm_busy_component() when we enter cs4231_ad_set_config() and
 * cs4231_ad_set_format() so that the Codec is powered up when we change any
 * of its configuration. We then call pm_idle_component() before we return to
 * unstack the busy count.
 *
 *	NOTE: This module depends on the misc/audiosup and misc/mixer modules
 *		being loaded first.
 */

#include <sys/mixer_impl.h>
#include <sys/audio_4231.h>
#include <sys/sunddi.h>		/* ddi_prop_op, etc. */
#include <sys/modctl.h>		/* modldrv */
#include <sys/kmem.h>		/* kmem_free(), etc. */

/*
 * Global routines.
 */
int cs4231_poll_ready(CS_state_t *);

/*
 * Module linkage routines for the kernel
 */
static int cs4231_attach(dev_info_t *, ddi_attach_cmd_t);
static int cs4231_detach(dev_info_t *, ddi_detach_cmd_t);
static int cs4231_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int cs4231_power(dev_info_t *, int, int);

/*
 * Entry point routine prototypes
 */
static int cs4231_ad_setup(int, int, int);
static void cs4231_ad_teardown(int, int);
static int cs4231_ad_set_config(int, int, int, int, int, int);
static int cs4231_ad_set_format(int, int, int, int, int, int, int);
static int cs4231_ad_start_play(int, int);
static void cs4231_ad_pause_play(int, int);
static void cs4231_ad_stop_play(int, int);
static int cs4231_ad_start_record(int, int);
static void cs4231_ad_stop_record(int, int);

/*
 * Global variables, but viewable only by this file.
 */

/* anchor for soft state structures */
static void *cs_statep;

/* don't filter the higher sample rates, this causes the high to be lost */
static am_ad_src1_info_t cs_play_sample_rates_info[] = {
	{ CS4231_SAMPR8000, CS4231_SAMPR48000, 2,	/* up 6, down 1 */
	    3, (2|AM_FILTER), 0, 0, 1, 1, 0, 0, 3 },
	{ CS4231_SAMPR9600, CS4231_SAMPR48000, 1,	/* up 5, down 1 */
	    (5|AM_FILTER), 0, 0, 0, 1, 0, 0, 0, 3 },
	{ CS4231_SAMPR11025, CS4231_SAMPR48000, 3,	/* up 640, down 147 */
	    10, 8, (8|AM_FILTER), 0, 7, 7, 3, 0, 3 },
	{ CS4231_SAMPR16000, CS4231_SAMPR48000, 1,	/* up 3, down 1 */
	    (3|AM_FILTER), 0, 0, 0, 1, 0, 0, 0, 3 },
	{ CS4231_SAMPR18900, CS4231_SAMPR48000, 3,	/* up 160, down 63 */
	    8, 5, (4|AM_FILTER), 0, 7, 3, 3, 0, 3 },
	{ CS4231_SAMPR22050, CS4231_SAMPR48000, 3,	/* up 320, down 147 */
	    10, 8, (4|AM_FILTER), 0, 7, 7, 3, 0, 3 },
	{ CS4231_SAMPR32000, CS4231_SAMPR48000, 1,	/* up 3, down 2 */
	    (3|AM_FILTER), 0, 0, 0, 2, 0, 0, 0, 3 },
	{ CS4231_SAMPR33075, CS4231_SAMPR48000, 4,	/* up 640, down 441 */
	    8, 5, 4, 4, 7, 7, 3, 3, 3 },
	{ CS4231_SAMPR37800, CS4231_SAMPR48000, 3,	/* up 80, down 63 */
	    5, 4, 4, 0, 7, 3, 3, 0, 3 },
	{ CS4231_SAMPR44100, CS4231_SAMPR48000, 3,	/* up 160, down 147 */
	    8, 5, 4, 0, 7, 7, 3, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR48000, 1,	/* up 1, down 1 */
	    1, 0, 0, 0, 1, 0, 0, 0, 3 },
	{ 0 }
};

static am_ad_src1_info_t cs_record_sample_rates_info[] = {
	{ CS4231_SAMPR48000, CS4231_SAMPR8000, 1,	/* up 1, down 6 */
	    1, 0, 0, 0, 6, 0, 0, 0, 0 },
	{ CS4231_SAMPR48000, CS4231_SAMPR9600, 1,	/* up 1, down 5 */
	    1, 0, 0, 0, 5, 0, 0, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR11025, 3,	/* up 147, down 640 */
	    7, 7, (3|AM_FILTER), 0, 10, 8, 8, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR16000, 1,	/* up 1, down 3 */
	    1, 0, 0, 0, 3, 0, 0, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR18900, 3,	/* up 63, down 160 */
	    7, 3, (3|AM_FILTER), 0, 8, 5, 4, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR22050, 3,	/* up 147, down 320 */
	    7, 7, (3|AM_FILTER), 0, 10, 8, 4, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR32000, 1,	/* up 2, down 3 */
	    (2|AM_FILTER), 0, 0, 0, 3, 0, 0, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR33075, 4,	/* up 441, down 640 */
	    7, 7, 3, 3, 8, 5, 4, 4, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR37800, 3,	/* up 63, down 80 */
	    7, 3, 3, 0, 5, 4, 4, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR44100, 3,	/* up 147, down 160 */
	    7, 7, 3, 0, 8, 5, 4, 0, 3 },
	{ CS4231_SAMPR48000, CS4231_SAMPR48000, 1,	/* up 1, down 1 */
	    1, 0, 0, 0, 1, 0, 0, 0, 3 },
	{ 0 }
};

static uint_t cs_mixer_srs[] = {
	CS4231_SAMPR8000, CS4231_SAMPR9600, CS4231_SAMPR11025,
	CS4231_SAMPR16000, CS4231_SAMPR18900, CS4231_SAMPR22050,
	CS4231_SAMPR32000, CS4231_SAMPR33075, CS4231_SAMPR37800,
	CS4231_SAMPR44100, CS4231_SAMPR48000, 0
};

static uint_t cs_compat_srs[] = {
	CS4231_SAMPR5510, CS4231_SAMPR6620, CS4231_SAMPR8000,
	CS4231_SAMPR9600, CS4231_SAMPR11025, CS4231_SAMPR16000,
	CS4231_SAMPR18900, CS4231_SAMPR22050, CS4231_SAMPR27420,
	CS4231_SAMPR32000, CS4231_SAMPR33075, CS4231_SAMPR37800,
	CS4231_SAMPR44100, CS4231_SAMPR48000, 0
};

static am_ad_sample_rates_t cs_mixer_sample_rates = {
	MIXER_SRS_FLAG_SR_NOT_LIMITS,
	cs_mixer_srs
};

static am_ad_sample_rates_t cs_compat_sample_rates = {
	MIXER_SRS_FLAG_SR_NOT_LIMITS,
	cs_compat_srs
};

static uint_t cs_channels[] = {
	AUDIO_CHANNELS_MONO, AUDIO_CHANNELS_STEREO, 0
};

static am_ad_cap_comb_t cs_combinations[] = {
	{ AUDIO_PRECISION_8, AUDIO_ENCODING_LINEAR },
	{ AUDIO_PRECISION_8, AUDIO_ENCODING_ULAW },
	{ AUDIO_PRECISION_8, AUDIO_ENCODING_ALAW },
	{ AUDIO_PRECISION_16, AUDIO_ENCODING_LINEAR },
	{ 0 }
};

static am_ad_entry_t cs_entry = {
	cs4231_ad_setup,	/* ad_setup() */
	cs4231_ad_teardown,	/* ad_teardown() */
	cs4231_ad_set_config,	/* ad_set_config() */
	cs4231_ad_set_format,	/* ad_set_format() */
	cs4231_ad_start_play,	/* ad_start_play() */
	cs4231_ad_pause_play,	/* ad_pause_play() */
	cs4231_ad_stop_play,	/* ad_stop_play() */
	cs4231_ad_start_record,	/* ad_start_record() */
	cs4231_ad_stop_record,	/* ad_stop_record() */
	NULL,			/* ad_ioctl() */
	NULL			/* ad_iocdata() */
};

/* play gain array, converts linear gain to 64 steps of log10 gain */
static uint8_t cs4231_atten[] = {
	0x3f,	0x3e,	0x3d,	0x3c,	0x3b,	/* [000] -> [004] */
	0x3a,	0x39,	0x38,	0x37,	0x36,	/* [005] -> [009] */
	0x35,	0x34,	0x33,	0x32,	0x31,	/* [010] -> [014] */
	0x30,	0x2f,	0x2e,	0x2d,	0x2c,	/* [015] -> [019] */
	0x2b,	0x2a,	0x29,	0x29,	0x28,	/* [020] -> [024] */
	0x28,	0x27,	0x27,	0x26,	0x26,	/* [025] -> [029] */
	0x25,	0x25,	0x24,	0x24,	0x23,	/* [030] -> [034] */
	0x23,	0x22,	0x22,	0x21,	0x21,	/* [035] -> [039] */
	0x20,	0x20,	0x1f,	0x1f,	0x1f,	/* [040] -> [044] */
	0x1e,	0x1e,	0xe,	0x1d,	0x1d,	/* [045] -> [049] */
	0x1d,	0x1c,	0x1c,	0x1c,	0x1b,	/* [050] -> [054] */
	0x1b,	0x1b,	0x1a,	0x1a,	0x1a,	/* [055] -> [059] */
	0x1a,	0x19,	0x19,	0x19,	0x19,	/* [060] -> [064] */
	0x18,	0x18,	0x18,	0x18,	0x17,	/* [065] -> [069] */
	0x17,	0x17,	0x17,	0x16,	0x16,	/* [070] -> [074] */
	0x16,	0x16,	0x16,	0x15,	0x15,	/* [075] -> [079] */
	0x15,	0x15,	0x15,	0x14,	0x14,	/* [080] -> [084] */
	0x14,	0x14,	0x14,	0x13,	0x13,	/* [085] -> [089] */
	0x13,	0x13,	0x13,	0x12,	0x12,	/* [090] -> [094] */
	0x12,	0x12,	0x12,	0x12,	0x11,	/* [095] -> [099] */
	0x11,	0x11,	0x11,	0x11,	0x11,	/* [100] -> [104] */
	0x10,	0x10,	0x10,	0x10,	0x10,	/* [105] -> [109] */
	0x10,	0x0f,	0x0f,	0x0f,	0x0f,	/* [110] -> [114] */
	0x0f,	0x0f,	0x0e,	0x0e,	0x0e,	/* [114] -> [119] */
	0x0e,	0x0e,	0x0e,	0x0e,	0x0d,	/* [120] -> [124] */
	0x0d,	0x0d,	0x0d,	0x0d,	0x0d,	/* [125] -> [129] */
	0x0d,	0x0c,	0x0c,	0x0c,	0x0c,	/* [130] -> [134] */
	0x0c,	0x0c,	0x0c,	0x0b,	0x0b,	/* [135] -> [139] */
	0x0b,	0x0b,	0x0b,	0x0b,	0x0b,	/* [140] -> [144] */
	0x0b,	0x0a,	0x0a,	0x0a,	0x0a,	/* [145] -> [149] */
	0x0a,	0x0a,	0x0a,	0x0a,	0x09,	/* [150] -> [154] */
	0x09,	0x09,	0x09,	0x09,	0x09,	/* [155] -> [159] */
	0x09,	0x09,	0x08,	0x08,	0x08,	/* [160] -> [164] */
	0x08,	0x08,	0x08,	0x08,	0x08,	/* [165] -> [169] */
	0x08,	0x07,	0x07,	0x07,	0x07,	/* [170] -> [174] */
	0x07,	0x07,	0x07,	0x07,	0x07,	/* [175] -> [179] */
	0x06,	0x06,	0x06,	0x06,	0x06,	/* [180] -> [184] */
	0x06,	0x06,	0x06,	0x06,	0x05,	/* [185] -> [189] */
	0x05,	0x05,	0x05,	0x05,	0x05,	/* [190] -> [194] */
	0x05,	0x05,	0x05,	0x05,	0x04,	/* [195] -> [199] */
	0x04,	0x04,	0x04,	0x04,	0x04,	/* [200] -> [204] */
	0x04,	0x04,	0x04,	0x04,	0x03,	/* [205] -> [209] */
	0x03,	0x03,	0x03,	0x03,	0x03,	/* [210] -> [214] */
	0x03,	0x03,	0x03,	0x03,	0x03,	/* [215] -> [219] */
	0x02,	0x02,	0x02,	0x02,	0x02,	/* [220] -> [224] */
	0x02,	0x02,	0x02,	0x02,	0x02,	/* [225] -> [229] */
	0x02,	0x01,	0x01,	0x01,	0x01,	/* [230] -> [234] */
	0x01,	0x01,	0x01,	0x01,	0x01,	/* [235] -> [239] */
	0x01,	0x01,	0x01,	0x00,	0x00,	/* [240] -> [244] */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* [245] -> [249] */
	0x00,	0x00,	0x00,	0x00,	0x00,	/* [250] -> [254] */
	0x00					/* [255] */
};

/*
 * STREAMS Structures
 */

/* STREAMS driver id and limit value structure */
static struct module_info cs4231_modinfo = {
	CS4231_IDNUM,		/* module ID number */
	CS4231_NAME,		/* module name */
	CS4231_MINPACKET,	/* minimum packet size */
	CS4231_MAXPACKET,	/* maximum packet size */
	CS4231_HIWATER,		/* high water mark */
	CS4231_LOWATER		/* low water mark */
};

/* STREAMS queue processing procedures structures */
/* read queue */
static struct qinit cs4231_rqueue = {
	audio_sup_rput,		/* put procedure */
	audio_sup_rsvc,		/* service procedure */
	audio_sup_open,		/* open procedure */
	audio_sup_close,	/* close procedure */
	NULL,			/* unused */
	&cs4231_modinfo,	/* module parameters */
	NULL			/* module statistics */
};

/* write queue */
static struct qinit cs4231_wqueue = {
	audio_sup_wput,		/* put procedure */
	audio_sup_wsvc,		/* service procedure */
	NULL,			/* open procedure */
	NULL,			/* close procedure */
	NULL,			/* unused */
	&cs4231_modinfo,	/* module parameters */
	NULL			/* module statistics */
};

/* STREAMS entity declaration structure */
static struct streamtab cs4231_str_info = {
	&cs4231_rqueue,		/* read queue */
	&cs4231_wqueue,		/* write queue */
	NULL,			/* mux lower read queue */
	NULL,			/* mux lower write queue */
};

/*
 * DDI Structures
 */

/* Entry points structure */
static struct cb_ops cs4231_cb_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&cs4231_str_info,	/* cb_str */
	D_NEW|D_MP|D_64BIT,	/* cb_flag */
	CB_REV,			/* cb_rev */
	nodev,			/* cb_aread */
	nodev,			/* cb_arwite */
};

/* Device operations structure */
static struct dev_ops cs4231_dev_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	cs4231_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify - obsolete */
	nulldev,		/* devo_probe - not needed */
	cs4231_attach,		/* devo_attach */
	cs4231_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cs4231_cb_ops,		/* devi_cb_ops */
	NULL,			/* devo_bus_ops */
	cs4231_power		/* devo_power */
};

/* Linkage structure for loadable drivers */
static struct modldrv cs4231_modldrv = {
	&mod_driverops,		/* drv_modops */
	CS4231_MOD_NAME,	/* drv_linkinfo */
	&cs4231_dev_ops		/* drv_dev_ops */
};

/* Module linkage structure */
static struct modlinkage cs4231_modlinkage = {
	MODREV_1,			/* ml_rev */
	(void *)&cs4231_modldrv,	/* ml_linkage */
	NULL				/* NULL terminates the list */
};


/* *******  Loadable Module Configuration Entry Points  ********************* */

/*
 * _init()
 *
 * Description:
 *	Driver initialization, called when driver is first loaded.
 *	This is how access is initially given to all the static structures.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	ddi_soft_state_init() status, see ddi_soft_state_init(9f), or
 *	mod_install() status, see mod_install(9f)
 */
int
_init(void)
{
	int		error;

	ATRACE("in audiocs _init()", 0);

	/* initialize the soft state */
	if ((error = ddi_soft_state_init(&cs_statep, sizeof (CS_state_t), 0)) !=
	    DDI_SUCCESS) {
		ATRACE("audiocs ddi_soft_state_init() failed", cs_statep);
		return (error);
	}

	if ((error = mod_install(&cs4231_modlinkage)) != DDI_SUCCESS) {
		ddi_soft_state_fini(&cs_statep);
	}

	ATRACE("aduiocs _init() cs_statep", cs_statep);

	ATRACE_32("audiocs _init() returning", error);

	return (error);
}

/*
 * _fini()
 *
 * Description:
 *	Module de-initialization, called when the driver is to be unloaded.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	mod_remove() status, see mod_remove(9f)
 */
int
_fini(void)
{
	int		error;

	ATRACE("in audiocs _fini()", cs_statep);

	if ((error = mod_remove(&cs4231_modlinkage)) == DDI_SUCCESS) {
		/* free the soft state internal structures */
		ddi_soft_state_fini(&cs_statep);
	}

	ATRACE_32("audiocs _fini() returning", error);

	return (error);
}

/*
 * _info()
 *
 * Description:
 *	Module information, returns infomation about the driver.
 *
 * Arguments:
 *	modinfo *modinfop	Pointer to the opaque modinfo structure
 *
 * Returns:
 *	mod_info() status, see mod_info(9f)
 */
int
_info(struct modinfo *modinfop)
{
	int		error;

	ATRACE("in audiocs _info()", 0);

	error = mod_info(&cs4231_modlinkage, modinfop);

	ATRACE_32("audiocs _info() returning", error);

	return (error);
}

/* *******  Driver Entry Points  ******************************************** */

/*
 * cs4231_attach()
 *
 * Description:
 *	Attach an instance of the CS4231 driver. This routine does the device
 *	dependent attach tasks. When it is complete it calls audio_sup_attach()
 *	and am_attach() so they may do their work.
 *
 *	NOTE: mutex_init() no longer needs a name string, so set
 *		to NULL to save kernel space.
 *
 * Arguments:
 *	dev_info_t	*dip	Pointer to the device's dev_info struct.
 *	ddi_attach_cmd_t cmd	Attach command
 *
 * Returns:
 *	DDI_SUCCESS		If the driver was initialized properly
 *	DDI_FAILURE		If the driver couldn't be initialized properly
 */
static int
cs4231_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	CS_state_t		*state;
	char			*name = "audiocs0";
	char			*tmp = (char *)NULL;
	char			*pm_comp[] = {
					"NAME=audiocs audio device",
					"0=off",
					"1=on" };
	ddi_acc_handle_t	handle;
	size_t			cbuf_size;
	size_t			pbuf_size;
	int			cs4231_pints;
	int			cs4231_rints;
	int			instance;
	int			proplen;

	ATRACE("in cs_attach()", dip);

	instance = ddi_get_instance(dip);
	ATRACE_32("cs_attach() instance", instance);
	ATRACE("cs_attach() cs_statep", cs_statep);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		ATRACE("cs_attach() DDI_RESUME", 0);

		/* we've already allocated the state structure so get ptr */
		if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
			cmn_err(CE_NOTE,
			    "audiocs: attach() RESUME get soft state failed");
			return (DDI_FAILURE);
		}

		handle = state->cs_handles.cs_codec_hndl;

		/* make sure we haven't already resumed */
		mutex_enter(&state->cs_lock);
		if (!state->cs_suspended) {
			ATRACE("cs_attach() resuming return success", 0);
			mutex_exit(&state->cs_lock);
			return (DDI_SUCCESS);
		}
		mutex_exit(&state->cs_lock);

#ifdef NEW_PM
		if (pm_raise_power(dip, CS4231_COMPONENT, CS4231_PWR_ON) ==
		    DDI_FAILURE) {
			return (DDI_FAILURE);
		}
		mutex_enter(&state->cs_lock);
#else
		mutex_enter(&state->cs_lock);
		/* make sure the Codec has power */
		while (state->cs_powered == CS4231_PWR_OFF) {
			mutex_exit(&state->cs_lock);
			if (cs4231_power(dip, CS4231_COMPONENT,
			    CS4231_PWR_OFF) == DDI_FAILURE) {
				ATRACE("cs_attach() raise power failed", state);
				return (DDI_FAILURE);
			}
			mutex_enter(&state->cs_lock);
		}

#endif
		ASSERT(mutex_owned(&state->cs_lock));

		state->cs_suspended = 0;

		/* now restart playing and recording */
		(void) CS4231_DMA_START_PLAY(state);
		(void) CS4231_DMA_START_RECORD(state);
		REG_SELECT(handle, &CS4231_IAR, INTC_REG);
		OR_SET_BYTE(handle, &CS4231_IDR, INTC_CEN);

		mutex_exit(&state->cs_lock);

		ATRACE("cs_attach() DDI_RESUME succeeded", 0);

		return (DDI_SUCCESS);
	default:
		ATRACE_32("cs_attach() unknown command failure", cmd);
		cmn_err(CE_NOTE, "audiocs: attach() unknown command 0x%x", cmd);
		return (DDI_FAILURE);
	}

	/* allocate the state structure */
	if (ddi_soft_state_zalloc(cs_statep, instance) == DDI_FAILURE) {
		cmn_err(CE_NOTE,
		    "audiocs: attach() soft state allocate failed");
		return (DDI_FAILURE);
	}

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: attach() get soft state failed");
		return (DDI_FAILURE);
	}

	/* get the play and record unterrupts per second */
	cs4231_pints = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_CANSLEEP,
	    "cs4231_pints", CS4231_INTS);
	ATRACE_32("cs_attach() play interrupts per sec", cs4231_pints);

	cs4231_rints = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_CANSLEEP,
	    "cs4231_rints", CS4231_INTS);
	ATRACE_32("cs_attach() record interrupts per sec", cs4231_rints);

	if (cs4231_pints < CS4231_MIN_INTS) {
		ATRACE_32(
		    "cs_attach() play interrupt rate set too low, resetting",
		    cs4231_pints);
		cmn_err(CE_NOTE, "audiocs: attach() "
		    "play interrupt rate set too low, %d, resetting to %d",
		    cs4231_pints, CS4231_INTS);
		cs4231_pints = CS4231_INTS;
	} else if (cs4231_pints > CS4231_MAX_INTS) {
		ATRACE_32(
		    "cs_attach() play interrupt rate set too high, resetting",
		    cs4231_pints);
		cmn_err(CE_NOTE, "audiocs: attach() "
		    "play interrupt rate set too high, %d, resetting to %d",
		    cs4231_pints, CS4231_INTS);
		cs4231_pints = CS4231_INTS;
	}
	if (cs4231_rints < CS4231_MIN_INTS) {
		ATRACE_32(
		    "cs_attach() record interrupt rate set too low, resetting",
		    cs4231_rints);
		cmn_err(CE_NOTE, "audiocs: attach() "
		    "record interrupt rate set too low, %d, resetting to %d",
		    cs4231_rints, CS4231_INTS);
		cs4231_rints = CS4231_INTS;
	} else if (cs4231_rints > CS4231_MAX_INTS) {
		ATRACE_32(
		    "cs_attach() record interrupt rate set too high, resetting",
		    cs4231_rints);
		cmn_err(CE_NOTE, "audiocs: attach() "
		    "record interrupt rate set too high, %d, resetting to %d",
		    cs4231_rints, CS4231_INTS);
		cs4231_rints = CS4231_INTS;
	}

	/* get the mode from the .conf file */
	state->cs_ad_info.ad_mode = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_CANSLEEP, "cs4231_mode", AM_MIXER_MODE);
	ATRACE_32("cs_attach() setting mode", state->cs_ad_info.ad_mode);

	/* set up the pm-components */
	if (ddi_prop_update_string_array(DDI_DEV_T_NONE, dip,
	    "pm-components", pm_comp, 3) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: couldn't create component");
		return (DDI_FAILURE);
	}

	/* now fill it in, initialize the state mutexs first */
	mutex_init(&state->cs_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&state->cs_swlock, NULL, MUTEX_DRIVER, NULL);

	/* save the device info pointer */
	state->cs_dip = dip;

	/* fill in the device default state */
	state->cs_defaults.play.sample_rate = CS4231_DEFAULT_SR;
	state->cs_defaults.play.channels = CS4231_DEFAULT_CH;
	state->cs_defaults.play.precision = CS4231_DEFAULT_PREC;
	state->cs_defaults.play.encoding = CS4231_DEFAULT_ENC;
	state->cs_defaults.play.gain = CS4231_DEFAULT_PGAIN;
	state->cs_defaults.play.port = AUDIO_SPEAKER;
	state->cs_defaults.play.buffer_size = CS4231_BSIZE;
	state->cs_defaults.play.balance = CS4231_DEFAULT_BAL;
	state->cs_defaults.record.sample_rate = CS4231_DEFAULT_SR;
	state->cs_defaults.record.channels = CS4231_DEFAULT_CH;
	state->cs_defaults.record.precision = CS4231_DEFAULT_PREC;
	state->cs_defaults.record.encoding = CS4231_DEFAULT_ENC;
	state->cs_defaults.record.gain = CS4231_DEFAULT_PGAIN;
	state->cs_defaults.record.port = AUDIO_MICROPHONE;
	state->cs_defaults.record.buffer_size = CS4231_BSIZE;
	state->cs_defaults.record.balance = CS4231_DEFAULT_BAL;
	state->cs_defaults.monitor_gain = CS4231_DEFAULT_MONITOR_GAIN;
	state->cs_defaults.output_muted = B_FALSE;
	state->cs_defaults.hw_features =
	    AUDIO_HWFEATURE_DUPLEX|AUDIO_HWFEATURE_IN2OUT;
	state->cs_defaults.sw_features = AUDIO_SWFEATURE_MIXER;

	/* fill in the ad_info structure */
	state->cs_ad_info.ad_int_vers = AMAD_VERS1;

	state->cs_ad_info.ad_add_mode = 0;
	state->cs_ad_info.ad_codec_type = AM_TRAD_CODEC;
	state->cs_ad_info.ad_defaults = &state->cs_defaults;
	state->cs_ad_info.ad_play_comb = cs_combinations;
	state->cs_ad_info.ad_rec_comb = cs_combinations;
	state->cs_ad_info.ad_entry = &cs_entry;
	state->cs_ad_info.ad_dev_info = &state->cs_dev_info;
	state->cs_ad_info.ad_diag_flags = 0;
	state->cs_ad_info.ad_diff_flags = AM_DIFF_CH|AM_DIFF_PREC|AM_DIFF_ENC;
	state->cs_ad_info.ad_assist_flags = AM_ASSIST_MIC;
	state->cs_ad_info.ad_misc_flags = AM_MISC_RP_EXCL|AM_MISC_MONO_DUP;
	state->cs_ad_info.ad_translate_flags =
	    AM_MISC_8_P_TRANSLATE|AM_MISC_8_R_TRANSLATE;
	state->cs_ad_info.ad_num_mics = 1;

	/* play capabilities */
	state->cs_ad_info.ad_play.ad_mixer_srs = cs_mixer_sample_rates;
	state->cs_ad_info.ad_play.ad_compat_srs = cs_compat_sample_rates;
	state->cs_ad_info.ad_play.ad_conv = &am_src1;
	state->cs_ad_info.ad_play.ad_sr_info = cs_play_sample_rates_info;
	state->cs_ad_info.ad_play.ad_chs = cs_channels;
	state->cs_ad_info.ad_play.ad_int_rate = cs4231_pints;
	state->cs_ad_info.ad_play.ad_flags = 0;
	state->cs_ad_info.ad_play.ad_bsize = CS4231_BSIZE;

	/* record capabilities */
	state->cs_ad_info.ad_record.ad_mixer_srs = cs_mixer_sample_rates;
	state->cs_ad_info.ad_record.ad_compat_srs = cs_compat_sample_rates;
	state->cs_ad_info.ad_record.ad_conv = &am_src1;
	state->cs_ad_info.ad_record.ad_sr_info = cs_record_sample_rates_info;
	state->cs_ad_info.ad_record.ad_chs = cs_channels;
	state->cs_ad_info.ad_record.ad_int_rate = cs4231_rints;
	state->cs_ad_info.ad_record.ad_flags = 0;
	state->cs_ad_info.ad_record.ad_bsize = CS4231_BSIZE;

	/* figure out which DMA engine hardware we have */
	tmp = NULL;
	if ((ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP, "dma-model", (caddr_t)&tmp,
	    &proplen) == DDI_PROP_SUCCESS) && (strcmp(tmp, "eb2dma") == 0)) {
		ATRACE("cs_attach() eb2dma", state);
		state->cs_dma_engine = EB2_DMA;
		state->cs_dma_ops = &cs4231_eb2dma_ops;
	} else {	/* default */
		ATRACE("cs_attach() apcdma", state);
		state->cs_dma_engine = APC_DMA;
		state->cs_dma_ops = &cs4231_apcdma_ops;
	}
	if (tmp) {	/* we don't need this property string, so free it */
		kmem_free(tmp, proplen);
	}

	/* cs_regs, cs_eb2_regs and cs_handles filled in later */

	(void) strcpy(&state->cs_dev_info.name[0], CS_DEV_NAME);
	/* always set to onboard1, not really correct, but very high runner */
	(void) strcpy(&state->cs_dev_info.config[0], CS_DEV_CONFIG_ONBRD1);
	/* version filled in below */


	/* set up the max and # of channels pointers */
	state->cs_max_chs =	CS4231_MAX_CHANNELS;
	state->cs_max_p_chs =	CS4231_MAX_CHANNELS;
	state->cs_max_r_chs =	CS4231_MAX_CHANNELS;
	state->cs_chs =		0;
	state->cs_p_chs =	0;
	state->cs_r_chs =	0;

	/* most of what's left is filled in when the registers are mapped */

	/*
	 * Now we get which audiocs h/w version we have. We use this to
	 * determine the input and output ports, as well whether or not
	 * the hardware has internal loopbacks or not. We also have three
	 * different ways for the properties to be specified, so we also
	 * need to worry about that.
	 *
	 * Vers	Platform(s)	DMA eng.	audio-module**	loopback
	 * a    SS-4+/SS-5+	apcdma		no		no
	 * b	Ultra-1&2	apcdma		no		yes
	 * c	positron	apcdma		no		yes
	 * d	PPC - retired
	 * e	x86 - retired
	 * f	tazmo		eb2dma		Perigee		no
	 * g	tazmo		eb2dma		Quark		yes
	 * h	darwin+		eb2dma		no		N/A
	 *
	 * Vers	model~		aux1*		aux2*
	 * a	N/A		N/A		N/A
	 * b	N/A		N/A		N/A
	 * c	N/A		N/A		N/A
	 * d	retired
	 * e	retired
	 * f	SUNW,CS4231f	N/A		N/A
	 * g	SUNW,CS4231g	N/A		N/A
	 * h	SUNW,CS4231h	cdrom		none
	 *
	 * *   = Replaces internal-loopback for latest property type, can be
	 *	 set to "cdrom", "loopback", or "none".
	 *
	 * **  = For plugin audio modules only. Starting with darwin, this
	 *	 property is replaces by the model property.
	 *
	 * ~   = Replaces audio-module.
	 *
	 * +   = Has the capability of having a cable run from the internal
	 *	 CD-ROM to the audio device.
	 *
	 * N/A = Not applicable, the property wasn't created for early
	 *	 platforms, or the property has been retired.
	 *
	 * NOTE: Older tazmo and quark machines don't have the model property.
	 *
	 * First we set the common ports, etc.
	 */
	state->cs_defaults.play.avail_ports =
	    AUDIO_SPEAKER|AUDIO_HEADPHONE|AUDIO_LINE_OUT;
	state->cs_defaults.play.mod_ports =
	    AUDIO_SPEAKER|AUDIO_HEADPHONE|AUDIO_LINE_OUT;
	state->cs_defaults.record.avail_ports =
	    AUDIO_MICROPHONE|AUDIO_LINE_IN|AUDIO_CODEC_LOOPB_IN;
	state->cs_defaults.record.mod_ports |=
	    AUDIO_MICROPHONE|AUDIO_LINE_IN|AUDIO_CODEC_LOOPB_IN;
	state->cs_cd_input_line = NO_INTERNAL_CD;

	/* now we try the new "model" property */
	tmp = NULL;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP, "model", (caddr_t)&tmp,
	    &proplen) == DDI_PROP_SUCCESS) {
		if (strncmp(tmp, "SUNW,CS4231h", proplen) == 0) {
			/* darwin */
			ATRACE("cs_attach() NEW - darwin", state);
			(void) strcpy(&state->cs_dev_info.version[0],
			    CS_DEV_VERSION_H);
			state->cs_defaults.record.avail_ports |=
			    AUDIO_INTERNAL_CD_IN;
			state->cs_defaults.play.mod_ports =
			    AUDIO_SPEAKER;
			state->cs_defaults.record.avail_ports |= AUDIO_CD;
			state->cs_defaults.record.mod_ports |= AUDIO_CD;
			state->cs_cd_input_line = INTERNAL_CD_ON_AUX1;
		} else if (strncmp(tmp, "SUNW,CS4231g", proplen) == 0) {
			/* quark audio module */
			ATRACE("cs_attach() NEW - quark", state);
			(void) strcpy(&state->cs_dev_info.version[0],
			    CS_DEV_VERSION_G);
			state->cs_defaults.record.avail_ports |= AUDIO_SUNVTS;
			state->cs_defaults.record.mod_ports |= AUDIO_SUNVTS;
		} else if (strncmp(tmp, "SUNW,CS4231f", proplen) == 0) {
			/* tazmo */
			ATRACE("cs_attach() NEW - tazmo", state);
			(void) strcpy(&state->cs_dev_info.version[0],
			    CS_DEV_VERSION_F);
		} else {
			ATRACE("cs_attach() NEW - unknown", state);
			(void) strcpy(&state->cs_dev_info.version[0], "?");
			cmn_err(CE_NOTE,
			    "audiocs: attach() unknown audio model: %s, "
			    "some parts of audio may not work correctly",
			    (tmp ? tmp : "unknown"));
		}
		kmem_free(tmp, proplen);	/* done with the property */
	} else {	/* now try the older "audio-module" property */
		tmp = NULL;	/* make sure */
		if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
		    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP, "audio-module",
		    (caddr_t)&tmp, &proplen) == DDI_PROP_SUCCESS) {
			switch (*tmp) {
			case 'Q':	/* quark audio module */
				ATRACE("cs_attach() OLD - quark", state);
				(void) strcpy(&state->cs_dev_info.version[0],
				    CS_DEV_VERSION_G);
				state->cs_defaults.record.avail_ports |=
				    AUDIO_SUNVTS;
				state->cs_defaults.record.mod_ports |=
				    AUDIO_SUNVTS;
				break;
			case 'P':	/* tazmo */
				ATRACE("cs_attach() OLD - tazmo", state);
				(void) strcpy(&state->cs_dev_info.version[0],
				    CS_DEV_VERSION_F);
				break;
			default:
				ATRACE("cs_attach() OLD - unknown", state);
				(void) strcpy(&state->cs_dev_info.version[0],
					"?");
				cmn_err(CE_NOTE,
				    "audiocs: attach() unknown audio "
				    "module: %s, some parts of audio may "
				    "not work correctly",
				    (tmp ? tmp : "unknown"));
				break;
			}
			kmem_free(tmp, proplen);	/* done with the prop */
		} else {	/* now try heuristics, ;-( */
			if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			    "internal-loopback", B_FALSE)) {
				if (state->cs_dma_engine == EB2_DMA) {
				    ATRACE("cs_attach() OLD - C", state);
				    (void) strcpy(
					&state->cs_dev_info.version[0],
					CS_DEV_VERSION_C);
				} else {
				    ATRACE("cs_attach() OLD - B", state);
				    (void) strcpy(
					&state->cs_dev_info.version[0],
					CS_DEV_VERSION_B);
				}
				state->cs_defaults.record.avail_ports |=
				    AUDIO_SUNVTS;
				state->cs_defaults.record.mod_ports |=
				    AUDIO_SUNVTS;
			} else {
				ATRACE("cs_attach() ANCIENT - A", state);
				(void) strcpy(&state->cs_dev_info.version[0],
				    CS_DEV_VERSION_A);
				state->cs_defaults.record.avail_ports |=
				    AUDIO_INTERNAL_CD_IN;
				state->cs_defaults.record.mod_ports |=
				    AUDIO_INTERNAL_CD_IN;
				state->cs_cd_input_line = INTERNAL_CD_ON_AUX1;
			}
		}
	}

	/*
	 * Figure out the largest transfer size for the DMA engine. Then
	 * map in the CS4231 and the DMA registers and reset the DMA engine.
	 */
	pbuf_size = CS4231_SAMPR48000 * AUDIO_CHANNELS_STEREO *
	    (AUDIO_PRECISION_16 >> AUDIO_PRECISION_SHIFT) / cs4231_pints;
	cbuf_size = CS4231_SAMPR48000 * AUDIO_CHANNELS_STEREO *
	    (AUDIO_PRECISION_16 >> AUDIO_PRECISION_SHIFT) / cs4231_rints;
	if (CS4231_DMA_MAP_REGS(dip, state, pbuf_size, cbuf_size) ==
	    AUDIO_FAILURE) {
		goto error_mem;
	}

	/* make sure we are powered up */
	mutex_enter(&state->cs_lock);
	CS4231_DMA_POWER(state, CS4231_PWR_ON);
	mutex_exit(&state->cs_lock);
	state->cs_suspended = 0;
	state->cs_powered = CS4231_PWR_ON;
	state->cs_busy_cnt = 0;

	ATRACE("cs_attach() calling DMA_RESET()", state);
	CS4231_DMA_RESET(state);

	/* no autocalibrate */
	state->cs_autocal = B_FALSE;

	/* initialize the Codec */
	handle = state->cs_handles.cs_codec_hndl;

	/* activate registers 16 -> 31 */
	REG_SELECT(handle, &CS4231_IAR, MID_REG);
	ddi_put8(handle, &CS4231_IDR, MID_MODE2);

	/* now figure out what version we have */
	REG_SELECT(handle, &CS4231_IAR, VID_REG);
	if (ddi_get8(handle, &CS4231_IDR) & VID_A) {
		ATRACE("cs_attach() revA", state);
		state->cs_revA = B_TRUE;
	} else {
		ATRACE("cs_attach() !revA", state);
		state->cs_revA = B_FALSE;
	}

	/* get rid of annoying popping by muting the output channels */
	REG_SELECT(handle, &CS4231_IAR, LDACO_REG);
	DDI_PUT8(handle, &CS4231_IDR, (LDACO_LDM | LDACO_MID_GAIN));
	REG_SELECT(handle, &CS4231_IAR, RDACO_REG);
	DDI_PUT8(handle, &CS4231_IDR, (RDACO_RDM | RDACO_MID_GAIN));

	/* initialize aux input channels to known gain values & muted */
	REG_SELECT(handle, &CS4231_IAR, LAUX1_REG);
	DDI_PUT8(handle, &CS4231_IDR, (LAUX1_LX1M | LAUX1_UNITY_GAIN));
	REG_SELECT(handle, &CS4231_IAR, RAUX1_REG);
	DDI_PUT8(handle, &CS4231_IDR, (RAUX1_RX1M | RAUX1_UNITY_GAIN));
	REG_SELECT(handle, &CS4231_IAR, LAUX2_REG);
	DDI_PUT8(handle, &CS4231_IDR, (LAUX2_LX2M | LAUX2_UNITY_GAIN));
	REG_SELECT(handle, &CS4231_IAR, RAUX2_REG);
	DDI_PUT8(handle, &CS4231_IDR, (RAUX2_RX2M | RAUX2_UNITY_GAIN));

	/* initialize aux input channels to known gain values & muted */
	REG_SELECT(handle, &CS4231_IAR, LLIC_REG);
	DDI_PUT8(handle, &CS4231_IDR, (LLIC_LLM|LLIC_UNITY_GAIN));
	REG_SELECT(handle, &CS4231_IAR, RLIC_REG);
	DDI_PUT8(handle, &CS4231_IDR, (RLIC_RLM|RLIC_UNITY_GAIN));

	/* program the sample rate, play and capture must be the same */
	REG_SELECT(handle, &CS4231_IAR, (FSDF_REG | IAR_MCE));
	DDI_PUT8(handle, &CS4231_IDR, (FS_8000 | PDF_ULAW8 | PDF_MONO));
	REG_SELECT(handle, &CS4231_IAR, (CDF_REG | IAR_MCE));
	DDI_PUT8(handle, &CS4231_IDR, (CDF_ULAW8 | CDF_MONO));

	/*
	 * Set up the Codec for playback and capture disabled, dual DMA, and
	 * playback and capture DMA. Also, set autocal if we are supposed to.
	 */
	REG_SELECT(handle, &CS4231_IAR, (INTC_REG | IAR_MCE));
	if (state->cs_autocal == B_TRUE) {
		DDI_PUT8(handle, &CS4231_IDR,
		    (INTC_ACAL | INTC_DDC | INTC_PDMA | INTC_CDMA));
	} else {
		DDI_PUT8(handle, &CS4231_IDR,
		    (INTC_DDC | INTC_PDMA | INTC_CDMA));
	}

	/* turn off the MCE bit */
	REG_SELECT(handle, &CS4231_IAR, LADCI_REG);

	/* wait for the Codec before we continue XXX - do we need this? */
	if (cs4231_poll_ready(state) == AUDIO_FAILURE) {
		ATRACE("cs_attach() poll_ready() #1 failed", state);
		goto error_unmap;
	}

	/*
	 * Turn on the output level bit to be 2.8 Vpp. Also, don't go to 0 on
	 * underflow.
	 */
	REG_SELECT(handle, &CS4231_IAR, AFE1_REG);
	DDI_PUT8(handle, &CS4231_IDR, (AFE1_OLB|AFE1_DACZ));

	/* turn on the high pass filter if Rev A */
	REG_SELECT(handle, &CS4231_IAR, AFE2_REG);
	if (state->cs_revA) {
		DDI_PUT8(handle, &CS4231_IDR, (AFE2_HPF));
	} else {
		DDI_PUT8(handle, &CS4231_IDR, 0);
	}

	/* clear the play and capture interrupt flags */
	REG_SELECT(handle, &CS4231_IAR, AFS_REG);
	ddi_put8(handle, &CS4231_STATUS, (AFS_RESET_STATUS));

	/* the play and record gains will be set by the audio mixer */

	/* unmute the output */
	REG_SELECT(handle, &CS4231_IAR, LDACO_REG);
	AND_SET_BYTE(handle, &CS4231_IDR, ~LDACO_LDM);
	REG_SELECT(handle, &CS4231_IAR, RDACO_REG);
	AND_SET_BYTE(handle, &CS4231_IDR, ~RDACO_RDM);

	/* unmute the mono speaker and mute mono in */
	REG_SELECT(handle, &CS4231_IAR, MIOC_REG);
	DDI_PUT8(handle, &CS4231_IDR, MIOC_MIM);

	/* clear the mode change bit */
	REG_SELECT(handle, &CS4231_IAR, RDACO_REG);

	/* wait for the Codec before we continue XXX - do we need this? */
	if (cs4231_poll_ready(state) == AUDIO_FAILURE) {
		ATRACE("attach() poll_ready() #2 failed", state);
		goto error_unmap;
	}

	ATRACE("cs_attach() chip initialized", state);

	/* set up the interrupt handlers */
	ATRACE("cs_attach() calling DMA_ADD_INTR()", state);
	if (CS4231_DMA_ADD_INTR(state) != AUDIO_SUCCESS) {
		ATRACE("cs_attach() DMA_ADD_INTR() failed", state);
		goto error_unmap;
	}

	/* call the audio support module attach() routine */
	ATRACE("cs_attach() calling audio_sup_attach()", &state->cs_ad_info);
	if (audio_sup_attach(dip, cmd) == AUDIO_FAILURE) {
		ATRACE("cs_attach() audio_sup_attach() failed",
		    &state->cs_ad_info);
		goto error_destroy;
	}

	/* call the mixer attach() routine */
	ATRACE("cs_attach() calling am_attach()", &state->cs_ad_info);
	if (am_attach(dip, cmd, &state->cs_ad_info, &state->cs_swlock,
	    &state->cs_max_chs, &state->cs_max_p_chs, &state->cs_max_r_chs,
	    &state->cs_chs, &state->cs_p_chs, &state->cs_r_chs) ==
	    AUDIO_FAILURE) {
		ATRACE("cs_attach() am_attach() failed", &state->cs_ad_info);
		(void) audio_sup_detach(dip, DDI_DETACH);
		goto error_destroy;
	}

	/* when we load the driver we're idle */
	(void) pm_idle_component(dip, CS4231_COMPONENT);

	/* set up kernel statistics */
	if ((state->cs_ksp = kstat_create(name, instance, name, "controller",
	    KSTAT_TYPE_INTR, 1, KSTAT_FLAG_PERSISTENT)) != NULL) {
		kstat_install(state->cs_ksp);
	}

	/* everything worked out, so report the device */
	ddi_report_dev(dip);

	ATRACE("cs_attach() returning success", state);

	return (DDI_SUCCESS);

error_destroy:
	ATRACE("cs_attach() error_destroy", state);

	ATRACE("cs_attach() error_rem_intr", state);
	CS4231_DMA_REM_INTR(dip, state);
	mutex_destroy(&state->cs_lock);
	mutex_destroy(&state->cs_swlock);

error_unmap:
	ATRACE("cs_attach() error_unmap", state);
	CS4231_DMA_UNMAP_REGS(state);

error_mem:
	ATRACE("cs_attach() error_mem", state);
	ddi_soft_state_free(cs_statep, instance);

	ATRACE("cs_attach() returning failure", 0);

	return (DDI_FAILURE);

}	/* cs4231_attach() */

/*
 * cs4231_detach()
 *	Detach an instance of the CS4231 driver. After the Codec is detached
 *	we call am_detach() and audio_sup_detach() so they may do their work
 *
 *	Power management is pretty simple. If active we fail, otherwise
 *	we save the Codec state.
 *
 * Arguments:
 *	dev_info_t	*dip	Pointer to the device's dev_info struct.
 *	ddi_detach_cmd_t cmd	Detach command
 *
 * Returns:
 *	DDI_SUCCESS		If the driver was detached
 *	DDI_FAILURE		If the driver couldn't be detached
 */
static int
cs4231_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	CS_state_t		*state;
	int			instance;

	ATRACE_32("in cs_detach()", cmd);

	instance = ddi_get_instance(dip);
	ATRACE_32("cs_detach() instance", instance);
	ATRACE("cs_detach() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: detach() get soft state failed");
		return (DDI_FAILURE);
	}

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		ATRACE("cs_detach() DDI_SUSPEND failure", 0);

		mutex_enter(&state->cs_lock);

		/* make sure we aren't in the process of being suspended */
		if (state->cs_suspended) {
			ATRACE("cs_detach() suspending return failed", 0);
			mutex_exit(&state->cs_lock);
			return (DDI_SUCCESS);
		}

		state->cs_suspended = 1;

		/* stop playing and recording */
		CS4231_DMA_STOP_RECORD(state);
		CS4231_DMA_STOP_PLAY(state);

		mutex_exit(&state->cs_lock);

		/* now we can power down the Codec */
		if (cs4231_power(dip, CS4231_COMPONENT, CS4231_PWR_OFF) ==
		    DDI_FAILURE) {
			ATRACE("cs_attach() lower power failed", state);
			return (DDI_FAILURE);
		}

		ATRACE("cs_detach() SUSPEND successful", state);
		return (DDI_SUCCESS);
	default:
		ATRACE_32("cs_detach() unknown command failure", cmd);
		cmn_err(CE_NOTE, "audiocs: detach() unknown command 0x%x", cmd);
		return (DDI_FAILURE);
	}

	/*
	 * Call the mixer detach routine to tear down the mixer before
	 * we lose the hardware.
	 */
	ATRACE("cs_detach() calling am_detach()", dip);
	if (am_detach(dip, cmd) == AUDIO_FAILURE) {
		ATRACE_32("cs_detach() am_detach() failed", cmd);
		return (DDI_FAILURE);
	}
	ATRACE("cs_detach() calling audio_sup_detach()", dip);
	if (audio_sup_detach(dip, cmd) == AUDIO_FAILURE) {
		ATRACE_32("cs_detach() audio_sup_detach() failed", cmd);
		return (DDI_FAILURE);
	}

	ASSERT(state->cs_busy_cnt == 0);

	/* remove the interrupt handler */
	ATRACE("cs_detach() calling DMA_REM_INTR", state);
	CS4231_DMA_REM_INTR(dip, state);

	/* unmap the registers */
	CS4231_DMA_UNMAP_REGS(state);

	/* free the kernel statistics structure */
	if (state->cs_ksp) {
		kstat_delete(state->cs_ksp);
	}
	state->cs_ksp = NULL;

	/* destroy the state mutex */
	mutex_destroy(&state->cs_lock);
	mutex_destroy(&state->cs_swlock);

	/* free the memory for the state pointer */
	ddi_soft_state_free(cs_statep, instance);

	ATRACE("cs_detach() returning success", cs_statep);

	return (DDI_SUCCESS);

}	/* cs4231_detach() */

/*
 * cs4231_getinfo()
 *
 * Description:
 *	Get driver information.
 *
 * Arguments:
 *	def_info_t	*dip	Pointer to the device's dev_info structure
 *				WARNING: Don't use this dev_info structure
 *	ddi_info_cmd_t	infocmd	Getinfo command
 *	void		*arg	Command specific argument
 *	void		**result Pointer to the requested information
 *
 * Returns:
 *	DDI_SUCCESS		If the mixer information could be returned
 *	DDI_FAILURE		If the mixer information couldn't be returned
 */
/*ARGSUSED*/
static int
cs4231_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	CS_state_t		*state;
	int			error;
	int			instance;

	ATRACE_32("in cs_getinfo()", infocmd);
	ATRACE("cs_getinfo() cs_statep", cs_statep);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		/* get the state structure */
		instance = audio_sup_get_dev_instance((dev_t)arg, NULL);
		ATRACE_32("cs_getinfo() instance", instance);
		if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
			ATRACE_32("audiocs: getinfo() get soft state failed",
			    getminor((dev_t)arg));
			return (DDI_FAILURE);
		}
		if (state->cs_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)state->cs_dip;
			error = DDI_SUCCESS;
		}
		ATRACE_32("cs_getinfo() DDI_INFO_DEVT2DEVINFO", error);
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)audio_sup_get_dev_instance((dev_t)arg, NULL);
		ATRACE("cs_getinfo() DDI_INFO_DEVT2INSTANCE", *result);
		error = DDI_SUCCESS;
		break;

	default:
		ATRACE_32("cs_getinfo() default", infocmd);
		error = DDI_FAILURE;
		break;
	}

	ATRACE_32("cs_getinfo() returning", error);

	return (error);

}	/* cs4231_getinfo() */

/*
 * cs4231_power()
 *
 * Description:
 *	This routine is used to turn the power to the Codec on and off.
 *	The different DMA engines have different ways to turn on/off the
 *	power to the Codec. Therefore we call the DMA engine specific code
 *	to do the work, if we need to make a change.
 *
 *	If the level is CS4231_PWR_OFF then the Codec's registers are saved.
 *	If the level is CS4231_PWR_ON then the Codec's registers are restored.
 *	This routine doesn't stop or restart play and record. Other routines
 *	are responsible for that.
 *
 * Arguments:
 *	def_info_t	*dip		Ptr to the device's dev_info structure
 *	int		component	Which component to power up/down
 *	int		level		The power level for the component
 *
 * Returns:
 *	DDI_SUCCSS		Power level changed, we always succeed
 */
/*ARGSUSED*/
static int
cs4231_power(dev_info_t *dip, int component, int level)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;
	int			i;
	int			instance;

	ATRACE("in cs_power()", dip);
	ATRACE("cs_power() cs_statep", cs_statep);

	ASSERT(component == 0);

	instance = ddi_get_instance(dip);
	ATRACE_32("cs_power() instance", instance);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: power() get soft state failed");
		return (DDI_FAILURE);
	}
	handle = state->cs_handles.cs_codec_hndl;

	/* make sure we have some work to do */
	mutex_enter(&state->cs_lock);

	/* check the level change to see what we need to do */
	if (level == CS4231_PWR_OFF && state->cs_powered == CS4231_PWR_ON) {
		/*
		 * We are powering down, so we don't need to do a thing with
		 * the DMA engines. However, we do need to save the Codec
		 * registers.
		 */

		for (i = 0; i < CS4231_REGS; i++) {
			/* save Codec regs */
			REG_SELECT(handle, &CS4231_IAR, i);
			state->cs_save[i] = ddi_get8(handle, &CS4231_IDR);
		}

		/* turn off the Codec */
		CS4231_DMA_POWER(state, level);

		ATRACE("cs_power() power down successful", state);

	} else if (level == CS4231_PWR_ON &&
	    state->cs_powered == CS4231_PWR_OFF) {
		/* turn on the Codec */
		CS4231_DMA_POWER(state, level);

		/* reset the DMA engine(s) */
		CS4231_DMA_RESET(state);

		/*
		 * Reload the Codec's registers, the DMA engines will be
		 * taken care of when play and record start up again. But
		 * first enable registers 16 -> 31.
		 */
		REG_SELECT(handle, &CS4231_IAR, MID_REG);
		DDI_PUT8(handle, &CS4231_IDR, state->cs_save[MID_REG]);

		for (i = 0; i < CS4231_REGS; i++) {
			/* restore Codec registers */
			REG_SELECT(handle, &CS4231_IAR, (i | IAR_MCE));
			ddi_put8(handle, &CS4231_IDR, state->cs_save[i]);
			drv_usecwait(500);	/* chip bug */
		}
		REG_SELECT(handle, &CS4231_IAR, 0);	/* clear MCE bit */

		ATRACE("cs_power() power up successful", state);

#ifdef DEBUG
	} else {
		ATRACE_32("cs_power() no change to make", level);
#endif
	}

	mutex_exit(&state->cs_lock);

	ATRACE_32("cs_power() done", level);

	return (DDI_SUCCESS);

}	/* cs4231_power() */

/* *******  Audio Driver Entry Point Routines ******************************* */

/*
 * cs4231_ad_pause_play()
 *
 * Description:
 *	This routine pauses the play DMA engine.
 *
 * Arguments:
 *	int		instance 	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Returns:
 *	void
 */
/*ARGSUSED*/
static void
cs4231_ad_pause_play(int instance, int stream)
{
	CS_state_t		*state;

	ATRACE_32("in cs_ad_pause_play()", instance);
	ATRACE("cs_ad_pause_play() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: pause() can't pause audio");
		return;
	}

	ASSERT(!mutex_owned(&state->cs_lock));

	/* we need to protect the state structure */
	mutex_enter(&state->cs_lock);

	ATRACE("cs_pause_play() calling DMA_PAUSE_PLAY()", state);
	CS4231_DMA_PAUSE_PLAY(state);
	ATRACE("cs_pause_play() DMA_PAUSE_PLAY() returned", state);

	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_pause_play() returning", state);

}	/* cs4231_ad_pause_play() */

/*
 * cs4231_ad_set_config()
 *
 * Description:
 *	This routine is used to set new Codec parameters, except the data
 *	format which has it's own routine. If the Codec doesn't support a
 *	particular parameter and it is asked to set it then we return
 *	AUDIO_FAILURE.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *	int		command		The configuration to set
 *	int		dir		AUDIO_PLAY or AUDIO_RECORD, if
 *					direction is important
 *	int		arg1		Argument #1
 *	int		arg2		Argument #2, not always needed
 *
 * Returns:
 *	AUDIO_SUCCESS		The Codec parameter has been set
 *	AUDIO_FAILURE		The Codec parameter has not been set, or the
 *				parameter couldn't be set
 */
/*ARGSUSED*/
static int
cs4231_ad_set_config(int instance, int stream, int command, int dir,
	int arg1, int arg2)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;
	uint8_t			tmp_value;
	int			rc = AUDIO_FAILURE;

	ATRACE_32("in cs_ad_set_config()", command);
	ATRACE_32("cs_ad_set_config() instance", instance);
	ATRACE("cs_ad_set_config() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: set_config() get soft state failed");
		return (DDI_FAILURE);
	}

	ASSERT(!mutex_owned(&state->cs_lock));

	/* mark as busy so we won't be power managed */
	(void) pm_busy_component(state->cs_dip, CS4231_COMPONENT);

	/* CAUTION: From here on we must goto done to exit. */

	/* make sure we've got power before we program the Codec */
#ifdef NEW_PM
	if (pm_raise_power(state->cs_dip, CS4231_COMPONENT, CS4231_PWR_ON) ==
	    DDI_FAILURE) {
		cmn_err(CE_WARN, "audiocs: set_config() power up failed");
		goto done;
	}
#endif
	ASSERT(state->cs_powered == CS4231_PWR_ON);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt++;
	mutex_exit(&state->cs_lock);

	handle = state->cs_handles.cs_codec_hndl;

	switch (command) {
	case AMAD_SET_GAIN:
		/*
		 * Set the gain for a channel. The audio mixer calculates the
		 * impact, if any, on the channel's gain.
		 *
		 *	0 <= gain <= AUDIO_MAX_GAIN
		 *
		 *	arg1 --> gain
		 *	arg2 --> channel #, 0 == left, 1 == right
		 */
		if (arg1 > AUDIO_MAX_GAIN) {	/* sanity check */
			arg1 = AUDIO_MAX_GAIN;
		}

		if (dir == AUDIO_PLAY) {	/* play gain */
			mutex_enter(&state->cs_lock);
			if (arg2 == 0) {	/* left channel */
				REG_SELECT(handle, &CS4231_IAR, LDACO_REG);
			} else {		/* right channel */
				ASSERT(arg2 == 1);
				REG_SELECT(handle, &CS4231_IAR, RDACO_REG);
			}

			/* we use cs4231_atten[] to linearize attenuation */
			if (state->output_muted || arg1 == 0) {
				/* mute the output */
				DDI_PUT8(handle, &CS4231_IDR,
				    (cs4231_atten[arg1]|LDACO_LDM));
				/* NOTE: LDACO_LDM == RDACO_LDM, so either ok */
			} else {
				DDI_PUT8(handle, &CS4231_IDR,
				    cs4231_atten[arg1]);
			}
			mutex_exit(&state->cs_lock);
		} else {
			ASSERT(dir == AUDIO_RECORD);

			mutex_enter(&state->cs_lock);
			if (arg2 == 0) {	/* left channel */
				REG_SELECT(handle, &CS4231_IAR, LADCI_REG);
				tmp_value = ddi_get8(handle, &CS4231_IDR) &
				    ~LADCI_GAIN_MASK;
			} else {		/* right channel */
				ASSERT(arg2 == 1);
				REG_SELECT(handle, &CS4231_IAR, RADCI_REG);
				tmp_value = ddi_get8(handle, &CS4231_IDR) &
				    ~RADCI_GAIN_MASK;
			}
			/* we shift right by 4 to go from 8-bit to 4-bit gain */
			DDI_PUT8(handle, &CS4231_IDR, (tmp_value|(arg1 >> 4)));
			mutex_exit(&state->cs_lock);
		}

		rc = AUDIO_SUCCESS;
		goto done;

	case AMAD_SET_PORT:
		/*
		 * Enable/disable the input or output ports. The audio mixer
		 * enforces exclusiveness of ports, as well as which ports
		 * are modifyable. We just turn on the ports that match the
		 * bits.
		 *
		 *	arg1 --> port bit pattern
		 *	arg2 --> not used
		 */
		if (dir == AUDIO_PLAY) {	/* output port(s) */
			/* figure out which output port(s) to turn on */
			tmp_value = 0;

			mutex_enter(&state->cs_lock);
			REG_SELECT(handle, &CS4231_IAR, MIOC_REG);
			if (arg1 & AUDIO_SPEAKER) {
				AND_SET_BYTE(handle, &CS4231_IDR,
				    ~MONO_SPKR_MUTE);
				tmp_value |= AUDIO_SPEAKER;
			} else {
				OR_SET_BYTE(handle, &CS4231_IDR,
				    MONO_SPKR_MUTE);
			}

			REG_SELECT(handle, &CS4231_IAR, PC_REG);
			if (arg1 & AUDIO_HEADPHONE) {
				AND_SET_BYTE(handle, &CS4231_IDR,
				    ~HEADPHONE_MUTE);
				tmp_value |= AUDIO_HEADPHONE;
			} else {
				OR_SET_BYTE(handle, &CS4231_IDR,
				    HEADPHONE_MUTE);
			}

			REG_SELECT(handle, &CS4231_IAR, PC_REG);
			if (arg1 & AUDIO_LINE_OUT) {
				AND_SET_BYTE(handle, &CS4231_IDR,
				    ~LINE_OUT_MUTE);
				tmp_value |= AUDIO_LINE_OUT;
			} else {
				OR_SET_BYTE(handle, &CS4231_IDR,
				    LINE_OUT_MUTE);
			}
			mutex_exit(&state->cs_lock);

			ATRACE_32("cs_ad_set_config() set out port", tmp_value);

			if (tmp_value != (arg1 & 0x0ff)) {
				ATRACE_32("cs_ad_set_config() bad out port",
				    arg1);
				goto done;
			}
			rc = AUDIO_SUCCESS;
			goto done;

		} else {
			ASSERT(dir == AUDIO_RECORD);

			/*
			 * Figure out which input port to set. Fortunately
			 * the left and right port bit patterns are the same.
			 */
			switch (arg1) {
			case AUDIO_NONE:
				tmp_value = 0;
				break;
			case AUDIO_MICROPHONE:
				tmp_value = LADCI_LMIC;
				break;
			case AUDIO_LINE_IN:
				tmp_value = LADCI_LLINE;
				break;
			case AUDIO_CD:
				tmp_value = LADCI_LAUX1;
				break;
			case AUDIO_CODEC_LOOPB_IN:
				tmp_value = LADCI_LLOOP;
				break;
			case AUDIO_SUNVTS:
				tmp_value = LADCI_LAUX1;
				break;
			default:
				/* unknown or inclusive input ports */
				ATRACE_32("cs_ad_set_config() bad in port",
				    arg1);
				goto done;
			}

			mutex_enter(&state->cs_lock);
			REG_SELECT(handle, &CS4231_IAR, LADCI_REG);
			DDI_PUT8(handle, &CS4231_IDR,
			    ((ddi_get8(handle, &CS4231_IDR) & ~LADCI_IN_MASK) |
			    tmp_value));
			REG_SELECT(handle, &CS4231_IAR, RADCI_REG);
			DDI_PUT8(handle, &CS4231_IDR,
			    ((ddi_get8(handle, &CS4231_IDR) & ~RADCI_IN_MASK) |
			    tmp_value));
			mutex_exit(&state->cs_lock);

		}
		rc = AUDIO_SUCCESS;
		goto done;

	case AMAD_SET_MONITOR_GAIN:
		/*
		 * Set the loopback monitor gain.
		 *
		 *	0 <= gain <= AUDIO_MAX_GAIN
		 *
		 *	dir ---> N/A
		 *	arg1 --> gain
		 *	arg2 --> not used
		 */
		if (arg1 > AUDIO_MAX_GAIN) {	/* sanity check */
			arg1 = AUDIO_MAX_GAIN;
		}

		mutex_enter(&state->cs_lock);
		REG_SELECT(handle, &CS4231_IAR, LC_REG);

		if (arg1 == 0) {
			/* disable loopbacks when gain == 0 */
			DDI_PUT8(handle, &CS4231_IDR, LC_OFF);
		} else {
			/* we use cs4231_atten[] to linearize attenuation */
			DDI_PUT8(handle, &CS4231_IDR,
			    ((cs4231_atten[arg1] << 2) | LC_LBE));
		}
		mutex_exit(&state->cs_lock);

		rc = AUDIO_SUCCESS;
		goto done;

	case AMAD_OUTPUT_MUTE:
		/*
		 * Mute or enable the output.
		 *
		 *	dir ---> N/A
		 *	arg1 --> ~0 == mute, 0 == unmute
		 *	arg2 --> not used
		 */
		mutex_enter(&state->cs_lock);
		if (arg1) {
			REG_SELECT(handle, &CS4231_IAR, LDACO_REG);
			OR_SET_BYTE(handle, &CS4231_IDR, LDACO_LDM);
			REG_SELECT(handle, &CS4231_IAR, RDACO_REG);
			OR_SET_BYTE(handle, &CS4231_IDR, RDACO_RDM);
			state->output_muted = B_TRUE;
		} else {
			REG_SELECT(handle, &CS4231_IAR, LDACO_REG);
			AND_SET_BYTE(handle, &CS4231_IDR, ~LDACO_LDM);
			REG_SELECT(handle, &CS4231_IAR, RDACO_REG);
			AND_SET_BYTE(handle, &CS4231_IDR, ~RDACO_RDM);
			state->output_muted = B_FALSE;
		}
		mutex_exit(&state->cs_lock);

		rc = AUDIO_SUCCESS;
		goto done;

	case AMAD_MIC_BOOST:
		/*
		 * Enable or disable the mic's 20 dB boost preamplifier.
		 *
		 *	dir ---> N/A
		 *	arg1 --> ~0 == enable, 0 == disabled
		 *	arg2 --> not used
		 */
		mutex_enter(&state->cs_lock);
		if (arg1) {
			REG_SELECT(handle, &CS4231_IAR, LADCI_REG);
			OR_SET_BYTE(handle, &CS4231_IDR, LADCI_LMGE);
			REG_SELECT(handle, &CS4231_IAR, RADCI_REG);
			OR_SET_BYTE(handle, &CS4231_IDR, RADCI_RMGE);
			state->cs_ad_info.ad_add_mode |= AM_ADD_MODE_MIC_BOOST;
		} else {
			REG_SELECT(handle, &CS4231_IAR, LADCI_REG);
			AND_SET_BYTE(handle, &CS4231_IDR, ~LADCI_LMGE);
			REG_SELECT(handle, &CS4231_IAR, RADCI_REG);
			AND_SET_BYTE(handle, &CS4231_IDR, ~RADCI_RMGE);
			state->cs_ad_info.ad_add_mode &= ~AM_ADD_MODE_MIC_BOOST;
		}
		mutex_exit(&state->cs_lock);

		rc = AUDIO_SUCCESS;
		goto done;

	default:
		/*
		 * We let default catch commands we don't support, as well
		 * as bad commands.
		 */
		ATRACE_32("cs_ad_set_config() unsupported command", command);
		goto done;
	}

done:
	/* need an idle for the busy above */
	(void) pm_idle_component(state->cs_dip, CS4231_COMPONENT);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt--;
	mutex_exit(&state->cs_lock);

	return (rc);

}	/* cs_ad_set_config() */

/*
 * cs4231_ad_set_format()
 *
 * Description:
 *	This routine is used to set a new Codec data format.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *	int		command		The configuration to set
 *	int		dir		AUDIO_PLAY or AUDIO_RECORD, if
 *					direction is important
 *	int		sample_rate	Data sample rate
 *	int		channels	Number of channels, 1 or 2
 *	int		precision	Bits per sample, 8 or 16
 *	int		encoding	Encoding method, u-law, A-law and linear
 *
 * Returns:
 *	AUDIO_SUCCESS		The Codec data format has been set
 *	AUDIO_FAILURE		The Codec data format has not been set, or the
 *				data format couldn't be set
 */
/*ARGSUSED*/
static int
cs4231_ad_set_format(int instance, int stream, int dir,
	int sample_rate, int channels, int precision, int encoding)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;
	uint8_t			value;
	int			rc = AUDIO_FAILURE;

	ATRACE_32("in cs_ad_set_format()", sample_rate);
	ATRACE_32("cs_ad_set_format() instance", instance);
	ATRACE("cs_ad_set_format() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: set_format() get soft state failed");
		return (DDI_FAILURE);
	}

	ASSERT(!mutex_owned(&state->cs_lock));

	/* mark as busy so we won't be power managed */
	(void) pm_busy_component(state->cs_dip, CS4231_COMPONENT);

	/* CAUTION: From here on we must goto done to exit. */

	/* make sure we've got power before we program the Codec */
#ifdef NEW_PM
	if (pm_raise_power(state->cs_dip, CS4231_COMPONENT, CS4231_PWR_ON) ==
	    DDI_FAILURE) {
		cmn_err(CE_WARN, "audiocs: set_format() power up failed");
		goto done;
	}
#endif
	ASSERT(state->cs_powered == CS4231_PWR_ON);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt++;
	mutex_exit(&state->cs_lock);

	handle = state->cs_handles.cs_codec_hndl;

	if (dir == AUDIO_PLAY) {	/* sample rate set on play side only */
		switch (sample_rate) {
		case CS4231_SAMPR5510:		value = FS_5510; break;
		case CS4231_SAMPR6620:		value = FS_6620; break;
		case CS4231_SAMPR8000:		value = FS_8000; break;
		case CS4231_SAMPR9600:		value = FS_9600; break;
		case CS4231_SAMPR11025:		value = FS_11025; break;
		case CS4231_SAMPR16000:		value = FS_16000; break;
		case CS4231_SAMPR18900:		value = FS_18900; break;
		case CS4231_SAMPR22050:		value = FS_22050; break;
		case CS4231_SAMPR27420:		value = FS_27420; break;
		case CS4231_SAMPR32000:		value = FS_32000; break;
		case CS4231_SAMPR33075:		value = FS_33075; break;
		case CS4231_SAMPR37800:		value = FS_37800; break;
		case CS4231_SAMPR44100:		value = FS_44100; break;
		case CS4231_SAMPR48000:		value = FS_48000; break;
		default:
			ATRACE_32("cs_ad_set_format() bad sample rate",
			    sample_rate);
			goto done;
		}
	} else {
		value = 0;
	}

	/* if not mono then must be stereo, i.e., the default */
	if (channels == AUDIO_CHANNELS_STEREO) {
		ATRACE_32("cs_ad_set_format() STEREO", channels);
		value |= PDF_STEREO;
	} else if (channels != AUDIO_CHANNELS_MONO) {
		ATRACE_32("cs_ad_set_format() bad # of channels", channels);
		goto done;
#ifdef DEBUG
	} else {
		ATRACE_32("cs_ad_set_format() MONO", channels);
#endif
	}

	if (precision == AUDIO_PRECISION_8) {
		ATRACE_32("cs_ad_set_format() 8-bit", precision);
		switch (encoding) {
		case AUDIO_ENCODING_ULAW:	value |= PDF_ULAW8;	break;
		case AUDIO_ENCODING_ALAW:	value |= PDF_ALAW8;	break;
		case AUDIO_ENCODING_LINEAR:	value |= PDF_LINEAR8;	break;
		default:
			goto done;
		}
	} else {	/* 16 bit, default, and there is only one choice */
		ATRACE_32("cs_ad_set_format() 16-bit", precision);
		if (encoding != AUDIO_ENCODING_LINEAR) {
			goto done;
		}

		value |= PDB_LINEAR16BE;
	}

	mutex_enter(&state->cs_lock);
	if (dir == AUDIO_PLAY) {	/* play side */
		REG_SELECT(handle, &CS4231_IAR, (FSDF_REG | IAR_MCE));
		ATRACE_8("cs_ad_set_format() programming FSDF_REG", value);
		state->play_sr = sample_rate;
		state->play_ch = channels;
		state->play_prec = precision;
		state->play_enc = encoding;
	} else {			/* capture side */
		REG_SELECT(handle, &CS4231_IAR, (CDF_REG | IAR_MCE));
		ATRACE_8("cs_ad_set_format() programming CDF_REG", value);
		state->record_sr = sample_rate;
		state->record_ch = channels;
		state->record_prec = precision;
		state->record_enc = encoding;
	}

	DDI_PUT8(handle, &CS4231_IDR, value);

	(void) cs4231_poll_ready(state);

	/* clear the mode change bit */
	REG_SELECT(handle, &CS4231_IAR, FSDF_REG);
	mutex_exit(&state->cs_lock);

	ATRACE_32("cs_ad_set_format() returning", sample_rate);

	rc = AUDIO_SUCCESS;

done:
	/* need an idle for the busy above */
	(void) pm_idle_component(state->cs_dip, CS4231_COMPONENT);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt--;
	mutex_exit(&state->cs_lock);

	return (rc);

}	/* cs4231_ad_set_config() */

/*
 * cs4231_ad_setup()
 *
 * Description:
 *	This routine is called when the AUDIO device is opened. All it
 *	is used for is to mark the device as busy. The mixer module is
 *	responsible for ensuring that for every call to cs4231_ad_setup()
 *	that there is a corresponding call to cs4231_ad_teardown().
 *
 * Arguments:
 *	int		instance	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *	int		dir		Direction of audio, we don't care for
 *					marking busy so just ignore
 *
 * Returns:
 *	AUDIO_SUCCESS		Device marked busy and powered up
 *	AUDIO_FAILURE		Device couldn't be powered up
 */
/*ARGSUSED*/
static int
cs4231_ad_setup(int instance, int stream, int dir)
{
	CS_state_t		*state;

	ATRACE_32("in cs_ad_setup()", instance);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		ATRACE_32("cs_ad_setup() ddi_get_soft_state() failed",
		    instance);
		return (AUDIO_FAILURE);
	}

	/* mark as busy so we won't be power managed */
	(void) pm_busy_component(state->cs_dip, CS4231_COMPONENT);

	/* make sure the device is powered up */
#ifdef NEW_PM
	if (pm_raise_power(state->cs_dip, CS4231_COMPONENT, CS4231_PWR_ON) ==
	    DDI_FAILURE) {
		/* need an idle for the busy above */
		(void) pm_idle_component(state->cs_dip, CS4231_COMPONENT);

		cmn_err(CE_WARN, "audiocs: setup() power up failed");
		return (AUDIO_FAILURE);
	}
#endif
	ASSERT(state->cs_powered == CS4231_PWR_ON);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt++;
	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_setup() succeeded", 0);

	return (AUDIO_SUCCESS);

}	/* cs4231_ad_setup() */

/*
 * cs4231_ad_start_play()
 *
 * Description:
 *	This routine starts the play DMA engine. It checks to make sure the
 *	DMA engine is off before it does anything, otherwise it may mess
 *	things up.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance 	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 * Returns:
 *	AUDIO_SUCCESS		Playing started/restarted
 *	AUDIO_FAILURE		Audio not restarted, no audio to play
 */
/*ARGSUSED*/
static int
cs4231_ad_start_play(int instance, int stream)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;
	int			rc;

	ATRACE("in cs_ad_start_play()", instance);
	ATRACE_32("cs_ad_start_play() instance", instance);
	ATRACE("cs_ad_start_play() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE,
		    "audiocs: start_play() couldn't start playing");
		return (AUDIO_FAILURE);
	}

	ASSERT(!mutex_owned(&state->cs_lock));
	ASSERT(state->cs_powered == CS4231_PWR_ON);

	handle = state->cs_handles.cs_codec_hndl;

	/* we need to protect the state structure */
	mutex_enter(&state->cs_lock);

	/* see if we are already playing */
	REG_SELECT(handle, &CS4231_IAR, INTC_REG);
	if (INTC_PEN & ddi_get8(handle, &CS4231_IDR)) {
		mutex_exit(&state->cs_lock);
		ATRACE("cs_ad_start_play() already playing", 0);
		return (AUDIO_SUCCESS);
	}

	if (state->cs_flags & PDMA_ENGINE_INITALIZED) {
		ATRACE("cs_ad_start_play() calling DMA_RESTART_PLAY()", state);
		CS4231_DMA_RESTART_PLAY(state);
		ATRACE_32("cs_ad_start_play() DMA_RESTART_PLAY() rturned", 0);
		rc = AUDIO_SUCCESS;
	} else {
		ATRACE("cs_ad_start_play() calling DMA_START_PLAY()", state);
		rc = CS4231_DMA_START_PLAY(state);
		ATRACE_32("cs_ad_start_play() DMA_START_PLAY() returned", rc);

		if (rc == AUDIO_SUCCESS) {
			ATRACE("cs_ad_start_play() programming Codec to play",
			    state);
			REG_SELECT(handle, &CS4231_IAR, INTC_REG);
			OR_SET_BYTE(handle, &CS4231_IDR, INTC_PEN);

			ATRACE_8("cs_ad_start_play() Codec INTC_REG",
			    ddi_get8(handle, &CS4231_IDR));
#ifdef DEBUG
		} else {
			ATRACE("cs_ad_start_play() Codec not started", rc);
#endif
		}
	}

	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_start_play() returning", state);

	return (rc);

}	/* cs4231_ad_start_play() */

/*
 * cs4231_ad_stop_play()
 *
 * Description:
 *	This routine stops the play DMA engine.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance 	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 * Returns:
 *	void
 */
/*ARGSUSED*/
static void
cs4231_ad_stop_play(int instance, int stream)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;

	ATRACE("in cs_ad_stop_play()", instance);
	ATRACE("cs_ad_stop_play() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE, "audiocs: stop_play() couldn't stop playing");
		return;
	}

	ASSERT(!mutex_owned(&state->cs_lock));

	handle = state->cs_handles.cs_codec_hndl;

	/* we need to protect the state structure */
	mutex_enter(&state->cs_lock);

	ATRACE_8("cs_ad_stop_play() Codec INTC_REG",
	    ddi_get8(handle, &CS4231_IDR));

	/* stop the play DMA engine */
	ATRACE("cs_ad_stop_play() calling DMA_STOP_PLAY()", state);
	CS4231_DMA_STOP_PLAY(state);
	ATRACE("cs_ad_stop_play() DMA_STOP_PLAY() returned", state);
	/* DMA_STOP() returns with the PEN cleared */

	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_stop_play() returning", state);

}	/* cs4231_ad_stop_play() */

/*
 * cs4231_ad_start_record()
 *
 * Description:
 *	This routine starts the record DMA engine. It checks to make sure the
 *	DMA engine is off before it does anything, otherwise it may mess
 *	things up.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance 	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 * Returns:
 *	AUDIO_SUCCESS		Recording successfully started
 *	AUDIO_FAILURE		Recording not successfully started
 */
/*ARGSUSED*/
static int
cs4231_ad_start_record(int instance, int stream)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;
	int			rc = AUDIO_FAILURE;

	ATRACE("in cs_ad_start_record()", instance);
	ATRACE("cs_ad_start_record() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE,
		    "audiocs: start_record() couldn't start recording");
		return (rc);
	}

	ASSERT(!mutex_owned(&state->cs_lock));
	ASSERT(state->cs_powered == CS4231_PWR_ON);

	handle = state->cs_handles.cs_codec_hndl;

	/* we need to protect the state structure */
	mutex_enter(&state->cs_lock);

	/* see if we are already recording */
	REG_SELECT(handle, &CS4231_IAR, INTC_REG);
	if (INTC_CEN & ddi_get8(handle, &CS4231_IDR)) {
		mutex_exit(&state->cs_lock);
		ATRACE("cs_ad_start_record() already recording", 0);
		return (AUDIO_SUCCESS);
	}

	/* enable record DMA on the Codec */
	ATRACE("cs_ad_start_record() calling DMA_START_RECORD()", state);
	rc = CS4231_DMA_START_RECORD(state);
	ATRACE("cs_ad_start_record() DMA_START_RECORD() returned", rc);

	if (rc == AUDIO_SUCCESS) {
		ATRACE("cs_ad_start_record() programming Codec to rec.", state);
		REG_SELECT(handle, &CS4231_IAR, INTC_REG);
		OR_SET_BYTE(handle, &CS4231_IDR, INTC_CEN);

		ATRACE_8("cs_ad_start_record() Codec INTC_REG",
		    ddi_get8(handle, &CS4231_IDR));
#ifdef DEBUG
	} else {
		ATRACE("cs_ad_start_record() Codec not started", rc);
#endif
	}

	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_start_record() returning", state);

	return (rc);

}	/* cs4231_ad_start_record() */

/*
 * cs4231_ad_stop_record()
 *
 * Description:
 *	This routine stops the record DMA engine.
 *
 *	The stream argument is ignored because this isn't a multi-stream Codec.
 *
 *	NOTE: This routine must be called with the state unlocked.
 *
 * Arguments:
 *	int		instance	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 * Returns:
 *	void
 */
/*ARGSUSED*/
static void
cs4231_ad_stop_record(int instance, int stream)
{
	CS_state_t		*state;
	ddi_acc_handle_t	handle;

	ATRACE("in cs_ad_stop_record()", instance);
	ATRACE("cs_ad_stop_record() cs_statep", cs_statep);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE,
		    "audiocs: stop_record() couldn't stop recording");
		return;
	}

	ASSERT(!mutex_owned(&state->cs_lock));

	handle = state->cs_handles.cs_codec_hndl;

	/* we need to protect the state structure */
	mutex_enter(&state->cs_lock);

	/* stop the record DMA engine and clear the active flag */
	ATRACE("cs_ad_stop_record() calling DMA_STOP_RECORD()", state);
	CS4231_DMA_STOP_RECORD(state);
	ATRACE("cs_ad_stop_record() DMA_STOP_RECORD() returned", state);

	ATRACE("cs_ad_stop_record() programming Codec to rec.", state);
	REG_SELECT(handle, &CS4231_IAR, INTC_REG);
	AND_SET_BYTE(handle, &CS4231_IDR, ~INTC_CEN);

	ATRACE_8("cs_ad_stop_record() Codec INTC_REG",
	    ddi_get8(handle, &CS4231_IDR));

	mutex_exit(&state->cs_lock);

	ATRACE("cs_ad_stop_record() returning", state);

}       /* cs4231_ad_stop_record() */

/*
 * cs4231_ad_teardown()
 *
 * Description:
 *	This routine is called when the AUDIO device is closed. All it
 *	is used for is to mark the device as idle. The mixer module is
 *	responsible for ensuring that for every call to cs4231_ad_setup()
 *	that there is a corresponding call to cs4231_ad_teardown().
 *
 * Arguments:
 *	int		instance	The Audio Driver instance
 *	int		stream		Stream number for multi-stream Codecs,
 *					which this isn't, so just ignore
 *
 * Returns:
 *	void
 */
static void
cs4231_ad_teardown(int instance, int stream)
{
	CS_state_t		*state;

	ATRACE_32("in cs_ad_teardown()", stream);

	/* get the state structure */
	if ((state = ddi_get_soft_state(cs_statep, instance)) == NULL) {
		cmn_err(CE_NOTE,
		    "audiocs: stop_record() couldn't stop recording");
		return;
	}

	/* decrement the busy count */
	(void) pm_idle_component(state->cs_dip, CS4231_COMPONENT);

	mutex_enter(&state->cs_lock);
	state->cs_busy_cnt--;
	mutex_exit(&state->cs_lock);

}	/* cs4231_ad_teardown() */


/* *******  Global Local Routines ******************************************* */

/*
 * cs4231_poll_ready()
 *
 * Description:
 *	This routine waits for the Codec to complete its initialization
 *	sequence and is done with its autocalibration.
 *
 *	Early versions of the Codec have a bug that can take as long as
 *	15 seconds to complete its initialization. For these cases we
 *	use a timeout mechanism so we don't keep the machine locked up.
 *
 * Arguments:
 *	CS_state_t	*state	The device's state structure
 *
 * Returns:
 *	AUDIO_SUCCESS		If the Codec is ready to continue
 *	AUDIO_FAILURE		If the Codec isn't ready to continue
 */
int
cs4231_poll_ready(CS_state_t *state)
{
	ddi_acc_handle_t	handle = state->cs_handles.cs_codec_hndl;
	int			x = 0;
	uint8_t			iar;
	uint8_t			idr;

	ATRACE("in cs_poll_ready()", state);

	ASSERT(state->cs_regs != NULL);
	ASSERT(handle != NULL);

	/* wait for the chip to initialize itself */
	iar = ddi_get8(handle, &CS4231_IAR);

	while ((iar & IAR_INIT) && x++ < CS4231_TIMEOUT) {
		drv_usecwait(50);
		iar = ddi_get8(handle, &CS4231_IAR);
	}

	if (x >= CS4231_TIMEOUT) {
		ATRACE("cs_poll_ready() timeout #1", state);
		ASSERT(x == CS4231_TIMEOUT);
		return (AUDIO_FAILURE);
	}

	x = 0;

	/*
	 * Now wait for the chip to complete its autocalibration.
	 * Set the test register.
	 */
	REG_SELECT(handle, &CS4231_IAR, ESI_REG);

	idr = ddi_get8(handle, &CS4231_IDR);

	while ((idr & ESI_ACI) && x++ < CS4231_TIMEOUT) {
		drv_usecwait(50);
		idr = ddi_get8(handle, &CS4231_IDR);
	}

	if (x >= CS4231_TIMEOUT) {
		ATRACE("cs_poll_ready() timeout #1", state);
		ASSERT(x == CS4231_TIMEOUT);
		return (AUDIO_FAILURE);
	}

	ATRACE("cs_poll_ready() returining", state);

	return (AUDIO_SUCCESS);

}	/* cs4231_poll_ready() */
