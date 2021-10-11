/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef	_MIXER_IMPL_H
#define	_MIXER_IMPL_H

#pragma ident	"@(#)mixer_impl.h	1.6	99/12/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/dditypes.h>
#include <sys/audio_impl.h>
#include <sys/mixer.h>
#include <sys/audioio.h>
#include <sys/audiovar.h>

#define	AM_MAX_GAIN_SHIFT		8	/* 1 more then 255, but close */
#define	AM_BALANCE_SHIFT		5

#define	AM_MIN_CHS			1

#define	AUDIO_MIXER_CTL_STRUCT_SIZE(num_ch)	(sizeof (am_control_t) + \
					((num_ch - 1) * sizeof (int8_t)))

#define	AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(num_srs)		\
					(sizeof (am_sample_rates_t) + \
					((num_srs - 1) * sizeof (uint_t)))

/*
 * Defines and structures needed for an Audio Driver that uses the
 * audio mixer Audio Personality Module
 */

/*
 * am_ad_src_entry_t	- Audio Driver sample rate conversion routines
 */
struct am_ad_src_entry {
	int		*(*ad_src_convert)(audio_ch_t *, int, int *,
				int *, int *);
	void		(*ad_src_exit)(audio_ch_t *, int);
	int		(*ad_src_init)(audio_ch_t *, int);
	size_t		(*ad_src_size)(audio_ch_t *, int, int, int);
	int		(*ad_src_adjust)(audio_ch_t *, int, int);
};
typedef struct am_ad_src_entry am_ad_src_entry_t;

/*
 * am_ad_sample_rates_t		- supported sample rates
 */
struct am_ad_sample_rates {
	int		ad_limits;	/* 0 if sample rates not limits */
	uint_t		*ad_srs;	/* NULL term. list of sample rates */
};
typedef struct am_ad_sample_rates am_ad_sample_rates_t;

/* am_ad_ample_rates.ad_limits */
#define	MIXER_SRS_FLAG_SR_NOT_LIMITS	0x00000000u
						/* samp rates not limits */
#define	MIXER_SRS_FLAG_SR_LIMITS	MIXER_SR_LIMITS
						/* samp rates set limits */

/*
 * am_ad_ch_cap_t	- Audio Driver play/record capabilities
 */
struct am_ad_ch_cap {
	am_ad_sample_rates_t	ad_mixer_srs;	/* mixer mode sample rates */
	am_ad_sample_rates_t	ad_compat_srs;	/* compat mode sample rates */
	am_ad_src_entry_t	*ad_conv;	/* sample rate conv. routines */
	void			*ad_sr_info;	/* sample rate conv. info */
	uint_t			*ad_chs;	/* list of channel types */
	int			ad_int_rate;	/* interrupt rate */
	uint_t			ad_flags;	/* capability flags */
	size_t			ad_bsize;	/* buffer size */
};
typedef struct am_ad_ch_cap am_ad_ch_cap_t;

/* am_ad_ch_cap.ad_flags defines */
#define	MIXER_CAP_FLAG_SR_LIMITS	0x00000001u /* samp rates set limits */

/*
 * am_ad_cap_comb_t	- Audio Driver play/record capability combinations
 */
struct am_ad_cap_comb {
	int		ad_prec;	/* the data precision */
	int		ad_enc;		/* the data encoding method */
};
typedef struct am_ad_cap_comb am_ad_cap_comb_t;

/*
 * am_ad_entry_t	- Audio Driver ops vector definition
 */
struct am_ad_entry {
	int		(*ad_setup)(int instance, int stream, int dir);
	void		(*ad_teardown)(int instance, int stream);
	int		(*ad_set_config)(int instance, int stream,
				int command, int dir, int arg1, int arg2);
	int		(*ad_set_format)(int instance, int stream, int dir,
				int sample_rate, int channels,
				int precision, int encoding);
	int		(*ad_start_play)(int, int);
	void		(*ad_pause_play)(int, int);
	void		(*ad_stop_play)(int, int);
	int		(*ad_start_record)(int, int);
	void		(*ad_stop_record)(int, int);
	int		(*ad_ioctl)(int, int, queue_t *, mblk_t *);
	int		(*ad_iocdata)(int, int, queue_t *, mblk_t *);
};
typedef struct am_ad_entry am_ad_entry_t;

/* ad_set_config() and ad_set_format() stream # */
#define	AMAD_SET_CONFIG_BOARD	(-1)	/* for the whole board */

