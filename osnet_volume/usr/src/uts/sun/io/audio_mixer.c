/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_mixer.c	1.30	99/12/09 SMI"

/*
 * Mixer Audio Personality Module (mixer)
 *
 * This module is used by Audio Drivers that wish to have the audio(7I)
 * and mixer(7I) semantics for /dev/audio and /dev/audioctl. In general,
 * the mixer(7I) semantics are a superset of the audio(7I) semantcs. Either
 * semantics may be enforced, depending on the mode of the mixer, AM_MIXER_MODE
 * or AM_COMPAT_MODE, respectively. The initial mode is set by the Audio
 * Driver and may be changed on the fly, even while playing or recording
 * audio. When mixing is enabled multiple play and record streams are allowed,
 * and new ioctls are also available. Some legacy applications may not behave
 * well in this mode, thus the availability of the compatibility mode.
 *
 * In addition to providing two sets of semantics, this module also supports
 * two types of audio Codecs, those that have a single play and record
 * stream (traditional Codecs) and those that allow multiple play and
 * record streams (multi-stream Codecs). Because multi-streaming Codecs
 * must do sample rate conversion in order to mix, in hardware, the audio
 * stream is passed directly between applications and hardware. However, for
 * traditional Codecs the audio streams must be converted to a canonical
 * form, which allows them to be mixed. Therefore this module performs
 * format, size, and sample rate conversion for these devices.
 *
 *
 *	NOTE: statep->as_max_chs is set when the audiosup module loads, so we
 *		don't need to protect it when we access it.
 *
 *	NOTE: All linear PCM is assumed to be signed. Therefore if the device
 *		only supports unsigned linear PCM we need to translate either
 *		before we send it to the device or after we take it from the
 *		device. This way we save each Audio Driver from having to do
 *		this.
 *
 *	NOTE: This module depends on the misc/audiosup module being loaded 1st.
 */

#include <sys/mixer_impl.h>
#include <sys/g711.h>
#include <sys/modctl.h>		/* modldrv */
#include <sys/debug.h>		/* ASSERT() */
#include <sys/kmem.h>		/* kmem_zalloc(), etc. */
#include <sys/ksynch.h>		/* mutex_init(), etc. */
#include <sys/ddi.h>		/* getminor(), etc. */
#include <sys/sunddi.h>		/* nochpoll, ddi_prop_op, etc. */
#include <sys/stat.h>		/* S_IFCHR */
#include <sys/errno.h>		/* EIO, etc. */
#include <sys/file.h>		/* FREAD, FWRITE, etc. */
#include <sys/fcntl.h>		/* O_NONBLOCK */
#include <sys/stropts.h>	/* I_PLINK, I_UNPLINK */
#include <sys/inttypes.h>	/* UINTMAX_MAX */
#include <sys/modctl.h>		/* mod_miscops */

/*
 * Flow control is used to keep too many buffers from being allocated.
 * However, sometimes we come across apps that have a problem with flow
 * control. Therefore we can comment out and turn off flow control temporarily
 * so we can debug the app and come up with a work around.
 */
#define	FLOW_CONTROL

/*
 * These defines are used by the sample rate conversion routine.
 */
#define	SRC1_SHIFT1		1
#define	SRC1_SHIFT2		2
#define	SRC1_SHIFT3		3
#define	SRC1_SHIFT8		8

/*
 * Sample rate conversion routines.
 */
static int am_src1_adjust(audio_ch_t *, int, int);
static int *am_src1_convert(audio_ch_t *, int, int *, int *, int *);
static void am_src1_exit(audio_ch_t *, int);
static int am_src1_init(audio_ch_t *, int);
static size_t am_src1_size(audio_ch_t *, int, int, int);
static int am_src1_down(am_src1_data_t *, int, int, int, int *, int *, int);
static int am_src1_up_2m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_2s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_3m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_3s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_4m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_4s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_5m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_5s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_6m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_6s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_7s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_7m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_8s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_8m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_9s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_9m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_10s(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_10m(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_ds(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_dm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_2fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_2fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_3fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_3fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_4fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_4fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_5fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_5fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_6fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_6fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_7fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_7fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_8fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_8fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_9fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_9fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_10fs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_10fm(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_dfs(am_src1_data_t *, int, int, int *, int *, int);
static int am_src1_up_dfm(am_src1_data_t *, int, int, int *, int *, int);

/*
 * Global variable to provide generic sample rate conversion facility.
 */
am_ad_src_entry_t am_src1 = {
	am_src1_convert,
	am_src1_exit,
	am_src1_init,
	am_src1_size,
	am_src1_adjust
};

/*
 * Local Routine Prototypes For The Audio Mixer Audio Personality Module
 */
static int am_audio_set_info(audio_ch_t *, audio_info_t *, audio_info_t *);
static int am_ck_bits_set32(uint_t);
static int am_ck_channels(am_ad_ch_cap_t *, uint_t);
static int am_ck_combinations(am_ad_cap_comb_t *, int, int, boolean_t);
static int am_ck_sample_rate(am_ad_ch_cap_t *, int, int);
static int am_close(queue_t *, int, cred_t *);
static void am_convert(int, int *, void *, audio_prinfo_t *);
static void am_convert_to_int(void *, int *, size_t, int, int);
static void am_fix_info(audio_ch_t *, audio_info_t *, am_ad_info_t *,
	am_ch_private_t *);
static int am_get_audio_multi(audio_state_t *, void *, int, int);
static int am_get_audio_trad(audio_state_t *, void *, int, audio_apm_info_t *);
static int am_get_samples(audio_ch_t *, int, void *, int);
static int am_open(queue_t *, dev_t *, int, int, cred_t *);
static int am_p_process(audio_ch_t *, mblk_t *, void **, void **, size_t *);
static void am_restart(audio_state_t *, audio_info_t *);
static int am_rput(queue_t *, mblk_t *);
static int am_rsvc(queue_t *);
static void am_send_audio_common(audio_ch_t *, void *, int, int);
static void am_send_audio_multi(audio_state_t *, void *, int, int);
static void am_send_audio_trad(audio_state_t *, void *, int,
	audio_apm_info_t *);
static void am_send_signal(audio_state_t *);
static int am_set_compat_mode(audio_ch_t *, am_ad_info_t *, audio_ch_t *,
	audio_ch_t *);
static int am_set_format(am_ad_entry_t *, uint_t, int, int, int, int,
	int, int, int);
static int am_set_gain(audio_apm_info_t *, uint_t, uint_t, uint_t, int,
	int, int);
static int am_set_mixer_mode(audio_ch_t *, am_ad_info_t *, am_apm_private_t *,
	audio_ch_t *, audio_ch_t *);
static int am_wput(queue_t *, mblk_t *);
static int am_wsvc(queue_t *);
static int am_wiocdata(queue_t *, mblk_t *, audio_ch_t *);
static int am_wioctl(queue_t *, mblk_t *, audio_ch_t *);

/*
 * Module Linkage Structures
 */
/* Linkage structure for loadable drivers */
static struct modlmisc mixer_modlmisc = {
	&mod_miscops,		/* drv_modops */
	MIXER_MOD_NAME,		/* drv_linkinfo */
};

static struct modlinkage mixer_modlinkage =
{
	MODREV_1,		/* ml_rev */
	(void*)&mixer_modlmisc,	/* ml_linkage */
	NULL			/* NULL terminates the list */
};

/*
 * Global hidden variables
 */
static audio_device_t mixer_device_info = {
	MIXER_NAME,
	MIXER_VERSION,
	MIXER_CONFIGURATION
};

static int mixer_bufsize = AM_DEFAULT_MIX_BUFSIZE;

/*
 *  Loadable Module Configuration Entry Points
 *
 *
 * _init()
 *
 * Description:
 *	Driver initialization, called when driver is first loaded.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	mod_install() status, see mod_install(9f)
 */
int
_init(void)
{
	int	error;

	ATRACE("in mixer _init()", 0);

	/* standard linkage call */
	if ((error = mod_install(&mixer_modlinkage)) != 0) {
		ATRACE_32("mixer _init() error 1", error);
		return (error);
	}

	ATRACE("mixer _init() successful", 0);

	return (error);

}	/* _init() */

/*
 * _fini()
 *
 * Description
 *	Module de-initialization, called when driver is to be unloaded.
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
	int	error;

	ATRACE("in mixer _fini()", 0);

	if ((error = mod_remove(&mixer_modlinkage)) != 0) {
		ATRACE_32("mixer _fini() mod_remove failed", error);
		return (error);
	}

	ATRACE_32("mixer _fini() successful", error);

	return (error);

}	/* _fini() */

/*
 * _info()
 *
 * Description:
 *	Module information, returns infomation about the driver.
 *
 * Arguments:
 *	modinfo	*modinfop	Pointer to an opaque modinfo structure
 *
 * Returns:
 *	mod_info() status, see mod_info(9f)
 */
int
_info(struct modinfo *modinfop)
{
	int		rc;

	rc = mod_info(&mixer_modlinkage, modinfop);

	ATRACE_32("mixer _info() returning", rc);

	return (rc);

}	/* _info() */

/*
 * Public Audio Mixer Audio Personality Module Entry Points
 */

/*
 * am_attach()
 *
 * TODO:	Check for PLINK
 *
 * Description:
 *	Attach an instance of the mixer. We initialize all the data structures
 *	and register the APM for both AUDIO and AUDIOCTL.
 *
 *	We check both MIXER and COMPAT modes because we can switch between
 *	them and thus we need to make sure everything is okay for both modes.
 *	Plus we need the initial condition for both modes to support switching.
 *
 *	NOTE: mutex_init() and cv_init() no longer needs a name string, so set
 *	      to NULL to save kernel space.
 *
 *	NOTE: It is okay for memory allocation to sleep.
 *
 * Arguments:
 *	dev_info_t	*dip		Ptr to the device's dev_info structure
 *	ddi_attach_cmd_t cmd		Attach command
 *	am_ad_info_t	*ad_infop	Ptr to the device's capabilities struct
 *	kmutex_t	*ad_swlock	Ptr to the sw lock to coordinate APMs
 *	int		*max_chs	Ptr to max chs supported
 *	int		*max_play_chs	Ptr to max supported play streams
 *	int		*max_record_chs	Ptr to max supported record streams
 *	int		*chs		Ptr to current chs open
 *	int		*play_chs	Ptr to current play channels open
 *	int		*record_chs	Ptr to current record channels open
 *
 * Returns:
 *	>= 0			Device instance number if the mixer was
 *				initialized properly
 *	AUDIO_FAILURE		If the mixer couldn't be initialized properly
 */
int
am_attach(dev_info_t *dip, ddi_attach_cmd_t cmd, am_ad_info_t *ad_infop,
	kmutex_t *ad_swlock, int *max_chs, int *max_play_chs,
	int *max_record_chs, int *chs, int *play_chs, int *record_chs)
{
	audio_state_t		*statep;	/* instance state structure */
	audio_info_t		*hw_info;
	audio_apm_info_t	*apm_infop1;
	audio_apm_info_t	*apm_infop2;
	am_apm_private_t	*stpptr;
	minor_t			minor;
	uint_t			cpenc;		/* compat play encoding */
	uint_t			crenc;		/* compat capture encoding */
	uint_t			cpch;		/* compat play channels */
	uint_t			crch;		/* compat record channels */
	uint_t			cpprec;		/* compat play precision */
	uint_t			crprec;		/* compat record precision */
	int			cpsr;		/* compat play sample rate */
	int			crsr;		/* compat record sample rate */
	uint_t			mpenc;		/* mixer play encoding */
	uint_t			mrenc;		/* mixer capture encoding */
	uint_t			mpch;		/* mixer play channels */
	uint_t			mrch;		/* mixer record channels */
	uint_t			mpprec;		/* mixer play precision */
	uint_t			mrprec;		/* mixer record precision */
	uint_t			pgain;		/* play gain to set h/w */
	uint_t			pbalance;	/* play balance to set h/w */
	uint_t			rgain;		/* record gain to set h/w */
	uint_t			rbalance;	/* record balance to set h/w */
	int			mpsr;		/* mixer play sample rate */
	int			mrsr;		/* mixer record sample rate */
	int			dev_instance;
	int			mode;

	ATRACE("in am_attach()", ad_infop);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	default:
		ATRACE_32("am_attach() unknown command failure", cmd);
		return (AUDIO_FAILURE);
	}

	/* before we do anything else, make sure the interface version is ok */
	if (ad_infop->ad_int_vers != AMAD_VERS1) {
		ATRACE_32("am_attach() unsupported interface version",
		    ad_infop->ad_int_vers);
		cmn_err(CE_NOTE,
		    "mixer: attach() interface version not supported: %d",
		    ad_infop->ad_int_vers);
		return (AUDIO_FAILURE);
	}

	/* get the properties from the .conf file */
	if ((mixer_bufsize = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_CANSLEEP, "mixer_bufsize", AM_DEFAULT_MIX_BUFSIZE)) ==
	    AM_DEFAULT_MIX_BUFSIZE) {
		ATRACE_32("am_attach() setting mix buffer size",
		    AM_DEFAULT_MIX_BUFSIZE);
#ifdef DEBUG
	} else {
		ATRACE_32("am_attach() setting mix buffer size from .conf",
		    mixer_bufsize);
#else
		/*EMPTY*/
#endif
	}
	if (mixer_bufsize < AM_DEFAULT_MIX_BUFSIZE) {
		ATRACE_32("am_attach() mix buffer too small", mixer_bufsize);
		cmn_err(CE_NOTE, "mixer: attach() "
		    "ddi_getprop() mix buffer too small, setting to %d",
		    AM_DEFAULT_MIX_BUFSIZE);
		mixer_bufsize = AM_DEFAULT_MIX_BUFSIZE;
		ATRACE_32("am_attach() setting new mix buffer size",
		    mixer_bufsize);
	}

	/* get the state pointer for this instance */
	dev_instance = ddi_get_instance(dip);
	if ((statep = audio_sup_get_state(dip, NODEV)) == NULL) {
		ATRACE("am_attach() couldn't get state structure", 0);
		cmn_err(CE_NOTE,
		    "mixer: attach() couldn't get state structure");
		return (AUDIO_FAILURE);
	}

	/* allocate the audio mixer private data */
	stpptr = kmem_zalloc(sizeof (*stpptr), KM_SLEEP);
	hw_info = &stpptr->hw_info;

	/* make sure we won't free the device state structure */
	hw_info->ref_cnt = 1;

	/* initialize mutexs and cvs */
	mutex_init(&stpptr->lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&stpptr->cv, NULL, CV_DRIVER, NULL);

	/*
	 * WARNING: From here on all error returns must be through one
	 *	of the error_? labels. Otherwise we'll have a memory leak.
	 */

	/* audiosup module makes sure the maximums are okay, so only ck mins */
	if (*max_chs < AM_MIN_CHS) {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "max_chs can not be less than 2, setting to 2");
		*max_chs = AM_MIN_CHS * 2;	/* one play and one record */
	}
	if (*max_play_chs < AM_MIN_CHS) {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "max_ps_chs can not be less than 1, setting to 1");
		*max_play_chs = AM_MIN_CHS;
	}
	if (*max_record_chs < AM_MIN_CHS) {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "ad_max_rs_chs can not be less than 1, setting to 1");
		*max_record_chs = AM_MIN_CHS;
	}
	mode = ad_infop->ad_mode;

	ATRACE_32("am_attach() max channels", *max_chs);
	ATRACE_32("am_attach() play channels", *max_play_chs);
	ATRACE_32("am_attach() record channels", *max_record_chs);

	/* register the mixer with the Audio Support Module */
	if ((apm_infop1 = audio_sup_register_apm(statep, AUDIO, am_open,
	    am_close, ad_swlock, stpptr, ad_infop, hw_info, max_chs,
	    max_play_chs, max_record_chs, chs, play_chs, record_chs,
	    &mixer_device_info)) == NULL) {
		ATRACE_32("am_attach() couldn't register AUDIO", 0);
		cmn_err(CE_NOTE, "mixer: attach() "
		    "couldn't register the AUDIO device with the audiosup "
		    "drvr");
		goto error_free_private;
	}
	if ((apm_infop2 = audio_sup_register_apm(statep, AUDIOCTL, am_open,
	    am_close, ad_swlock, stpptr, ad_infop, hw_info, max_chs,
	    max_play_chs, max_record_chs, chs, play_chs, record_chs,
	    &mixer_device_info)) == NULL) {
		ATRACE_32("am_attach() couldn't register AUDIOCTL", 0);
		cmn_err(CE_NOTE, "mixer: attach() "
		    "couldn't register the AUDIOCTL device with the audiosup "
		    "drvr");
		goto error_unregister;
	}

	/*
	 * Check the Audio Driver capabilities for good sample rate info
	 * before we continue. For MIXER mode we get the default H/W sample
	 * rate.
	 */
	if (am_ck_sample_rate(&ad_infop->ad_play, AM_COMPAT_MODE, NODEV) ==
	    AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad COMPAT mode play sample rate list");
		goto error_unregister_both;
	}
	if (am_ck_sample_rate(&ad_infop->ad_record, AM_COMPAT_MODE, NODEV) ==
	    AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad COMPAT mode record sample rate list");
		goto error_unregister_both;
	}

	if ((mpsr = am_ck_sample_rate(&ad_infop->ad_play,
	    AM_MIXER_MODE, NODEV)) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER mode play sample rate list");
		goto error_unregister_both;
	}
	if ((mrsr = am_ck_sample_rate(&ad_infop->ad_record,
	    AM_MIXER_MODE, NODEV)) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER mode record sample rate list");
		goto error_unregister_both;
	}
	ATRACE_32("am_attach() max MIXER play sample rate", mpsr);
	ATRACE_32("am_attach() max MIXER record sample rate", mrsr);

	/* verify that the default sample rates for COMPAT mode are ok */
	if (am_ck_sample_rate(&ad_infop->ad_play, AM_COMPAT_MODE,
	    ad_infop->ad_defaults->play.sample_rate) == AUDIO_FAILURE) {
		/* couldn't set driver's default, so set mixers's */
		if (am_ck_sample_rate(&ad_infop->ad_play, AM_COMPAT_MODE,
		    hw_info->play.sample_rate) == AUDIO_FAILURE) {
			cmn_err(CE_NOTE, "mixer: attach() "
			    "bad COMPAT play sample rate: %d",
			    hw_info->play.sample_rate);
			goto error_unregister_both;
		}
		cpsr = hw_info->play.sample_rate;
	} else {
		cpsr = ad_infop->ad_defaults->play.sample_rate;
	}
	if (am_ck_sample_rate(&ad_infop->ad_record, AM_COMPAT_MODE,
	    ad_infop->ad_defaults->record.sample_rate) == AUDIO_FAILURE) {
		/* couldn't set mixer's default, so set driver's */
		if (am_ck_sample_rate(&ad_infop->ad_record, AM_COMPAT_MODE,
		    hw_info->record.sample_rate) == AUDIO_FAILURE) {
			cmn_err(CE_NOTE, "mixer: attach() "
			    "bad COMPAT record sample rate: %d",
			    hw_info->record.sample_rate);
			goto error_unregister_both;
		}
		crsr = hw_info->record.sample_rate;
	} else {
		crsr = ad_infop->ad_defaults->record.sample_rate;
	}
	ATRACE_32("am_attach() COMPAT play sample rate", cpsr);
	ATRACE_32("am_attach() COMPAT record sample rate", crsr);

	/* now look for the best channels, for MIXER mode */
	if (am_ck_channels(&ad_infop->ad_play, AUDIO_CHANNELS_STEREO) ==
	    AUDIO_SUCCESS) {
		mpch = AUDIO_CHANNELS_STEREO;
	} else if (am_ck_channels(&ad_infop->ad_play, AUDIO_CHANNELS_MONO) ==
	    AUDIO_SUCCESS) {
		mpch = AUDIO_CHANNELS_MONO;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER play channels list");
		goto error_unregister_both;
	}
	if (am_ck_channels(&ad_infop->ad_record, AUDIO_CHANNELS_STEREO) ==
	    AUDIO_SUCCESS) {
		mrch = AUDIO_CHANNELS_STEREO;
	} else if (am_ck_channels(&ad_infop->ad_record, AUDIO_CHANNELS_MONO) ==
	    AUDIO_SUCCESS) {
		mrch = AUDIO_CHANNELS_MONO;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER record channels list");
		goto error_unregister_both;
	}
	ATRACE_32("am_attach() MIXER play channels", mpch);
	ATRACE_32("am_attach() MIXER record channels", mrch);

	/* check the channels for COMPAT mode */
	if (am_ck_channels(&ad_infop->ad_play,
	    ad_infop->ad_defaults->play.channels) == AUDIO_SUCCESS) {
		cpch = ad_infop->ad_defaults->play.channels;
	} else if (am_ck_channels(&ad_infop->ad_play,
	    hw_info->play.channels) == AUDIO_SUCCESS) {
		cpch = hw_info->play.channels;
	} else if (am_ck_channels(&ad_infop->ad_play, AUDIO_CHANNELS_MONO) ==
	    AUDIO_SUCCESS) {
		cpch = AUDIO_CHANNELS_MONO;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad COMPAT play channels list");
		goto error_unregister_both;
	}
	if (am_ck_channels(&ad_infop->ad_record,
	    ad_infop->ad_defaults->record.channels) == AUDIO_SUCCESS) {
		crch = ad_infop->ad_defaults->record.channels;
	} else if (am_ck_channels(&ad_infop->ad_record,
	    hw_info->record.channels) == AUDIO_SUCCESS) {
		crch = hw_info->record.channels;
	} else if (am_ck_channels(&ad_infop->ad_record, AUDIO_CHANNELS_MONO) ==
	    AUDIO_SUCCESS) {
		crch = AUDIO_CHANNELS_MONO;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad COMPAT record channels list");
		goto error_unregister_both;
	}
	ATRACE_32("am_attach() COMPAT play channels", cpch);
	ATRACE_32("am_attach() COMPAT record channels", crch);

	/* now look for the best precision and encoding, for MIXER mode */
	if (am_ck_combinations(ad_infop->ad_play_comb, AUDIO_ENCODING_LINEAR,
	    AUDIO_PRECISION_16, B_TRUE) == AUDIO_SUCCESS) {
		mpenc = AUDIO_ENCODING_LINEAR;
		mpprec = AUDIO_PRECISION_16;
	} else if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_LINEAR, AUDIO_PRECISION_8, B_TRUE) ==
	    AUDIO_SUCCESS) {
		mpenc = AUDIO_ENCODING_LINEAR;
		mpprec = AUDIO_PRECISION_8;
	} else if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		mpenc = AUDIO_ENCODING_ULAW;
		mpprec = AUDIO_PRECISION_8;
	} else if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_ALAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		mpenc = AUDIO_ENCODING_ALAW;
		mpprec = AUDIO_PRECISION_8;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER play encoding list");
		goto error_unregister_both;
	}

	if (am_ck_combinations(ad_infop->ad_rec_comb, AUDIO_ENCODING_LINEAR,
	    AUDIO_PRECISION_16, B_TRUE) == AUDIO_SUCCESS) {
		mrenc = AUDIO_ENCODING_LINEAR;
		mrprec = AUDIO_PRECISION_16;
	} else if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_LINEAR, AUDIO_PRECISION_8, B_TRUE) ==
	    AUDIO_SUCCESS) {
		mrenc = AUDIO_ENCODING_LINEAR;
		mrprec = AUDIO_PRECISION_8;
	} else if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		mrenc = AUDIO_ENCODING_ULAW;
		mrprec = AUDIO_PRECISION_8;
	} else if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_ALAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		mrenc = AUDIO_ENCODING_ALAW;
		mrprec = AUDIO_PRECISION_8;
	} else {
		cmn_err(CE_NOTE,
		    "mixer: attach() bad MIXER record encoding list");
		goto error_unregister_both;
	}

	ATRACE_32("am_attach() MIXER play precision", mpprec);
	ATRACE_32("am_attach() MIXER play encoding", mpenc);
	ATRACE_32("am_attach() MIXER record precision", mrprec);
	ATRACE_32("am_attach() MIXER record encoding", mrenc);

	/* figure out if we need to translate u-law or A-law to PCM */
	if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_FAILURE) {
		/* we need to convert u-law to PCM */
		stpptr->flags |= AM_PRIV_PULAW_TRANS;
	}
	if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_ALAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_FAILURE) {
		/* we need to convert A-law to PCM */
		stpptr->flags |= AM_PRIV_PALAW_TRANS;
	}
	if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_FAILURE) {
		/* we need to convert u-law to PCM */
		stpptr->flags |= AM_PRIV_RULAW_TRANS;
	}
	if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_ALAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_FAILURE) {
		/* we need to convert A-law to PCM */
		stpptr->flags |= AM_PRIV_RALAW_TRANS;
	}

	/* check precision and encoding for COMPAT mode */
	if (am_ck_combinations(ad_infop->ad_play_comb,
	    ad_infop->ad_defaults->play.encoding,
	    ad_infop->ad_defaults->play.precision, B_TRUE) == AUDIO_SUCCESS) {
		cpenc = ad_infop->ad_defaults->play.encoding;
		cpprec = ad_infop->ad_defaults->play.precision;
	} else if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		cpenc = AUDIO_ENCODING_ULAW;
		cpprec = AUDIO_PRECISION_8;
	} else if (am_ck_combinations(ad_infop->ad_play_comb,
	    AUDIO_ENCODING_LINEAR, AUDIO_PRECISION_8, B_TRUE) ==
	    AUDIO_SUCCESS) {
		/* as long as we can do 8-bit PCM we can do u-law */
		cpenc = AUDIO_ENCODING_ULAW;
		cpprec = AUDIO_PRECISION_8;
	} else {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "bad COMPAT play combinations list");
		goto error_unregister_both;
	}

	if (am_ck_combinations(ad_infop->ad_rec_comb,
	    ad_infop->ad_defaults->record.encoding,
	    ad_infop->ad_defaults->record.precision, B_TRUE) == AUDIO_SUCCESS) {
		crenc = ad_infop->ad_defaults->record.encoding;
		crprec = ad_infop->ad_defaults->record.precision;
	} else if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_ULAW, AUDIO_PRECISION_8, B_TRUE) == AUDIO_SUCCESS) {
		crenc = hw_info->record.encoding;
		crprec = hw_info->record.precision;
	} else if (am_ck_combinations(ad_infop->ad_rec_comb,
	    AUDIO_ENCODING_LINEAR, AUDIO_PRECISION_8, B_TRUE) ==
	    AUDIO_SUCCESS) {
		/* as long as we can do 8-bit PCM we can do u-law */
		crenc = AUDIO_ENCODING_ULAW;
		crprec = AUDIO_PRECISION_8;
	} else {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "bad COMPAT record combinations list");
		goto error_unregister_both;
	}

	ATRACE_32("am_attach() COMPAT play precision", cpprec);
	ATRACE_32("am_attach() COMPAT play encoding", cpenc);
	ATRACE_32("am_attach() COMPAT record precision", crprec);
	ATRACE_32("am_attach() COMPAT record encoding", crenc);

	/* initialize the Codec's state */
	if (mode == AM_MIXER_MODE && ad_infop->ad_codec_type == AM_TRAD_CODEC) {
		hw_info->play.sample_rate = mpsr;
		hw_info->play.channels = mpch;
		hw_info->play.precision = mpprec;
		hw_info->play.encoding = mpenc;
		pgain = AM_DEFAULT_MIXER_GAIN;
		pbalance = AUDIO_MID_BALANCE;
		hw_info->record.sample_rate = mrsr;
		hw_info->record.channels = mrch;
		hw_info->record.precision = mrprec;
		hw_info->record.encoding = mrenc;
		rgain = AUDIO_MID_GAIN;
		rbalance = AUDIO_MID_BALANCE;
	} else {
		hw_info->play.sample_rate = cpsr;
		hw_info->play.channels = cpch;
		hw_info->play.precision = cpprec;
		hw_info->play.encoding = cpenc;
		pgain = ad_infop->ad_defaults->play.gain;
		pbalance = ad_infop->ad_defaults->play.balance;
		hw_info->record.sample_rate = crsr;
		hw_info->record.channels = crch;
		hw_info->record.precision = crprec;
		hw_info->record.encoding = crenc;
		rgain = ad_infop->ad_defaults->record.gain;
		rbalance = ad_infop->ad_defaults->record.balance;
	}
	hw_info->play.gain = 0xffffffff;
	hw_info->play.balance = AUDIO_MID_BALANCE;
	hw_info->record.gain = 0xffffffff;
	hw_info->record.balance = AUDIO_MID_BALANCE;

	/* set the hardware gains */
	if (am_set_gain(apm_infop1, hw_info->play.channels,
	    pgain, pbalance, AUDIO_PLAY, dev_instance,
	    AMAD_SET_CONFIG_BOARD) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() couldn't set play gain");
		goto error_rem_minor;
	}
	hw_info->play.gain = pgain;
	hw_info->play.balance = pbalance;
	if (am_set_gain(apm_infop1, hw_info->record.channels,
	    rgain, rbalance, AUDIO_RECORD, dev_instance,
	    AMAD_SET_CONFIG_BOARD) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() couldn't set record gain");
		goto error_rem_minor;
	}
	hw_info->record.gain = rgain;
	hw_info->record.balance = rbalance;

	/* initialize the saved state for when we go from COMPAT->MIXER mode */
	stpptr->save_psr = mpsr;
	stpptr->save_pch = mpch;
	stpptr->save_pprec = mpprec;
	stpptr->save_penc = mpenc;
	stpptr->save_pgain = hw_info->play.gain;
	stpptr->save_pbal = hw_info->play.balance;
	stpptr->save_hw_pgain = AM_DEFAULT_MIXER_GAIN;
	stpptr->save_hw_pbal = AUDIO_MID_BALANCE;
	stpptr->save_rsr = mrsr;
	stpptr->save_rch = mrch;
	stpptr->save_rprec = mrprec;
	stpptr->save_renc = mrenc;
	stpptr->save_rgain = hw_info->record.gain;
	stpptr->save_rbal = hw_info->record.balance;
	stpptr->save_hw_rgain = AUDIO_MID_GAIN;
	stpptr->save_hw_rbal = AUDIO_MID_BALANCE;
	stpptr->save_hw_muted = ad_infop->ad_defaults->output_muted;

	/* allocate mix and send buffers, but only if TRAD CODEC */
	stpptr->mix_size = mixer_bufsize;
	stpptr->send_size = mixer_bufsize;
	if (ad_infop->ad_codec_type == AM_TRAD_CODEC) {
		/* allocate the mix buffer */
		stpptr->mix_buf = kmem_alloc(stpptr->mix_size, KM_SLEEP);

		/* allocate the send buffer */
		stpptr->send_buf = kmem_alloc(stpptr->send_size, KM_SLEEP);
	} else {
		ASSERT(ad_infop->ad_codec_type == AM_MS_CODEC);
		stpptr->mix_size = 0;
		stpptr->mix_buf = NULL;
	}

	/* create the devices for successive open()'s to clone off of */
	if ((minor = (dev_instance * audio_sup_get_minors_per_inst()) +
	    audio_sup_getminor(NODEV, AUDIO)) == AUDIO_FAILURE) {
		ATRACE("am_attach() sound,audio get minor failure", minor);
		cmn_err(CE_NOTE,
		    "mixer: attach() create audio minor node failure");
		goto error_unregister_both;
	}
	if (ddi_create_minor_node(dip, "sound,audio", S_IFCHR, minor,
	    DDI_NT_AUDIO, 0) == DDI_FAILURE) {
		ATRACE("am_attach() sound,audio minor dev failure", 0);
		cmn_err(CE_NOTE,
		    "mixer: attach() create audio minor node failure");
		goto error_unregister_both;
	}

	if ((minor = (dev_instance * audio_sup_get_minors_per_inst()) +
	    audio_sup_getminor(NODEV, AUDIOCTL)) == AUDIO_FAILURE) {
		ATRACE("am_attach() sound,audiotcl get minor failure", minor);
		cmn_err(CE_NOTE,
		    "mixer: attach() create audiotcl minor node failure");
		goto error_rem_minor;
	}
	if (ddi_create_minor_node(dip, "sound,audioctl", S_IFCHR, minor,
	    DDI_NT_AUDIO, 0) == DDI_FAILURE) {
		ATRACE("am_attach() sound,audioctl minor dev failure", 0);
		cmn_err(CE_NOTE,
		    "mixer: attach() create audioctl minor node failure");
		goto error_rem_minor;
	}

	/* now the final step, program the Codec */
	if (am_set_format(ad_infop->ad_entry, stpptr->flags, dev_instance,
	    AMAD_SET_CONFIG_BOARD, AUDIO_PLAY, hw_info->play.sample_rate,
	    hw_info->play.channels, hw_info->play.precision,
	    hw_info->play.encoding) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "couldn't set play data format: %d %d %d %d",
		    hw_info->play.sample_rate, hw_info->play.channels,
		    hw_info->play.precision, hw_info->play.encoding);
		goto error_rem_minor;
	}
	if (am_set_format(ad_infop->ad_entry, stpptr->flags, dev_instance,
	    AMAD_SET_CONFIG_BOARD, AUDIO_RECORD,
	    hw_info->record.sample_rate, hw_info->record.channels,
	    hw_info->record.precision, hw_info->record.encoding) ==
	    AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() "
		    "couldn't set record data format: %d %d %d %d",
		    hw_info->record.sample_rate, hw_info->record.channels,
		    hw_info->record.precision, hw_info->record.encoding);
		goto error_rem_minor;
	}

	if (ad_infop->ad_entry->ad_set_config(dev_instance,
	    AMAD_SET_CONFIG_BOARD, AMAD_SET_PORT, AUDIO_PLAY,
	    ad_infop->ad_defaults->play.port, NULL) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() couldn't set play port: 0x%x",
		    ad_infop->ad_defaults->play.port);
		goto error_rem_minor;
	}
	if (ad_infop->ad_entry->ad_set_config(dev_instance,
	    AMAD_SET_CONFIG_BOARD, AMAD_SET_PORT, AUDIO_RECORD,
	    ad_infop->ad_defaults->record.port, NULL) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() couldn't set record port: 0x%x",
		    ad_infop->ad_defaults->record.port);
		goto error_rem_minor;
	}

	if ((ad_infop->ad_defaults->hw_features & AUDIO_HWFEATURE_IN2OUT) &&
	    ad_infop->ad_entry->ad_set_config(dev_instance,
	    AMAD_SET_CONFIG_BOARD, AMAD_SET_MONITOR_GAIN, NULL,
	    ad_infop->ad_defaults->monitor_gain, NULL) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() couldn't set monitor gain: 0x%x",
		    ad_infop->ad_defaults->monitor_gain);
		goto error_rem_minor;
	}

	if (ad_infop->ad_entry->ad_set_config(dev_instance,
	    AMAD_SET_CONFIG_BOARD, AMAD_OUTPUT_MUTE, NULL,
	    ad_infop->ad_defaults->output_muted, NULL) == AUDIO_FAILURE) {
		cmn_err(CE_NOTE,
		    "mixer: attach() couldn't set output muted: 0x%x",
		    ad_infop->ad_defaults->output_muted);
		goto error_rem_minor;
	}

	if ((ad_infop->ad_assist_flags & AM_ASSIST_MIC) &&
	    ad_infop->ad_entry->ad_set_config(dev_instance,
	    AMAD_SET_CONFIG_BOARD, AMAD_MIC_BOOST, NULL,
	    (ad_infop->ad_add_mode & AM_ADD_MODE_MIC_BOOST), NULL) ==
	    AUDIO_FAILURE) {
		cmn_err(CE_NOTE, "mixer: attach() couldn't set mic boost: 0x%x",
		    ad_infop->ad_add_mode);
		goto error_rem_minor;
	}

	hw_info->play.port = ad_infop->ad_defaults->play.port;
	hw_info->play.avail_ports = ad_infop->ad_defaults->play.avail_ports;
	hw_info->play.mod_ports = ad_infop->ad_defaults->play.mod_ports;
	hw_info->play.buffer_size = ad_infop->ad_defaults->play.buffer_size;
	hw_info->play.samples = 0;
	hw_info->play.eof = 0;
	hw_info->play.pause = 0;
	hw_info->play.error = 0;
	hw_info->play.waiting = 0;
	hw_info->play.minordev = 0;
	hw_info->play.open = 0;
	hw_info->play.active = 0;
	hw_info->record.port = ad_infop->ad_defaults->record.port;
	hw_info->record.avail_ports =
	    ad_infop->ad_defaults->record.avail_ports;
	hw_info->record.mod_ports = ad_infop->ad_defaults->record.mod_ports;
	hw_info->record.buffer_size =
	    ad_infop->ad_defaults->record.buffer_size;
	hw_info->record.samples = 0;
	hw_info->record.eof = 0;
	hw_info->record.pause = 0;
	hw_info->record.error = 0;
	hw_info->record.waiting = 0;
	hw_info->record.minordev = 0;
	hw_info->record.open = 0;
	hw_info->record.active = 0;

	hw_info->monitor_gain = ad_infop->ad_defaults->monitor_gain;
	hw_info->output_muted = ad_infop->ad_defaults->output_muted;
	hw_info->hw_features = ad_infop->ad_defaults->hw_features;
	hw_info->sw_features = ad_infop->ad_defaults->sw_features;

	if (mode == AM_MIXER_MODE) {
		hw_info->sw_features_enabled = AUDIO_SWFEATURE_MIXER;
	} else {
		hw_info->sw_features_enabled = 0;
	}

	ATRACE_32("am_attach() returning", dev_instance);

	return (dev_instance);

error_rem_minor:
	ATRACE("am_attach() failure, removing minor nodes", 0);
	/*
	 * We don't use NULL because other APMs may own other minor devices
	 * and we don't want to remove them from under them.
	 */
	ddi_remove_minor_node(dip, "sound,audio");
	ddi_remove_minor_node(dip, "sound,audioctl");

error_unregister_both:
	apm_infop2->apm_private = NULL;
	if (audio_sup_unregister_apm(statep, AUDIOCTL) == AUDIO_FAILURE) {
		ATRACE_32("am_attach() audio_sup_unregister_apm() "
		    "AUDIOCTL failed", dev_instance);
		cmn_err(CE_NOTE, "mixer: attach() audio_sup_unregister_apm() "
		    "AUDIOCTL failed");
	}

error_unregister:
	apm_infop1->apm_private = NULL;
	if (audio_sup_unregister_apm(statep, AUDIO) == AUDIO_FAILURE) {
		ATRACE_32("am_attach() audio_sup_unregister_apm() "
		    "AUDIO failed", dev_instance);
		cmn_err(CE_NOTE, "mixer: attach() audio_sup_unregister_apm() "
		    "AUDIO failed");
	}

error_free_private:
	ATRACE("am_attach() failure, freeing private structure", 0);
	if (stpptr->mix_buf) {
		kmem_free(stpptr->mix_buf, stpptr->mix_size);
	}
	if (stpptr->send_buf) {
		kmem_free(stpptr->send_buf, stpptr->send_size);
	}
	kmem_free(stpptr, sizeof (*stpptr));

	ATRACE("am_attach() returning failure", 0);

	return (AUDIO_FAILURE);

}	/* am_attach() */

/*
 * am_detach()
 *
 * Description:
 *	Detach an instance of the mixer. Free up all data structures and
 *	unregister the APM for both AUDIO and AUDIOCTL. We also remove
 *	the device nodes. However it is possible another APM may still be
 *	attached, so we are careful to only remove the audio and audioctl
 *	devices.
 *
 *	NOTE: This routine will never be called in the audio device
 *		has any channels in use, so we don't need to check
 *		for this.
 *
 *
 * Arguments:
 *	def_info_t	*dip	Pointer to the device's def_info structure
 *	ddi_detach_cmd_t cmd	Detach command
 *
 * Returns:
 *	AUDIO_SUCCESS		If the mixer was detached
 *	AUDIO_FAILURE		If the mixer couldn't be detached
 */
int
am_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	audio_state_t		*statep;
	audio_apm_info_t	*apm_infop1;
	audio_apm_info_t	*apm_infop2;
	am_apm_private_t	*stpptr;

	ATRACE_32("in am_detach()", cmd);

	switch (cmd) {
	case DDI_DETACH:
		break;
	default:
		ATRACE_32("am_detach() unknown command failure", cmd);
		return (AUDIO_FAILURE);
	}

	/* get state structure */
	if ((statep = audio_sup_get_state(dip, NODEV)) == NULL) {
		ATRACE("am_detach() audio_sup_get_state() failed", statep);
		cmn_err(CE_NOTE,
		    "mixer: detach() audio_sup_get_state() failed");
		return (AUDIO_FAILURE);
	}

	/*
	 * Remove only the AUDIO and AUDIOCTL minor nodes for this
	 * dev_info. We don't want to free the nodes other APMs are
	 * responsible for.
	 */
	ddi_remove_minor_node(dip, "sound,audio");
	ddi_remove_minor_node(dip, "sound,audioctl");

	/* get rid of the private data structure */
	if ((apm_infop1 = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_detach() audio_sup_get_apm_info() AUDIO failed",
		    statep);
		cmn_err(CE_NOTE,
		    "mixer: detach() audio_sup_get_apm_info() AUDIO failed");
		return (AUDIO_FAILURE);
	}
	if ((apm_infop2 =
	    audio_sup_get_apm_info(statep, AUDIOCTL)) == NULL) {
		ATRACE("am_detach() audio_sup_get_apm_info() AUDIOCTL failed",
		    statep);
		cmn_err(CE_NOTE,
		    "mixer: detach() audio_sup_get_apm_info() AUDIOCTL failed");
		return (AUDIO_FAILURE);
	}
	/*
	 * Both apm_info pointers use the same apm_private sturcture, so we
	 * only need to clear it once.
	 */
	stpptr = apm_infop1->apm_private;
	ASSERT(stpptr->active_ioctls == 0);
	ASSERT(stpptr->open_waits == 0);

	/* destroy mutexs and cvs */
	mutex_destroy(&stpptr->lock);
	cv_destroy(&stpptr->cv);

	if (stpptr->mix_buf) {
		ASSERT(stpptr->mix_size);
		kmem_free(stpptr->mix_buf, stpptr->mix_size);
	}
	if (stpptr->send_buf) {
		ASSERT(stpptr->send_size);
		kmem_free(stpptr->send_buf, stpptr->send_size);
	}
	kmem_free(stpptr, sizeof (*stpptr));

	apm_infop1->apm_private = NULL;
	apm_infop2->apm_private = NULL;

	if (audio_sup_unregister_apm(statep, AUDIO) == AUDIO_FAILURE) {
		ATRACE("am_detach() audio_sup_unregister_apm() "
			"AUDIO failed", statep);
		cmn_err(CE_NOTE, "mixer: detach() audio_sup_unregister_apm() "
		    "AUDIO failed");
	}

	if (audio_sup_unregister_apm(statep, AUDIOCTL) == AUDIO_FAILURE) {
		ATRACE("am_detach() audio_sup_unregister_apm() "
			"AUDIOCTL failed", statep);
		cmn_err(CE_NOTE, "mixer: detach() audio_sup_unregister_apm() "
		    "AUDIOCTL failed");
	}

	ATRACE("am_detach() done, doing audio_detach() in return", 0);

	return (AUDIO_SUCCESS);

}	/* am_detach() */

/*
 * am_get_audio()
 *
 * Description:
 *	This routine directs the call to get audio depending on whether or
 *	not the CODEC is a traditional or multi-channel CODEC. It also does
 *	some error checking to make sure the call is valid.
 *
 *	It is an error for the number of samples to not be modulo the
 *	hardware's number of channels. The old diaudio driver would hang
 *	the channel until it was closed. We throw away enough of the damaged
 *	sample and press on.
 *
 *	We support devices that accept only unsigned linear PCM by translating
 *	the final data, if required.
 *
 *	NOTE: The variable "samples" is the number of samples the hardware
 *		wants. So it is samples at the hardware's sample rate.
 *
 * Arguments:
 *	dev_info_t	*dip	The device we are getting audio data for
 *	dev_t		dev	The device we are getting audio data for
 *	void		*buf	The buffer to place the audio into
 *	int		channel	For multi-channel CODECs this is the channel
 *				that will be playing the sound.
 *	int		samples	The number of samples to get
 *
 * Returns
 *	>= 0			The number of samples transferred to the buffer
 *	AUDIO_FAILURE		An error has occurred
 */
int
am_get_audio(dev_info_t *dip, dev_t dev, void *buf, int channel,
	int samples)
{
	audio_state_t		*statep;
	audio_info_t		*hw_info;
	audio_apm_info_t	*apm_infop;
	am_ad_info_t		*ad_infop;
	uint_t			encoding;
	uint_t			flags;
	uint_t			translate;
	int			i;
	int			rc;

	ATRACE_32("in am_get_audio() samples requested", samples);

	/* get the state pointer for this instance */
	if ((statep = audio_sup_get_state(dip, dev)) == NULL) {
		ATRACE("am_get_audio() audio_sup_get_state() failed", 0);
		cmn_err(CE_NOTE, "mixer: get_audio() "
		    "audio_sup_get_state() failed, audio lost");
		return (AUDIO_FAILURE);
	}

	if ((apm_infop = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_get_audio() audio_sup_get_apm_info() AUDIO failed",
		    statep);
		cmn_err(CE_NOTE, "mixer: get_audio() "
		    "audio_sup_get_apm_info() AUDIO failed, audio lost");
		return (AUDIO_FAILURE);
	}

	ad_infop = apm_infop->apm_ad_infop;
	hw_info = apm_infop->apm_ad_state;
	encoding = hw_info->play.encoding;
	translate = ad_infop->ad_translate_flags;
	flags = ((am_apm_private_t *)apm_infop->apm_private)->flags;

	/* make sure the number of samples is modulo the # of H/W channels */
	if (hw_info->play.channels != AUDIO_CHANNELS_MONO &&
	    samples % hw_info->play.channels != 0) {
		ATRACE_32("am_get_audio() bad sample size", samples);
		cmn_err(CE_NOTE, "mixer: get_audio() "
		    "number of samples not modulo the number of "
		    "hardware channels, audio samples lost");
		samples -= samples % hw_info->play.channels;
	}

	/* deal with multi-channel CODECs or regular CODECs */
	if (ad_infop->ad_codec_type == AM_MS_CODEC) {
		ATRACE("am_get_audio() calling am_get_audio_multi()", statep);
		rc = am_get_audio_multi(statep, buf, channel, samples);
		ATRACE_32("am_get_audio() am_get_audio_multi() returning", rc);
	} else {
		ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);
		ATRACE("am_get_audio() calling am_get_audio_trad()", statep);
		rc = am_get_audio_trad(statep, buf, samples, apm_infop);
		ATRACE_32("am_get_audio() am_get_audio_trad() returning", rc);
	}

	/* do we need to translate? */
	if (hw_info->play.precision == AUDIO_PRECISION_16) {
		ASSERT(encoding == AUDIO_ENCODING_LINEAR);

		if (translate & AM_MISC_16_P_TRANSLATE) {
			uint16_t	*ptr = (uint16_t *)buf;

			for (i = rc; i--; ptr++) {
				*ptr = ((short)*ptr) + -INT16_MIN;
			}
		}
	} else {
		ASSERT(hw_info->play.precision == AUDIO_PRECISION_8);

		if (encoding == AUDIO_ENCODING_LINEAR &&
		    (translate & AM_MISC_8_P_TRANSLATE)) {
			uint8_t		*ptr = (uint8_t *)buf;

			for (i = rc; i--; ptr++) {
				*ptr = ((char)*ptr) + -INT8_MIN;
			}
		} else if (encoding == AUDIO_ENCODING_ULAW &&
		    (flags & AM_PRIV_PULAW_TRANS)) {
			int8_t		*aptr;
			int8_t		*ptr = (int8_t *)buf;

			aptr = &_8ulaw2linear8[G711_256_ARRAY_MIDPOINT];
			if (translate & AM_MISC_8_P_TRANSLATE) {
			    for (i = rc; i--; ptr++) {
				*ptr = aptr[*ptr] + -INT8_MIN;
			    }
			} else {
			    for (i = rc; i--; ptr++) {
				*ptr = aptr[*ptr];
			    }
			}
		} else if (encoding == AUDIO_ENCODING_ALAW &&
		    (flags & AM_PRIV_PALAW_TRANS)) {
			int8_t		*aptr;
			int8_t		*ptr = (int8_t *)buf;

			aptr = &_8alaw2linear8[G711_256_ARRAY_MIDPOINT];
			if (translate & AM_MISC_8_P_TRANSLATE) {
			    for (i = rc; i--; ptr++) {
				*ptr = aptr[*ptr] + -INT8_MIN;
			    }
			} else {
			    for (i = rc; i--; ptr++) {
				*ptr = aptr[*ptr];
			    }

			}
		}
	}

	return (rc);

}	/* am_get_audio() */

/*
 * am_play_shutdown()
 *
 * Description:
 *	This routine is used to clean things up when the Audio Driver will
 *	no longer be servicing it's play interrupts. I.e., play interrupts
 *	have been turned off.
 *
 *	This routine makes sure that any DRAINs waiting for an interrupt are
 *	cleared. This is another instance of Case #3.
 *
 *	It is also used to coordinate shutting down play so that we can
 *	switch between MIXER and COMPAT modes.
 *
 * Arguments:
 *	dev_info_t	*dip		The device we are shutting down
 *	dev_t		dev		The device we are shutting down
 *	int		channel		For multi-stream Codecs this is the
 *					stream to shutdown.
 *
 * Returns:
 *	void
 */
void
am_play_shutdown(dev_info_t *dip, dev_t dev, int channel)
{
	audio_state_t			*statep;
	audio_ch_t			*chptr;
	am_ch_private_t			*chpptr;
	audio_info_t			*info;
	audio_info_t			*hw_info;
	audio_apm_info_t		*apm_infop;
	am_apm_private_t		*stpptr;
	am_ad_info_t			*ad_infop;
	int				i;
	int				max_chs;

	ATRACE("in am_play_shutdown()", dip);

	/* get the state pointer and the number of chs for this instance */
	if ((statep = audio_sup_get_state(dip, dev)) == NULL) {
		ATRACE("am_play_shutdown() audio_sup_get_state() failed", 0);
		cmn_err(CE_NOTE,
		    "mixer: play_shutdown() audio_sup_get_state() failed");
		return;
	}

	max_chs = statep->as_max_chs;
	if ((apm_infop = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_play_shutdown() audio_sup_get_apm_info() failed", 0);
		cmn_err(CE_NOTE,
		    "mixer: play_shutdown() audio_sup_get_apm_info() failed");
		return;
	}
	ad_infop = apm_infop->apm_ad_infop;
	stpptr = apm_infop->apm_private;
	hw_info = apm_infop->apm_ad_state;

	/* deal with multi-channel CODECs vs. regular CODECs */
	if (ad_infop->ad_codec_type == AM_MS_CODEC) {
		chptr = &statep->as_channels[channel];
		mutex_enter(&chptr->ch_lock);
		/*
		 * The channel may have been closed while we waited
		 * on the mutex. So once we get it we make sure the
		 * channel is still valid.
		 */
		chpptr = chptr->ch_private;
		if (chpptr == NULL || chptr->ch_info.info == NULL ||
		    (chpptr->flags & AM_CHNL_OPEN) == 0) {
			ATRACE("am_play_shutdown(M) channel closed", chptr);
			mutex_exit(&chptr->ch_lock);
			return;
		} else if (chpptr->flags &
		    (AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT|AM_CHNL_EMPTY|\
		    AM_CHNL_ALMOST_EMPTY1|AM_CHNL_ALMOST_EMPTY2|\
		    AM_CHNL_CLOSING)) {
			/* don't clear DRAIN_SIG so am_close() doesn't block */
			chpptr->flags &=
			    ~(AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT|\
			    AM_CHNL_ALMOST_EMPTY1|AM_CHNL_ALMOST_EMPTY2);

			/* are we empty? */
			if (audio_sup_get_msg_cnt(chptr) == 0) {
				/* yes, so mark as empty */
				chpptr->flags |= AM_CHNL_EMPTY;
			}

			cv_signal(&chptr->ch_cv);

			/* turn off the channel, but only if not paused */
			info = chptr->ch_info.info;
			if (!info->play.pause) {
				info->play.active = 0;
			}

			/* make sure we send all pending signals */
			for (; chpptr->EOF[chpptr->EOF_toggle];
			    chpptr->EOF[chpptr->EOF_toggle]--) {
				am_send_signal(statep);
				info->play.eof++;
			}
			AUDIO_TOGGLE(chpptr->EOF_toggle);
			for (; chpptr->EOF[chpptr->EOF_toggle];
			    chpptr->EOF[chpptr->EOF_toggle]--) {
				am_send_signal(statep);
				info->play.eof++;
			}
			mutex_exit(&chptr->ch_lock);
		}
	} else {
		ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);

		if (stpptr->flags & AM_PRIV_SW_MODES) {
			ATRACE("am_play_shutdown() change mode shutdown",
			    stpptr);

			hw_info->play.active = 0;

			/* let the mode change proceed */
			mutex_enter(&stpptr->lock);
			cv_signal(&stpptr->cv);
			mutex_exit(&stpptr->lock);

			return;
		}

		/* go through all the channels */
		for (i = 0, chptr = &statep->as_channels[0];
		    i < max_chs; i++, chptr++) {
			/* skip non-AUDIO and unallocated channels */
			if (chptr->ch_info.dev_type != AUDIO &&
			    chptr->ch_info.pid == 0) {
				continue;
			}

			/* lock the channel before we check it out */
			mutex_enter(&chptr->ch_lock);

			/*
			 * The channel may have been closed while we waited
			 * on the mutex. So once we get it we make sure the
			 * channel is still valid.
			 */
			chpptr = chptr->ch_private;
			if (chpptr == NULL || chptr->ch_info.info == NULL ||
			    (chpptr->flags & AM_CHNL_OPEN) == 0) {
				mutex_exit(&chptr->ch_lock);
				ATRACE("am_play_shutdown(T) channel closed",
				    chptr);
				continue;
			}

			ATRACE_32("am_play_shutdown(T) checking flags",
			    chpptr->flags);

			info = chptr->ch_info.info;
			if (chpptr->flags &
			    (AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT|\
			    AM_CHNL_EMPTY|AM_CHNL_ALMOST_EMPTY1|\
			    AM_CHNL_ALMOST_EMPTY2|AM_CHNL_CLOSING)) {
				/*
				 * Don't clear DRAIN_SIG so am_close()
				 * doesn't block.
				 */
				chpptr->flags &=
				    ~(AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT|\
				    AM_CHNL_ALMOST_EMPTY1|\
				    AM_CHNL_ALMOST_EMPTY2);

				/* are we empty? */
				if (audio_sup_get_msg_cnt(chptr) == 0) {
					/* yes, so mark as empty */
					chpptr->flags |= AM_CHNL_EMPTY;
				}

				ATRACE("am_play_shutdown(T) regular drain",
				    chptr);

				/* turn off the ch, but only if not paused */
				if (!info->play.pause) {
					/* before the cv_signal() */
					info->play.active = 0;
				}

				cv_signal(&chptr->ch_cv);
			} else {
				/* turn off the ch, but only if not paused */
				if (!info->play.pause) {
					info->play.active = 0;
				}
			}

			/* make sure we send all pending signals */
			chpptr = chptr->ch_private;
			for (; chpptr->EOF[chpptr->EOF_toggle];
			    chpptr->EOF[chpptr->EOF_toggle]--) {
				am_send_signal(statep);
				info->play.eof++;
			}
			AUDIO_TOGGLE(chpptr->EOF_toggle);
			for (; chpptr->EOF[chpptr->EOF_toggle];
			    chpptr->EOF[chpptr->EOF_toggle]--) {
				am_send_signal(statep);
				info->play.eof++;
			}

			/* we can free the lock now */
			mutex_exit(&chptr->ch_lock);
		}

		/* turn off the hardware active flag */
		hw_info->play.active = 0;
	}

	ATRACE("am_play_shutdown() returning", statep);

	return;

}	/* am_play_shutdown() */

/*
 * am_send_audio()
 *	This routine directs the call to send audio depending on whether or
 *	not the CODEC is a traditional or multi-channel CODEC. It also does
 *	some error checking to make sure the call is valid.
 *
 *	It is an error for the number of samples to not be modulo the
 *	hardware's number of channels. The old diaudio driver would hang
 *	the channel until it was closed. We throw away enough of the damaged
 *	sample and press on.
 *
 *	We support devices that provide only unsigned linear PCM by translating
 *	the data to signed before it is used.
 *
 *	NOTE: The variable "samples" is the number of samples the hardware
 *		sends. So it is samples at the hardware's sample rate.
 *
 * Description:
 *
 * Arguments:
 *	dev_info_t	*dip	The device we are getting audio data from
 *	dev_t		dev	The device we are getting audio data from
 *	void		*buf	The buffer the audio is in
 *	int		channel	For multi-channel CODECs this is the channel
 *				that will be receiving the audio.
 *	int		samples	The number of samples to send
 *
 * Returns
 *	void
 */
void
am_send_audio(dev_info_t *dip, dev_t dev, void *buf, int channel, int samples)
{
	audio_state_t			*statep;
	audio_apm_info_t		*apm_infop;
	audio_info_t			*hw_info;
	am_ad_info_t			*ad_infop;
	uint_t				encoding;
	uint_t				flags;
	uint_t				translate;
	int				i;

	ATRACE("in am_send_audio()", dip);

	/* get the state pointer for this instance */
	if ((statep = audio_sup_get_state(dip, dev)) == NULL) {
		ATRACE("am_send_audio() audio_sup_get_state() failed", 0);
		cmn_err(CE_NOTE,
		    "mixer: send_audio() audio_sup_get_state() failed,"
		    " recorded audio lost");
		return;
	}

	if ((apm_infop = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_send_audio() audio_sup_get_apm_info() AUDIO failed",
		    statep);
		cmn_err(CE_NOTE,
		    "mixer: send_audio() audio_sup_get_apm_info() AUDIO failed,"
		    " recorded audio lost");
		return;
	}

	ad_infop = apm_infop->apm_ad_infop;
	hw_info = apm_infop->apm_ad_state;
	encoding = hw_info->record.encoding;
	translate = ad_infop->ad_translate_flags;
	flags = ((am_apm_private_t *)apm_infop->apm_private)->flags;

	/* make sure the number of samples is modulo the # of H/W channels */
	if (hw_info->record.channels != AUDIO_CHANNELS_MONO &&
	    ((samples % hw_info->record.channels) != 0)) {
		ATRACE_32("am_send_audio() bad sample size", samples);
		cmn_err(CE_NOTE,
		    "mixer: send_audio() number of samples not modulo "
		    "the number of hardware channels, audio samples lost");
		samples -= samples % hw_info->record.channels;
	}

	/* de we need to translate? */
	if (hw_info->record.precision == AUDIO_PRECISION_16) {
		ASSERT(encoding == AUDIO_ENCODING_LINEAR);

		if (translate & AM_MISC_16_R_TRANSLATE) {
			int16_t		*ptr = (int16_t *)buf;

			for (i = samples; i--; ptr++) {
				*ptr = (short)*ptr + INT16_MIN;
			}
		}
	} else {
		ASSERT(hw_info->record.precision == AUDIO_PRECISION_8);

		if (encoding == AUDIO_ENCODING_LINEAR &&
		    (translate & AM_MISC_8_R_TRANSLATE)) {
			char		*ptr = (char *)buf;

			for (i = samples; i--; ptr++) {
				*ptr = *ptr + INT8_MIN;
			}
		} else if (encoding == AUDIO_ENCODING_ULAW &&
		    (flags & AM_PRIV_RULAW_TRANS)) {
			char	*ptr = (char *)buf;
			uint8_t	*cptr = _14linear2ulaw8;

			/*
			 * Converting from signed and unsigned PCM to u-law
			 * doesn't require a shift, as was done above. This
			 * is because an offset of 128 just changes the phase
			 * 180 degrees, but not the signal. So we don't bother
			 * fixing this. The same for A-law.
			 */
			for (i = samples; i--; ptr++) {
				*ptr = cptr[((uint8_t)*ptr)<<6];
			}
		} else if (encoding == AUDIO_ENCODING_ALAW &&
		    (flags & AM_PRIV_RALAW_TRANS)) {
			char	*ptr = (char *)buf;
			uint8_t	*cptr = _13linear2alaw8;

			for (i = samples; i--; ptr++) {
				*ptr = cptr[((uint8_t)*ptr)<<5];
			}
		}
	}

	/* deal with multi-channel CODECs or regular CODECs */
	if (ad_infop->ad_codec_type == AM_MS_CODEC) {
		ATRACE("am_send_audio() calling am_send_audio_multi()",
		    statep);
		ASSERT(channel != AUDIO_NO_CHANNEL);
		am_send_audio_multi(statep, buf, channel, samples);
		ATRACE_32("am_send_audio() am_send_audio_multi() returning", 0);
	} else {
		ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);
		ATRACE("am_send_audio() calling am_send_audio_trad()",
		    statep);
		ASSERT(channel == AUDIO_NO_CHANNEL);
		am_send_audio_trad(statep, buf, samples, apm_infop);
		ATRACE_32("am_send_audio() am_send_audio_trad() returning", 0);
	}

}	/* am_send_audio() */

/*
 * am_get_src_data()
 *
 * Description:
 *	This routine returns the PLAY or RECORD sample rate conversion
 *	data structure that is saved in the channel's private data structure.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel state structure
 *	int		dir		AUDIO_PLAY or AUDIO_RECORD
 *
 * Returns:
 *	void *				Pointer to the sample rate conversion
 *					structure
 */
void *
am_get_src_data(audio_ch_t *chptr, int dir)
{
	am_ch_private_t		*chpptr = chptr->ch_private;

	ATRACE("in am_get_src_data()", chptr);
	ATRACE("am_get_src_data() chpptr", chpptr);
	ATRACE_32("am_get_src_data() dir", dir);

	if (dir == AUDIO_PLAY) {
		ATRACE("am_get_src_data() PLAY returning",
		    chpptr->play_src_data);
		return (chpptr->play_src_data);
	} else {
		ASSERT(dir == AUDIO_RECORD);
		ATRACE("am_get_src_data() CAPTURE returning",
		    chpptr->rec_src_data);
		return (chpptr->rec_src_data);
	}

}	/* am_get_src_data() */

/*
 * am_set_src_data()
 *
 * Description:
 *	This routine sets the PLAY or RECORD sample rate conversion
 *	data structure pointer with the pointer passed in.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel state structure
 *	int		dir		AUDIO_PLAY or AUDIO_RECORD
 *	void		*data		The sample rate conversion data
 *
 * Returns:
 *	void
 */
void
am_set_src_data(audio_ch_t *chptr, int dir, void *data)
{
	am_ch_private_t		*chpptr = chptr->ch_private;

	ATRACE("in am_set_src_data()", chptr);
	ATRACE_32("am_set_src_data() dir", dir);
	ATRACE("am_set_src_data() data", data);

	if (dir == AUDIO_PLAY) {
		ATRACE("am_set_src_data() setting PLAY", data);
		chpptr->play_src_data = data;
	} else {
		ASSERT(dir == AUDIO_RECORD);
		ATRACE("am_set_src_data() setting CAPTURE", data);
		chpptr->rec_src_data = data;
	}

}	/* am_set_src_data() */

/*
 * Private Audio Mixer Audio Personality Module Entry Points
 */

/*
 * am_audio_set_info()
 *
 * Description:
 *	This routine double checks the passed in audio_info_t structure to
 *	make sure the values are legal. If they are then they are used to
 *	update the audio hardware. In COMPAT mode all the hardware is updated,
 *	as it is for a multi-stream Codec. However tradional Codecs in MIXER
 *	mode don't update the data format or gain. Everything else can be
 *	updated.
 *
 *	After the checks are completed and the hardware has been updated
 *	the reti pointer is checked. If NULL we are done. Otherwise the
 *	structure pointed to by reti is filled in with the new hardware
 *	configuration.
 *
 *	The mixer only supports a few formats, 16-bit linear and 8-bit
 *	u-law, A-law and linear. Any other format will cause the check to
 *	fail.
 *
 *	We don't bother checking the read only members, silently ignoring any
 *	modifications.
 *
 *	XXX Need to set hardware to original state if error, especially
 *	if src_init() fails. Also, maybe move src_init() up higher so it
 *	can fail before we change hardware. Plus, it's easier to undo
 *	src_init().
 *
 *	NOTE: The Codec's lock must NOT be held when calling this routine.
 *
 *	NOTE: reti will be NULL only when this routine is being called by
 *		am_open().
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	audio_info_t	*newi		Pointer to the struct with new values
 *	audio_info_t	*reti		Pointer to the updated struct that is
 *					returned
 *
 * Returns:
 *	AUDIO_SUCCESS			Successful
 *	AUDIO_FAILURE			Failed
 */
static int
am_audio_set_info(audio_ch_t *chptr, audio_info_t *newi, audio_info_t *reti)
{
	audio_info_t		*curi;		/* current state */
	am_ch_private_t		*chpptr = chptr->ch_private;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	am_apm_private_t	*stpptr = apm_infop->apm_private;
	am_ad_info_t		*ad_infop = apm_infop->apm_ad_infop;
	audio_info_t		*hw_info = apm_infop->apm_ad_state;
	audio_info_t		tempi;		/* approved values in here */
	audio_device_type_e	type = chptr->ch_info.dev_type;
	boolean_t		ch_ctl = B_FALSE;
	boolean_t		new_play_samples = B_FALSE;
	boolean_t		new_record_samples = B_FALSE;
	boolean_t		play = B_FALSE;
	boolean_t		record = B_FALSE;
	boolean_t		start_play = B_FALSE;
	boolean_t		start_record = B_FALSE;
	boolean_t		stop_play = B_FALSE;
	boolean_t		stop_record = B_FALSE;
	int			codec_type = ad_infop->ad_codec_type;
	int			dev_instance;
	int			mode = ad_infop->ad_mode;
	int			stream;

	ATRACE("in am_audio_set_info()", chptr);

	ATRACE_32("am_audio_set_info() mode", mode);

	ASSERT(apm_infop);

	/*
	 * Are we playing and/or recording? For AUDIOCTL channels we
	 * force play and record, thus checking to see if it is changing
	 * the format. Since AUDIOCTL channels can't change the format
	 * we fail if the format isn't the same.
	 */
	curi = chptr->ch_info.info;
	if ((chptr->ch_dir & AUDIO_PLAY) || type == AUDIOCTL) {
		play = B_TRUE;
	}
	if ((chptr->ch_dir & AUDIO_RECORD) || type == AUDIOCTL) {
		record = B_TRUE;
	}

	/* we use curi to get info to check against */
	if (mode == AM_COMPAT_MODE) {
		curi = hw_info;
#ifdef DEBUG
	} else {
		/* this was set above, just be a bit paranoid */
		ASSERT(mode == AM_MIXER_MODE);
		ASSERT(curi == chptr->ch_info.info);
#endif
	}

	dev_instance = audio_sup_get_dev_instance(NODEV, chptr->ch_qptr);

	/* first make sure the new data format is legal */
	if (play && Modify(newi->play.sample_rate) &&
	    newi->play.sample_rate != curi->play.sample_rate) {
		if (type != AUDIO || am_ck_sample_rate(&ad_infop->ad_play,
		    mode, newi->play.sample_rate) == AUDIO_FAILURE) {
			goto error;
		}
		tempi.play.sample_rate = newi->play.sample_rate;
	} else {
		tempi.play.sample_rate = curi->play.sample_rate;
	}
	if (record && Modify(newi->record.sample_rate) &&
	    newi->record.sample_rate != curi->record.sample_rate) {
		if (type != AUDIO || am_ck_sample_rate(&ad_infop->ad_record,
		    mode, newi->record.sample_rate) == AUDIO_FAILURE) {
			goto error;
		}
		tempi.record.sample_rate = newi->record.sample_rate;
	} else {
		tempi.record.sample_rate = curi->record.sample_rate;
	}
	if (mode == AM_COMPAT_MODE && !(ad_infop->ad_diff_flags & AM_DIFF_SR) &&
	    tempi.play.sample_rate != tempi.record.sample_rate) {
		/* if only play or record we can fix this */
		if (*apm_infop->apm_in_chs && *apm_infop->apm_out_chs == 0) {
			/* set play to capture sample rate */
			tempi.play.sample_rate = tempi.record.sample_rate;
		} else if (*apm_infop->apm_in_chs == 0 &&
		    *apm_infop->apm_out_chs) {
			/* set capture to play sample rate */
			tempi.record.sample_rate = tempi.play.sample_rate;
		} else {
			goto error;
		}
	} else {
		/*
		 * There's a bug in audiotool which after doing an AUDIO_SETINFO
		 * it updates the state in AudioDevice.cc SetState() it uses
		 * the record side to get the new sample rate! So work around
		 * if write only. Who knows, perhaps other apps are as stupid!
		 */
		if (*apm_infop->apm_out_chs != 0 &&
		    *apm_infop->apm_in_chs == 0) {
			/* set to the same sample rate, gads! */
			tempi.record.sample_rate = tempi.play.sample_rate;
		}
	}
	ATRACE_32("am_audio_set_info() PLAY sample rate set",
	    tempi.play.sample_rate);
	ATRACE_32("am_audio_set_info() RECORD sample rate set",
	    tempi.record.sample_rate);

	if (play && Modify(newi->play.channels) &&
	    newi->play.channels != curi->play.channels) {
		if (type != AUDIO || am_ck_channels(&ad_infop->ad_play,
		    newi->play.channels) == AUDIO_FAILURE) {
			goto error;
		}
		tempi.play.channels = newi->play.channels;
	} else {
		tempi.play.channels = curi->play.channels;
	}
	if (record && Modify(newi->record.channels) &&
	    newi->record.channels != curi->record.channels) {
		if (type != AUDIO || am_ck_channels(&ad_infop->ad_record,
		    newi->record.channels) == AUDIO_FAILURE) {
			goto error;
		}
		tempi.record.channels = newi->record.channels;
	} else {
		tempi.record.channels = curi->record.channels;
	}
	if (mode == AM_COMPAT_MODE && !(ad_infop->ad_diff_flags & AM_DIFF_CH) &&
	    tempi.play.channels != tempi.record.channels) {
		/* if only play or record we can fix this */
		if (*apm_infop->apm_in_chs && *apm_infop->apm_out_chs == 0) {
			/* set play to capture sample rate */
			tempi.play.channels = tempi.record.channels;
		} else if (*apm_infop->apm_in_chs != 0 &&
		    *apm_infop->apm_out_chs) {
			/* set capture to play sample rate */
			tempi.record.channels = tempi.play.channels;
		} else {
			goto error;
		}
	} else {
		/* see audiotool bug description in set sample_rate above */
		if (*apm_infop->apm_out_chs != 0 &&
		    *apm_infop->apm_in_chs == 0) {
			/* set to the same channels, gads! */
			tempi.record.channels = tempi.play.channels;
		}
	}
	ATRACE_32("am_audio_set_info() PLAY channels set",
	    tempi.play.channels);
	ATRACE_32("am_audio_set_info() RECORD channels set",
	    tempi.record.channels);

	if (play && Modify(newi->play.precision) &&
	    newi->play.precision != curi->play.precision) {
		if (type != AUDIO) {
			goto error;
		}
		tempi.play.precision = newi->play.precision;
	} else {
		tempi.play.precision = curi->play.precision;
	}
	if (record && Modify(newi->record.precision) &&
	    newi->record.precision != curi->record.precision) {
		if (type != AUDIO) {
			goto error;
		}
		tempi.record.precision = newi->record.precision;
	} else {
		tempi.record.precision = curi->record.precision;
	}
	if (mode == AM_COMPAT_MODE &&
	    !(ad_infop->ad_diff_flags & AM_DIFF_PREC) &&
	    tempi.play.precision != tempi.record.precision) {
		/* if only play or record we can fix this */
		if (*apm_infop->apm_in_chs && *apm_infop->apm_out_chs == 0) {
			/* set play to capture sample rate */
			tempi.play.precision = tempi.record.precision;
		} else if (*apm_infop->apm_in_chs == 0 &&
		    *apm_infop->apm_out_chs) {
			/* set capture to play sample rate */
			tempi.record.precision = tempi.play.precision;
		} else {
			goto error;
		}
	} else {
		/* see audiotool bug description in set sample_rate above */
		if (*apm_infop->apm_out_chs != 0 &&
		    *apm_infop->apm_in_chs == 0) {
			/* set to the same precision, gads! */
			tempi.record.precision = tempi.play.precision;
		}
	}
	ATRACE_32("am_audio_set_info() PLAY precision set",
	    tempi.play.precision);
	ATRACE_32("am_audio_set_info() RECORD precision set",
	    tempi.record.precision);

	if (play && Modify(newi->play.encoding) &&
	    newi->play.encoding != curi->play.encoding) {
		if (type != AUDIO) {
			goto error;
		}
		tempi.play.encoding = newi->play.encoding;
	} else {
		tempi.play.encoding = curi->play.encoding;
	}
	if (record && Modify(newi->record.encoding) &&
	    newi->record.encoding != curi->record.encoding) {
		if (type != AUDIO) {
			goto error;
		}
		tempi.record.encoding = newi->record.encoding;
	} else {
		tempi.record.encoding = curi->record.encoding;
	}
	if (mode == AM_COMPAT_MODE &&
	    !(ad_infop->ad_diff_flags & AM_DIFF_ENC) &&
	    tempi.play.encoding != tempi.record.encoding) {
		/* if only play or record we can fix this */
		if (*apm_infop->apm_in_chs && *apm_infop->apm_out_chs == 0) {
			/* set play to capture sample rate */
			tempi.play.encoding = tempi.record.encoding;
		} else if (*apm_infop->apm_in_chs == 0 &&
		    *apm_infop->apm_out_chs) {
			/* set capture to play sample rate */
			tempi.record.encoding = tempi.play.encoding;
		} else {
			goto error;
		}
	} else {
		/* see audiotool bug description in set sample_rate above */
		if (*apm_infop->apm_out_chs != 0 &&
		    *apm_infop->apm_in_chs == 0) {
			/* set to the same encoding, gads! */
			tempi.record.encoding = tempi.play.encoding;
		}
	}
	ATRACE_32("am_audio_set_info() PLAY encoding set",
	    tempi.play.encoding);
	ATRACE_32("am_audio_set_info() RECORD encoding set",
	    tempi.record.encoding);

	/*
	 * In COMPAT mode or with multi-channel CODECs we check against
	 * what the hardware allows. Otherwise, we check against what the
	 * mixer can deal with. But only if an AUDIO channel.
	 */
	if (type == AUDIO) {
		if (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC) {
			if (am_ck_combinations(ad_infop->ad_play_comb,
			    tempi.play.encoding, tempi.play.precision,
			    B_FALSE) == AUDIO_FAILURE) {
				goto error;
			}
			if (am_ck_combinations(ad_infop->ad_rec_comb,
			    tempi.record.encoding, tempi.record.precision,
			    B_FALSE) == AUDIO_FAILURE) {
				goto error;
			}

		} else {	/* AM_MIXER_MODE */
			/* make sure the mixer can deal with the combinations */
			ASSERT(mode == AM_MIXER_MODE);

			switch ((int)tempi.play.channels) {
			case -1:		/* no change to channel */
			case AUDIO_CHANNELS_MONO:
			case AUDIO_CHANNELS_STEREO:
				break;
			default:
				goto error;
			}
			switch ((int)tempi.record.channels) {
			case -1:		/* no change to channel */
			case AUDIO_CHANNELS_MONO:
			case AUDIO_CHANNELS_STEREO:
				break;
			default:
				goto error;
			}

			switch ((int)tempi.play.encoding) {
			case -1:		/* no change to encoding */
			    break;
			case AUDIO_ENCODING_LINEAR:	/* signed */
			    /* we support 8 & 16-bit linear */
			    if (tempi.play.precision != AUDIO_PRECISION_16 &&
				tempi.play.precision != AUDIO_PRECISION_8) {
				    goto error;
			    }
			    break;
			case AUDIO_ENCODING_ULAW:
			case AUDIO_ENCODING_ALAW:
			    /* we only support 8-bit u-law/A-law */
			    if (tempi.play.precision != AUDIO_PRECISION_8) {
				    goto error;
			    }
			    break;
			default:
			    goto error;
			}
			switch ((int)tempi.record.encoding) {
			case -1:		/* no change to encoding */
			    break;
			case AUDIO_ENCODING_LINEAR:	/* signed */
			    /* we support 8 & 16-bit linear */
			    if (tempi.record.precision != AUDIO_PRECISION_16 &&
				tempi.record.precision != AUDIO_PRECISION_8) {
				    goto error;
			    }
			    break;
			case AUDIO_ENCODING_ULAW:
			case AUDIO_ENCODING_ALAW:
			    /* we only support 8-bit u-law/A-law */
			    if (tempi.record.precision != AUDIO_PRECISION_8) {
				    goto error;
			    }
			    break;
			default:
			    goto error;
			}
		}
	}
	ATRACE("am_audio_set_info() precision/encoding checked OK", &tempi);

	if (Modify(newi->play.gain)) {
		if (newi->play.gain > AUDIO_MAX_GAIN) {
			goto error;
		}
		tempi.play.gain = newi->play.gain;
	} else {
		tempi.play.gain = curi->play.gain;
	}
	if (Modify(newi->record.gain)) {
		if (newi->record.gain > AUDIO_MAX_GAIN) {
			goto error;
		}
		tempi.record.gain = newi->record.gain;
	} else {
		tempi.record.gain = curi->record.gain;
	}
	ATRACE_32("am_audio_set_info() PLAY gain set", tempi.play.gain);
	ATRACE_32("am_audio_set_info() RECORD gain set", tempi.record.gain);

	if (Modify(newi->play.port)) {
		tempi.play.port = newi->play.port;
	} else {
		tempi.play.port = hw_info->play.port;
	}
	if (tempi.play.port & ~hw_info->play.avail_ports) { /* legal port? */
		goto error;
	}
	/* always turn on un-modifyable ports */
	tempi.play.port |= hw_info->play.avail_ports & ~hw_info->play.mod_ports;
	if (ad_infop->ad_misc_flags & AM_MISC_PP_EXCL) { /* check exclusivity */
		if (am_ck_bits_set32(tempi.play.port) > 1) {
			goto error;
		}
	}
	if (Modify(newi->record.port)) {
		tempi.record.port = newi->record.port;
	} else {
		tempi.record.port = hw_info->record.port;
	}
	if (tempi.record.port & ~hw_info->record.avail_ports) {	/* legal ? */
		goto error;
	}
	/* always turn on un-modifyable ports */
	tempi.record.port |=
	    hw_info->record.avail_ports & ~hw_info->record.mod_ports;
	/* check exclusivity */
	if (ad_infop->ad_misc_flags & AM_MISC_RP_EXCL) {
		if (am_ck_bits_set32(tempi.record.port) > 1) {
			goto error;
		}
	}
	ATRACE_32("am_audio_set_info() PLAY ports set", tempi.play.port);
	ATRACE_32("am_audio_set_info() RECORD ports set", tempi.record.port);

	if (Modifyc(newi->play.balance)) {
		if (newi->play.balance > AUDIO_RIGHT_BALANCE) {
			goto error;
		}
		tempi.play.balance = newi->play.balance;
	} else {
		tempi.play.balance = curi->play.balance;
	}
	if (Modifyc(newi->record.balance)) {
		if (newi->record.balance > AUDIO_RIGHT_BALANCE) {
			goto error;
		}
		tempi.record.balance = newi->record.balance;
	} else {
		tempi.record.balance = curi->record.balance;
	}

	ATRACE_32("am_audio_set_info() PLAY balance set", tempi.play.balance);
	ATRACE_32("am_audio_set_info() REC balance set", tempi.record.balance);

	if (ad_infop->ad_defaults->hw_features & AUDIO_HWFEATURE_IN2OUT) {
		if (Modify(newi->monitor_gain)) {
			if (newi->monitor_gain > AUDIO_MAX_GAIN) {
				goto error;
			}
			tempi.monitor_gain = newi->monitor_gain;
		} else {
			tempi.monitor_gain = hw_info->monitor_gain;
		}
	} else {
		ATRACE("am_audio_set_info() monitor gain cannot be set", 0);
		goto error;
	}
	ATRACE_32("am_audio_set_info() monitor gain set", tempi.monitor_gain);

	if (Modifyc(newi->output_muted)) {
		tempi.output_muted = newi->output_muted;
	} else {
		tempi.output_muted = curi->output_muted;
	}
	ATRACE_32("am_audio_set_info() output muted set", tempi.output_muted);

	/*
	 * Now that we've got the new values verified we need to update the
	 * hardware. The following is updated:
	 *   COMPAT Mode, All Devices
	 *	play.minordev (H/W)		record.minordev (H/W)
	 *   COMPAT Mode, AUDIO Device
	 *	play.sample_rate (H/W)		record.sample_rate (H/W)
	 *	play.channels (H/W)		record.channels (H/W)
	 *	play.precision (H/W)		record.precision (H/W)
	 *	play.encoding (H/W)		record.encoding (H/W)
	 *	play.gain (H/W)			record.gain (H/W)
	 *	play.balance (H/W)		record.balance (H/W)
	 *	output_muted (H/W)
	 *   COMPAT Mode, AUDIOCTL Device
	 *	play.gain (H/W)			record.gain (H/W)
	 *	play.balance (H/W)		record.balance (H/W)
	 *	output_muted (H/W)
	 *   MIXER Mode, All Devices
	 *	play.minordev (CH)		record.minordev (CH)
	 *   MIXER Mode, AUDIO Device, Traditional CODEC
	 *	play.sample_rate (CH)		record.sample_rate (CH)
	 *	play.channels (CH)		record.channels (CH)
	 *	play.precision (CH)		record.precision (CH)
	 *	play.encoding (CH)		record.encoding (CH)
	 *	play.gain (CH)			record.gain (CH)
	 *	play.balance (CH)		record.balance (CH)
	 *	output_muted (CH)
	 *   MIXER Mode, AUDIOCTL Device, Traditional CODEC, Same Process As
	 *   An AUDIO Channel, ch_ctl == TRUE
	 *	play.gain (CH)			record.gain (CH)
	 *	play.balance (CH)		record.balance (CH)
	 *	output_muted (CH)
	 *   MIXER Mode, AUDIOCTL Device, Traditional CODEC, Different Proc.
	 *   From An AUDIO Channel, ch_ctl == FALSE
	 *	play.gain (H/W)			record.gain (H/W)
	 *	play.balance (H/W)		record.balance (H/W)
	 *	output_muted (H/W)
	 *   MIXER Mode, AUDIO Device, Multi-Channel CODEC
	 *	play.sample_rate (CH H/W)	record.sample_rate (CH H/W)
	 *	play.channels (CH H/W)		record.channels (CH H/W)
	 *	play.precision (CH H/W)		record.precision (CH H/W)
	 *	play.encoding (CH H/W)		record.encoding (CH H/W)
	 *	play.gain (CH H/W)		record.gain (CH H/W)
	 *	play.balance (CH H/W)		record.balance (CH H/W)
	 *	output_muted (CH H/W)
	 *   MIXER Mode, AUDIOCTL Device, Multi-Channel CODEC, Same Proc. As
	 *   An AUDIO Channel, ch_ctl == TRUE
	 *	play.gain (CH H/W)		record.gain (CH H/W)
	 *	play.balance (CH H/W)		record.balance (CH H/W)
	 *	output_muted (CH H/W)
	 *   MIXER Mode, AUDIOCTL Device, Multi-Channel CODEC, Different
	 *   Process From An AUDIO, ch_ctl == FALSE
	 *	play.gain (H/W)			record.gain (H/W)
	 *	play.balance (H/W)		record.balance (H/W)
	 *	output_muted (H/W)
	 *   All May Modify These Fields
	 *	play.port (H/W)			record.port (H/W)
	 *	monitor_gain (H/W)
	 *
	 * If we are in AM_COMPAT_MODE then output_muted controls the hardware,
	 * otherwise it just affects the channel, if it is a ch_ctl.
	 */

	/* only AUDIO channels can affect the data format */
	if (type == AUDIO) {
		int	old_p_sr = curi->play.sample_rate;
		int	old_r_sr = curi->record.sample_rate;

		/* figure out our "stream number" */
		if (codec_type == AM_MS_CODEC) {
			stream = chptr->ch_info.ch_number;
		} else {
			stream = AMAD_SET_CONFIG_BOARD;
		}

		if (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC) {
			/*
			 * We only set the format if there's been a change.
			 * Otherwise we risk introducing noise, pops, etc.,
			 * for little good reason.
			 */
			if (hw_info->play.sample_rate !=
				tempi.play.sample_rate ||
			    hw_info->play.channels !=
				tempi.play.channels ||
			    hw_info->play.precision !=
				tempi.play.precision ||
			    hw_info->play.encoding !=
				tempi.play.encoding) {
				if (am_set_format(ad_infop->ad_entry,
				    stpptr->flags, dev_instance, stream,
				    AUDIO_PLAY, tempi.play.sample_rate,
				    tempi.play.channels,
				    tempi.play.precision,
				    tempi.play.encoding) == AUDIO_FAILURE) {
					goto error;
				}
			}
			if (hw_info->record.sample_rate !=
				tempi.record.sample_rate ||
			    hw_info->record.channels !=
				tempi.record.channels ||
			    hw_info->record.precision !=
				tempi.record.precision ||
			    hw_info->record.encoding !=
				tempi.record.encoding) {
				if (am_set_format(ad_infop->ad_entry,
				    stpptr->flags, dev_instance, stream,
				    AUDIO_RECORD, tempi.record.sample_rate,
				    tempi.record.channels,
				    tempi.record.precision,
				    tempi.record.encoding) == AUDIO_FAILURE) {
					goto error;
				}
			}
		}
		if (mode == AM_MIXER_MODE) {
			curi->play.sample_rate = tempi.play.sample_rate;
			curi->play.channels = tempi.play.channels;
			curi->play.precision = tempi.play.precision;
			curi->play.encoding = tempi.play.encoding;
			curi->record.sample_rate = tempi.record.sample_rate;
			curi->record.channels = tempi.record.channels;
			curi->record.precision = tempi.record.precision;
			curi->record.encoding = tempi.record.encoding;
		} else {
			hw_info->play.sample_rate = tempi.play.sample_rate;
			hw_info->play.channels = tempi.play.channels;
			hw_info->play.precision = tempi.play.precision;
			hw_info->play.encoding = tempi.play.encoding;
			hw_info->record.sample_rate = tempi.record.sample_rate;
			hw_info->record.channels = tempi.record.channels;
			hw_info->record.precision = tempi.record.precision;
			hw_info->record.encoding = tempi.record.encoding;
		}

		/* see if we need to update the sample rate conv. routines */
		mutex_enter(&chpptr->src_lock);
		if (mode == AM_MIXER_MODE && codec_type == AM_TRAD_CODEC) {
			if (chpptr->writing &&
			    (tempi.play.sample_rate != old_p_sr ||
			    reti == NULL || chpptr->play_src_data == NULL)) {
				ATRACE("am_audio_set_info() PLAY, "
				    "calling src init", chpptr);
				if (ad_infop->ad_play.ad_conv->ad_src_init(
				    chptr, AUDIO_PLAY) == AUDIO_FAILURE) {
					mutex_exit(&chpptr->src_lock);
					ATRACE("am_audio_set_info() "
					    "play src_init() failed", 0);
					goto error;
				}
			}
			if (chpptr->reading &&
			    (tempi.record.sample_rate != old_r_sr ||
			    reti == NULL || chpptr->rec_src_data == NULL)) {
				ATRACE("am_audio_set_info() RECORD, "
				    "calling src init", chpptr);
				if (ad_infop->ad_record.ad_conv->ad_src_init(
				    chptr, AUDIO_RECORD) == AUDIO_FAILURE) {
					mutex_exit(&chpptr->src_lock);
					ATRACE("am_audio_set_info() "
					    "record src_init() failed", 0);
					goto error;
				}
			}
		}
		mutex_exit(&chpptr->src_lock);
	}

	/* is this an AUDIOCTL ch with the PID as another AUDIO ch? */
	ch_ctl = (chpptr->flags & AM_CHNL_CONTROL) ? B_TRUE : B_FALSE;

	/* re-figure out our "stream number" */
	if (mode == AM_COMPAT_MODE || (mode == AM_MIXER_MODE && !ch_ctl)) {
		stream = AMAD_SET_CONFIG_BOARD;
	} else {
		stream = chptr->ch_info.ch_number;
	}

	/*
	 * AUDIO and AUDIOCTL can affect gains, ports, etc. If in COMPAT
	 * mode or a MS Codec we affect hardware. Otherwise this is a
	 * virtual ch. and only that channel's parameters are affected,
	 * i.e., no hardware update. Also, if a MS Codec and an AUDIOCTL
	 * channel isn't associated with a particular stream then we don't
	 * muck with hardware either.
	 */
	if (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC ||
	    (type == AUDIOCTL && !ch_ctl)) {
		if (am_set_gain(apm_infop, tempi.play.channels,
		    (tempi.play.gain & 0x0ff), tempi.play.balance, AUDIO_PLAY,
		    dev_instance, stream) == AUDIO_FAILURE) {
			goto error;
		}
		if (am_set_gain(apm_infop, tempi.record.channels,
		    (tempi.record.gain & 0x0ff), tempi.record.balance,
		    AUDIO_RECORD, dev_instance, stream) == AUDIO_FAILURE) {
			goto error;
		}
		/* only if output_muted actually changed */
		if (hw_info->output_muted != tempi.output_muted) {
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_OUTPUT_MUTE, NULL,
			    tempi.output_muted, NULL) == AUDIO_FAILURE) {
				goto error;
			}
		}
	}
	if (mode == AM_MIXER_MODE) {
		curi->play.gain = tempi.play.gain;
		curi->play.balance = tempi.play.balance;
		curi->record.gain = tempi.record.gain;
		curi->record.balance = tempi.record.balance;
		curi->output_muted = tempi.output_muted;
		tempi.play.minordev = curi->play.minordev;
		tempi.record.minordev = curi->record.minordev;
		if (type == AUDIOCTL && !ch_ctl) {
			stpptr->save_hw_pgain = tempi.play.gain;
			stpptr->save_hw_pbal = tempi.play.balance;
			stpptr->save_hw_rgain = tempi.record.gain;
			stpptr->save_hw_rbal = tempi.record.balance;
			stpptr->save_hw_muted = tempi.output_muted;
		}
	} else {
		hw_info->play.gain = tempi.play.gain;
		hw_info->play.balance = tempi.play.balance;
		hw_info->record.gain = tempi.record.gain;
		hw_info->record.balance = tempi.record.balance;
		hw_info->output_muted = tempi.output_muted;
		tempi.play.minordev = hw_info->play.minordev;
		tempi.record.minordev = hw_info->record.minordev;
	}

	/* now we can set the ports and monitor gain, since all can set them */
	if (tempi.play.port != hw_info->play.port) {
		/* only if the play port actually changed */
		if (hw_info->play.port != tempi.play.port) {
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_PORT, AUDIO_PLAY,
			    tempi.play.port, NULL) == AUDIO_FAILURE) {
				goto error;
			}
		}
		hw_info->play.port = tempi.play.port;
	}
	if (tempi.record.port != hw_info->record.port) {
		/* only if the record port actually changed */
		if (hw_info->record.port != tempi.record.port) {
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_PORT, AUDIO_RECORD,
			    tempi.record.port, NULL) == AUDIO_FAILURE) {
				goto error;
			}
		}
		hw_info->record.port = tempi.record.port;
	}
	if (tempi.monitor_gain != hw_info->monitor_gain) {
		/* only if the monitor gain actually changed */
		if (hw_info->monitor_gain != tempi.monitor_gain) {
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_MONITOR_GAIN, NULL,
			    tempi.monitor_gain, NULL) == AUDIO_FAILURE) {
			goto error;
			}
		}
		hw_info->monitor_gain = tempi.monitor_gain;
	}

	/* we need to update the virtual channel, if we have one */
	if (mode == AM_MIXER_MODE) {
		curi->play.port = tempi.play.port;
		curi->record.port = tempi.record.port;
		curi->monitor_gain = tempi.monitor_gain;
	}

	/* now fix virtual channel parameters */
	if (Modify(newi->play.buffer_size)) {
		tempi.play.buffer_size = newi->play.buffer_size;
	} else {
		tempi.play.buffer_size = curi->play.buffer_size;
	}
	if (Modify(newi->record.buffer_size)) {
		tempi.record.buffer_size = newi->record.buffer_size;
	} else {
		tempi.record.buffer_size = curi->record.buffer_size;
	}
	ATRACE("am_audio_set_info() buffer size set", &tempi);

	if (Modify(newi->play.samples)) {
		tempi.play.samples = newi->play.samples;
		new_play_samples = B_TRUE;
	} else {
		tempi.play.samples = curi->play.samples;
	}
	if (Modify(newi->record.samples)) {
		tempi.record.samples = newi->record.samples;
		new_record_samples = B_TRUE;
	} else {
		tempi.record.samples = curi->record.samples;
	}
	ATRACE("am_audio_set_info() samples updated", &tempi);

	if (Modify(newi->play.eof)) {
		tempi.play.eof = newi->play.eof;
	} else {
		tempi.play.eof = curi->play.eof;
	}
	ATRACE("am_audio_set_info() eof updated", &tempi);

	if (Modifyc(newi->play.pause)) {
		tempi.play.pause = newi->play.pause;
	} else {
		tempi.play.pause = curi->play.pause;
	}
	if (Modifyc(newi->record.pause)) {
		tempi.record.pause = newi->record.pause;
	} else {
		tempi.record.pause = curi->record.pause;
	}
	/* if we unpaused we need to make sure we start up again */
	if (!tempi.play.pause && curi->play.pause) {
		start_play = B_TRUE;
	} else if (tempi.play.pause && !curi->play.pause &&
	    (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC)) {
		stop_play = B_TRUE;
	}
	if (!tempi.record.pause && curi->record.pause) {
		start_record = B_TRUE;
	} else if (tempi.record.pause && !curi->record.pause &&
	    (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC)) {
		stop_record = B_TRUE;
	}
	ATRACE("am_audio_set_info() pause set", &tempi);

	if (Modifyc(newi->play.error)) {
		tempi.play.error = newi->play.error;
	} else {
		tempi.play.error = curi->play.error;
	}
	if (Modifyc(newi->record.error)) {
		tempi.record.error = newi->record.error;
	} else {
		tempi.record.error = curi->record.error;
	}
	ATRACE("am_audio_set_info() error updated", &tempi);

	if (Modifyc(newi->play.waiting)) {
		tempi.play.waiting = newi->play.waiting;
	} else {
		tempi.play.waiting = curi->play.waiting;
	}
	if (Modifyc(newi->record.waiting)) {
		tempi.record.waiting = newi->record.waiting;
	} else {
		tempi.record.waiting = curi->record.waiting;
	}
	ATRACE("am_audio_set_info() waiting updated", &tempi);

	/*
	 * For MIXER mode we must update virtual channel parameters, because
	 * as soon as we restart the DMA engine(s) it's going to ask for audio.
	 * If the pause is still in effect then no data is going to be
	 * transferred.
	 */
	if (mode == AM_MIXER_MODE) {
		curi->play.buffer_size = tempi.play.buffer_size;
		curi->play.pause = tempi.play.pause;
		curi->play.eof = tempi.play.eof;
		curi->play.error = tempi.play.error;
		curi->play.waiting = tempi.play.waiting;
		curi->record.buffer_size = tempi.record.buffer_size;
		curi->record.pause = tempi.record.pause;
		curi->record.error = tempi.record.error;
		curi->record.waiting = tempi.record.waiting;
	}

	/* before we leave, we need to restart the DMA engines, or ... */
	if (start_play == B_TRUE) {
		/* make sure the play DMA engine is runnig */
		ASSERT(stop_play == B_FALSE);
		ATRACE("am_audio_set_info() start play", chptr);
		curi->play.pause = 0;	/* must be before the call */
		if (ad_infop->ad_entry->ad_start_play(dev_instance, stream) ==
		    AUDIO_SUCCESS) {
			curi->play.active = 1;
			hw_info->play.active = 1;
		} else {
			curi->play.active = 0;
			hw_info->play.active = 0;
			/* we don't change pause flag if failed to start */
		}
	} else if (stop_play == B_TRUE) {
		/* make sure the play DMA engine is paused */
		ATRACE("am_audio_set_info() pause play", chptr);
		ad_infop->ad_entry->ad_pause_play(dev_instance, stream);
		curi->play.active = 0;
		hw_info->play.active = 0;
		curi->play.pause = 1;
	}
	if (start_record == B_TRUE) {
		/* make sure the record DMA engine is runnnig */
		ASSERT(stop_record == B_FALSE);
		ATRACE("am_audio_set_info() start record", chptr);
		curi->record.pause = 0;	/* must be before the call */
		if (ad_infop->ad_entry->ad_start_record(dev_instance, stream) ==
		    AUDIO_SUCCESS) {
			curi->record.active = 1;
			hw_info->record.active = 1;
		} else {
			curi->record.active = 0;
			hw_info->record.active = 0;
			/* we don't change pause flag if failed to start */
		}
	} else if (stop_record == B_TRUE) {
		/* make sure the record DMA engine is stopped */
		ATRACE("am_audio_set_info() stop record", chptr);
		ad_infop->ad_entry->ad_stop_record(dev_instance, stream);
		curi->record.active = 0;
		hw_info->record.active = 0;
		curi->record.pause = 1;
	}

	/*
	 * For COMPAT mode we are dealing with the hardware, not a virtual
	 * channel. So the true state of the hardware can't be modified before
	 * starting or stopping the DMA engine(s).
	 */
	if (mode == AM_COMPAT_MODE) {
		hw_info->play.buffer_size = tempi.play.buffer_size;
		hw_info->play.pause = tempi.play.pause;
		hw_info->play.eof = tempi.play.eof;
		hw_info->play.error = tempi.play.error;
		hw_info->play.waiting = tempi.play.waiting;
		hw_info->record.buffer_size = tempi.record.buffer_size;
		hw_info->record.pause = tempi.record.pause;
		hw_info->record.error = tempi.record.error;
		hw_info->record.waiting = tempi.record.waiting;
	}

	/* everything passed so we can update the samples count */
	if (new_play_samples) {
		curi->play.samples = tempi.play.samples;
		chpptr->psamples_c = 0;
		chpptr->psamples_f = 0;
		chpptr->psamples_p = 0;
	}
	if (new_record_samples) {
		curi->record.samples = tempi.record.samples;
	}

	/*
	 * If we don't have a reti pointer we ignore the R/O members. If we
	 * need them we get them directly from the channel that is active.
	 * So if reti == NULL we are done. Otherwise copy tempi into the
	 * memory pointed to by reti and then copy over the R/O members.
	 *
	 * We pass the reserved members, just in case.
	 *	play._xxx[1]			record._xxx[1]
	 *	_xxx[1]
	 *	_xxx[2]
	 */
	if (reti != NULL) {
		ATRACE("am_audio_set_info() reti succeeded", chptr);

		/* do a quick copy, and then fill in the special fields */
		bcopy(curi, reti, sizeof (*curi));

		reti->play.avail_ports =	hw_info->play.avail_ports;
		reti->record.avail_ports =	hw_info->record.avail_ports;
		reti->play.mod_ports =		hw_info->play.mod_ports;
		reti->record.mod_ports =	hw_info->record.mod_ports;
		reti->record.eof =		0;
		reti->monitor_gain =		hw_info->monitor_gain;
		reti->output_muted =		hw_info->output_muted;
		reti->hw_features =		hw_info->hw_features;
		reti->sw_features =		hw_info->sw_features;
		reti->sw_features_enabled =	hw_info->sw_features_enabled;
	}

	ATRACE("am_audio_set_info() succeeded", chptr);

	/* we've modified the h/w, so send a signal, if we want it */
	am_send_signal(chptr->ch_statep);

	return (AUDIO_SUCCESS);

error:
	/* we may have modified the h/w, so send a signal, if we want it */
	am_send_signal(chptr->ch_statep);

	return (AUDIO_FAILURE);

}	/* am_audio_set_info() */

/*
 * am_ck_bits_set32()
 *
 * Description:
 *      This routine figures out how many bits are set in the passed in val.
 *
 * Arguments:
 *      uint    val             The argument to test
 *
 * Returns:
 *      0 - 32                  The number of bits set
 */
int
am_ck_bits_set32(uint_t val)
{
	uint_t		mask = 0x00000001u;
	int		count;
	int		i;

	ATRACE_32("in am_ck_bits_set32()", val);

	for (i = 0, count = 0; i < 32; i++) {
		if (mask & val) {
			count++;
		}
		mask <<= 1;
	}

	ATRACE_32("am_ck_bits_set32() done", count);

	return (count);

}	/* am_ck_bits_set32() */

/*
 * am_ck_channels()
 *
 * Description:
 *	This routine checks to see if the number of channels passed is one of
 *	the supported number of channels.
 *
 * Arguments:
 *	am_ad_ch_cap_t	*cptr	Pointer to the play/record capability struct
 *	uint_t		ch	Number of channels to check
 *
 * Returns:
 *	AUDIO_SCCESS		Valid number of channels
 *	AUDIO_FAILURE		Invalid number of channels
 */
static int
am_ck_channels(am_ad_ch_cap_t *cptr, uint_t ch)
{
	uint_t		*iptr = cptr->ad_chs;
	int		i;

	ATRACE("in am_ck_channels()", cptr);

	for (i = 0; *iptr != 0; i++, iptr++) {
		if (*iptr == ch) {
			ATRACE("am_ck_channels() succeeded", iptr);
			return (AUDIO_SUCCESS);
		}
	}

	ATRACE("am_ck_channels() failed", cptr->ad_chs);

	return (AUDIO_FAILURE);

}	/* am_ck_channels */

/*
 * am_ck_combinations()
 *
 * Description:
 *	This routine makes sure that the combination of encoding and
 *	precision are legal. If also compensates for devices that don't
 *	support u-law and A-law by allowing them because we can do the
 *	conversion in the mixer.
 *
 * Arguments:
 *	am_ad_cap_comb_t	*comb	Ptr to the play/rec legal combinations
 *	int			enc	The encoding to check
 *	int			prec	The precision to check
 *	boolean_t		hw	If B_TRUE report the true H/W capability
 *
 * Returns:
 *	AUDIO_SUCCESS		It is a legal combination or value
 *	AUDIO_FAILURE		It is not a legal combination or value
 */
static int
am_ck_combinations(am_ad_cap_comb_t *comb, int enc, int prec, boolean_t hw)
{
	am_ad_cap_comb_t	*ptr;
	boolean_t		ulaw = B_FALSE;
	boolean_t		alaw = B_FALSE;

	ATRACE("in am_ck_combinations()", comb);
	ATRACE("am_ck_combinations() enc", enc);
	ATRACE("am_ck_combinations() prec", prec);

	for (ptr = comb; ptr->ad_prec != 0; ptr++) {
		ATRACE("am_ck_combinations() enc", ptr->ad_enc);
		ATRACE("am_ck_combinations() prec", ptr->ad_prec);
		if (ptr->ad_prec == AUDIO_PRECISION_8) {
			if (ptr->ad_enc == AUDIO_ENCODING_ULAW) {
				ulaw = B_TRUE;
			} else if (ptr->ad_enc == AUDIO_ENCODING_ALAW) {
				alaw = B_TRUE;
			}
		}
		if (ptr->ad_prec == prec && ptr->ad_enc == enc) {
			ATRACE("am_ck_combinations() found a legal combination",
			    ptr);
			return (AUDIO_SUCCESS);
		}
	}
	ATRACE("am_ck_combinations() not in combination array", 0);

	/* if 8-bit we support u-law and a-law */
	if (hw == B_FALSE && prec == AUDIO_PRECISION_8 &&
	    ((enc == AUDIO_ENCODING_ULAW && !ulaw) ||
	    (enc == AUDIO_ENCODING_ALAW && !alaw))) {
		ATRACE("am_ck_combinations() faked the combination", 0);
		return (AUDIO_SUCCESS);
	}

	ATRACE("am_ck_combinations() no legal comb/value found", NULL);

	return (AUDIO_FAILURE);

}	/* am_ck_combinations() */

/*
 * am_ck_sample_rate()
 *
 * Description:
 *	This routine does two different things, depeneding on if sr == 0 or
 *	not.
 *
 *	If sr == NODEV then the list of supported sample rates is sanity
 *	checked. If it passes then the highest conversion rate is returned,
 *	otherwise AUDIO_FAILURE is returned. This list of sample rates MUST
 *	be in increasing size.
 *
 *	If sr != NODEV then the sample rate conversion information list is
 *	searched for the sample rate. If found AUDIO_SUCCESS is returned,
 *	otherwise AUDIO_FAILURE is returned.
 *
 *	NOTE: The sample rate conversion information is specific to the routines
 *		that perform the conversions. Therefore the audio mixer doesn't
 *		check them for sanity. It could be done by creating another
 *		call in am_ad_src_entry_t, but thorough testing is all that
 *		is really needed.
 *
 * Arguments:
 *	am_ad_ch_cap_t	*cptr	Pointer to the play/record capability struct
 *	int		mode	AM_MIXER_MODE or AM_COMPAT_MODE
 *	int		sr	Sample rate to check
 *
 * Returns:
 *	Sample Rate		Highest supported sample rate
 *	AUDIO_SUCCESS		Sample rate found
 *	AUDIO_FAILURE		Invalid sample rate/bad sample rate list
 */
static int
am_ck_sample_rate(am_ad_ch_cap_t *cptr, int mode, int sr)
{
	am_ad_sample_rates_t	*srs;
	uint_t			*ptr;
	uint_t			big = 0;
	int			i;

	ATRACE("in am_ck_sample_rate()", cptr);
	ATRACE_32("am_ck_sample_rate() mode", mode);

	if (mode == AM_MIXER_MODE) {
		srs = &cptr->ad_mixer_srs;
	} else {
		srs = &cptr->ad_compat_srs;
	}
	ptr = srs->ad_srs;

	if (sr != NODEV) {
		/* check the passed in sample rate against the list */
		if (srs->ad_limits & MIXER_SRS_FLAG_SR_LIMITS) {
			/*
			 * We only check the limits and becuse we've already
			 * done the sanity check in am_attach(). Therefore
			 * position 0 must be the bottom limit and postiion
			 * 1 must be the top limit.
			 */
			if (sr < ptr[0] || sr > ptr[1]) {
				ATRACE("am_ck_sample_rate() limit failed", srs);
				return (AUDIO_FAILURE);
			}

			ATRACE_32("am_ck_sample_rate() found in limit", sr);
			return (AUDIO_SUCCESS);
		}

		for (; *ptr != NULL; ptr++) {
			if (*ptr == sr) {
				ATRACE_32("am_ck_sample_rate() found", sr);
				return (AUDIO_SUCCESS);
			}
			if (*ptr > sr) {
				ATRACE_32("am_ck_sample_rate() past", sr);
				return (AUDIO_FAILURE);
			}
		}

		ATRACE("am_ck_sample_rate() failed", cptr->ad_sr_info);

		return (AUDIO_FAILURE);

	}

	/* do a sanity check on the list, it must be in increasing order */
	for (i = 0; *ptr != NULL; ptr++, i++) {
		if (*ptr > big) {
			big = *ptr;
		} else {
			ATRACE_32("am_ck_sample_rate() bad order, big", big);
			ATRACE_32("am_ck_sample_rate() *ptr", *ptr);
			return (AUDIO_FAILURE);
		}
	}

	/* if limits then there should be only two samples */
	if ((srs->ad_limits & MIXER_SRS_FLAG_SR_LIMITS) && i != 2) {
		ATRACE_32("am_ck_sample_rate() too many samples for limits", i);
		return (AUDIO_FAILURE);
	}

	ATRACE_32("am_ck_sample_rate() found highest sample rate", big);

	return (big);

}	/* am_ck_sample_rate */

/*
 * am_close()
 *
 * Description:
 *      Close a minor device, returing the minor number to the pool so that
 *      open(2) may use it again.
 *
 *	chpptr->flags is used to coordinate draining the write queue.
 *	am_close() sets flags to AM_CHNL_CLOSING. It then waits for the
 *	flags to have AM_CHNL_EMPTY set by am_get_audio() when all available
 *	data has been drained and played. If a signal interrupts the draining
 *	the queue is flushed and and the AM_CHNL_OPEN flag is cleared.
 *	am_get_audio() then ignores this channel until it is opened again.
 *
 *	NOTE: When ch_info.info is set and ref_cnt is changed either
 *		statep->as_lock is used, for the device info state
 *		structure, or chptr->ch_lock is used when a new info
 *		state structure is set.
 *
 *	NOTE: We need to behave differently for a normal close vs the user
 *		app calling exit(). Unfortunately there isn't a DDI compliant
 *		method for doing this, so we take a look at the current thread
 *		and see if it's exiting or not. This is how the old diaudio
 *		module did it.
 *
 * Arguments:
 *      queue_t		*q	Pointer to the read queue
 *      int		flag	File status flag
 *      cred_t		*credp	Pointer to the user's credential structure
 *
 * Returns:
 *      AUDIO_SUCCESS		Successfully closed the device
 *      errno			Error number for failed close
 */
/*ARGSUSED*/
int
am_close(queue_t *q, int flag,  cred_t *credp)
{
	audio_info_t		*info;
	audio_info_t		*tinfo;
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	audio_ch_t		*tchptr;
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	audio_info_t		*hw_info = apm_infop->apm_ad_state;
	am_apm_private_t	*stpptr = apm_infop->apm_private;
	am_ch_private_t		*chpptr = chptr->ch_private;
	am_ad_info_t		*ad_infop;
	audio_device_type_e	type = chptr->ch_info.dev_type;
	int			codec_type;
	int			dev_instance;
	int			i;
	int			max_chs;
	int			mode;
	int			was_reading = 0;
	int			was_writing = 0;

	ATRACE("in am_close()", chptr);
	ATRACE_32("am_close() channel number", chptr->ch_info.ch_number);

	ASSERT(q == chptr->ch_qptr);

	/* mark the channel as in the process of being closed */
	mutex_enter(&statep->as_lock);		/* freezes channel allocation */
	chpptr->flags |= AM_CHNL_CLOSING;
	mutex_exit(&statep->as_lock);

	/* get the device instance before the streams q is invalidated */
	dev_instance = audio_sup_get_dev_instance(NODEV, q);

	/*
	 * Is it ever possible to have an outstanding ioctl when a close
	 * happens? Most likely not. If we can then we need to deal with
	 * this.
	 */
	ASSERT(chpptr->ioctl_tmp == 0);

	ad_infop = chptr->ch_apm_infop->apm_ad_infop;
	codec_type = ad_infop->ad_codec_type;
	info = 	chptr->ch_info.info;
	mode = ad_infop->ad_mode;

	/* drain out the final data */
	mutex_enter(&chptr->ch_lock);

	/*
	 * Wait for queue to drain, unless we were signaled in AUDIO_DRAIN
	 * or the process is exiting (in which case we use the hack).
	 */
	ATRACE("am_close() checking to see if need to wait", chptr);

	if (chpptr->writing) {
		was_writing = 1;
		if (!(chpptr->flags & AM_CHNL_DRAIN_SIG) &&
		    info->play.active && !info->play.pause) {
			ATRACE("am_close() need to wait", chptr);
			while (!(chpptr->flags & AM_CHNL_EMPTY) &&
			    !(curthread->t_proc_flag & TP_LWPEXIT)) {
				ATRACE_32("am_close() not empty",
				    chpptr->flags);

				if (cv_wait_sig(&chptr->ch_cv,
				    &chptr->ch_lock) <= 0) {
					ATRACE("am_close() signal wakeup",
					    chptr);
					break;
				}
				ATRACE_32("am_close() normal wakeup",
				    chpptr->flags);
			}
			ATRACE_32("am_close() empty", chpptr->flags);

			/* clear the writing flag, for mode switching */
			chpptr->writing = 0;
		}
	}

	/*
	 * Shutdown recording, if the channel was opened this way. We do it
	 * AFTER we shut down playing because it could take a long time for
	 * for playing to shut down and we don't want to stop recording too
	 * soon.
	 */
	if (chpptr->reading) {
		ASSERT(type == AUDIO);
		/*
		 * We shutdown the device if in COMPAT mode or for multi-stream
		 * Codecs, or if this is the last recording stream.
		 */
		if (mode == AM_COMPAT_MODE || codec_type == AM_MS_CODEC ||
		    *apm_infop->apm_in_chs == 1) {
			ATRACE("am_close() stopping record", statep);
			mutex_exit(&chptr->ch_lock);	/* stop regets lock */
			ad_infop->ad_entry->ad_stop_record(dev_instance,
			    chptr->ch_info.ch_number);
			mutex_enter(&chptr->ch_lock);	/* reget the lock */
			info->record.active = 0;
			hw_info->record.active = 0;
			info->record.pause = 0;
		}

		/* send any recorded data that may still be hanging around */
		if (chpptr->rec_mp) {
			info->record.samples += (chpptr->rec_mp->b_wptr -
			    chpptr->rec_mp->b_rptr) / info->record.channels /
			    (info->record.precision >> AUDIO_PRECISION_SHIFT);
			(void) putq(RD(q), chpptr->rec_mp);
			chpptr->rec_mp = NULL;
		}

		/* clear the reading flag, for mode switching */
		was_reading = 1;	/* we need to know later */
		chpptr->reading = 0;
	}

	/* save the gain and balance for the next open */
	if (info != apm_infop->apm_ad_state) {
		if (was_writing && Modify(info->play.gain)) {
			stpptr->save_pgain = info->play.gain;
		}
		if (was_writing && Modifyc(info->play.balance)) {
			stpptr->save_pbal = info->play.balance;
		}
		if (was_reading && Modify(info->record.gain)) {
			stpptr->save_rgain = info->record.gain;
		}
		if (was_reading && Modifyc(info->record.balance)) {
			stpptr->save_rbal = info->record.balance;
		}
	}

	/* clear flags, but only in COMPAT mode, where mem is just freed */
	if (mode == AM_COMPAT_MODE) {
		if (was_reading) {
			info->record.open = 0;
			info->record.waiting = 0;
		}
		if (was_writing) {
			info->play.open = 0;
			info->play.waiting = 0;
		}
	}

	mutex_exit(&chptr->ch_lock);

	/* protect the flags from AUDIO_MIXER_GET/SET_CHINFO ioctls */
	mutex_enter(&statep->as_lock);

	/* mark the channel closed so we can shut down the STREAMS queue */
	chpptr->flags &= ~AM_CHNL_OPEN;

	mutex_exit(&statep->as_lock);

	ATRACE("am_close() flushing q", chptr);
	flushq(q, FLUSHALL);

	/* unschedule the queue */
	ATRACE("am_close() qprocsoff()", chptr);
	qprocsoff(q);

	/* we are modifying mixer global data, so lock the mixer */
	mutex_enter(apm_infop->apm_swlock);

	ASSERT(*apm_infop->apm_in_chs >= 0);
	ASSERT(*apm_infop->apm_out_chs >= 0);

	/* only on AUDIO chs can this be true */
	if (was_reading) {
		ASSERT(type == AUDIO);
		if (*apm_infop->apm_in_chs == 1) {
			/* turn off capture */
			info->record.active = 0;
		}

		info->record.samples = 0;
		info->record.error = 0;
	}
	/* only on AUDIO chs can this be true */
	if (was_writing) {
		ASSERT(type == AUDIO);

		/* don't need to be locked when we stop */
		mutex_exit(apm_infop->apm_swlock);

		/* make sure a paused DMA engine is cleared */
		if (info->play.pause &&
		    (mode == AM_COMPAT_MODE || (codec_type == AM_MS_CODEC))) {
			ad_infop->ad_entry->ad_stop_play(dev_instance,
			    chptr->ch_info.ch_number);
		}
		info->play.active = 0;
		info->play.pause = 0;
		info->play.samples = 0;
		info->play.eof = 0;
		info->play.error = 0;
	} else {
		mutex_exit(apm_infop->apm_swlock);
	}

	/* free the sample rate conversion routine buffers and info struct */
	if (type == AUDIO) {
		/* first, take care of the sample rate conv. buffers */
		if (codec_type == AM_TRAD_CODEC) {
			(void) ad_infop->ad_play.ad_conv->ad_src_exit(chptr,
			    AUDIO_PLAY);
			(void) ad_infop->ad_record.ad_conv->ad_src_exit(chptr,
			    AUDIO_RECORD);
		}

		/* look for AUDIOCTL chs that are assoc. with ch & fix */
		max_chs = statep->as_max_chs;
		mutex_enter(&statep->as_lock);		/* keep chs static */
		for (i = 0, tchptr = &statep->as_channels[0];
		    i < max_chs; i++, tchptr++) {
			/* skip the same and unallocated channels */
			if (tchptr == chptr ||
			    !(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED)) {
				continue;
			}

			/* skip if not AUDIOCTL or different PIDs */
			if (tchptr->ch_info.dev_type != AUDIOCTL ||
			    tchptr->ch_info.pid != chptr->ch_info.pid) {
				continue;
			}

			ATRACE("am_close() setting AUDIOCTL info to hw",
			    &stpptr->hw_info);

			/* this is the same PID, so fix info pointer */
			mutex_enter(&tchptr->ch_lock);
			tchptr->ch_info.info = &stpptr->hw_info;
			stpptr->hw_info.ref_cnt++;
			mutex_exit(&tchptr->ch_lock);

			/*
			 * Decrement our channel's count, but we don't have to
			 * lock the info structure because channel allocation
			 * and freeing are frozen by as_lock, so we should be
			 * the only thread running that can modify the count.
			 */
			info->ref_cnt--;
			ATRACE_32(
			    "am_close() ref_cnt decremented", info->ref_cnt);
		}
		mutex_exit(&statep->as_lock);
	}

	/*
	 * We need to free the info structure. However, the play and record
	 * interrupt service routines will be looking at the channels as well.
	 * When they do they'll have the channel locked. So we lock the channel
	 * and then NULL the info pointer. That way the ISRs won't see the
	 * channel as active. As soon as we NULL the info pointer we unlock
	 * the channel so the ISR doesn't get blocked for long.
	 */
	tinfo = chptr->ch_info.info;
	mutex_enter(&chptr->ch_lock);
	chptr->ch_info.info = NULL;
	mutex_exit(&chptr->ch_lock);

	/* take care of references to the audio state structure */
	if (info->ref_cnt == 1) {
		/*
		 * Need to free the buffer. We don't need to lock because
		 * only this thread can now be using this channel.
		 */
		if (info != apm_infop->apm_ad_state) {
			kmem_free(tinfo, sizeof (audio_info_t));
#ifdef DEBUG
		} else {
			cmn_err(CE_WARN, "am_close() didn't free info struct");
#endif
		}
	} else {
		/* we need to lock the state before we decrement the count */
		mutex_enter(&statep->as_lock);
		info->ref_cnt--;
		ASSERT(info->ref_cnt >= 1);
		mutex_exit(&statep->as_lock);
	}

	/* tell the Audio Driver to free up any channel config info */
	if (type == AUDIO && ad_infop->ad_entry->ad_teardown) {
		ATRACE("am_close() calling ad_teardown()", 0);
		ad_infop->ad_entry->ad_teardown(dev_instance,
		    chptr->ch_info.ch_number);
		ATRACE("am_close() ad_teardown() returned", 0);
#ifdef DEBUG
	} else {
		ATRACE("am_close() didn't call ad_teardown()", 0);
#endif
	}

	/* free all the buffers */
	if (chpptr->play_samp_buf) {
		kmem_free(chpptr->play_samp_buf, chpptr->psb_size);
		chpptr->play_samp_buf = 0;
		chpptr->psb_size = 0;
	}
	if (chpptr->ch_pptr1) {
		kmem_free(chpptr->ch_pptr1, chpptr->ch_psize1);
		chpptr->ch_pptr1 = NULL;
		chpptr->ch_psize1 = 0;
	}
	if (chpptr->ch_pptr2) {
		kmem_free(chpptr->ch_pptr2, chpptr->ch_psize2);
		chpptr->ch_pptr2 = NULL;
		chpptr->ch_psize2 = 0;
	}
	if (chpptr->ch_rptr1) {
		kmem_free(chpptr->ch_rptr1, chpptr->ch_rsize1);
		chpptr->ch_rptr1 = NULL;
		chpptr->ch_rsize1 = 0;
	}
	if (chpptr->ch_rptr2) {
		kmem_free(chpptr->ch_rptr2, chpptr->ch_rsize2);
		chpptr->ch_rptr2 = NULL;
		chpptr->ch_rsize2 = 0;
	}
	/* free the private channel data structure */
	if (chpptr->rec_mp) {
		freemsg(chpptr->rec_mp);
	}

	/*
	 * Send the close signal. CAUTION: This must be called before
	 * the channel private data structure is freed.
	 */
	am_send_signal(statep);

	/* need to destroy the mutex */
	mutex_destroy(&chpptr->src_lock);

	kmem_free(chpptr, sizeof (*chpptr));
	chptr->ch_private = 0;

	/*
	 * Wait to the last minute to flush the messages, just in case one
	 * arrives while closing. If we don't then audiosup blows an ASSERT().
	 */
	if (type == AUDIO) {
		ATRACE("am_close() flushing messages", chptr);
		audio_sup_flush_msgs(chptr);
	} else {
		chptr->ch_msg_cnt = 0;	/* make sure */
	}

	ATRACE("am_close() calling audio_free_ch()", chptr);
	if (audio_sup_free_ch(chptr) == AUDIO_FAILURE) {
		/* not much we can do if this fails */
		ATRACE("am_close() audio_sup_free_ch() failed", chptr);
		cmn_err(CE_NOTE, "mixer: close() audio_sup_free_ch() error");
	}

	ATRACE("am_close() successful", 0);
	return (AUDIO_SUCCESS);

}	/* am_close() */

/*
 * am_convert()
 *
 * Description:
 *	This routine takes the source buffer, which is 32-bit integers,
 *	and converts it to whatever format the destination buffer is.
 *	While the source buffer is a 32-bit buffer, the data is really
 *	16-bits. We use 32-bits to make sure we can sum audio streams
 *	without loosing bits. When this routine is called we clip and
 *	then convert the data.
 *
 *	The supported conversions are:
 *		32-bit signed linear	->	16-bit clipped signed linear
 *		16-bit clipped linear	->	8-bit u-law
 *		16-bit clipped linear	->	8-bit A-law
 *		16-bit clipped linear	->	8-bit signed linear
 *
 * Arguments:
 *	int		samples		The number of samples to convert
 *	int		*src		Ptr to the src buffer, data to convert
 *	void		*dest		Ptr to the dest buffer, for converted
 *					data
 *	audio_prinfo_t	*prinfo		Personality module data structure
 *
 * Returns:
 *	none
 */
static void
am_convert(int samples, int *src, void *dest, audio_prinfo_t *prinfo)
{
	uint_t		encoding = prinfo->encoding;
	uint_t		precision = prinfo->precision;
	int		val;

	ATRACE_32("in am_convert()", samples);

	/* this should have been stopped earlier */
	ASSERT(precision != AUDIO_PRECISION_16 ||
	    encoding == AUDIO_ENCODING_LINEAR);

	/* make sure we have work to do */
	if (samples == 0) {
		ATRACE("am_convert() no samples to convert, returning", 0);
		return;
	}

	ATRACE_32("am_convert() NON-Linearize Audio", samples);

	if (precision == AUDIO_PRECISION_16) {
		int16_t	*dptr = (int16_t *)dest;

		ASSERT(encoding == AUDIO_ENCODING_LINEAR);

		for (; samples--; ) {
			val = *src++;
			if (val > INT16_MAX) {
				*dptr++ = INT16_MAX;
			} else if (val < INT16_MIN) {
				*dptr++ = INT16_MIN;
			} else {
				*dptr++ = (int16_t)val;
			}
		}
		ATRACE("am_convert() 16-bit linear done", 0);
	} else {	/* end 16-bit, begine 8-bit */
		uint8_t		*dptr = (uint8_t *)dest;

		ASSERT(precision == AUDIO_PRECISION_8);

		if (encoding == AUDIO_ENCODING_LINEAR) {
			ATRACE_32("am_convert() 8-bit signed linear", samples);

			for (; samples--; ) {
				val = *src++;
				if (val > INT16_MAX) {
					val = INT16_MAX;
				} else if (val < INT16_MIN) {
					val = INT16_MIN;
				}
				*dptr++ = (uint8_t)(val >> AM_256_SHIFT);
			}
		} else {	/* 8-bit U/A-Law */
			uint8_t		*cptr;
			int		shift;

			if (encoding == AUDIO_ENCODING_ULAW) {
				ATRACE("am_convert() 8-bit U-Law", 0);
				cptr = &_14linear2ulaw8[G711_ULAW_MIDPOINT];
				shift = 2;
			} else {
				ATRACE("am_convert() 8-bit A-Law", 0);
				ASSERT(encoding == AUDIO_ENCODING_ALAW);
				cptr = &_13linear2alaw8[G711_ALAW_MIDPOINT];
				shift = 3;
			}

			ATRACE_32("am_convert() 8-bit U/A-Law", encoding);

			for (; samples--; ) {
				val = *src++;
				if (val > INT16_MAX) {
					val = INT16_MAX;
				} else if (val < INT16_MIN) {
					val = INT16_MIN;
				}
				*dptr++ = (uint8_t)cptr[val >> shift];
			}
		}
		ATRACE("am_convert() 8-bit done", 0);
	}

	ATRACE_32("am_convert() done", encoding);

}	/* am_convert() */

/*
 * am_convert_to_int()
 *
 * Description:
 *	Convert a buffer of various precisions and encodings into a buffer of
 *	32-bit linear PCM samples.
 *
 *	CAUTION: The calling routine must ensure that the outbuf is large
 *		enough for the data, or we'll panic.
 *
 * Arguments:
 *	void		*inbuf		Input data buffer
 *	int		*outbuf		Output data buffer
 *	size_t		size		The number of samples to convert
 *	int		precision	The precision of the input buffer.
 *	int		encoding	The encoding of the input buffer.
 *
 * Returns:
 * 	void
 */
static void
am_convert_to_int(void *inbuf, int *outbuf, size_t size, int precision,
	int encoding)
{
	int		i;

	ATRACE("in am_convert_to_int()", inbuf);

	if (precision == AUDIO_PRECISION_16) {	/* do the easy case first */
		int16_t		*src = (int16_t *)inbuf;

		ASSERT(encoding == AUDIO_ENCODING_LINEAR);

		ATRACE_32("am_convert_to_int() 16-Bit", size);

		for (i = size; i--; ) {
			*outbuf++ = (int)*src++;
		}
	} else {		/* now the hard case, 8-bit */
		int16_t		*aptr;
		int8_t		*src = (int8_t *)inbuf;

		ASSERT(precision == AUDIO_PRECISION_8);

		if (encoding == AUDIO_ENCODING_ULAW) {
			aptr = _8ulaw2linear16;
			ATRACE("am_convert_to_int() 8-bit u-law", aptr);

			/* copy the data into the buf. ch_pptr1, char -> int */
			for (i = size; i--; ) {
				/* the conversion array does the scaling */
				*outbuf++ = (int)aptr[(unsigned char)*src++];
			}
		} else if (encoding == AUDIO_ENCODING_ALAW) {
			aptr = _8alaw2linear16;
			ATRACE("am_convert_to_int() 8-bit A-law", aptr);

			/* copy the data into the buf. ch_pptr1, char -> int */
			for (i = size; i--; ) {
				/* the conversion array does the scaling */
				*outbuf++ = (int)aptr[(unsigned char)*src++];
			}
		} else {
			ASSERT(encoding == AUDIO_ENCODING_LINEAR);
			/* copy the data into the buf. ch_pptr1, char -> int */
			for (i = size; i--; ) {
				/* the conversion array does the scaling */
				*outbuf++ = (int)(*src++ << AM_256_SHIFT);
			}
		}
	}

	ATRACE("am_convert_to_int() done", outbuf);

}	/* am_convert_to_int() */

/*
 * am_fix_info()
 *
 * Description:
 *	When in mixer mode we usually play at a different sample rate
 *	than the data stream from the application. Therefore the sample
 *	count from the Codec is meaningless. This routine adjusts for the
 *	difference in sample rates.
 *
 *	We only adjust the play sample count because when recording you send
 *	x samples so you always know how many samples you sent so you don't
 *	have to adjust.
 *
 *	If this is an AUDIOCTL channel and it is associated with the H/W
 *	we don't do anything.
 *
 *	We also fix port and pause info, as well as other H/W related info,
 *	depending on the mixer mode.
 *
 * Arguments:
 *	audio_ch_t		*chptr	Ptr to the channel's state structure
 *	audio_info_t		*info	Ptr to the info structure to update
 *	am_ad_info_t		*ad_infop Ptr to the channels device info struct
 *	am_ch_private_t		*chpptr	Ptr to the channel's private data
 *
 * Returns:
 *	none
 */
static void
am_fix_info(audio_ch_t *chptr, audio_info_t *info,
	am_ad_info_t *ad_infop, am_ch_private_t *chpptr)
{
	audio_device_type_e	type = chptr->ch_info.dev_type;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	audio_info_t		*hw_info = apm_infop->apm_ad_state;
	int			mode = ad_infop->ad_mode;

	ATRACE("in am_fix_info()", chptr);

	/* first, fix samples, if we need to */
	if (!chpptr->writing || (type == AUDIOCTL &&
	    hw_info == chptr->ch_info.info)) {

		ATRACE_32("am_fix_info() not writing, returning",
		    chpptr->writing);

		return;
	} else {
		if (mode == AM_MIXER_MODE &&
		    ad_infop->ad_codec_type == AM_TRAD_CODEC) {
			ATRACE("am_fix_info() sample conversion", 0);

			mutex_enter(&chpptr->src_lock);
			info->play.samples +=
			    ad_infop->ad_play.ad_conv->ad_src_adjust(chptr,
			    AUDIO_PLAY, chpptr->psamples_p) /
			    hw_info->play.channels;
			mutex_exit(&chpptr->src_lock);
		} else {
			ATRACE("am_fix_info() NO sample conversion",
			    info->play.samples);

			info->play.samples += chpptr->psamples_p /
			    info->play.channels;
		}
	}

	/* now fix various other things */
	if (mode == AM_MIXER_MODE) {
		ATRACE("am_fix_info() fixing other things", 0);
		if (info->play.pause) {
			info->play.active = 0;
		}
		info->play.port = hw_info->play.port;
		if (info->record.pause) {
			info->record.active = 0;
		}
		info->record.port = hw_info->record.port;
		info->monitor_gain = hw_info->monitor_gain;
	}

	ATRACE("am_fix_info() done", info);

}	/* am_adjust_sample_cnt() */

/*
 * am_get_audio_multi()
 *
 * Description:
 *	This routine is used by multi-channel CODECs to get a single stream
 *	of audio data for an individual channel. The data is raw and
 *	unprocessed.
 *
 * Arguments:
 *	audio_state_t	*statep		Pointer to the device instance's state
 *	void		*buf		The buffer to place the audio into
 *	int		channel		The device channel number.
 *	int		samples		The number of samples to get
 *
 *	NOTE: The variable "samples" is the number of samples the hardware
 *		wants. So it is samples at the hardware's sample rate.
 *
 * Returns:
 *	>= 0			The number of samples transferred to the buffer
 *	AUDIO_FAILURE		An error has occurred
 */
static int
am_get_audio_multi(audio_state_t *statep, void *buf, int channel, int samples)
{
	audio_ch_t		*chptr = &statep->as_channels[channel];
	am_ch_private_t		*chpptr;
	audio_info_t		*info;
	int			ret_val;

	ATRACE("in am_get_audio_multi()", statep);

	ASSERT(chptr->ch_info.ch_number == channel);

	/* lock the channel before we check it out */
	mutex_enter(&chptr->ch_lock);

	/*
	 * The channel may have been closed while we waited on the mutex.
	 * So once we get it we make sure the channel is still valid.
	 */
	chpptr = chptr->ch_private;
	info = chptr->ch_info.info;
	if (chpptr == NULL || info == NULL ||
	    (chpptr->flags & AM_CHNL_OPEN) == 0) {
		mutex_exit(&chptr->ch_lock);
		ATRACE("am_get_audio_multi() channel closed", chptr);
		return (AUDIO_FAILURE);
	}

	/* make sure it is an AUDIO channel */
	if (chptr->ch_info.dev_type != AUDIO) {
		mutex_exit(&chptr->ch_lock);
		ATRACE_32("am_get_audio_multi() not an AUDIO channel",
		    chptr->ch_info.dev_type);
		cmn_err(CE_NOTE, "mixer: get_audio_m() bad channel type: %d",
		    chptr->ch_info.dev_type);
		return (AUDIO_FAILURE);
	}

	/* skip if the channel is paused */
	if (info->play.pause) {
		mutex_exit(&chptr->ch_lock);
		ATRACE("am_get_audio_multi() channel paused", statep);
		return (0);
	}

	/* get "samples" worth of data */
	ATRACE("am_get_audio_multi() calling am_get_samples()", chptr);
	ret_val = am_get_samples(chptr, samples, buf, AM_COMPAT_MODE);
	if (ret_val == AUDIO_FAILURE || ret_val == 0) {
		mutex_exit(&chptr->ch_lock);
		ATRACE_32("am_get_audio_multi() am_get_samples() failed",
		    ret_val);
		return (ret_val);
	}

	ATRACE_32("am_get_audio_multi() am_get_samples() succeeded", ret_val);

	mutex_exit(&chptr->ch_lock);

	ATRACE("am_get_audio_multi() done", buf);

	return (ret_val);

}	/* am_get_audio_multi() */

/*
 * am_get_audio_trad()
 *
 * Description:
 *	This routine is used by traditional CODECs to get multiple streams
 *	of audio data and mixing them down into one channel for the CODEC.
 *
 *	In MIXER mode we use play_samp_buf to get audio samples. These samples
 *	are then mixed into the mix buffer. When all playing audio channels
 *	are mixed we convert to the proper output format, along with applying
 *	gain and balance, if needed.
 *
 *	In COMPAT mode the audio samples are placed directly into the buffer
 *	provided by the Audio Driver. Once one playing channel is found
 *	the search ends, no reason to waste more time.
 *
 *	CAUTION: This routine is called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 * Arguments:
 *	audio_state_t		*statep		Ptr to the dev instances's state
 *	void			*buf		The buf to place the audio into
 *	int			samples		The number of samples to get
 *	audio_apm_info_t	*apm_infop	Personality module data struct
 *
 *	NOTE: The variable "samples" is the number of samples the hardware
 *		wants. So it is samples at the hardware's sample rate.
 *
 * Returns:
 *	>= 0			The number of samples transferred to the buffer
 *	AUDIO_FAILURE		An error has occurred
 */
static int
am_get_audio_trad(audio_state_t *statep, void *buf, int samples,
	audio_apm_info_t *apm_infop)
{
	audio_ch_t		*chptr;
	audio_info_t		*info;
	am_apm_private_t	*stpptr = apm_infop->apm_private;
	audio_info_t		*hw_info = &stpptr->hw_info;
	am_ch_private_t		*chpptr;
	int			*mix_dest;
	int			*mix_src;
	size_t			size = samples << AM_INT32_SHIFT;
	int			balance;
	int			i;
	int			l_gain;
	int			r_gain;
	int			max_chs;
	int			max_ret_val = 0;
	int			ret_val;

	ATRACE("in am_get_audio_trad()", statep);

	ASSERT((samples << AM_INT32_SHIFT) <= mixer_bufsize);

	/* get the number of chs for this instance */
	max_chs = statep->as_max_chs;

	hw_info = apm_infop->apm_ad_state;

	/* we behave differently for MIXER vs. COMPAT mode */
	if (((am_ad_info_t *)apm_infop->apm_ad_infop)->ad_mode ==
	    AM_MIXER_MODE) {
		/* make sure the mix buffer is large enough */
		if (stpptr->mix_size < size) {
		    /* mix buffer too small, adjust sample request */
		    ATRACE_32("am_get_audio_trad(M) mix buffer too small",
			stpptr->mix_size);
		    ATRACE_32("am_get_audio_trad(M) adjust num samples from",
			samples);
		    samples = stpptr->mix_size >> AM_INT32_SHIFT;
		    ATRACE_32("am_get_audio_trad(M) num samples now set to",
			samples);
		}

		/* zero the mix buffer */
		bzero(stpptr->mix_buf, stpptr->mix_size);

		/* go through the chs, looking for the first AUDIO ch */
		for (i = 0, chptr = &statep->as_channels[0];
		    i < max_chs; i++, chptr++) {

		    /* lock the channel before we check it out */
		    mutex_enter(&chptr->ch_lock);

		    /* skip non-AUDIO and unallocated channels */
		    if (!(chptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
			chptr->ch_info.dev_type != AUDIO ||
			chptr->ch_info.pid == 0) {

			mutex_exit(&chptr->ch_lock);
			continue;
		    }
			/*
			 * The channel may have been closed while we waited
			 * on the mutex. So once we get it we make sure the
			 * channel is still valid.
			 */
		    chpptr = chptr->ch_private;
		    info = chptr->ch_info.info;
		    if (chpptr == NULL || info == NULL ||
			(chpptr->flags & AM_CHNL_OPEN) == 0) {
			    mutex_exit(&chptr->ch_lock);
			    ATRACE("am_get_audio_trad(M) channel closed",
				chptr);
			    continue;
		    }

		    /* skip non-playing AUDIO channels */
		    if (!chpptr->writing) {
			ATRACE("am_get_audio_trad(M) not playing", chpptr);
			mutex_exit(&chptr->ch_lock);
			continue;
		    }

		    /* skip paused AUDIO channels */
		    if (info->play.pause) {
			mutex_exit(&chptr->ch_lock);
			continue;
		    }

		    ATRACE_32("am_get_audio_trad(M) found channel", i);

		    info = chptr->ch_info.info;

		    /* make sure the buffer is big enough */
		    if (chpptr->play_samp_buf == NULL ||
			chpptr->psb_size < size) {

			ATRACE_32("am_get_audio_trad(M) freeing buffer",
			    chpptr->psb_size);
			if (chpptr->play_samp_buf) {
			    /* free the old buffer */
			    kmem_free(chpptr->play_samp_buf, chpptr->psb_size);
			}
			chpptr->play_samp_buf = kmem_alloc(size, KM_NOSLEEP);
			if (chpptr->play_samp_buf == NULL) {
			    ATRACE_32("am_get_audio_trad(M) kmem_alloc() "
				"play_samp_buf failed", i);
			    cmn_err(CE_WARN, "mixer: am_get_audio_trad(M) "
				" sample buffer %d not allocated", i);
			    chpptr->psb_size = 0;
			    continue;
			}
			chpptr->psb_size = size;
		    }

		    /* get "samples" worth of audio */
		    ATRACE_32("am_get_audio_trad(M) calling am_get_samples()",
			samples);
		    ret_val = am_get_samples(chptr, samples,
			chpptr->play_samp_buf,
			((am_ad_info_t *)apm_infop->apm_ad_infop)->ad_mode);

		    /* now we can see how well the am_get_samples() call did */
		    if (ret_val == AUDIO_FAILURE || ret_val == 0) {
			mutex_exit(&chptr->ch_lock);
			ATRACE_32(
			    "am_get_audio_trad(M) am_get_samples() failed",
			    ret_val);
			continue;
		    }

		    ATRACE_32("am_get_audio_trad(M) am_get_samples() succeeded",
			ret_val);

		    /* we can free the lock now */
		    mutex_exit(&chptr->ch_lock);

		    /* we return the maximum # of samples found & processed */
		    if (ret_val > max_ret_val) {
			/* update to a new value */
			max_ret_val = ret_val;
			ATRACE_32("am_get_audio_trad(M) updated max_ret_val",
			    max_ret_val);
		    }

		    /* mix this channel into the mix buffer */
		    mix_src = chpptr->play_samp_buf;
		    mix_dest = stpptr->mix_buf;
		    ATRACE("am_get_audio_trad(M) mix_src before", mix_src);
		    ATRACE("am_get_audio_trad(M) mix_dest before", mix_dest);
		    /* apply gain and balance while summing */
		    if (hw_info->play.channels == AUDIO_CHANNELS_MONO) {
			l_gain = info->play.gain;

			for (; ret_val; ret_val--) {
			    *mix_dest++ +=
				(*mix_src++ * l_gain) >> AM_MAX_GAIN_SHIFT;
			}
		    } else {
			ASSERT(hw_info->play.channels == AUDIO_CHANNELS_STEREO);

			l_gain = r_gain = info->play.gain;
			balance = info->play.balance;

			if (balance < AUDIO_MID_BALANCE) {
			    /* leave l gain alone and scale down r gain */
			    r_gain = (r_gain * balance) >> 5;
			} else if (balance > AUDIO_MID_BALANCE) {
			    /* leave r gain alone and scale down l gain */
			    l_gain = (l_gain * (64 - balance)) >> 5;
			}

			for (; ret_val; ret_val -= 2) {
			    *mix_dest++ +=
				(*mix_src++ * l_gain) >> AM_MAX_GAIN_SHIFT;
			    *mix_dest++ +=
				(*mix_src++ * r_gain) >> AM_MAX_GAIN_SHIFT;
			}
		    }
		    ATRACE("am_get_audio_trad(M) mix_src after", mix_src);
		    ATRACE("am_get_audio_trad(M) mix_dest after", mix_dest);

		    ATRACE_32("am_get_audio_trad(M) ret_val", ret_val);
		    ATRACE_32("am_get_audio_trad(M) max_ret_val", max_ret_val);

		    ATRACE("am_get_audio_trad(M) going again", chptr);
		}

		/* now convert into the format the hardware needs */
		ATRACE("am_get_audio_trad(M) calling am_convert()",
		    stpptr->mix_buf);
		am_convert(max_ret_val, stpptr->mix_buf, buf,
		    &((audio_info_t *)apm_infop->apm_ad_state)->play);

		/* update hardware sample count */
		hw_info->play.samples += max_ret_val / hw_info->play.channels;

		ATRACE("am_get_audio_trad(M) done", buf);

		return (max_ret_val);
	} else {
		ASSERT(((am_ad_info_t *)apm_infop->apm_ad_infop)->ad_mode ==
							AM_COMPAT_MODE);

		/* go through the chs, looking for the 1 playing AUDIO ch */
		for (i = 0, chptr = &statep->as_channels[0];
		    i < max_chs; i++, chptr++) {

		    /* lock the channel before we check it out */
		    mutex_enter(&chptr->ch_lock);

		    /* skip non-AUDIO and unallocated channels */
		    if (!(chptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
			chptr->ch_info.dev_type != AUDIO ||
			chptr->ch_info.pid == 0) {

			mutex_exit(&chptr->ch_lock);
			continue;
		    }

			/*
			 * The channel may have been closed while we waited
			 * on the mutex. So once we get it we make sure the
			 * channel is still valid.
			 */
		    chpptr = chptr->ch_private;
		    info = chptr->ch_info.info;
		    if (chpptr == NULL || info == NULL ||
			(chpptr->flags & AM_CHNL_OPEN) == 0) {

			mutex_exit(&chptr->ch_lock);
			ATRACE("am_get_audio_trad(S) channel closed", chptr);
			continue;
		    }

		    /* skip non-playing AUDIO channels */
		    if (!chpptr->writing) {
			ATRACE("am_get_audio_trad(S) not playing", chpptr);
			mutex_exit(&chptr->ch_lock);
			continue;
		    }

		    /* skip paused AUDIO channels */
		    if (info->play.pause) {
			mutex_exit(&chptr->ch_lock);
			return (0);
		    }

		    ATRACE_32("am_get_audio_trad(S) found channel", i);

		    /* get "samples" worth of audio */
		    ATRACE_32("am_get_audio_trad(S) calling am_get_samples()",
			samples);
		    ret_val = am_get_samples(chptr, samples, buf,
			((am_ad_info_t *)apm_infop->apm_ad_infop)->ad_mode);

		    /* now we can see how well the am_get_samples() call did */
		    if (ret_val == AUDIO_FAILURE || ret_val == 0) {
			mutex_exit(&chptr->ch_lock);
			ATRACE_32(
			    "am_get_audio_trad(S) am_get_samples() failed",
			    ret_val);
			return (ret_val);
		    }

		    ATRACE_32("am_get_audio_trad(S) am_get_samples() succeeded",
			ret_val);

		    /* we can free the lock now */
		    mutex_exit(&chptr->ch_lock);

		    ATRACE("am_get_audio_trad(S) done", buf);

		    return (ret_val);
		}

		ATRACE("am_get_audio_trad(S) done, no play channel", buf);
		return (0);
	}

}	/* am_get_audio_trad() */

/*
 * am_get_samples()
 *
 * Description:
 *	This routine takes the first message off the channel's queue. It
 *	then takes audio data until the requested number of samples has
 *	been reached or there are no more samples. If the message isn't
 *	empty it is put back onto the message queue.
 *
 *	If the channel is muted then the requested number of samples is
 *	updated in the buffer, the pointer for the message is advanced and
 *	the buffer is zeroed, filling it with silence.
 *
 *	If the "mode" argument is set to AM_COMPAT_MODE then the data size
 *	is set by the channel's info structure. Otherwise it is set to the
 *	size of an integer. Because multi-stream devices get raw data,
 *	am_get_audio_multi() calls this with AM_COMPAT_MODE set.
 *
 *	No conversions are done in this routine.
 *
 *	CAUTION: The channel must be locked before this routine is called.
 *
 *	NOTE: The variable "samples" is the number of samples returned
 *		ultimately to the hardware.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel's state structure
 *	int		samples		The number of samples to get
 *	void		*buf		The buffer to put the samples into
 *	int		mode		Mixer mode
 *
 * Returns:
 *	0 -> samples			The number of samples retrieved
 *	AUDIO_FAILURE			There was an error getting samples
 */
static int
am_get_samples(audio_ch_t *chptr, register int samples, void *buf, int mode)
{
	audio_msg_t		*msg;
	am_ad_info_t		*ad_infop = chptr->ch_apm_infop->apm_ad_infop;
	am_ch_private_t		*chpptr = chptr->ch_private;
	am_apm_private_t	*stpptr = chptr->ch_apm_infop->apm_private;
	audio_info_t		*hw_info = &stpptr->hw_info;
	audio_info_t		*info = chptr->ch_info.info;
	mblk_t			*mp;
	void			*pstart;
	void			*eptr;
#ifdef FLOW_CONTROL
	queue_t			*q;
#endif
	size_t			orig_size;
	size_t			size;
	boolean_t		empty = B_FALSE;
	boolean_t		mute;
	boolean_t		EOF_processed = B_FALSE;
	uint_t			ret_samples = 0;
	int			bytes_needed;
	int			count;

	ATRACE("in am_get_samples()", chptr);

	ASSERT(mutex_owned(&chptr->ch_lock));

	mute = info->output_muted;

	/*
	 * DRAIN Case #3, maybe. If there isn't any more data then this is
	 * actually Case #2, so turn it into that case by not doing the
	 * cv_signal(). We clear the AM_CHNL_DRAIN_NEXT_INT flag because
	 * we want to make sure the next time we run out of audio that we
	 * wait for the DMA buffers to clear.
	 *
	 * If data is queued up then that means this was a Case #3 drain, in
	 * which case we do the cv_signal() and clear the flags. We do it here
	 * at the beginning of the routine so the process can continue as soon
	 * as possible. It also insulates us from any errors in getting the
	 * next batch of samples.
	 */
	if (chpptr->flags & AM_CHNL_DRAIN_NEXT_INT &&
	    audio_sup_get_msg_cnt(chptr)) {
		ATRACE_32("am_get_samples() DRAIN Case #3",
		    chpptr->flags);
		/* if more data than Case #3 */
		chpptr->flags &= ~(AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT);
		chpptr->flags |= AM_CHNL_EMPTY;
		cv_signal(&chptr->ch_cv);
	}

	/* now see how many bytes we need to get from the message buffers */
	if (mode == AM_MIXER_MODE) {
		bytes_needed = samples << AM_INT32_SHIFT;
		size = sizeof (int32_t);
	} else {
		ASSERT(mode == AM_COMPAT_MODE);
		switch (info->play.precision) {
		case AUDIO_PRECISION_8:
			bytes_needed = samples;
			size = sizeof (int8_t);
			break;
		case AUDIO_PRECISION_16:
			bytes_needed = samples << AM_INT16_SHIFT;
			size = sizeof (int16_t);
			break;
		default:
			ATRACE("am_get_samples() bad precision",
			    info->play.precision);
			cmn_err(CE_NOTE,
			    "mixer: get_samples() bad precision, %d",
			    info->play.precision);
			return (AUDIO_FAILURE);
		}
	}

	/* do this now where it's easier */
	if (mute == B_TRUE) {
		/* we return zeros */
		ATRACE("am_get_samples() bzero", buf);
		bzero(buf, bytes_needed);
	}

	/* update played samples */
	if (chpptr->psamples_f) {
		chpptr->psamples_p = 0;
	}
	info->play.samples += chpptr->psamples_f;
	chpptr->psamples_p += chpptr->psamples_c;
	chpptr->psamples_f = 0;
	chpptr->psamples_c = 0;

	/* go through as many buffers as we need to get the samples */
	for (; samples > 0; ) {
		if (!(chpptr->flags & AM_CHNL_OPEN)) {
			ATRACE_32("am_get_samples() not open", chpptr->flags);
			return (0);
		}

		/* get the msg off the list */
		msg = audio_sup_get_msg(chptr);

#ifdef FLOW_CONTROL
		/*
		 * See if we should re-enable the queue. We do this now
		 * because we always need to do it and the code branches below.
		 */
		ASSERT(mutex_owned(&chptr->ch_lock));
		if ((chpptr->flags & AM_CHNL_PFLOW) &&
		    audio_sup_get_msg_size(chptr) <
		    (AM_MIN_QUEUED_MSGS_SIZE)) {
			/* yes, re-enable the q */
			chpptr->flags &= ~AM_CHNL_PFLOW;

			q = WR(chptr->ch_qptr);

			ATRACE("am_get_samples() flow control disabled, q on",
			    q);

			enableok(q);
			qenable(q);
		}
#endif

		/* now we see if we got a message */
		if (msg == 0) {
			ATRACE("am_get_samples() no msg", chptr);
			/* we underflowed, so up error count and send signal */
			if (!(chpptr->flags &
			    (AM_CHNL_EMPTY|AM_CHNL_ALMOST_EMPTY1)) &&
			    !(stpptr->flags & AM_PRIV_SW_MODES)) {
				/* but only send it one time */
			    info->play.error = 1;
			    empty = B_TRUE;
			    am_send_signal(chptr->ch_statep);
			}

			/* did we get any audio at all? */
			ATRACE_32("am_get_samples() ret_samples", ret_samples);
			if (ret_samples == 0) {
			    ATRACE("am_get_samples() no msg returning", chptr);

				/*
				 * Marking the channel as empty is a two step
				 * process because data is queued up and still
				 * being played the first time we determine we
				 * are empty. So the first time we set
				 * ALMOST_EMPTY. The second time we set EMPTY.
				 */
			    if (!(chpptr->flags & (AM_CHNL_ALMOST_EMPTY1|
				AM_CHNL_ALMOST_EMPTY2|AM_CHNL_EMPTY))) {

				chpptr->flags |= AM_CHNL_ALMOST_EMPTY1;
				ATRACE("am_get_samples() no msg empty1",
				    chpptr->flags);
			    } else if
				((chpptr->flags & AM_CHNL_ALMOST_EMPTY1)) {

				chpptr->flags &= ~AM_CHNL_ALMOST_EMPTY1;
				chpptr->flags |= AM_CHNL_ALMOST_EMPTY2;
				ATRACE("am_get_samples() no msg empty2",
				    chpptr->flags);
			    } else {
				ASSERT(chpptr->flags &
				    (AM_CHNL_ALMOST_EMPTY2|AM_CHNL_EMPTY));
				chpptr->flags &= ~(AM_CHNL_ALMOST_EMPTY1|\
				    AM_CHNL_ALMOST_EMPTY2|\
				    AM_CHNL_DRAIN_NEXT_INT|AM_CHNL_DRAIN);
				chpptr->flags |= AM_CHNL_EMPTY;
				ATRACE("am_get_samples() no msg empty",
				    chpptr->flags);
				/* DRAIN case #2 */
				info->play.active = 0;
				cv_signal(&chptr->ch_cv);
			    }
			    goto done_getting_samples;
			}
			ATRACE_32("am_get_samples() no msg but samps",
			    ret_samples);
			goto done_getting_samples;
		}

		/* see if we need to change modes */
		if (stpptr->flags & AM_PRIV_SW_MODES &&
		    ((mode == AM_MIXER_MODE && msg->msg_proc &&
		    msg->msg_proc == msg->msg_pptr) ||
		    (mode == AM_COMPAT_MODE && msg->msg_orig &&
		    msg->msg_orig->b_rptr == msg->msg_optr))) {

			ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);

			audio_sup_putback_msg(chptr, msg);

			ATRACE("am_get_samples() AM_PRIV_SW_MODES return", 0);

			goto done_getting_samples;
		}

		/*
		 * We may have changed modes, and the code immeadiately above
		 * ensures we change modes on a boundary, and have gotten
		 * messages that were queued, but not processed. Or a previous
		 * call to am_p_process() failed, and this gives us a second
		 * chance to process the data before we punt it.
		 */
		if (mode == AM_MIXER_MODE && msg->msg_proc == NULL &&
		    msg->msg_orig->b_datap->db_type == M_DATA &&
		    msg->msg_orig->b_wptr - msg->msg_orig->b_rptr != 0 &&
		    ad_infop->ad_codec_type == AM_TRAD_CODEC) {
			ASSERT(msg->msg_optr == msg->msg_orig->b_rptr);
			ATRACE("am_get_samples(M) calling am_p_process()", msg);
			/* don't let mode switch NULL out src routines */
			mutex_enter(&stpptr->lock);
			if (am_p_process(chptr, msg->msg_orig, &msg->msg_proc,
			    &msg->msg_eptr, &msg->msg_psize) == AUDIO_FAILURE) {
				mutex_exit(&stpptr->lock);
				ATRACE("am_get_samples(M) process failed", msg);
				cmn_err(CE_NOTE, "mixer: get_samples() "
				    "couldn't process message, data lost");
				audio_sup_free_msg(msg);
				continue;
			}
			mutex_exit(&stpptr->lock);
			msg->msg_pptr = msg->msg_proc;
			/* msg_eptr and msg_psize have already been set */

			ATRACE("am_get_samples() process successful", msg);
		}

		/* AUDIO_DRAIN and EOF msgs are always pointed to by msg_orig */
		mp = msg->msg_orig;
		ATRACE("am_get_samples() got msg", mp);

		/* check to see if we've got a DRAIN */
		if (mp->b_datap->db_type == M_BREAK) {
			ATRACE("am_get_samples(8) M_BREAK", mp);
			/*
			 * We mark for DRAIN only if active and DRAIN set.
			 * It is possible for a signal to release the DRAIN
			 * before this gets processed. So if this happens
			 * we ignore the M_BREAK.
			 */
			if (info->play.active &&
			    (chpptr->flags & AM_CHNL_DRAIN)) {
				chpptr->flags |= AM_CHNL_DRAIN_NEXT_INT;
			}
			audio_sup_free_msg(msg);
			continue;	/* get the next msg */
		}

		ATRACE("am_get_samples() !M_BREAK", mp);

		/* check for EOF message, i.e., zero length buffer */
		if ((mp->b_wptr - mp->b_rptr) == 0) {
			ATRACE_32("am_get_samples() EOF",
			    chpptr->EOF[chpptr->EOF_toggle]);

			chpptr->EOF[chpptr->EOF_toggle]++;

			EOF_processed = B_TRUE;

			ATRACE_32("am_get_samples() EOF, new count",
			    chpptr->EOF[chpptr->EOF_toggle]);

			audio_sup_free_msg(msg);
			continue;
		}

		/* get the size of the original msg, in samples */
		orig_size = (int)(msg->msg_orig->b_wptr -
		    msg->msg_orig->b_rptr) /
		    (info->play.precision >> AM_8_SHIFT) / info->play.channels;

		/* get the right buffer */
		if (mode == AM_MIXER_MODE &&
		    ad_infop->ad_codec_type == AM_TRAD_CODEC) {
			pstart = msg->msg_pptr;
			eptr = msg->msg_eptr;
		} else {
			ASSERT(mode == AM_COMPAT_MODE ||
			    ad_infop->ad_codec_type == AM_MS_CODEC);
			ASSERT(mp == msg->msg_orig);
			ASSERT(mp->b_datap->db_type == M_DATA);
			pstart = msg->msg_optr;
			eptr = mp->b_wptr;
		}

		ATRACE("am_get_samples() beginning eptr", eptr);
		ATRACE_32("am_get_samples() samples needed", samples);

		/* get the data from the message and put into the buf */
		if (size == sizeof (int32_t)) {
			int32_t		*sptr;
			int32_t		*bptr = buf;

			/* 32-bits means cannonical, 32-bit data */
			sptr = (int32_t *)pstart;
			ASSERT(sptr >= (int32_t *)pstart);
			ATRACE("am_get_samples(B1) beginning sptr", sptr);

			if (mute == B_TRUE) {
				/* we already zeroed the buffer above */
				if (((int32_t *)eptr - sptr) < samples) {
				    count = (int32_t *)eptr - sptr;
				    samples -= count;
				    sptr = (int32_t *)eptr;
				    ASSERT(samples > 0);
				} else {
				    count = samples;
				    samples = 0;
				    sptr += count;
				    ASSERT(sptr <= (int32_t *)eptr);
				}
			} else {
				/* copy into the buffer */
				for (count = 0;
				    sptr < (int32_t *)eptr && samples > 0;
				    samples--, count++) {
				    *bptr++ = *sptr++;
				}
			}

			ATRACE("am_get_samples(B1) ending sptr", sptr);
			ATRACE_32("am_get_samples(B1) ending samples needed",
			    samples);
			ATRACE_32("am_get_samples(B1) ending count", count);

			ASSERT(sptr <= (int32_t *)eptr);
			ret_samples += count;

			ATRACE_32("am_get_samples(B1) ret_samples",
			    ret_samples);

			/* see if we need to go again */
			if (samples == 0) {	/* nope */
			    /* see if we're done with this message */
			    if (sptr >= (int32_t *)eptr) {
				ASSERT(sptr == (int32_t *)eptr);
				/* update sample counts */
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;

				/* end of message, so free */
				audio_sup_free_msg(msg);
				msg = NULL;
			    } else {	/* nope, use again next time */
				pstart = sptr;
				chpptr->psamples_c += count;
			    }
			    break;
			} else {
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;
			}
			/* we need to go again, but free msg first */
			audio_sup_free_msg(msg);
			buf = bptr;	/* save for next go around */
		} else if (size == sizeof (int16_t)) {
			int16_t		*sptr;
			int16_t		*bptr = buf;

			/* 16-bits means unprocessed data */
			sptr = (int16_t *)pstart;
			ASSERT(sptr >= (int16_t *)pstart);
			ATRACE("am_get_samples(B2) beginning sptr", sptr);

			if (mute == B_TRUE) {
				/* we already zeroed the buffer above */
				if (((int16_t *)eptr - sptr) < samples) {
				    count = (int16_t *)eptr - sptr;
				    samples -= count;
				    sptr = (int16_t *)eptr;
				    ASSERT(samples > 0);
				} else {
				    count = samples;
				    samples = 0;
				    sptr += count;
				    ASSERT(sptr <= (int16_t *)eptr);
				}
			} else {
				/* copy into the buffer */
				for (count = 0;
				    sptr < (int16_t *)eptr && samples > 0;
				    samples--, count++) {
				    *bptr++ = *sptr++;
				}
			}

			ATRACE("am_get_samples(B2) ending sptr", sptr);
			ATRACE_32("am_get_samples(B2) ending samples needed",
			    samples);
			ATRACE_32("am_get_samples(B2) ending count", count);

			ASSERT(sptr <= (int16_t *)eptr);
			ret_samples += count;

			ATRACE_32("am_get_samples(B2) ret_samples",
			    ret_samples);

			/* see if we need to go again */
			if (samples == 0) {	/* nope */
			    /* see if we're done with this message */
			    if (sptr >= (int16_t *)eptr) {
				ASSERT(sptr == (int16_t *)eptr);
				/* update sample counts */
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;

				/* end of message, so free */
				audio_sup_free_msg(msg);
				msg = NULL;
			    } else {	/* nope, use again next time */
				pstart = sptr;
				chpptr->psamples_c += count;
			    }
			    break;
			} else {
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;
			}
			/* we need to go again, but free msg first */
			audio_sup_free_msg(msg);
			buf = bptr;	/* save for next go around */
		} else {
			int8_t		*sptr;
			int8_t		*bptr = buf;

			/* 8-bits means unprocessed data */
			sptr = (int8_t *)pstart;
			ASSERT(sptr >= (int8_t *)pstart);
			ATRACE("am_get_samples(B2) beginning sptr", sptr);

			if (mute == B_TRUE) {
				/* we already zeroed the buffer above */
				if (((int8_t *)eptr - sptr) < samples) {
				    count = (int8_t *)eptr - sptr;
				    samples -= count;
				    sptr = (int8_t *)eptr;
				    ASSERT(samples > 0);
				} else {
				    count = samples;
				    samples = 0;
				    sptr += count;
				    ASSERT(sptr <= (int8_t *)eptr);
				}
			} else {
				/* copy into the buffer */
				for (count = 0;
				    sptr < (int8_t *)eptr && samples > 0;
				    samples--, count++) {
				    *bptr++ = *sptr++;
				}
			}

			ATRACE("am_get_samples(B3) ending sptr", sptr);
			ATRACE_32("am_get_samples(B3) ending samples needed",
			    samples);
			ATRACE_32("am_get_samples(B3) ending count", count);

			ASSERT(sptr <= (int8_t *)eptr);
			ret_samples += count;

			ATRACE_32("am_get_samples(B3) ret_samples",
			    ret_samples);

			/* see if we need to go again */
			if (samples == 0) {	/* nope */
			    /* see if we're done with this message */
			    if (sptr >= (int8_t *)eptr) {
				ASSERT(sptr == (int8_t *)eptr);
				/* update sample counts */
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;

				/* end of message, so free */
				audio_sup_free_msg(msg);
				msg = NULL;
			    } else {	/* nope, use again next time */
				pstart = sptr;
				chpptr->psamples_c += count;
			    }
			    break;
			} else {
				chpptr->psamples_f += orig_size;
				chpptr->psamples_c = 0;
			}
			/* we need to go again, but free msg first */
			audio_sup_free_msg(msg);
			buf = bptr;	/* save for next go around */
		}
	}

	/* update pointers, if partial buffer used */
	if (msg) {
		if (mode == AM_MIXER_MODE &&
		    ad_infop->ad_codec_type == AM_TRAD_CODEC) {
			/* update the processed data pointer */
			msg->msg_pptr = pstart;
		} else {
			/* update the original data pointer */
			msg->msg_optr = pstart;
		}
		audio_sup_putback_msg(chptr, msg);
	}

done_getting_samples:

	/* see if we need to send any EOF signals */
	AUDIO_TOGGLE(chpptr->EOF_toggle);
	if (chpptr->EOF[chpptr->EOF_toggle]) {
		info->play.eof += chpptr->EOF[chpptr->EOF_toggle];
		chpptr->EOF[chpptr->EOF_toggle] = 0;
		am_send_signal(chptr->ch_statep);
	}

	/* if all we have are EOFs then we need to flush the EOF count */
	if (audio_sup_get_msg_cnt(chptr) == 0 && !hw_info->play.active) {
		AUDIO_TOGGLE(chpptr->EOF_toggle);
		if (chpptr->EOF[chpptr->EOF_toggle]) {
			info->play.eof += chpptr->EOF[chpptr->EOF_toggle];
			chpptr->EOF[chpptr->EOF_toggle] = 0;
			am_send_signal(chptr->ch_statep);
		}

		/* if all we had was EOFs then we really are empty */
		if (EOF_processed && ret_samples == 0) {
			chpptr->flags &= ~(AM_CHNL_ALMOST_EMPTY1|\
			    AM_CHNL_ALMOST_EMPTY2);
			chpptr->flags |= AM_CHNL_EMPTY;
		}
	}

	ATRACE("am_get_samples() done_getting_samples", chptr);

	/* now we are done, so return how many samples we have */

	ATRACE_32("am_get_samples() normal return", ret_samples);

	/* make sure virtual channels are still active */
	if (mode == AM_MIXER_MODE && ad_infop->ad_codec_type == AM_TRAD_CODEC) {
		if (ret_samples) {
			((audio_info_t *)chptr->ch_info.info)->play.active = 1;
			hw_info->play.active = 1;
		}
		if (empty == B_TRUE) {
			((audio_info_t *)chptr->ch_info.info)->play.active = 0;
			/* we don't turn off the h/w active flag here */
		}
	}

	return (ret_samples);

}	/* am_get_samples() */

/*
 * am_open()
 *
 * Description:
 *      Open a minor device to gain access to the mixer. There are lots of
 *      rules here, depending on audio vs audioctl and backward compatibility
 *	vs mixer mode.
 *
 *      The high points are:
 *              1. Figure out which device was opened
 *              2. Figure out if need to read and/or write
 *		3. Get the PID for the process opening the device
 *              4. Allocate a channel
 *		5. Initialize the channel
 *
 *	NOTE: When ch_info.info is set and ref_cnt is changed either
 *		statep->as_lock is used, for the device info state
 *		structure, or chptr->ch_lock is used when a new info
 *		state structure is set.
 *
 *	NOTE: In user context so it is okay for memory allocation to sleep.
 *
 * Arguments:
 *      queue_t		*q		Pointer to the read queue
 *      dev_t		*devp		Pointer to the device
 *      int		oflag		Open flags
 *      int		sflag		STREAMS flag
 *      cred_t		*credp		Ptr to the user's credential structure
 *
 * Returns:
 *      AUDIO_SUCCESS			Successfully opened the device
 *      errno				Error number for failed open
 */
/*ARGSUSED*/
static int
am_open(queue_t *q, dev_t *devp, int oflag, int sflag, cred_t *credp)
{
	audio_state_t		*statep;
	audio_apm_info_t	*apm_infop;
	audio_ch_t		*chptr;
	audio_ch_t		*tchptr;
	audio_info_t		*default_info;
	audio_info_t		*hw_info;
	audio_info_t		*iptr;
	am_ch_private_t		*chpptr = 0;
	am_ad_info_t		*ad_infop;
	am_apm_private_t	*stpptr;
	audio_device_type_e	m_type;
	pid_t			pid;
	boolean_t		wantread = B_FALSE;
	boolean_t		wantwrite = B_FALSE;
	ulong_t			minor;
	uint_t			temp = 0;
	int			ch_flags;
	int			i;
	int			instance =
				    audio_sup_get_dev_instance(*devp, NULL);
	int			max_chs;
	int			mode;
	int			rc;

	ATRACE("in am_open()", devp);

	/* this driver does only a conventional open, i.e., no clone opens */
	if (sflag) {
		ATRACE("am_open() clone open failure", sflag);
		cmn_err(CE_NOTE,
			"mixer: open() only conventional opens are supported");
		return (EIO);
	}

	/* get the state structure */
	if ((statep = audio_sup_get_state(NULL, *devp)) == NULL) {
		ATRACE_32("am_open() audio_sup_get_state() failed", 0);
		return (EIO);
	}

	/* figure out which minor device was opened, and make sure it's good */
	m_type = audio_sup_get_ch_type(*devp, NULL);
	ATRACE_32("am_open() device type", m_type);

	ASSERT(m_type == AUDIO || m_type == AUDIOCTL);

	/*
	 * Determine if opening to FREAD and/or FWRITE. We also make sure
	 * that at least one of FREAD or FWRITE is set. We ignore this for
	 * control channels.
	 */
	if (m_type == AUDIO) {
		wantread =  (oflag & FREAD)  ? B_TRUE : B_FALSE;
		wantwrite = (oflag & FWRITE) ? B_TRUE : B_FALSE;
		if (wantread == B_FALSE && wantwrite == B_FALSE) {
			ATRACE_32("am_open(): must be RD, WR or RDWR", oflag);
			return (EINVAL);
		}
	}

	/* get the PID for the process opening the channel */
	if (drv_getparm(PPID, &pid) != 0) {
		ATRACE("am_open() drv_getparm() failed", 0);
		cmn_err(CE_NOTE, "mixer: open() drv_getparm() failed");
		return (EIO);
	}

	/* figure out if allocates should sleep or not */
	ch_flags = (oflag & (O_NONBLOCK|O_NDELAY)) ?
	    AUDIO_NO_SLEEP : AUDIO_SLEEP;

	/* get pointers to various data structures */
	if ((apm_infop = audio_sup_get_apm_info(statep, m_type)) == NULL) {
		ATRACE("am_open() audio_sup_get_apm_info() failed", 0);
		return (EIO);
	}

	stpptr = apm_infop->apm_private;
	hw_info = &stpptr->hw_info;
	ad_infop = apm_infop->apm_ad_infop;
	default_info = ad_infop->ad_defaults;
	max_chs = statep->as_max_chs;

	/* we start looking for a channel with the state struct locked */
	mutex_enter(&statep->as_lock);

	/*
	 * While waiting for the mutex we may have started the process of
	 * changing modes. When we shutdown playing and recording the as_lock
	 * mutex may have been released for a cv_wait_sig(). This would allow
	 * this thread to continue and thus another playing/recording channel
	 * would be established, which would mess things up greatly. So we
	 * check to see if we are in the midst of changing modes, and if we
	 * are we block and wait for the mode change to complete. The semantics
	 * of the new mode then determine if we succeed or not.
	 */
	while (stpptr->flags & AM_PRIV_SW_MODES) {
		ATRACE("am_open() AM_PRIV_SW_MODES block", 0);
		if (cv_wait_sig(&statep->as_cv, &statep->as_lock) <= 0) {
			ATRACE("am_open() AM_PRIV_SW_MODES sig wakeup", 0);
			mutex_exit(&statep->as_lock);
			return (EINTR);
		}
		ATRACE("am_open() AM_PRIV_SW_MODES normal wakeup", 0);
	}

again:	/* we loop back to here when cv_wait_sig returns due to a wakeup */
	ASSERT(mutex_owned(&statep->as_lock));

	mode = ad_infop->ad_mode;	/* mode may have changed, so re-get */

	/* get read and write flags, and check for multiple open enabled */
	if (m_type == AUDIO) {
		audio_ch_t	*rd_chptr;
		audio_ch_t	*wr_chptr;
		audio_info_t	*rd_info;
		audio_info_t	*wr_info;
		int		max_rd;
		int		max_wr;
		int		multi_open;

		if (wantread) {
			ch_flags |= AUDIO_RECORD;
			ATRACE_32("am_open() "
				"allocate channel with read limits", ch_flags);
		}
		if (wantwrite) {
			ch_flags |= AUDIO_PLAY;
			ATRACE_32("am_open() "
			    "allocate channel with write limits", ch_flags);
		}

		/*
		 * The hardware may be limited to simplex operation, i.e., it
		 * may only play or record at any one time. We make sure we
		 * haven't asked for something the hardware can't do before
		 * we continue.
		 */
		if ((default_info->hw_features & AUDIO_HWFEATURE_DUPLEX) == 0) {
			ATRACE_32("am_open() simplex",
			    default_info->hw_features);
			/* make sure we didn't open read/write */
			if (wantread && wantwrite) {
			    ATRACE("am_open() simplex failed, RD_WR", stpptr);
			    mutex_exit(&statep->as_lock);
			    return (EBUSY);
			}

			/* make sure we are asking for something we can have */
			if ((*apm_infop->apm_in_chs && wantwrite) ||
			    (*apm_infop->apm_out_chs && wantread)) {

			    ATRACE("am_open() simplex blocked", stpptr);

			    /* is it okay to block and wait for the hw? */
			    if (ch_flags & AUDIO_NO_SLEEP) {
				mutex_exit(&statep->as_lock);
				return (EBUSY);
			    }

				/*
				 * Mark all AUDIO ch waiting flags. We may be
				 * re-marking some, but there may be new chs
				 * since the last loop through the channels.
				 */
			    stpptr->open_waits++;
			    for (i = 0, tchptr = &statep->as_channels[0];
				i < max_chs; i++, tchptr++) {

				/* skip non-AUDIO channels without a PID */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    tchptr->ch_info.dev_type != AUDIO ||
				    tchptr->ch_info.pid == 0) {

				    continue;
				}

				/* use rd_info for a temp variable */
				rd_info = tchptr->ch_info.info;

				/*
				 * We set both because we want one direction
				 * to know that there is a process waiting for
				 * the other direction.
				 */
				rd_info->play.waiting = 1;
				rd_info->record.waiting = 1;
			    }

				/*
				 * Wait for a channel to be freed. We use
				 * wr_info as a flag.
				 */
			    ATRACE("am_open() simplex blocked", stpptr);
			    wr_info = (audio_info_t *)~NULL;
			    if (cv_wait_sig(&statep->as_cv,
				&statep->as_lock) <= 0) {

				ATRACE("am_open() simplex signal wakeup",
					statep);
				/*
				 * This channel may have had a signal, but
				 * that doesn't mean any of the other channels
				 * may proceed. So make sure every channel
				 * gets another go.
				 */
				cv_broadcast(&statep->as_cv);

				wr_info = NULL;		/* mark the failure */
			    }

				/*
				 * Now see if we need to clear the waiting
				 * flags. This happens only when the last
				 * waiting open clears. We reuse rd_info.
				 */
			    if (--stpptr->open_waits == 0) {
				for (i = 0, tchptr = &statep->as_channels[0];
				    i < max_chs; i++, tchptr++) {

				    /* skip non-AUDIO channels without a PID */
				    if (!(tchptr->ch_flags &
					AUDIO_CHNL_ALLOCATED) ||
					tchptr->ch_info.dev_type != AUDIO ||
					tchptr->ch_info.pid == 0) {

					continue;
				    }

				    /* use rd_info for a temp variable */
				    rd_info = tchptr->ch_info.info;

				    /* we set both, so we clear both */
				    rd_info->play.waiting = 0;
				    rd_info->record.waiting = 0;
				}
			    }

			    /* now we see if there's an error or not */
			    if (wr_info == NULL) {
				mutex_exit(&statep->as_lock);
				return (EINTR);
			    }

			    ATRACE("am_open() simplex normal wakeup", statep);
			    goto again;		/* as_lock must be held */
			}
		}

		/*
		 * Getting to here means there aren't any conflicts with the
		 * hardware, so now the semantics of MIXER and COMPAT modes
		 * come to play. But first we make sure there is at least
		 * one channel left to allocate.
		 */

		if (statep->as_ch_inuse >= max_chs) {
			ASSERT(statep->as_ch_inuse == max_chs);
			ATRACE_32("am_open() AUDIO max channels", max_chs);

			/* wait for a channel to be freed, if allowed */
			if (ch_flags & AUDIO_NO_SLEEP) {
			    mutex_exit(&statep->as_lock);
			    return (EBUSY);
			}

			/* we need to set AUDIO and AUDIOCTL ch as waiting */
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip non-AUDIO{CTL} channels without a PID */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    (tchptr->ch_info.dev_type != AUDIO &&
				    tchptr->ch_info.dev_type != AUDIOCTL) ||
				    tchptr->ch_info.pid == 0) {

					continue;
				}

				/* use rd_info for a temp variable */
				rd_info = tchptr->ch_info.info;
				ASSERT(rd_info);

				if (tchptr->ch_dir & AUDIO_PLAY) {
					rd_info->play.waiting = 1;
				}
				if (tchptr->ch_dir & AUDIO_RECORD) {
					rd_info->record.waiting = 1;
				}
			}

			/*
			 * Wait for a channel to be freed. We use wr_info
			 * as a flag.
			 */
			ATRACE("am_open() max channels blocked", stpptr);
			wr_info = (audio_info_t *)~NULL;
			if (cv_wait_sig(&statep->as_cv,
			    &statep->as_lock) <= 0) {

			    ATRACE("am_open() max chs signal wakeup", statep);
				/*
				 * This channel may have had a signal, but
				 * that doesn't mean any of the other channels
				 * may proceed. So make sure every channel
				 * gets another go.
				 */
			    cv_broadcast(&statep->as_cv);

			    wr_info = NULL;		/* mark the failure */
			}

			/*
			 * Clear the waiting flags, the next go around will
			 * cause each blocked thread to re-evaluate setting
			 * their respective waiting flags.
			 */
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip non-AUDIO{CTL} channels without a PID */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    (tchptr->ch_info.dev_type != AUDIO &&
				    tchptr->ch_info.dev_type != AUDIOCTL) ||
				    tchptr->ch_info.pid == 0) {

					continue;
				}

				/* use rd_info for a temp variable */
				rd_info = tchptr->ch_info.info;
				ASSERT(rd_info);

				rd_info->play.waiting = 0;
				rd_info->record.waiting = 0;
			}

			/* now we see if there's an error or not */
			if (wr_info == NULL) {
				mutex_exit(&statep->as_lock);
				return (EINTR);
			}

			ATRACE("am_open() max chs normal wakeup", statep);
			goto again;		/* as_lock must be held */
		}

		/*
		 * We know there's a channel available, so find it. We also
		 * look for any AUDIO channels with the same PID. Normally
		 * we don't allow additional channels to complete, unless
		 * the AUDIO_MIXER_MULTIPLE_OPEN ioctl() has been issued.
		 * Also find current read/write channels if in compat mode,
		 * or current read/write channels in the same PID if mixer
		 * mode. We need this info for the waiting flags.
		 */
		rd_chptr = NULL;
		wr_chptr = NULL;
		rd_info = NULL;
		wr_info = NULL;

		for (i = 0, chptr = NULL, multi_open = 0,
		    tchptr = &statep->as_channels[0];
		    i < max_chs; i++, tchptr++) {

			/* is this channel allocated? we ck only if need */
			if (chptr == NULL &&
			    !(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED)) {
				ATRACE_32("am_open() found channel", i);
				ATRACE("am_open() new channel", tchptr);
				ASSERT(tchptr->ch_info.ch_number == i);
				chptr = tchptr;
				continue;
			}

			/* skip non-audio channels */
			if (!(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
			    tchptr->ch_info.dev_type != AUDIO ||
			    tchptr->ch_info.pid == 0) {
				continue;
			}

			/* if in COMPAT mode set the read/write pointers */
			if (mode == AM_COMPAT_MODE) {
				if (tchptr->ch_dir & AUDIO_RECORD) {
					ASSERT(rd_chptr == NULL);
					rd_chptr = tchptr;
					rd_info = tchptr->ch_info.info;
				}
				if (tchptr->ch_dir & AUDIO_PLAY) {
					ASSERT(wr_chptr == NULL);
					wr_chptr = tchptr;
					wr_info = tchptr->ch_info.info;
				}
				continue;
			}

			/* else look for the same pid in other AUDIO chs */
			if (tchptr->ch_info.pid == pid) {
				ASSERT(tchptr->ch_private);

				/*
				 * Set the read/write pointers, but only for
				 * the first channel found.
				 */
				if (wantread && tchptr->ch_dir & AUDIO_RECORD &&
				    !rd_chptr) {
					rd_chptr = tchptr;
					rd_info = tchptr->ch_info.info;
					multi_open++;
				}
				if (wantwrite && tchptr->ch_dir & AUDIO_PLAY &&
				    !wr_chptr) {
					wr_chptr = tchptr;
					wr_info = tchptr->ch_info.info;
					multi_open++;
				}
				if (((am_ch_private_t *)tchptr->ch_private)->
				    flags & AM_CHNL_MULTI_OPEN) {

				    /* multiple opens allowed, so propagate */
				    temp = AM_CHNL_MULTI_OPEN;
				}
			}
		}
		ASSERT(chptr);

		/*
		 * We've got a channel and we know if there are multiple opens
		 * allowed or not. So make sure that if there are multiple
		 * opens that we allow them.
		 */
		if (multi_open &&
		    (!(temp & AM_CHNL_MULTI_OPEN) || mode == AM_COMPAT_MODE)) {
			/* we don't allow multiple opens, so sleep, if okay */
			ATRACE("am_open() multi-open not allowed", chptr);
			if (ch_flags & AUDIO_NO_SLEEP) {
				/* not allowed to sleep, so return error */
				mutex_exit(&statep->as_lock);
				return (EBUSY);
			}

			/* need to set the waiting flags */
			if (wantread && rd_info) {
				rd_info->record.waiting = 1;
			}
			if (wantwrite && wr_info) {
				wr_info->play.waiting = 1;
			}

			/* we can wait */
			if (cv_wait_sig(&statep->as_cv,
			    &statep->as_lock) <= 0) {
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
				if (wantread && rd_chptr &&
				    rd_chptr->ch_info.dev_type == AUDIO &&
				    rd_chptr->ch_info.info == rd_info &&
				    rd_info->record.waiting) {
					ASSERT(rd_info);
					rd_info->record.waiting = 0;
				}
				if (wantwrite && wr_chptr &&
				    wr_chptr->ch_info.dev_type == AUDIO &&
				    wr_chptr->ch_info.info == wr_info &&
				    wr_info->play.waiting) {
					ASSERT(wr_info);
					wr_info->play.waiting = 0;
				}

				mutex_exit(&statep->as_lock);
				ATRACE("am_open() multi-open signal wakeup", 0);
				return (EINTR);
			}
			/*
			 * Need to clear the waiting flags, but make
			 * sure the channel is still allocated. It may
			 * have gone away while we waited.
			 */
			if (wantread && rd_chptr &&
			    rd_chptr->ch_info.dev_type == AUDIO &&
			    rd_chptr->ch_info.info == rd_info &&
			    rd_info->record.waiting) {
				ASSERT(rd_info);
				rd_info->record.waiting = 0;
			}
			if (wantwrite && wr_chptr &&
			    wr_chptr->ch_info.dev_type == AUDIO &&
			    wr_chptr->ch_info.info == wr_info &&
			    wr_info->play.waiting) {
				ASSERT(wr_info);
				wr_info->play.waiting = 0;
			}
			goto again;		/* as_lock must be held */
		}

		/* finally, make sure the Audio Driver can allocate a channel */
		if (ad_infop->ad_entry->ad_setup) {
			/* there is a setup entry point, so call it */
			if (ad_infop->ad_entry->ad_setup(instance,
			    chptr->ch_info.ch_number,
			    ch_flags) == AUDIO_FAILURE) {
				/*
				 * This should always succeed because we have
				 * the configuration information. So if we
				 * don't there's something wrong and we fail.
				 */
				mutex_exit(&statep->as_lock);
				ATRACE("am_open() ad_setup() failed", 0);
				rc = EIO;
				goto error_wakeup;
			}
			ATRACE("am_open() ad_setup() succeeded", 0);
#ifdef DEBUG
		} else {
			ATRACE("am_open() didn't call ad_setup()", 0);
#endif
		}

		/*
		 * CAUTION: From here on we have to call ad_teardown() to
		 *	free the stream resources in the Audio Driver.
		 *
		 * Now we allocate a channel private data structure and mark
		 * the channel allocated. From here on we have to free the
		 * channel and the private data structure if we return an
		 * error.
		 */
		chpptr = kmem_zalloc(sizeof (*chpptr), KM_SLEEP);

		ATRACE("am_open() AUDIO chpptr", chpptr);

		statep->as_ch_inuse++;
		chptr->ch_flags =		AUDIO_CHNL_ALLOCATED;
		chptr->ch_apm_infop =		apm_infop;
		chptr->ch_dir =			ch_flags&AUDIO_BOTH;
		chpptr->flags =			temp;
		chpptr->reading =		wantread;
		chpptr->writing =		wantwrite;

		/* figure out how many read and write opens are allowed */
		if (mode == AM_MIXER_MODE) {
			max_wr = *apm_infop->apm_max_out_chs;
			max_rd = *apm_infop->apm_max_in_chs;
		} else {
			ASSERT(mode == AM_COMPAT_MODE);
			max_wr = AM_MIN_CHS;
			max_rd = AM_MIN_CHS;
		}

		/*
		 * Now we check the rest of the open semantics to make
		 * sure we can open. It would have been nice to do this
		 * earlier, before we allocated the chpptr and such, but
		 * then the locking wouldn't have been as straight forward.
		 * The result is we have to deallocate the channel before
		 * we jump to again.
		 */
		mutex_enter(apm_infop->apm_swlock);
		ATRACE("am_open() mutex_enter(apm_infop->apm_swlock) ok", 0);
		if (wantread && wantwrite) {
			if (*apm_infop->apm_chs >= *apm_infop->apm_max_chs ||
			    *apm_infop->apm_in_chs >= max_rd ||
			    *apm_infop->apm_out_chs >= max_wr) {

			    /* see if we sleep or return an error */
			    if (ch_flags & AUDIO_NO_SLEEP) {
				/* just return an error */
				mutex_exit(apm_infop->apm_swlock);
				mutex_exit(&statep->as_lock);
				ATRACE("am_open() BOTH: no channels", chptr);
				rc = EBUSY;
				goto error_free;	/* no lock held */
			    }

			    /* need to set the waiting flags */
			    if (rd_info) {
				rd_info->record.waiting = 1;
			    }
			    if (wr_info) {
				wr_info->play.waiting = 1;
			    }

			    /* okay, we sleep, waiting for a channel */
			    mutex_exit(apm_infop->apm_swlock);
			    if (cv_wait_sig(&statep->as_cv,
				&statep->as_lock) <= 0) {
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
				if (rd_chptr &&
				    rd_chptr->ch_info.dev_type == AUDIO &&
				    rd_chptr->ch_info.info == rd_info &&
				    rd_info->record.waiting) {
					ASSERT(rd_info);
					rd_info->record.waiting = 0;
				}
				if (wr_chptr &&
				    wr_chptr->ch_info.dev_type == AUDIO &&
				    wr_chptr->ch_info.info == wr_info &&
				    wr_info->play.waiting) {
					ASSERT(wr_info);
					wr_info->play.waiting = 0;
				}
				mutex_exit(&statep->as_lock);

				ATRACE("am_open() B: signal wakeup", chptr);
				rc =  EINTR;
				goto error_free;	/* no lock held */
			    }
			    statep->as_ch_inuse--;
			    chptr->ch_flags = 0;
			    chptr->ch_apm_infop = 0;
			    chptr->ch_dir = 0;
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
			    if (rd_chptr &&
				rd_chptr->ch_info.dev_type == AUDIO &&
				rd_chptr->ch_info.info == rd_info &&
				rd_info->record.waiting) {
				    ASSERT(rd_info);
				    rd_info->record.waiting = 0;
			    }
			    if (wr_chptr &&
				wr_chptr->ch_info.dev_type == AUDIO &&
				wr_chptr->ch_info.info == wr_info &&
				wr_info->play.waiting) {
				    ASSERT(wr_info);
				    wr_info->play.waiting = 0;
			    }
			    kmem_free(chpptr, sizeof (*chpptr));

			    /* tell the Audio Driver to free up ch resources */
			    if (ad_infop->ad_entry->ad_teardown) {
				ATRACE("am_open() calling ad_teardown(1)", 0);
				ad_infop->ad_entry->ad_teardown(instance,
				    chptr->ch_info.ch_number);
				ATRACE("am_open() ad_teardown(1) returned", 0);
#ifdef DEBUG
			    } else {
				ATRACE("am_open() didn't call ad_teardown(1)",
				    0);
#endif
			    }

			    goto again;		/* as_lock must be held */
			}
			(*apm_infop->apm_in_chs)++;
			(*apm_infop->apm_out_chs)++;
		} else if (wantread) {
			ASSERT(!wantwrite);
			if (*apm_infop->apm_chs >= *apm_infop->apm_max_chs ||
			    *apm_infop->apm_in_chs >= max_rd) {

			    /* see if we sleep or return an error */
			    if (ch_flags & AUDIO_NO_SLEEP) {
				/* just return an error */
				mutex_exit(apm_infop->apm_swlock);
				mutex_exit(&statep->as_lock);
				ATRACE("am_open() READ: no channels", chptr);
				rc = EBUSY;
				goto error_free;	/* no lock held */
			    }

			    /* need to set the waiting flag */
			    if (rd_info) {
				rd_info->record.waiting = 1;
			    }

			    /* okay, we sleep, waiting for a channel */
			    mutex_exit(apm_infop->apm_swlock);
			    if (cv_wait_sig(&statep->as_cv,
				&statep->as_lock) <= 0) {
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
				if (rd_chptr &&
				    rd_chptr->ch_info.dev_type == AUDIO &&
				    rd_chptr->ch_info.info == rd_info &&
				    rd_info->record.waiting) {
					ASSERT(rd_info);
					rd_info->record.waiting = 0;
				}
				mutex_exit(&statep->as_lock);

				ATRACE("am_open() B: signal wakeup", chptr);
				rc = EINTR;
				goto error_free;	/* no lock held */
			    }
			    statep->as_ch_inuse--;
			    chptr->ch_flags = 0;
			    chptr->ch_apm_infop = 0;
			    chptr->ch_dir = 0;
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
			    if (rd_chptr &&
				rd_chptr->ch_info.dev_type == AUDIO &&
				rd_chptr->ch_info.info == rd_info &&
				rd_info->record.waiting) {
				    ASSERT(rd_info);
				    rd_info->record.waiting = 0;
			    }
			    kmem_free(chpptr, sizeof (*chpptr));

			    /* tell the Audio Driver to free up ch resources */
			    if (ad_infop->ad_entry->ad_teardown) {
				ATRACE("am_open() calling ad_teardown(2)", 0);
				ad_infop->ad_entry->ad_teardown(instance,
				    chptr->ch_info.ch_number);
				ATRACE("am_open() ad_teardown(2) returned", 0);
#ifdef DEBUG
			    } else {
				ATRACE("am_open() didn't call ad_teardown(2)",
				    0);
#endif
			    }

			    goto again;		/* as_lock must be held */
			}
			(*apm_infop->apm_in_chs)++;
		} else {
			ASSERT(wantwrite);
			ASSERT(!wantread);
			if (*apm_infop->apm_chs >= *apm_infop->apm_max_chs ||
			    *apm_infop->apm_out_chs >= max_wr) {

			    /* see if we sleep or return an error */
			    if (ch_flags & AUDIO_NO_SLEEP) {
				/* just return an error */
				mutex_exit(apm_infop->apm_swlock);
				mutex_exit(&statep->as_lock);
				ATRACE("am_open() WRITE: no channels", chptr);
				rc = EBUSY;
				goto error_free;	/* no lock held */
			    }

			    /* need to set the waiting flags */
			    if (wr_info) {
				wr_info->play.waiting = 1;
			    }

			    /* okay, we sleep, waiting for a channel */
			    mutex_exit(apm_infop->apm_swlock);
			    if (cv_wait_sig(&statep->as_cv,
				&statep->as_lock) <= 0) {
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
				if (wr_chptr &&
				    wr_chptr->ch_info.dev_type == AUDIO &&
				    wr_chptr->ch_info.info == wr_info &&
				    wr_info->play.waiting) {
					ASSERT(wr_info);
					wr_info->play.waiting = 0;
				}
				mutex_exit(&statep->as_lock);

				ATRACE("am_open() B: signal wakeup", chptr);
				rc = EINTR;
				goto error_free;	/* no lock held */
			    }
			    statep->as_ch_inuse--;
			    chptr->ch_flags = 0;
			    chptr->ch_apm_infop = 0;
			    chptr->ch_dir = 0;
				/*
				 * Need to clear the waiting flags, but make
				 * sure the channel is still allocated. It may
				 * have gone away while we waited.
				 */
			    if (wr_chptr &&
				wr_chptr->ch_info.dev_type == AUDIO &&
				wr_chptr->ch_info.info == wr_info &&
				wr_info->play.waiting) {
				    ASSERT(wr_info);
				    wr_info->play.waiting = 0;
			    }
			    kmem_free(chpptr, sizeof (*chpptr));

			    /* tell the Audio Driver to free up ch resources */
			    if (ad_infop->ad_entry->ad_teardown) {
				ATRACE("am_open() calling ad_teardown(3)", 0);
				ad_infop->ad_entry->ad_teardown(instance,
				    chptr->ch_info.ch_number);
				ATRACE("am_open() ad_teardown(3) returned", 0);
#ifdef DEBUG
			    } else {
				ATRACE("am_open() didn't call ad_teardown(3)",
				    0);
#endif
			    }

			    goto again;		/* as_lock must be held */
			}
			(*apm_infop->apm_out_chs)++;
		}
		(*apm_infop->apm_chs)++;
		mutex_exit(apm_infop->apm_swlock);
		mutex_exit(&statep->as_lock);
	} else {
		ASSERT(m_type == AUDIOCTL);

		/*
		 * AUDIOCTL channels are much easier. As long as there's
		 * a channel to allocate we succeed.
		 */
		if (statep->as_ch_inuse >= max_chs) {
			ASSERT(statep->as_ch_inuse == max_chs);
			ATRACE_32("am_open() AUDIOCTL max channels", max_chs);

			/* wait for a channel to be freed, if allowed */
			if (ch_flags & AUDIO_NO_SLEEP) {
			    mutex_exit(&statep->as_lock);
			    return (EBUSY);
			}

			/* wait for a channel to be freed */
			if (cv_wait_sig(&statep->as_cv,
			    &statep->as_lock) <= 0) {

			    ATRACE("am_open() max chs signal wakeup", statep);
			    mutex_exit(&statep->as_lock);
			    return (EINTR);
			}

			ATRACE("am_open() max chs normal wakeup", statep);
			goto again;		/* as_lock must be held */
		}

		/* we know there's a channel available, so find it */
		for (i = 0, chptr = NULL, tchptr = &statep->as_channels[0];
		    i < max_chs; i++, tchptr++) {

			/* is this channel allocated */
			if (!(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED)) {
				ATRACE_32("am_open() found CTL channel", i);
				ATRACE("am_open() new CTL channel", tchptr);
				ASSERT(tchptr->ch_info.ch_number == i);
				chptr = tchptr;
				break;
			}
		}
		ASSERT(chptr);
		ASSERT(i < max_chs);

		/*
		 * Now we allocate a channel private data sturcture and mark
		 * the channel allocated. From here on we have to free the
		 * channel and the private data structure if we return an
		 * error.
		 */
		chpptr = kmem_zalloc(sizeof (*chpptr), KM_SLEEP);

		ATRACE("am_open() AUDIOCTL chpptr", chpptr);

		statep->as_ch_inuse++;
		chptr->ch_flags =		AUDIO_CHNL_ALLOCATED;
		chptr->ch_apm_infop =		apm_infop;
		chptr->ch_dir =			0;
		chpptr->flags =			temp;

		/* now we can free the lock, from here on errors are harder */
		mutex_exit(&statep->as_lock);

		/*
		 * Update the number of AUDIOCTL chs open for the mixer. We
		 * don't bother updating the read or write counts because
		 * they don't make sence for AUDIOCTL channels.
		 */
		mutex_enter(apm_infop->apm_swlock);
		(*apm_infop->apm_chs)++;
		mutex_exit(apm_infop->apm_swlock);
	}

	/*
	 * We have a good channel, all open() semantics pass, so init the ch.
	 * NOTE: Since open() and close() use the read queue, the private
	 *	data is stored in the read queue. We put it in the write
	 * 	queue also to make it easier on other routimes, like the
	 *	put() routines.
	 *
	 * CAUTION: pid isn't filled in until the very end. Otherwise
	 *	other routines that look for AUDIO or AUDIOCTL channels may
	 *	think that the channel is fully allocated and available for
	 *	use.
	 */
	q->q_ptr = (caddr_t)chptr;
	WR(q)->q_ptr = (caddr_t)chptr;

	chptr->ch_qptr =		q;
	chptr->ch_msgs =		0;
	chptr->ch_msg_cnt =		0;
	chptr->ch_wput =		am_wput;
	chptr->ch_wsvc =		am_wsvc;
	chptr->ch_rput =		am_rput;
	chptr->ch_rsvc =		am_rsvc;
	chptr->ch_private =		chpptr;
	chptr->ch_info.pid =		0;	/* make sure */
	chptr->ch_info.dev_type =	m_type;
	chptr->ch_info.info_size =	sizeof (audio_info_t);
	chptr->ch_dev_info =		ad_infop->ad_dev_info;

	/* get the minor device for the new channel */
	minor = audio_sup_ch_to_minor(statep, chptr->ch_info.ch_number);
	ATRACE_32("am_open() channel number", chptr->ch_info.ch_number);
	ATRACE_32("am_open() new minor number", minor);

	/* figure out the audio_info sturcture, and initialize */
	if (m_type == AUDIO) {
		ATRACE("am_open() allocated ch, type == AUDIO", chptr);
		/* get an audio structure to use */
		iptr = kmem_alloc(sizeof (*iptr), KM_SLEEP);

		AUDIO_INIT(iptr, sizeof (*iptr));

		if (mode == AM_MIXER_MODE) {	/* virtual channel */
			ATRACE("am_open() AUDIO MIXER mode", chptr);
			/* set defaults for the virtual channel */
			iptr->record.sample_rate = AM_DEFAULT_SAMPLERATE;
			iptr->record.channels = AUDIO_CHANNELS_MONO;
			iptr->record.precision = AUDIO_PRECISION_8;
			iptr->record.encoding = AUDIO_ENCODING_ULAW;
			iptr->record.gain = stpptr->save_rgain;
			iptr->record.buffer_size = ad_infop->ad_record.ad_bsize;
			iptr->record.samples = 0;
			iptr->record.eof = 0;
			iptr->record.pause = 0;
			iptr->record.error = 0;
			iptr->record.waiting = 0;
			iptr->record.balance = stpptr->save_rbal;
			iptr->record.minordev = minor;
			iptr->record.open = 0;
			iptr->record.active = 0;

			iptr->play.sample_rate = AM_DEFAULT_SAMPLERATE;
			iptr->play.channels = AUDIO_CHANNELS_MONO;
			iptr->play.precision = AUDIO_PRECISION_8;
			iptr->play.encoding = AUDIO_ENCODING_ULAW;
			iptr->play.gain = stpptr->save_pgain;
			iptr->play.buffer_size = ad_infop->ad_play.ad_bsize;
			iptr->play.samples = 0;
			iptr->play.eof = 0;
			iptr->play.pause = 0;
			iptr->play.error = 0;
			iptr->play.waiting = 0;
			iptr->play.balance = stpptr->save_pbal;
			iptr->play.minordev = minor;
			iptr->play.open = 0;
			iptr->play.active = 0;

			/* we don't lock because nothing can be using but us */
			chpptr->flags |= temp;		/* propogate flags */
			chptr->ch_info.info = iptr;
			iptr->ref_cnt = 1;
		} else {	/* AM_COMPAT_MODE, physical channel */
			if (wantread) {
			    iptr->record.sample_rate =
				default_info->record.sample_rate;
			    iptr->record.channels =
				default_info->record.channels;
			    iptr->record.precision =
				default_info->record.precision;
			    iptr->record.encoding =
				default_info->record.encoding;
			    iptr->record.gain = hw_info->record.gain;
			    iptr->record.buffer_size =
				default_info->record.buffer_size;
			    iptr->record.samples = 0;
			    iptr->record.eof = 0;
			    iptr->record.pause = 0;
			    iptr->record.error = 0;
			    iptr->record.waiting = 0;
			    iptr->record.balance = hw_info->record.balance;
			    iptr->record.minordev = minor;
			    iptr->record.active = 0;
			}
			if (wantwrite) {
			    iptr->play.sample_rate =
				default_info->play.sample_rate;
			    iptr->play.channels =
				default_info->play.channels;
			    iptr->play.precision =
				default_info->play.precision;
			    iptr->play.encoding =
				default_info->play.encoding;
			    iptr->play.gain = hw_info->play.gain;
			    iptr->play.buffer_size =
				default_info->play.buffer_size;
			    iptr->play.samples = 0;
			    iptr->play.eof = 0;
			    iptr->play.pause = 0;
			    iptr->play.error = 0;
			    iptr->play.waiting = 0;
			    iptr->play.balance = hw_info->play.balance;
			    iptr->play.minordev = minor;
			    iptr->play.active = 0;
			}
			chptr->ch_info.info = hw_info;
			iptr->ref_cnt = 0;	/* delete struct when done */
		}
		iptr->monitor_gain = hw_info->monitor_gain;
		iptr->output_muted = 0;

		/* set the hardware */
		if (am_audio_set_info(chptr, iptr, NULL) == AUDIO_FAILURE) {
			ATRACE("am_open() hw set failed", chptr);
			/*
			 * Just couldn't do it, so fail, but we free the
			 * channel first.
			 */
			ASSERT(iptr != hw_info);
			kmem_free(iptr, sizeof (*iptr));

			rc = EIO;
			goto error_free_noinc;
		}

		if (wantread) {
			if (mode == AM_MIXER_MODE) {
				iptr->record.open = 1;
			} else {
				hw_info->record.open = 1;
			}
		}
		if (wantwrite) {
			if (mode == AM_MIXER_MODE) {
				iptr->play.open = 1;
			} else {
				hw_info->play.open = 1;
			}
		}
		/* we don't need the iptr struct, if COMPAT mode */
		if (iptr->ref_cnt == 0) {
			ASSERT(iptr != hw_info);
			kmem_free(iptr, sizeof (*iptr));
			/* we lock the state when setting the hardware */
			mutex_enter(&statep->as_lock);
			iptr = hw_info;
			iptr->ref_cnt++;
			mutex_exit(&statep->as_lock);
		}

		if (mode == AM_MIXER_MODE) {
			/*
			 * For mixer mode we see if there are any AUDIOCTL
			 * channels with this process.
			 */

			mutex_enter(&statep->as_lock);
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip unallocated and closing channels */
				if ((tchptr->ch_private &&
				    ((am_ch_private_t *)tchptr->ch_private)->
				    flags & AM_CHNL_CLOSING) ||
				    !(tchptr->ch_flags&AUDIO_CHNL_ALLOCATED)) {
					continue;
				}

				/*
				 * Skip if different PIDs. pid is set to 0
				 * when am_close() is entered, so we aren't
				 * associated with a closing channel.
				 */
				if (tchptr->ch_info.pid != pid) {
					continue;
				}

				/* same PID, make sure it's AUDIOCTL */
				if (tchptr->ch_info.dev_type != AUDIOCTL) {
					continue;
				}

				/*
				 * Yes! It's possible that the AUDIOCTL ch is
				 * already attached to an AUDIO ch. If so we
				 * don't muck with it. From this point it is
				 * indeterminate as to what happens with
				 * AUDIOCTL channels. If it isn't we associate
				 * it with this AUDIO channel.
				 */
				if (tchptr->ch_info.info == hw_info) {
					((audio_info_t *)
					    tchptr->ch_info.info)->ref_cnt--;
					ASSERT(((audio_info_t *)tchptr->
					    ch_info.info)->ref_cnt >= 1);

					/* lock so ioctl()s won't panic */
					mutex_enter(&tchptr->ch_lock);
					((am_ch_private_t *)
					    tchptr->ch_private)->flags |=
					    AM_CHNL_CONTROL;
					tchptr->ch_info.info = iptr;
					iptr->ref_cnt++;
					mutex_exit(&tchptr->ch_lock);
				}

				/*
				 * we don't break because there can be more
				 * than one AUDIOCTL for any one AUDIO channel.
				 */
			}
			mutex_exit(&statep->as_lock);

			/*
			 * Above we set Codec info, but we need to set the
			 * rest of the info in the iptr structure, except for
			 * hardware related members, which we fill in when
			 * needed.
			 */
			iptr->play.avail_ports = hw_info->play.avail_ports;
			iptr->play.mod_ports = hw_info->play.mod_ports;
			iptr->record.avail_ports = hw_info->record.avail_ports;
			iptr->record.mod_ports = hw_info->record.mod_ports;
		}

		ATRACE("am_open() AUDIO successful semantics check", chptr);

		chpptr->psb_size = ad_infop->ad_play.ad_bsize;

		chpptr->play_samp_buf = kmem_zalloc(chpptr->psb_size, KM_SLEEP);

	} else {	/* AUDIOCTL channel, all are virtual channels */

		ATRACE("am_open() allocated ch, type == AUDIOCTL", chptr);

		if (mode == AM_MIXER_MODE) {
			/* is this AUDIOCTL channel related to an AUDIO ch? */
			mutex_enter(&statep->as_lock);
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {
				/* skip myself and unallocated channels */
				if (tchptr == chptr ||
				    tchptr->ch_info.pid == 0) {

				    continue;
				}

				/*
				 * Skip if not an AUDIO channel or if different
				 * PIDs. pid is set to 0 when am_close() is
				 * entered, so we aren't associated with a
				 * closing channel.
				 */
				if (tchptr->ch_info.dev_type == AUDIO &&
				    tchptr->ch_info.pid == pid) {

				    /* yes, so link the info structs */
				    ATRACE("am_open() AUDIOCTL related", chptr);

				    /* we lock the channel, not the state */
				    iptr = tchptr->ch_info.info;
				    mutex_enter(&tchptr->ch_lock);
				    chptr->ch_info.info = iptr;
				    iptr->ref_cnt++;
				    mutex_exit(&tchptr->ch_lock);
				    chpptr->flags |= AM_CHNL_CONTROL;
				    break;
				}
			}
			mutex_exit(&statep->as_lock);

			if (i == max_chs) {	/* no, so link to HW */
				ATRACE("am_open() AUDIOCTL not related", chptr);

				iptr = hw_info;
				mutex_enter(&chptr->ch_lock);
				chptr->ch_info.info = iptr;
				iptr->ref_cnt++;
				mutex_exit(&chptr->ch_lock);
			}
		} else {
			/* in COMPAT mode there is only one state structure */
			ATRACE("am_open() AUDIOCTL: mode == AM_COMPAT_MODE",
			    chptr);

			iptr = hw_info;
			mutex_enter(&chptr->ch_lock);
			chptr->ch_info.info = iptr;
			iptr->ref_cnt++;
			mutex_exit(&chptr->ch_lock);
		}

		ATRACE("am_open() AUDIOCTL iptr", iptr);
		ASSERT(iptr != NULL);
		ASSERT(wantread == 0);
		ASSERT(wantwrite == 0);
	}

	/* we start out open and empty */
	chpptr->flags |= (AM_CHNL_OPEN | AM_CHNL_EMPTY);

	ASSERT(iptr != NULL);

	/* we've made it through all the checks, so it's safe to make the dev */
	*devp = makedevice(getmajor(*devp), minor);
	ATRACE("am_open() made device", devp);
	chptr->ch_dev = *devp;

	ASSERT(chptr->ch_info.ch_number == audio_sup_minor_to_ch(minor));

	ATRACE("am_open() qprocson()", chptr);

	/* schedule the queue */
	qprocson(q);

	/*
	 * Now we can set the pid. We don't need to lock the structure because
	 * the worst thing that will happen is the channel will be skipped and
	 * then picked up on the next sweep through the channels. Once this is
	 * set the other mixer routines will see the channel.
	 *
	 * CAUTION: This must be after qprocson(), otherwise other threads will
	 *	try to process STREAMS messages on a partially started stream.
	 *	This will cause a panic. This shows up in am_send_signal().
	 */
	chptr->ch_info.pid = pid;

	/* MUST be after inc. above - not locked any more, start recording */
	if (wantread) {
		/* start recording, regardless of mode or Codec type */
		ATRACE("am_open() starting record DMA engine", statep);
		if (ad_infop->ad_entry->ad_start_record(instance,
		    chptr->ch_info.ch_number) == AUDIO_SUCCESS) {
			iptr->record.active = 1;
			hw_info->record.active = 1;
			iptr->record.pause = 0;
		} else {
			iptr->record.active = 0;
			hw_info->record.active = 0;
			/* we don't change pause flag if failed to start */
		}
	}

	/* we're going to succeed, so initialize mutex now */
	mutex_init(&chpptr->src_lock, NULL, MUTEX_DRIVER, NULL);

	ATRACE("am_open() successful", statep);

	am_send_signal(statep);

	return (AUDIO_SUCCESS);

error_free:
	/* tell the Audio Driver to free up ch resources */
	if (ad_infop->ad_entry->ad_teardown) {
		ATRACE("am_open() calling ad_teardown(4)", 0);
		ad_infop->ad_entry->ad_teardown(instance,
		    chptr->ch_info.ch_number);
		ATRACE("am_open() ad_teardown(4) returned", 0);
#ifdef DEBUG
	} else {
		ATRACE("am_open() didn't call ad_teardown(4)", 0);
#endif
	}

	/*
	 * When we free a channel structure, audio_sup_free_ch() manipulates
	 * the number of open channels as well as how many read and write
	 * channels. When there's an error we haven't yet updated these counts,
	 * so to keep the counts accurate we increment them here so that it
	 * all works out properly in the end.
	 */
	/* XXX - need to lock, or not have free_ch() dec counts */
	ASSERT(m_type == AUDIO);
	(*apm_infop->apm_chs)++;
	if (wantread) {
		(*apm_infop->apm_in_chs)++;
	}
	if (wantwrite) {
		(*apm_infop->apm_out_chs)++;
	}

error_free_noinc:
	/* never allocated an info structure */
	if (chpptr) {
		ATRACE("am_open() freeing private data", chptr);
		kmem_free(chpptr, sizeof (*chpptr));
	}

	mutex_enter(&chptr->ch_lock);
	chptr->ch_private = 0;
	chptr->ch_info.info = 0;
	mutex_exit(&chptr->ch_lock);

	ATRACE("am_open() calling audio_sup_free_ch()", chptr);
	if (audio_sup_free_ch(chptr) == AUDIO_FAILURE) {
		/* not much we can do if this fails */
		cmn_err(CE_NOTE, "mixer: open() audio_sup_free_ch() error");
	}

	ATRACE_32("am_open() at \"error\"", rc);

error_wakeup:

	return (rc);

}	/* am_open() */

/*
 * am_p_process()
 *
 * Description:
 *	Process the message block into canonical form, if in MIXER mode,
 *	which is 16-bit in a 32-bit word. Then a new STREAMS message is
 *	created, with the original message unchanged. This new message
 *	is then returned.
 *
 *	CAUTION: This routine is called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 *	WARNING: Don't free msg_orig on error, the calling routines will
 *		take care of this.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	mblk_t		*msg_orig	Ptr to the original msg block being
 *					processed
 *	void		**msg_proc	Ptr to the buffer allocated that has
 *					processed data placed into
 *	void		**msg_eptr	Ptr to the ptr that marks the end of
 *					data in the buffer
 *	size_t		*msg_psize	Ptr to the size of the buffer
 *
 * Returns:
 *	AUDIO_SUCCESS		Buffer allocated and data processed
 *	AUDIO_FAILURE		Buffer not allocated, or data not processed
 */
static int
am_p_process(audio_ch_t *chptr, mblk_t *msg_orig, void **msg_proc,
	void **msg_eptr, size_t *msg_psize)
{
	audio_info_t		*info = chptr->ch_info.info;
	am_ad_info_t		*ad_infop = chptr->ch_apm_infop->apm_ad_infop;
	am_ad_src_entry_t	*psrs = ad_infop->ad_play.ad_conv;
	am_ch_private_t		*chpptr = chptr->ch_private;
	int			*conv_data;
	size_t			size;
	uint_t			channels = info->play.channels;
	uint_t			encoding = info->play.encoding;
	uint_t			precision = info->play.precision;
	uint_t			hw_channels;
	int			i;
	int			samples;
#ifdef DEBUG
	int			mode = ad_infop->ad_mode;
#endif

	ATRACE("in am_p_process()", msg_orig);

	/* figure out how large the message is */
	size = msg_orig->b_wptr - msg_orig->b_rptr;

	/* make sure we've got samples to process */
	if (size <= 0) {
		ATRACE_32("am_p_process() no samples to process", size);
		goto error;
	}

	ASSERT(mode == AM_MIXER_MODE);
	ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);

	hw_channels =
	    ((audio_info_t *)chptr->ch_apm_infop->apm_ad_state)->play.channels;

	ATRACE_32("am_p_process() msg size", size);
	ATRACE_32("am_p_process() mixer channel", chptr->ch_info.ch_number);
	ATRACE_32("am_p_process() channels", channels);
	ATRACE_32("am_p_process() encoding", encoding);
	ATRACE_32("am_p_process() precision", precision);

	/* figure out the number of samples */
	samples = size / (precision >> AM_8_SHIFT);
	ATRACE_32("am_p_process() samples", samples);

	/* make sure we have a good modulo number, if in stereo */
	if (channels != AUDIO_CHANNELS_MONO && (samples % channels) != 0) {
		ATRACE_32("am_p_process() !mono, samples not mod number chs",
		    channels);
		cmn_err(CE_NOTE,
		    "mixer: process() samples not mod number of channels, ");
		cmn_err(CE_CONT, "samples lost: %d", samples % channels);
		samples -= samples % channels;
	}

	/*
	 * Get the size we can expect for sample rate conversion. This reuses
	 * the size variable.
	 */
	ATRACE("am_p_process() calling ad_src_size()", chptr);
	mutex_enter(&chpptr->src_lock);
	size = psrs->ad_src_size(chptr, AUDIO_PLAY, samples, hw_channels);
	mutex_exit(&chpptr->src_lock);
	ATRACE_32("am_p_process() buffer size", size);
	ATRACE_32("am_p_process() buffer size for this many samples", samples);

	/* make sure we've got buffers large enough, if not, we fix */
	if (size > chpptr->ch_psize1) {
		/* not big enough, so free and reallocate */
		if (chpptr->ch_pptr1) {
			kmem_free(chpptr->ch_pptr1, chpptr->ch_psize1);
		}
		if ((chpptr->ch_pptr1 = kmem_alloc(size, KM_NOSLEEP)) == NULL) {
			ATRACE("am_p_process() ch_pptr1 kmem_alloc() failed",
			    0);
			chpptr->ch_psize1 = 0;
			goto error;
		}
		chpptr->ch_psize1 = size;
	}
	if (size > chpptr->ch_psize2) {
		if (chpptr->ch_pptr2) {
			kmem_free(chpptr->ch_pptr2, chpptr->ch_psize2);
		}
		if ((chpptr->ch_pptr2 = kmem_alloc(size, KM_NOSLEEP)) == NULL) {
			ATRACE("am_p_process() ch_pptr2 kmem_alloc() failed",
			    0);
			chpptr->ch_psize2 = 0;
			goto error;
		}
		chpptr->ch_psize2 = size;
	}

	/*
	 * Now that we've got good buffers it's time to convert to the
	 * canonical form, which is 32-bit. We don't deal with the number
	 * of channels just yet.
	 */
	am_convert_to_int(msg_orig->b_rptr, chpptr->ch_pptr1, samples,
	    precision, encoding);

	/* do the sample rate conversion */
	ASSERT(mode == AM_MIXER_MODE);
	ATRACE_32("am_p_process() AM_MIXER_MODE, calling src_conv", mode);

	mutex_enter(&chpptr->src_lock);
	conv_data = psrs->ad_src_convert(chptr, AUDIO_PLAY,
	    chpptr->ch_pptr1, chpptr->ch_pptr2, &samples);
	mutex_exit(&chpptr->src_lock);

	if (conv_data == NULL) {
		ATRACE("am_p_process() ad_src_conv() failed", msg_orig);
		cmn_err(CE_NOTE, "mixer: process() "
		    "sample rate conversion failed, audio lost");
		goto error;
	}

	ATRACE_32("am_p_process() ad_src_conv() succeeded, samples", samples);

	/*
	 * If hw_channels != channels try to convert. If hw_channels == mono
	 * then we average all the "channels" and place the result into the
	 * mono channel. If hw_channels > channels then we put the converted
	 * audio into the first N channels and silence into the rest. Any other
	 * condition we don't know how to deal with and fail.
	 */
	if (hw_channels == channels) {
		ATRACE_32("am_p_process() hw_chs == chs", hw_channels);

		/* alloc buffer for sample rate converted data */
		*msg_psize = samples << AM_INT32_SHIFT;
		ATRACE_32("am_p_process() mp_proc size #1", *msg_psize);
		if ((*msg_proc = kmem_alloc(*msg_psize, KM_NOSLEEP)) == NULL) {
			ATRACE("am_p_process() "
			    "hw_chs == chs kmem_alloc() failed", 0);
			goto error;
		}

		bcopy(conv_data, *msg_proc, samples * sizeof (int));
		*msg_eptr = (char *)*msg_proc + (samples * sizeof (int));
	} else if (hw_channels == AUDIO_CHANNELS_MONO) {
		int		tmp;
		int		*tmp_ptr;

		ATRACE_32("am_p_process() hw_chs == mono, chs != mono",
		    channels);

		/* alloc buffer for sample rate converted data */
		samples /= channels;
		*msg_psize = samples << AM_INT32_SHIFT;
		ATRACE_32("am_p_process() mp_proc size #2", *msg_psize);
		if ((*msg_proc = kmem_alloc(*msg_psize, KM_NOSLEEP)) == NULL) {
			ATRACE("am_p_process() "
			    "hw_chs == chs kmem_alloc() failed", 0);
			goto error;
		}

		tmp_ptr = (int *)*msg_proc;
		for (tmp = 0; samples--; ) {
			for (i = channels; i--; ) {
				tmp += *conv_data++;
			}
			*tmp_ptr++ = tmp / channels;
		}
		*msg_eptr = tmp_ptr;
	} else if (hw_channels > channels) {
		int		*tmp_ptr;

		ATRACE_32("am_p_process() hw_chs > chs", channels);

		/* alloc buffer for sample rate converted data */
		*msg_psize = size;
		ATRACE_32("am_p_process() mp_proc size #3", *msg_psize);
		if ((*msg_proc = kmem_alloc(*msg_psize, KM_NOSLEEP)) == NULL) {
			ATRACE("am_p_process() "
			    "hw_chs == chs kmem_alloc() failed", 0);
			goto error;
		}

		tmp_ptr = (int *)*msg_proc;
		for (samples /= channels; samples--; ) {
			for (i = 0; i < hw_channels; i++) {
				if (i < channels) {
					*tmp_ptr++ = *conv_data++;
				} else {
					*tmp_ptr++ = 0;
				}
			}
		}
		*msg_eptr = tmp_ptr;
	} else {
		ATRACE_32("am_p_process() hw_chs < chs", hw_channels);
		cmn_err(CE_NOTE, "mixer: process() hw_channels < channels, "
		    "don't know how to convert, audio lost");
		goto error;
	}

	ATRACE("am_p_process() size", *msg_psize);
	ATRACE("am_p_process() returning", *msg_proc);

	return (AUDIO_SUCCESS);

error:
	ATRACE("am_p_process() error return", msg_orig);

	if (*msg_proc) {	/* we may or may not have the buffer */
		kmem_free(*msg_proc, *msg_psize);
		*msg_proc = NULL;
		*msg_eptr = NULL;
		*msg_psize = 0;
	}

	return (AUDIO_FAILURE);

}	/*am_p_process() */

/*
 * am_restart()
 *
 * Description:
 *	This routine is used to restart playing and recording audio when
 * 	they have been stopped to switch mixer modes.
 *
 *	NOTE: This can only be for traditional Codecs, multi-stream Codecs
 *		aren't stopped to changed modes.
 *
 * Arguments:
 *	audio_state_t	*statep		Pointer to the device instance's state
 *	audio_info_t	*hw_info	Pointer to the hardware state
 *
 * Returns:
 *	void
 */
static void
am_restart(audio_state_t *statep, audio_info_t *hw_info)
{
	audio_ch_t		*tchptr;
	am_ch_private_t		*chpptr;
	audio_info_t		*tinfo;
	int			dev_instance;
	int			i;
	int			max_chs = statep->as_max_chs;

	ATRACE("in am_restart()", statep);
	ASSERT(mutex_owned(&statep->as_lock));

	for (i = 0, tchptr = &statep->as_channels[0]; i < max_chs;
	    i++, tchptr++) {

		/* skip non-AUDIO and unallocated channels */
		if (!(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
		    tchptr->ch_info.dev_type != AUDIO ||
		    tchptr->ch_info.pid == 0) {
			continue;
		}


		dev_instance =
		    audio_sup_get_dev_instance(NODEV, tchptr->ch_qptr);
		chpptr = tchptr->ch_private;

		if (chpptr->writing) {
			/*
			 * It is possible we switched modes right when the last
			 * message was played and there's no way we're going
			 * to get the three calls to am_get_samples() we need
			 * to call cv_signal() for AUDIO_DRAIN. Therefore we
			 * set AM_CHNL_EMPTY, which means the next time
			 * am_get_samples() is called, which will happen when
			 * we start playing below, it does the cv_signal().
			 * We can subvert the number of times am_get_samples()
			 * needs to be called because we know the DMA buffers
			 * have been drained.
			 */
			if (audio_sup_get_msg_cnt(tchptr) == 0 &&
			    chpptr->flags & AM_CHNL_DRAIN) {
				chpptr->flags &= ~(AM_CHNL_ALMOST_EMPTY1|\
				    AM_CHNL_ALMOST_EMPTY2|AM_CHNL_DRAIN|\
				    AM_CHNL_DRAIN_NEXT_INT);
				chpptr->flags |= AM_CHNL_EMPTY;
				/* signal any DRAIN situation */
				cv_signal(&tchptr->ch_cv);
			}
			ATRACE("am_restart() starting playing again", tchptr);
			tinfo = tchptr->ch_info.info;
			if (!tinfo->play.pause && ((am_ad_info_t *)
			    tchptr->ch_apm_infop->apm_ad_infop)->
			    ad_entry->ad_start_play(dev_instance,
			    tchptr->ch_info.ch_number) == AUDIO_SUCCESS) {
				tinfo->play.active = 1;
				hw_info->play.active = 1;
				tinfo->play.pause = 0;
			} else {
				tinfo->play.active = 0;
				hw_info->play.active = 0;
				/* we don't change pause if failed to start */
			}
		}
		if (chpptr->reading) {
			ATRACE("am_restart() starting recording again", tchptr);
			if (!tinfo->record.pause && ((am_ad_info_t *)
			    tchptr->ch_apm_infop->apm_ad_infop)->
			    ad_entry->ad_start_record(dev_instance,
			    tchptr->ch_info.ch_number) == AUDIO_SUCCESS) {
				tinfo->record.active = 1;
				hw_info->record.active = 1;
				tinfo->record.pause = 0;
			} else {
				tinfo->record.active = 0;
				hw_info->record.active = 0;
				/* we don't change pause if failed to start */
			}
		}
	}

}	/* am_restart() */

/*
 * am_rput()
 *
 * Description:
 *	We have this here just for symmetry. There aren't any modules/drivers
 *	below this, so this should never be called. But just in case, we
 *	return.
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *      mblk_t		*mp	Ptr to the msg block being passed to the queue
 *
 * Returns:
 *      0			Always returns 0
 */
/*ARGSUSED*/
static int
am_rput(queue_t *q, mblk_t *mp)
{
	ATRACE("in am_rput()", q);

	ATRACE("am_rput() panic", q);

	freemsg(mp);

	return (0);

}	/* am_rput() */

/*
 * am_rsvc()
 *
 * Description:
 *	We have this here just for symmetry. There aren't any modules/drivers
 *	below this, so this should never be called. But just in case, we
 *	return
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *
 * Returns:
 *      0			Always returns 0
 */
/*ARGSUSED*/
static int
am_rsvc(queue_t *q)
{
	ATRACE("in am_rsvc()", q);

	ATRACE("am_rsvc() returning 0", q);

	return (0);

}	/* am_rsvc() */

/*
 * am_wput()
 *
 * Description:
 *	All messages to the mixer arrive here. We just pass them right on
 *	to the write service routine and let it handle everything.
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *      mblk_t		*mp	Ptr to the msg block being passed to the queue
 *
 * Returns:
 *      0			Always returns 0
 */
static int
am_wput(queue_t *q, mblk_t *mp)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	audio_info_t		*hw_info = apm_infop->apm_ad_state;
	am_ad_info_t		*ad_infop = chptr->ch_apm_infop->apm_ad_infop;
	am_ch_private_t		*chpptr = chptr->ch_private;
	audio_info_t		*tinfo;
	int			error = EIO;
	int			instance;

	ATRACE("in am_wput()", q);

	/*
	 * Make sure we've got a channel pointer. Something is terribly
	 * wrong if we don't.
	 */
	if (chptr == 0) {
		ATRACE("am_wput() no chptr", q);
		cmn_err(CE_NOTE, "mixer: wput() failed to get chptr");
		error = EIO;
		goto done;
	}

	/* figure out what kind of message we've got */
	ATRACE_32("am_wput() type", mp->b_datap->db_type);
	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		ATRACE("am_wput() FLUSH", chptr);

		ASSERT(WR(q) == q);

		instance = audio_sup_get_dev_instance(NODEV, q);

		/* are we flushing the play side? */
		if (*mp->b_rptr & FLUSHW) {
			ATRACE("am_wput() flushing play side", 0);
			flushq(q, FLUSHDATA);
			*mp->b_rptr &= ~FLUSHW;

			/* flush accumulated data */
			audio_sup_flush_msgs(chptr);

			/*
			 * Flush the DMA engine and CODEC, but only if this
			 * channel points to the hardware and is an AUDIO
			 * channel.
			 */
			mutex_enter(apm_infop->apm_swlock);
			tinfo = chptr->ch_info.info;
			if (chptr->ch_info.dev_type == AUDIO &&
			    (chptr->ch_info.info == apm_infop->apm_ad_state ||
			    ad_infop->ad_codec_type == AM_MS_CODEC)) {
				/*
				 * Before we can flush the DMA engine we
				 * must stop it.
				 */
				ad_infop->ad_entry->ad_stop_play(instance,
				    chptr->ch_info.ch_number);

				/* do we need to restart the play? */
				if (tinfo->play.active &&
				    ad_infop->ad_entry->ad_start_play(
				    instance, chptr->ch_info.ch_number) ==
				    AUDIO_FAILURE) {
					tinfo->play.active = 0;
					hw_info->play.active = 0;
				} else {
					tinfo->play.active = 1;
					hw_info->play.active = 1;
				}
				/* we don't change pause flag, it can be set */
			}
			mutex_exit(apm_infop->apm_swlock);

			/* by definition we are empty */
			mutex_enter(&chptr->ch_lock);
			chpptr->flags |= AM_CHNL_EMPTY;
			cv_signal(&chptr->ch_cv);	/* wake up any DRAINs */
			mutex_exit(&chptr->ch_lock);

			/* just in case we were flow controlled */
			enableok(q);
			qenable(q);
		}

		/* now for the record side */
		if (*mp->b_rptr & FLUSHR) {
			ATRACE("am_wput() flushing record side", 0);

			/*
			 * Flush the DMA engine and CODEC, but only if this
			 * channel points to the hardware and is an AUDIO
			 * channel.
			 */
			mutex_enter(apm_infop->apm_swlock);
			tinfo = chptr->ch_info.info;
			if (chptr->ch_info.dev_type == AUDIO) {
				/*
				 * We only flush AUDIO channels, there's
				 * nothing on AUDIOCTL channels to flush.
				 */
				if (tinfo->record.active &&
				    (chptr->ch_info.info ==
				    apm_infop->apm_ad_state ||
				    ad_infop->ad_codec_type == AM_MS_CODEC)) {
					/*
					 * Before we can flush the DMA engine we
					 * must stop it.
					 */
					ad_infop->ad_entry->ad_stop_record(
					    instance, chptr->ch_info.ch_number);

					/*
					 * Flush any partially captured data.
					 * This needs to be done before we
					 * restart recording.
					 */
					mutex_enter(&chptr->ch_lock);
					if (chpptr->rec_mp) {
						freemsg(chpptr->rec_mp);
						chpptr->rec_mp = NULL;
					}
					mutex_exit(&chptr->ch_lock);

					/* restart the record */
					if (ad_infop->ad_entry->ad_start_record(
					    instance,
					    chptr->ch_info.ch_number) ==
					    AUDIO_FAILURE) {
						cmn_err(CE_WARN, "mixer: "
						    "couldn't restart record "
						    "after flush");
						tinfo->record.active = 0;
						hw_info->record.active = 0;
					}

					mutex_exit(apm_infop->apm_swlock);
				} else {
					/*
					 * Not currently recording but we still
					 * need to flush any partial buffers.
					 */
					mutex_exit(apm_infop->apm_swlock);

					/* flush any partially captured data */
					mutex_enter(&chptr->ch_lock);
					if (chpptr->rec_mp) {
						freemsg(chpptr->rec_mp);
						chpptr->rec_mp = NULL;
					}
					mutex_exit(&chptr->ch_lock);
				}
			}

			/* send the flush back up to the STREAMS head */
			*mp->b_rptr &= ~ FLUSHW;	/* clear the write */
			qreply(q, mp);
			mp = NULL;		/* stop freemsg() */
		}

		error = 0;

		break;

	case M_IOCTL:
		ATRACE("am_wput() IOCTL", chptr);
		return (am_wioctl(q, mp, chptr));

	case M_IOCDATA:
		ATRACE("am_wput() IOCDATA", chptr);
		return (am_wiocdata(q, mp, chptr));

	case M_DATA:
		ATRACE("am_wput() DATA", chptr);
		/* make sure the write is on an AUDIO channel */
		if (chptr->ch_info.dev_type != AUDIO ||
		    !((am_ch_private_t *)chptr->ch_private)->writing) {

			/* NOT an AUDIO channel, we don't allow write */
			ATRACE_32("am_wput() not AUDIO",
			    chptr->ch_info.dev_type);
			goto done;
		} else {
			ATRACE("am_wput() putting msg on q", mp);

			/*
			 * First, concatenate the message. If in mixer mode
			 * with a traditional Codec we do sample rate conversion
			 * on the concatenated buffer before we save the data
			 * for later use.
			 */
			if (pullupmsg(mp, -1)) {
				ATRACE("am_wput() pullupmsg() successful", mp);
				(void) putq(q, mp);	/* does qenable() */
			} else {
				ATRACE("am_wput() pullupmsg() failed", mp);
				cmn_err(CE_NOTE, "mixer: wput() "
				    "pullupmsg() failed, sound lost");
				freemsg(mp);
			}
			ATRACE_32("am_wput() putting msg on q done", 0);
		}

		/* don't break because then we free a msg we want to keep */
		ATRACE_32("am_wput() returning", 0);
		return (0);

	case M_PROTO:
		/*
		 * The mixer, and it's associated audio drivers, don't
		 * support any protocol messages. Therefore we just get
		 * rid of it now and return.
		 */
		ATRACE("am_wput() PROTO", chptr);

		error = 0;

		break;

	case M_SIG:
		ATRACE("am_wput() SIG", chptr);
		break;

	case M_PCSIG:
		ATRACE("am_wput() PCSIG", chptr);
		break;

	default:
		ATRACE("am_wput() default", chptr);
		break;
	}

	/* if we get here there was some kind of error, so send an M_ERROR */
done:
	ATRACE("am_wput() done:", chptr);

	if (error) {
		ATRACE_32("am_wput() error", error);
		mp->b_datap->db_type = M_ERROR;
		mp->b_rptr = mp->b_datap->db_base;
		*(int *)mp->b_rptr = EIO;
		mp->b_wptr = mp->b_rptr + sizeof (int *);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = 0;
		}
		qreply(q, mp);
	} else {
		if (mp) {
			freemsg(mp);
		}
	}

	ATRACE("am_wput() returning", chptr);

	return (0);

}	/* am_wput() */

/*
 * am_wsvc()
 *
 * Description:
 *	Write service routine. By definition, this service routine grabs
 *	all messages from the queue before it returns. It creates a linked
 *	list of these messages if an AUDIO channel. Otherwise it processes
 *	the message.
 *
 *	We also make sure the play DMA engine is running.
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *
 * Returns:
 *      0			Always returns 0
 */
static int
am_wsvc(queue_t *q)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	am_ch_private_t		*chpptr = chptr->ch_private;
#ifdef DEBUG
	audio_state_t		*statep = chptr->ch_statep;
#endif
	am_ad_info_t		*ad_infop = chptr->ch_apm_infop->apm_ad_infop;
	am_apm_private_t	*stpptr = chptr->ch_apm_infop->apm_private;
	audio_info_t		*hw_info = &stpptr->hw_info;
	audio_info_t		*tinfo;
	mblk_t			*mp;
	void			*msg_proc = NULL;
	void			*msg_eptr;
	size_t			msg_psize;
	int			codec_type = ad_infop->ad_codec_type;
	int			dev_instance;
	int			mode;

	ATRACE("in am_wsvc()", q);

	mode = ad_infop->ad_mode;
	dev_instance = audio_sup_get_dev_instance(NODEV, q);

	/* we always have to drain the queue */
	while ((mp = getq(q)) != NULL) {
		/* this is an AUDIO channel */
		ATRACE("am_wsvc() processing data", mp);

		/* if this is an M_BREAK, then put on the list */
		if (mp->b_datap->db_type == M_BREAK) {
			ATRACE("am_wsvc() M_BREAK message", mp);
			if (audio_sup_save_msg(chptr, mp, NULL, NULL, NULL) ==
			    AUDIO_FAILURE) {
				cmn_err(CE_NOTE,
				    "mixer: am_wsvc() AUDIO_DRAIN lost");
				freemsg(mp);
			}
#ifdef FLOW_CONTROL
			goto flow;
#else
			continue;
#endif
		}

		/*
		 * Process the message into canonical form and put
		 * it on the list, but only if it isn't an EOF msg.
		 */
		if ((mp->b_wptr - mp->b_rptr) == 0) {
			ATRACE("am_wsvc() EOF message, putting on list", mp);
			if (audio_sup_save_msg(chptr, mp, NULL, NULL, NULL) ==
			    AUDIO_FAILURE) {
				cmn_err(CE_NOTE,
				    "mixer: am_wsvc() EOF marker lost");
				freemsg(mp);
#ifdef FLOW_CONTROL
				goto flow;
#else
				continue;
#endif
			}
		} else if (mode == AM_MIXER_MODE && codec_type != AM_MS_CODEC) {
			ATRACE("am_wsvc() calling am_p_process()", chptr);
			/* don't let mode switch NULL out src routines */
			mutex_enter(&stpptr->lock);

			/*
			 * Regardless of the outcome we still save the data.
			 * The only way this can fail is if we couldn't
			 * allocate memory. When am_get_samples() runs it'll
			 * give it another try before we give up and drop
			 * the audio.
			 */
			if (am_p_process(chptr, mp, &msg_proc, &msg_eptr,
			    &msg_psize) == AUDIO_SUCCESS) {

				/* we don't need the lock any more */
				mutex_exit(&stpptr->lock);

				ASSERT((msg_proc && msg_eptr && msg_psize) ||
				    (!msg_proc && !msg_eptr &&! msg_psize));

				if (audio_sup_save_msg(chptr, mp, msg_proc,
				    msg_eptr, msg_psize) == AUDIO_FAILURE) {
					cmn_err(CE_NOTE, "mixer: am_wsvc(S) "
					    "insufficient memory, audio lost");
					freemsg(mp);
#ifdef FLOW_CONTROL
					goto flow;
#else
					continue;
#endif
				}
			} else {
				/* we don't need the lock any more */
				mutex_exit(&stpptr->lock);

				/*
				 * am_p_process couldn't alloc memory - put the
				 * message on the private queue, and switch flow
				 * control to suspend the queue
				 */
				ATRACE("am_wsvc() unable to alloc memory",
				    chptr);

				if (audio_sup_save_msg(chptr, mp, NULL, NULL,
				    0) == AUDIO_FAILURE) {
					cmn_err(CE_NOTE, "mixer: am_wsvc(F) "
					    "insufficient memory, audio lost");
					freemsg(mp);
				}
#ifdef FLOW_CONTROL
				goto flow;
#else
				continue;
#endif
			}

			ATRACE("am_wsvc() am_p_process succeeded", chptr);
		} else {
			ATRACE("am_wsvc() NOT calling am_p_process()", chptr);

			if (audio_sup_save_msg(chptr, mp, NULL, NULL, 0) ==
			    AUDIO_FAILURE) {
				cmn_err(CE_NOTE, "mixer: am_wsvc(N) "
				    "insufficient memory, audio lost");
				freemsg(mp);
#ifdef FLOW_CONTROL
				goto flow;
#else
				continue;
#endif
			}

			ATRACE("am_wsvc() am_p_process not called, save good",
			    chptr);
		}

		/* mark the channel as busy */
		mutex_enter(&chptr->ch_lock);

		/* we are no longer empty */
		chpptr->flags &= ~(AM_CHNL_EMPTY|AM_CHNL_ALMOST_EMPTY1|\
		    AM_CHNL_ALMOST_EMPTY2);
		ATRACE_32("am_wsvc() not empty", chpptr->flags);

		mutex_exit(&chptr->ch_lock);

		/* if !paused make sure the DMA engine is on */
		tinfo = chptr->ch_info.info;
		if (!tinfo->play.pause &&
		    ad_infop->ad_entry->ad_start_play(dev_instance,
		    chptr->ch_info.ch_number) == AUDIO_SUCCESS) {
			tinfo->play.active = 1;
			hw_info->play.active = 1;
			tinfo->play.pause = 0;
		} else {
			tinfo->play.active = 0;
			hw_info->play.active = 0;
			/* we don't change pause if failed to start */
		}
		ATRACE("am_wsvc() start DMA eng ret", statep);

#ifdef FLOW_CONTROL
flow:
		/* do we need to do flow control? */
		if (audio_sup_get_msg_size(chptr) > AM_MAX_QUEUED_MSGS_SIZE &&
		    !(chpptr->flags & AM_CHNL_PFLOW)) {
			/* yes, do flow control */
			ATRACE("am_wsvc() flow control enabled, q off", q);

			mutex_enter(&chptr->ch_lock);
			chpptr->flags |= AM_CHNL_PFLOW;
			mutex_exit(&chptr->ch_lock);

			noenable(q);		/* keep putq() from enabling */

			return (0);
		}
#endif
	}

	ATRACE("am_wsvc() returning", chptr);

	return (0);

}	/* am_wsvc() */

/*
 * am_send_audio_common()
 *	Common code between am_send_audio_multi() and am_send_audio_trad().
 *
 *	NOTE: We hold the channel mutex because we don't want the channel
 * 		to be closed out from under us while we are assembling the
 *		record buffer.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *	queue_t		*q		Pointer to the channel's q structure
 *	void		*buf		The buffer to get audio from
 *	int		samples		The number of samples the Codec sent
 *	int		precision	The number of bits in each sample
 *
 * Returns:
 *	Void
 */
static void
am_send_audio_common(audio_ch_t *chptr, void *buf, int samples, int precision)
{
	audio_info_t		*info = chptr->ch_info.info;
	am_ch_private_t		*chpptr;
	mblk_t			*mp;
	queue_t			*q = RD(chptr->ch_qptr);
	size_t			size;
	int			bytes_per_samplef;
	int			mp_size;
	int			remaining;

	ATRACE("in am_send_audio_common()", chptr);

	/* sanity checks */
	chpptr = chptr->ch_private;
	if (chpptr == NULL || info == NULL ||
	    (chpptr->flags & AM_CHNL_OPEN) == 0 ||
	    chpptr->flags & AM_CHNL_CLOSING || !chpptr->reading) {
		ATRACE("am_send_audio_common() channel closed", chptr);
		return;
	}

	/* don't do anything if we are paused */
	if (info->record.pause) {
		ATRACE("am_send_audio_common() channel paused", chptr);
		return;
	}

	/* figure out how many bytes there are in a sample */
	bytes_per_samplef = info->record.channels *
		(precision >> AUDIO_PRECISION_SHIFT);

	/* figure out how many bytes we've got */
	size = samples * (precision >> AUDIO_PRECISION_SHIFT);
	ATRACE_32("am_send_audio_common() size", size);

	/* first we see if we have a partial buffer waiting to be sent */
	if (chpptr->rec_mp) {		/* yup, we need to fill it first */
		ATRACE("am_send_audio_common() filling partial buffer",
		    chpptr->rec_mp);

		mp = chpptr->rec_mp;

		/*
		 * Figure out how much of the buffer is remaining. We don't
		 * use the record buffer size because it may have changed
		 * since the message was allocated.
		 */
		remaining = chpptr->rec_remaining;
		ATRACE_32("am_send_audio_common() remaining", remaining);

		/* make sure we've got enough to fill this buffer */
		if (remaining > size) {
			/* we don't, so use what we have and return */
			bcopy(buf, mp->b_wptr, size);
			mp->b_wptr += size;
			chpptr->rec_remaining -= size;
			info->record.samples += size / bytes_per_samplef;
			ATRACE("am_send_audio_common() not enough", mp);
			return;
		}
		/* we do, so fill and then go on to get a new buffer */
		bcopy(buf, mp->b_wptr, remaining);
		mp->b_wptr += remaining;
		info->record.samples += remaining / bytes_per_samplef;
		putnext(q, mp);
		chpptr->rec_mp = NULL;
		buf = (char *)buf + remaining;
		size -= remaining;
		ATRACE_32("am_send_audio_common() partial buffer filled", size);
	}

	/* now place remaning data into buffers */
	remaining = info->record.buffer_size;
	while (size) {
		/* buffer_size may change during loop */
		mp_size = info->record.buffer_size;

		if ((mp = allocb(mp_size, BPRI_HI)) == NULL) {
			/*
			 * Often times when one allocb() fails we get many
			 * more that fail. We don't want this error message
			 * to spew uncontrolled. So we set this flag and the
			 * next time if it's set we don't display the message.
			 * Once we get a success we clear the flags, thus one
			 * message per continuious set of failures.
			 */
			if (!(chpptr->flags & AM_CHNL_CLIMIT)) {
				cmn_err(CE_NOTE, "mixer: send_audio_c() "
				    "allocb() failed, recorded audio lost");
				chpptr->flags |= AM_CHNL_CLIMIT;
			}

			ATRACE("am_send_audio_common() allocb() failed",
				chptr);
			info->record.error = 1;
			return;
		}
		chpptr->flags &= ~AM_CHNL_CLIMIT;

		mp->b_datap->db_type = M_DATA;

		if (mp_size > size) {
			/* partial buffer */
			bcopy(buf, mp->b_rptr, size);
			mp->b_wptr += size;
			info->record.samples += size / bytes_per_samplef;
			chpptr->rec_mp = mp;
			chpptr->rec_remaining = mp_size - size;
			break;
		}
		/* full buffer */
		bcopy(buf, mp->b_rptr, mp_size);
		mp->b_wptr += mp_size;
		info->record.samples += mp_size / bytes_per_samplef;
		putnext(q, mp);
		buf = (char *)buf + mp_size;
		size -= mp_size;
	}

	/* make sure the channel is still active */
	info->record.active = 1;

	ATRACE("am_send_audio_common() done", buf);

}	/* am_send_audio_common() */

/*
 * am_send_audio_multi()
 *
 * Description:
 *	This routine is used by multi-channel CODECs to send a single stream
 *	of audio data to an individual channel. The data is raw and unprocessed.
 *
 * Arguments:
 *	audio_state_t	*statep		Pointer to the device instance's state
 *	void		*buf		The buffer to get audio from
 *	int		channel		The device channel number
 *	int		samples		The number of samples the Codec sent
 *
 *	NOTE: The variable "samples" is the number of samples the hardware
 *		sent. So it is samples at the hardware's sample rate.
 *
 *	NOTE: We don't lock the channel because this routine is never called
 *		once am_close() has turned off recording. Thus the pointers
 *		are always valid.
 *
 * Returns:
 *	void
 */
static void
am_send_audio_multi(audio_state_t *statep, void *buf, int channel, int samples)
{
	audio_ch_t		*chptr = &statep->as_channels[channel];
	audio_info_t		*info = chptr->ch_info.info;

	ATRACE("in am_send_audio_multi()", statep);

	ASSERT(chptr->ch_info.ch_number == channel);

	/* make sure it is an AUDIO channel */
	if (chptr->ch_info.dev_type != AUDIO) {
		ATRACE_32("am_send_audio_multi() not an AUDIO channel",
		    chptr->ch_info.dev_type);
		cmn_err(CE_NOTE, "mixer: send_audio_m() bad channel type: %d",
		    chptr->ch_info.dev_type);
		return;
	}

	/* see if read flow controlled */
	if (!canputnext(RD(chptr->ch_qptr))) {
		info->record.active = 0;
		info->record.error = 1;
		ATRACE_32("am_send_audio_multi() ch flow controlled",
		    chptr->ch_info.ch_number);
		return;
	}

	am_send_audio_common(chptr, buf, samples, (int)info->record.precision);

	ATRACE("am_send_audio_multi() done", buf);

}	/* am_send_audio_multi() */

/*
 * am_send_audio_trad()
 *
 * Description:
 *	This routine is used by traditional Codecs to send a single recorded
 *	audio stream up one or more individual channels. If in COMPAT mode
 *	the data is raw and unprocessed, otherwise it is converted to whatever
 *	format the recording process wants, including sample rate conversion.
 *
 *	NOTE: In compat mode we don't lock the channel because this routine is
 *		never called once am_close() has turned off recording. Thus
 *		the pointers are always valid. However in mixer mode we need
 *		to lock the channel, otherwise the channel could disappear
 *		on us.
 *
 *	CAUTION: This routine is called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 * Arguments:
 *	audio_state_t		*statep		Ptr to the dev instance's state
 *	void			*buf		The buffer to get audio from
 *	int			samples		The # of samples the Codec sent
 *	audio_apm_info_t	*apm_infop	Personality module data struct
 *
 * Returns:
 *	void
 */
static void
am_send_audio_trad(audio_state_t *statep, void *buf, int samples,
	audio_apm_info_t *apm_infop)
{
	audio_ch_t		*chptr;
	am_ch_private_t		*chpptr;
	audio_info_t		*info;
	audio_info_t		*hw_info;
	am_ad_info_t		*ad_infop;
	am_ad_src_entry_t	*rsrs;
	am_apm_private_t	*stpptr;
	int			*conv_data;
	size_t			size;
	uint_t			channels;
	uint_t			hw_channels;
	int			i;
	int			max_chs;
	int			new_samples;

	ATRACE("in am_send_audio_trad()", statep);

	/* get the number of chs for this instance */
	max_chs = statep->as_max_chs;

	/* we behave differently for MIXER vs. COMPAT mode */
	ad_infop = apm_infop->apm_ad_infop;
	if (ad_infop->ad_mode == AM_MIXER_MODE) {
		/* pointers for MIXER mode only */
		rsrs = ad_infop->ad_record.ad_conv;
		hw_info = apm_infop->apm_ad_state;
		stpptr = apm_infop->apm_private;

		/* get the number of recording channels */
		hw_channels = hw_info->record.channels;

		/* make sure our send audio buffer is large enough */
		size = samples * sizeof (int);
		if (stpptr->send_size < size) {
			kmem_free(stpptr->send_buf, stpptr->send_size);
			stpptr->send_buf = kmem_alloc(size, KM_NOSLEEP);
			if (stpptr->send_buf == NULL) {
				cmn_err(CE_NOTE,
				    "mixer: am_send_audio_trad() "
				    "recorded audio lost");
				stpptr->send_size = 0;
				return;
			}
			stpptr->send_size = size;
		}

		/* convert to 32-bit Linear PCM sound, the canonical form */
		am_convert_to_int(buf, stpptr->send_buf, samples,
		    hw_info->record.precision, hw_info->record.encoding);

		/* update recorded sample count */
		hw_info->record.samples += samples / hw_channels;

		/* find each recoding channel and send audio to it */
		for (i = 0, chptr = &statep->as_channels[0];
						i < max_chs; i++, chptr++) {
		    int		*dest;

		    /* lock the channel before we check it out */
		    mutex_enter(&chptr->ch_lock);

		    /* skip non-AUDIO and unallocated channels */
		    if (!(chptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
			chptr->ch_info.dev_type != AUDIO ||
			chptr->ch_info.pid == 0) {

			mutex_exit(&chptr->ch_lock);

			continue;
		    }

			/*
			 * The channel may have been closed while we waited on
			 * the mutex. So once we get it we make sure the
			 * channel is still valid.
			 */
		    chpptr = chptr->ch_private;
		    info = chptr->ch_info.info;
		    if (chpptr == NULL || info == NULL ||
			(chpptr->flags & AM_CHNL_OPEN) == 0 ||
			chpptr->flags & AM_CHNL_CLOSING) {

			mutex_exit(&chptr->ch_lock);
			ATRACE("am_send_audio_trad() channel closed", chptr);
			continue;
		    }

		    if (!chpptr->reading) {
			mutex_exit(&chptr->ch_lock);
			ATRACE_32("am_send_audio_trad() channel !reading",
			    info->record.pause);
			continue;
		    }
		    if (info->record.pause) {
			mutex_exit(&chptr->ch_lock);
			ATRACE_32("am_send_audio_trad() channel paused",
			    info->record.pause);
			continue;
		    }

		    /* see if read flow controlled */
		    if (!canputnext(RD(chptr->ch_qptr))) {
			info->record.active = 0;
			info->record.error = 1;
			mutex_exit(&chptr->ch_lock);
			ATRACE_32("am_send_audio_trad(M) ch flow controlled",
			    chptr->ch_info.ch_number);
			continue;
		    }

		    ATRACE_32("am_send_audio_trad(S) found channel", i);

		    /* figure out the size of sample rate converted data */
		    mutex_enter(&chpptr->src_lock);
		    size = rsrs->ad_src_size(chptr, AUDIO_RECORD, samples,
			hw_channels);
		    mutex_exit(&chpptr->src_lock);
		    ATRACE_32("am_send_audio_trad() buffer size", size);
		    ATRACE_32("am_send_audio_trad() "
			"buffer size for this many samples", samples);

		    /* make sure we've got buffers large enough, if not - fix */
		    if (size > chpptr->ch_rsize1) {
			/* not big enough, so free and reallocate */
			if (chpptr->ch_rptr1) {
			    kmem_free(chpptr->ch_rptr1, chpptr->ch_rsize1);
			}
			if ((chpptr->ch_rptr1 = kmem_alloc(size, KM_NOSLEEP)) ==
			    NULL) {
				mutex_exit(&chptr->ch_lock);
				ATRACE_32("am_send_audio_trad() ch_rptr1 "
				    "kmem_alloc() failed", i);
				cmn_err(CE_WARN, "mixer: am_send_audio_trad()"
				    " couldn't allocate memory, audio lost: %d",
				    i);
				chpptr->ch_rsize1 = 0;
				continue;
			}
			chpptr->ch_rsize1 = size;
		    }

		    if (size > chpptr->ch_rsize2) {
			/* not big enough, so free and reallocate */
			if (chpptr->ch_rptr2) {
			    kmem_free(chpptr->ch_rptr2, chpptr->ch_rsize2);
			}
			if ((chpptr->ch_rptr2 = kmem_alloc(size, KM_NOSLEEP)) ==
			    NULL) {
				mutex_exit(&chptr->ch_lock);
				ATRACE_32("am_send_audio_trad() ch_rptr2 "
				    "kmem_alloc() failed", i);
				cmn_err(CE_WARN, "mixer: am_send_audio_trad()"
				    " couldn't allocate memory, audio lost: %d",
				    i);
				chpptr->ch_rsize2 = 0;
				continue;
			}
			chpptr->ch_rsize2 = size;
		    }

		    /* do the sample rate conversion */
		    new_samples = samples;
		    /* we need to save the original buffer for the next ch */
		    bcopy(stpptr->send_buf, chpptr->ch_rptr1,
			samples * sizeof (int));
		    mutex_enter(&chpptr->src_lock);
		    conv_data = rsrs->ad_src_convert(chptr, AUDIO_RECORD,
			chpptr->ch_rptr1, chpptr->ch_rptr2, &new_samples);
		    mutex_exit(&chpptr->src_lock);

		    if (conv_data == NULL) {
			mutex_exit(&chptr->ch_lock);
			ATRACE("am_send_audio_trad() ad_src_conv() failed", 0);
			cmn_err(CE_NOTE, "mixer: send_audio_t() ad_src_conv() "
				"failed, recorded audio lost");
			continue;
		    }

			/* now we apply gain and balance */
		    channels = info->record.channels;
		    if (channels == AUDIO_CHANNELS_MONO) {
			int		c;
			int		gain;
			int		*ptr;

			gain = info->record.gain;

			for (c = new_samples, ptr = conv_data; c; c--) {
			    *ptr = (*ptr * gain) >> AM_MAX_GAIN_SHIFT;
			    ptr++;
			}
		    } else {
			int		balance;
			int		c;
			int		l_gain;
			int		r_gain;
			int		*ptr;

			l_gain = r_gain = info->record.gain;
			balance = info->record.balance;

			if (balance < AUDIO_MID_BALANCE) {
			    /* leave l gain alone and scale down r gain */
			    r_gain = (r_gain * balance) >> 5;
			} else if (balance > AUDIO_MID_BALANCE) {
			    /* leave r gain alone and scale down l gain */
			    l_gain = (l_gain * (64 - balance)) >> 5;
			}

			for (c = new_samples, ptr = conv_data; c; c -= 2) {
			    *ptr = (*ptr * l_gain) >> AM_MAX_GAIN_SHIFT;
			    ptr++;
			    *ptr = (*ptr * r_gain) >> AM_MAX_GAIN_SHIFT;
			    ptr++;
			}
		    }

			/*
			 * If hw_channels != channels try to convert. If
			 * hw_channels > channels then we put the converted
			 * audio into the first N channels and ignore the rest.
			 * If hw_channels < channels then we put the hw_channels
			 * into the first N channels and silence into the rest.
			 *
			 * Regardless of the number of channels, we still need
			 * to put it into the channel's format.
			 */
		    if (conv_data == chpptr->ch_rptr1) {
			dest = chpptr->ch_rptr2;
		    } else {
			ASSERT(conv_data == chpptr->ch_rptr2);
			dest = chpptr->ch_rptr1;
		    }
		    if (hw_channels == channels) {
			/* just convert and send */
			am_convert(new_samples, conv_data, dest, &info->record);
			am_send_audio_common(chptr, dest, new_samples,
			    info->record.precision);
		    } else if (hw_channels > channels) {
			/* take first N hw_channels and put into channels */
			int		diff = hw_channels - channels;
			int		j = new_samples / hw_channels;
			int		k;
			int		l = channels;
			int		*savec = conv_data;
			int		*saved = dest;

			ASSERT((new_samples % hw_channels) == 0);

			for (; j--; ) {
			    for (k = l; k--; ) {
				*dest++ = *conv_data++;
			    }
			    conv_data += diff;
			}
			am_convert(new_samples, saved, savec, &info->record);
			am_send_audio_common(chptr, savec,
			    new_samples * channels / hw_channels,
			    info->record.precision);
		    } else {
			/* take hw_chs and then put in silence */
			int		h = hw_channels;
			int		k;
			int		l = channels;
			int		j = new_samples / hw_channels;
			int		*savec = conv_data;
			int		*saved = dest;

			ASSERT(hw_channels < channels);
			ASSERT((new_samples % hw_channels) == 0);

			for (; j--; ) {
			    for (k = 0; k < l; k++) {
				if (k < h) {
				    *dest++ = *conv_data++;
				} else {
				    *dest++ = 0;
				}
			    }
			}
			am_convert(new_samples, saved, savec, &info->record);
			am_send_audio_common(chptr, savec,
			    new_samples * channels / hw_channels,
			    info->record.precision);
		    }
		    mutex_exit(&chptr->ch_lock);
		}
		ATRACE("am_send_audio_trad(M) done", buf);

	} else {
		ASSERT(((am_ad_info_t *)apm_infop->apm_ad_infop)->ad_mode ==
		    AM_COMPAT_MODE);

		/* go through the chs looking for the only recording AUDIO ch */
		for (i = 0, chptr = &statep->as_channels[0];
						i < max_chs; i++, chptr++) {

		    /* skip non-AUDIO and unallocated channels */
		    if (!(chptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
			chptr->ch_info.dev_type != AUDIO ||
			chptr->ch_info.pid == 0) {
			continue;
		    }

		    /* make sure this channel is reading */
		    chpptr = chptr->ch_private;
		    if (!chpptr->reading) {
			continue;
		    }

		    /* see if read flow controlled */
		    info = chptr->ch_info.info;
		    if (!canputnext(RD(chptr->ch_qptr))) {
			info->record.active = 0;
			info->record.error = 1;
			ATRACE_32("am_send_audio_multi(C) ch flow controlled",
			    chptr->ch_info.ch_number);
			return;
		    }

		    ATRACE_32("am_send_audio_trad(S) found channel", i);

		    am_send_audio_common(chptr, buf, samples,
			(int)((audio_info_t *)chptr->ch_info.info)->
			record.precision);

		    break;

		}
		ATRACE("am_send_audio_trad(C) done", buf);
	}

}	/* am_send_audio_trad() */

/*
 * am_send_signal()
 *
 * Description:
 *	This routine is used to send signals back to user land processes.
 *
 *	We always create a prototype signal message, but we use dupb() to
 *	actually send up the queue.
 *
 *	This routine needs the state structure to be locked so that the
 *	channels and thus their queue's don't disappear and cause a panic.
 *	However, sometimes this routine can be called with the lock held,
 *	so we need to use mutex_tryenter().
 *
 *	CAUTION: We use mutex_tryenter() to attempt to lock the state
 *		structure. If it's already locked we just give up and
 *		return. Currently this only happens when switching modes,
 *		which sends a signal when it's done, so it's okay to skip
 *		in this case.
 *
 * Arguments:
 *	audio_state_t	*statep		Pointer to the device instance's state
 *
 * Rturns:
 *	void
 */
static void
am_send_signal(audio_state_t *statep)
{
	audio_ch_t	*tchptr;
	mblk_t		*mp;
	mblk_t		*dup_mp;
	int		i;
	int		max_chs;

	ATRACE("in am_send_signal()", statep);

	/* get the number of chs for this instance */
	max_chs = statep->as_max_chs;

	/* allocate a message block for the signal */
	if ((mp = allocb(sizeof (int8_t), BPRI_HI)) == NULL) {
		ATRACE("am_send_signal() couldn't allocate message block",
		    statep);
		cmn_err(CE_NOTE, "mixer: signal() couldn't "
		    "allocate message block to send signal, signal lost");
		return;
	}

	ASSERT(mp->b_cont == 0);

	/* turn it into a signal */
	mp->b_datap->db_type = M_PCSIG;
	*mp->b_wptr++ = SIGPOLL;

	ATRACE("am_send_signal() AM_SIGNAL_ALL_CTL", mp);

	/* keep the channels stable */
	if (mutex_tryenter(&statep->as_lock) == 0) {
		ATRACE("am_send_signal() already locked, so just return",
		    statep);
		freemsg(mp);
		return;
	}

	for (i = 0, tchptr = &statep->as_channels[0];
	    i < max_chs; i++, tchptr++) {
		/* skip unallocated, non-AUDIOCTL and closing channels */
		if (!(tchptr->ch_flags & AUDIO_CHNL_ALLOCATED) ||
		    tchptr->ch_info.dev_type != AUDIOCTL ||
		    tchptr->ch_info.pid == 0 || tchptr->ch_private == NULL ||
		    (((am_ch_private_t *)tchptr->ch_private)->flags
		    & AM_CHNL_CLOSING)) {
			continue;
		}

		ATRACE("am_send_signal() tchptr", tchptr);
		if ((dup_mp = dupb(mp)) == NULL) {
			ATRACE("am_send_signal() AUDIOCTL "
			    "couldn't allocate duplicate message", tchptr);
			cmn_err(CE_NOTE, "mixer: signal() couldn't "
			    "allocate duplicate message to send signal, "
			    "signal lost");
			continue;
		}

		ATRACE_32("am_send_signal() AM_SIGNAL_ALL_CTL putnext()",
		    tchptr->ch_info.ch_number);
		ASSERT((((am_ch_private_t *)tchptr->ch_private)->flags &
		    AM_CHNL_CLOSING) == 0);
		putnext(RD(tchptr->ch_qptr), dup_mp);
		ATRACE("am_send_signal() "
		    "AM_SIGNAL_ALL_CTL putnext() done", dup_mp);
	}

	mutex_exit(&statep->as_lock);

	ATRACE("am_send_signal() done", statep);

	freemsg(mp);	/* we always use dups, so this need to go */

}	/* am_send_signal() */

/*
 * am_set_compat_mode()
 *
 * Description:
 *	This routine is used to convert the mixer from MIXER mode to COMPAT
 *	mode. Any playing and recording channels should have been stopped
 *	before this routine is called.
 *
 *	When this routine is called there may be one playing and one recording
 *	channel.
 *
 *	We don't have to worry about resetting psamples_f after calling
 *	am_audio_set_info() because am_get_samples() has been called twice
 *	while we wait to shutdown. Thus it has already been added into the
 *	sample count.
 *
 *	NOTE: Only traditional Codecs will use this code.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Ptr to the channel chaning the mode
 *	am_ad_info_t	*ad_infop	Ptr to the Audio Driver's config info
 *	audio_ch_t	*pchptr		Ptr to the play channel
 *	audio_ch_t	*pchptr		Ptr to the record channel
 *
 * Returns:
 *	AUDIO_SUCCESS		Mode change completed successfully.
 *	AUDIO_FAILURE		Mode change failed.
 */
static int
am_set_compat_mode(audio_ch_t *chptr, am_ad_info_t *ad_infop,
	audio_ch_t *pchptr, audio_ch_t *rchptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop;
	audio_info_t		*hw_info;
	am_apm_private_t	*stpptr;
	audio_ch_t		nchptr;
	audio_info_t		new_info;
	am_ch_private_t		ch_private;
	uchar_t			popen = 0;
	uchar_t			ropen = 0;

	ATRACE("in am_set_compat_mode()", chptr);
	ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);
	ASSERT(mutex_owned(&statep->as_lock));

	if ((apm_infop = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_set_compat_mode() audio_sup_get_apm_info() failed",
		    statep);
		return (AUDIO_FAILURE);
	}
	stpptr = apm_infop->apm_private;
	hw_info = &stpptr->hw_info;

	/* copy the original channel structure to the temp, just in case */
	bcopy(chptr, &nchptr, sizeof (nchptr));

	/* we only reset the hardware if we are playing or recording */
	AUDIO_INIT(&new_info, sizeof (new_info));
	bzero(&ch_private, sizeof (ch_private));

	nchptr.ch_dir = 0;

	if (pchptr) {
		audio_info_t *p_info = pchptr->ch_info.info;

		new_info.play.sample_rate = p_info->play.sample_rate;
		new_info.play.channels = p_info->play.channels;
		new_info.play.precision = p_info->play.precision;
		new_info.play.encoding = p_info->play.encoding;
		new_info.play.gain = p_info->play.gain;
		new_info.play.balance = p_info->play.balance;
		new_info.play.samples = p_info->play.samples;
		popen = p_info->play.open;
		ch_private.writing = 1;
		nchptr.ch_dir |= AUDIO_PLAY;
	}
	if (rchptr) {
		audio_info_t *r_info = rchptr->ch_info.info;

		new_info.record.sample_rate = r_info->record.sample_rate;
		new_info.record.channels = r_info->record.channels;
		new_info.record.precision = r_info->record.precision;
		new_info.record.encoding = r_info->record.encoding;
		new_info.record.gain = r_info->record.gain;
		new_info.record.balance = r_info->record.balance;
		new_info.record.samples = r_info->record.samples;
		ropen = r_info->record.open;
		ch_private.reading = 1;
		nchptr.ch_dir |= AUDIO_RECORD;
	}

	/* we always save the hardware state, even if no play/rec channels */
	ad_infop->ad_mode = AM_COMPAT_MODE;
	hw_info->sw_features_enabled &= ~AUDIO_SWFEATURE_MIXER;

	/* change the hardware, if we had active play/record channels */
	if (pchptr || rchptr) {
		nchptr.ch_qptr = chptr->ch_qptr;
		nchptr.ch_statep = chptr->ch_statep;
		nchptr.ch_info.dev_type = AUDIO;
		nchptr.ch_apm_infop = apm_infop;
		nchptr.ch_private = &ch_private;
		nchptr.ch_info.info = &new_info;

		if (am_audio_set_info(&nchptr, &new_info, NULL) ==
		    AUDIO_FAILURE) {

		    ATRACE("am_set_compat_mode() am_audio_set_info() failed",
			&nchptr);
		    cmn_err(CE_NOTE, "mixer: set_compat() "
			"couldn't reconfigure hardware");
		    ad_infop->ad_mode = AM_MIXER_MODE;
		    hw_info->sw_features_enabled |= AUDIO_SWFEATURE_MIXER;
		    return (AUDIO_FAILURE);
		}
	}

	/* update the hardware open flags */
	hw_info->play.open = popen;
	hw_info->record.open = ropen;

	return (AUDIO_SUCCESS);

}	/* am_set_compat_mode() */

/*
 * am_set_format()
 *
 *	This routine is used to convert a format from 8-bit u-law or A-law
 *	to 8-bit PCM. Some hardware can't support u-law or A-law, but as
 *	long as we have PCM we can do the conversion.
 *
 * Arguments:
 *	am_ad_entry_t		*entry		Ptr to Audio Driver entry pts.
 *	uint_t			flags		Translation flags
 *	int			instance	Device instance
 *	int			stream		Audio stream
 *	int			dir		AUDIO_PLAY or AUDIO_RECORD
 *	int			sample_rate	Sample rate to set
 *	int			channels	The number of channels to set
 *	int			precision	The sample precision
 *	int			encoding	The encoding method
 *
 * Returns:
 *	AUDIO_SUCCESS		The format was successfully set
 *	AUDIO_FAILURE		The format wasn't set
 */
static int
am_set_format(am_ad_entry_t *entry, uint_t flags, int instance, int stream,
	int dir, int sample_rate, int channels, int precision, int encoding)
{
	ATRACE_32("in am_set_format()", flags);

	if (encoding == AUDIO_ENCODING_ULAW) {
		if (dir == AUDIO_PLAY && flags & AM_PRIV_PULAW_TRANS) {
			ATRACE("am_set_format() PULAW translate", 0);
			encoding = AUDIO_ENCODING_LINEAR;
		} else if (dir == AUDIO_RECORD && flags & AM_PRIV_RULAW_TRANS) {
			ATRACE("am_set_format() RULAW translate", 0);
			encoding = AUDIO_ENCODING_LINEAR;
		}
	} else if (encoding == AUDIO_ENCODING_ALAW) {
		if (dir == AUDIO_PLAY && flags & AM_PRIV_PALAW_TRANS) {
			ATRACE("am_set_format() PALAW translate", 0);
			encoding = AUDIO_ENCODING_LINEAR;
		} else if (dir == AUDIO_RECORD && flags & AM_PRIV_RALAW_TRANS) {
			ATRACE("am_set_format() RALAW translate", 0);
			encoding = AUDIO_ENCODING_LINEAR;
		}
	}

	ATRACE("am_set_format() calling ad_set_format()", 0);
	return (entry->ad_set_format(instance, stream, dir,
	    sample_rate, channels, precision, encoding));

}	/* am_set_format() */

/*
 * am_set_gain()
 *
 * Description:
 *	This routine is used to set the gain of all channels in the Codec.
 *	The gain is modified by balance. We try two different methods, the
 *	first with the gain and balance, and the second with gain and balance
 *	mixed down into left and right gain. This lets the Audio Driver accept
 *	whichever format it prefers. If the Audio Driver doesn't like the
 *	first method it retuns AUDIO_FAILURE and the second is tried.
 *
 *	Some Codecs, like the Crystal 4231, will copy a mono input signal
 *	over to the 2nd channel. If this is the case, and the Audio Driver
 *	does want this, then we mute the other channel(s) so that audio
 *	only goes out channel 0, which is what is supposed to happen.
 *
 *	NOTE: We change the gain only if it actually did change.
 *
 * Arguments:
 *	audio_apm_info_t *apm_infop	Ptr to driver's audio_apm_info structure
 *	uint_t		channels	The number of h/w channels
 *	uint_t		gain		The gain to set
 *	uint_t		balance		The balance to set
 *	int		dir		AUDIO_PLAY or AUDIO_RECORD
 *	int		dev_instance	The hardware device instance
 *	int		stream		The hardware stream to set gain on
 *
 * Returns:
 *	AUDIO_SUCCESS			The gain was successfully set
 *	AUDIO_FAILURE			The gain was not successfully set
 */
static int
am_set_gain(audio_apm_info_t *apm_infop, uint_t channels, uint_t gain,
	uint_t balance, int dir, int dev_instance, int stream)
{
	audio_prinfo_t	*hw_pinfo;
	am_ad_info_t	*ad_infop = apm_infop->apm_ad_infop;
	uint_t		g;

	ATRACE("in am_set_gain()", apm_infop);
	ATRACE("am_set_gain() channels", channels);
	ATRACE("am_set_gain() gain", gain);
	ATRACE("am_set_gain() balance", balance);

	if (dir == AUDIO_PLAY) {
		hw_pinfo = &((audio_info_t *)apm_infop->apm_ad_state)->play;
	} else {
		ASSERT(dir == AUDIO_RECORD);
		hw_pinfo = &((audio_info_t *)apm_infop->apm_ad_state)->record;
	}

	/* 1st try the gain and balance method since it's the easiest for us */
	if (ad_infop->ad_entry->ad_set_config(dev_instance, stream,
	    AMAD_SET_GAIN_BAL, dir, gain, balance) == AUDIO_SUCCESS) {
		ATRACE("am_set_gain() AMAD_SET_GAIN_BAL successful", 0);
		return (AUDIO_SUCCESS);
	}

	if (channels == 1) {
		/* make sure there was a change */
		if (hw_pinfo->gain == gain) {
			ATRACE_32("am_set_gain() mono, the same gain", gain);
			return (AUDIO_SUCCESS);
		}

		/* we always set left gain */
		if (ad_infop->ad_entry->ad_set_config(dev_instance,
		    stream, AMAD_SET_GAIN, dir, gain, 0) ==
		    AUDIO_FAILURE) {
			return (AUDIO_FAILURE);
		}
		ATRACE_32("am_set_gain() mono gain", gain);
		if (ad_infop->ad_misc_flags & AM_MISC_MONO_DUP) {
			/* mono gets duped to all channels, so set all */
			/* left channel, ignoring balance info */
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, gain, 1) ==
			    AUDIO_FAILURE) {
				return (AUDIO_FAILURE);
			}
		}
		return (AUDIO_SUCCESS);
	} else {
		/* make sure there was a change */
		if (hw_pinfo->gain == gain && hw_pinfo->balance == balance) {
			ATRACE_32("am_set_gain() stereo, the same gain", gain);
			ATRACE_32("am_set_gain() stereo, the same balance",
			    balance);
			return (AUDIO_SUCCESS);
		}

		/*
		 * Balance adjusts gain. If balance < 32 then left is
		 * enhanced by attenuating right. If balance > 32 then
		 * right is enhanced by attenuating left.
		 */
		if (balance == AUDIO_MID_BALANCE) {	/* no adj. */
			/* left channel */
			ATRACE_32("am_set_gain() L1 gain", gain);
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, gain, 0) ==
			    AUDIO_FAILURE) {
				return (AUDIO_FAILURE);
			}
			/* right channel */
			ATRACE_32("am_set_gain() R1 gain", gain);
			return (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, gain, 1));
		} else if (balance < AUDIO_MID_BALANCE) {
			/*
			 * l = gain
			 * r = (gain * balance) / 32
			 */
			g = (gain * balance) >> AM_BALANCE_SHIFT;
			/* left channel */
			ATRACE_32("am_set_gain() L2 gain", gain);
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, gain, 0) ==
			    AUDIO_FAILURE) {
				return (AUDIO_FAILURE);
			}
			/* right channel */
			ATRACE_32("am_set_gain() R2 gain", g);
			return (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, g, 1));
		} else {
			/*
			 * l = (gain * (64 - balance)) / 32
			 * r = gain
			 */
			g = (gain * (AUDIO_RIGHT_BALANCE - balance)) >>
			    AM_BALANCE_SHIFT;
			/* left channel */
			ATRACE_32("am_set_gain() L3 gain", g);
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, g, 0) ==
			    AUDIO_FAILURE) {
				return (AUDIO_FAILURE);
			}
			/* right channel */
			ATRACE_32("am_set_gain() R3 gain", gain);
			return (ad_infop->ad_entry->ad_set_config(dev_instance,
			    stream, AMAD_SET_GAIN, dir, gain, 1));
		}
	}

}	/* am_set_gain() */

/*
 * am_set_mixer_mode()
 *
 * Description:
 *	This routine is used to convert the mixer from COMPAT mode to MIXER
 *	mode. Any playing and recording channels should have been stopped
 *	before this routine is called.
 *
 *	When this routine is called there may be one playing and one recording
 *	channel.
 *
 *	Just like am_set_compat_mode(), psamples_f has already been added into
 *	the played sample count. So we don't need to do anything with it here.
 *
 *	NOTE: Only traditional Codecs will use this code.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Ptr to the channel chaning the mode
 *	am_ad_info_t	*ad_infop	Ptr to the Audio Driver's config info
 *	am_apm_private_t **stpptr	Ptr to the mixer's private state data
 *	audio_apm_info_t *apm_infop	Ptr to the mixer's APM info structure
 *	audio_ch_t	*pchptr		Ptr to the play channel
 *	audio_ch_t	*rchptr		Ptr to the record channel
 *
 * Returns:
 *	AUDIO_SUCCESS		Mode change completed successfully.
 *	AUDIO_FAILURE		Mode change failed.
 */
static int
am_set_mixer_mode(audio_ch_t *chptr, am_ad_info_t *ad_infop,
	am_apm_private_t *stpptr, audio_ch_t *pchptr, audio_ch_t *rchptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop;
	audio_ch_t		nchptr;
	am_ch_private_t		ch_private;
	audio_info_t		*hw_info;
	audio_info_t		new_info;

	ATRACE("in am_set_mixer_mode()", statep);
	ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);
	ASSERT(mutex_owned(&statep->as_lock));

	if ((apm_infop = audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
		ATRACE("am_set_mixer_mode() audio_sup_get_apm_info() failed",
		    statep);
		return (AUDIO_FAILURE);
	}
	hw_info = apm_infop->apm_ad_state;

	/* copy the original channel structure to the temp, just in case */
	bcopy(chptr, &nchptr, sizeof (nchptr));

	/* we always reset the hardware, even if no play/rec channels */
	AUDIO_INIT(&new_info, sizeof (new_info));
	bzero(&ch_private, sizeof (ch_private));

	if (pchptr) {
		new_info.play.samples =
		    ((audio_info_t *)pchptr->ch_info.info)->play.samples;
		ch_private.writing = 1;
	}
	if (rchptr) {
		new_info.record.samples =
		    ((audio_info_t *)rchptr->ch_info.info)->record.samples;
		ch_private.reading = 1;
	}

	new_info.play.sample_rate = stpptr->save_psr;
	new_info.play.channels = stpptr->save_pch;
	new_info.play.precision = stpptr->save_pprec;
	new_info.play.encoding = stpptr->save_penc;
	new_info.play.gain = stpptr->save_pgain;
	new_info.play.balance = stpptr->save_pbal;
	new_info.record.sample_rate = stpptr->save_rsr;
	new_info.record.channels = stpptr->save_rch;
	new_info.record.precision = stpptr->save_rprec;
	new_info.record.encoding = stpptr->save_renc;
	new_info.record.gain = stpptr->save_rgain;
	new_info.record.balance = stpptr->save_rbal;

	nchptr.ch_qptr = chptr->ch_qptr;
	nchptr.ch_statep = chptr->ch_statep;
	nchptr.ch_dir = AUDIO_BOTH;
	nchptr.ch_info.dev_type = AUDIO;
	nchptr.ch_apm_infop = apm_infop;
	nchptr.ch_private = &ch_private;
	nchptr.ch_info.info = &new_info;

	if (am_audio_set_info(&nchptr, &new_info, &new_info) == AUDIO_FAILURE) {
		ATRACE("am_set_mixer_mode() am_audio_set_info() failed",
		    &nchptr);
		cmn_err(CE_NOTE, "mixer: set_mixer() "
		    "couldn't reconfigure hardware");
		return (AUDIO_FAILURE);
	}

	/*
	 * We need to look like we've changed modes AFTER we try to set hw.
	 * Otherwise am_audio_set_info() won't update the hardware. It'll
	 * try to upate the virtual channel.
	 */
	ad_infop->ad_mode = AM_MIXER_MODE;
	hw_info->sw_features_enabled |= AUDIO_SWFEATURE_MIXER;

	/* clear the open flags in the hardware */
	hw_info->play.open = 0;
	hw_info->record.open = 0;

	return (AUDIO_SUCCESS);

}	/* am_set_mixer_mode() */

/*
 * am_wiocdata()
 *
 * Description:
 *	This routine is called by am_wsvc() to process all M_IOCDATA
 *	messages.
 *
 *	We only support transparent ioctls.
 *
 *	This routine also is used to return a IOCNAK if the state pointer
 *	or the channel pointer, set up in am_wsvc(), are invalid.
 *
 *	CAUTION: This routine is called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 *	WARNING: Don't forget to free the mblk_t struct used to hold private
 *		data. The ack: and nack: jump points take care of this.
 *
 *	WARNING: Don't free the private mblk_t structure if the command is
 *		going to call qreply(). This frees the private date that will
 *		be needed for the next M_IOCDATA message.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the STREAMS queue
 *	mblk_t		*mp	Pointer to the message block
 *	audio_ch_t	*chptr	Pointer to this channel's state information
 *
 * Returns:
 *	0			Always returns a 0, becomes a return for
 *				am_wsvc()
 */
static int
am_wiocdata(queue_t *q, mblk_t *mp, audio_ch_t *chptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	am_ad_info_t		*ad_infop = apm_infop->apm_ad_infop;
	am_apm_private_t	*stpptr = apm_infop->apm_private;
	am_ch_private_t		*chpptr = chptr->ch_private;
	audio_info_t		*hw_info = apm_infop->apm_ad_state;
	struct iocblk		*iocbp;
	struct copyreq		*cqp;
	struct copyresp		*csp;
	audio_i_state_t		*cmd;
	int			dev_instance =
					audio_sup_get_dev_instance(NODEV, q);
	int			error;
	int			max_chs = statep->as_max_chs;
	int			mode = ad_infop->ad_mode;

	ATRACE("in am_wiocdata()", chptr);
	ATRACE_32("am_wiocdate() channel type", chptr->ch_info.dev_type);

	ASSERT(!mutex_owned(apm_infop->apm_swlock));

	iocbp = (struct iocblk *)mp->b_rptr;	/* pointer to ioctl info */
	csp = (struct copyresp *)mp->b_rptr;	/* set up copy response ptr */
	cmd = (audio_i_state_t *)csp->cp_private;	/* get state info */
	cqp =  (struct copyreq *)mp->b_rptr;	/* set up copy requeset ptr */

	/* make sure this is a transparent ioctl and we have good pointers */
	if (statep == NULL || chptr == NULL) {
		ATRACE("am_wiocdata() no statep or chptr", statep);
		error = EINVAL;
		goto nack;
	}

	/* make sure we've got a good return value */
	if (csp->cp_rval) {
		ATRACE("am_wiocdata() bad return value", csp->cp_rval);
		error = EINVAL;
		goto nack;
	}

	/* find the command */
	ATRACE_32("am_wiocdata() command", cmd->command);
	switch (cmd->command) {

	case COPY_OUT_AUDIOINFO:	/* AUDIO_GETINFO */
		ATRACE("am_wiocdata() COPY_OUT_AUDIOINFO", chptr);
		if (csp->cp_cmd != AUDIO_GETINFO) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_AUDIOINFO2:	/* AUDIO_GETDEV */
		ATRACE("am_wiocdata() COPY_OUT_AUDIOINFO2", chptr);
		if (csp->cp_cmd != AUDIO_SETINFO) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_IN_AUDIOINFO:	{	/* AUDIO_SETINFO */
		audio_info_t	*tinfo = (audio_info_t *)mp->b_cont->b_rptr;

		ATRACE("am_wiocdata() COPY_IN_AUDIOINFO", chptr);

		if (csp->cp_cmd != AUDIO_SETINFO) {
			error = EINVAL;
			goto nack;
		}

		/* too ugly to check here, so send to a utility routine */
		ATRACE("am_wiocdata() "
		    "COPY_IN_AUDIOINFO calling am_audio_set_info()", chptr);
		if (am_audio_set_info(chptr, tinfo, tinfo) == AUDIO_FAILURE) {
			error = EINVAL;
			goto nack;
		}

		/* update the played sample count */
		am_fix_info(chptr, tinfo, ad_infop, chpptr);

		/*
		 * Since there wasn't an error we were successful, now return
		 * the updated structure.
		 *
		 * Reuse the cq_private buffer, saving data for M_IOCDATA
		 * processing.
		 */
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_OUT_AUDIOINFO2;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*tinfo);
		cqp->cq_addr = (caddr_t)cmd->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/*
		 * We reuse the buffer with the audio_info_t structure
		 * already in place.
		 */
		mp->b_cont->b_wptr = (unsigned char *)tinfo + sizeof (*tinfo);

		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_AUDIOINFO returning", chptr);

		return (0);

		}

		/* end COPY_IN_AUDIOINFO */

	case COPY_IN_DIAG_LOOPB:	/* AUDIO_DIAG_LOOPBACK */
		ATRACE("am_wiocdata() COPY_IN_DIAG_LOOPB", chptr);
		if (csp->cp_cmd != AUDIO_DIAG_LOOPBACK) {
			error = EINVAL;
			goto nack;
		}

		if (*(int *)mp->b_cont->b_rptr) {
			ATRACE_32("am_wiocdata() COPY_IN_DIAG_LOOPB enable",
				*(int *)mp->b_cont->b_rptr);
			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    AMAD_SET_CONFIG_BOARD, AMAD_SET_DIAG_MODE,
			    NULL, 1, NULL) == AUDIO_FAILURE) {
				ATRACE("am_wiocdata() "
				    "COPY_IN_DIAG_LOOPB enable failed", 0);
				error = EINVAL;
				goto nack;
			}
		} else {
			ATRACE_32("am_wiocdata() COPY_IN_DIAG_LOOPB disable",
			    *(int *)mp->b_cont->b_rptr);

			if (ad_infop->ad_entry->ad_set_config(dev_instance,
			    AMAD_SET_CONFIG_BOARD, AMAD_SET_DIAG_MODE,
			    NULL, 0, NULL) == AUDIO_FAILURE) {
				ATRACE("am_wiocdata() "
				    "COPY_IN_DIAG_LOOPB disable failed", 0);
				error = EINVAL;
				goto nack;
			}
		}
		goto ack;

	case COPY_OUT_GETDEV:		/* AUDIO_GETDEV */
		ATRACE("am_wiocdata() COPY_OUT_GETDEV", chptr);
		if (csp->cp_cmd != AUDIO_GETDEV) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_SAMP_RATES:	/* AUDIO_MIXER_GET_SAMPLE_RATES */
		ATRACE("am_wiocdata() COPY_OUT_SAMP_RATES", chptr);
		if (csp->cp_cmd != AUDIO_MIXER_GET_SAMPLE_RATES) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_IN_SAMP_RATES: {	/* AUDIO_MIXER_GET_SAMPLE_RATES */
		am_sample_rates_t	*new;
		size_t			size;

		/*
		 * We copy in just the am_sample_rates_t structure to get the
		 * number of sample rates the samp_rates array can support.
		 * Once we know this we make another call to get the whole
		 * thing.
		 */
		ATRACE("am_wiocdata() COPY_IN_SAMP_RATES", chptr);

		if (csp->cp_cmd != AUDIO_MIXER_GET_SAMPLE_RATES) {
			error = EINVAL;
			goto nack;
		}

		/* get a pointer to the user supplied structure */
		new = (am_sample_rates_t *)mp->b_cont->b_rptr;

		/* make sure the number of array elements is sane */
		if (new->num_samp_rates <= 0) {
			ATRACE_32("am_wiocdata() COPY_IN_SAMP_RATES "
			    "bad num_samp_rates", new->num_samp_rates);
			error = EINVAL;
			goto nack;
		}

		size = AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(new->num_samp_rates);

		/*
		 * Now that we know the number of array elements, we can ask
		 * for the right number of bytes.
		 *
		 * Reuse the cq_private buffer, saving data for
		 * M_IOCDATA processing.
		 */
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_IN_SAMP_RATES2;
		cqp->cq_flag = 0;
		cqp->cq_size = size;
		cqp->cq_addr = (caddr_t)cmd->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_SAMP_RATES returning",
		    chptr);

		return (0);

		}

		/* end COPY_IN_SAMP_RATES */

	case COPY_IN_SAMP_RATES2: {	/* AUDIO_MIXER_GET_SAMPLE_RATES */
		am_sample_rates_t	*new;
		am_ad_sample_rates_t	*src;
		size_t			size;
		int			i;
		int			num;

		/*
		 * We now have the whole array, so we can fill it in and
		 * copy it out.
		 */
		ATRACE("am_wiocdata() COPY_IN_SAMP_RATES2", chptr);

		if (csp->cp_cmd != AUDIO_MIXER_GET_SAMPLE_RATES) {
			error = EINVAL;
			goto nack;
		}

		/* get a pointer to the user supplied structure */
		new = (am_sample_rates_t *)mp->b_cont->b_rptr;

		/* make sure the number of array elements is sane */
		if (new->num_samp_rates <= 0) {
			ATRACE_32("am_wiocdata() COPY_IN_SAMP_RATES2 "
			    "bad num_samp_rates", new->num_samp_rates);
			error = EINVAL;
			goto nack;
		}

		size = AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(new->num_samp_rates);

		ATRACE_32("am_wiocdata() COPY_IN_SAMP_RATES2 type", new->type);
		if (new->type == AUDIO_PLAY) {
			if (mode == AM_MIXER_MODE) {
				src = &ad_infop->ad_play.ad_mixer_srs;
			} else {
				src = &ad_infop->ad_play.ad_compat_srs;
			}
		} else if (new->type == AUDIO_RECORD) {
			if (mode == AM_MIXER_MODE) {
				src = &ad_infop->ad_record.ad_mixer_srs;
			} else {
				src = &ad_infop->ad_record.ad_compat_srs;
			}
		} else {
			error = EINVAL;
			goto nack;
		}

		/* figure out how many sample rates we have */
		for (num = 0; src->ad_srs[num] != 0; num++);

		/* we don't copy more sample rates than we have */
		if (num < new->num_samp_rates) {
			new->num_samp_rates = num;
		}

		/* we reuse the buffer we got from user space */
		for (i = 0; i < new->num_samp_rates; i++) {
			/* get sample rate for array elements */
			if (src->ad_srs[i] == 0) {
				/* at the end of sample rates */
				break;
			}
			new->samp_rates[i] = src->ad_srs[i];
		}

		/* let the app know there are more */
		if (num > new->num_samp_rates) {
			new->num_samp_rates = num;
		}

		/* type remains the same, but update others */
		if (ad_infop->ad_play.ad_flags & MIXER_CAP_FLAG_SR_LIMITS) {
			new->flags = MIXER_SR_LIMITS;
		}

		/*
		 * Ready to send the filled in structure back.
		 *
		 * Reuse the cq_private buffer, saving data for
		 * M_IOCDATA processing.
		 */
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_OUT_SAMP_RATES;
		cqp->cq_flag = 0;
		cqp->cq_size = size;
		cqp->cq_addr = (caddr_t)cmd->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_SAMP_RATES2 returning",
		    chptr);

		return (0);

		}

		/* end COPY_IN_SAMP_RATES2 */

	case COPY_OUT_MIXCTLINFO:	/* AUDIO_MIXERCTL_GET/SETINFO */
		ATRACE("am_wiocdata() COPY_OUT_MIXCTLINFO", chptr);
		if (csp->cp_cmd != AUDIO_MIXERCTL_GETINFO &&
		    csp->cp_cmd != AUDIO_MIXERCTL_SETINFO) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_IN_MIXCTLINFO: {	/* AUDIO_MIXERCTL_SETINFO */
		am_control_t		*new;
		audio_ch_t		ch;
		audio_info_t		ninfo;
		audio_info_t		*tinfo;

		ATRACE("am_wiocdata() COPY_IN_MIXCTLINFO", chptr);

		if (csp->cp_cmd != AUDIO_MIXERCTL_SETINFO) {
			error = EINVAL;
			goto nack;
		}

		/* get a pointer to the user supplied structure */
		new = (am_control_t *)mp->b_cont->b_rptr;
		tinfo = &new->dev_info;

		AUDIO_INIT(&ninfo, sizeof (ninfo));
		ninfo.play.gain = tinfo->play.gain;
		ninfo.play.balance = tinfo->play.balance;
		ninfo.play.port = tinfo->play.port;
		ninfo.play.pause = tinfo->play.pause;
		ninfo.record.gain = tinfo->record.gain;
		ninfo.record.balance = tinfo->record.balance;
		ninfo.record.port = tinfo->record.port;
		ninfo.record.pause = tinfo->record.pause;
		ninfo.monitor_gain = tinfo->monitor_gain;
		ninfo.output_muted = tinfo->output_muted;

		/* we always create a pseudo channel to points to the h/w */
		bcopy(chptr, &ch, sizeof (*chptr));
		ch.ch_info.info = hw_info;

		/* too ugly to check here, so send to a utility routine */
		ATRACE("am_wiocdata() COPY_IN_MIXCTLINFO calling "
		    "am_audio_set_info()", chptr);
		if (am_audio_set_info(&ch, &ninfo, &ninfo) == AUDIO_FAILURE) {
			ATRACE("am_wiocdata() am_audio_set_info() failed",
			    chptr);
			error = EINVAL;
			goto nack;
		}

		/* since there wasn't an error we succeeded, so return struct */
		tinfo->play.gain = ninfo.play.gain;
		tinfo->play.balance = ninfo.play.balance;
		tinfo->play.port = ninfo.play.port;
		tinfo->play.pause = ninfo.play.pause;
		tinfo->record.gain = ninfo.record.gain;
		tinfo->record.balance = ninfo.record.balance;
		tinfo->record.port = ninfo.record.port;
		tinfo->record.pause = ninfo.record.pause;
		tinfo->monitor_gain = ninfo.monitor_gain;
		tinfo->output_muted = ninfo.output_muted;

		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_OUT_MIXCTLINFO;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*new);
		cqp->cq_addr = (caddr_t)cmd->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* we reuse the buffer with the am_contro_t struct in place */
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (*new);

		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_MIXCTLINFO returning", chptr);

		return (0);

		}

		/* end COPY_IN_MIXCTLINFO */

	case COPY_OUT_MIXCTL_CHINFO: {	/* AUDIO_MIXERCTL_GET/SET_CHINFO */
		ATRACE("am_wiocdata() COPY_OUT_MIXCTL_CHINFO", chptr);

		chpptr = chptr->ch_private;

		if (csp->cp_cmd != AUDIO_MIXERCTL_GET_CHINFO &&
		    csp->cp_cmd != AUDIO_MIXERCTL_SET_CHINFO) {
			error = EINVAL;
			goto nack;
		}

		/*
		 * we've copied out the main part of the audio_channel_t
		 * structure, now do the info part.
		 */

		/* try to reuse the buffer, if we have one and big enough */
		if (mp->b_cont == NULL || (mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) < sizeof (audio_info_t)) {
			if (mp->b_cont) {
				freemsg(mp->b_cont);
			}
			mp->b_cont = (struct msgb *)allocb(
			    sizeof (audio_info_t), BPRI_HI);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				goto nack;
			}
		}

		/*
		 * Reuse the cq_private buffer, saving data for
		 * M_IOCDATA processing.
		 */
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_OUT_MIXCTL_CHINFO2;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (audio_info_t);
		cqp->cq_addr = (caddr_t)cmd->address2;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		bcopy(chpptr->ioctl_tmp_info, mp->b_cont->b_rptr,
		    sizeof (audio_info_t));

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (audio_info_t);

		/* we don't need these any more */
		ASSERT(chpptr->ioctl_tmp_info != hw_info);
		kmem_free(chpptr->ioctl_tmp_info,
		    sizeof (*chpptr->ioctl_tmp_info));
		chpptr->ioctl_tmp_info = 0;
		ASSERT(chpptr->ioctl_size);
		kmem_free(chpptr->ioctl_tmp, chpptr->ioctl_size);
		chpptr->ioctl_tmp = NULL;

		qreply(q, mp);
		return (0);

		}

		/* end COPY_OUT_MIXCTL_CHINFO */

	case COPY_OUT_MIXCTL_CHINFO2:	/* AUDIO_MIXERCTL_GET/SET_CHINFO */
		ATRACE("am_wiocdata() COPY_OUT_MIXCTL_CHINFO2", chptr);
		if (csp->cp_cmd != AUDIO_MIXERCTL_GET_CHINFO &&
		    csp->cp_cmd != AUDIO_MIXERCTL_SET_CHINFO) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_IN_MIXCTL_CHINFO: {	/* AUDIO_MIXERCTL_GET/SET_CHINFO */
		STRUCT_HANDLE(audio_channel, audio_channel);

		/*
		 * am_wioctl() asked for the first part of an audio_channel_t
		 * structure. This gets it and then asks for the second part.
		 */
		ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO", chptr);

		ASSERT(chpptr->ioctl_tmp == 0);

		/* make sure we got here legally */
		ATRACE_32("am_wiocdata() COPY_IN_MIXCTL_CHINFO command",
		    csp->cp_cmd);
		if (csp->cp_cmd != AUDIO_MIXERCTL_SET_CHINFO &&
		    csp->cp_cmd != AUDIO_MIXERCTL_GET_CHINFO) {
			error = EINVAL;
			goto nack;
		}

		/* protect the state */
		mutex_enter(&chptr->ch_lock);

		/* double check to make sure no one snuck in */
		if (chpptr->ioctl_tmp) {
			/* we've got allocated memory, so we fail */
			ATRACE("am_wiocdata() "
			    "AUDIO_MIXERCTL_G/SET_CHINFO fail, busy", chptr);

			mutex_exit(&chptr->ch_lock);
			error = EBUSY;
			goto nack;
		}

		/*
		 * Allocate memory for the audio_channel_t structure, but not
		 * for the info structure. Unfortunately, this structure has
		 * pointers in it, so _ILP32 and _LP64 does make a difference.
		 * We did get the size back in am_wioctl().
		 */
		ASSERT(chpptr->ioctl_size);
		chpptr->ioctl_tmp = kmem_zalloc(chpptr->ioctl_size, KM_NOSLEEP);
		if (chpptr->ioctl_tmp == NULL) {
			ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO "
			    "kmem_zalloc() failed", 0);
			mutex_exit(&chptr->ch_lock);
			error = ENOMEM;
			goto nack;
		}

		/* copy the audio_channel_t structure in */
		bcopy(mp->b_cont->b_rptr, chpptr->ioctl_tmp,
		    chpptr->ioctl_size);

		/* get ready to do the model conversion */
		STRUCT_SET_HANDLE(audio_channel, iocbp->ioc_flag,
		    chpptr->ioctl_tmp);

		/*
		 * Make sure the size is good, fortunately audio_info_t
		 * doesn't have any pointers in it, so it's the same size,
		 * regardless of _ILP32 or _LP64.
		 */
		if (STRUCT_FGET(audio_channel, info_size) !=
		    sizeof (audio_info_t)) {
			ATRACE_32(
			    "am_wiocdata() COPY_IN_MIXCTL_CHINFO bad size",
			    STRUCT_FGET(audio_channel, info_size));
			mutex_exit(&chptr->ch_lock);
			error = EIO;
			goto nack;
		}

		/* make sure we have a buffer for the audio_info_t struct */
		if (STRUCT_FGETP(audio_channel, info) == NULL) {
			ATRACE("am_wiocdata() "
			    "COPY_IN_MIXCTL_CHINFO no info address", chptr);
			mutex_exit(&chptr->ch_lock);
			error = EIO;
			goto nack;
		}

		/* make sure the channel number is reasonable */
		if (STRUCT_FGET(audio_channel, ch_number) > max_chs) {
			ATRACE_32("am_wiocdata() "
			    "COPY_IN_MIXCTL_CHINFO bad ch num",
			    STRUCT_FGET(audio_channel, ch_number));
			mutex_exit(&chptr->ch_lock);
			error = EINVAL;
			goto nack;
		}

		/* we've got the memory allocated, so we free the lock */
		mutex_exit(&chptr->ch_lock);

		/*
		 * Reuse the cq_private buffer, saving data for further
		 * M_IOCDATA processing.
		 */
		/* keep address for later, so we now use address2 */
		cmd->address2 = STRUCT_FGETP(audio_channel, info);
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_IN_MIXCTL_CHINFO2;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (audio_info_t);
		cqp->cq_addr = (caddr_t)cmd->address2;
		mp->b_datap->db_type = M_COPYIN;

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO returning", chptr);

		return (0);

		}

		/* end COPY_IN_MIXCTL_CHINFO */

	case COPY_IN_MIXCTL_CHINFO2: {	/* AUDIO_MIXERCTL_GET/SET_CHINFO */
		am_ch_private_t		*tchpptr;
		STRUCT_HANDLE(audio_channel, audio_channel);

		/*
		 * This gets the 2nd part, the audio_info_t structure.
		 *
		 * It then either returns the requested data if it's for
		 * an AUDIO_MIXERCTL_GET_CHINFO ioctl, or it updates the
		 * channel if it's an AUDIO_MIXERCTL_SET_CHINFO ioctl.
		 * Fortunately we have a perfectly sized buffer to reuse
		 * for sending the channel info to the user.
		 *
		 * We don't need to lock the channel because the ioctl_tmp
		 * variable has memory allocated and no other ioctls for
		 * this channel can get here as long as this is true.
		 */

		boolean_t		set;
		audio_ch_t		*cptr;

		ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO2", chptr);

		/* make sure we've got something in ioctl_tmp */
		if (chpptr->ioctl_tmp == 0) {
			ATRACE("am_wiocdata(): no private ioctl data", chptr);
			cmn_err(CE_NOTE,
			    "mixer: wiocdata() no private ioctl data");
			error = EIO;
			goto nack;
		}

		/* see which ioctl we are */
		ATRACE_32("am_wiocdata() COPY_IN_MIXCTL_CHINFO2 command",
		    csp->cp_cmd);
		switch (csp->cp_cmd) {
		case AUDIO_MIXERCTL_SET_CHINFO:
			ATRACE_32("am_wiocdata() AUDIO_MIXERCTL_SET_CHINFO",
			    csp->cp_cmd);
			set = B_TRUE;
			break;
		case AUDIO_MIXERCTL_GET_CHINFO:
			ATRACE_32("am_wiocdata() AUDIO_MIXERCTL_GET_CHINFO",
			    csp->cp_cmd);
			set = B_FALSE;
			break;
		default:
			ATRACE_32("am_wiocdata() unrecognized command",
			    csp->cp_cmd);
			error = EINVAL;
			goto nack;
		}

		/* get a pointer to the user supplied structure, using models */
		STRUCT_SET_HANDLE(audio_channel, iocbp->ioc_flag,
		    chpptr->ioctl_tmp);

		/* make sure the channel number is reasonable */
		if (STRUCT_FGET(audio_channel, ch_number) >= max_chs) {
			ATRACE_32("am_wiocdata() channel to high",
			    STRUCT_FGET(audio_channel, ch_number));
			error = EINVAL;
			goto nack;
		}

		/* get a channel pointer */
		cptr =
		    &statep->as_channels[STRUCT_FGET(audio_channel, ch_number)];

		/* freeze the channels while we transfer data */
		mutex_enter(&cptr->ch_lock);
		/* get the private pointer */
		tchpptr = cptr->ch_private;

		/* make sure the channel is valid and is an AUDIO/AUDIOCTL ch */
		if (chptr->ch_info.pid == 0 || tchpptr == NULL ||
		    (cptr->ch_info.dev_type != AUDIO &&
		    cptr->ch_info.dev_type != AUDIOCTL) ||
		    !(tchpptr->flags & AM_CHNL_OPEN)) {
			mutex_exit(&cptr->ch_lock);
			ATRACE_32("am_wiocdata() bad device type or invalid",
			    cptr->ch_info.dev_type);
			error = EINVAL;
			goto nack;
		}

		/* allocate memory for the structure */
		chpptr->ioctl_tmp_info = kmem_zalloc(
		    sizeof (*chpptr->ioctl_tmp_info), KM_NOSLEEP);
		if (chpptr->ioctl_tmp_info == NULL) {
			ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO2 "
			    "kmem_zalloc() failed", 0);
			error = ENOMEM;
			goto nack;
		}

		if (set) {		/* set the channel */
			/* get the new info structure */
			bcopy(mp->b_cont->b_rptr, chpptr->ioctl_tmp_info,
			    sizeof (*chpptr->ioctl_tmp_info));

			/* too ugly to check here, so do elsewhere */
			ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO2 "
			    "calling am_audio_set_info()", chptr);
			if (am_audio_set_info(cptr, chpptr->ioctl_tmp_info,
			    chpptr->ioctl_tmp_info) == AUDIO_FAILURE) {
				mutex_exit(&cptr->ch_lock);
				error = EINVAL;
				goto nack;
			}
		} else {		/* has to be `get' */
			/* don't let anything change the struct */
			/* final sanity check */
			if (cptr->ch_info.info == NULL) {
				mutex_exit(&cptr->ch_lock);
				error = EIO;
				goto nack;
			}

			bcopy(cptr->ch_info.info, chpptr->ioctl_tmp_info,
			    sizeof (*chpptr->ioctl_tmp_info));
		}

		/* update the played sample count */
		am_fix_info(chptr, chpptr->ioctl_tmp_info, ad_infop, chpptr);

		/*
		 * Copy out the audio_channel_t structure first. Reuse the
		 * cq_private buffer, saving data for M_IOCDATA processing.
		 * Also, reuse the buffer, if big enough.
		 */
		if (mp->b_cont == NULL || (mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) < chpptr->ioctl_size) {
			if (mp->b_cont) {
				freemsg(mp->b_cont);
			}
			mp->b_cont = (struct msgb *)allocb(
					chpptr->ioctl_size, BPRI_HI);
			if (mp->b_cont == NULL) {
				mutex_exit(&cptr->ch_lock);
				error = ENOMEM;
				goto nack;
			}
		}

		/* we need to fill in information */
		ASSERT(STRUCT_FGET(audio_channel, ch_number) ==
		    cptr->ch_info.ch_number);
		STRUCT_FSET(audio_channel, pid, cptr->ch_info.pid);
		STRUCT_FSET(audio_channel, dev_type, cptr->ch_info.dev_type);
		STRUCT_FSET(audio_channel, info_size, cptr->ch_info.info_size);

		/* now the channels can be changed */
		mutex_exit(&cptr->ch_lock);

		ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO2 get", chptr);
		cqp->cq_private = (mblk_t *)cmd;
		/* M_IOCDATA command, same address */
		cmd->command = COPY_OUT_MIXCTL_CHINFO;
		cqp->cq_flag = 0;
		cqp->cq_size = chpptr->ioctl_size;
		cqp->cq_addr = (caddr_t)cmd->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/*
		 * We reuse the buffer with the audio_channel_t structure
		 * already in place.
		 */
		bcopy(STRUCT_BUF(audio_channel), mp->b_cont->b_rptr,
		    chpptr->ioctl_size);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("am_wiocdata() COPY_IN_MIXCTL_CHINFO2 return", chptr);

		return (0);

		}

		/* end COPY_IN_MIXCTL_CHINFO2 */

	case COPY_OUT_MIXCTL_MODE:	/* AUDIO_MIXERCTL_GET_MODE */
		ATRACE("am_wiocdata() COPY_OUT_MIXCTL_MODE", chptr);
		if (csp->cp_cmd != AUDIO_MIXERCTL_GET_MODE) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_IN_MIXCTL_MODE: {	/* AUDIO_MIXERCTL_SET_MODE */
		audio_ch_t	*pchptr = 0;
		audio_ch_t	*rchptr = 0;
		audio_ch_t	*tchptr;
		am_ch_private_t	*tchpptr;
		audio_info_t	*new_pinfo = 0;
		audio_info_t	*new_rinfo = 0;
		audio_info_t	old_hw_info;
		audio_info_t	*tinfo;
		int		i;
		int		new_mode;
		int		ppid = 0;
		int		rpid = 0;
#ifdef DEBUG
		int		pcount = 0;
		int		rcount = 0;
#endif

		ATRACE("am_wiocdata() COPY_IN_MIXCTL_MODE", chptr);

		if (csp->cp_cmd != AUDIO_MIXERCTL_SET_MODE) {
			error = EINVAL;
			goto nack;
		}

		/* get the new_mode and make sure it's good */
		new_mode = *((int *)mp->b_cont->b_rptr);
		ATRACE_32("am_wiocdata() new mode", new_mode);
		if (new_mode != AM_MIXER_MODE && new_mode != AM_COMPAT_MODE) {
			ATRACE_32("am_wiocdata() bad mode", new_mode);
			error = EINVAL;
			goto nack;
		}

		/* make sure we aren't going into the same mode */
		if (mode == new_mode) {
			goto ack;
		}

		/* we allocate this memory while it's easy to back out */
		if (new_mode == AM_MIXER_MODE &&
		    ad_infop->ad_codec_type == AM_TRAD_CODEC) {
			new_pinfo = kmem_alloc(sizeof (*new_pinfo), KM_NOSLEEP);
			if (new_pinfo == NULL) {
				ATRACE("am_wiocdata() "
				    "new_pinfo kmem_alloc() failed", 0);
				error = EINVAL;
				goto nack;
			}
			new_rinfo = kmem_alloc(sizeof (*new_rinfo), KM_NOSLEEP);
			if (new_rinfo == NULL) {
				ATRACE("am_wiocdata() "
				    "new_pinfo kmem_alloc() failed", 0);
				kmem_free(new_pinfo, sizeof (*new_pinfo));
				error = EINVAL;
				goto nack;
			}
		}

		/* keep the allocated channels stable */
		mutex_enter(&statep->as_lock);

		/* we disable all other ioctls while we change modes */
		mutex_enter(apm_infop->apm_swlock);

		if (stpptr->active_ioctls > 1) {
			mutex_exit(apm_infop->apm_swlock);
			mutex_exit(&statep->as_lock);
			if (new_pinfo) {
				kmem_free(new_pinfo, sizeof (*new_pinfo));
			}
			if (new_rinfo) {
				kmem_free(new_rinfo, sizeof (*new_rinfo));
			}
			error = EBUSY;
			goto nack;
		}

		/* get the AUDIO apm_info pointer, we've got one for AUDIOCTL */
		if ((apm_infop =
		    audio_sup_get_apm_info(statep, AUDIO)) == NULL) {
			mutex_exit(apm_infop->apm_swlock);
			mutex_exit(&statep->as_lock);
			if (new_pinfo) {
				kmem_free(new_pinfo, sizeof (*new_pinfo));
			}
			if (new_rinfo) {
				kmem_free(new_rinfo, sizeof (*new_rinfo));
			}
			error = EIO;
			goto nack;
		}

		/*
		 * Make sure we can go to COMPAT mode while we're locked. By
		 * definition if we are going to MIXER mode we can't have
		 * more than one in and one out channel allocated.
		 */
		if (new_mode == AM_COMPAT_MODE && (*apm_infop->apm_in_chs > 1 ||
		    *apm_infop->apm_out_chs > 1)) {
			ATRACE("am_wiocdata() COPY_IN_MIXCTL_MODE busy", chptr);
			stpptr->flags &= ~AM_PRIV_SW_MODES;
			mutex_exit(apm_infop->apm_swlock);
			mutex_exit(&statep->as_lock);
			ASSERT(new_pinfo == NULL);
			ASSERT(new_rinfo == NULL);
			error = EBUSY;
			goto nack;
#ifdef DEBUG
		} else {
			ASSERT(*apm_infop->apm_in_chs <= 1);
			ASSERT(*apm_infop->apm_out_chs <= 1);
#endif
		}

		/*
		 * Once playing/recording has stopped we can set this to 0
		 * without locking apm_infop->apm_swlock because there isn't
		 * any thread executing that will be looking at flags, except
		 * other ioctl calls. If there's a miss-read of flags while
		 * it is being reset to 0 (re-enabling ioctls) the worse that
		 * can happen is an ioctl will return EBUSY. But this saves us
		 * from having to do locks that aren't going to really do
		 * anything for us, but increase the risk of deadlock.
		 */
		stpptr->flags |= AM_PRIV_SW_MODES;

		/* how we do the switch is different based on the device */
		if (ad_infop->ad_codec_type == AM_MS_CODEC) {
			/* find the reading and writing channels */
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip non-AUDIO and unallocated channels */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    tchptr->ch_info.dev_type != AUDIO ||
				    tchptr->ch_info.pid == 0) {
					continue;
				}

				/* is this channel playing?, recording? */
				tchpptr = tchptr->ch_private;
				ATRACE("am_wiocdate() found ch flags",
				    chpptr->flags);
				if (tchpptr->writing) {
				    ATRACE_32("am_wiocdata() MS found play ch",
					tchptr->ch_info.ch_number);
				    ASSERT(pchptr == 0);
				    pchptr = tchptr;
#ifdef DEBUG
				    pcount++;
#endif
				}
				if (tchpptr->reading) {
				    ATRACE_32(
					"am_wiocdata() MS found record ch",
					tchptr->ch_info.ch_number);
				    ASSERT(rchptr == 0);
				    rchptr = tchptr;
#ifdef DEBUG
				    rcount++;
#endif
				}

				/* are we done finding active channels? */
				if (pchptr && rchptr) {
				    ASSERT(pchptr->ch_apm_infop ==
					rchptr->ch_apm_infop);
				    ASSERT(pchptr->ch_info.dev_type ==
					rchptr->ch_info.dev_type);
				    ASSERT(pchptr->ch_apm_infop->apm_private ==
					rchptr->ch_apm_infop->apm_private);
#ifndef DEBUG
				    break;
#endif
				}
			}
			ASSERT(pcount <= 1);
			ASSERT(rcount <= 1);

			mutex_exit(apm_infop->apm_swlock);

			/* pause playing & recording, so the ISR isn't called */
			ATRACE("am_wiocdata() pause play", chptr);
			tinfo = pchptr->ch_info.info;
			ad_infop->ad_entry->ad_pause_play(dev_instance,
			    pchptr->ch_info.ch_number);
			tinfo->play.active = 0;
			hw_info->play.active = 0;
			tinfo->play.pause = 1;
			ATRACE("am_wiocdata() stop record", chptr);
			ad_infop->ad_entry->ad_stop_record(dev_instance,
			    rchptr->ch_info.ch_number);
			tinfo->record.active = 0;
			hw_info->record.active = 0;
			tinfo->record.pause = 0;

			/*
			 * Multi-stream Codecs already use the virtual channel
			 * configuration to set the hardware, so this is a
			 * trivial change to make. Everything is halted so we
			 * we don't need to lock this.
			 */
			if (new_mode == AM_COMPAT_MODE) {
				ad_infop->ad_mode = AM_COMPAT_MODE;
			} else {
				ASSERT(new_mode == AM_MIXER_MODE);
				ad_infop->ad_mode = AM_MIXER_MODE;
			}

			ATRACE("am_wiocdata() AM_MS_CODEC switch done",
				ad_infop);

		} else {
			ASSERT(ad_infop->ad_codec_type == AM_TRAD_CODEC);

			mutex_exit(apm_infop->apm_swlock);

			/* wait for playing to end */
			mutex_enter(&stpptr->lock);

			while (hw_info->play.active) {
				ATRACE_32("am_wiocdata() wait to stop playing",
				    hw_info->play.active);
				if (cv_wait_sig(&stpptr->cv,
				    &stpptr->lock) <= 0) {

				    ATRACE("am_wiocdata() signal interrupt",
					hw_info->play.active);

				    /* turn on ioctls again */
				    stpptr->flags &= ~AM_PRIV_SW_MODES;

				    mutex_exit(&stpptr->lock);

				    /* we are bailing, so we need to restart */
				    am_restart(statep, hw_info);

				    mutex_exit(&statep->as_lock);

				    if (new_mode == AM_MIXER_MODE) {
					kmem_free(new_pinfo,
					    sizeof (*new_pinfo));
					kmem_free(new_rinfo,
					    sizeof (*new_rinfo));
				    }

				    stpptr->flags &= ~AM_PRIV_SW_MODES;
				    error = EINTR;
				    goto nack;
				}
				ATRACE("am_wiocdata() signal returned normally",
				    0);
			}
			mutex_exit(&stpptr->lock);

			/*
			 * Now we shutdown the record channel, if active.
			 * We have to lock to make this call.
			 */
			if (hw_info->record.active) {
				ad_infop->ad_entry->ad_stop_record(dev_instance,
				    chptr->ch_info.ch_number);
				hw_info->record.active = 0;
			}

			/*
			 * Wait as long as possible to save the old state for
			 * later.
			 */
			bcopy(hw_info, &old_hw_info, sizeof (*hw_info));

			/* find the play and record channels */
			ASSERT(pcount == 0);
			ASSERT(rcount == 0);
			ASSERT(pchptr == 0);
			ASSERT(rchptr == 0);
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {
				/* skip non-AUDIO and unallocated channels */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    tchptr->ch_info.dev_type != AUDIO ||
				    tchptr->ch_info.pid == 0) {

				    continue;
				}

				/* is this channel playing?, recording? */
				chpptr = tchptr->ch_private;
				ATRACE("am_wiocdate() found ch flags",
				    chpptr->flags);
				if (chpptr->writing) {
				    ATRACE_32("am_wiocdate() T found play ch",
					tchptr->ch_info.ch_number);
				    pchptr = tchptr;
#ifdef DEBUG
				    pcount++;
#endif
				}
				if (chpptr->reading) {
				    ATRACE_32("am_wiocdate() T found record ch",
					tchptr->ch_info.ch_number);
				    rchptr = tchptr;
#ifdef DEBUG
				    rcount++;
#endif
				}

				/* are we done finding active channels? */
				if (pchptr && rchptr) {
				    ASSERT(pchptr->ch_apm_infop ==
				    rchptr->ch_apm_infop);
				    ASSERT(pchptr->ch_info.dev_type ==
					rchptr->ch_info.dev_type);
				    ASSERT(pchptr->ch_apm_infop->apm_private ==
				    rchptr->ch_apm_infop->apm_private);
#ifndef DEBUG
				    break;
#endif
				}
			}
			ASSERT(pcount <= 1);
			ASSERT(rcount <= 1);

			if (new_mode == AM_MIXER_MODE) {
				if (am_set_mixer_mode(chptr, ad_infop, stpptr,
				    pchptr, rchptr) == AUDIO_FAILURE) {
					/* turn on ioctls again */
					stpptr->flags &= ~AM_PRIV_SW_MODES;

					am_restart(statep, hw_info);

					mutex_exit(&statep->as_lock);

					if (new_pinfo) {
						kmem_free(new_pinfo,
						    sizeof (*new_pinfo));
					}
					if (new_rinfo) {
						kmem_free(new_rinfo,
						    sizeof (*new_rinfo));
					}

					error = EIO;
					goto nack;
				}
			} else {
				if (am_set_compat_mode(chptr, ad_infop,
				    pchptr, rchptr) == AUDIO_FAILURE) {
					/* turn on ioctls again */
					stpptr->flags &= ~AM_PRIV_SW_MODES;

					am_restart(statep, hw_info);

					mutex_exit(&statep->as_lock);

					ASSERT(new_pinfo == 0);
					ASSERT(new_rinfo == 0);

					error = EIO;
					goto nack;
				}
			}
		}
		ASSERT(mutex_owned(&statep->as_lock));

		/* we're in a new mode now, so fix the play/rec info pointers */
		hw_info = &stpptr->hw_info;
		if (new_mode == AM_MIXER_MODE) {
			int	tpid;
			/* rebuild info structures and get pids */
			if (pchptr && pchptr == rchptr) {
				/* this is play/record channel */
				ppid = pchptr->ch_info.pid;
				rpid = ppid;

				tinfo = new_pinfo;
				pchptr->ch_info.info = tinfo;

				kmem_free(new_rinfo, sizeof (*new_rinfo));
				new_rinfo = 0;

				/* copy in the old play state */
				bcopy(&old_hw_info.play, &tinfo->play,
				    (2 * sizeof (old_hw_info.play)));

				/* copy the current dev state */
				tinfo->monitor_gain = hw_info->monitor_gain;
				tinfo->output_muted = hw_info->output_muted;
				tinfo->hw_features = hw_info->hw_features;
				tinfo->sw_features = hw_info->sw_features;
				tinfo->sw_features_enabled =
				    hw_info->sw_features_enabled;
				tinfo->ref_cnt = 1;
			} else {
				/* play or record channels */
				ASSERT(pchptr != rchptr ||
				    (pchptr == 0 && rchptr == 0));
				if (pchptr) {
				    ppid = pchptr->ch_info.pid;

				    tinfo = new_pinfo;
				    AUDIO_INIT(tinfo, sizeof (*tinfo));
				    pchptr->ch_info.info = tinfo;

				    /* copy in the old play state */
				    bcopy(&old_hw_info.play, &tinfo->play,
					sizeof (tinfo->play));
				    /* copy the current dev state */
				    tinfo->monitor_gain = hw_info->monitor_gain;
				    tinfo->output_muted = hw_info->output_muted;
				    tinfo->hw_features = hw_info->hw_features;
				    tinfo->sw_features = hw_info->sw_features;
				    tinfo->sw_features_enabled =
					hw_info->sw_features_enabled;
				    tinfo->ref_cnt = 1;
				} else {
				    kmem_free(new_pinfo, sizeof (*new_pinfo));
				    new_pinfo = 0;
				}
				if (rchptr) {
				    rpid = rchptr->ch_info.pid;

				    tinfo = new_rinfo;
				    AUDIO_INIT(tinfo, sizeof (*tinfo));
				    rchptr->ch_info.info = tinfo;

				    /* copy in the old record state */
				    bcopy(&old_hw_info.record,
					&tinfo->record,
					sizeof (old_hw_info.record));
				    /* copy the current dev state */
				    tinfo->monitor_gain = hw_info->monitor_gain;
				    tinfo->output_muted = hw_info->output_muted;
				    tinfo->hw_features = hw_info->hw_features;
				    tinfo->sw_features = hw_info->sw_features;
				    tinfo->sw_features_enabled =
					hw_info->sw_features_enabled;
				    tinfo->ref_cnt = 1;
				} else {
				    kmem_free(new_rinfo, sizeof (*new_rinfo));
				    new_rinfo = 0;
				}
			}

			/* start over with the reference count */
			hw_info->ref_cnt = 1;

			/* find AUDIOCTL channels to assoc. with AUDIO chs */
			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip if not AUDIOCTL or allocated chs */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    tchptr->ch_info.dev_type != AUDIOCTL ||
				    tchptr->ch_info.pid == 0) {
					continue;
				}

				ASSERT(hw_info == tchptr->ch_info.info);

				tpid = tchptr->ch_info.pid;

				if (ppid && tpid == ppid) {
					tchptr->ch_info.info = new_pinfo;
					new_pinfo->ref_cnt++;
				} else if (rpid && tpid == rpid) {
					tchptr->ch_info.info = new_rinfo;
					new_rinfo->ref_cnt++;
				} else {
					tchptr->ch_info.info = hw_info;
					hw_info->ref_cnt++;
				}
			}

			/* now we need to re-initialize the src structures */
			mutex_enter(&chpptr->src_lock);
			if (pchptr) {
				ATRACE("am_wiocdata() calling play src init",
					pchptr->ch_private);
				if (ad_infop->ad_play.ad_conv->ad_src_init(
					pchptr, AUDIO_PLAY) == AUDIO_FAILURE) {
					/* how do we recover? see below */
					stpptr->flags &= ~AM_PRIV_SW_MODES;

					mutex_exit(&statep->as_lock);
					mutex_exit(&chpptr->src_lock);

					cmn_err(CE_NOTE, "mixer: wiocdata() "
						"play mixer mode failure");

					kmem_free(new_pinfo,
						sizeof (*new_pinfo));
					new_pinfo = 0;
					if (new_rinfo) {
						kmem_free(new_rinfo,
						    sizeof (*new_rinfo));
						new_rinfo = 0;
					}
					ad_infop->ad_mode = AM_COMPAT_MODE;
					hw_info->sw_features_enabled &=
					    ~AUDIO_SWFEATURE_MIXER;
					error = EIO;
					goto nack;
				}
			}
			if (rchptr) {
				ATRACE("am_wiocdata() calling rec src init",
				    rchptr->ch_private);
				if (ad_infop->ad_record.ad_conv->ad_src_init(
				    rchptr, AUDIO_RECORD) == AUDIO_FAILURE) {
					/*
					 * XXX how do we recover? We should hang
					 * and clear on close. We can test by
					 * setting AUDIO_RECORD above to PLAY.
					 */
					stpptr->flags &= ~AM_PRIV_SW_MODES;

					mutex_exit(&statep->as_lock);
					mutex_exit(&chpptr->src_lock);

					cmn_err(CE_NOTE, "mixer: wiocdata() "
						"record mixer mode failure");
					kmem_free(new_rinfo,
					    sizeof (*new_rinfo));
					new_rinfo = 0;
					if (new_pinfo) {
						kmem_free(new_pinfo,
						    sizeof (*new_pinfo));
						new_pinfo = 0;
					}
					ad_infop->ad_mode = AM_COMPAT_MODE;
					hw_info->sw_features_enabled &=
					    ~AUDIO_SWFEATURE_MIXER;
					error = EIO;
					goto nack;
				}
			}
			mutex_exit(&chpptr->src_lock);
		} else {
			/* start over with the reference count */
			hw_info->ref_cnt = 1;

			for (i = 0, tchptr = &statep->as_channels[0];
			    i < max_chs; i++, tchptr++) {

				/* skip if ! AUDIO, AUDIOCTL or allocated chs */
				if (!(tchptr->ch_flags &
				    AUDIO_CHNL_ALLOCATED) ||
				    (tchptr->ch_info.dev_type != AUDIO &&
				    tchptr->ch_info.dev_type != AUDIOCTL) ||
				    tchptr->ch_info.pid == 0) {
					continue;
				}

				/* see if we need to free the info structure */
				tinfo = tchptr->ch_info.info;
				if (tinfo == hw_info) {
				    /* already set to hw, so inc ref cnt */
				    ASSERT(tchptr->ch_info.info == hw_info);
				    hw_info->ref_cnt++;
				} else {
				    /* not set to hw */
				    ASSERT(tchptr->ch_info.info != hw_info);
				    if (tinfo->ref_cnt == 1) {
					/* not set to hw, and only ref so clr */
					kmem_free(tinfo, sizeof (*tinfo));
				    } else {
					/* someone else has a link to it */
					tinfo->ref_cnt--;
				    }
				    /* now set to hardware */
				    tchptr->ch_info.info = hw_info;
				    hw_info->ref_cnt++;
				}
			}
		}

		/* we don't need this flag anymore, re-enabling ioctls */
		stpptr->flags &= ~AM_PRIV_SW_MODES;

		/* we're in the new mode, so restart I/O */
		am_restart(statep, hw_info);

		/* if we have blocked processes they should unblock */
		if (new_mode == AM_MIXER_MODE) {
			cv_broadcast(&statep->as_cv);
		}

		mutex_exit(&statep->as_lock);

		am_send_signal(statep);

		goto ack;

		}

		/* end COPY_IN_MIXCTL_MODE */

	default:	/* see if we have an entry pt in the Audio Driver */
		if (ad_infop->ad_entry->ad_iocdata) {
			/* we do, so call it */
			ATRACE("am_wiocdata(): "
			    "calling Audio Driver iocdata() routine",
			    ad_infop->ad_entry);
			switch (ad_infop->ad_entry->ad_iocdata(dev_instance,
			    chptr->ch_info.ch_number, q, mp)) {
			case AM_WIOCDATA:	return (0);
			case AM_ACK:		goto ack;
			case AM_NACK:
				/*FALLTHROUGH*/
			default:		goto nack;
			}
		}

		/* no - we're a driver, so we nack unrecognized iocdata cmds */
		ATRACE("am_wiocdata() default", chptr);
		error = EINVAL;
		goto nack;
	}

	/* the switch above should never reach here */

ack:
	ATRACE("am_wiocdata() ack", chptr);

	ASSERT(chpptr->ioctl_tmp == 0);
	ASSERT(chpptr->ioctl_tmp_info == 0);

	/* the ioctl is done, so decrement the active count */
	mutex_enter(apm_infop->apm_swlock);
	stpptr->active_ioctls--;
	mutex_exit(apm_infop->apm_swlock);

	ATRACE("am_wiocdata() ack2", chptr);
	if (csp->cp_private) {
		kmem_free(csp->cp_private, sizeof (audio_i_state_t));
		csp->cp_private = NULL;
	}
	if (cqp->cq_private) {
		kmem_free(cqp->cq_private, sizeof (audio_i_state_t));
		cqp->cq_private = NULL;
	}
	iocbp->ioc_rval = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_error = 0;		/* just in case */
	mp->b_wptr = mp->b_rptr + sizeof (*iocbp);
	mp->b_datap->db_type = M_IOCACK;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = 0;
	}
	qreply(q, mp);


	ATRACE("am_wiocdata() returning success", chptr);

	return (0);

nack:
	ATRACE("am_wiocdata() nack1", chptr);

	/* we could have been in the middle of and ioctl that failed */
	if (chpptr->ioctl_tmp_info) {
		ASSERT(chpptr->ioctl_tmp_info != hw_info);
		kmem_free(chpptr->ioctl_tmp_info,
		    sizeof (*chpptr->ioctl_tmp_info));
		chpptr->ioctl_tmp_info = 0;
	}
	if (chpptr->ioctl_tmp) {
		kmem_free(chpptr->ioctl_tmp, chpptr->ioctl_size);
		chpptr->ioctl_tmp = NULL;
	}

	/* the ioctl is done, so decrement the active count */
	mutex_enter(apm_infop->apm_swlock);
	stpptr->active_ioctls--;
	mutex_exit(apm_infop->apm_swlock);

	ATRACE("am_wiocdata() nack2", chptr);
	if (csp->cp_private) {
		kmem_free(csp->cp_private, sizeof (audio_i_state_t));
		csp->cp_private = NULL;
	}
	if (cqp->cq_private) {
		kmem_free(cqp->cq_private, sizeof (audio_i_state_t));
		cqp->cq_private = NULL;
	}
	iocbp->ioc_rval = -1;
	iocbp->ioc_count = 0;
	iocbp->ioc_error = error;
	mp->b_wptr = mp->b_rptr + sizeof (*iocbp);
	mp->b_datap->db_type = M_IOCNAK;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = 0;
	}
	qreply(q, mp);

	ATRACE("am_wiocdata() returning failure", chptr);

	return (0);

}	/* am_wiocdata() */

/*
 * am_wioctl()
 *
 * Description:
 *	This routine is called by am_wsvc() to process all M_IOCTL
 *	messages.
 *
 *	We only support transparent ioctls. Since this is a driver we
 *	nack unrecognized ioctls.
 *
 *	This routine also is used to return a IOCNAK if the state pointer
 *	or the channel pointer, set up in am_wsvc(), are invalid.
 *
 *	The following ioctls are supported:
 *		AUDIO_DIAG_LOOPBACK		special diagnostics mode
 *		AUDIO_DRAIN
 *		AUDIO_GETINFO
 *		AUDIO_SETINFO
 *		AUDIO_GETDEV
 *		AUDIO_MIXER_MULTIPLE_OPEN
 *		AUDIO_MIXER_SINGLE_OPEN
 *		AUDIO_MIXER_GET_SAMPLE_RATES
 *		AUDIO_MIXERCTL_GETINFO
 *		AUDIO_MIXERCTL_SETINFO
 *		AUDIO_MIXERCTL_GET_CHINFO
 *		AUDIO_MIXERCTL_SET_CHINFO
 *		AUDIO_MIXERCTL_GET_MODE
 *		AUDIO_MIXERCTL_SET_MODE
 *		unknown		call Audio Driver ioctl() routine
 *
 *	WARNING: There cannot be any locks owned by calling routines.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the STREAMS queue
 *	mblk_t		*mp	Pointer to the message block
 *	audio_ch_t	*chptr	Pointer to this channel's state information
 *
 * Returns:
 *	0			Always returns a 0, becomes a return for
 *				am_wsvc()
 */
static int
am_wioctl(queue_t *q, mblk_t *mp, audio_ch_t *chptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;
	am_ad_info_t		*ad_infop = apm_infop->apm_ad_infop;
	am_ch_private_t		*chpptr = chptr->ch_private;
	am_apm_private_t	*stpptr = apm_infop->apm_private;
	audio_info_t		*info = chptr->ch_info.info;
	struct iocblk		*iocbp;
	struct copyreq		*cqp;
	audio_i_state_t		*state = NULL;
	uint_t			*flags = &chpptr->flags;
	audio_device_type_e	type = chptr->ch_info.dev_type;
	int			command;
	int			error;
	int			max_chs = statep->as_max_chs;
	int			mode = ad_infop->ad_mode;

	ATRACE("in am_wioctl()", chptr);
	ATRACE_32("am_wioctl() channel type", chptr->ch_info.dev_type);

	ASSERT(!mutex_owned(apm_infop->apm_swlock));

	iocbp = (struct iocblk *)mp->b_rptr;	/* pointer to ioctl info */
	cqp = (struct copyreq *)mp->b_rptr;	/* set up copyreq ptr */

	command = iocbp->ioc_cmd;

	/* we count active ioctls to control switching modes, if not DRAIN */
	if (command != AUDIO_DRAIN) {
		mutex_enter(apm_infop->apm_swlock);
		/* nack decs, so we need to inc first */
		stpptr->active_ioctls++;
		if (stpptr->flags & AM_PRIV_SW_MODES) {
			ATRACE("am_wioctl() no ioctls allowed", stpptr);
			mutex_exit(apm_infop->apm_swlock);
			error = EBUSY;
			goto nack;
		}
		mutex_exit(apm_infop->apm_swlock);
	}

	/* make sure we have good pointers */
	if (statep == NULL || chptr == NULL) {
		ATRACE("am_wioctl() no statep or chptr", statep);
		error = EINVAL;
		goto nack;
	}

	/* make sure this is a transparent ioctl */
	if (iocbp->ioc_count != TRANSPARENT) {
		ATRACE_32("am_wioctl() not TRANSPARENT", iocbp->ioc_count);
		error = EINVAL;
		goto nack;
	}

	/* get a buffer for priv. data, but only if this isn't an AUDIO_DRAIN */
	if (command != AUDIO_DRAIN) {
		if ((state = kmem_zalloc(sizeof (*state), KM_NOSLEEP)) ==
		    NULL) {
			ATRACE("am_wioctl() state kmem_zalloc() failed", 0);
			error = ENOMEM;
			goto nack;
		}
	}

	ATRACE_32("am_wioctl() command", iocbp->ioc_cmd);
	switch (command) {

	case AUDIO_DIAG_LOOPBACK:
		ATRACE("am_wioctl() AUDIO_DIAG_LOOPBACK", chptr);

		/* we don't limit this ioctl to either mode or device */

		/* not all Audio Drivers and their hardware support loopbacks */
		if (ad_infop->ad_diag_flags & AM_DIAG_INTERNAL_LOOP) {
			ATRACE_32(
			    "am_wioctl() AUDIO_DIAG_LOOPBACK no loopbacks",
			    ad_infop->ad_diag_flags);
			error = EINVAL;
			goto nack;
		}

		/* get the mode, TRUE == on, FALSE == off */

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_DIAG_LOOPB;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_DIAG_LOOPBACK returning", chptr);

		return (0);

		/* end AUDIO_SETINFO */
	case AUDIO_DRAIN: {
		/*
		 * There are three ways for the DRAIN to be satisfied,
		 * 1. This is a DRAIN before the first M_DATA message, in
		 *	which case the channel isn't active. If not active
		 *	then we can go ahead and ack. Or, there isn't anything
		 *	queued up, so again we ack.
		 * 2. The DRAIN (M_BREAK) message happens after the last
		 *	M_DATA package, or at least before any more arrive,
		 *	again, the channel isn't active. This is detected
		 *	in am_get_samples(), where the cv_wait_sig() is
		 *	signaled.
		 * 3. The high runner case is the interrupt following the
		 *	interrupt where the M_BREAK message is processed and
		 *	the AM_CHNL_DRAIN_NEXT_INT flag is set. This is
		 *	cleared and dealt with at the top of am_get_samples()
		 *	and in am_play_shutdown().
		 *
		 * It should be noted that when DRAIN is driven by interrupts
		 * that the buffer filled by am_get_audio() won't actually
		 * be played until after the current buffer is used. Therefore
		 * we need to wait for the 2nd interrupt, not the next
		 * interrupt.
		 */
		    mblk_t	*tmp;
		    ATRACE("am_wioctl() AUDIO_DRAIN", chptr);

		    /* must be on a play audio channel */
		    if (!chpptr->writing || type != AUDIO) {
			ATRACE("am_wioctl() AUDIO_DRAIN bad type", chptr);
			error = EINVAL;
			goto nack;
		    }

		    /* protect the channel struct */
		    mutex_enter(&chptr->ch_lock);

		    /* we only allow one outstanding AUDIO_DRAIN at a time */
		    if (*flags & AM_CHNL_DRAIN) {
			mutex_exit(&chptr->ch_lock);

			ATRACE("am_wioctl() AUDIO_DRAIN 1 at a time", chptr);

			error = EINVAL;
			goto nack;
		    }

		    /* DRAIN Case #1, the ch isn't active, so we can return */
		    if (info->play.active == 0 ||
			audio_sup_get_msg_cnt(chptr) == 0) {

			ATRACE("am_wioctl() DRAIN Case #1", chptr);
			mutex_exit(&chptr->ch_lock);
			goto ack;
		    }

		    /* set the flag while protected */
		    *flags |= AM_CHNL_DRAIN;

		    mutex_exit(&chptr->ch_lock);

			/*
			 * This message happens after 0 or more M_DATA messages
			 * that are putting data on the channel's queue for
			 * mixing. We need the drain to happen after all these
			 * messages have been mixed, but before the next M_DATA
			 * message. So create an M_BREAK message block, which
			 * we never send to the mixer, thus telling the mixer
			 * that there is an active AUDIO_DRAIN.
			 */
		    if ((tmp = allocb(1, BPRI_HI)) == NULL) {
			error = ENOMEM;
			goto nack;
		    }

		    tmp->b_datap->db_type = M_BREAK;

		    ATRACE("am_wioctl() M_BREAK msg to q", tmp);

		    (void) putq(WR(q), tmp);

		    /* protect the channel struct again */
		    mutex_enter(&chptr->ch_lock);

		    while ((*flags & AM_CHNL_EMPTY) == 0 &&
			(*flags & AM_CHNL_DRAIN)) {

			ATRACE_32("am_wioctl() AUDIO_DRAIN while", *flags);
			if (cv_wait_sig(&chptr->ch_cv, &chptr->ch_lock) <= 0) {
			    /* a signal interrupted the drain */
			    *flags &= ~(AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT);

			    /* used to get out of am_close() */
			    *flags |= AM_CHNL_DRAIN_SIG;

			    mutex_exit(&chptr->ch_lock);

			    ATRACE_32("am_wioctl() AUDIO_DRAIN signaled",
				*flags);
			    error = EINTR;
			    goto nack;
			}
		    }

		    /* DRAIN completed, so clear */
		    *flags &= ~(AM_CHNL_DRAIN|AM_CHNL_DRAIN_NEXT_INT|
			AM_CHNL_DRAIN_SIG);

		    mutex_exit(&chptr->ch_lock);

		    ATRACE("am_wioctl() AUDIO_DRAIN continues", chptr);

		    goto ack;
		}

		/* end AUDIO_DRAIN */

	case AUDIO_GETINFO: {
		audio_info_t	*hw_info = apm_infop->apm_ad_state;
		audio_info_t	*info_out;

		ATRACE("am_wioctl() AUDIO_GETINFO", chptr);

		/* must be on an audio or audioctl channel */
		if (type != AUDIO && type != AUDIOCTL) {
			ATRACE_32("am_wioctl() AUDIO_GETINFO bad type", type);
			error = EPERM;
			goto nack;
		}

		/* save data for M_IOCDATA processing */
		state->command = COPY_OUT_AUDIOINFO;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*info_out);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) < sizeof (*info_out)) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(
			    sizeof (*info_out), BPRI_HI);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				goto nack;
			}
		}

		info_out = (audio_info_t *)mp->b_cont->b_rptr;

		mutex_enter(&chptr->ch_lock);	/* protect the channel struct */

		bcopy(info, info_out, sizeof (*info_out));

		mutex_exit(&chptr->ch_lock);

		/* update the played sample count */
		am_fix_info(chptr, info_out, ad_infop, chpptr);

		/* update the features */
		info_out->hw_features =		hw_info->hw_features;
		info_out->sw_features =		hw_info->sw_features;
		info_out->sw_features_enabled =	hw_info->sw_features_enabled;

		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (*info_out);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_GETINFO returning", chptr);

		return (0);

		}

		/* end AUDIO_GETINFO */

	case AUDIO_SETINFO:
		ATRACE("am_wioctl() AUDIO_SETINFO", chptr);

		/*
		 * Must be on an audio or audioctl channel and not an audioctl
		 * associated with a mixer mode audio channel.
		 */
		if ((type != AUDIO && type != AUDIOCTL) ||
		    (type == AUDIOCTL && (*flags & AM_CHNL_MULTI_OPEN))) {
			ATRACE_32("am_wioctl() AUDIO_SETINFO bad type", type);
			error = EINVAL;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_AUDIOINFO;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (audio_info_t);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_SETINFO returning", chptr);

		return (0);

		/* end AUDIO_SETINFO */

	case AUDIO_GETDEV: {
		audio_device_t	*devp;

		ATRACE("am_wioctl() AUDIO_GETDEV", chptr);

		/* must be on an audio or audioctl channel */
		if (type != AUDIO && type != AUDIOCTL) {
			ATRACE_32("am_wioctl() AUDIO_GETDEV bad type", type);
			error = EPERM;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_GETDEV;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*ad_infop->ad_dev_info);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) <
		    sizeof (*ad_infop->ad_dev_info)) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(
			    sizeof (*ad_infop->ad_dev_info), BPRI_MED);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				goto nack;
			}
		}

		/*
		 * We don't bother to lock the state structure because this
		 * is static data.
		 */

		devp = (audio_device_t *)mp->b_cont->b_rptr;

		bcopy(ad_infop->ad_dev_info, devp,
		    sizeof (*ad_infop->ad_dev_info));

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (*ad_infop->ad_dev_info);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_GETDEV returning", chptr);

		return (0);

		}

		/* end AUDIO_GETDEV */

	case AUDIO_MIXER_MULTIPLE_OPEN:
		ATRACE("am_wioctl() AUDIO_MIXER_MULTIPLE_OPEN", chptr);

		/* must be on an audio channel in MIXER mode */
		if (type != AUDIO || mode == AM_COMPAT_MODE) {
			ATRACE_32("am_wioctl() "
			    "AUDIO_MIXER_MULTIPLE_OPEN bad type", type);
			error = EINVAL;
			goto nack;
		}

		/* protect the channel struct */
		mutex_enter(&chptr->ch_lock);

		/* see if we already multiple opens */
		if (*flags & AM_CHNL_MULTI_OPEN) {
			/* we do, so just ack it */
			mutex_exit(&chptr->ch_lock);
			goto ack;
		}

		/* we aren't, so set mixer mode */
		*flags |= AM_CHNL_MULTI_OPEN;

		mutex_exit(&chptr->ch_lock);

		ATRACE("am_wioctl() AUDIO_MIXER_MULTIPLE_OPEN set", *flags);

		goto ack;

		/* end AUDIO_MIXER_MULTIPLE_OPEN */

	case AUDIO_MIXER_SINGLE_OPEN: {
		audio_ch_t	*cptr;
		am_ch_private_t	*chpptr;
		pid_t		tpid;
		int		i;
		int		num_rd = 0;
		int		num_wr = 0;

		ATRACE("am_wioctl() AUDIO_MIXER_SINGLE_OPEN", chptr);

		/* must be on an audio channel in MIXER mode */
		if (type != AUDIO || mode == AM_COMPAT_MODE) {
			ATRACE_32(
			    "am_wioctl() AUDIO_MIXER_SINGLE_OPEN bad type",
			    type);
			error = EINVAL;
			goto nack;
		}

		tpid = chptr->ch_info.pid;

		/* we need to freeze channel allocation */
		mutex_enter(&statep->as_lock);
		for (i = 0, cptr = &statep->as_channels[0]; i < max_chs;
		    i++, cptr++) {
			/* ignore different processes */
			if (cptr->ch_info.pid != tpid) {
				continue;
			}

			chpptr = cptr->ch_private;
			if (chpptr->reading) {
				num_rd++;
			}
			if (chpptr->writing) {
				num_wr++;
			}
		}
		mutex_exit(&statep->as_lock);

		/* we change back only if at most 1 read or write ch. open */
		if (num_rd > 1 || num_wr > 1) {
			/* too many channels open, so we have to fail */
			error = EIO;
			goto nack;
		}

		/* if we get here we know there is only one channel, ours */
		*flags &= ~AM_CHNL_MULTI_OPEN;

		ATRACE("am_wioctl() AUDIO_MIXER_SINGLE_OPEN set", *flags);

		goto ack;

		}

		/* end AUDIO_MIXER_SINGLE_OPEN */

	case AUDIO_MIXER_GET_SAMPLE_RATES:
		ATRACE("am_wioctl() AUDIO_MIXER_GET_SAMPLE_RATES", chptr);

		/* must be on an AUDIO channel */
		if (type != AUDIO) {
			ATRACE_32("am_wioctl() "
			    "AUDIO_MIXER_GET_SAMPLE_RATES bad type", type);
			error = EINVAL;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_SAMP_RATES;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		/* don't know the size, so get the am_sample_rates_t struct */
		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (am_sample_rates_t);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_MIXER_GET_SAMPLE_RATES returning",
		    chptr);

		return (0);

		/* end AUDIO_MIXER_GET_SAMPLE_RATES */

	case AUDIO_MIXERCTL_GETINFO: {
		audio_ch_t		*cptr;
		am_control_t		*ptr;
		size_t			size;
		int			i;

		ATRACE("am_wioctl() AUDIO_MIXERCTL_GETINFO", chptr);

		/* must be on an audioctl channel in mixer mode */
		if (mode != AM_MIXER_MODE || type != AUDIOCTL) {
			ATRACE_32("am_wioctl() "
			    "AUDIO_MIXERCTL_GETINFO bad type", type);
			error = EINVAL;
			goto nack;
		}

		size = AUDIO_MIXER_CTL_STRUCT_SIZE(max_chs);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_MIXCTLINFO;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = size;
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) < size) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(size, BPRI_HI);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				goto nack;
			}
		}

		ptr = (am_control_t *)mp->b_cont->b_rptr;

		/*
		 * We have to assemble this one by pieces. First we take
		 * care of the hardware state, then extended hardware state.
		 */
		/* protect the struct */
		mutex_enter(apm_infop->apm_swlock);

		bcopy(apm_infop->apm_ad_state, &ptr->dev_info,
		    sizeof (audio_info_t));

		mutex_exit(apm_infop->apm_swlock);

		/* update the played sample count */
		am_fix_info(chptr, &ptr->dev_info, ad_infop, chpptr);

		/* now get the channel information */
		mutex_enter(&statep->as_lock);	/* freeze ch state */

		for (i = 0, cptr = &statep->as_channels[0];
		    i < max_chs; i++, cptr++) {
			if (cptr->ch_info.pid) {
				ptr->ch_open[i] = 1;
			} else {
				ptr->ch_open[i] = 0;
			}
		}
		mutex_exit(&statep->as_lock);

		mp->b_cont->b_wptr = mp->b_cont->b_rptr + size;

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_MIXERCTL_GETINFO returning", chptr);

		return (0);

		}

		/* end AUDIO_MIXERCTL_GETINFO */

	case AUDIO_MIXERCTL_SETINFO:
		ATRACE("am_wioctl() AUDIO_MIXERCTL_SETINFO", chptr);

		/* must be on an audioctl channel in mixer mode */
		if (mode != AM_MIXER_MODE || type != AUDIOCTL) {
			ATRACE_32("am_wioctl() "
			    "AUDIO_MIXERCTL_SETINFO bad type", type);
			error = EINVAL;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_MIXCTLINFO;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (am_control_t);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_MIXERCTL_SETINFO returning", chptr);

		return (0);

		/* end AUDIO_MIXERCTL_SETINFO */

	case AUDIO_MIXERCTL_GET_CHINFO:		/* both need to get a struct */
		ATRACE("am_wioctl() AUDIO_MIXERCTL_GET_CHINFO falling through",
		    chptr);
		/*FALLTHROUGH*/
	case AUDIO_MIXERCTL_SET_CHINFO: {	/* before we know what to do */
		ATRACE("am_wioctl() AUDIO_MIXERCTL_SET_CHINFO", chptr);

		/* must be on an audioctl channel in mixer mode */
		if (mode != AM_MIXER_MODE || type != AUDIOCTL) {
			ATRACE_32("am_wioctl() "
			    "AUDIO_MIXERCTL_SET_CHINFO bad type", type);
			error = EINVAL;
			goto nack;
		}

		/* make sure we can do the ioctl */
		if (chpptr->ioctl_tmp) {
			/* nope, we've got allocated memory, so we fail */
			ATRACE("am_wioctl() AUDIO_MIXERCTL_G/SET_CHINFO fail",
			    chptr);
			error = EBUSY;
			goto nack;
		}

		/*
		 * The first step is to get the audio_channel_t structure, which
		 * is in two parts, to get the channel number. The following
		 * code asks for that structure. am_wiocdata() will get the
		 * number and then take the appropriate action.
		 */

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_MIXCTL_CHINFO;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		/* size can be different, depending on _ILP32 and _LP64 */
		cqp->cq_size = SIZEOF_STRUCT(audio_channel, iocbp->ioc_flag);
		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;	/* MUST be after IOC_MODELS check */
		chpptr->ioctl_size = cqp->cq_size;
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE("am_wioctl() AUDIO_MIXERCTL_SET_CHINFO returning",
		    chptr);

		return (0);

		}

		/* end AUDIO_MIXERCTL_GET/SET_CHINFO */

	case AUDIO_MIXERCTL_GET_MODE:
		ATRACE("am_wioctl() AUDIO_MIXERCTL_GET_MODE", chptr);

		/* must be an audioctl channel */
		if (type != AUDIOCTL) {
			ATRACE_32(
			    "am_wioctl() AUDIO_MIXERCTL_GET_MODE bad type",
			    type);
			error = EINVAL;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_MIXCTL_MODE;	/* M_IOCDATA command */
		/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* only an int, so reuse the data block without checks */
		*(int *)mp->b_cont->b_rptr = mode;
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (AUDIO);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE_32("am_wioctl() "
		    "AUDIO_MIXERCTL_GET_MODE returning", mode);

		return (0);

		/* end AUDIO_MIXERCTL_GET_MODE */

	case AUDIO_MIXERCTL_SET_MODE:
		ATRACE("am_wioctl() AUDIO_MIXERCTL_SET_MODE", chptr);

		/* must be an audioctl channel */
		if (type != AUDIOCTL) {
			ATRACE_32(
			    "am_wioctl() AUDIO_MIXERCTL_SET_MODE bad type",
			    type);
			error = EINVAL;
			goto nack;
		}

		/* save state for M_IOCDATA processing */
		state->command = COPY_IN_MIXCTL_MODE;	/* M_IOCDATA command */
		/* user space addr */
		state->address = *(void **)mp->b_cont->b_rptr;	/* user addr. */

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* free the excess buffer */
		freemsg(mp->b_cont);
		mp->b_cont = 0;

		/* send the copy in request */
		qreply(q, mp);

		ATRACE_32("am_wioctl() "
		    "AUDIO_MIXERCTL_SET_MODE returning", mode);

		return (0);

		/* end AUDIO_MIXERCTL_SET_MODE */

	default:	/* see if we have an entry pt in the Audio Driver */
		if (ad_infop->ad_entry->ad_ioctl) {
			/* we do, so call it */
			ATRACE("am_wioctl(): "
			    "calling Audio Driver ioctl() routine",
			    ad_infop->ad_entry);
			ASSERT(ad_infop->ad_entry->ad_iocdata);
			switch (ad_infop->ad_entry->ad_ioctl(
			    audio_sup_get_dev_instance(NODEV, q),
			    chptr->ch_info.ch_number, q, mp)) {
			case AM_WIOCDATA:	return (0);
			case AM_ACK:		goto ack;
			case AM_NACK:
				/*FALLTHROUGH*/
			default:		goto nack;
			}
		}

		/* no - we're a driver, so we nack unrecognized ioctls */

		ATRACE_32("am_wioctl() default", iocbp->ioc_cmd);

		cmn_err(CE_NOTE, "mixer: wioctl() unrecognized ioc_cmd: 0x%x",
		    iocbp->ioc_cmd);
		error = EINVAL;
		goto nack;
	}

ack:
	ATRACE("am_wioctl() ack", chptr);

	/* the ioctl is done, so decrement the active count, if not DRAIN */
	if (command != AUDIO_DRAIN) {
		mutex_enter(apm_infop->apm_swlock);
		stpptr->active_ioctls--;
		mutex_exit(apm_infop->apm_swlock);

		if (state) {		/* free allocated state memory */
			kmem_free(state, sizeof (*state));
		}
	}

	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;		/* just in case */
	iocbp->ioc_count = 0;
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);

	ATRACE("am_wioctl() returning successful", chptr);

	return (0);

nack:
	ATRACE("am_wioctl() nack", chptr);

	/* the ioctl is done, so decrement the active count, if not DRAIN */
	if (command != AUDIO_DRAIN) {
		mutex_enter(apm_infop->apm_swlock);
		stpptr->active_ioctls--;
		mutex_exit(apm_infop->apm_swlock);

		if (state) {		/* free allocated state memory */
			kmem_free(state, sizeof (*state));
		}
	}

	iocbp->ioc_rval = -1;
	iocbp->ioc_error = error;
	iocbp->ioc_count = 0;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);

	ATRACE("am_wioctl() returning failure", chptr);

	return (0);

}	/* am_wioctl() */

/*
 * Private routines for sample rate conversion
 */

/*
 * am_src1_adjust()
 *
 * Description:
 *	This routine is used to adjust the number of samples so we know how
 *	many were converted.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	int		dir		Direction, AUDIO_PLAY or AUDIO_RECORD
 *	int		samples		The number of samples to adjust
 *
 * Returns:
 *	>= 0				The number of samples converted
 */
static int
am_src1_adjust(audio_ch_t *chptr, int dir, int samples)
{
	am_src1_data_t		*pptr;

	ATRACE("in am_src1_adjust()", chptr);
	ATRACE_32("am_src1_adjust() direction", dir);
	ATRACE_32("am_src1_adjust() samples", samples);

	/* get the conversion info */
	pptr = (am_src1_data_t *)am_get_src_data(chptr, dir);

	ATRACE_32("am_src1_adjust() up_factor", pptr->up_factor);
	ATRACE_32("am_src1_adjust() down_factor", pptr->down_factor);
	ATRACE_32("am_src1_adjust() returning",
	    (samples * pptr->up_factor) / pptr->down_factor);

	/* we do the math the opposite way! */
	return ((samples * pptr->down_factor) / pptr->up_factor);

}       /* am_src1_adjust() */

/*
 * am_src1_convert()
 *
 * Description:
 *	This routine manages the sample rate conversion process. It converts
 *	from inFs to outFs. The input stream must be 16-bit Linear PCM held
 *	as 32-bit integers.
 *
 *	Down conversion may use the same buffer as the source. This works
 *	because the down conversion process gets rid of samples, so we overwite
 *	samples that have been removed.
 *
 *	The returned pointer, if valid, must be one of the two passed in
 *	pointers. Otherwise memory will become lost.
 *
 * Argumments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	int		dir		Direction, AUDIO_PLAY or AUDIO_RECORD
 *	int		*ptr1		Conversion buffer, data arrives here
 *	int		*ptr2		Conversion buffer
 *	int		*samples	Pointer to the number of samples to
 *					convert, and when we return, the number
 *					of samples converted.
 *
 * Returns:
 *	valid pointer			Pointer to the converted audio stream
 *	NULL				Conversion failed
 */
static int *
am_src1_convert(audio_ch_t *chptr, int dir, int *ptr1, int *ptr2, int *samples)
{
	am_src1_data_t		*pdata;
	int			channels;
	int			converted;

	ATRACE("in am_src1_convert()", chptr);
	ATRACE_32("am_src1_convert() direction", dir);

	if (dir == AUDIO_PLAY) {
		/* # chs based on the play stream */
		channels = (int)((audio_info_t *)(chptr->ch_info.info))->
		    play.channels;
	} else {
		/* # chs based on the hardware */
		ASSERT(dir == AUDIO_RECORD);
		channels = (int)(((audio_info_t *)
		    (chptr->ch_apm_infop->apm_ad_state))->record.channels);
	}
	ATRACE_32("am_src1_convert() channels", channels);

	/* get the sample rate conversion data */
	pdata = am_get_src_data(chptr, dir);

	/* special case for when inFs == outFs, no conversion required */
	if (pdata->inFs == pdata->outFs) {
		ASSERT(pdata->up_factor == pdata->down_factor);
		ASSERT(pdata->up_factor == pdata->down_factor);

		ATRACE("am_src1_convert() inFs == outFS, returning", ptr1);
		return (ptr1);
	}

	/* 1st up/down conversion, src in ptr1, results in ptr2 */
	if (pdata->up[0] > 1) {	/* it could have been 1 */
		ATRACE_32("am_src1_convert() up[0]", pdata->up[0]);

		if (pdata->up[1] == 0) {
			/* only this up/down conversion to do */
			if (pdata->down[0] == 1) {
				ATRACE("am_src1_convert() "
				    "only 1st up conversion", ptr1);
				converted = pdata->up0(pdata, 0, pdata->up[0],
				    ptr1, ptr2, *samples);
			} else {
				ATRACE("am_src1_convert() "
				    "only 1st up/down conversion", ptr1);
				converted = pdata->up0(pdata, 0, pdata->up[0],
				    ptr1, ptr2, *samples);
				ATRACE_32("am_src1_convert() 1A down[0]",
				    pdata->down[0]);
				converted = am_src1_down(pdata, channels, 0,
				    pdata->down[0], ptr2, ptr2, converted);
			}

			ATRACE_32("am_src1_convert() 1st up/dwn samples",
			    converted);
			ATRACE("am_src1_convert() 1st up/dwn returning", ptr2);
			*samples = converted;
			return (ptr2);
		} else {	/* at least one more up/down ahead */
			ATRACE("am_src1_convert() 1st up/dwn conversion", ptr1);

			converted = pdata->up0(pdata, 0, pdata->up[0], ptr1,
			    ptr2, *samples);

			if (pdata->down[0] > 1) {
				ATRACE_32("am_src1_convert() 1B down[0]",
				    pdata->down[0]);
				converted = am_src1_down(pdata, channels, 0,
				    pdata->down[0], ptr2, ptr2, converted);
			}
		}
	} else {	/* 1st is down conversion only */
		ATRACE_32("am_src1_convert() down[0]", pdata->down[0]);
		ASSERT(pdata->up[0] == 1);

		if (pdata->up[1] == 0) {
			/* only this down conversion to do */
			ATRACE_32("am_src1_convert() 1C down[0]",
			    pdata->down[0]);
			converted = am_src1_down(pdata, channels, 0,
			    pdata->down[0], ptr1, ptr2, *samples);

			ATRACE_32("am_src1_convert() 1st down samples",
			    converted);
			ATRACE("am_src1_convert() 1st down returning", ptr2);
			*samples = converted;
			return (ptr2);
		} else {
			ATRACE("am_src1_convert() down[0] 1D", ptr1);
			converted = am_src1_down(pdata, channels, 0,
			    pdata->down[0], ptr1, ptr2, *samples);
		}
	}

	/* 2nd up/down conversion, src in ptr2, results in ptr1 */
	if (pdata->up[1] > 1) {	/* it could have been 1 */
		ATRACE_32("am_src1_convert() up[1]", pdata->up[1]);

		if (pdata->up[2] == 0) {
			/* only this up/down conversion to do */
			if (pdata->down[1] == 1) {
				ATRACE("am_src1_convert() "
				    "only 2nd up conversion", ptr2);
				converted = pdata->up1(pdata, 1, pdata->up[1],
				    ptr2, ptr1, converted);
			} else {
				ATRACE("am_src1_convert() "
				    "only 2nd up/down conversion", ptr2);
				converted = pdata->up1(pdata, 1, pdata->up[1],
				    ptr2, ptr1, converted);
				ATRACE_32("am_src1_convert() 2A down[1]",
				    pdata->down[1]);
				converted = am_src1_down(pdata, channels, 1,
				    pdata->down[1], ptr1, ptr1, converted);
			}

			ATRACE_32("am_src1_convert() 2nd up/dwn samples",
			    converted);
			ATRACE("am_src1_convert() 2nd up/dwn returning", ptr1);
			*samples = converted;
			return (ptr1);
		} else {	/* at least one more up/down ahead */
			ATRACE("am_src1_convert() 2nd up/dwn conversion", ptr2);

			converted = pdata->up1(pdata, 1, pdata->up[1], ptr2,
			    ptr1, converted);

			if (pdata->down[1] > 1) {
				ATRACE_32("am_src1_convert() 2B down[1]",
				    pdata->down[1]);
				converted = am_src1_down(pdata, channels, 1,
				    pdata->down[1], ptr1, ptr1, converted);
			}
		}
	} else {	/* 2nd is down conversion only */
		ATRACE_32("am_src1_convert() down[1]", pdata->down[1]);
		ASSERT(pdata->up[1] == 1);

		if (pdata->up[2] == 0) {
			/* only this down conversion to do */
			ATRACE_32("am_src1_convert() 2C down[1]",
			    pdata->down[1]);
			converted = am_src1_down(pdata, channels, 1,
			    pdata->down[1], ptr2, ptr1, converted);

			ATRACE_32("am_src1_convert() 2nd down samples",
				converted);
			ATRACE("am_src1_convert() 2nd down returning", ptr1);
			*samples = converted;
			return (ptr1);
		} else {
			ATRACE("am_src1_convert() down[1] 2D", ptr2);
			converted = am_src1_down(pdata, channels, 2,
			    pdata->down[1], ptr2, ptr1, converted);
		}
	}

	/* 3rd up/down conversion, src in ptr1, results in ptr2 */
	if (pdata->up[2] > 1) {	/* it could have been 1 */
		ATRACE_32("am_src1_convert() up[2]", pdata->up[2]);

		if (pdata->up[3] == 0) {
			/* only this up/down conversion to do */
			if (pdata->down[2] == 1) {
				ATRACE("am_src1_convert() "
				    "only 3rd up conversion", ptr1);
				converted = pdata->up2(pdata, 2, pdata->up[2],
				    ptr1, ptr2, converted);
			} else {
				ATRACE("am_src1_convert() "
				    "only 3rd up/down conversion", ptr1);
				converted = pdata->up2(pdata, 2, pdata->up[2],
				    ptr1, ptr2, converted);
				ATRACE_32("am_src1_convert() 2A down[2]",
				    pdata->down[2]);
				converted = am_src1_down(pdata, channels, 2,
				    pdata->down[2], ptr2, ptr2, converted);
			}

			ATRACE_32("am_src1_convert() 3rd up/dwn samples",
			    converted);
			ATRACE("am_src1_convert() 3rd up/dwn returning", ptr2);
			*samples = converted;
			return (ptr2);
		} else {	/* at least one more up/down ahead */
			ATRACE("am_src1_convert() 3rd up/dwn conversion", ptr1);

			converted = pdata->up2(pdata, 2, pdata->up[2], ptr1,
			    ptr2, converted);

			if (pdata->down[2] > 1) {
				ATRACE_32("am_src1_convert() 2B down[2]",
				    pdata->down[2]);
				converted = am_src1_down(pdata, channels, 2,
				    pdata->down[2], ptr2, ptr2, converted);
			}
		}
	} else {	/* 3rd is down conversion only */
		ATRACE_32("am_src1_convert() down[2]", pdata->down[2]);
		ASSERT(pdata->up[2] == 1);

		if (pdata->up[3] == 0) {
			/* only this down conversion to do */
			ATRACE_32("am_src1_convert() 2C down[2]",
			    pdata->down[2]);
			converted = am_src1_down(pdata, channels, 2,
			    pdata->down[2], ptr1, ptr2, converted);

			ATRACE_32("am_src1_convert() 3rd down samples",
			    converted);
			ATRACE("am_src1_convert() 3rd down returning", ptr2);
			*samples = converted;
			return (ptr2);
		} else {
			ATRACE("am_src1_convert() down[2] 2D", ptr1);
			converted = am_src1_down(pdata, channels, 2,
			    pdata->down[2], ptr1, ptr2, converted);
		}
	}

	/* 4th and final up/down conversion, src in ptr2, results in ptr1 */
	if (pdata->up[3] > 1) {	/* it could have been 1 */
		ATRACE_32("am_src1_convert() up[3]", pdata->up[3]);

		if (pdata->down[3] == 1) {
			ATRACE("am_src1_convert() "
			    "only 4th up conversion", ptr2);
			converted = pdata->up3(pdata, 3, pdata->up[3], ptr2,
			    ptr1, converted);
		} else {
			ATRACE("am_src1_convert() "
			    "only 4th up/down conversion", ptr2)
			converted = pdata->up3(pdata, 3, pdata->up[3], ptr2,
			    ptr1, converted);
			ATRACE_32("am_src1_convert() 3A down[3]",
							pdata->down[3]);
			converted = am_src1_down(pdata, channels, 3,
			    pdata->down[3], ptr1, ptr1, converted);
		}
	} else {	/* 4th is down conversion only */
		ATRACE_32("am_src1_convert() down[3]", pdata->down[3]);
		ASSERT(pdata->up[3] == 1);

		converted = am_src1_down(pdata, channels, 3, pdata->down[3],
		    ptr2, ptr1, converted);
	}

	ATRACE_32("am_src1_convert() final number converted", converted);
	ATRACE("am_src1_convert() final conversions returning", ptr1);

	*samples = converted;

	return (ptr1);

}	/* am_src1_convert() */

/*
 * am_src1_exit()
 *
 * Description:
 *	Free the private data structure allocated in am_src1_init()
 *
 *	NOTE: We do NOT free the buffers used for sample rate conversion.
 *
 * Argumments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	int		dir		Direction, AUDIO_PLAY or AUDIO_RECORD
 *
 * Returns:
 *	void
 */
static void
am_src1_exit(audio_ch_t *chptr, int dir)
{
	am_src1_data_t		*pdata;

	ATRACE("in am_src1_exit()", chptr);
	ATRACE_32("am_src1_exit() direction", dir);

	/* get pointers, based on which direction we are going */
	pdata = am_get_src_data(chptr, dir);
	ATRACE("am_src1_exit() pdata", pdata);

	/* make sure we've got an am_src1_data structure */
	if (pdata) {
		ATRACE("am_src1_exit() freeing", pdata);
		kmem_free(pdata, sizeof (*pdata));
		am_set_src_data(chptr, dir, NULL);
	}

	ATRACE("am_src1_exit() done", pdata);

}	/* am_src1_exit() */

/*
 * am_src1_init()
 *
 * Description:
 *	Initialize the sample rate conversion private data structure. If
 *	needed, allocate memory for it. Initialization includes setting
 *	up and down conversion factors as well as setting the conversion
 *	routines based on whether filtering is enabled or not.
 *
 *	NOTE: We do NOT allocate the buffers used for sample rate conversion.
 *
 * Argumments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	int		dir		Direction, AUDIO_PLAY or AUDIO_RECORD
 *
 * Returns:
 *	AUDIO_SUCCESS			Initialization succeeded
 *	AUDIO_FAILURE			Initialization failed
 */
static int
am_src1_init(audio_ch_t *chptr, int dir)
{
	audio_prinfo_t		*ch_prinfo;	/* channel state info */
	audio_prinfo_t		*hw_prinfo;	/* Codec state info */
	audio_apm_info_t	*apm_info = chptr->ch_apm_infop;
	am_src1_data_t		*pdata;
	am_ad_src1_info_t	*psrs;
	int			i;

	ATRACE("in am_src1_init()", chptr);
	ATRACE_32("am_src1_init() direction", dir);
	ATRACE("am_src1_init() apm_info", apm_info);
	ATRACE("am_src1_init() apm_ad_infop", apm_info->apm_ad_infop);

	/* get src data structure */
	if ((pdata = am_get_src_data(chptr, dir)) == 0) {
		if ((pdata = kmem_zalloc(sizeof (*pdata), KM_NOSLEEP)) ==
		    NULL) {
			ATRACE("am_src1_init() kmem_zalloc() failed", 0);
			return (AUDIO_FAILURE);
		}

		ATRACE("am_src1_init() new src data structure", pdata);

		am_set_src_data(chptr, dir, pdata);
	} else {
		ATRACE("am_src1_init() reusing src data structure", pdata);
		bzero(pdata, sizeof (*pdata));
	}

	/* get pointers, based on which direction we are going */
	if (dir == AUDIO_PLAY) {
		ch_prinfo = &((audio_info_t *)chptr->ch_info.info)->play;
		hw_prinfo = &((audio_info_t *)apm_info->apm_ad_state)->play;

		psrs = ((am_ad_info_t *)
		    apm_info->apm_ad_infop)->ad_play.ad_sr_info;
		pdata->inFs =	ch_prinfo->sample_rate;
		pdata->outFs =	hw_prinfo->sample_rate;

		/* find the input sample rate in the array */
		for (i = 0; psrs->ad_from_sr; i++, psrs++) {
			if (psrs->ad_from_sr == pdata->inFs) {
				break;
			}
		}
		if (psrs->ad_from_sr == 0) {
			ATRACE_32("am_src1_init() bad play sample rate",
			    pdata->inFs);
			cmn_err(CE_NOTE,
			    "mixer: src1_init() bad play sample rate: %d",
			    pdata->inFs);
			return (AUDIO_FAILURE);
		}
	} else {
		ch_prinfo = &((audio_info_t *)chptr->ch_info.info)->record;
		hw_prinfo = &((audio_info_t *)apm_info->apm_ad_state)->record;

		psrs = ((am_ad_info_t *)
		    apm_info->apm_ad_infop)->ad_record.ad_sr_info;
		pdata->inFs =	hw_prinfo->sample_rate;
		pdata->outFs =	ch_prinfo->sample_rate;

		/* find the output sample rate in the array */
		for (i = 0; psrs->ad_to_sr; i++, psrs++) {
			if (psrs->ad_to_sr == pdata->outFs) {
				break;
			}
		}
		if (psrs->ad_to_sr == 0) {
			ATRACE_32("am_src1_init() bad record sample rate",
			    pdata->outFs);
			cmn_err(CE_NOTE,
			    "mixer: src1_init() bad record sample rate: %d",
			    pdata->outFs);
			return (AUDIO_FAILURE);
		}
	}
	ATRACE("am_src1_init() ch_prinfo", ch_prinfo);

	/* fill in all the generic data */
	ATRACE_32("am_src1_init() inFs", pdata->inFs);
	ATRACE_32("am_src1_init() outFs", pdata->outFs);
	ATRACE("am_src1_init() psrs", psrs);
	ATRACE_32("am_src1_init() psrs->ad_from_sr", psrs->ad_from_sr);
	ATRACE_32("am_src1_init() psrs->ad_to_sr", psrs->ad_to_sr);


	/* now fill in the sample rate conversion specific data */
	pdata->k = psrs->ad_k;

	/* we always have at least one up and down sampling */
	pdata->up[0] =			psrs->ad_u0 & ~AM_FILTER;
	pdata->down[0] =		psrs->ad_d0 & ~AM_FILTER;
	ASSERT(pdata->down[0] != 0);
	if (psrs->ad_u0 & AM_FILTER) {
		if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
			switch ((psrs->ad_u0 & ~AM_FILTER)) {
			case 2:		pdata->up0 = am_src1_up_2fm;	break;
			case 3:		pdata->up0 = am_src1_up_3fm;	break;
			case 4:		pdata->up0 = am_src1_up_4fm;	break;
			case 5:		pdata->up0 = am_src1_up_5fm;	break;
			case 6:		pdata->up0 = am_src1_up_6fm;	break;
			case 7:		pdata->up0 = am_src1_up_7fm;	break;
			case 8:		pdata->up0 = am_src1_up_8fm;	break;
			case 9:		pdata->up0 = am_src1_up_9fm;	break;
			case 10:	pdata->up0 = am_src1_up_10fm;	break;
			default:	pdata->up0 = am_src1_up_dfm;	break;
			}
		} else {
			switch ((psrs->ad_u0 & ~AM_FILTER)) {
			case 2:		pdata->up0 = am_src1_up_2fs;	break;
			case 3:		pdata->up0 = am_src1_up_3fs;	break;
			case 4:		pdata->up0 = am_src1_up_4fs;	break;
			case 5:		pdata->up0 = am_src1_up_5fs;	break;
			case 6:		pdata->up0 = am_src1_up_6fs;	break;
			case 7:		pdata->up0 = am_src1_up_7fs;	break;
			case 8:		pdata->up0 = am_src1_up_8fs;	break;
			case 9:		pdata->up0 = am_src1_up_9fs;	break;
			case 10:	pdata->up0 = am_src1_up_10fs;	break;
			default:	pdata->up0 = am_src1_up_dfs;	break;
			}
		}
	} else {
		if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
			switch (psrs->ad_u0) {
			case 2:		pdata->up0 = am_src1_up_2m;	break;
			case 3:		pdata->up0 = am_src1_up_3m;	break;
			case 4:		pdata->up0 = am_src1_up_4m;	break;
			case 5:		pdata->up0 = am_src1_up_5m;	break;
			case 6:		pdata->up0 = am_src1_up_6m;	break;
			case 7:		pdata->up0 = am_src1_up_7m;	break;
			case 8:		pdata->up0 = am_src1_up_8m;	break;
			case 9:		pdata->up0 = am_src1_up_9m;	break;
			case 10:	pdata->up0 = am_src1_up_10m;	break;
			default:	pdata->up0 = am_src1_up_dm;	break;
			}
		} else {
			switch (psrs->ad_u0) {
			case 2:		pdata->up0 = am_src1_up_2s;	break;
			case 3:		pdata->up0 = am_src1_up_3s;	break;
			case 4:		pdata->up0 = am_src1_up_4s;	break;
			case 5:		pdata->up0 = am_src1_up_5s;	break;
			case 6:		pdata->up0 = am_src1_up_6s;	break;
			case 7:		pdata->up0 = am_src1_up_7s;	break;
			case 8:		pdata->up0 = am_src1_up_8s;	break;
			case 9:		pdata->up0 = am_src1_up_9s;	break;
			case 10:	pdata->up0 = am_src1_up_10s;	break;
			default:	pdata->up0 = am_src1_up_ds;	break;
			}
		}
	}
	pdata->up_factor = pdata->up[0];
	pdata->down_factor = pdata->down[0];
	pdata->count = 1;
	ATRACE_32("am_src1_init() up[0]", pdata->up[0]);
	ATRACE_32("am_src1_init() down[0]", pdata->down[0]);

	if (psrs->ad_u1) {
		pdata->up[1] =		psrs->ad_u1 & ~AM_FILTER;
		pdata->down[1] =	psrs->ad_d1 & ~AM_FILTER;
		ASSERT(pdata->down[1] != 0);
		if (psrs->ad_u1 & AM_FILTER) {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch ((psrs->ad_u1 & ~AM_FILTER)) {
				case 2:	pdata->up1 = am_src1_up_2fm;	break;
				case 3:	pdata->up1 = am_src1_up_3fm;	break;
				case 4:	pdata->up1 = am_src1_up_4fm;	break;
				case 5:	pdata->up1 = am_src1_up_5fm;	break;
				case 6:	pdata->up1 = am_src1_up_6fm;	break;
				case 7:	pdata->up1 = am_src1_up_7fm;	break;
				case 8:	pdata->up1 = am_src1_up_8fm;	break;
				case 9:	pdata->up1 = am_src1_up_9fm;	break;
				case 10: pdata->up1 = am_src1_up_10fm;	break;
				default: pdata->up1 = am_src1_up_dfm;	break;
				}
			} else {
				switch ((psrs->ad_u1 & ~AM_FILTER)) {
				case 2:	pdata->up1 = am_src1_up_2fs;	break;
				case 3:	pdata->up1 = am_src1_up_3fs;	break;
				case 4:	pdata->up1 = am_src1_up_4fs;	break;
				case 5:	pdata->up1 = am_src1_up_5fs;	break;
				case 6:	pdata->up1 = am_src1_up_6fs;	break;
				case 7:	pdata->up1 = am_src1_up_7fs;	break;
				case 8:	pdata->up1 = am_src1_up_8fs;	break;
				case 9:	pdata->up1 = am_src1_up_9fs;	break;
				case 10: pdata->up1 = am_src1_up_10fs;	break;
				default: pdata->up1 = am_src1_up_dfs;	break;
				}
			}
		} else {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch (psrs->ad_u1) {
				case 2:	pdata->up1 = am_src1_up_2m;	break;
				case 3:	pdata->up1 = am_src1_up_3m;	break;
				case 4:	pdata->up1 = am_src1_up_4m;	break;
				case 5:	pdata->up1 = am_src1_up_5m;	break;
				case 6:	pdata->up1 = am_src1_up_6m;	break;
				case 7:	pdata->up1 = am_src1_up_7m;	break;
				case 8:	pdata->up1 = am_src1_up_8m;	break;
				case 9:	pdata->up1 = am_src1_up_9m;	break;
				case 10: pdata->up1 = am_src1_up_10m;	break;
				default: pdata->up1 = am_src1_up_dm;	break;
				}
			} else {
				switch (psrs->ad_u1) {
				case 2:	pdata->up1 = am_src1_up_2s;	break;
				case 3:	pdata->up1 = am_src1_up_3s;	break;
				case 4:	pdata->up1 = am_src1_up_4s;	break;
				case 5:	pdata->up1 = am_src1_up_5s;	break;
				case 6:	pdata->up1 = am_src1_up_6s;	break;
				case 7:	pdata->up1 = am_src1_up_7s;	break;
				case 8:	pdata->up1 = am_src1_up_8s;	break;
				case 9:	pdata->up1 = am_src1_up_9s;	break;
				case 10: pdata->up1 = am_src1_up_10s;	break;
				default: pdata->up1 = am_src1_up_ds;	break;
				}
			}
		}
		pdata->up_factor *= pdata->up[1];
		pdata->down_factor *= pdata->down[1];
		pdata->count++;
		ATRACE_32("am_src1_init() up[1]", pdata->up[1]);
		ATRACE_32("am_src1_init() down[1]", pdata->down[1]);
	}

	if (psrs->ad_u2) {
		pdata->up[2] =		psrs->ad_u2 & ~AM_FILTER;
		pdata->down[2] =	psrs->ad_d2 & ~AM_FILTER;
		ASSERT(pdata->down[2] != 0);
		if (psrs->ad_u2 & AM_FILTER) {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch ((psrs->ad_u2 & ~AM_FILTER)) {
				case 2:	pdata->up2 = am_src1_up_2fm;	break;
				case 3:	pdata->up2 = am_src1_up_3fm;	break;
				case 4:	pdata->up2 = am_src1_up_4fm;	break;
				case 5:	pdata->up2 = am_src1_up_5fm;	break;
				case 6:	pdata->up2 = am_src1_up_6fm;	break;
				case 7:	pdata->up2 = am_src1_up_7fm;	break;
				case 8:	pdata->up2 = am_src1_up_8fm;	break;
				case 9:	pdata->up2 = am_src1_up_9fm;	break;
				case 10: pdata->up2 = am_src1_up_10fm;	break;
				default: pdata->up2 = am_src1_up_dfm;	break;
				}
			} else {
				switch ((psrs->ad_u2 & ~AM_FILTER)) {
				case 2:	pdata->up2 = am_src1_up_2fs;	break;
				case 3:	pdata->up2 = am_src1_up_3fs;	break;
				case 4:	pdata->up2 = am_src1_up_4fs;	break;
				case 5:	pdata->up2 = am_src1_up_5fs;	break;
				case 6:	pdata->up2 = am_src1_up_6fs;	break;
				case 7:	pdata->up2 = am_src1_up_7fs;	break;
				case 8:	pdata->up2 = am_src1_up_8fs;	break;
				case 9:	pdata->up2 = am_src1_up_9fs;	break;
				case 10: pdata->up2 = am_src1_up_10fs;	break;
				default: pdata->up2 = am_src1_up_dfs;	break;
				}
			}
		} else {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch (psrs->ad_u2) {
				case 2:	pdata->up2 = am_src1_up_2m;	break;
				case 3:	pdata->up2 = am_src1_up_3m;	break;
				case 4:	pdata->up2 = am_src1_up_4m;	break;
				case 5:	pdata->up2 = am_src1_up_5m;	break;
				case 6:	pdata->up2 = am_src1_up_6m;	break;
				case 7:	pdata->up2 = am_src1_up_7m;	break;
				case 8:	pdata->up2 = am_src1_up_8m;	break;
				case 9:	pdata->up2 = am_src1_up_9m;	break;
				case 10: pdata->up2 = am_src1_up_10m;	break;
				default: pdata->up2 = am_src1_up_dm;	break;
				}
			} else {
				switch (psrs->ad_u2) {
				case 2:	pdata->up2 = am_src1_up_2s;	break;
				case 3:	pdata->up2 = am_src1_up_3s;	break;
				case 4:	pdata->up2 = am_src1_up_4s;	break;
				case 5:	pdata->up2 = am_src1_up_5s;	break;
				case 6:	pdata->up2 = am_src1_up_6s;	break;
				case 7:	pdata->up2 = am_src1_up_7s;	break;
				case 8:	pdata->up2 = am_src1_up_8s;	break;
				case 9:	pdata->up2 = am_src1_up_9s;	break;
				case 10: pdata->up2 = am_src1_up_10s;	break;
				default: pdata->up2 = am_src1_up_ds;	break;
				}
			}
		}
		pdata->up_factor *= pdata->up[2];
		pdata->down_factor *= pdata->down[2];
		pdata->count++;
		ATRACE_32("am_src1_init() up[2]", pdata->up[2]);
		ATRACE_32("am_src1_init() down[2]", pdata->down[2]);
	}

	if (psrs->ad_u3) {
		pdata->up[3] =		psrs->ad_u3 & ~AM_FILTER;
		pdata->down[3] =	psrs->ad_d3 & ~AM_FILTER;
		ASSERT(pdata->down[3] != 0);
		if (psrs->ad_u1 & AM_FILTER) {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch ((psrs->ad_u3 & ~AM_FILTER)) {
				case 2:	pdata->up3 = am_src1_up_2fm;	break;
				case 3:	pdata->up3 = am_src1_up_3fm;	break;
				case 4:	pdata->up3 = am_src1_up_4fm;	break;
				case 5:	pdata->up3 = am_src1_up_5fm;	break;
				case 6:	pdata->up3 = am_src1_up_6fm;	break;
				case 7:	pdata->up3 = am_src1_up_7fm;	break;
				case 8:	pdata->up3 = am_src1_up_8fm;	break;
				case 9:	pdata->up3 = am_src1_up_9fm;	break;
				case 10: pdata->up3 = am_src1_up_10fm;	break;
				default: pdata->up3 = am_src1_up_dfm;	break;
				}
			} else {
				switch ((psrs->ad_u3 & ~AM_FILTER)) {
				case 2:	pdata->up3 = am_src1_up_2fs;	break;
				case 3:	pdata->up3 = am_src1_up_3fs;	break;
				case 4:	pdata->up3 = am_src1_up_4fs;	break;
				case 5:	pdata->up3 = am_src1_up_5fs;	break;
				case 6:	pdata->up3 = am_src1_up_6fs;	break;
				case 7:	pdata->up3 = am_src1_up_7fs;	break;
				case 8:	pdata->up3 = am_src1_up_8fs;	break;
				case 9:	pdata->up3 = am_src1_up_9fs;	break;
				case 10: pdata->up3 = am_src1_up_10fs;	break;
				default: pdata->up3 = am_src1_up_dfs;	break;
				}
			}
		} else {
			if (ch_prinfo->channels == AUDIO_CHANNELS_MONO) {
				switch (psrs->ad_u3) {
				case 2:	pdata->up3 = am_src1_up_2m;	break;
				case 3:	pdata->up3 = am_src1_up_3m;	break;
				case 4:	pdata->up3 = am_src1_up_4m;	break;
				case 5:	pdata->up3 = am_src1_up_5m;	break;
				case 6:	pdata->up3 = am_src1_up_6m;	break;
				case 7:	pdata->up3 = am_src1_up_7m;	break;
				case 8:	pdata->up3 = am_src1_up_8m;	break;
				case 9:	pdata->up3 = am_src1_up_9m;	break;
				case 10: pdata->up3 = am_src1_up_10m;	break;
				default: pdata->up3 = am_src1_up_dm;	break;
				}
			} else {
				switch (psrs->ad_u3) {
				case 2:	pdata->up3 = am_src1_up_2s;	break;
				case 3:	pdata->up3 = am_src1_up_3s;	break;
				case 4:	pdata->up3 = am_src1_up_4s;	break;
				case 5:	pdata->up3 = am_src1_up_5s;	break;
				case 6:	pdata->up3 = am_src1_up_6s;	break;
				case 7:	pdata->up3 = am_src1_up_7s;	break;
				case 8:	pdata->up3 = am_src1_up_8s;	break;
				case 9:	pdata->up3 = am_src1_up_9s;	break;
				case 10: pdata->up3 = am_src1_up_10s;	break;
				default: pdata->up3 = am_src1_up_ds;	break;
				}
			}
		}
		pdata->up_factor *= pdata->up[3];
		pdata->down_factor *= pdata->down[3];
		pdata->count++;
		ATRACE_32("am_src1_init() up[3]", pdata->up[3]);
		ATRACE_32("am_src1_init() down[3]", pdata->down[3]);
	}

	ATRACE_32("am_src1_init() up factor", pdata->up_factor);
	ATRACE_32("am_src1_init() down factor", pdata->down_factor);
	ATRACE_32("am_src1_init() count", pdata->count);

	/* sanity check */
	if ((pdata->inFs * pdata->up_factor / pdata->down_factor) !=
	    pdata->outFs) {
		ATRACE_32("am_src1_init() bad outFs",
		    pdata->inFs * pdata->up_factor / pdata->down_factor);
		cmn_err(CE_NOTE,
		    "mixer: src1_init() %d -> %d bad conversion factors",
		    pdata->inFs, pdata->outFs);
		return (AUDIO_FAILURE);
	}

	return (AUDIO_SUCCESS);

}	/* am_src1_init() */

/*
 * am_src1_size()
 *
 * Description:
 *	Determine the size of a buffer, in bytes, needed to hold the number
 *	of "samples" when they are converted. We adjust the size based on
 *	the number of source hardware channels.
 *
 *	NOTE: This size of the buffer is based on 32-bit per sample.
 *
 * Argumments:
 *	audio_ch_t	*chptr		Pointer to this channel's state info
 *	int		dir		Direction, AUDIO_PLAY or AUDIO_RECORD
 *	int		samples		The number of samples
 *	int		hw_channels	Number of hardware channels
 *
 * Returns:
 *	size			The max # of bytes any sample rate conversion
 *				step could need in buffer space
 */
static size_t
am_src1_size(audio_ch_t *chptr, int dir, int samples, int hw_channels)
{
	am_src1_data_t		*pdata;
	audio_prinfo_t		*pinfo;
	size_t			size;
	size_t			size0 = 0;
	size_t			size1 = 0;
	size_t			size2 = 0;
	size_t			size3 = 0;

	ATRACE("in am_src1_size()", chptr);
	ATRACE_32("am_src1_size() direction", dir);
	ATRACE_32("am_src1_size() samples", samples);
	ATRACE_32("am_src1_size() hw_channels", hw_channels);

	if (dir == AUDIO_PLAY) {
		pinfo = &((audio_info_t *)chptr->ch_info.info)->play;
	} else {
		ASSERT(dir == AUDIO_RECORD);
		pinfo = &((audio_info_t *)chptr->ch_info.info)->record;
	}

	pdata = (am_src1_data_t *)am_get_src_data(chptr, dir);
	ASSERT(pdata);
	ASSERT(pdata->count <= 4);
	ATRACE_32("am_src1_size() count", pdata->count);

	/* we always have at least one */
	size0 = samples * pdata->up[0];
	ATRACE_32("am_src1_size() size0", size0);

	if (pdata->count >= 2) {
		size1 = size0 / pdata->down[0] * pdata->up[1];
		ATRACE_32("am_src1_size() size1", size1);
	}

	if (pdata->count >= 3) {
		size2 = size1 / pdata->down[1] * pdata->up[2];
		ATRACE_32("am_src1_size() size2", size2);
	}

	if (pdata->count == 4) {
		size3 = size2 / pdata->down[2] * pdata->up[3];
		ATRACE_32("am_src1_size() size3", size3);
	}

	size = (AUDIO_MAX4(size0, size1, size2, size3) +
	    (MUPSIZE1 * AM_SRC1_BUFFER)) << AM_INT32_SHIFT;

	/* now adjust for the number of channels */
	if (pinfo->channels < hw_channels) {
		size *= hw_channels;
	}

	ATRACE_32("am_src1_size() returned size", size);

	return (size);

}	/* am_src1_size() */

/*
 * am_src1_down()
 *
 * Description:
 *	Down sample the sound buffer by decimation. There isn't any filtering
 *	done at all.
 *
 * Argumments:
 *	am_src1_data_t	*parms		Conversion parameters structure
 *	int		channels	The number of channels to convert
 *	int		count		Which down sample this is, 0 -> 3
 *	int		factor		The down sampling factor
 *	int		inbuf		The input buffer to convert
 *	int		outbuf		The converted audio buffer
 *	int		samples		The number of samples to convert
 *
 * Returns:
 *	>= 0				The number of samples remaining
 */
static int
am_src1_down(am_src1_data_t *parms, int channels, int count,
	int factor, int *inbuf, int *outbuf, int samples)
{
	int		i;
	int		to;

	ATRACE("in am_src1_down()", parms);

	i = parms->dstart[count];

	if (channels == AUDIO_CHANNELS_MONO) {
		ATRACE("am_src1_down() MONO", inbuf);

		for (to = 0; i < samples; to++, i += factor) {
			*outbuf++ = inbuf[i];
		}

		ATRACE("am_src1_down() MONO done", outbuf);
	} else {
		ATRACE("am_src1_down() STEREO", inbuf);

		ASSERT(channels == AUDIO_CHANNELS_STEREO);

		factor <<= 1;

		for (to = 0; i < samples; to += 2, i += factor) {
			*outbuf++ = inbuf[i];
			*outbuf++ = inbuf[i+1];

		}

		ATRACE("am_src1_down() STEREO done", outbuf);
	}

	/* figure out the starting place for next time */
	i -= factor;
	parms->dstart[count] = factor - (samples - i);

	ATRACE_32("am_src1_down() returning", to);

	return (to);

}	/* am_src1_down() */

/*
 * am_src1_[2-10,d][m,s]()
 *
 * Description:
 *	Up sample the input buffer by linear interpolation. There isn't any
 *	filtering done at all in these routines. The following routines are
 *	split up so we don't waste time checking the factor and number of
 *	channels. This helps boost performance a little, since these
 *	routines are called constantly.
 *
 * Argumments:
 *	am_src1_data_t	*parms		Conversion parameters structure
 *	int		spos		Which up sample this is, 0 -> 3
 *	int		factor		The up sampling factor
 *	int		inbuf		The input buffer to convert
 *	int		outbuf		The converted audio buffer
 *	int		samples		The number of samples to convert
 *
 * Returns:
 *	> 0				The number of samples created
 */
static int
am_src1_up_2m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		end1;
	int		from;
	int		start1;

	ATRACE("in am_src1_up_2m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		end1 = inbuf[0];

		outbuf[0] = start1;
		outbuf[1] = (start1 + end1) >> SRC1_SHIFT1;

		start1 = end1;
		outbuf	+= 2;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_2m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_2m() */

static int
am_src1_up_2s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		end1;
	int		end2;
	int		from;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_2s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		end1 = inbuf[0];
		end2 = inbuf[1];

		outbuf[0] = start1;
		outbuf[1] = start2;
		outbuf[2] = (start1 + end1) >> SRC1_SHIFT1;
		outbuf[3] = (start2 + end2) >> SRC1_SHIFT1;

		start1 = end1;
		start2 = end2;
		outbuf	+= 4;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_2s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_2s() */

static int
am_src1_up_3m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_3m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 3;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 3;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_3m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_3m() */

static int
am_src1_up_3s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_3s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 3;
		delta2 = (inbuf[1] - start2) / 3;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 6;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_3s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_3s() */

static int
am_src1_up_4m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_4m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT2;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 4;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_4m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_4m() */

static int
am_src1_up_4s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_4s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT2;
		delta2 = (inbuf[1] - start2) >> SRC1_SHIFT2;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 8;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_4s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_4s() */

static int
am_src1_up_5m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_5m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 5;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 5;
		inbuf	+= 1;

	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_5m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_5m() */

static int
am_src1_up_5s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_5s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 5;
		delta2 = (inbuf[1] - start2) / 5;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 10;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_5s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_5s() */

static int
am_src1_up_6m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_2m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 6;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;
		sum1 = outbuf[5] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 6;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_6m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_6m() */

static int
am_src1_up_6s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_6s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 6;
		delta2 = (inbuf[1] - start2) / 6;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;
		sum1 = outbuf[10] = sum1 + delta1;
		sum2 = outbuf[11] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 12;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_6s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_6s() */

static int
am_src1_up_7m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_7m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 7;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;
		sum1 = outbuf[5] = sum1 + delta1;
		sum1 = outbuf[6] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 7;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_7m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_7m() */

static int
am_src1_up_7s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_7s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 7;
		delta2 = (inbuf[1] - start2) / 7;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;
		sum1 = outbuf[10] = sum1 + delta1;
		sum2 = outbuf[11] = sum2 + delta2;
		sum1 = outbuf[12] = sum1 + delta1;
		sum2 = outbuf[13] = sum2 + delta2;


		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 14;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_7s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_7s() */

static int
am_src1_up_8m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_8m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT3;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;
		sum1 = outbuf[5] = sum1 + delta1;
		sum1 = outbuf[6] = sum1 + delta1;
		sum1 = outbuf[7] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 8;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_8m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_2m() */

static int
am_src1_up_8s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_8s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT3;
		delta2 = (inbuf[1] - start2) >> SRC1_SHIFT3;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;
		sum1 = outbuf[10] = sum1 + delta1;
		sum2 = outbuf[11] = sum2 + delta2;
		sum1 = outbuf[12] = sum1 + delta1;
		sum2 = outbuf[13] = sum2 + delta2;
		sum1 = outbuf[14] = sum1 + delta1;
		sum2 = outbuf[15] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 16;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_8s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_8s() */

static int
am_src1_up_9m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_9m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 9;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;
		sum1 = outbuf[5] = sum1 + delta1;
		sum1 = outbuf[6] = sum1 + delta1;
		sum1 = outbuf[7] = sum1 + delta1;
		sum1 = outbuf[8] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 9;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_9m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_9m() */

static int
am_src1_up_9s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_9s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 9;
		delta2 = (inbuf[1] - start2) / 9;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;
		sum1 = outbuf[10] = sum1 + delta1;
		sum2 = outbuf[11] = sum2 + delta2;
		sum1 = outbuf[12] = sum1 + delta1;
		sum2 = outbuf[13] = sum2 + delta2;
		sum1 = outbuf[14] = sum1 + delta1;
		sum2 = outbuf[15] = sum2 + delta2;
		sum1 = outbuf[16] = sum1 + delta1;
		sum2 = outbuf[17] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[0];
		outbuf	+= 18;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_9s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_9s() */

static int
am_src1_up_10m(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_10m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 10;

		sum1 = outbuf[0] = start1;
		sum1 = outbuf[1] = sum1 + delta1;
		sum1 = outbuf[2] = sum1 + delta1;
		sum1 = outbuf[3] = sum1 + delta1;
		sum1 = outbuf[4] = sum1 + delta1;
		sum1 = outbuf[5] = sum1 + delta1;
		sum1 = outbuf[6] = sum1 + delta1;
		sum1 = outbuf[7] = sum1 + delta1;
		sum1 = outbuf[8] = sum1 + delta1;
		sum1 = outbuf[9] = sum1 + delta1;

		start1 = inbuf[0];
		outbuf	+= 10;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_10m() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_10m() */

static int
am_src1_up_10s(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_10s()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 10;
		delta2 = (inbuf[1] - start2) / 10;

		sum1 = outbuf[0] = start1;
		sum2 = outbuf[1] = start2;
		sum1 = outbuf[2] = sum1 + delta1;
		sum2 = outbuf[3] = sum2 + delta2;
		sum1 = outbuf[4] = sum1 + delta1;
		sum2 = outbuf[5] = sum2 + delta2;
		sum1 = outbuf[6] = sum1 + delta1;
		sum2 = outbuf[7] = sum2 + delta2;
		sum1 = outbuf[8] = sum1 + delta1;
		sum2 = outbuf[9] = sum2 + delta2;
		sum1 = outbuf[10] = sum1 + delta1;
		sum2 = outbuf[11] = sum2 + delta2;
		sum1 = outbuf[12] = sum1 + delta1;
		sum2 = outbuf[13] = sum2 + delta2;
		sum1 = outbuf[14] = sum1 + delta1;
		sum2 = outbuf[15] = sum2 + delta2;
		sum1 = outbuf[16] = sum1 + delta1;
		sum2 = outbuf[17] = sum2 + delta2;
		sum1 = outbuf[18] = sum1 + delta1;
		sum2 = outbuf[19] = sum2 + delta2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 20;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_10s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_10s() */

static int
am_src1_up_dm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		i;
	int		start1;
	int		sum1;

	ATRACE("in am_src1_up_2m()", parms);

	start1 = parms->ustart1[spos];

	for (from = samples; from--; ) {
		delta1 = (*inbuf - start1) / factor;
		*outbuf++ = start1;
		sum1 = delta1;
		for (i = factor - 1; i--; sum1 += delta1) {
			*outbuf++ = start1 + sum1;
		}
		start1 = *inbuf++;
	}
	parms->ustart1[spos] = start1;

	ATRACE_32("am_src1_up_dm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_dm() */

static int
am_src1_up_ds(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		i;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;

	ATRACE("in am_src1_up_ds()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];

	for (from = samples >> 1; from--; ) {
		delta1 = (*inbuf - start1) / factor;
		delta2 = (*(inbuf+1) - start2) / factor;
		*outbuf++ = start1;
		*outbuf++ = start2;
		sum1 = delta1;
		sum2 = delta2;
		for (i = factor - 1; i--;
			sum1 += delta1, sum2 += delta2) {
			*outbuf++ = start1 + sum1;
			*outbuf++ = start2 + sum2;
		}
		start1 = *inbuf++;
		start2 = *inbuf++;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;

	ATRACE_32("am_src1_up_ds() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_ds() */

/*
 * am_src1_up[2-10,d]f[m,s]()
 *
 * Description:
 *	Up sample the input buffer by linear interpolation. A simple 1 stage
 *	FIR filter is applied in each of these routines. The following
 *	routines are split up so we don't waste time checking the factor and
 *	number of channels. This helps boost performance a little, since these
 *	routines are called constantly.
 *
 * Argumments:
 *	am_src1_data_t	*parms		Conversion parameters structure
 *	int		spos		Which up sample this is, 0 -> 3
 *	int		factor		The up sampling factor
 *	int		inbuf		The input buffer to convert
 *	int		outbuf		The converted audio buffer
 *	int		samples		The number of samples to convert
 *
 * Returns:
 *	> 0				The number of samples created
 */

static int
am_src1_up_2fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_2fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT1;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 2;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_2fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_2fm() */

static int
am_src1_up_2fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_2fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT1;
		delta2 = (inbuf[1] - start2) >> SRC1_SHIFT1;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 4;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_2s() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_2s() */

static int
am_src1_up_3fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_3fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 3;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 3;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_3fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_3fm() */

static int
am_src1_up_3fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		k = parms->k;
	int		from;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_3fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 3;
		delta2 = (inbuf[1] - start2) / 3;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 6;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_3fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_3fs() */

static int
am_src1_up_4fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_4fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT2;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 4;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_4fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_4fm() */

static int
am_src1_up_4fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_4fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT2;
		delta2 = (inbuf[1] - start2) >> SRC1_SHIFT2;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 8;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_4fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_4fs() */

static int
am_src1_up_5fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_5fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 5;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 5;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_5fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_5fm() */

static int
am_src1_up_5fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_5fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 5;
		delta2 = (inbuf[1] - start2) / 5;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 10;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_5fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_5fs() */

static int
am_src1_up_6fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_6fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 6;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[5] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 6;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_6fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_6fm() */

static int
am_src1_up_6fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_6fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 6;
		delta2 = (inbuf[1] - start2) / 6;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[10] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[11] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 12;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_6fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_6fs() */

static int
am_src1_up_7fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_7fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 7;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[5] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 7;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_7fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_7fm() */

static int
am_src1_up_7fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_7fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 7;
		delta2 = (inbuf[1] - start2) / 7;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[10] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[11] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[12] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[13] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 14;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_7fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_7fs() */

static int
am_src1_up_8fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_28m()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT3;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[5] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[7] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 8;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_8fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_8fm() */

static int
am_src1_up_8fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_8fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) >> SRC1_SHIFT3;
		delta2 = (inbuf[1] - start2) >> SRC1_SHIFT3;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[10] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[11] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[12] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[13] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[14] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[15] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 16;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_8fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_8fs() */

static int
am_src1_up_9fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_9fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 9;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[5] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[7] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 9;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_9fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_9fm() */

static int
am_src1_up_9fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_9fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 9;
		delta2 = (inbuf[1] - start2) / 9;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[10] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[11] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[12] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[13] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[14] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[15] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[16] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[17] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[1];
		outbuf	+= 18;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_9fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_9fs() */

static int
am_src1_up_10fm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		k = parms->k;
	int		out1;
	int		start1;

	ATRACE("in am_src1_up_10fm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (inbuf[0] - start1) / 10;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out1 = outbuf[1] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[3] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[5] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[7] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out1 = outbuf[9] = ((start1 + delta1 - out1) >> k) + out1;

		start1 = inbuf[0];
		outbuf	+= 10;
		inbuf	+= 1;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_10fm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_10fm() */

static int
am_src1_up_10fs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		k = parms->k;
	int		out1;
	int		out2;
	int		start1;
	int		start2;

	ATRACE("in am_src1_up_10fs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = (samples >> 1); from--; ) {
		delta1 = (inbuf[0] - start1) / 10;
		delta2 = (inbuf[1] - start2) / 10;

		out1 = outbuf[0] = ((start1 - out1) >> k) + out1;
		out2 = outbuf[1] = ((start2 - out2) >> k) + out2;
		out1 = outbuf[2] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[3] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[4] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[5] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[6] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[7] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[8] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[9] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[10] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[11] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[12] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[13] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[14] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[15] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[16] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[17] = ((start2 + delta2 - out2) >> k) + out2;
		out1 = outbuf[18] = ((start1 + delta1 - out1) >> k) + out1;
		out2 = outbuf[19] = ((start2 + delta2 - out2) >> k) + out2;

		start1 = inbuf[0];
		start2 = inbuf[0];
		outbuf	+= 20;
		inbuf	+= 2;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_10fs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_10fs() */

static int
am_src1_up_dfm(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		from;
	int		i;
	int		start1;
	int		sum1;
	int		k = parms->k;
	int		out1;

	ATRACE("in am_src1_up_dfm()", parms);

	start1 = parms->ustart1[spos];
	out1 = parms->out1[spos];

	for (from = samples; from--; ) {
		delta1 = (*inbuf - start1) / factor;
		*outbuf++ = out1 = ((start1 - out1) >> k) + out1;
		sum1 = delta1;
		for (i = factor - 1; i--; sum1 += delta1) {
			*outbuf++ = out1 = ((start1 + sum1 - out1) >> k) + out1;
		}
		start1 = *inbuf++;
	}
	parms->ustart1[spos] = start1;
	parms->out1[spos] = out1;

	ATRACE_32("am_src1_up_dfm() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_dfm() */

static int
am_src1_up_dfs(am_src1_data_t *parms, int spos, int factor,
	int *inbuf, int *outbuf, int samples)
{
	int		delta1;
	int		delta2;
	int		from;
	int		i;
	int		start1;
	int		start2;
	int		sum1;
	int		sum2;
	int		k = parms->k;
	int		out1;
	int		out2;

	ATRACE("in am_src1_up_dfs()", parms);

	start1 = parms->ustart1[spos];
	start2 = parms->ustart2[spos];
	out1 = parms->out1[spos];
	out2 = parms->out2[spos];

	for (from = samples >> 1; from--; ) {
		delta1 = (*inbuf - start1) / factor;
		delta2 = (*(inbuf+1) - start2) / factor;
		*outbuf++ = out1 = ((start1 - out1) >> k) + out1;
		*outbuf++ = out2 = ((start2 - out2) >> k) + out2;
		sum1 = delta1;
		sum2 = delta2;
		for (i = factor - 1; i--; sum1 += delta1, sum2 += delta2) {
		    *outbuf++ = out1 = ((start1 + sum1 - out1) >> k) + out1;
		    *outbuf++ = out2 = ((start2 + sum2 - out2) >> k) + out2;
		}
		start1 = *inbuf++;
		start2 = *inbuf++;
	}
	parms->ustart1[spos] = start1;
	parms->ustart2[spos] = start2;
	parms->out1[spos] = out1;
	parms->out2[spos] = out2;

	ATRACE_32("am_src1_up_dfs() returning", samples * factor);

	return (samples * factor);

}	/* am_src1_up_dfs() */
