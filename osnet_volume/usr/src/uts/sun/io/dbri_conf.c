/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)dbri_conf.c	1.22	99/05/21 SMI"

/*
 * Sun DBRI Dual Basic Rate Controller configuration file
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/isdnio.h>
#include <sys/dbriio.h>
#include <sys/dbrireg.h>
#include <sys/dbrivar.h>
#include <sys/audiodebug.h>

/*
 * DBRI ISDN global variables that are patchable by the end-user through
 * kadb.
 */

int Default_DBRI_nbf = 3;
int Default_DBRI_f = B_FALSE;

/*
 * NT T101 timer in milliseconds (range: 5-30 seconds)
 */
int Default_T101_timer = 6000;

/*
 * NT T102 timer in milliseconds (range: 25-100 milliseconds)
 */
int Default_T102_timer = 33;

/*
 * TE T103 timer in milliseconds (range: 5-30 seconds)
 */
int Default_T103_timer = 6000;

/*
 * TE T104 timer in milliseconds (range: 0,500-1000 milliseconds)
 */
int Default_T104_timer = 600;

/*
 * DBRI keep-alive timer in milliseconds
 */
int Keepalive_timer = 4000;

/*
 * Dbri_panic - If B_TRUE, a fatal error causes a system panic, otherwise
 * the unit causing the error is disabled and the system will continue
 * without it.
 *
 * Dbri_fcscheck - If B_TRUE, received HDLC packets have their FCS re-checked
 * in software to be doubly sure of no packets problems between the BRI
 * wire and the CPU.
 */
boolean_t Dbri_panic = B_FALSE;
#if defined(DBRI_SWFCS)
boolean_t Dbri_fcscheck = B_TRUE;
#endif

enum isdn_param_asmb Default_asmb = ISDN_PARAM_TE_ASMB_CTS2;
int Default_power = ISDN_PARAM_POWER_ON;


/*
 * Dbri_chanmap - This table maps an isdnio channel number to the
 * associated DBRI channels.
 */
static dbri_chanmap_t Dbri_chanmap[] = {
	/* ISDN chan,		DBRI transmit chan,	DBRI receive chan */

	{ISDN_CHAN_HOST,	DBRI_CHAN_HOST,	DBRI_CHAN_HOST},

	/* Management Ports */

	{ISDN_CHAN_CTLR_MGT,	DBRI_DBRIMGT,	DBRI_DBRIMGT},
	{ISDN_CHAN_TE_MGT,	DBRI_TEMGT,	DBRI_TEMGT},
	{ISDN_CHAN_NT_MGT,	DBRI_NTMGT,	DBRI_NTMGT},

	/* NT channel defines */
	{ISDN_CHAN_NT_D,	DBRI_NT_D_OUT,	DBRI_NT_D_IN},
	{ISDN_CHAN_NT_D_TRACE,	DBRI_NT_D_TRACE, DBRI_NT_D_TRACE},
	{ISDN_CHAN_NT_B1,	DBRI_NT_B1_OUT,	DBRI_NT_B1_IN},
	{ISDN_CHAN_NT_B2,	DBRI_NT_B2_OUT,	DBRI_NT_B2_IN},

	/* TE channel defines */
	{ISDN_CHAN_TE_D,	DBRI_TE_D_OUT,	DBRI_TE_D_IN},
	{ISDN_CHAN_TE_D_TRACE,	DBRI_TE_D_TRACE, DBRI_TE_D_TRACE},
	{ISDN_CHAN_TE_B1,	DBRI_TE_B1_OUT,	DBRI_TE_B1_IN},
	{ISDN_CHAN_TE_B2,	DBRI_TE_B2_OUT,	DBRI_TE_B2_IN},

	/* SBI defines */
	{ISDN_CHAN_AUX0,	DBRI_AUDIO_OUT,	DBRI_AUDIO_IN},
};
#define	NCHANS	(sizeof (Dbri_chanmap) / sizeof (Dbri_chanmap[0]))


/*
 * dbri_itod_fmt - XXX
 */
/*ARGSUSED*/
dbri_chanformat_t *
dbri_itod_fmt(dbri_unit_t *unitp, dbri_chantab_t *ctep, isdn_format_t *ifmt)
{
	int i;
	dbri_chanformat_t *dfmt;

	if (ctep == NULL || ctep->legal_formats == NULL) {
		ATRACE(dbri_itod_fmt, '!ctp', 0);
		return (NULL);
	}

	for (i = 0; ctep->legal_formats[i] != NULL; ++i) {
		dfmt = ctep->legal_formats[i];

		/*
		 * Audio encodings must match exactly.
		 */
		if (ifmt->encoding != dfmt->encoding)
			continue;

		/*
		 * Are modes compatible?
		 */
		if (ifmt->mode == ISDN_MODE_UNKNOWN) {
			break;
		} else if (ifmt->mode == ISDN_MODE_HDLC) {
			if (dfmt->mode != DBRI_MODE_HDLC_D &&
			    dfmt->mode != DBRI_MODE_HDLC)
				continue;
		} else if (ifmt->mode == ISDN_MODE_TRANSPARENT) {
			if (dfmt->mode != DBRI_MODE_TRANSPARENT)
				continue;
		}

		/*
		 * DBRI doesn't really know about stereo.
		 *
		 * XXX - If eight bit stereo is ever supported, this
		 * check gets more complicated.
		 */
		if (ifmt->precision * ifmt->channels != dfmt->llength)
			continue;

		if (ifmt->sample_rate != dfmt->lsamplerate)
			continue;

		if (ifmt->channels != dfmt->channels)
			continue;

		return (dfmt);	/* Success! */
	}

	return (NULL);
} /* dbri_itod_fmt */


