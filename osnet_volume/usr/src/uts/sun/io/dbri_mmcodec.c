/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri_mmcodec.c	1.50	99/05/21 SMI"

/*
 * Sun DBRI MMCODEC/SpeakerBox driver
 */

/*
 * REMIND: exodus 1992/09/26 - keep CHI format in basepipe
 * The current format of CHI should be kept in the basepipe.  That would
 * keep it in one place for all channels to use.  Have a CHI format
 * "refcnt" that would be incremented for each *different* user of CHI.
 * Thus if the refcnt was 0 or 1 the format could be changed.  If it were
 * 2 then there are two different entities using CHI and the format could
 * not be changed.
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>	/* for MIN */
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/isdnio.h>
#include <sys/dbriio.h>
#include <sys/dbrireg.h>
#include <sys/dbrivar.h>
#include <sys/mmcodecreg.h>
#include <sys/audiodebug.h>


/*
 * Global Variables
 */
#if defined(AUDIOTRACE)
int Dbri_mmcodec_debug = 0;
#endif
int Dbri_mmcodec_use_hpf = 1;		/* risk management */
int Dbri_mmcodec_mb = 0;		/* for keyman */
int Dbri_mmcodec_pio = 0;		/* for keyman */
int Dbri_keymanwait_mute = 300;
int Dbri_keymanwait_unmute = 300;

/*
 * Private functions
 */
static void mmcodec_data_mode_ok(void *);
static uint_t reverse_bit_order(uint_t, int);
static void mmcodec_init_pipes(dbri_unit_t *);
static void mmcodec_do_dts(dbri_unit_t *, uint_t, uint_t, uint_t, int,
	int, int);
static void mmcodec_dts_datamode_pipes(dbri_unit_t *);
static void mmcodec_dts_cntlmode_pipes(dbri_unit_t *);

/*
 * Some useful defines - see the self documenting code
 */

#define	SB_PAL_LEN	8

#define	ULAW_ZERO	((uint_t)0xffffffff)
#define	ALAW_ZERO	((uint_t)0xd5d5d5d5)
#define	LINEAR_ZERO	((uint_t)0x00000000)

#define	DUMMYDATA_LEN	(32)
#define	DUMMYDATA_CYCLE	(unitp->ser_sts.chi.pal_present ? \
	unitp->ser_sts.chi.mmcodec_ts_start : CHI_DATA_MODE_LEN)

#define	DM_T_5_8_LEN	(32)
#define	DM_T_5_8_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 32)
#define	DM_R_5_6_LEN	(10)
#define	DM_R_5_6_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 32)
#define	DM_R_7_8_LEN	(16)
#define	DM_R_7_8_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 48)

/* NB: XMIT cycles can not start at zero */
#define	CM_T_1_4_LEN	(32)
#define	CM_T_1_4_CYCLE	(unitp->ser_sts.chi.pal_present ? \
	unitp->ser_sts.chi.mmcodec_ts_start : CHI_CTRL_MODE_LEN)
#define	CM_R_1_2_LEN	(16)
#define	CM_R_1_2_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 0)
#define	CM_R_3_4_LEN	(16)
#define	CM_R_3_4_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 16)
#define	CM_R_7_LEN	(8)
#define	CM_R_7_CYCLE	(unitp->ser_sts.chi.mmcodec_ts_start + 48)

/* Masks for reading Control Mode data back from the MMCODEC */
#define	CM_R_1_2_MASK	0x063f
#define	CM_R_3_4_MASK	0x3f03
#define	CM_R_7_MANU(a)	((a & 0xf0) >> 4)
#define	CM_R_7_REV(a)	(a & 0x0f)

/* parameters to mmcodec_setup_chi */
#define	SETUPCHI_CTRL_MODE	1
#define	SETUPCHI_CODEC_MASTER	2
#define	SETUPCHI_DBRI_MASTER	3

/* parameters to mmcodec_set_spkrmute */
#define	SPKRMUTE_SET		1
#define	SPKRMUTE_GET		2
#define	SPKRMUTE_CLEAR		3
#define	SPKRMUTE_TOGGLE		4

#define	MMCODEC_TIMEOUT		(TICKS(1000)) /* 1 sec */
#define	MMCODEC_PDN_WAIT	(TICKS(50)) /* 50 ms */
#define	SB_BUTTON_REPEAT	(TICKS(167)) /* 6 Hz repeat */
#define	SB_BUTTON_START		(TICKS(1000)) /* 1 Hz repeat */
static int sb_button_repeat = 0;

#define	MMCODEC_NOFS_THRESHOLD	(20)

#define	VMINUS_BUTTON	0x01
#define	VPLUS_BUTTON	0x02
#define	MUTE_BUTTON	0x04
#define	HPHONE_IN	0x08
#define	MIKE_IN		0x10
#define	ID_MASK		0xe0
#define	PAL_ID		0xa0


#define	XPIPENO(z) (((z->refcnt & 0xffff) << 16)|PIPENO(z))


/*
 * mmcodec_config_start
 *
 * XXX - TBD
 */
/*ARGSUSED*/
void
mmcodec_config_start(dbri_unit_t *unitp)
{
}


/*
 * mmcodec_config_finished - XXX
 */
void
mmcodec_config_finished(dbri_unit_t *unitp, boolean_t okay)
{
	dbri_stream_t *ds = NULL;
	dbri_pipe_t *pipep;
	dbri_chi_status_t *cs;

	cs = DBRI_CHI_STATUS_P(unitp);
	if (okay) {
		/*
		 * Restart any pipes that were stopped while we reconfig'd the
		 * codec.
		 */
		ds = DChanToDs(unitp, DBRI_AUDIO_IN);
		pipep = ds->pipep;
		if ((ds->pipep != NULL) && ISPIPEINUSE(pipep)) {
			ATRACE(mmcodec_config_finished, 'pius',
			    PIPENO(pipep));

			/*
			 * Adjust length and cycle for possible
			 * new format.
			 */
			pipep->dts.input_tsd.r.len =
			    get_aud_pipe_length(unitp, pipep, DBRI_AUDIO_IN);
			pipep->dts.input_tsd.r.cycle =
			    get_aud_pipe_cycle(unitp, pipep, DBRI_AUDIO_IN);

			{
			dbri_chip_cmd_t cmd;

			cmd.opcode = pipep->dts.opcode.word32;
			cmd.arg[0] = pipep->dts.input_tsd.word32;
			cmd.arg[1] = pipep->dts.output_tsd.word32;
			dbri_command(unitp, cmd);
			/*
			 * XXX - We are not adjusting the reference count
			 * here because this pipe was "ripped" out of
			 * DBRI's list and the driver proper thinks DBRI
			 * still knows about it.
			 */
			}

			/* XXX - call dbri_start? call before DTS? */
		}
		ATRACE(mmcodec_config_finished, 'pi2 ', pipep);

		ds = DChanToDs(unitp, DBRI_AUDIO_OUT);
		pipep = ds->pipep;
		if ((ds->pipep != NULL) && ISPIPEINUSE(pipep)) {
			ATRACE(mmcodec_config_finished, 'piuo',
			    PIPENO(pipep));

			/*
			 * Adjust length and cycle for possible
			 * new format.
			 */
			pipep->dts.output_tsd.r.len =
			    get_aud_pipe_length(unitp, pipep, DBRI_AUDIO_OUT);
			pipep->dts.output_tsd.r.cycle =
			    get_aud_pipe_cycle(unitp, pipep, DBRI_AUDIO_OUT);

			{
			dbri_chip_cmd_t cmd;

			cmd.opcode = pipep->dts.opcode.word32;
			cmd.arg[0] = pipep->dts.input_tsd.word32;
			cmd.arg[1] = pipep->dts.output_tsd.word32;
			dbri_command(unitp, cmd);
			/*
			 * XXX - We are not adjusting the reference count
			 * here because this pipe was "ripped" out of
			 * DBRI's list and the driver proper thinks DBRI
			 * still knows about it.
			 */
			}

			/* XXX - call dbri_start? call before DTS? */
		}
	} /* if configuration was successful */

	/*
	 * Complete any outstanding ioctls.
	 */
	{
	int i;
	dbri_stream_t *ids;

	for (i = 0; i < Dbri_nstreams; i++) {
		if (i != DBRI_AUDIO_IN &&
		    i != DBRI_AUDIO_OUT &&
		    i != DBRI_AUDIOCTL)
			continue;

		ids = DChanToDs(unitp, i);
		if (DsDisabled(ids))
			continue;

		if (DsToAs(ids)->dioctl.action == AUDIOCACTION_WAIT ||
		    DsToAs(ids)->dioctl.mp != NULL) {
			ATRACE(mmcodec_config_finished, 'iack', ids);
			DsToAs(ids)->dioctl.reason = okay ? 0 : EIO;
			audio_ack(DsToAs(ids)->writeq,
			    DsToAs(ids)->dioctl.mp,
			    DsToAs(ids)->dioctl.reason);
			if (!okay)
				dbri_error_msg(ids, M_ERROR);
		}
#if 0
		audio_sendsig(DsToAs(ids), AUDIO_SENDSIG_EXPLICIT);
#endif
	} /* every dbri channel */
	}

	/*
	 * Wakeup anyone waiting for this configuration to finish
	 */
	cs->chi_wait_for_init = B_FALSE;
	cv_signal(&unitp->audiocon_cv);	/* open() waits for config */
	ATRACE(mmcodec_config_finished, 'cvsg', unitp);
}


/*
 * mmcodec_configure - apply the passed format to the MMCODEC
 *
 * TBD
 */
/*ARGSUSED*/
boolean_t
mmcodec_configure(dbri_unit_t *unitp, dbri_chanformat_t *dfmt)
{
	return (B_TRUE);
}


/*
 * mmcodec_reset - XXX
 */
void
mmcodec_reset(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;
	dbri_stream_t *ds;
	uchar_t	tmp;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	/*
	 * After DBRI has been reset, and before we have done any
	 * write to the pio pins, we can read them to find out the
	 * status of the CODEC's currently "on the system". If there
	 * is a Speakerbox plugged in, then the external power down
	 * pin will be pulled high. If there is an onboard MMCODEC,
	 * then the internal power down pin will be pulled high. If
	 * there is both, the Speakerbox will take precedence.
	 */
	tmp = unitp->regsp->n.pio;
#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug)
		printf("!mmcodec_reset: reg2 initially 0x%x\n", (uint_t)tmp);
#endif

	if (tmp & SCHI_SET_PDN) {
		cs->chi_wait_for_init = B_TRUE;

		/*
		 * SpeakerBox found
		 *
		 * Since there is a SpeakerBox, we ignore any onboard CODEC.
		 */
		cs->pal_present = B_TRUE;
		cs->mmcodec_ts_start = SB_PAL_LEN;

		/*
		 * At this point the state is going to be called
		 * POWER_DOWN but we're actually going to CLR PDN so it's
		 * not powered down.  But noone is driving a clock and
		 * it's in control mode so no sampling is happening.
		 *
		 * The Crystal CODEC spec requires that the CODEC be
		 * powered up for 50ms before we go through a
		 * calibration.  So we power it up here and arrange to
		 * start it up in 50ms.
		 */
		cs->chi_state = CHI_POWERED_DOWN;
		unitp->regsp->n.pio = SCHI_ENA_ALL | SCHI_SET_CTRL_MODE |
		    SCHI_SET_INT_PDN | SCHI_CLR_RESET | SCHI_CLR_PDN;

	} else if (tmp & SCHI_SET_INT_PDN) {
		cs->chi_wait_for_init = B_TRUE;

		/*
		 * On-Board MMCODEC found
		 */
		cs->pal_present = B_FALSE;
		cs->mmcodec_ts_start = 0;

		/*
		 * Again, not really powered down...
		 */
		cs->chi_state = CHI_POWERED_DOWN;
		unitp->regsp->n.pio = SCHI_ENA_ALL | SCHI_SET_CTRL_MODE |
		    SCHI_CLR_INT_PDN | SCHI_CLR_RESET | SCHI_SET_PDN;

	} else {
		/*
		 * You cannot unplug an onboard CODEC so we know that if
		 * there's no device, it'll have to be a SpeakerBox.
		 */
		cs->pal_present = B_TRUE;
		cs->mmcodec_ts_start = SB_PAL_LEN;
		cs->chi_state = CHI_NO_DEVICES;
		unitp->regsp->n.pio = 0;
		cmn_err(CE_CONT, "?audio: no mmcodec device found\n");
	}

#define	PROP_VALUE(prop, default) ((ddi_getprop(DDI_DEV_T_ANY, unitp->dip, \
	DDI_PROP_DONTPASS, (prop), (default)) != 0) ? B_TRUE : B_FALSE)

	if (cs->pal_present) {	/* SpeakerBox */
		/*
		 * Do not want to override these from the PROM since the
		 * SpeakerBox is external hardware.
		 */
		cs->have_speaker = B_TRUE;
		cs->have_linein = B_TRUE;
		cs->have_lineout = B_TRUE;
		cs->have_microphone = B_TRUE;
		cs->have_headphone = B_TRUE;
	} else {		/* Motherboard audio */
		/*
		 * NB: The following defaults are set up to accomodate
		 * Sunergy's configuration.  It provides no properties
		 * so in their absence the defaults must be properly
		 * set.
		 */
		cs->have_speaker = PROP_VALUE("speaker", 1);
		cs->have_linein = PROP_VALUE("input-line", 0);
		cs->have_lineout = PROP_VALUE("output-line", 0);
		cs->have_microphone = PROP_VALUE("input-microphone", 1);
		cs->have_headphone = PROP_VALUE("output-headphone", 1);

		if (cs->have_linein) {
			ds = DChanToDs(unitp, DBRI_AUDIO_IN);
			ds->as.info.avail_ports |= AUDIO_LINE_IN;
			ds->as.info.mod_ports = ds->as.info.avail_ports;
		} else {
			ds = DChanToDs(unitp, DBRI_AUDIO_IN);
			ds->as.info.avail_ports &= ~AUDIO_LINE_IN;
			ds->as.info.mod_ports = ds->as.info.avail_ports;
		}
		if (cs->have_lineout) {
			ds = DChanToDs(unitp, DBRI_AUDIO_OUT);
			ds->as.info.avail_ports |= AUDIO_LINE_OUT;
			ds->as.info.mod_ports = ds->as.info.avail_ports;
		} else {
			ds = DChanToDs(unitp, DBRI_AUDIO_OUT);
			ds->as.info.avail_ports &= ~AUDIO_LINE_OUT;
			ds->as.info.mod_ports = ds->as.info.avail_ports;
		}
	}