/* ad_set_config() commands */
#define	AMAD_SET_GAIN		0x01	/* set input/ouput channel gain */
#define	AMAD_SET_GAIN_BAL	0x02	/* set input/ouput channel gain */
#define	AMAD_SET_PORT		0x03	/* set input/output port */
#define	AMAD_SET_MONITOR_GAIN	0x04	/* set monitor gain */
#define	AMAD_OUTPUT_MUTE	0x05	/* mute output */
#define	AMAD_MONO_MIC		0x06	/* set which mono microphone */
#define	AMAD_MIC_BOOST		0x07	/* enable/disable mic preamp */
#define	AMAD_BASS_BOOST		0x08	/* boost output bass */
#define	AMAD_MID_BOOST		0x09	/* boost output mid range */
#define	AMAD_TREBLE_BOOST	0x0a	/* boost output treble */
#define	AMAD_LOUDNESS		0x0b	/* enable/disable output loudness */
#define	AMAD_SET_DIAG_MODE	0x0c	/* set diagnostics mode */

/*
 * am_ad_info_t	- Audio Driver configuration information structure
 */
struct am_ad_info {
	int		ad_int_vers;	/* Audio Driver interface version */
	int		ad_mode;	/* MIXER or COMPAT mode */
	uint_t		ad_add_mode;	/* additional mode information */
	int		ad_codec_type;	/* Codec type */
	audio_info_t	*ad_defaults;	/* Audio Driver audio_info_t struct */
	am_ad_ch_cap_t	ad_play;	/* play capabilities */
	am_ad_ch_cap_t	ad_record;	/* record capabilities */
	am_ad_cap_comb_t *ad_play_comb;	/* list of play cap. combinations */
	am_ad_cap_comb_t *ad_rec_comb;	/* list of rec cap. combinations */
	am_ad_entry_t	*ad_entry;	/* Audio Driver entry points */
	audio_device_t	*ad_dev_info;	/* device information */
	uint_t		ad_diag_flags;	/* flags that specify diagnostics sup */
	uint_t		ad_diff_flags;	/* format difference flags */
	uint_t		ad_assist_flags; /* audio stream assist flags */
	uint_t		ad_misc_flags;	/* misc. flags */
	uint_t		ad_translate_flags; /* translate flags */
	int		ad_num_mics;	/* # of mic inputs */
	uint_t		_xxx[4];	/* reserved for future use */
};
typedef struct am_ad_info am_ad_info_t;

/* am_ad_info.ad_int_vers defines */
#define	AMAD_VERS1	1		/* Supported interface version */

/* am_ad_info.ad_add_mode defines */
#define	AM_ADD_MODE_DIAG_MODE	0x00000001u	/* dev supports diagnostics */
#define	AM_ADD_MODE_MIC_BOOST	0x00000002u	/* mic boost enabled */

/* am_ad_info.ad_codec_type defines */
#define	AM_TRAD_CODEC		0x00000001u	/* traditional Codec */
#define	AM_MS_CODEC		0x00000002u	/* multi-stream Codec */

/* am_ad_info.ad_diag_flags defines */
#define	AM_DIAG_INTERNAL_LOOP	0x00000001u	/* dev has internal loopbacks */

/* am_ad_info.ad_diff_flags defines */
#define	AM_DIFF_SR		0x00000001u	/* p/r sample rate may differ */
#define	AM_DIFF_CH		0x00000002u	/* p/r channels may differ */
#define	AM_DIFF_PREC		0x00000004u	/* p/r precision may differ */
#define	AM_DIFF_ENC		0x00000008u	/* p/r encoding may differ */

/* am_ad_info.ad_assist_flags defines */
#define	AM_ASSIST_BASE		0x00000001u	/* device has base boost */
#define	AM_ASSIST_MID		0x00000002u	/* device has mid range boost */
#define	AM_ASSIST_TREBLE	0x00000004u	/* device has treble boost */
#define	AM_ASSIST_LOUDNESS	0x00000008u	/* device has loudness boost */
#define	AM_ASSIST_MIC		0x00000010u	/* mic has preamp boost */

/* am_ad_info.ad_misc_flags defines */
#define	AM_MISC_PP_EXCL		0x00000001u	/* play ports are exclusive */
#define	AM_MISC_RP_EXCL		0x00000002u	/* record ports are exclusive */
#define	AM_MISC_MONO_MIC	0x00000004u	/* mono mic */
#define	AM_MISC_MONO_DUP	0x00000008u	/* mono is duped to all chs */