/*
 * dbri_itod_chan - XXX
 */
dbri_chan_t
dbri_itod_chan(isdn_chan_t ichan, int dir)
{
	int i;

	for (i = 0; i < NCHANS; ++i) {
		if (Dbri_chanmap[i].channel == ichan) {
			if (dir == DBRI_DIRECTION_OUT)
				return (Dbri_chanmap[i].out_channel);
			else
				return (Dbri_chanmap[i].in_channel);
		}
	}
	return (DBRI_CHAN_NONE);
} /* dbri_itod_chan */



int Dbri_nstreams = DBRI_NSTREAM;

/*
 * Some channels have permission to manipulate other channels.
 * It is assumed in these structures that permission to manipulate the write
 * side implies permissions for the read side as well.
 */
dbri_chan_t dbri_scope_temgt[] = {
	DBRI_TE_D_OUT, DBRI_TE_B1_OUT, DBRI_TE_B2_OUT,
};

dbri_chan_t dbri_scope_ntmgt[] = {
	DBRI_NT_D_OUT, DBRI_NT_B1_OUT, DBRI_NT_B2_OUT,
};

dbri_chan_t dbri_scope_audioctl[] = {
	DBRI_AUDIO_OUT,
};

dbri_chan_t dbri_scope_mgt[] = {
	DBRI_TE_D_OUT, DBRI_TE_B1_OUT, DBRI_TE_B2_OUT,
	DBRI_NT_D_OUT, DBRI_NT_B1_OUT, DBRI_NT_B2_OUT,
	DBRI_AUDIO_OUT,
};

#define	SCOPE(foo)	(sizeof (foo) / sizeof (foo[0])), foo
struct {
	dbri_chan_t	mgt;
	int		n;
	dbri_chan_t	*controlled;
} dbri_scope[] = {
	{ DBRI_DBRIMGT,	SCOPE(dbri_scope_mgt)},
	{ DBRI_TEMGT,	SCOPE(dbri_scope_temgt)},
	{ DBRI_NTMGT,	SCOPE(dbri_scope_ntmgt)},
	{ DBRI_AUDIOCTL, SCOPE(dbri_scope_audioctl)},
	{ 0,		0, 0}
};


/*
 * dbri_isowner - XXX
 */
boolean_t
dbri_isowner(dbri_stream_t *ds, isdn_chan_t ichan)
{
	dbri_unit_t *unitp;
	dbri_chan_t dchan;
	int i;

	unitp = DsToUnitp(ds);

	if (ichan == ISDN_CHAN_NONE)
		return (B_FALSE);

	/*
	 * Any audiostream can manipulate itself
	 */
	if (ichan == ISDN_CHAN_HOST || ichan == ISDN_CHAN_SELF)
		return (B_TRUE);

	dchan = dbri_itod_chan(ichan, DBRI_DIRECTION_OUT);
	if (dchan == DBRI_CHAN_NONE)
		return (B_FALSE);

	/*
	 * An audiostream or its control stream can configure both
	 * the play and record sides.
	 */
	if (AsToDs(DsToAs(ds)->output_as) == DChanToDs(unitp, dchan))
		return (B_TRUE);

	/*
	 * The dbri_stream must be a management stream.
	 */
	for (i = 0; dbri_scope[i].n != 0; ++i) {
		int j;

		if (ds != DChanToDs(unitp, dbri_scope[i].mgt))
			continue;
		for (j = 0; j < dbri_scope[i].n; ++j) {
			if (dbri_scope[i].controlled[j] == dchan)
				return (B_TRUE);
		}
	}

	return (B_FALSE);
} /* dbri_isowner */


/* --- */