#undef	PROP_VALUE

#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug) {
		printf("mmcodec: ");
		if (cs->have_speaker)
			printf("SPEAKER ");
		if (cs->have_linein)
			printf("LINEIN ");
		if (cs->have_lineout)
			printf("LINEOUT ");
		if (cs->have_microphone)
			printf("MIC ");
		if (cs->have_headphone)
			printf("HEADPHONE ");
		printf("\n");
	}
#endif /* AUDIOTRACE */

	/*
	 * Ensure that the driver's default settings are possible.
	 * If this changes in the future then various small pieces
	 * of code in this file will have to change to support defaults
	 * based on what's available.
	 */
	ASSERT(cs->have_microphone && cs->have_speaker && cs->have_headphone);

	cs->chi_format_refcnt = 0;
	cs->chi_dbri_clock = DBRI_CLOCK_EXT;

	ATRACE(mmcodec_reset, 'timo', MMCODEC_PDN_WAIT);
	unitp->mmcstartup_id = timeout(mmcodec_startup_audio,
	    unitp, MMCODEC_PDN_WAIT);
} /* mmcodec_reset */


/*
 * reverse_bit_order - XXX
 */
static uint_t
reverse_bit_order(uint_t word, int len)
{
	int	bit;
	int	count;
	uint_t	new_word = 0;

	len--;
	for (count = 0; count <= len; count++) {
		bit = ((word >> count) & 0x1);
		new_word += (bit << (len - count));
	}

	return (new_word);
} /* reverse_bit_order */


/*
 * mmcodec_init_pipes - initialize extraneous and sundry pipes only once
 * after reset
 */