/* am_ad_info.ad_translate_flags */
#define	AM_MISC_8_P_TRANSLATE	0x00000001u	/* trans. signed to unsigned */
#define	AM_MISC_16_P_TRANSLATE	0x00000002u	/* trans. signed to unsigned */
#define	AM_MISC_8_R_TRANSLATE	0x00010000u	/* trans. unsigned to signed */
#define	AM_MISC_16_R_TRANSLATE	0x00020000u	/* trans. unsigned to signed */

/*
 * Defines and structures needed for the audio mixer Audio Personality Module
 */

/*
 * Miscellaneous defines
 */
#define	AM_MAX_STRICT_CH_OPEN		1

#define	AM_INT16_SHIFT			1
#define	AM_INT32_SHIFT			2
#define	AM_256_SHIFT			8
#define	AM_2_SHIFT			1
#define	AM_4_SHIFT			2
#define	AM_8_SHIFT			3
#define	AM_MOD_STEREO			2

#define	AM_MIN_MIX_BUFSIZE		4800
#define	AM_DEFAULT_MIX_BUFSIZE		(AM_MIN_MIX_BUFSIZE * 4 * 2)

#define	AM_DEFAULT_SR_SIZE		10

#define	AM_DEFAULT_MIXER_GAIN		(AUDIO_MAX_GAIN*3/4)

#define	AM_MAX_QUEUED_MSGS		4
#define	AM_MAX_QUEUED_MSGS_SIZE		(1024*48*4)	/* ~1 secs of audio */
#define	AM_MIN_QUEUED_MSGS_SIZE		(1024*48*2)	/* ~0.5 secs of audio */

#define	AM_WIOCDATA			0	/* returned by Audio Driver */
#define	AM_ACK				1	/* with private ioctl() & */
#define	AM_NACK				2	/* iocdata() routines */

/* audio mixer ioctl/iocdata commands */
#define	COPY_OUT_AUDIOINFO	(MIOC|1)	/* AUDIO_GETINFO */
#define	COPY_OUT_AUDIOINFO2	(MIOC|2)	/* AUDIO_SETINFO */
#define	COPY_IN_AUDIOINFO	(MIOC|3)	/* AUDIO_SETINFO */
#define	COPY_IN_DIAG_LOOPB	(MIOC|4)	/* AUDIO_DIAG_LOOPBACK */
#define	COPY_OUT_GETDEV		(MIOC|5)	/* AUDIO_GETDEV */
#define	COPY_OUT_SAMP_RATES	(MIOC|6)	/* AUDIO_MIXER_GET_SAMPLE... */
#define	COPY_IN_SAMP_RATES	(MIOC|7)	/* AUDIO_MIXER_GET_SAMPLE... */
#define	COPY_IN_SAMP_RATES2	(MIOC|8)	/* AUDIO_MIXER_GET_SAMPLE... */
#define	COPY_OUT_MIXCTLINFO	(MIOC|9)	/* AUDIO_MIXERCTL_GETINFO */
#define	COPY_IN_MIXCTLINFO	(MIOC|10)	/* AUDIO_MIXERCTL_SETINFO */
#define	COPY_OUT_MIXCTL_CHINFO	(MIOC|11)	/* AUDIO_MIXERCTL_GET_CHINFO */
#define	COPY_OUT_MIXCTL_CHINFO2	(MIOC|12)	/* AUDIO_MIXERCTL_GET_CHINFO */
#define	COPY_IN_MIXCTL_CHINFO	(MIOC|13) /* AUDIO_MIXERCTL_GET/SET_CHINFO */
#define	COPY_IN_MIXCTL_CHINFO2	(MIOC|14) /* AUDIO_MIXERCTL_GET/SET_CHINFO */
#define	COPY_OUT_MIXCTL_MODE	(MIOC|15)	/* AUDIO_MIXERCTL_GET_MODE */
#define	COPY_IN_MIXCTL_MODE	(MIOC|16)	/* AUDIO_MIXERCTL_SET_MODE */

/*
 * am_ch_private_t	- audio mixer channel private data
 */