/* DBRI_TE_D_OUT */
dbri_chanformat_t DBRI_TE_D_OUT_fmt = {
	2,		/* physical len */
	8,		/* logical length */
	DBRI_MODE_HDLC_D, /* mode */
	8000,		/* physical samples */
	2000,		/* logical samples */
	1,		/* chans */
	4,		/* numbufs */		/* XXX - Hanging w/ 2??? */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_D_IN */
dbri_chanformat_t DBRI_TE_D_IN_fmt = {
	2,		/* physical len */
	8,		/* logical length */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* physical samples */
	2000,		/* logial samples */
	1,		/* chans */
	6,		/* numbufs */
	288,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_B1_OUT */
dbri_chanformat_t DBRI_TE_B1_OUT_fmt = {
	8,		/* physical len */
	8,		/* logical length */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* samples */
	8000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_B1_IN */
dbri_chanformat_t DBRI_TE_B1_IN_fmt = {
	8,		/* physical len */
	8,		/* logical len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* physical samples */
	8000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	4100,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_H_OUT */
dbri_chanformat_t DBRI_TE_H_OUT_fmt = {
	16,		/* physical len */
	8,		/* logical len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* phsyical samples */
	16000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_H_IN */
dbri_chanformat_t DBRI_TE_H_IN_fmt = {
	16,		/* phsyical len */
	8,		/* logical len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* phsyical samples */
	16000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	4100,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_B1_56_OUT */
dbri_chanformat_t DBRI_TE_B1_56_OUT_fmt = {
	7,		/* phsyical len */
	8,		/* logicial len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* phsyical samples */
	7000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_B1_56_IN */
dbri_chanformat_t DBRI_TE_B1_56_IN_fmt = {
	7,		/* physical len */
	8,		/* logical len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* physical samples */
	7000,		/* logical samples */
	1,		/* chans */
	8,		/* numbufs */
	4100,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_TE_B1_TR_OUT */
dbri_chanformat_t DBRI_TE_B1_TR_OUT_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	8,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_ULAW /* encoding */
};

/* DBRI_TE_B1_TR_IN */
dbri_chanformat_t DBRI_TE_B1_TR_IN_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	8,		/* numbufs */
	1000,		/* buffer_size */
	AUDIO_ENCODING_ULAW /* encoding */
};


/* DBRI_TE_B1_TR_ALAW_OUT */
dbri_chanformat_t DBRI_TE_B1_TR_ALAW_OUT_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	8,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_ALAW /* encoding */
};

/* DBRI_TE_B1_TR_ALAW_IN */
dbri_chanformat_t DBRI_TE_B1_TR_ALAW_IN_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	8,		/* numbufs */
	1000,		/* buffer_size */
	AUDIO_ENCODING_ALAW /* encoding */
};



/* DBRI_NT_D_OUT */
dbri_chanformat_t DBRI_NT_D_OUT_fmt = {
	2,		/* physical len */
	8,		/* logical len */
	DBRI_MODE_HDLC,	/* mode */
	8000,		/* phsyical samples */
	2000,		/* logical samples */
	1,		/* chans */
	4,		/* numbufs */		/* XXX - Hanging w/ 2??? */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};

/* DBRI_AUDIO_OUT */
dbri_chanformat_t DBRI_AUDIO_OUT_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	30,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_ULAW /* encoding */
};

/* DBRI_AUDIO_IN */
dbri_chanformat_t DBRI_AUDIO_IN_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	16,		/* numbufs */
	256,		/* buffer_size */
	AUDIO_ENCODING_ULAW /* encoding */
};

/* DBRI_AUDIO_ALAW_OUT */
dbri_chanformat_t DBRI_AUDIO_ALAW_OUT_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	30,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_ALAW /* encoding */
};

/* DBRI_AUDIO_ALAW_IN */
dbri_chanformat_t DBRI_AUDIO_ALAW_IN_fmt = {
	8,		/* len */
	8,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	16,		/* numbufs */
	256,		/* buffer_size */
	AUDIO_ENCODING_ALAW /* encoding */
};

/* DBRI_AUDIO_32_OUT */
dbri_chanformat_t DBRI_AUDIO_32_OUT_fmt = {
	32,		/* len */
	32,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	2,		/* chans */
	30,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_LINEAR /* encoding */
};

/* DBRI_AUDIO_32_IN */
dbri_chanformat_t DBRI_AUDIO_32_IN_fmt = {
	32,		/* len */
	32,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	2,		/* chans */
	16,		/* numbufs */
	4000,		/* buffer_size */
	AUDIO_ENCODING_LINEAR /* encoding */
};

/* DBRI_AUDIO_16_OUT */
dbri_chanformat_t DBRI_AUDIO_16_OUT_fmt = {
	16,		/* len */
	16,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	30,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_LINEAR /* encoding */
};

/* DBRI_AUDIO_16_IN */
dbri_chanformat_t DBRI_AUDIO_16_IN_fmt = {
	16,		/* len */
	16,		/* len */
	DBRI_MODE_TRANSPARENT, /* mode */
	8000,		/* samples */
	8000,		/* samples */
	1,		/* chans */
	16,		/* numbufs */
	2000,		/* buffer_size */
	AUDIO_ENCODING_LINEAR /* encoding */
};

/* DBRI_AUDIOCTL */
dbri_chanformat_t DBRI_AUDIOCTL_fmt = {
	0,		/* len */
	0,		/* len */
	DBRI_MODE_UNKNOWN, /* mode */
	0,		/* samples */
	0,		/* samples */
	0,		/* chans */
	0,		/* numbufs */
	0,		/* buffer_size */
	AUDIO_ENCODING_NONE /* encoding */
};


/*
 * Legal format sets
 */
dbri_chanformat_t *DBRI_AUDIO_IN_formats[] = {
	/* ISDN_CHAN_AUX0,DBRI_PIPE_CHI,0,DBRI_DIRECTION_IN */
	&DBRI_AUDIO_IN_fmt,
	&DBRI_AUDIO_ALAW_IN_fmt,
	&DBRI_AUDIO_32_IN_fmt,
	&DBRI_AUDIO_16_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_AUDIO_OUT_formats[] = {
	/* ISDN_CHAN_AUX0,DBRI_PIPE_CHI,64,DBRI_DIRECTION_OUT */
	&DBRI_AUDIO_OUT_fmt,
	&DBRI_AUDIO_ALAW_OUT_fmt,
	&DBRI_AUDIO_32_OUT_fmt,
	&DBRI_AUDIO_16_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_AUDIOCTL_formats[] = {
	/* ISDN_CHAN_AUX0_MGT,DBRI_PIPE_CHI,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_DBRIMGT_formats[] = {
	/* ISDN_CHAN_CTLR_MGT,DBRI_BAD_PIPE,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_HOST_CHAN_formats[] = {
	/* ISDN_CHAN_HOST,DBRI_BAD_PIPE,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_B1_IN_formats[] = {
	/* ISDN_CHAN_NT_B1,DBRI_PIPE_NT_D_IN,0,DBRI_DIRECTION_IN */
	&DBRI_TE_B1_IN_fmt,
	&DBRI_TE_H_IN_fmt,
	&DBRI_TE_B1_56_IN_fmt,
	&DBRI_TE_B1_TR_IN_fmt,
	&DBRI_TE_B1_TR_ALAW_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_B1_OUT_formats[] = {
	/* ISDN_CHAN_NT_B1,DBRI_PIPE_NT_D_OUT,0,DBRI_DIRECTION_OUT */
	&DBRI_TE_B1_OUT_fmt,
	&DBRI_TE_H_OUT_fmt,
	&DBRI_TE_B1_56_OUT_fmt,
	&DBRI_TE_B1_TR_OUT_fmt,
	&DBRI_TE_B1_TR_ALAW_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_B2_IN_formats[] = {
	/* ISDN_CHAN_NT_B2,DBRI_PIPE_NT_D_IN,8,DBRI_DIRECTION_IN */
	&DBRI_TE_B1_IN_fmt,
	&DBRI_TE_B1_56_IN_fmt,
	&DBRI_TE_B1_TR_IN_fmt,
	&DBRI_TE_B1_TR_ALAW_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_B2_OUT_formats[] = {
	/* ISDN_CHAN_NT_B2,DBRI_PIPE_NT_D_OUT,8,DBRI_DIRECTION_OUT */
	&DBRI_TE_B1_OUT_fmt,
	&DBRI_TE_B1_56_OUT_fmt,
	&DBRI_TE_B1_TR_OUT_fmt,
	&DBRI_TE_B1_TR_ALAW_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_D_IN_formats[] = {
	/* ISDN_CHAN_NT_D,DBRI_PIPE_NT_D_IN,17,DBRI_DIRECTION_IN */
	&DBRI_TE_D_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_D_OUT_formats[] = {
	/* ISDN_CHAN_NT_D,DBRI_PIPE_NT_D_OUT,17,DBRI_DIRECTION_OUT */
	&DBRI_NT_D_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_NT_D_TRACE_formats[] = {
	/* ISDN_CHAN_NT_D_TRACE,DBRI_BAD_PIPE,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_NTMGT_formats[] = {
	/* ISDN_CHAN_NT_MGT,DBRI_PIPE_NT_D_IN,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_B1_IN_formats[] = {
	/* ISDN_CHAN_TE_B1,DBRI_PIPE_TE_D_IN,0,DBRI_DIRECTION_IN */
	&DBRI_TE_B1_IN_fmt,
	&DBRI_TE_H_IN_fmt,
	&DBRI_TE_B1_56_IN_fmt,
	&DBRI_TE_B1_TR_IN_fmt,
	&DBRI_TE_B1_TR_ALAW_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_B1_OUT_formats[] = {
	/* ISDN_CHAN_TE_B1,DBRI_PIPE_TE_D_OUT,0,DBRI_DIRECTION_OUT */
	&DBRI_TE_B1_OUT_fmt,
	&DBRI_TE_H_OUT_fmt,
	&DBRI_TE_B1_56_OUT_fmt,
	&DBRI_TE_B1_TR_OUT_fmt,
	&DBRI_TE_B1_TR_ALAW_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_B2_IN_formats[] = {
	/* ISDN_CHAN_TE_B2,DBRI_PIPE_TE_D_IN,8,DBRI_DIRECTION_IN */
	&DBRI_TE_B1_IN_fmt,
	&DBRI_TE_B1_56_IN_fmt,
	&DBRI_TE_B1_TR_IN_fmt,
	&DBRI_TE_B1_TR_ALAW_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_B2_OUT_formats[] = {
	/* ISDN_CHAN_TE_B2,DBRI_PIPE_TE_D_OUT,8,DBRI_DIRECTION_OUT */
	&DBRI_TE_B1_OUT_fmt,
	&DBRI_TE_B1_56_OUT_fmt,
	&DBRI_TE_B1_TR_OUT_fmt,
	&DBRI_TE_B1_TR_ALAW_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_D_IN_formats[] = {
	/* ISDN_CHAN_TE_D,DBRI_PIPE_TE_D_IN,17,DBRI_DIRECTION_IN */
	&DBRI_TE_D_IN_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_D_OUT_formats[] = {
	/* ISDN_CHAN_TE_D,DBRI_PIPE_TE_D_OUT,17,DBRI_DIRECTION_OUT */
	&DBRI_TE_D_OUT_fmt,
	0
};
dbri_chanformat_t *DBRI_TE_D_TRACE_formats[] = {
	/* ISDN_CHAN_TE_D_TRACE,DBRI_BAD_PIPE,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};
dbri_chanformat_t *DBRI_TEMGT_formats[] = {
	/* ISDN_CHAN_TE_MGT,DBRI_PIPE_TE_D_IN,0,DBRI_DIRECTION_NONE */
	&DBRI_AUDIOCTL_fmt,
	0
};


/*
 * Channel definitions
 */
static dbri_chantab_t Dbri_chantab[] = {
{
	DBRI_TE_D_OUT,		/* DBRI_chan */
	ISDN_CHAN_TE_D,		/* ISDN_chan */
	DBRI_PIPE_TE_D_OUT,	/* base_pipe */
	17,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_TE_D_OUT_fmt,
	DBRI_TE_D_OUT_formats,
	DBRI_TE_D_IN,		/* input_as */
	DBRI_TE_D_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_BOTH,		/* audtype */
	DBRI_MINOR_TE_D,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_D_OUT",	/* name */
	"te,d",			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_D_IN,		/* DBRI_chan */
	ISDN_CHAN_TE_D,		/* ISDN_chan */
	DBRI_PIPE_TE_D_IN,	/* base_pipe */
	17,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_D_IN_fmt,
	DBRI_TE_D_IN_formats,
	DBRI_TE_D_IN,		/* input_as */
	DBRI_TE_D_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_BOTH,		/* audtype */
	DBRI_MINOR_NODEV,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_D_IN",		/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_B1_OUT,		/* DBRI_chan */
	ISDN_CHAN_TE_B1,	/* ISDN_chan */
	DBRI_PIPE_TE_D_OUT,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_TE_B1_OUT_fmt,
	DBRI_TE_B1_OUT_formats,
	DBRI_TE_B1_IN,		/* input_as */
	DBRI_TE_B1_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_TE_B1_RW,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_B1_OUT",	/* name */
	"te,b1",			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_B1_IN,		/* DBRI_chan */
	ISDN_CHAN_TE_B1,	/* ISDN_chan */
	DBRI_PIPE_TE_D_IN,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_B1_IN_fmt,
	DBRI_TE_B1_IN_formats,
	DBRI_TE_B1_IN,		/* input_as */
	DBRI_TE_B1_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_TE_B1_RO,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_B1_IN",	/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_B2_OUT,		/* DBRI_chan */
	ISDN_CHAN_TE_B2,	/* ISDN_chan */
	DBRI_PIPE_TE_D_OUT,	/* base_pipe */
	8,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_TE_B1_OUT_fmt,
	DBRI_TE_B2_OUT_formats,
	DBRI_TE_B2_IN,		/* input_as */
	DBRI_TE_B2_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_TE_B2_RW,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_B2_OUT",	/* name */
	"te,b2",			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_B2_IN,		/* DBRI_chan */
	ISDN_CHAN_TE_B2,	/* ISDN_chan */
	DBRI_PIPE_TE_D_IN,	/* base_pipe */
	8,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_B1_IN_fmt,
	DBRI_TE_B2_IN_formats,
	DBRI_TE_B2_IN,		/* input_as */
	DBRI_TE_B2_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_TE_B2_RO,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TE_B2_IN",	/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_D_OUT,		/* DBRI_chan */
	ISDN_CHAN_NT_D,		/* ISDN_chan */
	DBRI_PIPE_NT_D_OUT,	/* base_pipe */
	17,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_NT_D_OUT_fmt,
	DBRI_NT_D_OUT_formats,
	DBRI_NT_D_IN,		/* input_as */
	DBRI_NT_D_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_BOTH,		/* audtype */
	DBRI_MINOR_NT_D,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_D_OUT",	/* name */
	"nt,d",			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_D_IN,		/* DBRI_chan */
	ISDN_CHAN_NT_D,		/* ISDN_chan */
	DBRI_PIPE_NT_D_IN,	/* base_pipe */
	17,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_D_IN_fmt,
	DBRI_NT_D_IN_formats,
	DBRI_NT_D_IN,		/* input_as */
	DBRI_NT_D_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_BOTH,		/* audtype */
	DBRI_MINOR_NODEV,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_D_IN",		/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_B1_OUT,		/* DBRI_chan */
	ISDN_CHAN_NT_B1,	/* ISDN_chan */
	DBRI_PIPE_NT_D_OUT,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_TE_B1_OUT_fmt,
	DBRI_NT_B1_OUT_formats,
	DBRI_NT_B1_IN,		/* input_as */
	DBRI_NT_B1_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_NT_B1_RW,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_B1_OUT",	/* name */
	"nt,b1",			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_B1_IN,		/* DBRI_chan */
	ISDN_CHAN_NT_B1,	/* ISDN_chan */
	DBRI_PIPE_NT_D_IN,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_B1_IN_fmt,
	DBRI_NT_B1_IN_formats,
	DBRI_NT_B1_IN,		/* input_as */
	DBRI_NT_B1_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_NT_B1_RO,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_B1_IN",	/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_B2_OUT,		/* DBRI_chan */
	ISDN_CHAN_NT_B2,	/* ISDN_chan */
	DBRI_PIPE_NT_D_OUT,	/* base_pipe */
	8,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_TE_B1_OUT_fmt,
	DBRI_NT_B2_OUT_formats,
	DBRI_NT_B2_IN,		/* input_as */
	DBRI_NT_B2_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_NT_B2_RW,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_B2_OUT",	/* name */
	"nt,b2",		/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_B2_IN,		/* DBRI_chan */
	ISDN_CHAN_NT_B2,	/* ISDN_chan */
	DBRI_PIPE_NT_D_IN,	/* base_pipe */
	8,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_TE_B1_IN_fmt,
	DBRI_NT_B2_IN_formats,
	DBRI_NT_B2_IN,		/* input_as */
	DBRI_NT_B2_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_NT_B2_RO,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NT_B2_IN",	/* name */
	NULL,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_AUDIO_OUT,		/* DBRI_chan */
	ISDN_CHAN_AUX0,		/* ISDN_chan */
	DBRI_PIPE_CHI,		/* base_pipe */
	64,			/* cycle */
	DBRI_DIRECTION_OUT,	/* direction */
	&DBRI_AUDIO_OUT_fmt,
	DBRI_AUDIO_OUT_formats,
	DBRI_AUDIO_IN,		/* input_as */
	DBRI_AUDIO_OUT,		/* output_as */
	DBRI_AUDIOCTL,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_AUDIO_RW,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_AUDIO_OUT",	/* name */
	"aux,0",		/* device_name */
	DBRI_DEV_ONBRD1		/* device type */
	},
{
	DBRI_AUDIO_IN,		/* DBRI_chan */
	ISDN_CHAN_AUX0,		/* ISDN_chan */
	DBRI_PIPE_CHI,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_IN,	/* direction */
	&DBRI_AUDIO_IN_fmt,
	DBRI_AUDIO_IN_formats,
	DBRI_AUDIO_IN,		/* input_as */
	DBRI_AUDIO_OUT,		/* output_as */
	DBRI_AUDIOCTL,		/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_DATA,		/* audtype */
	DBRI_MINOR_AUDIO_RO,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_AUDIO_IN",	/* name */
	NULL,			/* device_name */
	DBRI_DEV_ONBRD1		/* device type */
	},
{
	DBRI_AUDIOCTL,		/* DBRI_chan */
	ISDN_CHAN_AUX0_MGT,	/* ISDN_chan */
	DBRI_PIPE_CHI,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_AUDIOCTL_formats,
	DBRI_AUDIO_IN,		/* input_as */
	DBRI_AUDIO_OUT,		/* output_as */
	DBRI_AUDIOCTL,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_AUDIOCTL,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_AUDIOCTL",	/* name */
	"aux,0ctl",		/* device_name */
	DBRI_DEV_ONBRD1		/* device type */
	},
{
	DBRI_TEMGT,		/* DBRI_chan */
	ISDN_CHAN_TE_MGT,	/* ISDN_chan */
	DBRI_PIPE_TE_D_IN,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_TEMGT_formats,
	DBRI_TE_D_IN,		/* input_as */
	DBRI_TE_D_OUT,		/* output_as */
	DBRI_TEMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_TEMGT,	/* minor_device */
	ISDN_TYPE_TE,		/* isdn_type */
	"DBRI_TEMGT",		/* name */
	"te,mgt",		/* device_name */
	DBRI_DEV_ISDN_B,	/* device type */
	},
{
	DBRI_NTMGT,		/* DBRI_chan */
	ISDN_CHAN_NT_MGT,	/* ISDN_chan */
	DBRI_PIPE_NT_D_IN,	/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_NTMGT_formats,
	DBRI_NT_D_IN,		/* input_as */
	DBRI_NT_D_OUT,		/* output_as */
	DBRI_NTMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_NTMGT,	/* minor_device */
	ISDN_TYPE_NT,		/* isdn_type */
	"DBRI_NTMGT",		/* name */
	"nt,mgt",		/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_DBRIMGT,		/* DBRI_chan */
	ISDN_CHAN_CTLR_MGT,	/* ISDN_chan */
	DBRI_BAD_PIPE,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_DBRIMGT_formats,
	DBRI_DBRIMGT,		/* input_as */
	DBRI_DBRIMGT,		/* output_as */
	DBRI_DBRIMGT,		/* control_as */
	B_TRUE,			/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_DBRIMGT,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_DBRIMGT",		/* name */
	"mgt,mgt",		/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_TE_D_TRACE,	/* DBRI_chan */
	ISDN_CHAN_TE_D_TRACE,	/* ISDN_chan */
	DBRI_BAD_PIPE,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_TE_D_TRACE_formats,
	DBRI_TE_D_TRACE,	/* input_as */
	DBRI_TE_D_TRACE,	/* output_as */
	DBRI_TE_D_TRACE,	/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_TE_D_TRACE,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_TE_D_TRACE",	/* name */
	"te,dtrace",		/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_NT_D_TRACE,	/* DBRI_chan */
	ISDN_CHAN_NT_D_TRACE,	/* ISDN_chan */
	DBRI_BAD_PIPE,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_NT_D_TRACE_formats,
	DBRI_NT_D_TRACE,	/* input_as */
	DBRI_NT_D_TRACE,	/* output_as */
	DBRI_NT_D_TRACE,	/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_CONTROL,	/* audtype */
	DBRI_MINOR_NT_D_TRACE,	/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_NT_D_TRACE",	/* name */
	"nt,dtrace",		/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	},
{
	DBRI_CHAN_HOST,		/* DBRI_chan */
	ISDN_CHAN_HOST,		/* ISDN_chan */
	DBRI_BAD_PIPE,		/* base_pipe */
	0,			/* cycle */
	DBRI_DIRECTION_NONE,	/* direction */
	&DBRI_AUDIOCTL_fmt,
	DBRI_HOST_CHAN_formats,
	0,			/* input_as */
	0,			/* output_as */
	0,			/* control_as */
	B_FALSE,		/* signal_okay */
	AUDTYPE_NONE,		/* audtype */
	0,			/* minor_device */
	ISDN_TYPE_OTHER,	/* isdn_type */
	"DBRI_HOST_CHAN",	/* name */
	0,			/* device_name */
	DBRI_DEV_ISDN_B		/* device type */
	}
};
/* --- */

#define	NDBRICHANNELS	(sizeof (Dbri_chantab) / sizeof (Dbri_chantab[0]))


/*
 * dbri_ichan_to_ctep - XXX
 */
dbri_chantab_t *
dbri_ichan_to_ctep(isdn_chan_t ichan, int dir)
{
	dbri_chantab_t *ctep;	/* Channel table entry pointer */
	dbri_chan_t dchan;

	dchan = dbri_itod_chan(ichan, dir);
	ctep = dbri_dchan_to_ctep(dchan);

	return (ctep);
} /* dbri_ich_to_ctep */


/*
 * dbri_dchan_to_ctep - XXX
 */
dbri_chantab_t *
dbri_dchan_to_ctep(dbri_chan_t dchan)
{
	dbri_chantab_t *ctep;	/* Channel table entry pointer */

	if (dchan == DBRI_CHAN_NONE)
		return (NULL);

	/*
	 * Loop through conf table searching for matching minordev.
	 */
	for (ctep = &Dbri_chantab[0]; ctep < &Dbri_chantab[NDBRICHANNELS];
	    ++ctep) {
		if (ctep->dchan == dchan)
			return (ctep);
	}

	return (NULL);
} /* dbri_dchan_to_ctep */


/*
 * dbri_chantab_first - XXX
 */
dbri_chantab_t *
dbri_chantab_first()
{
	return (&Dbri_chantab[0]);
} /* dbri_chantab_first */


/*
 * dbri_chantab_next - XXX
 */
dbri_chantab_t *
dbri_chantab_next(dbri_chantab_t *ctep)
{
	++ctep;
	if ((ctep >= &Dbri_chantab[0]) && (ctep < &Dbri_chantab[NDBRICHANNELS]))
		return (ctep);
	return ((dbri_chantab_t *)0);
} /* dbri_chantab_next */


/*
 * dbri_format_eq - XXX
 */
boolean_t
dbri_format_eq(dbri_chanformat_t *cfp, isdn_format_t *ifmt)
{
	switch (ifmt->mode) {
	case ISDN_MODE_HDLC:
		switch (cfp->mode) {
		case DBRI_MODE_HDLC:
		case DBRI_MODE_HDLC_D:
			break;
		case DBRI_MODE_UNKNOWN:
		case DBRI_MODE_TRANSPARENT:
		case DBRI_MODE_SERIAL:
		case DBRI_MODE_FIXED:
		default:
			return (B_FALSE);
		}
		break;

	case ISDN_MODE_TRANSPARENT:
		switch (cfp->mode) {
		case DBRI_MODE_TRANSPARENT:
			break;
		case DBRI_MODE_HDLC:
		case DBRI_MODE_HDLC_D:
		case DBRI_MODE_UNKNOWN:
		case DBRI_MODE_SERIAL:
		case DBRI_MODE_FIXED:
		default:
			return (B_FALSE);
		}
		break;

	case ISDN_MODE_UNKNOWN:
	default:
		return (B_FALSE);
	}

	if ((cfp->llength/cfp->channels) != ifmt->precision)
		return (B_FALSE);
	if (cfp->lsamplerate != ifmt->sample_rate)
		return (B_FALSE);

	if (cfp->channels != ifmt->channels)
		return (B_FALSE);
	if (cfp->encoding != ifmt->encoding)
		return (B_FALSE);
	return (B_TRUE);
} /* dbri_format_eq */


boolean_t
dbri_dtoi_fmt(dbri_chanformat_t *dfmt, isdn_format_t *ifmt)
{
	switch (dfmt->mode) {
	case DBRI_MODE_HDLC:
	case DBRI_MODE_HDLC_D:
		ifmt->mode = ISDN_MODE_HDLC;
		break;

	case DBRI_MODE_UNKNOWN:
	case DBRI_MODE_TRANSPARENT:
		ifmt->mode = ISDN_MODE_TRANSPARENT;
		break;

	case DBRI_MODE_SERIAL:
	case DBRI_MODE_FIXED:
	default:
		ifmt->mode = ISDN_MODE_UNKNOWN;
		break;
	}

	ifmt->sample_rate = dfmt->lsamplerate;
	ifmt->channels = dfmt->channels;
	ifmt->precision = dfmt->llength / dfmt->channels;
	ifmt->encoding = dfmt->encoding;
	ifmt->reserved[0] = 0;
	ifmt->reserved[1] = 0;
	ifmt->reserved[2] = 0;

	return (B_TRUE);
} /* dbri_dtoi_format */


/*
 * dbri_isvalidformat - return true if the format pointed to by fp is a
 * valid format for the DBRI channel "dc";
 */
boolean_t
dbri_isvalidformat(dbri_chan_t dchan, isdn_format_t *fp)
{
	dbri_chantab_t *ctep;
	dbri_chanformat_t *cfp;
	int i;

	ctep = dbri_dchan_to_ctep(dchan);
	if (ctep->dchan == DBRI_CHAN_HOST)
		return (B_TRUE);
	for (i = 0, cfp = ctep->legal_formats[0];
	    cfp != NULL;
	    cfp = ctep->legal_formats[++i]) {
		if (dbri_format_eq(cfp, fp))
			return (B_TRUE);
	}
	return (B_FALSE);
} /* dbri_isvalidformat */


/*
 * dbri_set_default_format - Assign the channel's default format if needed.
 */
boolean_t
dbri_set_default_format(dbri_stream_t *ds)
{
	if (ds->ctep == NULL)
		return (B_FALSE);
	if (ds->dfmt == NULL)
		ds->dfmt = ds->ctep->default_format;
	return (B_TRUE);
} /* dbri_set_default_format */


/*
 * dbri_reset_default_format - XXX
 */
void
dbri_reset_default_format(dbri_stream_t	*ds)
{
	if (ds->ctep == NULL)
		return;
	ds->dfmt = NULL;
} /* dbri_reset_default_format */


/*
 * dbri_basepipe - XXX
 */
dbri_pipe_t *
dbri_basepipe(dbri_unit_t *unitp, dbri_chan_t dchan)
{
	dbri_chantab_t *ctep;

	ctep = dbri_dchan_to_ctep(dchan);
	if (ctep != NULL)
		return (&unitp->ptab[ctep->bpipe]);
	else
		return (NULL);
} /* dbri_basepipe */


/*
 */
dbri_chan_t
dbri_base_p_to_ch(uint_t bpipe)
{
	switch (bpipe) {
	case DBRI_PIPE_TE_D_IN:
		return (DBRI_TE_D_IN);
	case DBRI_PIPE_TE_D_OUT:
		return (DBRI_TE_D_OUT);
	case DBRI_PIPE_NT_D_OUT:
		return (DBRI_NT_D_OUT);
	case DBRI_PIPE_NT_D_IN:
		return (DBRI_NT_D_IN);
	case DBRI_PIPE_CHI:	/* XXX */
	default:
		return (DBRI_CHAN_NONE);
	}
} /* dbri_base_p_to_ch */



/* XXX - function to return default format of ctep */