static void
mmcodec_init_pipes(dbri_unit_t *unitp)
{
	int tmp;
	dbri_pipe_t *pipep;
	dbri_chip_cmd_t cmd;
	mmcodec_data_t mmdata;
	mmcodec_ctrl_t mmctrl;
	dbri_stream_t *ds;
	dbri_chi_status_t *cs;

	ASSERT_UNITLOCKED(unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

	unitp->regsp->n.sts |= DBRI_STS_C;	/* Enable CHI interface */

	/*
	 * Setup CHI base pipe
	 */
	pipep = &unitp->ptab[DBRI_PIPE_CHI];
	pipep->dts.opcode.word32 = 0;
	pipep->dts.input_tsd.word32 = 0;
	pipep->dts.output_tsd.word32 = 0;

	/*
	 * Setup dts command words
	 */
/*
 * REMIND: exodus 1992/09/26 - need to use the normal pipe mgt routines
 * Someday this code should be using all of the standard pipe allocation
 * and manipulation routines.  For example, using a connection request
 * that has a timeslot on one end and a DBRI_CHAN_FIXED on the other
 * with a callback for when the value changes...
 */
	pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;	/* DTS command */
	pipep->dts.opcode.r.vi = DBRI_DTS_VI;		/* input tsd */
	pipep->dts.opcode.r.vo = DBRI_DTS_VO;		/* output tsd */
	pipep->dts.opcode.r.id = DBRI_DTS_ID;		/* Add timeslot */
	pipep->dts.opcode.r.oldin = DBRI_PIPE_CHI;	/* old input pipe */
	pipep->dts.opcode.r.oldout = DBRI_PIPE_CHI;	/* old output pipe */
	pipep->dts.opcode.r.pipe = DBRI_PIPE_CHI;	/* pipe 16 */

	pipep->dts.input_tsd.chi.mode = DBRI_DTS_ALTERNATE; /* alternate */
	pipep->dts.input_tsd.chi.next = DBRI_PIPE_CHI; /* next is 16 */

	pipep->dts.output_tsd.chi.mode = DBRI_DTS_ALTERNATE; /* alternamte */
	pipep->dts.output_tsd.chi.next = DBRI_PIPE_CHI;	/* next pipe 16 */

	cmd.opcode = pipep->dts.opcode.word32;		/* DTS command */
	cmd.arg[0] = pipep->dts.input_tsd.word32;
	cmd.arg[1] = pipep->dts.output_tsd.word32;
	dbri_command(unitp, cmd);
	pipep->refcnt++;
	ATRACE(mmcodec_init_pipes, 'ref+', XPIPENO(pipep));

	/*
	 * Issue SDP's for pipes 17, 18, and 19 - Data Mode pipes for,
	 * transmit timeslots 5-8 (32 bits), receive timeslots 5-6 (16
	 * bits), and receive timeslots 7-8 (16 bits) respectively
	 */
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_D | DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_DM_T_5_8;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	pipep = &unitp->ptab[DBRI_PIPE_DM_R_5_6];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_DM_R_5_6;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	pipep = &unitp->ptab[DBRI_PIPE_DM_R_7_8];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_DM_R_7_8;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	/*
	 * Now issue SDP's for pipes 20, 21, and 22 - Control Mode pipes for,
	 * Transmit Timeslots 1-4 (32 bits), Receive timeslots 1-2 (16 bits),
	 * and Receive timeslots 3-4 (16 bits) respectively
	 */
	pipep = &unitp->ptab[DBRI_PIPE_CM_T_1_4];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_D | DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_CM_T_1_4;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	pipep = &unitp->ptab[DBRI_PIPE_CM_R_1_2];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_CM_R_1_2;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	pipep = &unitp->ptab[DBRI_PIPE_CM_R_3_4];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_CM_R_3_4;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	pipep = &unitp->ptab[DBRI_PIPE_CM_R_7];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_CM_R_7;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	/*
	 * Set up pipe to transmit dummy data, when we're not transmitting
	 * real data.  Since DBRI drives the CHIDX line low (zero) when
	 * there is no pipe defined for a time slot, it looks like
	 * zeroes to the CODEC. In ulaw, zero is actually all ones so
	 * the zeroes sent by DBRI causes popping and clicking
	 */
	pipep = &unitp->ptab[DBRI_PIPE_DUMMYDATA];
	pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
	    DBRI_SDP_D | DBRI_SDP_PTR | DBRI_SDP_CLR | DBRI_PIPE_DUMMYDATA;
	cmd.opcode = pipep->sdp;
	cmd.arg[0] = 0;
	dbri_command(unitp, cmd);

	/*
	 * Set up data mode control information
	 */
	mmdata.word32 = 0;

	/*
	 * Whatever we initialize the MMCODEC to here must match with
	 * what dbri_attach set's up. Default is DBRI_DEFAULT_GAIN, with
	 * speaker and microphones as ports
	 */
	mmdata.r.om0 = ~MMCODEC_OM0_ENABLE & 1;
	mmdata.r.om1 = ~MMCODEC_OM1_ENABLE & 1;
	mmdata.r.sm = MMCODEC_SM; /* enable speaker */
	mmdata.r.ovr = MMCODEC_OVR_CLR;	/* clear overflow conditions */
	mmdata.r.is = MMCODEC_IS_MIC;
	mmdata.r.pio = (uint8_t)Dbri_mmcodec_pio;

	{
		dbri_pipe_t *palpipep;

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp = 0xc;
	}

	/*
	 * Calculate default output attenuation
	 */
	tmp = MMCODEC_MAX_ATEN -
	    (DBRI_DEFAULT_GAIN *
	    (MMCODEC_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	mmdata.r.lo = tmp & 0x3f;
	mmdata.r.ro = tmp & 0x3f;

	/*
	 * Calculate default input gain
	 */
	tmp = DBRI_DEFAULT_GAIN *
	    (MMCODEC_MAX_GAIN + 1) / (AUDIO_MAX_GAIN + 1);
	mmdata.r.lg = tmp & 0x0f;
	mmdata.r.rg = tmp & 0x0f;

	/*
	 * Calculate default monitor gain
	 */
	mmdata.r.ma = MMCODEC_MA_MAX_ATEN; /* monitor attenuation */

	/*
	 * We create a standard for handing data for fixed pipes; the
	 * data found in the pipe table will be "normal" so we can look
	 * at it and manipulate it without hurting our brains, however
	 * the data sent to the dbri_command routine will be reversed so
	 * it doesn't have to know about it.
	 */
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];
	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	pipep->ssp = mmdata.word32;
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd);

	/*
	 * Set up control mode initial values
	 */

	mmctrl.word32[0] = 0;

	mmctrl.r.dcb = 0;
	mmctrl.r.sre = ~MMCODEC_SRE & 1;
	mmctrl.r.vs0 = MMCODEC_VS0;
	mmctrl.r.vs1 = MMCODEC_VS1;
	mmctrl.r.dfr = MMCODEC_DFR_8000;	/* 8 KHz */
	mmctrl.r.st = MMCODEC_ST_MONO;
	mmctrl.r.df = MMCODEC_DF_ULAW;		/* uLAW encoding */
	if (Dbri_mmcodec_use_hpf)
		mmctrl.r.hpf = 1;		/* Turn on high-pass filter */
	mmctrl.r.mb = (uint8_t)Dbri_mmcodec_mb;
	mmctrl.r.pio = (uint8_t)Dbri_mmcodec_pio;
	mmctrl.r.mck = MMCODEC_MCK_XTAL1;	/* Clock is XTAL1 */
#if CHI_DATA_MODE_LEN == 64
	mmctrl.r.bsel = MMCODEC_BSEL_64;
#elif CHI_DATA_MODE_LEN == 128
	mmctrl.r.bsel = MMCODEC_BSEL_128;
#elif CHI_DATA_MODE_LEN == 256
	mmctrl.r.bsel = MMCODEC_BSEL_256;
#else
#error	illegal CHI_DATA_MODE_LEN value;
#endif
	mmctrl.r.xclk = MMCODEC_XCLK;		/* CODEC is CHI master */
	mmctrl.r.xen = MMCODEC_XEN;		/* enable serial tx */
	mmctrl.r.enl = ~MMCODEC_ENL & 1;	/* disable loopback */

	/* We currently don't use Time Slots 5, 6, or 8, in control mode */

	pipep = &unitp->ptab[DBRI_PIPE_CM_T_1_4];
	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	pipep->ssp = mmctrl.word32[0];
	cmd.arg[0] = reverse_bit_order(pipep->ssp, CM_T_1_4_LEN);
	dbri_command(unitp, cmd);

	if (cs->pal_present) {
		/*
		 * Issue SDP's for ID Pal pipes here
		 */
		pipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
		    DBRI_SDP_D | DBRI_SDP_PTR | DBRI_SDP_CLR |
		    PIPENO(pipep);
		pipep->ssp = 0xc; /* LED off */

		/*
		 * Set-up the pipe
		 */
		cmd.opcode = pipep->sdp;
		cmd.arg[0] = 0;
		dbri_command(unitp, cmd);

		/*
		 * Initialize the short-pipe data
		 */
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(pipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);

		pipep = &unitp->ptab[DBRI_PIPE_SB_PAL_R];
		pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_CHNG | DBRI_SDP_FIXED |
		    DBRI_SDP_PTR | DBRI_SDP_CLR | PIPENO(pipep);

		cmd.opcode = pipep->sdp;
		cmd.arg[0] = 0;
		dbri_command(unitp, cmd);

		/*
		 * Set an initial value to compare against when we get
		 * the first interrupt on this pipe.  This should match
		 * what we set avail_ports to initially.  On a SB, once
		 * we get the interrupt and see that they're *not* there,
		 * this will be set correctly, otherwise this is already
		 * set up right for a Sunergy.
		 */
		pipep->ssp = PAL_ID | HPHONE_IN | MIKE_IN;

		/*
		 * Set avail_ports and the config field appropriately for
		 * a SpeakerBox.
		 */
		ds = DChanToDs(unitp, DBRI_AUDIO_IN);
		ds->as.info.avail_ports |= AUDIO_LINE_IN;
		ds->as.info.mod_ports = ds->as.info.avail_ports;
		(void) strcpy(ds->config, DBRI_DEV_SPKRBOX);


		ds = DChanToDs(unitp, DBRI_AUDIO_OUT);
		ds->as.info.avail_ports |= AUDIO_LINE_OUT;
		ds->as.info.mod_ports = ds->as.info.avail_ports;
		(void) strcpy(ds->config, DBRI_DEV_SPKRBOX);

		ds = DChanToDs(unitp, DBRI_AUDIOCTL);
		(void) strcpy(ds->config, DBRI_DEV_SPKRBOX);
	}

	cmd.opcode = DBRI_CMD_PAUSE; /* PAUSE command */
	dbri_command(unitp, cmd);

	if (sb_button_repeat == 0)
		sb_button_repeat = SB_BUTTON_REPEAT;
} /* mmcodec_init_pipes */


/*
 * The dbri_setup_dts command won't work for regular overhead kind of
 * pipes since it wants to get all the information from the channel
 * table, and we don't want to add these to it. This is a much simpler
 * version of the routine.
 */
static void
mmcodec_do_dts(dbri_unit_t *unitp, uint_t pipe, uint_t oldpipe,
	uint_t nextpipe, int dir, int cycle, int length)
{
	dbri_pipe_t *pipep;
	dbri_pipe_t *op;
	dbri_pipe_t *np;

	ATRACE(mmcodec_do_dts, 'unit', unitp);
	ATRACE(mmcodec_do_dts, 'pipe', pipe);
	ASSERT_UNITLOCKED(unitp);

	pipep = &unitp->ptab[pipe];
	np = &unitp->ptab[nextpipe];
	op = &unitp->ptab[oldpipe];

	pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;
	pipep->dts.opcode.r.id = DBRI_DTS_ID; /* Add timeslot */
	pipep->dts.opcode.r.pipe = PIPENO(pipep);
	pipep->dts.input_tsd.word32 = 0;
	pipep->dts.output_tsd.word32 = 0;

	if (dir == DBRI_DIRECTION_IN) {
		op->dts.input_tsd.r.next = PIPENO(pipep);
		np->dts.opcode.r.oldin = PIPENO(pipep);

		pipep->dts.opcode.r.vi = DBRI_DTS_VI;
		pipep->dts.opcode.r.oldin = oldpipe;
		pipep->dts.input_tsd.r.len = length;
		pipep->dts.input_tsd.r.cycle = cycle;
		pipep->dts.input_tsd.r.mode = DBRI_DTS_SINGLE;
		pipep->dts.input_tsd.r.next = nextpipe;
	} else {
		op->dts.output_tsd.r.next = PIPENO(pipep);
		np->dts.opcode.r.oldout = PIPENO(pipep);

		pipep->dts.opcode.r.vo = DBRI_DTS_VO;
		pipep->dts.opcode.r.oldout = oldpipe;
		pipep->dts.output_tsd.r.len = length;
		pipep->dts.output_tsd.r.cycle = cycle;
		pipep->dts.output_tsd.r.mode = DBRI_DTS_SINGLE;
		pipep->dts.output_tsd.r.next = nextpipe;
	}

	{
		dbri_chip_cmd_t	 cmd;

		cmd.opcode = pipep->dts.opcode.word32;
		cmd.arg[0] = pipep->dts.input_tsd.word32;
		cmd.arg[1] = pipep->dts.output_tsd.word32;
		dbri_command(unitp, cmd);
	}
	pipep->refcnt++;
	ATRACE(mmcodec_do_dts, 'ref+', XPIPENO(pipep));
} /* mmcodec_do_dts */


/*
 * mmcodec_dts_datamoed_pipes - XXX
 */
static void
mmcodec_dts_datamode_pipes(dbri_unit_t *unitp)
{
	dbri_pipe_t *pipep;
	dbri_chi_status_t *cs;

	ASSERT_UNITLOCKED(unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

	ATRACE(mmcodec_dts_datamode_pipes, 'unit', unitp);

	/*
	 * This set's up the pipes so that the linked list is the
	 * same all the time. Transmit side looks like 16-[23]-17-16, and
	 * the receive side looks like 16-[24]-18-19-16
	 * We can't use the dbri_setup_dts routine since these don't/can't
	 * show up in the Channel table, but code is stolen from there
	 * It still needs to reflect the list in the pipe table though
	 * so that the above routine (and others) continue to work
	 *
	 * The SBPal pipes are DTS's both here and in mmcodec_dts_cntlmode.
	 * In theory, they could be done once in the beginning, like
	 * in init_pipes and then never touched again, but that would mean
	 * that the datamode and control mode frame lengths were the same.
	 * (The only difference between this code and the cntlmode code
	 * is the Transmit pipe cycle value)
	 */
	if (cs->pal_present) {
		mmcodec_ctrl_t mmctrl;
		uint_t fs;

		mmctrl.word32[0] = unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp;

		switch (mmctrl.r.bsel) {
		case MMCODEC_BSEL_256:
			fs = 256;
			break;
		case MMCODEC_BSEL_128:
			fs = 128;
			break;
		case MMCODEC_BSEL_64:
			fs = 64;
			break;
		}

		ATRACE(mmcodec_dts_datamode_pipes, 'palp', 1);
		mmcodec_do_dts(unitp, DBRI_PIPE_SB_PAL_T,
		    DBRI_PIPE_CHI, DBRI_PIPE_CHI, DBRI_DIRECTION_OUT,
		    fs, SB_PAL_LEN);

		mmcodec_do_dts(unitp, DBRI_PIPE_SB_PAL_R,
		    DBRI_PIPE_CHI, DBRI_PIPE_CHI, DBRI_DIRECTION_IN, 0,
		    SB_PAL_LEN);
	} else {
		/*
		 * Setup CHI base pipe
		 */
		pipep = &unitp->ptab[DBRI_PIPE_CHI];

		pipep->dts.opcode.word32 = 0;
		pipep->dts.input_tsd.word32 = 0;
		pipep->dts.output_tsd.word32 = 0;

		/*
		 * Setup dts command words
		 */
		pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;
		pipep->dts.opcode.r.vi = DBRI_DTS_VI; /* input tsd */
		pipep->dts.opcode.r.vo = DBRI_DTS_VO; /* output tsd */
		pipep->dts.opcode.r.id = DBRI_DTS_ID; /* Add timeslot */
		pipep->dts.opcode.r.oldin = DBRI_PIPE_CHI;
		pipep->dts.opcode.r.oldout = DBRI_PIPE_CHI;
		pipep->dts.opcode.r.pipe = DBRI_PIPE_CHI;

		pipep->dts.input_tsd.chi.mode = DBRI_DTS_ALTERNATE;
		pipep->dts.input_tsd.chi.next = DBRI_PIPE_CHI;

		pipep->dts.output_tsd.chi.mode = DBRI_DTS_ALTERNATE;
		pipep->dts.output_tsd.chi.next = DBRI_PIPE_CHI;

		{
			dbri_chip_cmd_t	cmd;

			cmd.opcode = pipep->dts.opcode.word32;
			cmd.arg[0] = pipep->dts.input_tsd.word32;
			cmd.arg[1] = pipep->dts.output_tsd.word32;
			dbri_command(unitp, cmd);
		}
		pipep->refcnt++;
		ATRACE(mmcodec_dts_datamode_pipes, 'ref+', XPIPENO(pipep));
		ATRACE(mmcodec_dts_datamode_pipes, 'palp', 0);
	}
	ATRACE(mmcodec_dts_datamode_pipes, '    ', 0);

	mmcodec_do_dts(unitp, DBRI_PIPE_DM_T_5_8,
	    (cs->pal_present ? DBRI_PIPE_SB_PAL_T : DBRI_PIPE_CHI),
	    DBRI_PIPE_CHI, DBRI_DIRECTION_OUT, DM_T_5_8_CYCLE, DM_T_5_8_LEN);
	ATRACE(mmcodec_dts_datamode_pipes, 't58 ', 0);

#if 0
	/*
	 * XXX5 - not using these pipes reduces the load on DBRI and
	 * hopefully this will lessen the number of receiver overflow
	 * errors and other problems (hanging CHI, etc).
	 */

	mmcodec_do_dts(unitp, DBRI_PIPE_DM_R_5_6,
	    (cs->pal_present ? DBRI_PIPE_SB_PAL_R : DBRI_PIPE_CHI),
	    DBRI_PIPE_CHI, DBRI_DIRECTION_IN, DM_R_5_6_CYCLE, DM_R_5_6_LEN);
	ATRACE(mmcodec_dts_datamode_pipes, 'r56 ', 0);

	mmcodec_do_dts(unitp, DBRI_PIPE_DM_R_7_8,
	    DBRI_PIPE_DM_R_5_6, DBRI_PIPE_CHI,
	    DBRI_DIRECTION_IN, DM_R_7_8_CYCLE, DM_R_7_8_LEN);
#endif
} /* mmcodec_dts_datamode_pipes */


/*
 * This set's up the pipes so that the linked list is the same all the
 * time. Transmit side looks like 16-[23]-20-16, and the receive side
 * looks like 16-[24]-21-22-16 We can't use the dbri_setup_dts routine
 * since these don't/can't show up in the Channel table, but code is
 * stolen from there It still needs to reflect the list in the pipe table
 * though so that the above routine (and others) continue to work
 */
static void
mmcodec_dts_cntlmode_pipes(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;

	ASSERT_UNITLOCKED(unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

	if (cs->pal_present) {
		mmcodec_do_dts(unitp, DBRI_PIPE_SB_PAL_T,
		    DBRI_PIPE_CHI, DBRI_PIPE_CHI, DBRI_DIRECTION_OUT,
		    CHI_CTRL_MODE_LEN, SB_PAL_LEN);

		mmcodec_do_dts(unitp, DBRI_PIPE_SB_PAL_R,
		    DBRI_PIPE_CHI, DBRI_PIPE_CHI, DBRI_DIRECTION_IN, 0,
		    SB_PAL_LEN);
	}

	mmcodec_do_dts(unitp, DBRI_PIPE_CM_T_1_4,
	    (cs->pal_present ? DBRI_PIPE_SB_PAL_T : DBRI_PIPE_CHI),
	    DBRI_PIPE_CHI, DBRI_DIRECTION_OUT, CM_T_1_4_CYCLE, CM_T_1_4_LEN);

	mmcodec_do_dts(unitp, DBRI_PIPE_CM_R_1_2,
	    (cs->pal_present ? DBRI_PIPE_SB_PAL_R : DBRI_PIPE_CHI),
	    DBRI_PIPE_CHI, DBRI_DIRECTION_IN, CM_R_1_2_CYCLE, CM_R_1_2_LEN);

	mmcodec_do_dts(unitp, DBRI_PIPE_CM_R_3_4, DBRI_PIPE_CM_R_1_2,
	    DBRI_PIPE_CHI, DBRI_DIRECTION_IN, CM_R_3_4_CYCLE, CM_R_3_4_LEN);

	mmcodec_do_dts(unitp, DBRI_PIPE_CM_R_7, DBRI_PIPE_CM_R_3_4,
	    DBRI_PIPE_CHI, DBRI_DIRECTION_IN, CM_R_7_CYCLE, CM_R_7_LEN);
} /* mmcodec_dts_cntlmode_pipes */


/*
 * mmcodec_setup_chi - Setup CHI bus appropriately; this is hardcoded for
 * a single MMCODEC on the CHI bus, with or without a Speakerbox PAL.
 */
static void
mmcodec_setup_chi(dbri_unit_t *unitp, int how)
{
	dbri_chip_cmd_t	cmd;
	dbri_chi_status_t *cs;
	uint_t chicm;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);
	ATRACE(mmcodec_setup_chi, ' how', how);

	/*
	 * The first thing to do when changing clock parameters
	 * (according to Harry French) is to disable CHI so that
	 * everything is reset.  Spec says "does not drive CHIDX or CHIDR
	 * when XEN is 0".
	 */
	cs->cdm_cmd = (DBRI_CMD_CDM | DBRI_CDM_XCE | DBRI_CDM_REN);

	cmd.opcode = cs->cdm_cmd;
	dbri_command(unitp, cmd);

	/*
	 * Set up parameters for CHI and CDM commands
	 */
	switch (how) {
	case SETUPCHI_CTRL_MODE:
	case SETUPCHI_DBRI_MASTER:
		chicm = (256*6)/CHI_CTRL_MODE_LEN;
		break;

	case SETUPCHI_CODEC_MASTER:
		chicm = 1;	/* CHICK is an input NOT 8kHz */
		break;

	default:
		cmn_err(CE_WARN, "dbri: invalid setup!");
		return;
	}

	/*
	 * Issue CHI command
	 */
	cs->chi_cmd = DBRI_CMD_CHI | DBRI_CHI_CHICM(chicm) |
	    DBRI_CHI_INT | DBRI_CHI_CHIL | DBRI_CHI_FD | DBRI_CHI_BPF(0);

	cmd.opcode = cs->chi_cmd;	/* CHI command */
	dbri_command(unitp, cmd);

	cmd.opcode = DBRI_CMD_PAUSE; 	/* PAUSE command */
	dbri_command(unitp, cmd);
} /* mmcodec_setup_chi */


/*
 * mmcodec_enable_chi - XXX
 */
static void
mmcodec_enable_chi(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	cs->cdm_cmd = DBRI_CMD_CDM | DBRI_CDM_XCE | DBRI_CDM_XEN |
	    DBRI_CDM_REN;

	{
		dbri_chip_cmd_t	 cmd;

		cmd.opcode = cs->cdm_cmd;	/* CDM command */
		dbri_command(unitp, cmd);
	}
} /* mmcodec_enable_chi */


/*
 * mmcodec_watchdog - XXX
 */
static void
mmcodec_watchdog(void *arg)
{
	dbri_unit_t *unitp = arg;
	dbri_chi_status_t *cs;

	LOCK_UNIT(unitp);
	if (unitp->suspended) {
		UNLOCK_UNIT(unitp);
		return;
	}
	cs = DBRI_CHI_STATUS_P(unitp);

	cs->chi_state = CHI_NO_DEVICES;
	unitp->regsp->n.pio = 0;

	/*
	 * This wasn't here before but since the cgeight driver blocked
	 * calls to our interrupt service routine it was discovered that
	 * after we set off the handshake we could never finish it
	 * (dbri_intr never being called and all) so dbri would then
	 * interrupt continuously.  This prevent that from happening
	 * in the face of catastrophic failure in the framework...
	 */
	{
		dbri_chip_cmd_t cmd;

		cmd.opcode = cs->chi_cmd;
		cmd.opcode &= ~(DBRI_CHI_CHIL);
		dbri_command(unitp, cmd);
	}

	mmcodec_config_finished(unitp, B_FALSE);

	cmn_err(CE_WARN, "audio: Unable to communicate with speakerbox (%d)",
	    cs->chi_state);

	UNLOCK_UNIT(unitp);
} /* mmcodec_watchdog */


/*
 * mmcodec_setup_ctrl_mode - Start the process of putting the MMCODEC
 * into Control Mode; we have to wait for interrupts from DBRI before
 * things can finish, so the rest of this is driven by the state machine
 * at interrupt level.  Check out dbri_fxdt_intr, to see how it works.
 */
void
mmcodec_setup_ctrl_mode(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;

	ASSERT(unitp != NULL);
	ATRACE(mmcodec_setup_ctrl_mode, 'UNIT', unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug) {
		printf("!mmcodec_setup_ctrl_mode: state=%d\n",
		    cs->chi_state);
	}
#endif

	/*
	 * 1) Volume attenuation should be increased to Max; mute all
	 * outputs (if CODEC is already powered down or previously did
	 * not exist, or all outputs are already muted, we don't need to
	 * do this).
	 */
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];
	mmdata.word32 = pipep->ssp;

	if ((cs->chi_state == CHI_POWERED_DOWN) ||
	    (cs->chi_state == CHI_NO_DEVICES) ||
	    (!mmdata.r.om1 && !mmdata.r.om0 && !mmdata.r.sm)) {
		if (unitp->mmcwatchdog_id)
			(void) untimeout(unitp->mmcwatchdog_id);
		unitp->mmcwatchdog_id = timeout(mmcodec_watchdog,
		    unitp, MMCODEC_TIMEOUT);
		{

		dbri_pipe_t *palpipep;

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		cs->savedpal = palpipep->ssp;
		}
	} else if (cs->chi_state != CHI_GOING_TO_CTRL_MODE) {

		mmcodec_do_dts(unitp, DBRI_PIPE_DM_R_5_6,
		    (uint_t)(cs->pal_present ? DBRI_PIPE_SB_PAL_R :
		    DBRI_PIPE_CHI), DBRI_PIPE_CHI, DBRI_DIRECTION_IN,
		    DM_R_5_6_CYCLE, DM_R_5_6_LEN);

		{
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *palpipep;

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		cs->savedpal = palpipep->ssp;
		palpipep->ssp |= 0xe;
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(palpipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);

		drv_usecwait(Dbri_keymanwait_mute);
		}

		mmdata.r.lo = MMCODEC_MAX_DEV_ATEN;
		mmdata.r.ro = MMCODEC_MAX_DEV_ATEN;
		mmdata.r.sm = ~MMCODEC_SM & 1;
		mmdata.r.om0 = ~MMCODEC_OM0_ENABLE & 1;
		mmdata.r.om1 = ~MMCODEC_OM1_ENABLE & 1;

		/*
		 * Be sure not to save these new values in pipep->ssp so
		 * that we can restore them to their original values later
		 */
		cs->chi_state = CHI_GOING_TO_CTRL_MODE;

		{
			dbri_chip_cmd_t	 cmd;

			cmd.opcode = DBRI_CMD_SSP |
			    DBRI_SSP_PIPE(PIPENO(pipep));
			cmd.arg[0] = reverse_bit_order(mmdata.word32,
			    DM_T_5_8_LEN);
			dbri_command(unitp, cmd);
			ATRACE(mmcodec_setup_ctrl_mode, 'SSP ', cmd.arg[0]);
		}
		{
			dbri_chip_cmd_t	 cmd;

			cmd.opcode = DBRI_CMD_PAUSE;
			dbri_command(unitp, cmd);
		}

		if (unitp->mmcwatchdog_id)
			(void) untimeout(unitp->mmcwatchdog_id);
		unitp->mmcwatchdog_id = timeout(mmcodec_watchdog,
		    unitp, MMCODEC_TIMEOUT);
		return;
	}

	/*
	 * 2) Stop receiving audio data.  A SDP NULL has been previously
	 * issued.
	 */

	/*
	 * 3) Set D/C low to indicate control mode
	 */
#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug) {
		uint_t tmp;

		tmp = unitp->regsp->n.pio;
		printf("!mmcodec_setup_ctrl_mode: init reg2=0x%x\n", tmp);
	}
#endif

	if (cs->pal_present) {
		unitp->regsp->n.pio = SCHI_ENA_ALL | SCHI_SET_CTRL_MODE |
		    SCHI_SET_INT_PDN | SCHI_CLR_RESET | SCHI_CLR_PDN;
	} else {
		unitp->regsp->n.pio = SCHI_ENA_ALL | SCHI_SET_CTRL_MODE |
		    SCHI_CLR_INT_PDN | SCHI_CLR_RESET | SCHI_SET_PDN;
	}

	/*
	 * 4) Wait 12 SCLKS or 12/(5.5125*64) ms
	 */
	drv_usecwait(34);
	cs->chi_state = CHI_IN_CTRL_MODE;
#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug) {
		uint_t tmp;

		tmp = unitp->regsp->n.pio;
		printf("!mmcodec_setup_ctrl_mode: fini reg2=0x%x\n", tmp);
	}