struct am_ch_private {
	kmutex_t		src_lock;	/* sample rate conv. lock */
	uint_t			flags;		/* channel flags */
	boolean_t		reading;	/* true for RD channel */
	boolean_t		writing;	/* true for WR channel */
	audio_channel_t		*ioctl_tmp;	/* _MIXERCTL_GET/SET_CHANNEL */
	audio_info_t		*ioctl_tmp_info; /* part of ioctl_tmp */
	size_t			ioctl_size;	/* size of audio_channel_t */
	int			EOF[2];		/* # of EOF signals to send */
	int			EOF_toggle;	/* toggle for EOF signals */
	int			psamples_f;	/* sample frame count mp_orig */
	int			psamples_c;	/* samples in buf to play */
	int			psamples_p;	/* samples in played buf */
	mblk_t			*rec_mp;	/* record message block */
	int			rec_remaining;	/* # of bytes left in mp buf */
	int			*play_samp_buf;	/* play sample buffer space */
	size_t			psb_size;	/* size of play_samp_buf */
	void			*play_src_data;	/* play sample rate conv data */
	void			*rec_src_data;	/* rec. sample rate conv data */
	void			*ch_pptr1;	/* play src buffer #1 */
	void			*ch_pptr2;	/* play src buffer #2 */
	void			*ch_rptr1;	/* record src buffer #1 */
	void			*ch_rptr2;	/* record src buffer #2 */
	size_t			ch_psize1;	/* data size of ch_ptr1 */
	size_t			ch_psize2;	/* data size of ch_ptr2 */
	size_t			ch_rsize1;	/* data size of ch_ptr3 */
	size_t			ch_rsize2;	/* data size of ch_ptr4 */
};
typedef struct am_ch_private am_ch_private_t;

/* am_ch_private.flags defines */
#define	AM_CHNL_OPEN		0x0001u	/* channel open if set */
#define	AM_CHNL_MULTI_OPEN	0x0002u	/* PID may open multiple streams */
#define	AM_CHNL_DRAIN		0x0004u	/* want drain semantics if set */
#define	AM_CHNL_DRAIN_NEXT_INT	0x0008u	/* signal drain on next intr., step 1 */
#define	AM_CHNL_DRAIN_SIG	0x0010u	/* signal received during drain */
#define	AM_CHNL_CLOSING		0x0020u	/* the channel is being closed */
#define	AM_CHNL_ALMOST_EMPTY1	0x0040u	/* 0 data for ch, but data in DMA buf */
#define	AM_CHNL_ALMOST_EMPTY2	0x0080u	/* 0 data for ch, but data in DMA buf */
#define	AM_CHNL_EMPTY		0x0100u	/* the channel doesn't have any data */
#define	AM_CHNL_PCONTINUE	0x0200u	/* continue playing after a pause */
#define	AM_CHNL_RCONTINUE	0x0400u	/* continue recordingafter a pause */
#define	AM_CHNL_CONTROL		0x0800u	/* AUDIOCTL in same proc as AUDIO */
#define	AM_CHNL_CLIMIT		0x1000u	/* used to limit allocb() failed msgs */
#define	AM_CHNL_PFLOW		0x2000u	/* play side has been flow controlled */
#define	AM_CHNL_SIGNAL		0x4000u	/* channel is sending a signal */

/*
 * am_apm_private_t	- audio mixer state private data
 */
struct am_apm_private {
	kmutex_t		lock;		/* lock for cv */
	kcondvar_t		cv;		/* used to switch modes only */
	audio_info_t		hw_info;	/* hardware state */
	int			*mix_buf;	/* buffer to mix audio in */
	size_t			mix_size;	/* the size of the buffer */
	int			*send_buf;	/* buffer to send audio from */
	size_t			send_size;	/* the size of the buffer */
	int			flags;		/* flags for the audio mixer */
	int			active_ioctls;	/* number of active ioctls */
	int			open_waits;	/* number of procs waiting */
	uint_t			save_psr;	/* saved play sample rate */
	uint_t			save_pch;	/* saved play channels */
	uint_t			save_pprec;	/* saved play precision */
	uint_t			save_penc;	/* saved play encoding */
	uint_t			save_pgain;	/* saved play gain */
	uint_t			save_pbal;	/* saved play balance */
	uint_t			save_rsr;	/* saved record sample rate */
	uint_t			save_rch;	/* saved record channels */
	uint_t			save_rprec;	/* saved record precision */
	uint_t			save_renc;	/* saved record encoding */
	uint_t			save_rgain;	/* saved record gain */
	uint_t			save_rbal;	/* saved record balance */
	uint_t			save_hw_pgain;	/* saved h/w play gain */
	uint_t			save_hw_pbal;	/* saved h/w play balance */
	uint_t			save_hw_rgain;	/* saved h/w record gain */
	uint_t			save_hw_rbal;	/* saved h/w record balance */
	uint_t			save_hw_muted;	/* saved h/w output_muted */
};
typedef struct am_apm_private am_apm_private_t;

