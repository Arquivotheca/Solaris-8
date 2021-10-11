/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef	_MIXER_H
#define	_MIXER_H

#pragma ident	"@(#)mixer.h	1.2	99/08/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/audio.h>

#define	MIXER_NAME		"audio mixer"	/* STREAMS module name */
#define	MIXER_VERSION		"Rev 1"		/* 1st version of audio mixer */
#define	MIXER_CONFIGURATION	"Config A"	/* 1st configuration */
#define	MIXER_MOD_NAME		"Audio Mixer"	/* STREAMS modldrv name */

#define	AM_MIXER_MODE			0
#define	AM_COMPAT_MODE			1

#define	AM_DEFAULT_SAMPLERATE		8000
#define	AM_DEFAULT_CHANNELS		AUDIO_CHANNELS_MONO
#define	AM_DEFAULT_PRECISION		AUDIO_PRECISION_8
#define	AM_DEFAULT_ENCODING		AUDIO_ENCODING_ULAW
#define	AM_DEFAULT_GAIN			AUDIO_MID_GAIN

/*
 * Mixer ioctls.
 */
#define	MIOC				('M'<<8)
#define	AUDIO_MIXER_MULTIPLE_OPEN	(MIOC|10)
#define	AUDIO_MIXER_SINGLE_OPEN		(MIOC|11)
#define	AUDIO_MIXER_GET_SAMPLE_RATES	(MIOC|12)
#define	AUDIO_MIXERCTL_GETINFO		(MIOC|13)
#define	AUDIO_MIXERCTL_SETINFO		(MIOC|14)
#define	AUDIO_MIXERCTL_GET_CHINFO	(MIOC|15)
#define	AUDIO_MIXERCTL_SET_CHINFO	(MIOC|16)
#define	AUDIO_MIXERCTL_GET_MODE		(MIOC|17)
#define	AUDIO_MIXERCTL_SET_MODE		(MIOC|18)

#define	AUDIO_MIXER_CTL_STRUCT_SIZE(num_ch)	(sizeof (am_control_t) + \
					((num_ch - 1) * sizeof (int8_t)))

#define	AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(num_srs)		\
					(sizeof (am_sample_rates_t) + \
					((num_srs - 1) * sizeof (uint_t)))

/*
 * Mixer software features
 */
#define	AM_MIXER			0x00000001	/* audio mixer */

/*
 * am_control_t		- structure that holds information on the audio device
 */
struct am_control {
	/*
	 * Because a particular channel may be virtual, it isn't possible
	 * to use the normal ioctl()s to set the some of the hardware's state.
	 * Only the dev_info structure's play/record gain, balance, port, and
	 * pause members, as well as the monitor_gain and output_muted members
	 * may be modified.
	 */
	audio_info_t	dev_info;

	/*
	 * The mixer(7I) manual page shows an example of using the ch_open[]
	 * array. Each element that is set to 0 represents a channel which
	 * isn't allocated, and non-zero elements represent a channel that is
	 * alloacted. This size of this array may change, depending on the
	 * number of channels the audiosup module allocates per device.
	 */
	int8_t		ch_open[1];
};
typedef struct am_control am_control_t;

/*
 * am_sample_rates_t	- structure for a list of supported sample rates
 */
struct am_sample_rates {
	/*
	 * Set this to AUIDO_PLAY or AUDIO_RECORD, but not both, to get
	 * the play or record sample rates, respectively.
	 */
	uint_t		type;

	/*
	 * Some devices support a complete range of sample rates between the
	 * two provided in the samp_rates[] array. If this is so then this
	 * flag is set to MIXER_SR_LIMITS when AUDIO_MIXER_GET_SAMPLE_RATES
	 * returns this structure.
	 */
	uint_t		flags;

	/*
	 * Set this number to the number of sample rates to request. The
	 * mixer(7I) manual page shows an example of using this structure.
	 * When AUDIO_MIXER_GET_SAMPLE_RATES returns the number of samples
	 * available is set. This may be more or less than the number requested.
	 * If more that only the requested number of samples is arctually
	 * returned in the samp_rates array.
	 */
	uint_t		num_samp_rates;

	/*
	 * Variable size array for the supported sample rates. See the example
	 * in the mixer(7I) manual page for how to use this array.
	 */
	uint_t		samp_rates[1];
};
typedef struct am_sample_rates am_sample_rates_t;

/* am_sample_rates.flags defines */
#define	MIXER_SR_LIMITS		0x00000001u	/* sample rates set limits */

#ifdef	__cplusplus
}
#endif

#endif	/* _MIXER_H */