#endif

	/*
	 * 6) Send Control information to MMCODEC w/ DCB bit low
	 */
	pipep = &unitp->ptab[DBRI_PIPE_CM_T_1_4];
	{
		mmcodec_ctrl_t mmctrl;

		mmctrl.word32[0] = pipep->ssp;
		mmctrl.r.dcb = 0;
		pipep->ssp = mmctrl.word32[0];
	}
	{
		dbri_chip_cmd_t	 cmd;

		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
		cmd.arg[0] = reverse_bit_order(pipep->ssp, CM_T_1_4_LEN);
		dbri_command(unitp, cmd);
		ATRACE(mmcodec_setup_ctrl_mode, 'SSP ', cmd.arg[0]);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}

	mmcodec_dts_cntlmode_pipes(unitp);

	/*
	 * 5) Start driving FSYNC and SCLK with DBRI
	 */
	mmcodec_setup_chi(unitp, SETUPCHI_CTRL_MODE);
	cs->chi_state = CHI_WAIT_DCB_LOW;
	mmcodec_enable_chi(unitp);
} /* mmcodec_setup_ctrl_mode */


/*
 * mmcodec_data_mode_ok - The first 194 frames of data mode is when the
 * CODEC does its auto-calibration, so data isn't valid for recording and
 * the CODEC won't play any data until it's finished.  This routine gets
 * called after auto-cal to send or receive data.
 */
static void
mmcodec_data_mode_ok(void *arg)
{
	dbri_unit_t *unitp = arg;
	dbri_chi_status_t *cs;

	ASSERT(unitp != NULL);
	LOCK_UNIT(unitp);
	if (unitp->suspended) {
		UNLOCK_UNIT(unitp);
		return;
	}

	cs = DBRI_CHI_STATUS_P(unitp);

	ATRACE(mmcodec_data_mode_ok, 'unit', unitp);

	{
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *pipep;

		pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
		cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
		dbri_command(unitp, cmd);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}
	{
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *palpipep;

		drv_usecwait(Dbri_keymanwait_unmute);

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp = cs->savedpal;
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(palpipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);
	}

	/*
	 * Configuration is now finished and we are in data mode!
	 */
	cs->chi_state = CHI_IN_DATA_MODE;
	mmcodec_config_finished(unitp, B_TRUE);

	UNLOCK_UNIT(unitp);
} /* mmcodec_data_mode_ok */


/*
 * mmcodec_setup_data_mode - XXX
 */
void
mmcodec_setup_data_mode(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;
	dbri_stream_t *ds;
	int mute_time;
#if 0
	dbri_pipe_t *pipep;
#endif

	ASSERT_UNITLOCKED(unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

	if (unitp->mmcwatchdog_id) {
		ATRACE(mmcodec_setup_data_mode, 'unto',
		    unitp->mmcwatchdog_id);
		(void) untimeout(unitp->mmcwatchdog_id);
		unitp->mmcwatchdog_id = 0;
	}

	/*
	 * 13) Start sending data
	 */
	mmcodec_dts_datamode_pipes(unitp);

	mmcodec_enable_chi(unitp);

	/*
	 * 14) Set D/C pin high to indicate data mode
	 */
	unitp->regsp->n.pio |= SCHI_SET_DATA_MODE;

	/*
	 * Calculate how long auto-calibration is supposed to take.
	 * It's 1/<sample rate> * 194 frames which results in 10, 20, or
	 * 30 milliseconds -- pick one.
	 *
	 * NB: experimentation has shown that doubling those time results
	 * in better signal-to-noise ratio, etc.
	 */
	ds = DChanToDs(unitp, DBRI_AUDIO_IN);
	if (ds->as.info.sample_rate >= 22050)
		mute_time = 20;
	else if (ds->as.info.sample_rate >= 11025)
		mute_time = 40;
	else
		mute_time = 60;

	if (unitp->mmcdataok_id)
		(void) untimeout(unitp->mmcdataok_id);
	unitp->mmcdataok_id = timeout(mmcodec_data_mode_ok, unitp,
	    TICKS(mute_time));

	cs->chi_nofs = 0;

	/*
	 * Unmute the outputs and set aten back to previous value.
	 * Crystal part will keep outputs muted during calibration and
	 * once that is done will unmute.  Unfortunately there is some
	 * wierd interaction where if we wait until "data_mode_ok" to set
	 * aten back, we miss the beginning of some files like drip.au.
	 * No answers right now but this is the workaround which is fine.
	 */
#if 0
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];

	{
		dbri_chip_cmd_t cmd;

		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
		cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
		dbri_command(unitp, cmd);
		ATRACE(mmcodec_setup_data_mode, 'SSP ', cmd.arg[0]);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}
#endif

	/*
	 * Send zeroes during calibration.  Actually we don't need to be
	 * sending them during calibration as the chip doesn't look at
	 * what is being send to it, but for the amount of time between
	 * when calibration is finished and when "data_mode_ok" is called
	 * we want to make sure we're filling the filter co-efficients
	 * with reaonable data.
	 */

	{
		dbri_dts_cmd_t dts;

		ATRACE(mmcodec_setup_data_mode, 'dumm', 0);

		/*
		 * Set-up dummy data pipe to send zeros; (XXX - this just
		 * screwed up 8-bit stereo even more).  The way we get
		 * away with this is by not putting any values in the
		 * pipe table, thereby hiding pipe 26 from the rest of
		 * the driver.  When a *real* data pipe gets defined in
		 * dbri_setup_dts, it will just get plopped in the linked
		 * list where 26 was and 26 will be out.
		 *
		 * For simplicity, dummy data is always 32-bits,
		 * regardless of current MMCODEC format.
		 *
		 * XXX - I *really* don't like this - this really sucks.
		 * Got any other ideas? (Besides rewriting from scratch).
		 */
		dts.opcode.word32 = 0;
		dts.output_tsd.word32 = 0;
		dts.input_tsd.word32 = 0;

		dts.opcode.r.cmd = DBRI_OPCODE_DTS;
		dts.opcode.r.id = DBRI_DTS_ID;
		dts.opcode.r.pipe = DBRI_PIPE_DUMMYDATA;
		dts.opcode.r.vo = DBRI_DTS_VO;
		dts.opcode.r.oldout = cs->pal_present ?
		    DBRI_PIPE_SB_PAL_T : DBRI_PIPE_CHI;
		dts.output_tsd.r.len = DUMMYDATA_LEN;
		dts.output_tsd.r.cycle = DUMMYDATA_CYCLE;
		dts.output_tsd.r.mode = DBRI_DTS_SINGLE;
		dts.output_tsd.r.next = DBRI_PIPE_DM_T_5_8;

		{
			dbri_chip_cmd_t	cmd;

			cmd.opcode = DBRI_CMD_SSP |
			    DBRI_SSP_PIPE(DBRI_PIPE_DUMMYDATA);
			cmd.arg[0] =
			    reverse_bit_order(cs->chi_dd_val,
			    DUMMYDATA_LEN);
			dbri_command(unitp, cmd);
		}

		{
			dbri_chip_cmd_t cmd;

			cmd.opcode = dts.opcode.word32;
			cmd.arg[0] = dts.input_tsd.word32;
			cmd.arg[1] = dts.output_tsd.word32;
			dbri_command(unitp, cmd);
			/*
			 * XXX - We are not adjusting the reference count
			 * here because this pipe was "ripped" out of
			 * DBRI's list and the driver proper thinks DBRI
			 * still knows about it.  Also, this is a short
			 * pipe "owned" by the codec routines. Do
			 * "normal" rules apply?
			 */
		}
	}
} /* mmcodec_setup_data_mode */


/*
 * mmcodec_startup_audio - XXX
 */
void
mmcodec_startup_audio(void *arg)
{
	dbri_unit_t *unitp = arg;
	dbri_chi_status_t *cs;
	dbri_pipe_t *pipep;
	aud_stream_t *as;

	LOCK_UNIT(unitp);

	if (unitp->suspended) {
		UNLOCK_UNIT(unitp);
		return;
	}

	cs = DBRI_CHI_STATUS_P(unitp);

	pipep = &unitp->ptab[DBRI_PIPE_CHI];
	if (!ISPIPEINUSE(pipep))
		mmcodec_init_pipes(unitp);

	/*
	 * Always start up as ulaw; AUDIO_IN is as good an "as" as any
	 */
	as = DChanToAs(unitp, DBRI_AUDIO_IN);
	if (cs->chi_state == CHI_POWERED_DOWN) {
		/*
		 * Update the current configuration of the audio device passed
		 * to user programs
		 */
		as = as->control_as;
		as->info.sample_rate = 8000;
		as->info.channels = 1;
		as->info.precision = 8;
		as->info.encoding = AUDIO_ENCODING_ULAW;

		as = as->input_as;
		as->info.sample_rate = 8000;
		as->info.channels = 1;
		as->info.precision = 8;
		as->info.encoding = AUDIO_ENCODING_ULAW;

		as = as->output_as;
		as->info.sample_rate = 8000;
		as->info.channels = 1;
		as->info.precision = 8;
		as->info.encoding = AUDIO_ENCODING_ULAW;

		mmcodec_set_audio_config(AsToDs(as));
		mmcodec_setup_ctrl_mode(unitp);
	}

	UNLOCK_UNIT(unitp);
} /* mmcodec_startup_audio */


/*
 * dbri_mmcodec_connect - connect an audio stream; could be play or record
 *
 * NB: This calls cv_wait_sig and thus requires user context.
 */
/*
 * REMIND: exodus 1992/09/26 - if already being configured, return
 * This routine is called once for *each* end of a connection---
 * it must return immediately the second time.
 */