/* am_apm_private.flags defines */
#define	AM_PRIV_SW_MODES	0x00000001u	/* switch between M & C mode */
#define	AM_PRIV_PULAW_TRANS	0x00000002u	/* play convert u-law to PCM */
#define	AM_PRIV_PALAW_TRANS	0x00000004u	/* play convert A-law to PCM */
#define	AM_PRIV_RULAW_TRANS	0x00000008u	/* rec. convert u-law to PCM */
#define	AM_PRIV_RALAW_TRANS	0x00000010u	/* rec. convert A-law to PCM */

/*
 * Audio Mixer Driver Entry Point Routines
 */
int am_attach(dev_info_t *, ddi_attach_cmd_t, am_ad_info_t *, kmutex_t *,
	int *, int *, int *, int *, int *, int *);
int am_detach(dev_info_t *, ddi_detach_cmd_t);

/*
 * Audio Mixer Driver Device Dependent Driver Play Routines
 */
int am_get_audio(dev_info_t *, dev_t, void *, int, int);
void am_play_shutdown(dev_info_t *, dev_t, int);

/*
 * Audio Mixer Driver Device Dependent Driver Record Routines
 */
void am_send_audio(dev_info_t *, dev_t, void *, int, int);

/*
 * Audio Mixer Driver Device Dependent Driver Miscellaneous Routines
 */
int am_hw_state_change(dev_info_t *, dev_t);

/*
 * Audio Mixer Audio Driver Miscellaneous Routines
 */
void *am_get_src_data(audio_ch_t *, int);
void am_set_src_data(audio_ch_t *, int, void *);

/*
 * am_ad_src1_info_t	- mixer mode Audio Driver sample rate conversion #1 info
 */
struct am_ad_src1_info {
	uint_t		ad_from_sr;	/* going from this sample rate */
	uint_t		ad_to_sr;	/* to this sample rate */
	uint_t		ad_nconv;	/* number of conversions */
	uint_t		ad_u0;		/* up conversion parameter #1 */
	uint_t		ad_u1;		/* up conversion parameter #2 */
	uint_t		ad_u2;		/* up conversion parameter #3 */
	uint_t		ad_u3;		/* up conversion parameter #4 */
	uint_t		ad_d0;		/* conversion parameter #1 */
	uint_t		ad_d1;		/* conversion parameter #2 */
	uint_t		ad_d2;		/* conversion parameter #3 */
	uint_t		ad_d3;		/* conversion parameter #4 */
	uint_t		ad_k;		/* filter shift */
};
typedef struct am_ad_src1_info am_ad_src1_info_t;

#define	AM_FILTER		0x40000000	/* filter if ORed with ad_p?? */
#define	AM_MAX_CONVERSIONS	4		/* max # of up/down conv. */

/*
 * am_src1_data_t	- sample rate conversion algorithm #1 data
 */
#define	MUPSIZE1		4		/* max up/down conversions */
struct am_src1_data {
	uint_t		inFs;			/* input sample rate */
	uint_t		outFs;			/* device sample rate */
	int		k;			/* filter parameter */
	int		up[MUPSIZE1];		/* up sample steps */
	int		down[MUPSIZE1];		/* down sample steps */
	int		ustart1[MUPSIZE1];	/* up sample saved samples, L */
	int		ustart2[MUPSIZE1];	/* up sample saved samples, R */
	int		dstart[MUPSIZE1];	/* down sample saved samples */
	int		out1[MUPSIZE1];		/* down sample saved samples */
	int		out2[MUPSIZE1];		/* down sample saved samples */
	int		(*up0)(struct am_src1_data *, int, int,
				int *, int *, int);
	int		(*up1)(struct am_src1_data *, int, int,
				int *, int *, int);
	int		(*up2)(struct am_src1_data *, int, int,
				int *, int *, int);
	int		(*up3)(struct am_src1_data *, int, int,
				int *, int *, int);
	int		up_factor;		/* total up conversions */
	int		down_factor;		/* total down conversions */
	int		count;			/* number of up conversions */
};
typedef struct am_src1_data am_src1_data_t;

/* fudge factor to make sure we've got enough memory */
#define	AM_SRC1_BUFFER		(8 * sizeof (int))

extern am_ad_src_entry_t am_src1; /* audio mixer's sampe rate conv. entry pts */

#ifdef	__cplusplus
}
#endif

#endif	/* _MIXER_IMPL_H */