boolean_t
dbri_mmcodec_connect(dbri_unit_t *unitp, dbri_chan_t dchan)
{
	dbri_chi_status_t *cs;
	dbri_stream_t *ds;
	boolean_t skip = B_FALSE;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	ATRACE(dbri_mmcodec_connect, 'dchn', dchan);

	ds = DChanToDs(unitp, dchan);

	if (cs->chi_state == CHI_NO_DEVICES) {
		/*
		 * Because of a hardware bug, if the state is
		 * CHI_NO_DEVICES it could be because of a
		 * short-circuit on the back-panel of C2+.
		 * So reset the codec and try again if this
		 * could be the case.
		 */
		mmcodec_reset(unitp);
	}


	/*
	 * If the codec is currently being initialized, this thread will
	 * have to wait until the init process is over before it can play
	 * with the device.
	 *
	 * We can use the same condition variable as we use below since
	 * it is signaled at the correct time.
	 */
	while (cs->chi_wait_for_init) {
		ATRACE(dbri_mmcodec_connect, 'wint', unitp);
		(void) cv_wait_sig(&unitp->audiocon_cv, DsToAs(ds)->lock);
		/*
		 * Don't bother doing the config on open if we've just
		 * loaded the driver.
		 * (Since opens aren't serialized, the only way we could
		 * be here is if this is the first open after a load)
		 */
		skip = B_TRUE;
	}

	ATRACE(dbri_mmcodec_connect, 'init', 0);

	/*
	 * If both a reader and a writer open at the same time, we need
	 * to make sure the following is only done once ... eg: if reader
	 * is sleeping, waiting for completion, then the writer should
	 * not call "set_audio_config", but should go to sleep waiting
	 * for the reader's sleep to wake up and then they can both
	 * continue.
	 * Since opens are now serialized in SVR4, this can't happen but
	 * we've left the code in anyway.
	 */
	if ((DsToAs(ds)->output_as->info.open &&
	    DsToAs(ds)->input_as->info.open) &&
	    (DsToAs(ds)->output_as->readq != DsToAs(ds)->input_as->readq)) {
		skip = B_TRUE;
	}

	/*
	 * We reference count the current format and don't change
	 * it if it has already been initialized.
	 */
	if (cs->chi_format_refcnt > 0) {
		skip = B_TRUE;
	} else if (cs->chi_state != CHI_NO_DEVICES) {
		aud_stream_t *as;
		int precision = ds->dfmt->llength / ds->dfmt->channels;

		as = DsToAs(ds)->control_as;
		if (as->info.sample_rate == ds->dfmt->samplerate &&
		    as->info.channels == ds->dfmt->channels &&
		    as->info.precision == precision &&
		    as->info.encoding == ds->dfmt->encoding)
			skip = B_TRUE;
		/*
		 * But... if the clocking has to change, don't
		 * skip!
		 */
		if (cs->chi_dbri_clock != cs->chi_clock)
			skip = B_FALSE;
	}

	ATRACE(dbri_mmcodec_connect, 'skip', skip);
	if (!skip) {
		aud_stream_t *as;

		/*
		 * Update the current configuration of the audio device
		 * passed to user programs
		 *
		 * XXX - should get current or default format from
		 * ds->chanformat.  For now, store the "official"
		 * format in the control_as's format field.
		 */
		as = DsToAs(ds)->control_as;
		as->info.sample_rate = ds->dfmt->samplerate;
		as->info.channels = ds->dfmt->channels;
		as->info.precision = ds->dfmt->llength / ds->dfmt->channels;
		as->info.encoding = ds->dfmt->encoding;

		as = DsToAs(ds)->input_as;
		as->info.sample_rate = ds->dfmt->samplerate;
		as->info.channels = ds->dfmt->channels;
		as->info.precision = ds->dfmt->llength / ds->dfmt->channels;
		as->info.encoding = ds->dfmt->encoding;

		as = DsToAs(ds)->output_as;
		as->info.sample_rate = ds->dfmt->samplerate;
		as->info.channels = ds->dfmt->channels;
		as->info.precision = ds->dfmt->llength / ds->dfmt->channels;
		as->info.encoding = ds->dfmt->encoding;

		ATRACE(dbri_mmcodec_connect, 'mmcc', dchan);
		mmcodec_set_audio_config(ds);
		mmcodec_setup_ctrl_mode(unitp);
		while (cs->chi_wait_for_init) {
			ATRACE(dbri_mmcodec_connect, 'wait', 1);
			(void) cv_wait_sig(&unitp->audiocon_cv,
			    DsToAs(ds)->lock);
			ATRACE(dbri_mmcodec_connect, 'tiaw', 2);
		}
	} else {
		aud_stream_t *control_as;
		aud_stream_t *as;

		/*
		 * Copy the current format from the control_as.
		 */
		control_as = DsToAs(ds)->control_as;

		as = DsToAs(ds)->input_as;
		as->info.sample_rate = control_as->info.sample_rate;
		as->info.channels = control_as->info.channels;
		as->info.precision = control_as->info.precision;
		as->info.encoding = control_as->info.encoding;

		as = DsToAs(ds)->output_as;
		as->info.sample_rate = control_as->info.sample_rate;
		as->info.channels = control_as->info.channels;
		as->info.precision = control_as->info.precision;
		as->info.encoding = control_as->info.encoding;

		ATRACE(dbri_mmcodec_connect, '!mmc', dchan);
	}

	/*
	 * See if we timed out
	 */
	if (cs->chi_state == CHI_NO_DEVICES) {
		ATRACE(dbri_mmcodec_connect, 'fail', dchan);
		return (B_FALSE);
	}

	cs->chi_format_refcnt++;
	ATRACE(dbri_mmcodec_connect, 'ref+', cs->chi_format_refcnt);

	return (B_TRUE);
} /* dbri_mmcodec_connect */


/*
 * mmcodec_insert_dummydata - XXX
 */
void
mmcodec_insert_dummydata(dbri_unit_t *unitp)
{
	dbri_chi_status_t *cs;
	dbri_dts_cmd_t dts;

	cs = DBRI_CHI_STATUS_P(unitp);

	ATRACE(mmcodec_insert_dummydata, 'DUMM', unitp);

	/*
	 * Set up dummy data pipe to send zeroes; (XXX - this
	 * just screws up 8bit stereo even more.) The way we
	 * get away with this is by not putting any values in
	 * the pipe table, thereby hiding pipe 26 from the rest
	 * of the driver. When a *real* data pipe get's defined
	 * in dbri_setup_dts, it will just get plopped in the
	 * linked list where 26 was, and 26 will be out
	 *
	 * Dummy data is always 32 bits, regardless of current
	 * MMCODEC format, for simplicity
	 *
	 * BTW, I *really* don't like this - this really sucks
	 * but got any other ideas? (Besides rewriting from scratch)
	 */
	dts.opcode.word32 = 0;
	dts.input_tsd.word32 = 0;
	dts.output_tsd.word32 = 0;

	dts.opcode.r.cmd = DBRI_OPCODE_DTS;
	dts.opcode.r.id = DBRI_DTS_ID;
	dts.opcode.r.pipe = DBRI_PIPE_DUMMYDATA;
	dts.opcode.r.vo = DBRI_DTS_VO;
	dts.opcode.r.oldout = ((uint_t)
	    (cs->pal_present ? DBRI_PIPE_SB_PAL_T :
	    DBRI_PIPE_CHI));
	dts.output_tsd.r.len = DUMMYDATA_LEN;
	dts.output_tsd.r.cycle = DUMMYDATA_CYCLE;
	dts.output_tsd.r.mode = DBRI_DTS_SINGLE;
	dts.output_tsd.r.next = DBRI_PIPE_DM_T_5_8;

	{
		dbri_chip_cmd_t cmd;

		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_DUMMYDATA);
		cmd.arg[0] = reverse_bit_order(cs->chi_dd_val, DUMMYDATA_LEN);
		dbri_command(unitp, cmd);

		cmd.opcode = dts.opcode.word32;
		cmd.arg[0] = dts.input_tsd.word32;
		cmd.arg[1] = dts.output_tsd.word32;
		dbri_command(unitp, cmd);
		/*
		 * XXX - We are not adjusting the reference count
		 * here because this pipe was "ripped" out of
		 * DBRI's list and the driver proper thinks DBRI
		 * still knows about it.
		 *
		 * XXX - This is a short pipe "owned" by the
		 * codec routines. Do "normal" rules apply?
		 */
	}
} /* mmcodec_insert_dummydata */


/*
 * dbri_mmcodec_disconnect - disconnect an audio stream; dbri_disconnect
 * has already issued SDP to stop and deleted the pipe - we just cleanup
 * for audio if this is the "last close"
 */
/*ARGSUSED*/
void
dbri_mmcodec_disconnect(dbri_unit_t *unitp, dbri_chan_t dchan)
{
	dbri_chi_status_t *cs;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	/*
	 * We reference count the current format and don't change
	 * it if it has already been initialized.
	 */
	cs->chi_format_refcnt--;
	ATRACE(dbri_mmcodec_disconnect, 'ref-', cs->chi_format_refcnt);
} /* dbri_mmcodec_disconnect */


/*
 * mmcodec_check_audio_config - returns error if invalid configuration
 */
int
mmcodec_check_audio_config(dbri_unit_t *unitp, uint_t sample_rate,
	uint_t channels, uint_t precision, uint_t encoding)
{
	mmcodec_ctrl_t	 mmctrl;

	ASSERT_UNITLOCKED(unitp);

	mmctrl.word32[0] = unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp;
	if (!Modify(encoding)) {
		if (mmctrl.r.df == MMCODEC_DF_ULAW)
			encoding = AUDIO_ENCODING_ULAW;
		else if (mmctrl.r.df == MMCODEC_DF_ALAW)
			encoding = AUDIO_ENCODING_ALAW;
		else
			encoding = AUDIO_ENCODING_LINEAR;
	}

	switch (encoding) {
	case AUDIO_ENCODING_ULAW:
	case AUDIO_ENCODING_ALAW:
		if (Modify(channels) && (channels != 1))
			return (EINVAL);
		if (Modify(precision) && (precision != 8))
			return (EINVAL);
		break;

	case AUDIO_ENCODING_LINEAR:
		if (Modify(channels) && (channels != 2) && (channels != 1))
			return (EINVAL);
		if (Modify(precision) && (precision != 16))
			return (EINVAL);
		break;

	default:
		return (EINVAL);
	} /* switch on audio encoding */

	if (Modify(sample_rate)) {
		/*
		 * Current thinking is that close only
		 * counts in horseshoes and handgrenades and
		 * audio API's - the unitp wants it exact.
		 */
		switch (sample_rate) {
		case 8000:
		case 16000:
		case 18900:
		case 32000:
		case 37800:
		case 44100:
		case 48000:
		case 9600:
			/* semi-supported now means supported */
		case 5512:
		case 11025:
		case 27429:
		case 22050:
		case 33075:
		case 6615:
			break;
		default:
			return (EINVAL);
		}
	}

	return (0);
} /* mmcodec_check_audio_config */


/*
 * mmcodec_set_audio_config - Set the configuration of the "audio device"
 * to these parameters; although the encoding can imply the values for
 * the rest of the parameters, they need to be valid as these are used to
 * fill in the global values returned to user programs.
 *
 * XXX - This routine is performing high and low level tasks;
 * 	1) generate MMCODEC hardware configuration parameters
 * 	2) store aud_stream format parameters in aud_stream_t's
 *
 * This routine assumes a valid aud_stream format request
 */
void
mmcodec_set_audio_config(dbri_stream_t *ds)
{
	dbri_unit_t *unitp;
	dbri_chi_status_t *cs;
	uint_t sample_rate;
	uint_t channels;
	uint_t encoding;
	mmcodec_ctrl_t mmctrl;
	dbri_stream_t *ds_in;

	unitp = DsToUnitp(ds);
	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);
	cs->chi_wait_for_init = B_TRUE;

	ATRACE(mmcodec_set_audio_config, '  DS', ds);

	ds_in = AsToDs(DsToAs(ds)->input_as);

	sample_rate = ds_in->as.info.sample_rate;
	channels = ds_in->as.info.channels;
	encoding = ds_in->as.info.encoding;

	mmctrl.word32[0] = unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp;

	/*
	 * Select STEREO vs. MONO
	 */
	mmctrl.r.st = (channels == 2) ? MMCODEC_ST_STEREO : MMCODEC_ST_MONO;

#if defined(AUDIOTRACE)
	if (mmctrl.r.st == MMCODEC_ST_STEREO) {
		ATRACE(mmcodec_set_audio_config, 'STER', 0);
	} else {
		ATRACE(mmcodec_set_audio_config, 'MONO', 0);
	}
#endif

	/*
	 * Configure sampling rate and crystal based on the sampling rate
	 */
	switch (sample_rate) {
	case 16000:		/* G_722 */
		ATRACE(mmcodec_set_audio_config, '16k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_16000;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;
	case 18900:		/* CDROM_XA_C */
		ATRACE(mmcodec_set_audio_config, '189k', 0);
		mmctrl.r.dfr = MMCODEC_DFR_18900;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 32000:		/* DAT_32 */
		ATRACE(mmcodec_set_audio_config, '32k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_32000;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;
	case 37800:		/* CDROM_XA_AB */
		ATRACE(mmcodec_set_audio_config, '378k', 0);
		mmctrl.r.dfr = MMCODEC_DFR_37800;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 44100:		/* CD_DA */
		ATRACE(mmcodec_set_audio_config, '441k', 0);
		mmctrl.r.dfr = MMCODEC_DFR_44100;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 48000:		/* DAT_48 */
		ATRACE(mmcodec_set_audio_config, '48k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_48000;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;
	case 9600:		/* SPEECHIO */
		ATRACE(mmcodec_set_audio_config, '9600', 0);
		mmctrl.r.dfr = MMCODEC_DFR_9600;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;
	case 8000:		/* ULAW and ALAW */
	default:
		ATRACE(mmcodec_set_audio_config, '8000', 0);
		mmctrl.r.dfr = MMCODEC_DFR_8000;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;

		/*
		 * The above are the *real* ones, these are fake
		 */
	case 5513:
		ATRACE(mmcodec_set_audio_config, '5513', 0);
		mmctrl.r.dfr = MMCODEC_DFR_5513;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 11025:
		ATRACE(mmcodec_set_audio_config, '11k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_11025;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 27429:
		ATRACE(mmcodec_set_audio_config, '275k', 0);
		mmctrl.r.dfr = MMCODEC_DFR_27429;
		mmctrl.r.mck = MMCODEC_MCK_XTAL1;
		break;
	case 22050:
		ATRACE(mmcodec_set_audio_config, '22k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_22050;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 33075:
		ATRACE(mmcodec_set_audio_config, '33k ', 0);
		mmctrl.r.dfr = MMCODEC_DFR_33075;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	case 6615:
		ATRACE(mmcodec_set_audio_config, '6615', 0);
		mmctrl.r.dfr = MMCODEC_DFR_6615;
		mmctrl.r.mck = MMCODEC_MCK_XTAL2;
		break;
	} /* switch on sampling rate */

	/*
	 * Select the proper format based on the encoding.  Also change
	 * the input buffer size to reflect the new format; for
	 * consistency we try to keep it around 1/8 seconds but we cannot
	 * go above DBRI_MAX_BSIZE.
	 *
	 * XXX - Need to document that buffer_size changes when the format
	 * does?  Is this broken?
	 */
	switch (encoding) {
	case AUDIO_ENCODING_ULAW:
		ATRACE(mmcodec_set_audio_config, 'ulaw', 0);
		mmctrl.r.df = MMCODEC_DF_ULAW;
		ds_in->as.info.buffer_size = DBRI_DEFAULT_BSIZE;
		cs->chi_dd_val = ULAW_ZERO;
		break;

	case AUDIO_ENCODING_ALAW:
		ATRACE(mmcodec_set_audio_config, 'Alaw', 0);
		mmctrl.r.df = MMCODEC_DF_ALAW;
		ds_in->as.info.buffer_size = DBRI_DEFAULT_BSIZE;
		cs->chi_dd_val = ALAW_ZERO;
		break;

	case AUDIO_ENCODING_LINEAR:
		ATRACE(mmcodec_set_audio_config, 'linr', 0);
		mmctrl.r.df = MMCODEC_DF_16_BIT;
		ds_in->as.info.buffer_size =
		    MIN((sample_rate * 2 * channels / 8),
		    DBRI_MAX_BSIZE) & (~3);
		cs->chi_dd_val = LINEAR_ZERO;
		break;
	}

	if (cs->chi_dbri_clock == DBRI_CLOCK_DBRI) {
		/*
		 * Serial-serial audio connection
		 */
		mmctrl.r.xclk = ~MMCODEC_XCLK & 1; /* no transmit clk */
		mmctrl.r.bsel = MMCODEC_BSEL_256;
		mmctrl.r.mck = MMCODEC_MCK_MSTR;
		ATRACE(mmcodec_set_audio_config, 'MCLK', 0);
	} else {
		/*
		 * Normal audio connection
		 */
#if CHI_DATA_MODE_LEN == 64
		mmctrl.r.bsel = MMCODEC_BSEL_64;
#elif CHI_DATA_MODE_LEN == 128
		mmctrl.r.bsel = MMCODEC_BSEL_128;
#elif CHI_DATA_MODE_LEN == 256
		mmctrl.r.bsel = MMCODEC_BSEL_256;
#else
#error Illegal CHI_DATA_MODE_LEN value
#endif
		mmctrl.r.xclk = MMCODEC_XCLK;
		ATRACE(mmcodec_set_audio_config, 'XCLK', 0);
	}

	unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp = mmctrl.word32[0];
} /* mmcodec_set_audio_config */


/*
 * mmcodec_set_spkrmute - This essentially determines how to set the LED more
 * than anything else. Since we have two (mutually exclusive) interests,
 * Speaker Muted and Output to Speaker enabled or disabled, and only one
 * bit (sm), this gets a litte tricky. So we use the distate.output_muted
 * as the former and play.ports as the latter. Notice that the value of
 * 'sm' at any given time doesn't really tell us anything.
 */
static int
mmcodec_set_spkrmute(dbri_unit_t *unitp, int how)
{
	dbri_pipe_t	*pipep;
	mmcodec_data_t	mmdata;
	dbri_chip_cmd_t	cmd;
	aud_stream_t	*as;
	uint_t		mutes_set = 0;
	uint_t		mutes_clr = 0;

	ASSERT_UNITLOCKED(unitp);

	as = DChanToAs(unitp, DBRI_AUDIO_OUT);
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8];
	mmdata.word32 = pipep->ssp;

	/*
	 * We optimise a little bit here and if the "new" setting
	 * is the same as the existing one, don't issue the SSP
	 */
	switch (how) {
	case SPKRMUTE_SET:
		if (unitp->distate.output_muted)
			return (-1);
		mmdata.r.sm = ~MMCODEC_SM & 1;
		mmdata.r.om0 = ~MMCODEC_OM0_ENABLE & 1;
		mmdata.r.om1 = ~MMCODEC_OM1_ENABLE & 1;
		unitp->distate.output_muted = B_TRUE;
		mutes_set = 0xe;
		break;

	case SPKRMUTE_GET:
		return (unitp->distate.output_muted);

	case SPKRMUTE_CLEAR:
		if (!unitp->distate.output_muted)
			return (-1);
		if (as->info.port & AUDIO_SPEAKER) {
			mmdata.r.sm = MMCODEC_SM;
			mutes_clr |= 0x2;
		}
		if (as->info.port & AUDIO_HEADPHONE) {
			mmdata.r.om1 = MMCODEC_OM1_ENABLE;
			mutes_clr |= 0x8;
		}
		if (as->info.port & AUDIO_LINE_OUT) {
			mmdata.r.om0 = MMCODEC_OM0_ENABLE;
			mutes_clr |= 0x4;
		}
		unitp->distate.output_muted = B_FALSE;
		break;

	case SPKRMUTE_TOGGLE:
		if (!unitp->distate.output_muted) {
			mmdata.r.sm = ~MMCODEC_SM & 1;
			mmdata.r.om0 = ~MMCODEC_OM0_ENABLE & 1;
			mmdata.r.om1 = ~MMCODEC_OM1_ENABLE & 1;
			mutes_set = 0xe;
			unitp->distate.output_muted = B_TRUE;
		} else {
			/*
			 * If muted, but output to the Speaker is disabled,
			 * then we just want to clear the light and not set sm
			 */
			mutes_set = 0xe;
			mutes_clr = 0x0;
			if (as->info.port & AUDIO_SPEAKER) {
				mmdata.r.sm = MMCODEC_SM;
				mutes_clr |= 0x2;
				mutes_set &= ~0x2;
			}
			if (as->info.port & AUDIO_HEADPHONE) {
				mmdata.r.om1 = MMCODEC_OM1_ENABLE;
				mutes_clr |= 0x8;
				mutes_set &= ~0x8;
			}
			if (as->info.port & AUDIO_LINE_OUT) {
				mmdata.r.om0 = MMCODEC_OM0_ENABLE;
				mutes_clr |= 0x4;
				mutes_set &= ~0x4;
			}
			unitp->distate.output_muted = B_FALSE;
		}
		break;
	}
	pipep->ssp = mmdata.word32;

	if (mutes_set != 0) {
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *palpipep;

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp |= mutes_set;

		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(palpipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);

		drv_usecwait(Dbri_keymanwait_mute);
	}

	/*
	 * XXX - technically, if sm did not change, we don't need to do
	 * this.
	 */
	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_DM_T_5_8);
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd);
	ATRACE(mmcodec_set_spkrmute, 'SSP ', cmd.arg[0]);

	if (mutes_clr != 0) {
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *palpipep;

		drv_usecwait(Dbri_keymanwait_unmute);

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp &= ~mutes_clr;
		if (unitp->distate.output_muted)
			palpipep->ssp |= 0x1;
		else
			palpipep->ssp &= ~0x1;
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(palpipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);
	} else {
		dbri_chip_cmd_t cmd;
		dbri_pipe_t *palpipep;

		/*
		 * Set the mute LED appropriately
		 */
		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		if (unitp->distate.output_muted)
			palpipep->ssp |= 0x1;
		else
			palpipep->ssp &= ~0x1;
		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
		cmd.arg[0] = reverse_bit_order(palpipep->ssp, SB_PAL_LEN);
		dbri_command(unitp, cmd);
	}

	cmd.opcode = DBRI_CMD_PAUSE;
	dbri_command(unitp, cmd);

	return (-1);		/* should not be used */
} /* mmcodec_set_spkrmute */


/*
 */
uchar_t
dbri_output_muted(dbri_unit_t *unitp, uchar_t val)
{
	ASSERT_UNITLOCKED(unitp);

	if (val)
		(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_SET);
	else
		(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_CLEAR);

	return (unitp->distate.output_muted);
}


/*
 * Set output port to external jack or built-in speaker
 *
 * Notice the way the "sm" field is being overloaded here - it's both a
 * speaker muted bit, and an enable for the Speaker. If we call
 * mmcodec_set_spkrmute to set this bit, it's muting the speaker and all
 * the appropriate software things happen (LED turned on and field in
 * aud_state_t set correctly). If we set it to enable or disable output
 * to the speaker, none of this happens
 */
uint_t
dbri_outport(dbri_unit_t *unitp, uint_t val)
{
	dbri_chi_status_t *cs;
	int ret_val = 0;
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;
	mmcodec_data_t origmmdata;
	dbri_chip_cmd_t cmd;
	uint_t mutes_set = 0;
	uint_t mutes_clr = 0;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	/* Pipes need to be setup prior to calling this routine */
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* ctrl in data stream */
	mmdata.word32 = pipep->ssp;
	origmmdata.word32 = pipep->ssp;

	/*
	 * On MMCODEC om0 is AUDIO_LINE_OUT, om1 is AUDIO_HEADPHONE, and
	 * sm is effectively AUDIO_SPEAKER
	 *
	 * Disable everything, then selectively enable; if output is
	 * currently muted, don't do anything but make sure ret_val is
	 * set properly.
	 */
	mmdata.r.sm = ~MMCODEC_SM & 1;
	mmdata.r.om0 = ~MMCODEC_OM0_ENABLE & 1;
	mmdata.r.om1 = ~MMCODEC_OM1_ENABLE & 1;
	if ((val & AUDIO_SPEAKER) && (cs->have_speaker)) {
		mmdata.r.sm = MMCODEC_SM;
		ret_val |= AUDIO_SPEAKER;
	}
	if ((val & AUDIO_HEADPHONE) && (cs->have_headphone)) {
		mmdata.r.om1 = MMCODEC_OM1_ENABLE;
		ret_val |= AUDIO_HEADPHONE;
	}
	if ((val & AUDIO_LINE_OUT) && (cs->have_lineout)) {
		mmdata.r.om0 = MMCODEC_OM0_ENABLE;
		ret_val |= AUDIO_LINE_OUT;
	}

	if (origmmdata.r.sm != mmdata.r.sm) {
		if (mmdata.r.sm == MMCODEC_SM)
			mutes_clr |= 0x2;
		else
			mutes_set |= 0x2;
	}
	if (origmmdata.r.om1 != mmdata.r.om1) {
		if (mmdata.r.om1 == MMCODEC_OM1_ENABLE)
			mutes_clr |= 0x8;
		else
			mutes_set |= 0x8;
	}
	if (origmmdata.r.om0 != mmdata.r.om0) {
		if (mmdata.r.om0 == MMCODEC_OM0_ENABLE)
			mutes_clr |= 0x4;
		else
			mutes_set |= 0x4;
	}

	if (mutes_set != 0) {
		dbri_pipe_t *palpipep;

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp |= mutes_set;

		if (!unitp->distate.output_muted) {
			dbri_chip_cmd_t cmd;

			cmd.opcode = DBRI_CMD_SSP |
			    DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
			cmd.arg[0] = reverse_bit_order(palpipep->ssp,
			    SB_PAL_LEN);
			dbri_command(unitp, cmd);

			drv_usecwait(Dbri_keymanwait_mute);
		}
	}

	if (!unitp->distate.output_muted) {
		pipep->ssp = mmdata.word32;

		cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
		cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
		dbri_command(unitp, cmd); /* Set short pipe data */
		ATRACE(dbri_outport, 'SSP ', cmd.arg[0]);
	}

	if (mutes_clr != 0) {
		dbri_pipe_t *palpipep;

		drv_usecwait(Dbri_keymanwait_unmute);

		palpipep = &unitp->ptab[DBRI_PIPE_SB_PAL_T];
		palpipep->ssp &= ~mutes_clr;

		if (!unitp->distate.output_muted) {
			dbri_chip_cmd_t cmd;

			cmd.opcode = DBRI_CMD_SSP |
			    DBRI_SSP_PIPE(DBRI_PIPE_SB_PAL_T);
			cmd.arg[0] = reverse_bit_order(palpipep->ssp,
			    SB_PAL_LEN);
			dbri_command(unitp, cmd);
		}
	}

	return (ret_val);
} /* dbri_outport */


/*
 * Set input port to line in jack or microphone
 */
uint_t
dbri_inport(dbri_unit_t *unitp, uint_t val)
{
	dbri_chi_status_t *cs;
	int ret_val = 0;
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;

	ASSERT_UNITLOCKED(unitp);
	cs = DBRI_CHI_STATUS_P(unitp);

	/* Pipes need to be setup prior to calling this routine */
	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* ctrl in data stream */

	/*
	 * Inputs for DBRI are mutually exclusive; Don't allow users to
	 * set LINE_IN if the hardware does not support it.
	 */
	mmdata.word32 = pipep->ssp;
	if ((val & AUDIO_LINE_IN) && (cs->have_linein)) {
		mmdata.r.is = MMCODEC_IS_LINE;
		ret_val |= AUDIO_LINE_IN;
	} else if ((val & AUDIO_MICROPHONE) && (cs->have_microphone)) {
		mmdata.r.is = MMCODEC_IS_MIC;
		ret_val |= AUDIO_MICROPHONE;
	} else {
		/* if input doesn't make sense, leave it the same */
		ret_val = (mmdata.r.is == MMCODEC_IS_MIC) ?
		    AUDIO_MICROPHONE : AUDIO_LINE_IN;
		return (ret_val);
	}

	pipep->ssp = mmdata.word32;

	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd); /* Set short pipe data */
	ATRACE(dbri_inport, 'SSP ', cmd.arg[0]);

	return (ret_val);
} /* dbri_inport */


/*
 * Convert play gain to chip values and load them.  Return the closest
 * appropriate gain value.  Use the balance field to determine what to
 * put in each of the left and right attenuation fields.
 *
 * XXX - in all the routines, dbri_play_gain, dbri_record_gain, and in
 * mmcodec_change_volume, the code mistakenly uses 0 (zero) instead of
 * using AUDIO_MIN_GAIN.
 */
uint_t
dbri_play_gain(dbri_unit_t *unitp, uint_t val, uchar_t balance)
{
	dbri_pipe_t *pipep;
	uint_t la, ra;
	uint_t tmp = 0;
	int r, l;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;

	ASSERT_UNITLOCKED(unitp);

	ATRACE(dbri_play_gain, ' val', val);
	ATRACE(dbri_play_gain, 'baln', balance);

	/*
	 * Volume changes should unmute the output
	 */
	(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_CLEAR);

	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* ctrl in data stream */
	mmdata.word32 = pipep->ssp;

	r = l = val;
	if (balance < AUDIO_MID_BALANCE) {
		r = MAX(0, (int)(val -
		    ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT)));
	} else if (balance > AUDIO_MID_BALANCE) {
		l = MAX(0, (int)(val -
		    ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT)));
	}

	if (l == 0) {
		la = MMCODEC_MAX_DEV_ATEN;
	} else {
		la = MMCODEC_MAX_ATEN -
		    (l * (MMCODEC_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	}
	if (r == 0) {
		ra = MMCODEC_MAX_DEV_ATEN;
	} else {
		ra = MMCODEC_MAX_ATEN -
		    (r * (MMCODEC_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	}

	mmdata.r.lo = la;
	mmdata.r.ro = ra;
	pipep->ssp = mmdata.word32;

	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd); /* Set short pipe data */
	ATRACE(dbri_play_gain, 'SSP ', cmd.arg[0]);

	cmd.opcode = DBRI_CMD_PAUSE; /* PAUSE command */
	dbri_command(unitp, cmd);

	if ((val == 0) || (val == AUDIO_MAX_GAIN)) {
		tmp = val;
	} else {
		if (l == val) {
			tmp = ((MMCODEC_MAX_ATEN - la) * (AUDIO_MAX_GAIN + 1) /
			    (MMCODEC_MAX_ATEN + 1));
		} else if (r == val) {
			tmp = ((MMCODEC_MAX_ATEN - ra) * (AUDIO_MAX_GAIN + 1) /
			    (MMCODEC_MAX_ATEN + 1));
		}
	}

	return (tmp);
} /* dbri_play_gain */


/*
 * Increase or Decrease the play volume, *ONE* *device* unit at a time
 */
static void
mmcodec_change_volume(dbri_unit_t *unitp, uint_t dir)
{
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;
	aud_stream_t *as_output;
	uchar_t balance;

	as_output = DChanToAs(unitp, DBRI_AUDIO_OUT);
	balance = as_output->info.balance;

	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* control in data stream */
	mmdata.word32 = pipep->ssp;

	/*
	 * Volume up button pressed - decrease attenuation; Volume down
	 * button pressed - increase attenuation.  Handle left and right
	 * attenuation individually since balance my cause them to have
	 * different values; find out which one is the "correct" one and
	 * which one is compensating for balance.
	 */
	if (balance <= AUDIO_MID_BALANCE) {
		if (dir == VPLUS_BUTTON) {
			if (mmdata.r.lo == 0)
				return;

			if (mmdata.r.lo == MMCODEC_MAX_DEV_ATEN)
				mmdata.r.lo = MMCODEC_MAX_ATEN - 1;
			else
				mmdata.r.lo--;
			mmdata.r.ro = MIN(MMCODEC_MAX_ATEN, (int)(mmdata.r.lo +
			    (AUDIO_MID_BALANCE - balance)));
		} else if (dir == VMINUS_BUTTON) {
			if (mmdata.r.lo == MMCODEC_MAX_DEV_ATEN)
				return;

			mmdata.r.lo++;
			mmdata.r.ro = MIN(MMCODEC_MAX_ATEN, (int)(mmdata.r.lo +
			    (AUDIO_MID_BALANCE - balance)));

			if (mmdata.r.lo == MMCODEC_MAX_ATEN)
				mmdata.r.lo = MMCODEC_MAX_DEV_ATEN;
			if (mmdata.r.ro == MMCODEC_MAX_ATEN)
				mmdata.r.ro = MMCODEC_MAX_DEV_ATEN;
		} else {
			return;
		}

		if (mmdata.r.lo == 0) {
			as_output->info.gain = AUDIO_MAX_GAIN;
		} else if (mmdata.r.lo == MMCODEC_MAX_DEV_ATEN) {
			as_output->info.gain = 0;
		} else {
			as_output->info.gain = (uint_t)((MMCODEC_MAX_ATEN -
			    mmdata.r.lo) * (uint_t)(AUDIO_MAX_GAIN + 1) /
			    (uint_t)(MMCODEC_MAX_ATEN + 1));
		}
	} else if (balance > AUDIO_MID_BALANCE) {
		if (dir == VPLUS_BUTTON) {
			if (mmdata.r.ro == 0)
				return;

			if (mmdata.r.ro == MMCODEC_MAX_DEV_ATEN)
				mmdata.r.ro = MMCODEC_MAX_ATEN - 1;
			else
				mmdata.r.ro--;
			mmdata.r.lo = MIN(MMCODEC_MAX_ATEN, (int)(mmdata.r.ro +
			    (balance - AUDIO_MID_BALANCE)));
		} else if (dir == VMINUS_BUTTON) {
			if (mmdata.r.ro == MMCODEC_MAX_DEV_ATEN)
				return;

			mmdata.r.ro++;
			mmdata.r.lo = MIN(MMCODEC_MAX_ATEN, (int)(mmdata.r.ro +
			    (balance - AUDIO_MID_BALANCE)));

			if (mmdata.r.lo == MMCODEC_MAX_ATEN)
				mmdata.r.lo = MMCODEC_MAX_DEV_ATEN;
			if (mmdata.r.ro == MMCODEC_MAX_ATEN)
				mmdata.r.ro = MMCODEC_MAX_DEV_ATEN;
		} else {
			return;
		}

		if (mmdata.r.ro == 0) {
			as_output->info.gain = AUDIO_MAX_GAIN;
		} else if (mmdata.r.ro == MMCODEC_MAX_DEV_ATEN) {
			as_output->info.gain = 0;
		} else {
			as_output->info.gain = (uint_t)
			    ((MMCODEC_MAX_ATEN - mmdata.r.ro) *
			    (uint_t)(AUDIO_MAX_GAIN + 1) /
			    (uint_t)(MMCODEC_MAX_ATEN + 1));
		}
	} else {
		return;
	}

	pipep->ssp = mmdata.word32;

	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd);
	ATRACE(mmcodec_change_volume, 'SSP ', cmd.arg[0]);

	cmd.opcode = DBRI_CMD_PAUSE;
	dbri_command(unitp, cmd);
} /* mmcodec_change_volume */


/*
 * Convert record gain to chip values and load them.  Return the closest
 * appropriate gain value.
 *
 * NB: Pipes must be set up before calling this routine
 */
uint_t
dbri_record_gain(dbri_unit_t *unitp, uint_t val, uchar_t balance)
{
	uint_t lg, rg;
	uint_t tmp = 0;
	int l, r;
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;

	ASSERT_UNITLOCKED(unitp);

	ATRACE(dbri_record_gain, ' val', val);
	ATRACE(dbri_record_gain, 'baln', balance);

	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* control in data stream */
	mmdata.word32 = pipep->ssp;

	l = r = val;
	if (balance < AUDIO_MID_BALANCE) {
		r = MAX(0, (int)(val -
		    ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT)));
	} else if (balance > AUDIO_MID_BALANCE) {
		l = MAX(0, (int)(val -
		    ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT)));
	}
	lg = l * (MMCODEC_MAX_GAIN + 1) / (AUDIO_MAX_GAIN + 1);
	rg = r * (MMCODEC_MAX_GAIN + 1) / (AUDIO_MAX_GAIN + 1);

	mmdata.r.lg = lg;
	mmdata.r.rg = rg;
	pipep->ssp = mmdata.word32;

	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd); /* Set short pipe data */
	ATRACE(dbri_record_gain, 'SSP ', cmd.arg[0]);

	cmd.opcode = DBRI_CMD_PAUSE; /* PAUSE command */
	dbri_command(unitp, cmd);

	/*
	 * We end up returning a value slightly different than the one
	 * passed in - *most* applications expect this.
	 */
	if (l == val) {
		tmp = ((lg + 1) * AUDIO_MAX_GAIN) / (MMCODEC_MAX_GAIN + 1);
	} else if (r == val) {
		tmp = ((rg + 1) * AUDIO_MAX_GAIN) / (MMCODEC_MAX_GAIN + 1);
	}

	return (tmp);
} /* dbri_record_gain */


/*
 * Convert monitor gain to chip values and load them.
 * Return the closest appropriate gain value.
 */
uint_t
dbri_monitor_gain(dbri_unit_t *unitp, uint_t val)
{
	int aten;
	dbri_pipe_t *pipep;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;

	ASSERT_UNITLOCKED(unitp);

	pipep = &unitp->ptab[DBRI_PIPE_DM_T_5_8]; /* ctrl in data stream */

	mmdata.word32 = pipep->ssp;
	aten = MMCODEC_MA_MAX_ATEN -
	    (val * (MMCODEC_MA_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	mmdata.r.ma = aten & 0x0f;
	pipep->ssp = mmdata.word32;

	cmd.opcode = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	cmd.arg[0] = reverse_bit_order(pipep->ssp, DM_T_5_8_LEN);
	dbri_command(unitp, cmd); /* Set short pipe data */
	ATRACE(dbri_monitor_gain, 'SSP ', cmd.arg[0]);

	cmd.opcode = DBRI_CMD_PAUSE; /* PAUSE command */
	dbri_command(unitp, cmd);

	/*
	 * We end up returning a value slightly different than the one
	 * passed in - *most* applications expect this.
	 */
	return ((val == AUDIO_MAX_GAIN) ? AUDIO_MAX_GAIN :
	    ((MMCODEC_MAX_GAIN - aten) * (AUDIO_MAX_GAIN + 1) /
	    (MMCODEC_MAX_GAIN + 1)));
} /* dbri_monitor_gain */


/*
 */
static void
mmcodec_volume_timer(void *arg)
{
	dbri_unit_t *unitp = arg;
	boolean_t done;
	dbri_pipe_t *pipep;
	aud_stream_t *control_as;

	LOCK_UNIT(unitp);
	if (unitp->suspended) {
		UNLOCK_UNIT(unitp);
		return;
	}
	unitp->mmcvolume_id = 0;

	pipep = &unitp->ptab[DBRI_PIPE_SB_PAL_R];
	done = B_TRUE;		/* done if button no longer pushed */

	if (pipep->ssp & VPLUS_BUTTON) {
		mmcodec_change_volume(unitp, VPLUS_BUTTON);
		done = B_FALSE;
	} else if (pipep->ssp & VMINUS_BUTTON) {
		mmcodec_change_volume(unitp, VMINUS_BUTTON);
		done = B_FALSE;
	}

	if (!done) {
		control_as = &unitp->ioc[DBRI_AUDIOCTL].as;
		audio_sendsig(control_as, AUDIO_SENDSIG_ALL);

		unitp->mmcvolume_id = timeout(mmcodec_volume_timer,
		    unitp, sb_button_repeat);
	}

	UNLOCK_UNIT(unitp);
} /* mmcodec_volume_timer */


/*
 * dbri_fxdt_intr - handle interrupts from fixed pipes
 */
void
dbri_fxdt_intr(dbri_unit_t *unitp, dbri_intrq_ent_t intr)
{
	dbri_chi_status_t *cs;
	uint_t t14d, r12d, r34d, tmp;
	boolean_t change;
	boolean_t restart_timeout;
	dbri_pipe_t *pipep;
	mmcodec_ctrl_t mmctrl;
	mmcodec_data_t mmdata;
	dbri_chip_cmd_t cmd;
	aud_stream_t *control_as, *as_input, *as_output;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_fxdt_intr, 'unit', unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

#if defined(AUDIOTRACE)
	if (Dbri_mmcodec_debug > 1) {
		printf("!dbri_fxdt_intr: chan %d, data 0x%x\n",
		    intr.f.channel, (uint_t)intr.code_fxdt.changed_data);
	}
#endif

	switch (intr.f.channel) {
	case DBRI_PIPE_SB_PAL_R:
		pipep = &unitp->ptab[intr.f.channel];
		tmp = reverse_bit_order(intr.code_fxdt.changed_data,
		    SB_PAL_LEN);

#if defined(AUDIOTRACE)
		if (Dbri_mmcodec_debug)
			printf("!PAL_RECV: 0x%x\n", tmp);
#endif

		/* Ignore changes to zero from CODEC handshake */
		if (tmp == 0)
			break;

		/*
		 * Check for no change; DBRIa bug #110, DBRIc bug #7,
		 * DBRIe bug #?.
		 */
		if (pipep->ssp == tmp)
			break;
		if ((tmp & ID_MASK) != PAL_ID) {
			/*
			 * XXX - we get constant changes on this pipe
			 * suggesting no speakerbox - why?
			 */
#if defined(AUDIOTRACE)
			if (Dbri_mmcodec_debug)
				printf("audio: ID 0x%x, no SpeakerBox?\n", tmp);
#endif
			break;
		}
		change = B_FALSE;
		as_input = &unitp->ioc[DBRI_AUDIO_IN].as;
		as_output = &unitp->ioc[DBRI_AUDIO_OUT].as;
		control_as = &unitp->ioc[DBRI_AUDIOCTL].as;
		if ((tmp & MIKE_IN) != (pipep->ssp & MIKE_IN)) {
			if (tmp & MIKE_IN)
				as_input->info.avail_ports |= AUDIO_MICROPHONE;
			else
				as_input->info.avail_ports &= ~AUDIO_MICROPHONE;
			as_input->info.mod_ports = as_input->info.avail_ports;
			change = B_TRUE;
		}
		if ((tmp & HPHONE_IN) != (pipep->ssp & HPHONE_IN)) {
			if (tmp & HPHONE_IN)
				as_output->info.avail_ports |= AUDIO_HEADPHONE;
			else
				as_output->info.avail_ports &= ~AUDIO_HEADPHONE;
			as_output->info.mod_ports = as_output->info.avail_ports;
			change = B_TRUE;
		}

		/*
		 * Current button policy is: Mute reduces volume alot. If
		 * you hit either volume button while muted, it will
		 * "un-mute" the speaker Leaving button pressed continues
		 * to affect volume
		 *
		 * Since pressing the buttons really fast can leave a bunch
		 * of outstanding timeout requests, everytime we see a new
		 * button press, turn off the one from before.
		 */
		pipep->ssp = tmp;
		restart_timeout = B_FALSE;
		if (pipep->ssp & VPLUS_BUTTON) {
			mmcodec_change_volume(unitp, VPLUS_BUTTON);
			(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_CLEAR);
			change = B_TRUE;
			restart_timeout = B_TRUE;
		} else if (pipep->ssp & VMINUS_BUTTON) {
			mmcodec_change_volume(unitp, VMINUS_BUTTON);
			(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_CLEAR);
			change = B_TRUE;
			restart_timeout = B_TRUE;
		} else if (pipep->ssp & MUTE_BUTTON) {
			(void) mmcodec_set_spkrmute(unitp, SPKRMUTE_TOGGLE);
			change = B_TRUE;
		}

		if (restart_timeout) {
			/*
			 * We want to restart the timer but cannot
			 * do that if we are holding the lock (and
			 * we are).  Only start it if there isn't one
			 * active.
			 */
			if (!unitp->mmcvolume_id) {
				unitp->mmcvolume_id =
				    timeout(mmcodec_volume_timer,
				    unitp, SB_BUTTON_START);
			}
		}

		/*
		 * change means two things - one is that a signal needs to
		 * be sent up the control device, and the other is that a
		 * button was pushed down; button UP events mean untimeout
		 */
		if (change) {
			audio_sendsig(control_as, AUDIO_SENDSIG_ALL);
		}
		break;

	case DBRI_PIPE_CM_R_7:
		pipep = &unitp->ptab[DBRI_PIPE_CM_R_7];
		tmp = reverse_bit_order(intr.code_fxdt.changed_data,
		    CM_R_7_LEN);

		/*
		 * We get transitions to zero when we first start up, and
		 * due to bug (that isn't a bug anymore) in the Analog
		 * CODEC, we have to ignore tranisitions when we're going
		 * to data mode from control mode (bit's after DCB are
		 * invalid)
		 */
		if ((tmp) && (cs->chi_state == CHI_WAIT_DCB_LOW) &&
		    !cs->chi_codec_rptd) {
			pipep->ssp = tmp;
			cs->chi_codec_rptd = B_TRUE;

			cmn_err(CE_CONT,
			    "?MMCODEC: Manufacturer id %u, Revision %u\n",
			    CM_R_7_MANU(pipep->ssp), CM_R_7_REV(pipep->ssp));
		}
		break;

	case DBRI_PIPE_CHI:
	case DBRI_PIPE_DM_T_5_8:
	case DBRI_PIPE_CM_T_1_4:
	case DBRI_PIPE_SB_PAL_T:
		break;

	case DBRI_PIPE_DM_R_5_6:
		pipep = &unitp->ptab[intr.f.channel];
		/*
		 * XXX5 - non-intuitive code alert
		 *
		 * The receive pipe length for this pipe is only 10, but
		 * we reverse 16-bits - this allows us to put the correct
		 * data into word16 - just don't look at the other 6-bits
		 * since they are not valid.
		 */
		tmp = reverse_bit_order(intr.code_fxdt.changed_data, 16);

		/*
		 * Check for no change; DBRIa bug #110, DBRIc bug #7 and
		 * DBRId bug #?.
		 */
		if (pipep->ssp == tmp)
			break;
		pipep->ssp = tmp;

		/*
		 * Once at max attenuation and outputs have been muted,
		 * we can go into control mode. Note that the CODEC may
		 * not really be at max attenuation since they reflect it
		 * in the data stream before it actually happens, but we
		 * should at least be muted which is what we care about.
		 */
		if (cs->chi_state == CHI_GOING_TO_CTRL_MODE) {
			mmdata.word16[0] = tmp;
			if (!mmdata.r.om1 && !mmdata.r.om0 && !mmdata.r.sm)
				mmcodec_setup_ctrl_mode(unitp);
		}
		break;

	case DBRI_PIPE_DM_R_7_8:
		/*
		 * This pipe is no longer being used.
		 */
		break;

	case DBRI_PIPE_CM_R_1_2:
	case DBRI_PIPE_CM_R_3_4:
		pipep = &unitp->ptab[intr.f.channel];
		pipep->ssp =
		    reverse_bit_order(intr.code_fxdt.changed_data, 16);

		if (intr.f.channel == DBRI_PIPE_CM_R_1_2)
			pipep->ssp &= CM_R_1_2_MASK;
		else if (intr.f.channel == DBRI_PIPE_CM_R_3_4)
			pipep->ssp &= CM_R_3_4_MASK;
		else
			return;

		t14d = unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp;
		r12d = unitp->ptab[DBRI_PIPE_CM_R_1_2].ssp;
		r34d = unitp->ptab[DBRI_PIPE_CM_R_3_4].ssp;

		/*
		 * 7) Wait for DCB from MMCODEC to go low
		 * Must check all bits as they come back to make sure
		 * they're *all* correct
		 */
		if (cs->chi_state == CHI_WAIT_DCB_LOW) {
			if (((t14d & CM_R_3_4_MASK) != r34d) ||
			    (((t14d >> 16) & CM_R_1_2_MASK) != r12d)) {
				ATRACE(dbri_fxdt_intr, '+dcb', t14d);
				return;
			}
			ATRACE(dbri_fxdt_intr, '!dcb', t14d);

			/*
			 * 8) Set DCB bit to MMCODEC high
			 */
			mmctrl.word32[0] = t14d;
			mmctrl.r.dcb = MMCODEC_DCB;

			pipep = &unitp->ptab[DBRI_PIPE_CM_T_1_4];
			pipep->ssp = mmctrl.word32[0];

			cmd.opcode =
			    DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
			cmd.arg[0] = reverse_bit_order(pipep->ssp,
			    CM_T_1_4_LEN);
			dbri_command(unitp, cmd);
			ATRACE(dbri_fxdt_intr, 'SSP ', cmd.arg[0]);

			cs->chi_state = CHI_WAIT_DCB_HIGH;

		/*
		 * 9) Wait for DCB from MMCODEC to go high
		 * Due to Analog devices chip bug (which isn't a bug cause
		 * we changed the spec for them), all bits after DCB are
		 * invalid so we can't check for them - must isolate DCB bit.
		 */
		} else if (cs->chi_state == CHI_WAIT_DCB_HIGH) {
			ATRACE(dbri_fxdt_intr, 'HIGH', 0);

			mmctrl.word16[0] = r12d;
			if (!mmctrl.r.dcb) {
				/*
				 * XXX - this is actually very bad, if
				 * we're going from low to high, we're
				 * only going to get one interrupt on
				 * #21, so if we're not straight now, we
				 * never will be; Unfortunately, DBRI
				 * randomly gives us extra fxdt
				 * interrupts so this is not B_TRUE.
				 */
#if defined(AUDIOTRACE)
				if (Dbri_mmcodec_debug)
					printf("!dcb not high yet\n");
#endif
				return;
			}
#if defined(AUDIOTRACE)
			if (Dbri_mmcodec_debug)
				printf("!dcb is high now\n");
#endif

			/*
			 * 10) Discontinue txting and receiving control info
			 */

			/*
			 * 11) Stop driving FSYNC and SCLK
			 */
			cs->chi_state = CHI_GOING_TO_DATA_MODE;

			/*
			 * try waiting 2 frames before stopping clock
			 * (1/8000)*2 seconds + slop of almost a frame
			 */
			drv_usecwait(375);

			/*
			 * XXX - This is not quite correct for monitor
			 * pipes.  There could be a unidirectional
			 * audio stream as well as a bidirectional
			 * audio stream, this the conditional should be
			 * modified.
			 */
			if (cs->chi_dbri_clock == DBRI_CLOCK_DBRI) {
				mmcodec_setup_chi(unitp,
				    SETUPCHI_DBRI_MASTER);
				cs->chi_clock = cs->chi_dbri_clock;
			} else {
				mmcodec_setup_chi(unitp,
				    SETUPCHI_CODEC_MASTER);
			}
			cs->chi_clock = cs->chi_dbri_clock;
		}
		break;

	default:
		break;

	} /* switch on channel */
} /* dbri_fxdt_intr */


/*
 * dbri_chil_intr - handle interrupts from the CHI serial interface; this
 * routine only gets called if the channel indicates CHI (36).
 */
void
dbri_chil_intr(dbri_unit_t *unitp, dbri_intrq_ent_t intr)
{
	dbri_chi_status_t *cs;
	dbri_stream_t *ds;
	dbri_chip_cmd_t cmd;

	ATRACE(dbri_chil_intr, 'unit', unitp);

	cs = DBRI_CHI_STATUS_P(unitp);

	if (intr.code_chil.overflow) {
#if defined(AUDIOTRACE)
		if (Dbri_mmcodec_debug != 0) {
			cmn_err(CE_WARN, "dbri: CHI receiver overflow/error!");
		}
#endif

		/*
		 * must disable and then reenable the CHI receiver
		 * to get things going again
		 */
		cmd.opcode = cs->cdm_cmd & ~(DBRI_CDM_REN);
		dbri_command(unitp, cmd);

		cmd.opcode = cs->cdm_cmd;
		dbri_command(unitp, cmd);

		ds = &unitp->ioc[DBRI_AUDIO_IN];
		ds->audio_uflow = B_TRUE;
	}

	switch (cs->chi_state) {
	case CHI_GOING_TO_DATA_MODE:
		ATRACE(dbri_chil_intr, 'DATA', 0);

		/*
		 * Second CHIL - to setup data mode again
		 */
		cs->chi_state = CHI_CODEC_CALIBRATION;
		mmcodec_setup_data_mode(unitp);
		break;

	case CHI_WAIT_DCB_LOW:
		/*
		 * First CHIL - to setup control mode
		 */
		break;

	default:
		/*
		 * XXX5 - V5 keeps issuing CHIL interrupts forever when
		 * the SpeakerBox is unplugged so we must issue a CHI
		 * command to disable it.  Since the CODEC handshake
		 * changes Clock masters twice (once to DBRI master and
		 * once to MMCODEC master) we could potentially get a
		 * "CHIL nofs" interrupt for each if each change between
		 * the two left no frame sync for 125us.  We don't know
		 * whether the SpeakerBox is actually unplugged until we
		 * get the third "CHIL nofs".
		 *
		 * This assumes a "quick" system.  It's been observed
		 * that the handshake can take a long time on a busy
		 * system.  We wait for some abitrarily large number of
		 * CHIL's (like 10) before giving up.
		 *
		 * Issuing the CHI command with the interrupt bit off
		 * disables the interrupts once dbri has executed it,
		 * hence the check for state.
		 */
		if ((intr.code_chil.nofs) &&
		    (cs->chi_state != CHI_NO_DEVICES) &&
		    (cs->chi_state != CHI_CODEC_CALIBRATION)) {
			cs->chi_nofs++;
			if (cs->chi_nofs > MMCODEC_NOFS_THRESHOLD) {
				cmn_err(CE_WARN,
				    "dbri: SpeakerBox unplugged\n");
				cmd.opcode = cs->chi_cmd;
				cmd.opcode &= ~(DBRI_CHI_CHIL);
				dbri_command(unitp, cmd);

				cs->chi_state = CHI_NO_DEVICES;
				/* Don't drive PIO lines - use defaults */
				unitp->regsp->n.pio = 0;

#if 0 /* XXX - not a cool thing to do, can deadlock since we hold unit lock */
				{
				dbri_pipe_t *pipep;

				/*
				 * If we're in the middle of a
				 * read/write, we should send up an
				 * M_ERROR.
				 */
				ds = &unitp->ioc[DBRI_AUDIO_IN];
				pipep = ds->pipep;
				if ((pipep != NULL) &&
				    ISPIPEINUSE(pipep)) {
					dbri_error_msg(ds, M_ERROR);
				}
				ds = &unitp->ioc[DBRI_AUDIO_OUT];
				pipep = ds->pipep;
				if ((pipep != NULL) &&
				    ISPIPEINUSE(pipep)) {
					dbri_error_msg(ds, M_ERROR);
				}
				}
#endif
			}
		}
		break;
	} /* switch on CHI state */
} /* dbri_chil_intr */


/*ARGSUSED*/
uint_t
get_aud_pipe_cycle(dbri_unit_t *unitp, dbri_pipe_t *pipep, dbri_chan_t dchan)
{
	dbri_chi_status_t *cs;
	uint_t cycle;

	cs = DBRI_CHI_STATUS_P(unitp);

	cycle = UINT_MAX;	/* a very large number */

	/*
	 * We must return different values depending on the presence of
	 * speakerbox and whether or not this is an tx or rx pipe
	 */
	switch (dchan) {
	case DBRI_AUDIO_OUT:
		if (cs->pal_present) {
			cycle = SB_PAL_LEN;
		} else {
			if (pipep->source.dchan != DBRI_CHAN_HOST &&
			    pipep->sink.dchan != DBRI_CHAN_HOST)
				cycle = 256; /* hardware direct connect */
			else
				cycle = CHI_DATA_MODE_LEN; /* normal */
		}
		break;

	case DBRI_AUDIO_IN:
		cycle = cs->mmcodec_ts_start;
		break;

	default:
		break;
	} /* switch */

	ATRACE(get_aud_pipe_cycle, 'cycl', cycle);

	return (cycle);
} /* get_aud_pipe_cycle */


/*
 */
/*ARGSUSED*/
uint_t
get_aud_pipe_length(dbri_unit_t *unitp, dbri_pipe_t *pipep, dbri_chan_t dchan)
{
	uint_t length;
	mmcodec_ctrl_t mmctrl;

	length = UINT_MAX;	/* only to be compatible with cycle above */
	mmctrl.word32[0] = unitp->ptab[DBRI_PIPE_CM_T_1_4].ssp;

	/*
	 * Janice: As I see it, we return either 8, 16, or 32 and nothing
	 * else Right? 8 bit stereo, is uninteresting as Ben says ....
	 * Notice length is *not* direction dependent.
	 *
	 * Ben: It's not that it is "uninteresting", it is just too
	 * effing hard to do this year.
	 */
	if ((mmctrl.r.df == MMCODEC_DF_ULAW) ||
	    (mmctrl.r.df == MMCODEC_DF_ALAW)) {
		length = 8;
	} else if (mmctrl.r.df == MMCODEC_DF_16_BIT) {
		length = (mmctrl.r.st == MMCODEC_ST_MONO) ? 16 : 32;
	} else {
		length = 0;
	}

	ATRACE(get_aud_pipe_length, 'leng', length);

	return (length);
} /* get_aud_pipe_length */
