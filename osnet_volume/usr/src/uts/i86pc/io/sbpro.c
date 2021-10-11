/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sbpro.c	1.68	99/05/26 SMI"

/*
 *	Device driver for the Creative Labs SoundBlaster PRO,
 * 	SoundBlaster 16, and AWE32 audio cards for the PC.
 *
 *	The Analog Devices AD184x Audio chip is also supported,
 *	including when implemented with MWSS compatibility.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/ddidmareq.h>
#include <sys/modctl.h>
#include <sys/kmem.h>
#include <sys/debug.h>

#ifdef _DDICT

/* from sys/types.h */
typedef u_int uint_t;

/* from sys/errno.h */
#define	ENOSR	63	/* out of streams resources		*/

/* from sys/stream.h */
#define	M_SETOPTS	0x10		/* set various stream head options */

/* from sys/sunddi.h */
#define	DDI_NT_AUDIO	"ddi_audio"		/* audio device */

/* from time.h */
struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

#include "sys/audioio.h"

#else /* not _DDICT */

#include <sys/audioio.h>

#endif /* not _DDICT */

#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "sys/sbpro.h"

/* patchable values */
static int	min_ints_per_second = 2;	/* interrupts per second */
int	sbdebug = 0;
u_int	sberrmask = (u_int)SBEM_ALL;		/* debug control */
int	sberrlevel = 0;
int	delay_factor = 10;			/* nominally 10usec */

#define	BUG_1180033
#define	BUG_1176536
#define	BUG_1190146

#define	MAX_DMA_LIMIT	0x10000			/* SBpro DMA size limit */
#define	DFLT_BUFSIZE	(32*1024)
#define	LO_WATER	(44100*2*2)		/* 1 second at CD Rate */
#define	HI_WATER	((LO_WATER*5)/2)	/* 2.5 seconds at CD Rate */

static	int	sbpro_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
		    void *arg, void **result);
static	int	sbpro_probe(dev_info_t *dip);
static	void	prop_load(SBInfo *info, char *prop, int *word,
		    int default_value);
static	int	sbpro_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static	u_int	intr_vect(caddr_t arg);
static	int	sbpro_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static	int	sbpro_reset(dev_info_t *dip, ddi_reset_cmd_t cmd);
static	int	sbpro_open(queue_t *q, dev_t *devp, int flag, int sflag,
		    cred_t *credp);
static	int	sbpro_close(queue_t *q, int flag, cred_t *credp);

static	int	sbpro_wput(queue_t *q, mblk_t *mp);	/* write "put" */
static	int	sbpro_wsrv(queue_t *q);		/* write "service" */
static	void	sbpro_ioctl(SBInfo *info, queue_t *q, mblk_t *mp);
static	void	sbpro_iocdata(SBInfo *info, queue_t *q, mblk_t *mp);
static	void	sbpro_start(SBInfo *info, int rw);
static	u_int	sbpro_intr(caddr_t arg);
static  int	load_write_buffer(SBInfo *info, int bufnum);
static	int	sbpro_getdmahandle(SBInfo *info, int dmamode);
static	void	sbpro_progdmaeng(SBInfo *info, int der_command, int buf_flags);
static	int	sbpro_setinfo(SBInfo *info, audio_info_t *p, queue_t *q);
static	void	sbpro_setprinfo(SBInfo *info, struct audio_prinfo *dev,
				struct audio_prinfo *user, int record_info);
static	void	sbpro_pause(SBInfo *info, int record_info);
static	void	sbpro_pause_resume(SBInfo*info, int record_info);
static	int	sbpro_checkprinfo(queue_t *q, SBInfo *info,
				struct audio_prinfo *dev,
				struct audio_prinfo *user, int record_info);
static	void	dsp_cardreset(SBInfo *info);
static	int	dsp_reset(u_short ioaddr, SBInfo *info);
static	void	dsp_dmahalt(SBInfo *info);
static	void	dsp_command(u_short ioaddr, int cmd);
static	void	dsp_speed(SBInfo *info, int rate);
static	u_char	dsp_getdata(u_short ioaddr);
static	void	dsp_dmacont(SBInfo *info);
static	void	sbpro_sendsig(queue_t *q, int signo);
static	void	dsp_writedata(SBInfo *info, u_char *dmaptr, u_char *ptr,
		    int cnt);
static	void	dsp_readdata(SBInfo *info, u_char *dmaptr, u_char *ptr,
		    int cnt);
static	void	dsp_progdma(SBInfo *info, int reading, int len);
static	void	sbpro_fast_stereo_start(SBInfo *info);
static	void	set_volume(SBInfo *info, uint_t gain, u_char balance, int reg);
static	void	sbpro_filters(SBInfo *info, int reading);
static	void	setmixer(SBInfo *info, int port, int value);
static	u_char	getmixer(u_short ioaddr, int port);
static	int	probe_card(dev_info_t *dip, SBInfo *info);
static	int	check_sbpro_intr(dev_info_t *dip, SBInfo *info, int ioaddr,
		    int dmachan8);
static void	sbpro_passthrough(SBInfo *);
static void	setreg(SBInfo *, int, int);
static int	probe_mwss_ad184x(dev_info_t *, SBInfo *, char *, int);

/* DDI Structures */

static struct module_info sbpro_info = {
	('S'<<8)|'B',			/* module id number */
	"sbpro",			/* module name */
	0,				/* min packet size accepted */
	INFPSZ,				/* max packet size accepted */
	HI_WATER,			/* hi-water mark */
	LO_WATER			/* lo-water mark */
};

static struct qinit sbpro_rinit = {
	NULL,				/* put procedure */
	NULL,				/* service procedure */
	sbpro_open,			/* called on startup */
	sbpro_close,			/* called on finish */
	NULL,				/* for future use */
	&sbpro_info,			/* module information structure */
	NULL				/* module statistics structure */
};

static struct qinit sbpro_winit = {
	sbpro_wput,			/* put procedure */
	sbpro_wsrv,			/* service procedure */
	NULL,				/* startup - not called for write q */
	NULL,				/* finish - not called for write q */
	NULL,				/* for future use */
	&sbpro_info,			/* module information structure */
	(struct module_stat *)0		/* module statistics structure */
};

static struct streamtab sbpro_streamtab = {
	&sbpro_rinit,			/* read size qinit */
	&sbpro_winit,			/* write size qinit */
	(struct qinit *)0,		/* multiplexor read init */
	(struct qinit *)0		/* multiplexor write init */
};

static	struct cb_ops	sbpro_cb_ops = {
	nodev,				/* open		*/
	nodev,				/* close	*/
	nodev,				/* strategy	*/
	nodev,				/* print	*/
	nodev,				/* dump		*/
	nodev,				/* read		*/
	nodev,				/* write	*/
	nodev,				/* ioctl	*/
	nodev,				/* devmap	*/
	nodev,				/* mmap		*/
	nodev,				/* segmap	*/
	nochpoll,			/* poll		*/
	ddi_prop_op,			/* prop_op	*/
	&sbpro_streamtab,		/* streams information */
	D_NEW | D_MP			/* driver compatability flag */
};

static struct dev_ops	sbpro_ops = {
	DEVO_REV,			/* Driver build version */
	0,				/* device reference count */
	sbpro_getinfo,			/* getinfo */
	nulldev,			/* identify */
	sbpro_probe,			/* probe */
	sbpro_attach,			/* attach */
	sbpro_detach,			/* detach */
	sbpro_reset,			/* reset */
	&sbpro_cb_ops,			/* cb_ops pointer for leaf drivers   */
	(struct bus_ops *)0		/* bus_ops pointer for nexus drivers */
};

extern struct mod_ops	mod_driverops;
static struct modldrv modldrv = {
	&mod_driverops,			/* type of module */
	"Sound Blaster",		/* name of module */
	&sbpro_ops			/* driver ops	  */
};

static struct modlinkage modlinkage = {
	MODREV_1,			/* revision number */
	{				/* list of linkage structs */
		(void *)&modldrv,
		(void *)0		/* with null termination */
	}
};

static	void	*statep = (void *)0;		/* soft state "handle" */
static kmutex_t probe_mutex[4];
static int attached[4];


/* ************************************************************************ */
/*				Device Driver Framework			    */
/* ************************************************************************ */
/*
 *	Module initialization, called when module is first loaded.
 */
int
_init(void)
{
	register int	e;
	int i;

	for (i = 0; i < 4; i++) {
		mutex_init(&probe_mutex[i], NULL, MUTEX_DRIVER, NULL);
		attached[i] = 0;
	}

	e = ddi_soft_state_init(&statep, sizeof (SBInfo), 0);
	if (e != 0) {
		SBERRPRINT(SBEP_L4, SBEM_MODS, (CE_WARN,
		    "sbpro_init: ddi_soft_state_init failed: %d.", e));
		return (e);
	}
	if ((e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&statep);
		for (i = 0; i < 4; i++)
			mutex_destroy(&probe_mutex[i]);

	}

	SBERRPRINT(SBEP_L1, SBEM_MODS, (CE_NOTE, "sbpro_init:returns %d.", e));
	return (e);
}


/*
 *	Module de-initialization, called when module is to be unloaded.
 */
int
_fini(void)
{
	register int	e;
	int i;

	if ((e = mod_remove(&modlinkage)) == 0) {
		/* module can be removed, so release our private data too */
		ddi_soft_state_fini(&statep);

		/* destroy all probe mutexs */
		for (i = 0; i < 4; i++)
			mutex_destroy(&probe_mutex[i]);
	}

	SBERRPRINT(SBEP_L1, SBEM_MODS, (CE_NOTE, "Sbpro_fini:returns %d.", e));
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 *	Functions for configuration purposes:
 */

/*ARGSUSED*/
static int
sbpro_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	dev_t	dev;
	int	instance,
		retval;
	SBInfo	*info;

	dev = (dev_t)arg;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:	/* given dev_t, return a devinfo ptr */
		instance = getminor(dev)>>2;
		info = (SBInfo *) ddi_get_soft_state(statep, instance);
		SBERRPRINT(SBEP_L1, SBEM_MODS, (CE_NOTE,
		    "sbpro_getinfo: dev= %x, inst = %x, info = %x",
		    dev, instance, info));
		if (info) {
			*result = (void *)info->dip;
			retval = DDI_SUCCESS;
		} else
			retval = DDI_FAILURE;
		break;

	case DDI_INFO_DEVT2INSTANCE:	/* given dev_t, return instance num */
		instance = getminor(dev)>>2;
		SBERRPRINT(SBEP_L1, SBEM_MODS, (CE_NOTE,
		    "sbpro_getinfo: dev= %x, inst = %x", dev, instance));
		*result = (void *)instance;
		retval = DDI_SUCCESS;
		break;

	default:
		SBERRPRINT(SBEP_L4, SBEM_MODS, (CE_WARN,
		    "sbpro_getinfo: unknown cmd 0x%x", cmd));
		retval = DDI_FAILURE;
		break;
	}
	SBERRPRINT(SBEP_L1, SBEM_MODS, (CE_NOTE,
	    "sbpro_getinfo:returns %d, result = %x", retval, *result));
	return (retval);
}


static int
sbpro_probe(dev_info_t *dip)
{
	int retval;
	int			debug[2];
	int			delay;
	int			len;

	len = sizeof (debug);
	if (GET_INT_PROP(dip, "debug", debug, &len) == DDI_PROP_SUCCESS) {
		sberrlevel = debug[0];
		if (len >= 2*sizeof (int))sberrmask = debug[1];
		sbdebug = 1;
	}

	len = sizeof (delay);
	if (GET_INT_PROP(dip, "delay_factor", &delay, &len) ==
	    DDI_PROP_SUCCESS) {
		if (delay < 10)
			cmn_err(CE_WARN, "sbpro: delay_factor may not be < 10");
		else
			delay_factor = delay;
		if (delay > 100)
			cmn_err(CE_WARN, "sbpro: delay_factor way too big");
	}

	retval = probe_card(dip, NULL);

	SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE, "sbpro:probe return %d",
								retval));
	return (retval);
}

/* used to load property. */
static void
prop_load(SBInfo *info, char *prop, int *word, int default_value)
{
	int len, value;

	len = sizeof (value);
	if (GET_INT_PROP(info->dip, prop, &value, &len) == DDI_PROP_SUCCESS &&
	    len >= sizeof (int))
		*word = value;
	else
		*word = default_value;
}

static int
sbpro_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register SBInfo		*info;
	register int		instance;
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	int			i, ret;
	off_t			offset, length;
	ddi_dma_win_t		dmawin;		/* DMA window */
	ddi_dma_seg_t		dmaseg;		/* DMA segment */
	static unsigned char silent = SILENT_BYTE;

	SBERRPRINT(SBEP_L1, SBEM_ATTA, (CE_NOTE,
	    "sbpro_attach: dip=0x%x", (int)dip));
	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(dip);	/* find out which unit */

	if (ddi_soft_state_zalloc(statep, instance) != 0) {
		SBERRPRINT(SBEP_L4, SBEM_ATTA, (CE_WARN,
		    "sbpro: couldn't allocate soft state"));
		return (DDI_FAILURE);
	}

	/* get at the space for the SBInfo structure */
	info = (SBInfo *)ddi_get_soft_state(statep, instance);

	if (probe_card(dip, info) != DDI_PROBE_SUCCESS)
		goto failed0;

	/*
	 *	Allocate DMA-able buffers for data transfers.
	 */
	prop_load(info, "bufsize", (int *)&info->buflim, DFLT_BUFSIZE);
	if (info->buflim > MAX_DMA_LIMIT)
		info->buflim = MAX_DMA_LIMIT;
	/*
	 * ensure each half buffer can handle an integral number of stereo
	 * 16-bit samples
	 */
	info->buflim &= ~7;

	i = ddi_iopb_alloc(dip, (ddi_dma_lim_t *)0, info->buflim,
	    (caddr_t *)info->buffers);
	if (i != DDI_SUCCESS) {
		cmn_err(CE_WARN, "sbpro attach: iopb_alloc(%d) failed (%d).",
			info->buflim, i);
		goto failed0;
	}

	/*
	 *	Acquire the DMA channel
	 */
	if (ddi_dmae_alloc(dip, info->dmachan8, DDI_DMA_DONTWAIT,
	    NULL) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "sbpro attach: cannot acquire dma channel %d",
		    info->dmachan8);
		goto failed;
	}

	if ((info->flags & SB16) && (info->dmachan8 != info->dmachan16) &&
	    ddi_dmae_alloc(dip, info->dmachan16, DDI_DMA_DONTWAIT, NULL)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "sbpro attach: cannot acquire dma channel %d",
		    info->dmachan16);
		goto failed1;
	}

	/* Get the DMA limit information from the DMA engine */
	if (ddi_dmae_getlim(dip, &info->dma_limits) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro attach: dmae getlim failed");
	/* now adjust the limits to reflect what we can do with the SBPro */
	info->dma_limits.dlim_sgllen = 1;   /* no scatter/gather */

	if (ddi_intr_hilevel(dip, 0)) {
		cmn_err(CE_WARN,
		    "sbpro: device can't handle high-level interrupts - "
		    "attach failed");
		goto failed2;
	}

	if (ddi_add_intr(dip, 0, &(info->iblock_cookie),
	    &info->idevice_cookie, sbpro_intr,
	    (caddr_t)info) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "sbpro attach: ddi_add_intr failed.");
		goto failed2;
	}

	/*
	 *	 Initialize mutexes and locks.
	 */
	mutex_init(&info->mutex, NULL, MUTEX_DRIVER,
	    (void *)info->iblock_cookie);

	cv_init(&info->cvopen, NULL, CV_DRIVER, NULL);
	cv_init(&info->cvclosewait, NULL, CV_DRIVER, NULL);

	prop_load(info, "pgain", (int *)&info->audio_info.play.gain,
		(info->flags & AD184x) ? AD184x_INIT_AUDIO_GAIN :
		(info->flags & SB16) ? SB16_INIT_AUDIO_GAIN : INIT_AUDIO_GAIN);

	prop_load(info, "rgain", (int *)&info->audio_info.record.gain,
		(info->flags & AD184x) ? AD184x_INIT_AUDIO_RGAIN :
		(info->flags & SB16) ? SB16_INIT_AUDIO_GAIN : AUDIO_MAX_GAIN);

	prop_load(info, "monitor", (int *)&info->audio_info.monitor_gain,
		(info->flags & AD184x) ? AD184x_INIT_AUDIO_GAIN :
		(info->flags & SB16) ? SB16_INIT_AUDIO_GAIN : INIT_AUDIO_GAIN);

	info->audio_info.hw_features = 0;
	info->audio_info.sw_features = 0;
	info->audio_info.sw_features_enabled = 0;

	info->audio_info.play.balance = AUDIO_MID_BALANCE;
	info->audio_info.record.balance = AUDIO_MID_BALANCE;
	info->audio_info.play.port = AUDIO_HEADPHONE;
	info->audio_info.record.port = AUDIO_MICROPHONE;
	info->audio_info.play.avail_ports = AUDIO_HEADPHONE;
	info->audio_info.play.mod_ports = info->audio_info.play.avail_ports;
	info->audio_info.record.avail_ports = AUDIO_MICROPHONE | AUDIO_LINE_IN;
	if (!(info->flags & CBA1847))
		info->audio_info.record.avail_ports |= AUDIO_CD;
	info->audio_info.record.mod_ports = info->audio_info.record.avail_ports;

	prop_load(info, "rate", (int *)&info->sample_rate, 8000);
	prop_load(info, "channels", &info->channels, 1);
	prop_load(info, "precision", &info->precision, 8);
	prop_load(info, "encoding", &info->encoding, AUDIO_ENCODING_ULAW);

	/* Audio(7) defaults */
	info->audio_info.record.channels =
		info->audio_info.play.channels = info->channels;

	info->audio_info.record.sample_rate = info->sample_rate;
	info->audio_info.play.sample_rate = info->sample_rate;

	info->audio_info.record.precision=
		info->audio_info.play.precision = info->precision;
	info->audio_info.record.encoding=
		info->audio_info.play.encoding = info->encoding;

	prop_load(info, "bassleft", &info->bassleft, 0x80);
	prop_load(info, "bassright", &info->bassright, 0x80);
	prop_load(info, "trebleleft", &info->trebleleft, 0x80);
	prop_load(info, "trebleright", &info->trebleright, 0x80);
	prop_load(info, "agc", &info->agc, 0);
	prop_load(info, "pcspeaker", &info->pcspeaker, 0xC0);

	prop_load(info, "inputleftgain", &info->inputleftgain, 0x80);
	prop_load(info, "inputrightgain", &info->inputrightgain, 0x80);
	prop_load(info, "outputleftgain", &info->outputleftgain, 0x80);
	prop_load(info, "outputrightgain", &info->outputrightgain, 0x80);


	/*
	 *	Create the special file(s) involved.
	 *	the data stream is minor device 0 and the control
	 *	device is minor device 1.
	 */
	if (ddi_create_minor_node(dip, "sound,sbpro", S_IFCHR, instance<<2,
	    DDI_NT_AUDIO, 0) != DDI_SUCCESS)
		goto failed3;

	if (ddi_create_minor_node(dip, "sound,sbproctl", S_IFCHR,
	    1|(instance<<2), DDI_NT_AUDIO, 0) != DDI_SUCCESS)
		goto failed3;

	if (info->flags & SBPRO) {
		/*
		 * allocate memory for single byte DMA to get stereo started
		 * correctly on an SBPRO card
		 */
		if ((ret = ddi_dma_addr_setup(info->dip, (struct as *)0,
		    (caddr_t)&silent, 1, DDI_DMA_WRITE,
		    DDI_DMA_SLEEP, 0, 0, &info->stereo_byte_dmahand))
		    != DDI_SUCCESS) {
			SBERRPRINT(SBEP_L4, SBEM_DMA, (CE_WARN,
			    "sbpro: fast stereo start: can't set up dma (%d).",
			    ret));
			goto failed3;
		}

		(void) ddi_dma_nextwin(info->stereo_byte_dmahand, 0, &dmawin);
		/*
		 * Ignored return (DDI_SUCCESS).  First window requested.
		 */
		(void) ddi_dma_nextseg(dmawin, 0, &dmaseg);
		/*
		 * Ignored return (DDI_SUCCESS).  First segment requested.
		 */
		if (ddi_dma_segtocookie(dmaseg, &offset, &length,
		    &info->sbpro_stereo_byte_cookie) == DDI_FAILURE)
			cmn_err(CE_WARN, "sbpro attach: dma segtocookie "
			    "failed");
	}

	/* Prepare card for use, initialise registers */
	dsp_cardreset(info);

	info->flags |= ATTACH_COMPLETE;
	return (DDI_SUCCESS);

failed3:
	ddi_remove_minor_node(dip, NULL);

	cv_destroy(&info->cvopen);
	cv_destroy(&info->cvclosewait);
	mutex_destroy(&info->mutex);
	ddi_remove_intr(dip, 0, info->iblock_cookie);
failed2:
	if ((info->flags & SB16) && (info->dmachan8 != info->dmachan16))
		if (ddi_dmae_release(dip, info->dmachan16) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro attach: dmae release failed, "
			    "dip %p dma channel %d",
			    (void*)dip, info->dmachan16);

failed1:
	if (ddi_dmae_release(dip, info->dmachan8) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro attach: dmae release failed, "
		    "dip %p dma channel %d", (void*)dip, info->dmachan8);

failed:
	ddi_iopb_free((caddr_t)info->buffers[0]);
failed0:
	if (info->flags & (SBPRO | SB16))
		attached[(((int)info->ioaddr)-0x220)>>5] = 0;
	if (info->nj_card)
		setmixer(info, MIXER_IRQ, 0x0);
	ddi_soft_state_free(statep, ddi_get_instance(info->dip));
	return (DDI_FAILURE);
}

static u_int
intr_vect(caddr_t arg) {
	int	*countp;

	countp = (int *)arg;
	(*countp)++;
	return (INTR_DONT_KNOW);
}


static int
sbpro_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	register SBInfo *info;
	register int	instance;
	int index;

	switch (cmd) {
	case DDI_DETACH:
		instance = ddi_get_instance(dip);	/* which unit? */
		info = (SBInfo *)ddi_get_soft_state(statep, instance);
		if (info == (SBInfo *)0) {
			cmn_err(CE_WARN, "sbpro detach: soft state is NULL");
			return (DDI_FAILURE);
		}


		/* free single byte dma handle */
		if (info->flags & SBPRO)
			if (ddi_dma_free(info->stereo_byte_dmahand))
				cmn_err(CE_WARN, "sbpro detach: dma free "
				    "failed");

		(void) ddi_remove_minor_node(dip, NULL);

		ddi_remove_intr(dip, 0, info->iblock_cookie);

		if (ddi_dmae_release(dip, info->dmachan8) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro detach: dmae release failed, "
			    "dip %p dma channel %d",
			    (void*)dip, info->dmachan8);

		if ((info->flags & SB16) && (info->dmachan8 != info->dmachan16))
			if (ddi_dmae_release(dip, info->dmachan16) !=
			    DDI_SUCCESS)
				cmn_err(CE_WARN, "sbpro detach: "
				    "dmae release failed, "
				    "dip %p dma channel %d",
				    (void*)dip, info->dmachan16);

		cv_destroy(&info->cvopen);
		cv_destroy(&info->cvclosewait);
		mutex_destroy(&info->mutex);

		ddi_iopb_free((caddr_t)info->buffers[0]);
		/*
		 * indicate that there is no longer an instance attached
		 * on this ioaddr
		 */
		if (info->flags & (SBPRO | SB16)) {
			index = (((int)info->ioaddr)-0x220)>>5;
			attached[index] = 0;
		}

		/*
		 * If we are detaching an non-jumpered SB16, then reset the
		 * irq register so that it is recognised as a non-jumpered if
		 * attached again.
		 */
		if (info->nj_card)
			setmixer(info, MIXER_IRQ, 0x0);

		ddi_soft_state_free(statep, ddi_get_instance(info->dip));
		SBERRPRINT(SBEP_L1, SBEM_ATTA, (CE_NOTE,
		    "sbpro_detach: dip=0x%x returns DDI_SUCCESS ", (int)dip));

		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}


/*
 *	Called when the system is being halted to disable all hardware
 *	interrupts.  Note that we must not block at all, not even on mutexes.
 */
static int
sbpro_reset(dev_info_t *dip, ddi_reset_cmd_t cmd)
{
	register int	instance;
	register SBInfo *info;

	instance = ddi_get_instance(dip);
	info = (SBInfo *)ddi_get_soft_state(statep, instance);

	switch (cmd) {
	case DDI_RESET_FORCE:
		if (info) {
			(void) dsp_reset(info->ioaddr, info);
			/*
			 * If we are detaching an non-jumpered SB16, then reset
			 * the irq register so that it is recognised as a
			 * non-jumpered if attached again.
			 */
			if (info->nj_card)
				setmixer(info, MIXER_IRQ, 0x0);
		}

		SBERRPRINT(SBEP_L1, SBEM_ATTA, (CE_NOTE,
		    "sbpro_reset: dip=0x%x returns DDI_SUCCESS", (int)dip));
		return (DDI_SUCCESS);

	default:
		SBERRPRINT(SBEP_L4, SBEM_ATTA, (CE_WARN,
				"sbpro_reset: unknown cmd 0x%x", cmd));
		return (DDI_FAILURE);
	}
}


/*
 *	STREAMS-flavoured open function.  Always called for the *read* side
 *	of the queue, never for the write side.
 */
/*ARGSUSED*/
static int
sbpro_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{

	int	firstopen = 0,
		playing,
		recording,
		notify = 0;
	SBInfo	*info;

	mblk_t *bp;
	struct stroptions *strop;
	major_t our_major;
	minor_t our_minor;

	our_minor = getminor(*devp);
	if (!(info = (SBInfo *)ddi_get_soft_state(statep, our_minor>>2)))
		return (ENXIO);

	mutex_enter(&info->mutex);


	SBERRPRINT(SBEP_L1, SBEM_OPEN, (CE_NOTE,
	    "sbpro open: minor is %d, flag=%d, sflag=%d q=0x%x,info=0x%x",
	    (int)our_minor, flag, sflag, q, info));

	if (q->q_ptr == (caddr_t)0)
		firstopen = 1;

	if ((our_minor&3) == 1) {
		/* control minor device */
		if (firstopen) {
			/* this is the first open of the control device */
			q->q_ptr = (caddr_t)info;
			info->ctlwrq = WR(q);	/* write-side queue */
			info->ctlrdq = (q);	/* read-side queue */
			WR(q)->q_ptr = (caddr_t)info;
			qprocson(q);

			/* if nothing open, allow passthrough */
			if (!info->audio_info.play.open &&
			    !info->audio_info.record.open)
				sbpro_passthrough(info);
		}
		mutex_exit(&info->mutex);
		return (0);
	} else if ((our_minor&3) != 0) {
		cmn_err(CE_WARN,
		    "sbpro open: illegal minor device %d", (int)our_minor);
		mutex_exit(&info->mutex);
		return (ENXIO);
	}

	/*
	 * open of the data device -- Since we clone, this will always
	 * be a "first" open
	 */

	ASSERT((our_minor&3) == 0);
	ASSERT(firstopen);
	playing = (flag & FWRITE) != 0; /* write access requested */
	recording = (flag & FREAD) != 0; /* read access requested */

	if (playing == 0 && recording == 0) {
		mutex_exit(&info->mutex);
		return (EINVAL);
	}

	/* Soundblaster cannot record and play at the same time */
	if (playing == 1 && recording == 1) {
		cmn_err(CE_WARN, "sbpro: attempt to open driver for record"
		    " AND play");
		mutex_exit(&info->mutex);
		return (EINVAL);
	}

	/*
	 * The SB16 and SBPRO
	 * are single channel devices and only support recording or playing
	 * at any one time. The audio(7) interface defines that a conformant
	 * driver can be opened by one process in read mode and another
	 * in write mode. However this isn't possible with the SB16 or SBPRO
	 * so we've changed the wait condition so that if the driver is already
	 * open in either write or read, the opening thread will wait until
	 * it is signaled by the sbpro thread in the close routine
	 * The following condition has been changed. Was :-
		while ((playing && info->audio_info.play.open) ||
			(recording && info->audio_info.record.open))
	 */
	while (info->audio_info.play.open ||
		info->audio_info.record.open) {
		if (flag & (FNDELAY | FNONBLOCK)) {
			mutex_exit(&info->mutex);
			return (EBUSY);
		}

		/*
		 *if they want to play (and no one else is already
		 * waiting), set play.waiting
		 */
		if (playing && info->audio_info.play.waiting == 0) {
			info->audio_info.play.waiting = 1;
			notify = 1;
		} else {
			notify = 0;
		}

		/* same for recording */
		if (recording && info->audio_info.record.waiting == 0) {
			info->audio_info.record.waiting = 1;
			notify = 1;
		}
		/*
		 *send SIGPOLL to control dev to show that someone
		 * is now waiting to open the device.
		 */
		if (notify && info->ctlrdq)
			sbpro_sendsig(info->ctlrdq, SIGPOLL);

		SBERRPRINT(SBEP_L1, SBEM_OPEN, (CE_NOTE, "sbpro: wait for %s",
				playing ? "Play" : "Record"));

		if (cv_wait_sig(&info->cvopen, &info->mutex) == 0) {
			/* interrupted by a signal */
			SBERRPRINT(SBEP_L1, SBEM_OPEN, (CE_NOTE,
					"sbpro:-interrupted."));
			mutex_exit(&info->mutex);
			return (EINTR);
		}
		SBERRPRINT(SBEP_L1, SBEM_OPEN, (CE_NOTE, "sbpro: - got it."));
	}

	/* alloc buffer for setting stream options */
	if ((bp = allocb((int)sizeof (struct stroptions),
	    BPRI_MED)) == NULL) {
		mutex_exit(&info->mutex);
		return (ENOSR);
	}

	/* we got the device open, so set up the state properly now */
	if (recording) {
		info->audio_info.record.open = 1;
	}
	if (playing) {
		info->audio_info.play.open = 1;
	}

	/* Tell anyone on the control dev the state has changed */
	if (info->ctlrdq)
		sbpro_sendsig(info->ctlrdq, SIGPOLL);

	/* choose an available major/minor pair and fill it in. */
	our_major = getmajor(*devp);

	/* CLONE */
	our_minor |= playing ? 2 : 3;
	*devp = makedevice(our_major, our_minor);

	if (playing) {
		/*
		 * switch off all the input channels which are mixed
		 * into the output mixer along with voice
		 */
		if (info->flags & AD184x) {
			set_volume(info, 0, 0, info->audio_info.record.port);
		} else if (info->flags & SB16) {
			info->output_switch = 0;
			setmixer(info, MIXER_16_OUTPUT, OUTPUT_ALLOFF);
		} else if (info->flags & SBPRO) {
			setmixer(info, MIXER_CD, 0);
			setmixer(info, MIXER_MIC, 0);
			setmixer(info, MIXER_LINE, 0);
		}
		set_volume(info, info->audio_info.play.gain,
		    info->audio_info.play.balance, SET_MASTER);
		/*
		 * set voice register (DAC output) to maximum
		 */
		if (info->flags & SB16) {
			setmixer(info, MIXER_16_LEFT_VOICE, MAX_VOICE);
			setmixer(info, MIXER_16_RIGHT_VOICE, MAX_VOICE);
		} else if (info->flags & SBPRO)
			setmixer(info, MIXER_VOICE, MAX_VOICE);

		info->wrq = WR(q);	/* write-side queue */
	}
	if (recording) {
		set_volume(info, info->audio_info.record.gain,
		    info->audio_info.record.balance,
		    info->audio_info.record.port);
		set_volume(info, info->audio_info.monitor_gain,
		    AUDIO_MID_BALANCE, SET_MASTER);
		/*
		 * set voice register (DAC output) to minimum
		 */
		if (info->flags & SB16) {
			setmixer(info, MIXER_16_LEFT_VOICE, VOICE_OFF);
			setmixer(info, MIXER_16_RIGHT_VOICE, VOICE_OFF);
		} else if (info->flags & SBPRO)
			setmixer(info, MIXER_VOICE, VOICE_OFF);
		info->rdq = q;	/* read-side queue */
	}

	/* also put a pointer to our info into the queue itself */
	q->q_ptr = (caddr_t)info;
	WR(q)->q_ptr = (caddr_t)info;

	/*
	 * Tell the Streams code to enable put & srv procedures.
	 */
	qprocson(q);

	strop = (struct stroptions *)bp->b_wptr;

	strop->so_flags = SO_HIWAT | SO_LOWAT;

	strop->so_hiwat = HI_WATER;
	strop->so_lowat = LO_WATER;
	bp->b_wptr += sizeof (struct stroptions);
	bp->b_datap->db_type = M_SETOPTS;
	putnext(q, bp);

	/* if flag indicates reading, start sampling now */
	if (recording) {
		info->flags |= READING;
		info->audio_info.record.active = AUDIO_IS_ACTIVE;
		sbpro_start(info, FREAD);
	}

	/* if flag indicates writing, save the flag */
	if (playing)
		info->flags |= WRITING;

	mutex_exit(&info->mutex);

	return (0);
}

/*ARGSUSED*/
static int
sbpro_close(queue_t *q, int flag, cred_t *credp)
{
	SBInfo	*info;
	int	isctl;
	int	notify;
	int	flags;

	info = (SBInfo *)q->q_ptr;	/* use our private pointer */
	ASSERT(info);

	mutex_enter(&info->mutex);

	isctl = (q == info->ctlrdq || q == info->ctlwrq);
	SBERRPRINT(SBEP_L1, SBEM_CLOS, (CE_NOTE, "sbpro close, flag=%d (%s)",
	    flag, isctl ? "CTL" : "normal"));

	if (isctl) {			/* control device */
		qprocsoff(q);		/* turn off queue stuff */
		info->ctlrdq = (queue_t *)0;
		info->ctlwrq = (queue_t *)0;

		/* if we were in passthrough mode, silence output now */
		if (!(info->flags & (READING | WRITING)))
			dsp_cardreset(info);

		mutex_exit(&info->mutex);
		return (0);
	}

	/*
	 *	If writing to the card and output is pending, wait for
	 *	it to complete.	 If we are interrupted in here, flush
	 *	anything that's still left.
	 */
	if (info->flags & W_BUSY) {
		info->flags |= CLOSE_WAIT;	/* waiting in close() */
		while (info->flags & W_BUSY) {	/* while output in progress */
			SBERRPRINT(SBEP_L1, SBEM_CLOS, (CE_NOTE,
			    "sbpro: Close Draining"));
			if (cv_wait_sig(&info->cvclosewait, &info->mutex)
			    == 0) {
				SBERRPRINT(SBEP_L1, SBEM_CLOS, (CE_NOTE,
				    "sbpro: waitsig-INTR"));
				break;
			}
		}
		SBERRPRINT(SBEP_L1, SBEM_CLOS, (CE_NOTE,
						"sbpro: Closing Drained."));
		info->flags &= ~CLOSE_WAIT;
	}

	flags = info->flags;
	dsp_cardreset(info);

	/* if control open, allow passthrough */
	if (info->ctlrdq || info->ctlwrq)
		sbpro_passthrough(info);

	if (flags&(R_BUSY|W_BUSY))
		if (ddi_dmae_stop(info->dip, info->dmachan))
			cmn_err(CE_WARN, "sbpro close: dmae stop failed");

	if (info->wmsg) {		/* free any pending output message */
		freemsg(info->wmsg);
		info->wmsg = (mblk_t *)0;
	}

	if (info->paused_buffer) {
		freemsg(info->paused_buffer);
		info->paused_buffer = (mblk_t *)0;
	}

	/*
	 * Soundblaster is unidirectional, so we must notify processes
	 * waiting to open for record, whether we were playing or recording.
	 * On a bidirectional device, move this test inside the READING case.
	 */
	notify = 0;
	if (info->audio_info.record.waiting)
		notify++;
	if (info->flags & READING) {
		info->audio_info.record.waiting = 0;
		info->audio_info.record.open = 0;
		info->audio_info.record.pause = 0;
		info->audio_info.record.error = 0;
		info->audio_info.record.eof = 0;
		info->audio_info.record.samples = 0;
		info->audio_info.record.buffer_size = 0;
		info->audio_info.record.active = 0;
		info->audio_info.record.channels = info->channels;
		info->audio_info.record.sample_rate = info->sample_rate;
		info->audio_info.record.precision = info->precision;
		info->audio_info.record.encoding = info->encoding;
	}

	/*
	 * Soundblaster is unidirectional, so we must notify processes
	 * waiting to open for play, whether we were playing or recording.
	 * On a bidirectional device, move this test inside the WRITING case.
	 */
	if (info->audio_info.play.waiting)
		notify++;
	if (info->flags & WRITING) {	/* play side */
		info->audio_info.play.waiting = 0;
		info->audio_info.play.open = 0;
		info->audio_info.play.pause = 0;
		info->audio_info.play.error = 0;
		info->audio_info.play.eof = 0;
		info->audio_info.play.samples = 0;
		info->audio_info.play.buffer_size = 0;
		info->audio_info.play.active = 0;
		info->audio_info.play.channels = info->channels;
		info->audio_info.play.sample_rate = info->sample_rate;
		info->audio_info.play.precision = info->precision;
		info->audio_info.play.encoding = info->encoding;
	}

#ifdef BUG_1180033
	/*
	 * Play and record sides must be kept the same for now
	 * As a workaround for BugId 1180033
	 */
	info->audio_info.record.channels =
		info->audio_info.play.channels = info->channels;
	info->audio_info.record.sample_rate =
		info->audio_info.play.sample_rate = info->sample_rate;
	info->audio_info.record.precision =
		info->audio_info.play.precision = info->precision;
	info->audio_info.record.encoding =
		info->audio_info.play.encoding = info->encoding;
#endif

	/* now signal the control stream */
	if (info->ctlrdq)
		sbpro_sendsig(info->ctlrdq, SIGPOLL);

	if (notify)
		cv_broadcast(&info->cvopen);	/* wakeup sleepers in open() */

	if (info->aidmahandle) {
		if (ddi_dma_free(info->aidmahandle))
			cmn_err(CE_WARN, "sbpro detach: dma free failed");
		info->aidmahandle = (ddi_dma_handle_t)0;
	}

	if (info->flags & READING) {
		info->flags &= ~READING;
		flushq(info->rdq, FLUSHALL);
		flushq(WR(info->rdq), FLUSHALL);
		info->rdq = (queue_t *)0;
	}

	if (info->flags & WRITING) {
		info->flags &= ~WRITING;
		flushq(info->wrq, FLUSHALL);
		/* just to be clean! */
		flushq(RD(info->wrq), FLUSHALL);
		info->wrq = (queue_t *)0;
	}

	ASSERT(!(info->flags&~(SB16|SBPRO|AD184x|CBA1847|ATTACH_COMPLETE)));

	/*
	 *	Tell the Streams code to disable put & srv procedures.
	 */
	qprocsoff(q);
	mutex_exit(&info->mutex);
	return (0);
}

/* ************************************************************************ */
/*				SBPRO Engine				    */
/* ************************************************************************ */
/*
 *	Write side "put" function, called when an upstream module (or the
 *	stream head) wants to pass something down to the driver.
 */
static int
sbpro_wput(queue_t *q, mblk_t *mp)
{
	SBInfo		*info;
	struct iocblk	*ip;
	int cnt;

	SBERRPRINT(SBEP_L0, SBEM_WPUT, (CE_NOTE,
	    "sbpro_wput: q=%x, mp=%x", q, mp));

	info = (SBInfo *)q->q_ptr;	/* get at our private info */

	switch (mp->b_datap->db_type) {
	case M_DATA:		/* normal user data message */
		if (q == info->ctlrdq || q == info->ctlwrq)
			freemsg(mp);	/* control device can't write */
		else {
			SBERRPRINT(SBEP_L0, SBEM_WPUT, (CE_NOTE,
					"sbpro_wput: sbpro putq DATA "));

			/* put on q for srv routine to handle */
			(void) putq(q, mp);
		}
		break;

	case M_IOCTL:		/* ioctl command */
		ip = (struct iocblk *)mp->b_rptr;
		if ((ip->ioc_cmd == AUDIO_DRAIN) &&
		    (ip->ioc_count == TRANSPARENT)) {
			if (q == info->ctlrdq || q == info->ctlwrq) {
				/* DRAIN only allowed on data stream */
				mp->b_datap->db_type = M_IOCNAK;
				ip->ioc_error = EINVAL;
				qreply(q, mp);
				break;
			}
			/*
			 * block while output drains -
			 * Because we need to handle this ioctl in sequence
			 * with the data messages on the queue, we should
			 * putq() it now, and we'll see it again in the wsrv()
			 * function.
			 */
			SBERRPRINT(SBEP_L1, SBEM_WPUT,
			    (CE_NOTE, "sbpro_wput: IOCTL - AUDIO_DRAIN"));
			/* for sbpro_wsrv() to handle */
			(void) putq(q, mp);
		} else {
			/* handle all other ioctls now */
			sbpro_ioctl(info, q, mp);
		}
		break;

	case M_IOCDATA:		/* reply to driver's M_COPYIN/OUT request */
		sbpro_iocdata(info, q, mp);
		break;

	case M_FLUSH:		/* flush the stream message ... */
		if (q == info->ctlrdq || q == info->ctlwrq) {
			if (*mp->b_rptr & FLUSHW) {
				flushq(q, FLUSHDATA);
				*mp->b_rptr &= ~FLUSHW;
			}
			if (*mp->b_rptr & FLUSHR) {
				*mp->b_rptr &= ~FLUSHR;
				qreply(q, mp); /* loop back to read-side */
			}
			else
				freemsg(mp);
			break;
		}
		/* Data side */
		if (*mp->b_rptr & FLUSHW) {	/* flush the write queue */
			int count;
			flushq(q, FLUSHDATA);
			*mp->b_rptr &= ~FLUSHW;
			/* we need to toss any pending output */
			SBERRPRINT(SBEP_L1, SBEM_WPUT, (CE_NOTE,
			    "sbpro: Flush Write "));

			mutex_enter(&info->mutex);	/* enter critical */

			/* If output is in progress, we actually halt the DMA */
			if (info->flags & W_BUSY) {
				(void) dsp_reset(info->ioaddr, info);
				if (ddi_dmae_stop(info->dip, info->dmachan) !=
				    DDI_SUCCESS) {
					cmn_err(CE_WARN, "sbpro wput: "
					    "dma stop failed");
				}
				if (!info->audio_info.play.pause)
					enableok(info->wrq);
				/*
				 * determine how much of the current buffer had
				 * played before the flush had arrived. Add
				 * this to the play sample count so that the
				 * application doesn't replay those samples
				 * when it is resuming writes.
				 */
				if (ddi_dmae_getcnt(info->dip, info->dmachan,
				    &count) != DDI_SUCCESS) {
					cmn_err(CE_WARN, "sbpro wput: "
					    "dmae getcnt failed");
				}
				if (info->bufnum == 1) {
					if (count > info->buflen)
						cnt = 3*info->buflen-count;
					else
						cnt = info->buflen-count;
				} else
					cnt = 2*info->buflen - count;

				cnt = cnt / (info->dspbits / 8);
				if (info->dsp_stereomode)
					cnt /= 2;
				info->audio_info.play.samples += cnt;
				if (info->ctlrdq)
					sbpro_sendsig(info->ctlrdq, SIGPOLL);
			}

			if (info->wmsg) {		/* msg in progress */
				freemsg(info->wmsg);	/* free it */
				info->wmsg = (mblk_t *)0;
			}

			if (info->paused_buffer) {
				freemsg(info->paused_buffer);
				info->paused_buffer = (mblk_t *)0;
			}

			mutex_exit(&info->mutex);	/* exit critical */
		}

		if (*mp->b_rptr & FLUSHR) {	/* flush the read queue */
			SBERRPRINT(SBEP_L1, SBEM_WPUT, (CE_NOTE,
						"sbpro wput: Flush Read"));
			/* If input is in progress, we actually halt the DMA */
			mutex_enter(&info->mutex);	/* enter critical */
			if (info->flags & R_BUSY) {
				(void) dsp_reset(info->ioaddr, info);
				/* stop the DMA subsystem as well */
				if (ddi_dmae_stop(info->dip, info->dmachan) !=
				    DDI_SUCCESS) {
					cmn_err(CE_WARN, "sbpro wput: "
					    "dma stop failed");
				}
				sbpro_start(info, FREAD);
			}
			mutex_exit(&info->mutex);	/* exit critical */
			*mp->b_rptr &= ~FLUSHR;
			qreply(q, mp);		/* loop back to read-side */
		} else
			freemsg(mp);
		break;

	default:
		SBERRPRINT(SBEP_L2, SBEM_WPUT, (CE_NOTE,
		    "sbpro wput: unknown message type 0x%x",
		    mp->b_datap->db_type));
		freemsg(mp);
		break;
	}
	return (0);
}

/*
 *	Write side "service" function, called by the streams scheduler
 *	after we've "put" something onto our queue and need to service
 *	it at a lower priority.
 */
static int
sbpro_wsrv(queue_t *q)
{
	mblk_t		*mp;
	SBInfo		*info;

	info = (SBInfo *)q->q_ptr;	/* get at our private info */
	mutex_enter(&info->mutex);	/* enter critical */

	SBERRPRINT(SBEP_L0, SBEM_WSRV, (CE_NOTE, "sbpro_wsrv q=%x", q));

	if (info->flags & W_BUSY) {
		SBERRPRINT(SBEP_L3, SBEM_WSRV, (CE_WARN,
					    "sbpro_wsrv, W_BUSY on entry"));
		noenable(info->wrq);
		mutex_exit(&info->mutex);	/* exit critical */
		return (0);
	}


	while ((mp = info->wmsg) != (mblk_t *)(0) ||
	    (mp = getq(q)) != (mblk_t *)0) {
		info->wmsg = 0;
		switch (mp->b_datap->db_type) {
		case M_DATA:
			/* deal with the data buffer */
			SBERRPRINT(SBEP_L1, SBEM_WSRV, (CE_NOTE,
					    "sbpro_wsrv(): M_DATA"));
			/* device isn't busy, so start it now */
			info->wmsg = mp;

			if (info->audio_info.play.pause) {
				SBERRPRINT(SBEP_L3, SBEM_WSRV, (CE_WARN,
				    "sbpro_wsrv, paused on entry", q));
				noenable(info->wrq);
				mutex_exit(&info->mutex);
				return (0);
			}

			info->audio_info.play.active = AUDIO_IS_ACTIVE;
			sbpro_start(info, FWRITE);
			if (info->flags&W_BUSY) {
				noenable(info->wrq);
				mutex_exit(&info->mutex);
				return (0);
			}
			break;

		case M_IOCTL:
			ASSERT(((struct iocblk *)(mp->b_rptr))->ioc_cmd ==
			    AUDIO_DRAIN);
			mp->b_datap->db_type = M_IOCACK;
			((struct iocblk *)mp->b_rptr)->ioc_count = 0;
			qreply(q, mp);
			SBERRPRINT(SBEP_L0, SBEM_WPUT, (CE_NOTE,
			    "sbpro: AUDIO_DRAIN processed"));
			break;

		default:
			SBERRPRINT(SBEP_L4, SBEM_WSRV, (CE_WARN,
			    "sbpro_wsrv(): unexpected message in sbpro_wsrv "
			    "message type = %x", mp->b_datap->db_type));
			freemsg(mp);
			break;
		}
	}

	info->audio_info.play.active = AUDIO_IS_NOT_ACTIVE;
	if (info->flags & CLOSE_WAIT)
		cv_signal(&info->cvclosewait);
	mutex_exit(&info->mutex);	/* exit critical */
	return (0);
}

/*
 *	Called from the write put routine to handle the audio ioctls.
 */
static void
sbpro_ioctl(SBInfo *info, queue_t *q, mblk_t *mp)
{
	struct iocblk	*ip;
	struct copyreq	*cp;
	int		cmd;

	ip = (struct iocblk *)mp->b_rptr;
	cmd = ip->ioc_cmd;


	SBERRPRINT(SBEP_L1, SBEM_IOCT, (CE_NOTE,
	    "sbpro_ioctl: q=0x%x mp=0x%x cmd=0x%x", (int)q, (int)mp, cmd));
	if (ip->ioc_count != TRANSPARENT)
		goto bad;

	/*
	 *	If data is present, it's in mp->b_cont.
	 *	For all transparent ioctls, the user's "arg" value is
	 *	in the linked M_DATA block.
	 */
	switch (cmd) {
	case AUDIO_GETINFO:		/* get audio state info */

		SBERRPRINT(SBEP_L0, SBEM_IOCT, (CE_NOTE,
		    "sbpro_ioctl(): AUDIO_GETINFO"));
		/* fill in the "copyreq" control information */
		cp = (struct copyreq *)mp->b_rptr;
		cp->cq_size = sizeof (audio_info_t);

		/* get user's address from linked M_DATA block */
		cp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
		cp->cq_flag = 0;
		freemsg(mp->b_cont);	/* we'll tack on our own soon */

		/* allocate a message block large enough for our data */
		mp->b_cont = allocb(sizeof (audio_info_t), BPRI_MED);
		if (mp->b_cont == (mblk_t *)0) {	/* failed */
			mp->b_datap->db_type = M_IOCNAK;
			ip->ioc_error = EAGAIN;		/* try again later */
			break;		/* we'll qreply() below */
		}
		mp->b_datap->db_type = M_COPYOUT;
		/* copy our data into the data block */
		mutex_enter(&info->mutex);
		bcopy((caddr_t)&info->audio_info,
		    (caddr_t)mp->b_cont->b_wptr, sizeof (audio_info_t));
		mutex_exit(&info->mutex);
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (audio_info_t);
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		break;

	case AUDIO_SETINFO:		/* set audio state info */
		SBERRPRINT(SBEP_L0, SBEM_IOCT, (CE_NOTE,
		    "sbpro_ioctl(): AUDIO_SETINFO"));
		/*
		 *	Formulate an M_COPYIN message to fetch the data
		 *	from user space, and set our private pointer so we'll
		 *	know what to do when we get the M_IOCDATA response
		 *	message.
		 */
		cp = (struct copyreq *)mp->b_rptr;

		/* Get the user address from the linked M_DATA block */
		cp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;

		cp->cq_private = (mblk_t *)cp->cq_addr;

		freemsg(mp->b_cont);	/* free linked message block */
		mp->b_cont = (mblk_t *)0;
		cp->cq_size = sizeof (audio_info_t);	/* length of data */

		cp->cq_flag = 0;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		break;

	case AUDIO_GETDEV:		/* get audio device type */
		SBERRPRINT(SBEP_L0, SBEM_IOCT, (CE_NOTE,
		    "sbpro_ioctl(): AUDIO_GETDEV"));
		/* fill in the "copyreq" control information */
		cp = (struct copyreq *)mp->b_rptr;
		cp->cq_size = sizeof (audio_device_t);

		/* get user's address from linked M_DATA block */
		cp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
		cp->cq_flag = 0;
		freemsg(mp->b_cont);	/* we'll tack on our own soon */

		/* allocate a message block large enough for our data */
		mp->b_cont = allocb(sizeof (audio_device_t), BPRI_MED);
		if (mp->b_cont == (mblk_t *)0) {	/* failed */
			mp->b_datap->db_type = M_IOCNAK;
			ip->ioc_error = EAGAIN;		/* try again later */
			break;		/* we'll qreply() below */
		}
		mp->b_datap->db_type = M_COPYOUT;
		/* copy our device type information into the data block */
		bcopy((caddr_t)&info->sbpro_devtype,
		    (caddr_t)mp->b_cont->b_wptr, sizeof (audio_device_t));
		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (audio_device_t);
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		break;

	case AUDIO_RESET:
		cmn_err(CE_WARN, "sbpro: Received AUDIO_RESET unexpectedly");
		mp->b_datap->db_type = M_IOCACK;
		ip->ioc_count = 0;
		break;

bad:
	/*
	 * Don't print out error if this is the timing hack M_IOCTL sent
	 * by strioctl(), but we will nack it.
	 */
	if (ip->ioc_cmd != -1) {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L2, SBEM_IOCT, (CE_NOTE,
		    "sbpro_ioctl(): ip->ioc_count != TRANSPARENT cmd=%d, "
		    "ip->ioc_count %d", ip->ioc_cmd, ip->ioc_count));
	}
	goto bad2;

	default:
		SBERRPRINT(SBEP_L2, SBEM_IOCT, (CE_NOTE,
		    "sbpro_ioctl(): EINVAL cmd=%d", ip->ioc_cmd));
bad2:
		mp->b_datap->db_type = M_IOCNAK;
		ip->ioc_error = EINVAL;
		break;
	}
	qreply(q, mp);
}


/*
 *	We received an "M_IOCDATA" response to an M_COPYIN or M_COPYOUT
 *	request of ours.  Process the next part of the pending ioctl.
 */
static void
sbpro_iocdata(SBInfo *info, queue_t *q, mblk_t *mp)
{
	struct iocblk	*ip;
	struct copyresp *csp;
	struct copyreq	*cp;
	audio_info_t	*outinfo;
	int play_eof, play_samples, play_error, rec_samples, rec_error;

	SBERRPRINT(SBEP_L0, SBEM_IOCD,
	    (CE_NOTE, "sbpro_iocdata: q=0x%x mp=0x%x ", (int)q, (int)mp));

	ip = (struct iocblk *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	if (csp->cp_rval != 0) {	/* failure */
		SBERRPRINT(SBEP_L3, SBEM_IOCD,
		    (CE_WARN, "sbpro_iocdata failure info=0x%x", (int)info));
		freemsg(mp);
		return;
	}

	switch (csp->cp_cmd) {
	case AUDIO_SETINFO:		/* set audio state info */
		SBERRPRINT(SBEP_L0, SBEM_IOCD,
		    (CE_NOTE, "sbpro: setinfo info=0x%x", (int)info));
		/*
		 * Private ptr will contain the address of the structure to
		 * do a COPYOUT after the SETINFO, or will contain -1 if this
		 * is a confirm to that COPYOUT
		 */
		if (csp->cp_private != (mblk_t *)-1) {
			/* this is the user's data, copied in for us */
			/* user data is in mp->b_cont */

			/* allocate a message block large enough for our data */
			mblk_t *replymp;
			replymp = allocb(sizeof (audio_info_t), BPRI_MED);
			if (replymp == (mblk_t *)0) {	/* failed */
				mp->b_datap->db_type = M_IOCNAK;
				ip->ioc_count = 0;
				ip->ioc_error = EAGAIN;	 /* try again later */
				break;		/* we'll qreply() below */
			}

			/* lock audio_info against races */
			mutex_enter(&info->mutex);

			play_samples = info->audio_info.play.samples;
			play_error = info->audio_info.play.error;
			play_eof = info->audio_info.play.eof;
			rec_samples = info->audio_info.record.samples;
			rec_error = info->audio_info.record.error;

			if (sbpro_setinfo(info,
			    (audio_info_t *)mp->b_cont->b_rptr, q) != 0) {
				mutex_exit(&info->mutex);
				freemsg(replymp);
#ifdef BUG_1190146
				if (info->ctlrdq)
					sbpro_sendsig(info->ctlrdq, SIGPOLL);
#endif
				goto bad;
			}

			freemsg(mp->b_cont);  /* we'll tack on our own soon */
			mp->b_cont = replymp;

			/*
			 * it all worked, so now we need to continue
			 * by copying out the revised info.
			 * (with the saved counts and such.)
			 */
			bcopy((caddr_t)&info->audio_info,
			    (caddr_t)mp->b_cont->b_wptr, sizeof (audio_info_t));
			outinfo = (audio_info_t *)mp->b_cont->b_wptr;
			mutex_exit(&info->mutex);

			outinfo->play.samples = play_samples;
			outinfo->play.error = (uchar_t)play_error;
			outinfo->play.eof = play_eof;
			outinfo->record.samples = rec_samples;
			outinfo->record.error = (uchar_t)rec_error;

			/*
			 *	Fill in an M_COPYOUT message to send
			 *	the data back out to user space.
			 */
			/* fill in the "copyreq" control information */
			cp = (struct copyreq *)mp->b_rptr;
			cp->cq_size = sizeof (audio_info_t);
			/* get user's address we've saved */
			cp->cq_addr = (caddr_t)csp->cp_private;
			csp->cp_private = (mblk_t *)-1;

			cp->cq_flag = 0;
			mp->b_datap->db_type = M_COPYOUT;
			mp->b_cont->b_wptr = mp->b_cont->b_rptr
			    + sizeof (audio_info_t);
			mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);

			/*
			 * send SIGPOLL to the
			 * control stream to because parms have changed.
			 */
			if (info->ctlrdq)
				sbpro_sendsig(info->ctlrdq, SIGPOLL);
			break;
		}
		/* FALLTHROUGH */

	case AUDIO_GETDEV:		/* get audio device type */
	case AUDIO_GETINFO:		/* get audio device type */
		SBERRPRINT(SBEP_L0, SBEM_IOCD, (CE_NOTE,
		    " sbpro_iocdata, cmd = %d, info=0x%x",
		    csp->cp_cmd, (int)info));
		/* we're done - just send the final ioctl result value */
		if (csp->cp_rval == 0) {	/* all went okay */
			mp->b_datap->db_type = M_IOCACK;
			ip->ioc_error = 0;
			ip->ioc_count = 0;
			ip->ioc_rval = 0;
			mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
			break;
		} else {
			freemsg(mp);
			return;
		}
	default:
		SBERRPRINT(SBEP_L3, SBEM_IOCD,
		    (CE_WARN, "sbpro: setinfo invalid cmd=0x%x", csp->cp_cmd));
bad:
		mp->b_datap->db_type = M_IOCNAK;
		ip->ioc_count = 0;
		ip->ioc_rval = -1;
		ip->ioc_error = EINVAL;
		break;
	}


	qreply(q, mp);
}

/*
 *	Start an I/O transfer.
 */
static void
sbpro_start(SBInfo *info, int rw)
{

	int			reading;
	int			channels;
	int			len;
	u_int			dma_req_flags;
	int			precision = 8;

	ASSERT(mutex_owned(&info->mutex));
	reading = (rw == FREAD) ? 1 : 0;	/* reading or writing? */

	SBERRPRINT(SBEP_L1, SBEM_STRT,
	    (CE_NOTE, "sbpro_start: info=0x%x %s %x",
	    (int)info, reading ? "read" : "write", (int)info->bufnum));


	if (reading) {
		if (info->audio_info.record.open == 0) {
			SBERRPRINT(SBEP_L3, SBEM_STRT, (CE_WARN,
			    "SBPro: START on record but device isn't open"));
			return;
		}

		if (info->flags & R_BUSY)
			return;

		if (info->audio_info.record.precision == 16)
			precision = 16;
		dma_req_flags = DDI_DMA_READ | DDI_DMA_CONSISTENT;
	} else {
		if (info->audio_info.play.open == 0) {
			SBERRPRINT(SBEP_L3, SBEM_STRT, (CE_WARN,
			    "SBPro: START on play , but device isn't open"));
			return;
		}

		if (info->flags & W_BUSY)
			return;

		if (info->audio_info.play.precision == 16)
			precision = 16;
		dma_req_flags = DDI_DMA_WRITE | DDI_DMA_CONSISTENT;
	}

	info->dspbits = precision;
	if (sbpro_getdmahandle(info, dma_req_flags)) {
		SBERRPRINT(SBEP_L3, SBEM_STRT, (CE_WARN,
		    "sbpro_start(): sbpro_getdmahandle failed"));
		return;
	}

	info->bufnum = 0;

	info->eofbuf[0] = 0;
	info->eofbuf[1] = 0;
	if (reading)
		channels = info->audio_info.record.channels;
	else
		channels = info->audio_info.play.channels;
	info->dsp_stereomode = (channels == 2) ? 1 : 0;

	if (reading) {
		info->flags |= AUTO_DMA;
		len = info->buflen;
	} else {
		/*
		 * fill both buffers, if we cannot fill both buffers
		 * set the mode to single cycle
		 * if the data is less than two full buffers we will send
		 * the data in one single cycle operation
		 */

		info->length[0] = load_write_buffer(info, 0);
		if (info->length[0] <= 0) {
			return;
		} else if (info->length[0] != info->buflen) {
			goto bufready;
		}

		info->length[1] = load_write_buffer(info, 1);
		if (info->length[1] != info->buflen) {
			info->length[0] += info->length[1];
			info->sampbuf[0] += info->sampbuf[1];
			info->eofbuf[0] += info->eofbuf[1];
			info->eofbuf[1] = 0;
			info->sampbuf[1] = 0;
			info->length[1] = 0;
		} else
			info->flags |= AUTO_DMA;
bufready:
		len = info->length[0];
	}

	if ((info->flags & SBPRO) && (info->audio_info.play.channels == 2) &&
	    !reading) {
		sbpro_filters(info, reading);
		sbpro_fast_stereo_start(info);
	} else {
		sbpro_progdmaeng(info,
		    reading ? DMAE_CMD_READ : DMAE_CMD_WRITE, DMAE_BUF_AUTO);
		if (info->flags & SBPRO)
			sbpro_filters(info, reading);
		dsp_progdma(info, reading, len);
	}
}

static u_int
sbpro_intr(caddr_t arg)
{
	register SBInfo *info;
	mblk_t		*bp;
	int		size;
	u_char		isrval;
	int		intr_claim = INTR_DONT_KNOW;
	int		i;


	info = (SBInfo *)arg;

	if (!(info->flags & ATTACH_COMPLETE))
		return (DDI_INTR_UNCLAIMED);

	/* ACK the interrupt by reading the appropriate register */
	if (info->flags & AD184x) {
#if 0
		/* MWSS specific code */
		if (inb(info->ioaddr+MWSS_IRQSTAT) & 0x40) {
			intr_claim = DDI_INTR_CLAIMED;
			if (!inb(info->ioaddr+AD184x_STATUS) & 0x01) {
				cmn_err(CE_WARN, "sbpro intr: non-ASIC intr");
				return (DDI_INTR_CLAIMED);
			}
		} else {
			/* We got someone else's interrupt */
			SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
			    "SBPRO: AD184x unclaimed interrupt"));
			return (DDI_INTR_UNCLAIMED);
		}
#else
		/* Generic AD184x code */
		if (inb(info->ioaddr+AD184x_STATUS) & 0x01) {
			intr_claim = DDI_INTR_CLAIMED;
		} else {
			/* We got someone else's interrupt */
			SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
			    "SBPRO: AD184x unclaimed interrupt"));
			return (DDI_INTR_UNCLAIMED);
		}
#endif
		outb(info->ioaddr+AD184x_STATUS, 0); /* clear int */
	} else if (info->flags & SB16) {
		/*
		 * On the SB16, we can tell for certain whether our card
		 * generated the interrupt or not, so we can properly return
		 * CLAIMED or UNCLAIMED. We will set up to return the correct
		 * value from the interrupt routine.
		 */
		isrval = getmixer(info->ioaddr, MIXER_ISR);
		if (isrval & IS_8DMA_MIDI) {
			(void) inb(info->ioaddr + DSP_DATAAVAIL);
			intr_claim = DDI_INTR_CLAIMED;
		}
		if (isrval & IS_16DMA) {
			(void) inb(info->ioaddr + DSP_DMA16_ACK);
			intr_claim = DDI_INTR_CLAIMED;
		}
		if (isrval & IS_MPU401) {
			/*
			 * This should never happen because we don't support
			 * the MPU401, but if it does, clear the bit and claim
			 * the interrupt
			 */
			(void) inb(info->ioaddr + 0x100); /* SB16 MPU401 */
			cmn_err(CE_WARN, "sbpro intr: spurious MPU intr");
			/* return and CLAIM unless we also got DMA interrupt */
			if (intr_claim != DDI_INTR_CLAIMED)
				return (DDI_INTR_CLAIMED);
		}
		if (intr_claim != DDI_INTR_CLAIMED) {
			/* We got someone else's interrupt */
			SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
			    "SBPRO: unclaimed interrupt"));
			return (DDI_INTR_UNCLAIMED);
		}
	} else if (info->flags & SBPRO) {
		/*
		 * On the PRO, we can't tell if the interrupt was ours or some
		 * other device's, so we mustn't CLAIM the interrupt, even
		 * though we will otherwise assume that it was ours.  The PRO
		 * cannot operate with shared interrupts.
		 */
		(void) inb(info->ioaddr + DSP_DATAAVAIL);
	}

	SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
	    "sbpro intr: ioaddr=%x", info->ioaddr));
	mutex_enter(&info->mutex);

	if (info->audio_info.play.open == 0 &&
	    info->audio_info.record.open == 0) {
		SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
		    "SBPro: interrupt - device isn't open."));
		mutex_exit(&info->mutex);
		return (intr_claim);
	}
	if ((info->flags & (R_BUSY | W_BUSY)) == 0) {	/* not busy */
		SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
		    "SBPro intr: device isn't busy!"));
		mutex_exit(&info->mutex);
		return (intr_claim);
	}

	size = info->count;
	if (info->flags & READING) {
		ASSERT(info->aidmahandle);
		if (ddi_dma_sync(info->aidmahandle, 0, 0,
		    DDI_DMA_SYNC_FORKERNEL) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro intr: dma sync failed");
		ASSERT(info->rdq);
		if (!canputnext(info->rdq) || !(bp = allocb(size, BPRI_MED))) {
			info->audio_info.record.error = 1;
			SBERRPRINT(SBEP_L3, SBEM_INTR, (CE_WARN,
					"sbpro_intr: out of stream bufs"));
			/* send SIGPOLL to the *control* device if it is open */
			if (info->ctlrdq)
				sbpro_sendsig(info->ctlrdq, SIGPOLL);
		} else {
			SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
			    "sbpro_intr: read %d bytes", size));
			dsp_readdata(info, info->buffers[info->bufnum],
			    bp->b_wptr, size);
			bp->b_wptr += size;
			putnext(info->rdq, bp);
			info->audio_info.record.samples += info->sampcount;
		}
		info->bufnum ^= 1;		/* switch buffer */
		mutex_exit(&info->mutex);
		return (intr_claim);
	}

	/*
	 * in case of writing
	 */

	/*
	 * Check if we just did a Single step to start stereo on play
	 * for an SBPRO
	 */
	if (info->flags & START_STEREO) {
		ASSERT(info->flags & SBPRO);
		info->flags &= ~START_STEREO;

		/* program the DMA engine */
		sbpro_progdmaeng(info, DMAE_CMD_WRITE, DMAE_BUF_AUTO);

		/* program the card to start the dma */
		dsp_progdma(info, 0, info->length[0]);

		mutex_exit(&info->mutex);
		return (intr_claim);
	}

	info->audio_info.play.samples += info->sampbuf[info->bufnum];

	if (info->eofbuf[info->bufnum]) {
		info->audio_info.play.eof += info->eofbuf[info->bufnum];
		info->eofbuf[info->bufnum] = 0;
		if (info->ctlrdq)
			sbpro_sendsig(info->ctlrdq, SIGPOLL);
	}

	if (info->flags & ENDING) {
		/* We are now playing the last buffer */
		SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
		    "sbpro_intr: got an int for auto exit"));
		info->flags &= ~(AUTO_DMA | ENDING);

		/*
		 * If not fastmode, program the card to stop at the end of the
		 * buffer
		 */
		if ((info->flags & (SBPRO | SB16)) && !info->dsp_fastmode) {
			if (info->dspbits == 8)
				dsp_command(info->ioaddr, EXIT_AUTO_DMA8);
			else
				dsp_command(info->ioaddr, EXIT_AUTO_DMA16);
		}

		/*
		 * if fastmode, pad buffer after next (current buffer) with
		 * 200 SILENT bytes so that garbage doesn't play before
		 * the interrupt routine runs and stops the card during auto
		 * init. 200 bytes gives us a minimum of 4.5 milliseconds to
		 * take the interrupt and stop the card.
		 */
		if (info->dsp_fastmode || (info->flags & AD184x))
			for (i = 0; i < 200; i++)
				info->buffers[info->bufnum][i] =
				    info->dsp_silent;
		info->bufnum ^= 1;
		mutex_exit(&info->mutex);
		return (intr_claim);
	}

	if (!(info->flags & AUTO_DMA)) {
		/* We have finished playing the last buffer */
		SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
		    "sbpro: Stop DMA after %d", info->count));

		if (info->flags & AD184x) {
			setreg(info, CFG_REG, 0);	/* play/rec off */
			setreg(info, PIN_REG, 0);	/* intr off */
		}

		if (info->dsp_fastmode)
			/* within 4.5 ms */
			(void) dsp_reset(info->ioaddr, info);

		if (ddi_dmae_stop(info->dip, info->dmachan) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro intr: dma stop failed");
		info->flags &= ~W_BUSY;
		enableok(info->wrq);
		qenable(info->wrq);
		mutex_exit(&info->mutex);
		return (intr_claim);
	}

	/*
	 * this is the most usual case,
	 * The dma is done with the last buffer and has started
	 * dma'ing the other buffer; we should load more data.
	 */
	size = load_write_buffer(info, info->bufnum);

	info->length[info->bufnum] = size;


	for (i = size; i < info->buflen; i++)
		info->buffers[info->bufnum][i] = info->dsp_silent;

	if (!size) {
		/* We are now playing the last buffer */
		if ((info->flags & (SBPRO | SB16)) && !info->dsp_fastmode) {
			if (info->dspbits == 8)
				dsp_command(info->ioaddr, EXIT_AUTO_DMA8);
			else
				dsp_command(info->ioaddr, EXIT_AUTO_DMA16);
		}
		info->flags &= ~AUTO_DMA;

		/*
		 * Because we have no short buffer, there will be no interrupt
		 * for this buffer. We need to add any eof's encountered to the
		 * alternative buffer as it will be handled on the next
		 * (and last) interrupt.
		 */
		info->eofbuf[info->bufnum^1] += info->eofbuf[info->bufnum];

	} else if (info->buflen != size) { /* partial buffer */

/*
 * We would like to program single cycle mode here, which would cause
 * an automatic exit from auto-init mode, but if the last partial buffer
 * is too short, we will get the single cycle interrupt too soon after
 * the final auto-init interrupt, and we will only see one of them.  So
 * instead we simply allow the card to play out the entire buffer, both
 * the first part with real data, and the last part with "silent" bytes.
 * This will cost at most 1/2 second delay at the end of the sample, but
 * we will update the sample count properly to reflect only real data.
 *
 * 	if (!info->dsp_fastmode) {
 * 		info->flags &= ~AUTO_DMA;
 * 		dsp_progdma(info, 0, size);
 * 	}
*/
		/*
		 * We are now playing the last full buffer, after which is a
		 * partial buffer
		 */
		info->flags |= ENDING;
	}

	info->bufnum ^= 1;
	mutex_exit(&info->mutex);
	return (intr_claim);
}

static int
load_write_buffer(SBInfo *info, int bufnum)
{
	u_char		*bufptr;
	uint_t		maxcnt;
	mblk_t		*mp;
	int		len;
	int		samples;
	int		cnt = 0, remain;

	bufptr = info->buffers[bufnum];
	maxcnt = (uint_t)info->buflen;

	SBERRPRINT(SBEP_L1, SBEM_INTR, (CE_NOTE, "sbpro: load_write_buffer"));

	for (remain = maxcnt; ; ) {
		/* first see if we have a message stored in our private info */
		if (((mp = info->paused_buffer) == (mblk_t *)0) &&
		    ((mp = info->wmsg) == (mblk_t *)0)) {
			/* no, so check the queue */
			/* get the next message from the stream */
			if ((mp = getq(info->wrq)) != (mblk_t *)0) {
				if (mp->b_datap->db_type != M_DATA) {
					ASSERT(((struct iocblk *)mp->b_rptr)->
					    ioc_cmd == AUDIO_DRAIN);
					SBERRPRINT(SBEP_L1, SBEM_INTR, (CE_NOTE,
					    "sbpro: AUDIO_DRAIN received"));
					/* let wsrv handle DRAIN */
					(void) putbq(info->wrq, mp);
					/*
					 * Return ignored - failure not
					 * important
					 */
					break;
				}
			} else {
				if (remain && (info->flags & CLOSE_WAIT) == 0) {
					info->audio_info.play.error = 1;
					if (info->ctlrdq)
						sbpro_sendsig(info->ctlrdq,
						    SIGPOLL);
				}
				break;
			}
			info->wmsg = mp;	/* save for later use */
		}

		/*
		 *	Transfer as many mblocks as will fit into our dma-able
		 *	buffer, rather than only doing one mblock.
		 */
		len = (int)(mp->b_wptr - mp->b_rptr);

		ASSERT(len >= 0);

		if (len == 0) {		/* zero-length buffer */
			mblk_t *next;

			SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
			    "sbpro: Zero buffer (%d)", len));

			if (cnt != 0) {
				/*
				 * Delay reporting this zero-buffer until
				 * interrupt time, when the buffer we're
				 * currently loading will have been played out.
				 */
				SBERRPRINT(SBEP_L1, SBEM_INTR, (CE_NOTE,
				    "sbpro: noted zero buffer."));
				info->eofbuf[bufnum]++;
			} else if (info->flags & W_BUSY) {
				/*
				 * Currently busy with other buffer, and this
				 * EOF was discovered between buffers (since
				 * cnt is zero).  We add this EOF to the end
				 * of the busy buffer so that it will be
				 * acknowledged when the busy buffer completes,
				 * rather than being delayed an extra buffer
				 * time.
				 */
				SBERRPRINT(SBEP_L1, SBEM_INTR, (CE_NOTE,
				    "sbpro: noted zero buffer between bufs."));
				info->eofbuf[bufnum^1]++;
			} else {
				/* Start of buffer and not busy */
				info->audio_info.play.eof++;
				SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
				    "sbpro: Bumping play.eof to %d.",
				    info->audio_info.play.eof));
				if ((info->flags & CLOSE_WAIT) == 0) {
					if (info->ctlrdq)
						sbpro_sendsig(info->ctlrdq,
						    SIGPOLL);
				}
			}

			next = mp->b_cont;
			mp->b_cont = (mblk_t *)0;

			freeb(mp);
			info->wmsg = next;
			continue;	/* look for another buffer */
		}

		if (remain == 0)
			break;		/* Have real data, but buffer full */

		if (len > remain) {
			/* only do as much as will fit */
			len = remain;
			SBERRPRINT(SBEP_L0, SBEM_INTR, (CE_NOTE,
			    "sbpro: take=%d ", len));
		}

		if (info->paused_buffer)
			/* don't convert already-converted messages */
			bcopy((caddr_t)mp->b_rptr, (caddr_t)bufptr, len);
		else
			/* copy the user data from the mblk to our buffer */
			dsp_writedata(info, bufptr, mp->b_rptr, len);

		ASSERT(info->aidmahandle);
		if (ddi_dma_sync(info->aidmahandle, 0, 0, DDI_DMA_SYNC_FORDEV)
		    != DDI_SUCCESS) {
			cmn_err(CE_WARN, "sbpro load write buffer: "
			    "dma sync failed");
		}
		mp->b_rptr += len;

		bufptr += len;		/* advance buffer pointer */
		cnt += len;		/* increment count */
		remain -= len;		/* decrement amount remaining */

		/* if we've emptied the mblk, free it now */
		if (mp->b_rptr >= mp->b_wptr) {
			if (info->paused_buffer) {
				info->paused_buffer = 0;
				info->eofbuf[bufnum] += info->paused_eof;
			} else {
				mblk_t *next = mp->b_cont;
				mp->b_cont = (mblk_t *)0;
				info->wmsg = next;	/* may be null */
			}
			freeb(mp);
		}
	}

	samples = cnt / (info->dspbits / 8);
	if (info->dsp_stereomode)
		info->sampbuf[bufnum] = samples / 2;
	else
		info->sampbuf[bufnum] = samples;
	return (cnt);
}

static int
sbpro_getdmahandle(SBInfo *info, int dmamode)
{
	ddi_dma_win_t	dmawin;		/* DMA window */
	ddi_dma_seg_t	dmaseg;		/* DMA segment */
	off_t	offset, length;
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	int	ret;
	int	chnls = 1;
	int	rate;
	int	speed;

	info->buflen = 0;

	if (info->flags & READING) {
		if (info->audio_info.record.channels == 2)
			chnls = 2;
		speed = info->audio_info.record.sample_rate;
		if (info->audio_info.record.buffer_size != 0)
			info->buflen = info->audio_info.record.buffer_size;
	} else {
		if (info->audio_info.play.channels == 2)
			chnls = 2;
		speed = info->audio_info.play.sample_rate;
#if 1
		/*  Not Supported by audio(7)  */
		if (info->audio_info.play.buffer_size != 0)
			info->buflen = info->audio_info.play.buffer_size;
#endif
	}

	rate = speed * chnls * (info->dspbits / 8);
	if (!info->buflen) {
		info->buflen = rate / min_ints_per_second;
	}

	/* Maximum of 30 (outrageous) interrupts per second, please */
	if (info->buflen < rate/30)
		info->buflen = rate/30;

	if (info->buflen * 2 > info->buflim)
		info->buflen = info->buflim / 2;

	if (info->buflen < rate/30)
		cmn_err(CE_WARN,
		    "sbpro:small buffer size requires %d interrupts per second",
		    rate/info->buflen);

	info->buflen &= ~3;

	if (info->aidmahandle) {
		if (ddi_dma_free(info->aidmahandle))
			cmn_err(CE_WARN, "sbpro getdmahandle: dma free failed");
		info->aidmahandle = (ddi_dma_handle_t)0;
	}

	info->dma_limits.dlim_ctreg_max = MAX_DMA_LIMIT;

	if (info->dspbits == 8)
		info->dma_limits.dlim_minxfer = DMA_UNIT_8;
	else
		info->dma_limits.dlim_minxfer = DMA_UNIT_16;

	/*
	 *	Set up the DMA transfer.
	 *	The cookie will
	 *	always have the maximum transfer size in it and
	 *	the DMA hardware will be programmed for this length.
	 *	We will program the Sound Blaster for the correct length,
	 *	and call ddi_dmae_stop() to stop the DMA controller when we
	 *	have transferred all the data we need.
	 */
	if ((ret = ddi_dma_addr_setup(info->dip, (struct as *)0,
	    (caddr_t)info->buffers[0], info->buflen * 2, dmamode, DDI_DMA_SLEEP,
	    0, &info->dma_limits, &info->aidmahandle)) != DDI_SUCCESS) {
		SBERRPRINT(SBEP_L4, SBEM_STRT, (CE_WARN,
		    "sbpro start: can't set up dma (%d).", ret));
		return (1);
	}

	/* Assume one cookie, which better be true */
	(void) ddi_dma_nextwin(info->aidmahandle, (ddi_dma_win_t)(0), &dmawin);
	/*
	 * Ignored return (DDI_SUCCESS).  First window requested.
	 */
	(void) ddi_dma_nextseg(dmawin, (ddi_dma_seg_t)(0), &dmaseg);
	/*
	 * Ignored return (DDI_SUCCESS).  First segment requested.
	 */
	if (ddi_dma_segtocookie(dmaseg, &offset, &length, &info->dmacookie) ==
	    DDI_FAILURE)
		cmn_err(CE_WARN, "sbpro_getdmahandle: dma segtocookie failed");

	SBERRPRINT(SBEP_L0, SBEM_STRT, (CE_NOTE,
	    "sbpro: DMA size is %d vs. len %d",
	    info->dmacookie.dmac_size, info->buflen));
	if (info->buflen * 2 != info->dmacookie.dmac_size) {
		cmn_err(CE_WARN, "sbpro: bad DMA cookie, size %ld should be %d",
			info->dmacookie.dmac_size, info->buflen*2);
		return (1);
	}

	/* The second buffer is set at the middle of the allocated area */
	info->buffers[1] = &info->buffers[0][info->buflen];
	return (0);
}


static void
sbpro_progdmaeng(SBInfo *info, int der_command, int buf_flags)
{
	struct ddi_dmae_req	dmaereq;


	SBERRPRINT(SBEP_L0, SBEM_STRT, (CE_NOTE,
	    "sbpro: Programming DMA engine..."));
	ASSERT(info->aidmahandle);

	bzero((caddr_t)&dmaereq, sizeof (dmaereq));
	dmaereq.der_command = (u_char)der_command;
	if ((info->flags & SB16) && info->dspbits == 16) {
		info->dmachan = info->dmachan16;
	} else {
		info->dmachan = info->dmachan8;
	}
	dmaereq.der_bufprocess = (u_char)buf_flags;
	if (ddi_dmae_prog(info->dip, &dmaereq, &info->dmacookie,
	    info->dmachan) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro progdmaeng: dmae prog failed");
}


/*
 *	Set various audio parameters based on the passed "audio_info_t" info.
 */
static int
sbpro_setinfo(SBInfo *info, audio_info_t *p, queue_t *q)
{
	int		invalid = 0;

	SBERRPRINT(SBEP_L1, SBEM_SINF, (CE_NOTE, "sbpro: Setinfo entered"));

	if (p->monitor_gain != (unsigned)-1)
		if (p->monitor_gain > AUDIO_MAX_GAIN) {
			SBERRPRINT(SBEP_L1, SBEM_SINF,
			    (CE_NOTE, "sbpro: monitor gain out of range %d",
					p->monitor_gain));
			invalid++;
		}

	invalid +=
	    sbpro_checkprinfo(q, info, &info->audio_info.record,
	    &p->record, 1) ||
	    sbpro_checkprinfo(q, info, &info->audio_info.play, &p->play, 0);

	if (!invalid) {
		sbpro_setprinfo(info, &info->audio_info.record, &p->record, 1);
		sbpro_setprinfo(info, &info->audio_info.play, &p->play, 0);

		if (p->monitor_gain != (unsigned)-1) {
			info->audio_info.monitor_gain = p->monitor_gain;
			/* if not playing, put monitor level into master */
			if (!(info->flags & WRITING))
				set_volume(info, p->monitor_gain,
				    AUDIO_MID_BALANCE, SET_MASTER);
		}

		if (p->output_muted != (u_char)-1) {
			info->audio_info.output_muted = p->output_muted;
			if (info->audio_info.play.open)
				set_volume(info, info->audio_info.play.gain,
				    info->audio_info.play.balance, SET_MASTER);
			else
				set_volume(info, info->audio_info.monitor_gain,
				    AUDIO_MID_BALANCE, SET_MASTER);
		}
	}

	return (invalid);
}


/*
 * Range checks a Setinfo record or play element of an audio_info_t structure
 * passed from the user application.
 */
static int
sbpro_checkprinfo(queue_t *q, SBInfo *info, struct audio_prinfo *dev,
				struct audio_prinfo *user, int record_info)
{
	int		invalid = 0;

	if (user->sample_rate != -1) {		/* sampling rate */
		if (user->sample_rate < 4000 || user->sample_rate >
		    (info->flags & (SBPRO | SB16) ? 44100 : 48000)) {
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: invalid sample rate=0x%x",
			    user->sample_rate));
			invalid++;
		}
		CHECK_IF_CONTROL("Sample Rate");

#ifndef BUG_1180033
		CHECK_CORRECT_SIDE("Sample Rate");
#endif

		if ((info->flags & SBPRO) && (user->sample_rate > 22050))
			if (((dev->channels == 2) && (user->channels == -1)) ||
			    (user->channels == 2)) {
				invalid++;
			}
	}

	if (user->channels != -1) {
		switch (user->channels) {
		case 2:		/* stereo */
			if (info->flags & SBPRO)
				if (((dev->sample_rate > 22050) &&
				    (user->sample_rate == -1)) ||
				    (user->sample_rate > 22050))
					invalid++;
			break;
		case 1:		/* mono */
			break;
		default:
			invalid++;
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: invalid channel=0x%x",
			    user->channels));
			break;
		}
		CHECK_IF_CONTROL("Channels");
#ifndef BUG_1180033
		CHECK_CORRECT_SIDE("Channels");
#endif

	}

	if (user->precision != -1) {
		switch (user->precision) {
		case 16: /* we don't support 16bit on SBPRO */
			if (info->flags & SBPRO) {
				SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
				    "sbpro_checkprinfo: invalid precision for "
				    "SBPRO card =0x%x", user->precision));
				invalid++;
				break;
			}

			if (((user->encoding == -1) &&
			    (dev->encoding != AUDIO_ENCODING_LINEAR)) ||
			    ((user->encoding != -1) &&
			    (user->encoding != AUDIO_ENCODING_LINEAR))) {
				invalid++;
				break;
			}
			break;

		case 8:
			break;
		default:
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: invalid precision=0x%x",
			    user->precision));
			invalid++;
			break;
		}
		CHECK_IF_CONTROL("Precision");
#ifndef BUG_1180033
		CHECK_CORRECT_SIDE("Precision");
#endif
	}

	if (user->encoding != -1) {		/* sample format */
		switch (user->encoding) {
		case AUDIO_ENCODING_LINEAR:	/* PC-compat. linear samples */
			break;
		case AUDIO_ENCODING_ALAW:
			if (info->flags & (SBPRO | SB16)) {
				SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
				    "sbpro_checkprinfo: ALAW not supported"));
				invalid++;
			}
			/*FALLTHROUGH*/
		case AUDIO_ENCODING_ULAW:	/* Sparc-compatible U-law */
			if (((user->precision == -1) &&
			    (dev->precision != 8)) ||
			    ((user->precision != -1) &&
			    (user->precision != 8))) {
				invalid++;
				break;
			}
			break;
		default:
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: unknown encoding=0x%x",
			    user->encoding));
			invalid++;
			break;
		}
		CHECK_IF_CONTROL("Encoding");
#ifndef BUG_1180033
		CHECK_CORRECT_SIDE("Encoding");
#endif
	}

	if (user->buffer_size != -1) {
		if (user->buffer_size > MAX_DMA_LIMIT) {
			/*EMPTY*/ /* in lint case */
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: buffer_size=0x%x >max",
			    user->buffer_size));
		}
	}

	if (user->pause != (unsigned char)-1) {
		/*EMPTY*/
#ifndef BUG_1180033
		CHECK_CORRECT_SIDE("Pause");
#endif
	}

	if (user->gain != -1) {			/* gain (volume) level */
		if (user->gain > AUDIO_MAX_GAIN) {
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: gain level out of range=0x%x",
			    user->gain));
			invalid++;
		}
	}

	if (user->balance != (unsigned char)-1) {
		if (user->balance > AUDIO_RIGHT_BALANCE) {
			SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
			    "sbpro_checkprinfo: balance level out of "
			    "range=0x%x", user->balance));
			invalid++;
		}

		/* Balance not supported for record on the SBPRO */
		if ((info->flags & SBPRO) && record_info)
			invalid++;
	}

	if (user->port != -1) {			/* input source/output port */
		if (record_info) {
			switch (user->port) {
			case AUDIO_LINE_IN:	/* line-level inputs */
			case AUDIO_MICROPHONE:	/* microphone jack */
			case AUDIO_CD:		/* on-card CD inputs */
				break;
			default:
				SBERRPRINT(SBEP_L2, SBEM_SPRI, (CE_NOTE,
				    "sbpro_setprinfo: invalid "
				    "port=0x%x", user->port));
				invalid++;
			}
		}
	}

	if (invalid) {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L2, SBEM_SPRI,
		    (CE_WARN, "sbpro_checkprinfo: invalid input!"));
	}

	return (invalid);
}

/*
 *	Handle setting one side of the record/play "audio_prinfo" settings.
 *	When we change the format of the data ( sample rate, encoding,
 *	precision or number of channels) we should stop the dma and since the
 *	buffer size is dependent on the data format, we should also get a new
 *	dmahandle.
 */
static void
sbpro_setprinfo(SBInfo *info, struct audio_prinfo *dev,
				struct audio_prinfo *user, int record_info)
{
	int set_device;
	int do_start = 0, do_record_volume = 0, do_play_volume = 0;

	/* Allow "passthrough" gain changes when play/record not open */
	set_device = record_info ? !info->audio_info.play.open :
	    info->audio_info.play.open;

	if (user->sample_rate != -1 && dev->sample_rate != user->sample_rate) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: sample rate=0x%x", user->sample_rate));
		if (record_info) {
#ifdef BUG_1180033
			info->audio_info.play.sample_rate =
#endif
			info->audio_info.record.sample_rate = user->sample_rate;
		} else {
#ifdef BUG_1180033
			info->audio_info.record.sample_rate =
#endif
			info->audio_info.play.sample_rate = user->sample_rate;
		}
		do_start++;
	}

	if (user->channels != -1 && dev->channels != user->channels) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: channels=0x%x", user->channels));

#ifdef BUG_1180033
		if (info->audio_info.record.open)
#else
		if (record_info)
#endif
		{
#ifdef BUG_1180033
			info->audio_info.play.channels=
#endif
			info->audio_info.record.channels = user->channels;
			do_record_volume++;
		} else {
#ifdef BUG_1180033
			info->audio_info.record.channels =
#endif
			info->audio_info.play.channels = user->channels;
			do_play_volume++;
		}
		do_start++;
	}

	/* sample size (in bits) */
	if (user->precision != -1 && user->precision != dev->precision) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: precision=0x%x", user->precision));
		if (record_info) {
#ifdef BUG_1180033
			info->audio_info.play.precision =
#endif
			info->audio_info.record.precision = user->precision;
		} else {
#ifdef BUG_1180033
			info->audio_info.record.precision =
#endif
			info->audio_info.play.precision = user->precision;
		}
		do_start++;
	}

	if (user->encoding != -1 && user->encoding != dev->encoding) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: encoding=0x%x", user->encoding));
		if (record_info) {
#ifdef BUG_1180033
			info->audio_info.play.encoding =
#endif
			info->audio_info.record.encoding = user->encoding;
		} else {
#ifdef BUG_1180033
			info->audio_info.record.encoding =
#endif
			info->audio_info.play.encoding = user->encoding;
		}
		do_start++;
	}

	if (user->buffer_size != -1 && dev->buffer_size != user->buffer_size) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI,
		    (CE_NOTE, "sbpro_setprinfo: buffer_size=0x%x",
			user->buffer_size));
		/* buffer size on support in Audio(7) for record */
		if (record_info) {
			info->audio_info.record.buffer_size =
			    user->buffer_size;
			if (!do_start && (info->flags & R_BUSY)) {
				sbpro_pause(info, record_info);
				sbpro_pause_resume(info, record_info);
			}
		} else {
			/* Changed play buffer size */
			info->audio_info.play.buffer_size = user->buffer_size;
			if (info->flags & W_BUSY)
				do_start++;
		}
	}

	if (user->gain != -1 && user->gain != dev->gain) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: gain=0x%x", user->gain));
		dev->gain = user->gain;
		if (set_device)
			if (record_info)
				do_record_volume++;
			else
				do_play_volume++;
	}

	if (user->balance != (unsigned char)-1 &&
	    user->balance != dev->balance) {
		/* note balance, and set volume */
		dev->balance = user->balance;
		if (set_device)
			if (record_info)
				do_record_volume++;
			else
				do_play_volume++;
	}

	if (user->port != -1) {			/* input source/output port */
		SBERRPRINT(SBEP_L0, SBEM_SPRI,
		    (CE_NOTE, "sbpro_setprinfo: port=0x%x", user->port));

		dev->port = user->port;
		if (set_device)
			if (record_info) {
				if (info->flags & SBPRO)
					sbpro_filters(info,
					    !info->audio_info.play.open);
				do_record_volume++;
			}
	}

	if (user->samples != -1) {		/* number of samples */
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro: set %s samples from %d to %d",
		    record_info ? "REC" : "play", dev->samples, user->samples));
		dev->samples = user->samples;
	}

	if (user->eof != -1) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro: set %s eof from %d to %d",
		    record_info ? "REC" : "play", dev->eof, user->eof));
		dev->eof = user->eof;
	}

	if (user->error != (unsigned char)-1) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI,
		    (CE_NOTE, "sbpro_setprinfo: error=0x%x", user->error));
		dev->error = user->error;
	}

	if (user->waiting != (unsigned char)-1) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI,
		    (CE_NOTE, "sbpro_setprinfo: waiting=0x%x", user->waiting));
		if (user->waiting != 0)		/* don't allow setting to 0 */
			dev->waiting = user->waiting;
	}

	if (user->pause != (unsigned char)-1) {
	    if (set_device) {
		SBERRPRINT(SBEP_L0, SBEM_SPRI, (CE_NOTE,
		    "sbpro_setprinfo: pause=0x%x dev=%d",
		    user->pause, dev->pause));

		if (user->pause) { /* stop the audio now */
			if (dev->pause == 0) /* if not already paused */
				sbpro_pause(info, record_info);
		} else
			if (dev->pause != 0)
				sbpro_pause_resume(info, record_info);
	    }
	    dev->pause = user->pause;
	}

	if (do_start)
		if (info->flags & (R_BUSY | W_BUSY)) {
			(void) dsp_reset(info->ioaddr, info);
			if (ddi_dmae_stop(info->dip, info->dmachan) !=
			    DDI_SUCCESS)
				cmn_err(CE_WARN, "sbpro setprinfo: "
				    "dma stop failed");
			if (((info->audio_info.play.open) &&
			    (!info->audio_info.play.pause)) ||
			    ((info->audio_info.record.open) &&
			    (!info->audio_info.record.pause)))
				sbpro_start(info, (info->flags&WRITING) ?
				    FWRITE:FREAD);
		}

	if (do_play_volume)
		set_volume(info, info->audio_info.play.gain,
		    info->audio_info.play.balance, SET_MASTER);

	if (do_record_volume)
		set_volume(info, info->audio_info.record.gain,
		    info->audio_info.record.balance,
		    info->audio_info.record.port);
}

static void
sbpro_passthrough(SBInfo *info)
{
	/* set up when both play and record are closed, but control is open */
	set_volume(info, info->audio_info.record.gain,
	    info->audio_info.record.balance, info->audio_info.record.port);
	set_volume(info, info->audio_info.monitor_gain,
	    AUDIO_MID_BALANCE, SET_MASTER);
	if (info->flags & AD184x) {
		int i;
		outb(info->ioaddr+AD184x_INDEX, FORMAT_REG);
		i = inb(info->ioaddr+AD184x_DATA);
		/* set stereo mode */
		setreg(info, FORMAT_REG | AD184x_MODE_CHANGE, i | 0x10);
	}
}

/* ************************************************************************ */
/*			SBPRO Engine - DSP Support Routines		    */
/* ************************************************************************ */
/*
 * dsp_cardreset -- reset the card and the modes to standard state
 */
static void
dsp_cardreset(SBInfo *info)
{
	SBERRPRINT(SBEP_L1, SBEM_REST, (CE_NOTE, "sbpro: dsp_cardreset"));
	(void) dsp_reset(info->ioaddr, info);


	/* Restore Mixer State */
	if (info->flags & AD184x) {
		set_volume(info, 0, 0, SET_MASTER);
		set_volume(info, 0, 0, AUDIO_LINE_IN);
		setreg(info, MON_LOOP_REG, 0x1);	/* monitor enable */
		setreg(info, 0x02, 0x9f);		/* mute aux loop */
		setreg(info, 0x03, 0x9f);		/* mute aux loop */
		setreg(info, 0x04, 0x9f);		/* mute aux loop */
		setreg(info, 0x05, 0x9f);		/* mute aux loop */
	} else if (info->flags & SBPRO) {
		/* turn volume off while closed */
		setmixer(info, MIXER_MASTER, 0x0);
		setmixer(info, MIXER_FM, 0);		/* FM input level */
	} else if (info->flags & SB16) {
		/*
		 * set the required input and output ports masks to
		 * 'uninitialised' to ensure that the ports are written
		 * during open
		 */
		info->right_input_switch = 0;
		info->left_input_switch = 0;
		info->output_switch = 0;

		/* turn volume off while closed */
#ifdef BUG_1259621
		/*
		 * On the SB16, we can and should avoid turning off the
		 * master volume.  We originally had this in here to
		 * prevent screechy feedback on some notebooks where the
		 * mic and speaker were fixed too close to each other.
		 * We can avoid it, though, because the output switches
		 * are all off, MIDI input is off, and voice is of course
		 * off when we are closed.  Mic is controlled through the
		 * output switches.  We should leave master volume and
		 * pc_speaker on, because some machines now wire the PC
		 * speaker through the SB16, and if we turn off the master
		 * volume the user can no longer hear "the beep" once he
		 * has closed the audio device.
		 */
		setmixer(info, MIXER_16_LEFT_MASTER, 0x0);
		setmixer(info, MIXER_16_RIGHT_MASTER, 0x0);
#endif
		setmixer(info, MIXER_16_INPUT_LEFT, 0x0);
		setmixer(info, MIXER_16_INPUT_RIGHT, 0x0);
		setmixer(info, MIXER_16_OUTPUT, 0x0);

		/* MIDI not used, so switch off */
		setmixer(info, MIXER_16_LEFT_FM, 0x0);	    /* FM input level */
		setmixer(info, MIXER_16_RIGHT_FM, 0x0); /* FM input level */

		/*
		 * The Gain value being used is the 2nd level of gain control on
		 * the SB16. These values are none, 1st, 2nd, 3rd.
		 */
		setmixer(info, MIXER_16_INPUT_LEFT_GAIN, info->inputleftgain);
		setmixer(info, MIXER_16_INPUT_RIGHT_GAIN, info->inputrightgain);
		setmixer(info, MIXER_16_OUTPUT_LEFT_GAIN, info->outputleftgain);
		setmixer(info, MIXER_16_OUTPUT_RIGHT_GAIN,
		    info->outputrightgain);

		setmixer(info, MIXER_16_LEFT_BASS, info->bassleft);
		setmixer(info, MIXER_16_RIGHT_BASS, info->bassright);
		setmixer(info, MIXER_16_LEFT_TREBLE, info->trebleleft);
		setmixer(info, MIXER_16_RIGHT_TREBLE, info->trebleright);
		setmixer(info, MIXER_16_AGC, info->agc);
		setmixer(info, MIXER_16_PCSPEAKER, info->pcspeaker);
	}
}

/*
 *	Reset the DSP chip.  This will cause the chip to initialize itself,
 *	then become idle with the value 0xAA in the read data port.
 */
static int
dsp_reset(u_short ioaddr, SBInfo *info)
{
	u_char	data = 0x00;
	u_char	store_reg = 0x00;
	int	cnt;

	SBERRPRINT(SBEP_L1, SBEM_REST, (CE_NOTE, "sbpro: dsp_reset"));

	if (info != (SBInfo *)NULL) {
		info->flags &= ~(PAUSED_DMA | PAUSED_DMA8 | PAUSED_DMA16 |
		    R_BUSY | W_BUSY | AUTO_DMA | ENDING | START_STEREO);
		if (info->flags & CLOSE_WAIT) {
			cv_signal(&info->cvclosewait);
			SBERRPRINT(SBEP_L1, SBEM_REST, (CE_NOTE,
			    "sbpro: CLOSE_WAIT while reseting"));
		}
		info->dsp_speed = 0;
	}

	/* Compaq Business Audio case */
	if (info != (SBInfo *)NULL && (info->flags & AD184x)) {
		setreg(info, CFG_REG, 0);	/* play/rec off */
		setreg(info, PIN_REG, 0);	/* intr off */
		outb(ioaddr+AD184x_STATUS, 0);	/* clear int */
		return (0);
	}

	/* SoundBlaster SBPRO and SB16 case */

	/*
	 * When we're called from the probe routine, we don't yet know for
	 * sure whether there's an SB card at this address.  So we save the
	 * value we read from this address and restore it later if we decide
	 * there is no SB card here.  This isn't totally guaranteed to work,
	 * but it fixes the case where this routine was trashing the smc
	 * card's interrupt enable bit.
	 */
	store_reg  = inb(ioaddr + DSP_RESET);

	outb(ioaddr + DSP_RESET, RESET_CMD);	/* set the "reset" bit */
	drv_usecwait(delay_factor);		/* need at least 5 usec delay */
	outb(ioaddr + DSP_RESET, 0x00); /* clear the "reset" bit */
	drv_usecwait(delay_factor);		/* delay a while */

	/*
	 * now poll for READY in the read data port, checking the status
	 * port for DATA AVAILABLE before doing so.
	 * (Note that the DSP is expected to take 100 usec to initialize)
	 */
	for (cnt = 0; cnt < 100; cnt++) {	/* try a few times */
		if (inb(ioaddr + DSP_DATAAVAIL) & DATA_READY) {
			drv_usecwait(delay_factor);	/* delay a while */
			data = inb(ioaddr + DSP_RDDATA);
			if (data == READY) {	/* we got it */
				return (0);
			}
		}
		drv_usecwait(delay_factor);	/* delay a while */
	}

	outb(ioaddr + DSP_RESET, store_reg);	/* not an SB, restore reg */
	SBERRPRINT(SBEP_L1, SBEM_REST, (CE_NOTE,
	    "sbpro: DSP reset failed for addr 0x%x", ioaddr));
	return (-1);
}

/*
 * dsp_dmahalt -- halt DMA
 *		  check for the precisision and use the appropriate halt cmd
 *		  this routine and dsp_dmacont() assume that the device can
 *		  be opened for reading or writing but not both at the same
 *		  time.	 If that becomes true we have to take care of those
 *		  cases as well
 */
static void
dsp_dmahalt(SBInfo *info)
{
	int count, index, played;
	int old_flags;

	mblk_t *bp;

	SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_NOTE,
	    "sbpro: dsp_dmahalt: info=0x%x", (int)info));

	ASSERT(info->flags & WRITING);

	if (info->dsp_fastmode) {
		old_flags = info->flags;
		(void) dsp_reset(info->ioaddr, info);
		info->dsp_fastmode = 0;
		if (ddi_dmae_stop(info->dip, info->dmachan) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro dsp_dmahalt: "
			    "dma stop failed");
		if (ddi_dmae_getcnt(info->dip, info->dmachan, &count) !=
		    DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro pause: dsp_dma_halt: "
			    "dma getcnt failed");
		count &= ~1;


		/* handle case where doing a pause in fast mode single step */
		if (!(old_flags & AUTO_DMA)) {
			index = (2*info->buflen) - count;
			/*
			 * Is this a single step started by sbpro_start? If
			 * so, then copy data from both buffers, otherwise
			 * only copy data from the current buffer
			 */
			if (!info->length[1]) {
				if (index >= info->length[0]) {
					played = info->length[0];
					info->audio_info.play.eof +=
							info->eofbuf[0];
					goto single_out;
				}
				played = index;
				bp = allocb(info->length[0]-index, BPRI_MED);
				if (!bp) {
					SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
					    "sbpro_intr: out of stream bufs"));
					/* Pretend played */
					played = info->length[0];
					info->audio_info.play.eof +=
							info->eofbuf[0];
					goto single_out;
				}
				bcopy((caddr_t)&info->buffers[0][index],
				    (caddr_t)bp->b_wptr, info->length[0]-index);
				bp->b_wptr += info->length[0]-index;
				info->paused_eof = info->eofbuf[0];
			} else {
				int end_position;

				if (info->length[0] != info->buflen) {
					played = index;
					end_position = info->length[0];
				} else {
					played = index-info->buflen;
					end_position=
						info->buflen+info->length[1];
				}

				if (index >= end_position) {
					played = info->length[info->bufnum];
					info->audio_info.play.eof +=
						info->eofbuf[info->bufnum];
					goto single_out;
				}

				bp = allocb(end_position-index, BPRI_MED);
				if (!bp) {
					SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
					    "sbpro_intr: out of stream bufs"));
					info->audio_info.record.error = 1;
					goto single_out;
				}
				bcopy((caddr_t)&info->buffers[0][index],
				    (caddr_t)bp->b_wptr, end_position-index);
				bp->b_wptr += end_position - index;
				info->paused_eof = info->eofbuf[info->bufnum];
			}

			info->paused_buffer = bp;
single_out:
			if (info->dsp_stereomode)
				played /= 2;
			info->audio_info.play.samples += played;
			if (info->ctlrdq)
				sbpro_sendsig(info->ctlrdq, SIGPOLL);
			return;
		}

		if (info->bufnum == 1) {
			if (count > info->buflen) {
				index = (2*info->buflen)-count;
				bp = allocb(info->buflen-index, BPRI_MED);
				if (!bp) {
					SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
					    "sbpro_intr: out of stream bufs"));
					info->audio_info.play.eof +=
					    info->eofbuf[0] + info->eofbuf[1];
					played = 2*info->buflen;
					goto auto_out;
				}
				bcopy((caddr_t)&info->buffers[0][index],
				    (caddr_t)bp->b_wptr, info->buflen-index);
				bp->b_wptr += info->buflen-index;
				info->paused_eof = info->eofbuf[0];
				info->audio_info.play.eof += info->eofbuf[1];
				played = info->buflen+index;
			} else {
				bp = allocb(count+info->buflen, BPRI_MED);
				if (!bp) {
					SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
					    "sbpro_intr: out of stream bufs"));
					info->audio_info.play.eof +=
					    info->eofbuf[0] + info->eofbuf[1];
					goto auto_out;
				}
				bcopy((caddr_t)&info->buffers[1]
					[info->buflen-count],
					(caddr_t)bp->b_wptr, count);
				bp->b_wptr += count;
				bcopy((caddr_t)info->buffers[0],
				    (caddr_t)bp->b_wptr, info->buflen);
				bp->b_wptr += info->buflen;
				info->paused_eof =
				    info->eofbuf[0] + info->eofbuf[1];
				played = info->buflen-count;
			}
		} else {
			bp = allocb(count, BPRI_MED);
			if (!bp) {
				played = 2*info->buflen;
				info->audio_info.play.eof +=
					info->eofbuf[0] + info->eofbuf[1];
				goto auto_out;
			}
			played = 2*info->buflen - count;
			bcopy((caddr_t)&info->buffers[0][played],
				(caddr_t)bp->b_wptr, count);
			bp->b_wptr += count;
			info->paused_eof = info->eofbuf[1];
			if (count > info->buflen)
				info->paused_eof = info->eofbuf[0];
			else
				info->audio_info.play.eof +=
					info->eofbuf[0];
		}

		info->paused_buffer = bp;
auto_out:
		if (info->dsp_stereomode)
			played /= 2;
		info->audio_info.play.samples += played;

		if (info->ctlrdq)
			sbpro_sendsig(info->ctlrdq, SIGPOLL);
	} else {
		if (info->flags & AD184x) {
			setreg(info, CFG_REG, 0); /* play/rec off */
			info->flags |= PAUSED_DMA;
		} else if (info->audio_info.play.precision == 16) {
			SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_NOTE,
			    "sbpro: 16bit"));
			dsp_command(info->ioaddr, HALT_DMA16);
			info->flags |= PAUSED_DMA | PAUSED_DMA16;
		} else {
			SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_NOTE, "sbpro: 8bit"));
			dsp_command(info->ioaddr, HALT_DMA);
			info->flags |= PAUSED_DMA | PAUSED_DMA8;
		}

		if (ddi_dmae_getcnt(info->dip, info->dmachan, &count) !=
		    DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro pause: dsp_dma_halt: "
			    "dma getcnt failed");

		/*
		 * This code should be ok because the dma is paused, not
		 * killed, and the interrupt will always be received
		 */
		if (count <= info->buflen)
			played = info->buflen-count;
		else
			played = 2*info->buflen - count;

		played = played / (info->dspbits / 8);
		if (info->dsp_stereomode)
			played /= 2;

		if (count <= info->buflen)
			info->sampbuf[1] -= played;
		else
			info->sampbuf[0] -= played;
		info->audio_info.play.samples += played;
		if (info->ctlrdq)
			sbpro_sendsig(info->ctlrdq, SIGPOLL);
	}
	return;

}

/*
 *	Send a command to the DSP, waiting for it to become non-busy first.
 */
static void
dsp_command(u_short ioaddr, int cmd)
{
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	u_char	byte;
	int	cnt = 100;

	SBERRPRINT(SBEP_L0, SBEM_DCMD,
	    (CE_NOTE, "sbpro: dsp_command: 0x%x --> 0x%x", ioaddr, cmd));
	byte = (u_char)cmd;

	/*
	 *	Check that the "Write Status" port shows the DSP is ready
	 *	to receive the command (or data) before we send it.
	 */
	while ((inb(ioaddr + DSP_WRSTATUS) & WR_BUSY) && --cnt > 0)
		drv_usecwait(delay_factor);

	if (cnt <= 0) {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L3, SBEM_DCMD, (CE_WARN,
		    "sbpro: dsp_command(%x): Sound Blaster is hung.", byte));
	}

	/* now send the command over */
	outb(ioaddr + DSP_WRDATA_CMD, cmd);
	/* 25us insufficient for ESS, OK for others */
	drv_usecwait(8*delay_factor);

	/*
	 * The ESS chip has been seen to take up to 25 milliseconds after a
	 * SPEAKER_OFF command.  This is terrible.  We know that we never call
	 * this routine with SPEAKER_OFF from an interrupt routine, so we go
	 * ahead and wait here.  We don't postpone the delay until we need to
	 * send the next command, because if we did we might suffer that delay
	 * from a call to dsp_reset that came from the interrupt routine, and
	 * we don't want to hang around that long from an interrupt routine.
	 *
	 * Non ESS implementations will exit this loop substantially earlier.
	 */
	if (cmd == SPEAKER_OFF) {
		cnt = 3000;	/* up to 30 ms */
		while ((inb(ioaddr + DSP_WRSTATUS) & WR_BUSY) && --cnt > 0)
			drv_usecwait(delay_factor);
		if (cnt <= 0) {
			/*EMPTY*/ /* in lint case */
			SBERRPRINT(SBEP_L3, SBEM_DCMD, (CE_WARN,
			    "sbpro: dsp_command(SPEAKER_OFF):  not ready."));
		}
	}
}

/*
 *	Set the DSP chip's sampling rate by programming the "time constant".
 */
static void
dsp_speed(SBInfo *info, int rate)
{
	u_char	tc;		/* time constant value */

	SBERRPRINT(SBEP_L1, SBEM_DDAT, (CE_NOTE,
	    "sbpro: dsp_speed(%x):", rate));

	ASSERT(info->flags & (SBPRO | SB16));

	info->dsp_speed = rate;		/* so we know how the card is set */

	if (info->flags & SB16) {
		/*
		 * XXX We should only set the rate for the side
		 * (play or record) in use
		 */
		dsp_command(info->ioaddr, SET_SRATE_INPUT);
		dsp_command(info->ioaddr, (rate >> 8) & 0xff);
		dsp_command(info->ioaddr, rate&0xff);
		dsp_command(info->ioaddr, SET_SRATE_OUTPUT);
		dsp_command(info->ioaddr, (rate >> 8) & 0xff);
		dsp_command(info->ioaddr, rate&0xff);
		return;
	}

	if (rate >= 23000 || (info->dsp_stereomode && (rate >= 11025)))
		info->dsp_fastmode = 1;

	if (info->dsp_stereomode)
		rate *= 2; /* stereo mode requires doubling the rate */

	tc = ((65536 - (256000000L / rate)) >>8) & 0xff;

	dsp_command(info->ioaddr, SET_CONSTANT);	/* send the command */
	dsp_command(info->ioaddr, tc);			/* and send the data */
}

static u_char
dsp_getdata(u_short ioaddr)
{
	u_char data;
	int cnt;

	for (cnt = 0; cnt < 10; cnt++) {	/* try a few times */
		if (inb(ioaddr + DSP_DATAAVAIL) & DATA_READY) {
			drv_usecwait(delay_factor);	/* delay a while */
			data = inb(ioaddr + DSP_RDDATA);
			return (data);
		}
		drv_usecwait(delay_factor);	/* delay a while */
	}

	SBERRPRINT(SBEP_L3, SBEM_DDAT, (CE_WARN,
	    "sbpro: dsp_getdata: SB card not ready for read"));

	return (0);
}

/*
 * dsp_dmacont -- resume DMA operation
 *		  resume the last dma operation if we have the same prescision
 *		  otherwise turn the busy flag off and call sbpro_start()
 */
static void
dsp_dmacont(SBInfo *info)
{
	int precision;

	ASSERT(info->flags&WRITING);

	if (!(info->flags & PAUSED_DMA)) {
		SBERRPRINT(SBEP_L3, SBEM_DMA, (CE_WARN,
		    "sbpro: dsp_dmacont: DMA not paused!"));
		return;
	}

	SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_NOTE,
	    "sbpro: dsp_dmacont: info=0x%x", (int)info));

	if (info->flags & AD184x) {
		setreg(info, CFG_REG, 0x01); /* play on */
		info->flags &= ~PAUSED_DMA;
		return;
	}

	/* SBPRO or SB16 */
	if (info->audio_info.play.precision == 16)
		precision = 16;
	else
		precision = 8;

	if (info->flags & PAUSED_DMA8) {
		if (precision == 8) {
			SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_NOTE,
			    "sbpro: 8 bit"));
			info->flags &= ~(PAUSED_DMA|PAUSED_DMA8);
			dsp_command(info->ioaddr, CONTINUE_DMA);
			return;
		} else {
			/*EMPTY*/ /* in lint case */
			SBERRPRINT(SBEP_L5, SBEM_DMA, (CE_WARN,
				"sbpro: 8->16 bit"));
		}
	} else {
		if (precision == 16) {
			SBERRPRINT(SBEP_L0, SBEM_DMA, (CE_WARN,
			    "sbpro:16 bit"));
			info->flags &= ~(PAUSED_DMA|PAUSED_DMA16);
			dsp_command(info->ioaddr, CONTINUE_DMA16);
			return;
		} else {
			/*EMPTY*/ /* in lint case */
			SBERRPRINT(SBEP_L5, SBEM_DMA, (CE_NOTE,
			    "sbpro:16->8bit"));
		}
	}
}


/*
 *	Send a signal to the processes on this stream by sending an "M_PCSIG"
 *	message upstream.
 */
static void
sbpro_sendsig(queue_t *q, int signo)
{
	mblk_t		*mp;

#ifdef DEBUG
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	SBInfo	*info;
	info = (SBInfo *)q->q_ptr;
	SBERRPRINT(SBEP_L1, SBEM_DDAT, (CE_NOTE,
	    "sbpro: SendSig: output_muted=%d cnt=%d, eof=%d error=%d",
	    info->audio_info.output_muted, info->audio_info.play.samples,
	    info->audio_info.play.eof, info->audio_info.play.error));
#endif

	/* Allocate a message large enough to hold the signal number */
	if ((mp = allocb(sizeof (u_char), BPRI_HI)) == NULL)
		return;

	mp->b_datap->db_type = M_PCSIG;
	*mp->b_wptr++ = (u_char)signo;

	/* and pass the message up the stream */
	putnext(q, mp);
}

static void
dsp_writedata(SBInfo *info, register u_char *dmaptr, register u_char *ptr,
    register int cnt)
{
	register u_char *translate = ulaw_to_raw;

	SBERRPRINT(SBEP_L1, SBEM_DWDA, (CE_NOTE, "sbpro: dsp_writedata cnt=%d",
					    cnt));

	if (info->flags & AD184x) {
		bcopy((caddr_t)ptr, (caddr_t)dmaptr, (long)cnt);
		return;
	}

	/*
	 *	SBPRO or SB16:
	 *	If we're not doing U-law conversion, a block copy works.
	 */
	if (info->audio_info.play.encoding == AUDIO_ENCODING_LINEAR) {
		/* we can do a simple block copy */
		bcopy((caddr_t)ptr, (caddr_t)dmaptr, (long)cnt);
		return;
	}

	/*
	 *	We need to do U-law to raw conversion.
	 *	We use this method to get as much speed as we can muster,
	 *	otherwise the time taken for the conversion becomes audible!
	 */
	while (cnt > 16) {
		dmaptr[0]  = translate[ptr[0]];
		dmaptr[1]  = translate[ptr[1]];
		dmaptr[2]  = translate[ptr[2]];
		dmaptr[3]  = translate[ptr[3]];
		dmaptr[4]  = translate[ptr[4]];
		dmaptr[5]  = translate[ptr[5]];
		dmaptr[6]  = translate[ptr[6]];
		dmaptr[7]  = translate[ptr[7]];
		dmaptr[8]  = translate[ptr[8]];
		dmaptr[9]  = translate[ptr[9]];
		dmaptr[10] = translate[ptr[10]];
		dmaptr[11] = translate[ptr[11]];
		dmaptr[12] = translate[ptr[12]];
		dmaptr[13] = translate[ptr[13]];
		dmaptr[14] = translate[ptr[14]];
		dmaptr[15] = translate[ptr[15]];
		dmaptr += 16;
		ptr += 16;
		cnt -= 16;
	}
	switch (cnt) {
	case 16:
		dmaptr[15] = translate[ptr[15]];
		/*FALLTHROUGH*/
	case 15:
		dmaptr[14] = translate[ptr[14]];
		/*FALLTHROUGH*/
	case 14:
		dmaptr[13] = translate[ptr[13]];
		/*FALLTHROUGH*/
	case 13:
		dmaptr[12] = translate[ptr[12]];
		/*FALLTHROUGH*/
	case 12:
		dmaptr[11] = translate[ptr[11]];
		/*FALLTHROUGH*/
	case 11:
		dmaptr[10] = translate[ptr[10]];
		/*FALLTHROUGH*/
	case 10:
		dmaptr[9]  = translate[ptr[9]];
		/*FALLTHROUGH*/
	case 9:
		dmaptr[8]  = translate[ptr[8]];
		/*FALLTHROUGH*/
	case 8:
		dmaptr[7]  = translate[ptr[7]];
		/*FALLTHROUGH*/
	case 7:
		dmaptr[6]  = translate[ptr[6]];
		/*FALLTHROUGH*/
	case 6:
		dmaptr[5]  = translate[ptr[5]];
		/*FALLTHROUGH*/
	case 5:
		dmaptr[4]  = translate[ptr[4]];
		/*FALLTHROUGH*/
	case 4:
		dmaptr[3]  = translate[ptr[3]];
		/*FALLTHROUGH*/
	case 3:
		dmaptr[2]  = translate[ptr[2]];
		/*FALLTHROUGH*/
	case 2:
		dmaptr[1]  = translate[ptr[1]];
		/*FALLTHROUGH*/
	case 1:
		dmaptr[0]  = translate[ptr[0]];
		break;
	}
}

static void
dsp_readdata(SBInfo *info, register u_char *dmaptr, register u_char *ptr,
    register int cnt)
{
	register u_char *translate = raw_to_ulaw;

	SBERRPRINT(SBEP_L1, SBEM_DRDA, (CE_NOTE, "sbpro: dsp_readdata"));

	if (info->flags & AD184x) {
		bcopy((caddr_t)dmaptr, (caddr_t)ptr, (long)cnt);
		return;
	}

	/*
	 *	SBPRO or SB16:
	 *	If we're not doing U-law conversion, a block copy works.
	 */
	if (info->audio_info.record.encoding == AUDIO_ENCODING_LINEAR) {
		/* we can do a simple block copy */
		bcopy((caddr_t)dmaptr, (caddr_t)ptr, (long)cnt);
		return;
	}

	/*
	 *	We need to do raw to U-law conversion.
	 *	We use this method to get as much speed as we can muster,
	 *	otherwise the time taken for the conversion becomes audible!
	 */
	while (cnt > 16) {
		ptr[0]	= translate[dmaptr[0]];
		ptr[1]	= translate[dmaptr[1]];
		ptr[2]	= translate[dmaptr[2]];
		ptr[3]	= translate[dmaptr[3]];
		ptr[4]	= translate[dmaptr[4]];
		ptr[5]	= translate[dmaptr[5]];
		ptr[6]	= translate[dmaptr[6]];
		ptr[7]	= translate[dmaptr[7]];
		ptr[8]	= translate[dmaptr[8]];
		ptr[9]	= translate[dmaptr[9]];
		ptr[10] = translate[dmaptr[10]];
		ptr[11] = translate[dmaptr[11]];
		ptr[12] = translate[dmaptr[12]];
		ptr[13] = translate[dmaptr[13]];
		ptr[14] = translate[dmaptr[14]];
		ptr[15] = translate[dmaptr[15]];
		ptr += 16;
		dmaptr += 16;
		cnt -= 16;
	}
	switch (cnt) {
	case 16:
		ptr[15] = translate[dmaptr[15]];
		/*FALLTHROUGH*/
	case 15:
		ptr[14] = translate[dmaptr[14]];
		/*FALLTHROUGH*/
	case 14:
		ptr[13] = translate[dmaptr[13]];
		/*FALLTHROUGH*/
	case 13:
		ptr[12] = translate[dmaptr[12]];
		/*FALLTHROUGH*/
	case 12:
		ptr[11] = translate[dmaptr[11]];
		/*FALLTHROUGH*/
	case 11:
		ptr[10] = translate[dmaptr[10]];
		/*FALLTHROUGH*/
	case 10:
		ptr[9]  = translate[dmaptr[9]];
		/*FALLTHROUGH*/
	case 9:
		ptr[8]  = translate[dmaptr[8]];
		/*FALLTHROUGH*/
	case 8:
		ptr[7]  = translate[dmaptr[7]];
		/*FALLTHROUGH*/
	case 7:
		ptr[6]  = translate[dmaptr[6]];
		/*FALLTHROUGH*/
	case 6:
		ptr[5]  = translate[dmaptr[5]];
		/*FALLTHROUGH*/
	case 5:
		ptr[4]  = translate[dmaptr[4]];
		/*FALLTHROUGH*/
	case 4:
		ptr[3]  = translate[dmaptr[3]];
		/*FALLTHROUGH*/
	case 3:
		ptr[2]  = translate[dmaptr[2]];
		/*FALLTHROUGH*/
	case 2:
		ptr[1]  = translate[dmaptr[1]];
		/*FALLTHROUGH*/
	case 1:
		ptr[0]  = translate[dmaptr[0]];
		break;
	}
}

/*
 * dsp_progdma -- program the boards DMA
 */
static void
dsp_progdma(SBInfo *info, int reading, int len)
{
	int startcmd = 0, speed, channels;
	int encoding;

	if (reading) {
		speed = info->audio_info.record.sample_rate;
		channels = info->audio_info.record.channels;
		encoding = info->audio_info.record.encoding;

		/* switch on left channel for recording on an SBPRO */
		if (info->flags & SBPRO) {
			if (channels == 2) {
				/*
				 * send the command to start stereo recording
				 * on the left channel
				 */
				dsp_command(info->ioaddr, START_LEFT);
			} else {
				/* record in mono */
				dsp_command(info->ioaddr, RECORD_MONO);
			}
		}
	} else {
		channels = info->audio_info.play.channels;
		speed = info->audio_info.play.sample_rate;
		encoding = info->audio_info.play.encoding;
	}

	/* are we in stereo mode */
	info->dsp_stereomode = (channels == 2) ? 1 : 0;

	if (info->flags & AD184x) {
		int i;
		static struct cba_speed {
			int	speed;
			int	code;
		} cba_speeds[] = {
			{  5512, 0x01 },
			{  6615, 0x0f },
			{  8000, 0x00 },
			{  9600, 0x0e },
			{ 11025, 0x03 },
			{ 16000, 0x02 },
			{ 18900, 0x05 },
			{ 22050, 0x07 },
			{ 27429, 0x04 },
			{ 32000, 0x06 },
			{ 33075, 0x0d },
			{ 37800, 0x09 },
			{ 44100, 0x0b },
			{ 48000, 0x0c }
		};

		for (i = 1;
		    i < sizeof (cba_speeds) / sizeof (struct cba_speed) - 1;
		    i++)
			if (speed < cba_speeds[i].speed)
				break;
		if (cba_speeds[i].speed - speed > speed - cba_speeds[i-1].speed)
			i--;

		info->dsp_speed = cba_speeds[i].speed;
		i = cba_speeds[i].code;

		if (info->dsp_stereomode)
			i |= 0x10;

		switch (encoding) {
		case AUDIO_ENCODING_LINEAR:
			if (info->dspbits == 16) {
				i |= 0x40;
				info->dsp_silent = 0;
			} else
				info->dsp_silent = 0x80;
			break;

		case AUDIO_ENCODING_ULAW:
			i |= 0x20;
			info->dsp_silent = 0xff;
			break;

		case AUDIO_ENCODING_ALAW:
			i |= 0x60;
			info->dsp_silent = 0xd5;
			break;
		}

		setreg(info, FORMAT_REG | AD184x_MODE_CHANGE, i);

		info->count = len;
		len = len / (info->dspbits / 8);
		if (info->dsp_stereomode)
			len = len / 2;
		info->sampcount = len;

		len -= 1;			/* 0 means transfer is 1 byte */
		setreg(info, COUNT_LOW_REG, len & 0xff);
		setreg(info, COUNT_HIGH_REG, len >> 8);
		setreg(info, PIN_REG, 0x02);
		setreg(info, CFG_REG, reading ? 0x02 : 0x01);

		/* we're running now ... */
		info->flags |= (reading ? R_BUSY : W_BUSY);
		return;
	}

	/*
	 * if we are using an SBPRO card, then make sure speaker is on for
	 * playing and OFF for recording. Recording on an SBPRO won't work
	 * if the speaker is ON.
	 */
	if (info->flags & SBPRO) {
		if (info->audio_info.play.open)
			/* turn the speaker on */
			dsp_command(info->ioaddr, SPEAKER_ON);
		else	/* make sure it's off for recording */
			dsp_command(info->ioaddr, SPEAKER_OFF);
	}

	/* set speed back up if it has changed on the DSP */
	if (info->dsp_speed != speed)
		dsp_speed(info, speed);

	if (info->dspbits == 16)
		info->dsp_silent = 0;
	else
		info->dsp_silent = 0x80; /* SB H/W is always linear */

	if (reading) {
		if (info->flags & SB16) {
			if (info->dspbits == 8) {
				if (info->flags & AUTO_DMA)
					dsp_command(info->ioaddr,
					    SB_8_AI_INPUT);
				else
					dsp_command(info->ioaddr,
					    SB_8_SC_INPUT);
				if (info->dsp_stereomode)
					dsp_command(info->ioaddr, SB_8_ST);
				else
					dsp_command(info->ioaddr, SB_8_MONO);
			} else {
				if (info->flags & AUTO_DMA)
					dsp_command(info->ioaddr,
					    SB_16_AI_INPUT);
				else
					dsp_command(info->ioaddr,
					    SB_16_SC_INPUT);
				if (info->dsp_stereomode)
					dsp_command(info->ioaddr, SB_16_ST);
				else
					dsp_command(info->ioaddr, SB_16_MONO);
			}
		} else {
			if (info->flags & AUTO_DMA) {
				dsp_command(info->ioaddr, SET_BLOCK_SIZE);
				if (info->dsp_fastmode)
					startcmd = ADC_DMA_FAST_AI;
				else
					startcmd = ADC_DMA_AI;
			} else if (info->dsp_fastmode) {
				dsp_command(info->ioaddr, SET_BLOCK_SIZE);
				startcmd = ADC_DMA_FAST;
			} else
				dsp_command(info->ioaddr, ADC_DMA);
		}
	} else {
		SBERRPRINT(SBEP_L1, SBEM_DMA, (CE_NOTE,
		    "sbpro:  Programming DMA write..."));

		if (info->flags & SB16) {
			if (info->dspbits == 8) {
				if (info->flags & AUTO_DMA)
					dsp_command(info->ioaddr,
					    SB_8_AI_OUTPUT);
				else
					dsp_command(info->ioaddr,
					    SB_8_SC_OUTPUT);
				if (info->dsp_stereomode)
					dsp_command(info->ioaddr, SB_8_ST);
				else
					dsp_command(info->ioaddr, SB_8_MONO);
			} else {
				if (info->flags & AUTO_DMA)
					dsp_command(info->ioaddr,
					    SB_16_AI_OUTPUT);
				else
					dsp_command(info->ioaddr,
					    SB_16_SC_OUTPUT);
				if (info->dsp_stereomode)
					dsp_command(info->ioaddr, SB_16_ST);
				else
					dsp_command(info->ioaddr, SB_16_MONO);
			}
		} else {
			if (info->flags & AUTO_DMA) {
				dsp_command(info->ioaddr, SET_BLOCK_SIZE);
				if (info->dsp_fastmode)
					startcmd = DAC_DMA_FAST_AI;
				else
					startcmd = DAC_DMA_8_AI;
			} else if (info->dsp_fastmode) {
				dsp_command(info->ioaddr, SET_BLOCK_SIZE);
				startcmd = DAC_DMA_FAST;
			} else
				dsp_command(info->ioaddr, DAC_DMA_8);
		}
	}

	info->count = len;
	len = len / (info->dspbits / 8);

	if (info->dsp_stereomode)
		info->sampcount = len / 2;
	else
		info->sampcount = len;

	len -= 1;				/* 0 means transfer is 1 byte */
	dsp_command(info->ioaddr, len & 0xff);		/* low-order byte */
	dsp_command(info->ioaddr, (len >> 8) & 0xff);	/* high-order byte */

	if (startcmd)
		dsp_command(info->ioaddr, startcmd);

	/* we're running now ... */
	info->flags |= (reading ? R_BUSY : W_BUSY);	/* set the busy flag */
}

/*
 * To get the SBPRO to correctly enter stereo mode, we must use a single
 * cycle DMA to output an extra "silent" byte at the beginning of the
 * playback.  Otherwise it will play with left/right channels reversed.
 * See page 3-23 of the Sound Blaster Developer Kit, Hardware Programming
 * Reference, 2nd edition.
 */
static void
sbpro_fast_stereo_start(SBInfo *info)
{
	struct ddi_dmae_req	dmaereq;

	ASSERT(info->flags & SBPRO);

	bzero((caddr_t)&dmaereq, sizeof (dmaereq));
	dmaereq.der_command = (u_char)DMAE_CMD_WRITE;
	dmaereq.der_bufprocess = (u_char)DMAE_BUF_NOAUTO;
	if (ddi_dmae_prog(info->dip, &dmaereq, &info->sbpro_stereo_byte_cookie,
	    info->dmachan8) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro fast stereo start: "
		    "dmae prog failed");

	dsp_command(info->ioaddr, SPEAKER_ON);
	setmixer(info, MIXER_OUTPUT, VSTC|DNFI);
	dsp_command(info->ioaddr, DAC_DMA_8);
	dsp_command(info->ioaddr, 0);		 /* low-order byte (count-1) */
	dsp_command(info->ioaddr, 0);		 /* high-order byte */

	info->flags |= START_STEREO|W_BUSY;
}

/* ************************************************************************ */
/*			SBPRO Engine - Mixer Support Routines		    */
/* ************************************************************************ */
/*
 * Set the Mixer volume registers
 * This function assumes that the gain parameter is in the range 0-255
 * Set the gain based on the current balance
 */
static void
set_volume(SBInfo *info, uint_t volume, uchar_t balance, int port)
{
	int balval, right_speaker, left_speaker;
	u_char rightval, leftval, val;

	/* used to decide what ports to switch on */
	int left_input_switch = 0, right_input_switch = 0, output_switch = 0;

	/* determine balance difference from balance midpoint */
	balval = balance - AUDIO_MID_BALANCE;

	SBERRPRINT(SBEP_L1, SBEM_SPRI, (CE_NOTE,
	    "sbpro:set_volume vol=%x ,bal=%x ,port=%x", volume, balance, port));

	/*
	 * calculate left and right volumes. This will result
	 * in left and right values in the range 0-255.
	 */
	left_speaker = (balval <= 0) ? volume :
	    ((AUDIO_MID_BALANCE - balval)*volume) / AUDIO_MID_BALANCE;

	right_speaker = (balval >= 0) ? volume :
	    ((AUDIO_MID_BALANCE + balval)*volume) / AUDIO_MID_BALANCE;

	if (info->flags & CBA1847) {
		if (port & SET_MASTER) {
			/* Is output currently being muted? */
			if (info->audio_info.output_muted) {
				/* Don't use mute bit; it clicks loudly */
				setreg(info, LEFT_OUT_REG, 0x3f);
				setreg(info, RIGHT_OUT_REG, 0x3f);
			} else {
				/* Set left and right master volumes. */
				setreg(info, LEFT_OUT_REG,
				    (255-left_speaker)>>2);
				setreg(info, RIGHT_OUT_REG,
				    (255-right_speaker)>>2);
			}
			return;
		}

		if (port & AUDIO_MICROPHONE) {
			/* one mic 20dB gain bit, in left reg, controls L&R. */
			/* We never change it, to avoid loud clicks. */
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x80 |
			    (info->inputleftgain ? 0x20 : 0));
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x80 |
			    (info->inputrightgain ? 0x20 : 0));
			return;
		}

		if (port & AUDIO_CD) {
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x00 |
			    (info->inputleftgain ? 0x20 : 0));
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x00 |
			    (info->inputrightgain ? 0x20 : 0));
			return;
		}

		if (port & AUDIO_LINE_IN) {
			/* This input is called AUX1 on AD184x */
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x40 |
			    (info->inputleftgain ? 0x20 : 0));
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x40 |
			    (info->inputrightgain ? 0x20 : 0));
			return;
		}
	} else if (info->flags & AD184x) {
		if (port & SET_MASTER) {
			/* Is output currently being muted? */
			if (info->audio_info.output_muted) {
				setreg(info, LEFT_OUT_REG, 0xbf);
				setreg(info, RIGHT_OUT_REG, 0xbf);
			} else {
				/* Set left and right master volumes. */
				setreg(info, LEFT_OUT_REG,
				    ((255-left_speaker)>>2) |
				    (left_speaker ? 0 : 0x80));
				setreg(info, RIGHT_OUT_REG,
				    ((255-right_speaker)>>2) |
				    (right_speaker ? 0 : 0x80));
			}
			return;
		}

		if (port & AUDIO_MICROPHONE) {
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x80 |
			    (left_speaker && info->inputleftgain ? 0x20 : 0));
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x80 |
			    (right_speaker && info->inputrightgain ? 0x20 : 0));
		}

		if (port & AUDIO_LINE_IN) {
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x0);
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x0);
			return;
		}

		if (port & AUDIO_CD) {
			/* This input is called AUX1 on AD184x */
			setreg(info, LEFT_IN_REG, (left_speaker>>4) | 0x40);
			setreg(info, RIGHT_IN_REG, (right_speaker>>4) | 0x40);
			return;
		}
	} else if (info->flags & SB16) {
		/*
		 * Use the full SB16 capability registers, if SB16 card present
		 */
		/*
		 * The following code assumes that it is possible for the
		 * user to select more than one port. The audio(7) interface
		 * does not really allow this yet.
		 */

		/*
		 * Set the Master Mixer volume.
		 * The master mixer isn't a mixer port, it controls the
		 * overall output from the mixer. When setting the Master
		 * mixer volume, the record or play balance is used
		 * so I've included it here to keep all volume control in
		 * one place.
		 */
		if (port & SET_MASTER) {
			/* Is output currently being muted */
			if (info->audio_info.output_muted) {
				setmixer(info, MIXER_16_LEFT_MASTER, 0x0);
				setmixer(info, MIXER_16_RIGHT_MASTER, 0x0);
			} else {
				/* Set left and right master volumes. */
				setmixer(info, MIXER_16_LEFT_MASTER,
						left_speaker);
				setmixer(info, MIXER_16_RIGHT_MASTER,
						right_speaker);
			}

			/*
			 * Master volume is not a port, can't be switched on
			 * or off, so no further processing necessary
			 */
			return;
		}


		/*
		 * set microphone jack. The microphone on SB cards is a
		 * mono-port, so balance doesn't have any meaning here
		 */
		if (port & AUDIO_MICROPHONE)
			/*
			 * if the volume is zero, then switch off the
			 * microphone, otherwise set the volume
			 */
			if (volume) {
				setmixer(info, MIXER_16_MIC, volume);

				/* request that microphone is an active port */
				right_input_switch |= INPUT_16_MIC;
				left_input_switch |= INPUT_16_MIC;
				output_switch |= OUTPUT_16_MIC;
			}

		/*
		 * Is the line-in port required?
		 */
		if (port & AUDIO_LINE_IN) {
			/* After balance adj, is there left line volume? */
			if (left_speaker) {
				setmixer(info, MIXER_16_LEFT_LINE,
				    left_speaker);

				/* request left line on */
				left_input_switch |= INPUT_16_LEFT_LINE;
				output_switch |= OUTPUT_16_LEFT_LINE;
			}

			/* After balance adj, is there right line volume? */
			if (right_speaker) {
				setmixer(info, MIXER_16_RIGHT_LINE,
				    right_speaker);

				/* request right line is on */
				right_input_switch |= INPUT_16_RIGHT_LINE;
				output_switch |= OUTPUT_16_RIGHT_LINE;

				/*
				 * if we are in mono record mode, make sure that
				 * the right side is also being mixed into
				 * the left channel, (unless the balance
				 * is 100% left)
				 */
				if (info->audio_info.record.open &&
				    info->audio_info.record.channels == 1)
					left_input_switch |=
					    INPUT_16_RIGHT_LINE;
			}
		}

		/*
		 * Is the CD port required?
		 */
		if (port & AUDIO_CD) {
			/* the following conditions are as those for line-in */
			if (left_speaker) {
				setmixer(info, MIXER_16_LEFT_CD, left_speaker);
				left_input_switch |= INPUT_16_LEFT_CD;
				output_switch |= OUTPUT_16_LEFT_CD;
			}

			if (right_speaker) {
				setmixer(info, MIXER_16_RIGHT_CD,
				    right_speaker);
				right_input_switch |= INPUT_16_RIGHT_CD;
				output_switch |= OUTPUT_16_RIGHT_CD;

				/*
				 * if we are in mono record mode, make sure that
				 * the right side is also being mixed into
				 * the left channel
				 */
				if (info->audio_info.record.open &&
				    info->audio_info.record.channels == 1)
					left_input_switch |= INPUT_16_RIGHT_CD;
			}
		}


		/*
		 * Modify input and output port switches. If the switch
		 * already contains the setting, don't waste time resetting.
		 */
		if (right_input_switch != info->right_input_switch) {
			/* note the setting in the info structure */
			info->right_input_switch = right_input_switch;

			/* modify the right input switch */
			setmixer(info, MIXER_16_INPUT_RIGHT,
				info->right_input_switch);
		}

		/* does the left switch need to be changed ? */
		if (left_input_switch != info->left_input_switch) {
			info->left_input_switch = left_input_switch;
			setmixer(info, MIXER_16_INPUT_LEFT,
				info->left_input_switch);
		}

		/* does the output switch need to be changed? */
		if (output_switch != info->output_switch) {
			info->output_switch = output_switch;
			setmixer(info, MIXER_16_OUTPUT, info->output_switch);
		}

	} else { /* write into SBPRO registers */

		switch (port) {
		case SET_MASTER:
			/* calculate balance for SBPRO registers */
			/* scale right and left values to 4 bits */
			rightval = right_speaker >> 4;
			leftval = left_speaker >> 4;
			val = ((leftval & 0xF) << 4) | rightval; /* L & R */
			/* Is output currently being muted for Play ? */
			if (info->audio_info.output_muted)
				setmixer(info, MIXER_MASTER, 0x0);
			else
				setmixer(info, MIXER_MASTER, val);
			return;

		/* balance not applied to input on SBPRO */

		case AUDIO_LINE_IN:
			val = (volume & 0xF0) | (volume >> 4);
			setmixer(info, MIXER_LINE, val);
			setmixer(info, MIXER_CD, 0);
			setmixer(info, MIXER_MIC, 0);
			break;

		case AUDIO_CD:
			val = (volume & 0xF0) | (volume >> 4);
			setmixer(info, MIXER_MIC, 0);
			setmixer(info, MIXER_CD, val);
			setmixer(info, MIXER_LINE, 0);
			break;

		case AUDIO_MICROPHONE:
			val = (volume & 0xE0) >> 5;
			setmixer(info, MIXER_CD, 0);
			setmixer(info, MIXER_LINE, 0);
			setmixer(info, MIXER_MIC, val);
			break;
		}
	}
}

static void
sbpro_filters(SBInfo *info, int reading)
{
	int flag = 0;

	ASSERT(info->flags & SBPRO);

	/*
	 * Set the input mixer filter if recording
	 */
	if (reading) {
		/*
		 * which port is currently in use, on the SBPRO only one port
		 * can be selected at any one time
		 */
		switch (info->audio_info.record.port) {
		case AUDIO_LINE_IN:
			flag = LINE;
			break;
		case AUDIO_CD:
			flag = CD;
			break;
		case AUDIO_MICROPHONE:
			flag = MIC_2;
			break;
		default:
			SBERRPRINT(SBEP_L3, SBEM_SPRI, (CE_WARN,
			    "sbpro_filters:Port unknown"));
		}

		/* select the low pass filter if recording in mono mode.  */
		if (info->audio_info.record.channels == 2 ||
		    info->audio_info.record.sample_rate > TOP_MID_SAMPLE_RATE)
			/* recording stereo or fast mono; switch off filter */
			flag |= FLTR_NO;
		else { /* mono mode and low frequency */
			/*
			 * switch to the 8.8Khz filter if the sample rate is
			 * greater that 18kHz(sbpro manual recommendation)
			 * 3.2Khz filter is the default (zero bit)
			 */
			if (info->audio_info.record.sample_rate >
			    TOP_LOW_SAMPLE_RATE)
				flag |= FLTR_HI;
		}
		setmixer(info, MIXER_INPUT, flag);
	} else { /* playing, set output filter */
		if (info->audio_info.play.channels == 2)
			flag = VSTC;	/* STEREO */
		else
			flag = 0;

		/* Switch filter off if stereo mode or high sample rate */
		if ((info->audio_info.play.channels == 2) ||
		    (info->audio_info.play.sample_rate > TOP_LOW_SAMPLE_RATE))
			flag |= DNFI;	/* DO NOT FILTER */
		setmixer(info, MIXER_OUTPUT, flag);
	}
}

/*
 *	Set an SB mixer internal register to a particular value.
 */
static void
setmixer(SBInfo *info, int port, int value)
{
	ASSERT(info->flags & (SBPRO | SB16));
	SBERRPRINT(SBEP_L0, SBEM_MIXR,
	    (CE_NOTE, "sbpro: setmixer: reg(0x%x) --> 0x%x", port, value));
	outb(info->ioaddr + MIXER_ADDR, port);
	drv_usecwait(delay_factor);
	outb(info->ioaddr + MIXER_DATA, value);
	drv_usecwait(3*delay_factor); /* XXX Is this really necessary ? */
}

/*
 * getmixer -- get an SB mixer internal register
 */
static u_char
getmixer(u_short ioaddr, int port)
{
	u_char retval;

	SBERRPRINT(SBEP_L0, SBEM_MIXR, (CE_NOTE,
	    "sbpro: getmixer: reg(0x%x)", port));

	outb(ioaddr + MIXER_ADDR, port);
	drv_usecwait(delay_factor);
	retval = inb(ioaddr + MIXER_DATA);
	drv_usecwait(3*delay_factor); /* XXX Is this really necessary ? */
	SBERRPRINT(SBEP_L0, SBEM_MIXR,
	    (CE_NOTE, "sbpro: getmixer --> %d", retval));
	return (retval);
}

/*
 * Set an AD184x internal register
 */
static void
setreg(SBInfo *info, int index, int data)
{
	register int delay;

	int delay1 = 0, delay2 = 0, delay3 = 0;
	int was;
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	int is;

	ASSERT(info->flags & AD184x);

	/* If INIT is set, wait for it to clear. */
	delay =  600;		/* 6 milliseconds max */
	while ((inb(info->ioaddr+AD184x_INDEX) & 0x80) && delay--)
		drv_usecwait(delay_factor);

	delay1 = 600 - delay;

	/* avoid pops: don't set the register unless it needs it */
	outb(info->ioaddr+AD184x_INDEX, index & 0x0f);
	was = inb(info->ioaddr+AD184x_DATA);
	if (data == was && (index & 0x0f) < 0xe) {
		SBERRPRINT(SBEP_L0, SBEM_MIXR,
		    (CE_NOTE, "sbpro: AD184x SETREG: 0x%x was 0x%x",
		    index, was));
		return;
	}

	outb(info->ioaddr+AD184x_INDEX, index);
	outb(info->ioaddr+AD184x_DATA, data);
	is = inb(info->ioaddr+AD184x_DATA);

	/*
	 * We assume that the ACAL bit is not used.  If it is, we need to wait
	 * on the ACI bit whenever we deassert MCE.  As it is, we only have to
	 * worry when the sample rate changes.  In that case, we must wait for
	 * INIT high, then wait for INIT low again, and then lower MCE.  The
	 * INIT operation can take up to 6 milliseconds.  See AD1847 chip spec
	 * page 22.
	 *
	 * With debugs inserted, in practice I've never seen *any* delay below
	 * while waiting for INIT to go high.  I'm not sure the theoretical
	 * maximum, but I computed it as two frame times at the slowest sample
	 * rate: (2 frames) * (2 samples/frame) / (5512 samples/second).
	 *
	 * With debugs inserted I typically see INIT high for 4.5 to 5.5
	 * milliseconds, though sometimes lower.  The highest I've ever seen
	 * was 5.68ms.  The maximum is specified as 6ms at the bottom of page
	 * 4 of the AD1847 chip spec.
	 */
	if (index == (FORMAT_REG | AD184x_MODE_CHANGE) &&
	    (data & 0x0f) != (was & 0x0f)) {
		/* changing speed */
		/* wait for INIT high */
		delay = 73;	/* 726 microseconds max ? */
		while (!(inb(info->ioaddr+AD184x_INDEX) & 0x80) && delay--)
			drv_usecwait(delay_factor);
		delay2 = 73 - delay;
		/* wait for INIT low */
		delay =  600;		/* 6 milliseconds max */
		while ((inb(info->ioaddr+AD184x_INDEX) & 0x80) && delay--)
			drv_usecwait(delay_factor);
		delay3 = 600 - delay;
	}

	if (index & AD184x_MODE_CHANGE)
		outb(info->ioaddr+AD184x_INDEX, 0);	/* lower MCE */

	SBERRPRINT(SBEP_L0, SBEM_MIXR,
	    (CE_NOTE, "sbpro: AD184x SETREG: 0x%x to 0x%x was 0x%x is 0x%x",
	    index, data, was, is));
	if (delay1 || delay2 || delay3) {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L0, SBEM_MIXR,
		    (CE_NOTE, "        delay1 %d delay2 %d delay3 %d",
		    delay1, delay2, delay3));
	}
}

/*
 * Debugging aid -- allow arbitrary I/O for testing initialization of
 * newly supported almost-compatible cards.
 */
static void
custom_init(dev_info_t *dip, char *property)
{
	unsigned int *init;
	int len, i;

	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, property, (caddr_t)&init,
	    &len) != DDI_PROP_SUCCESS)
		return;

	/*
	 * Undocumented developer's aid:
	 *
	 * "init" property comes in pairs of integers:
	 *   0xDDDDIIII, 0xAAaaOOoo
	 * where
	 *   IIII is the I/O address to be read/written;
	 *   AAaa is ANDed with the input from the I/O address;
	 *   OOoo is ORed with the result of the AND;
	 *   DDDD is the delay in microseconds after in and/or out.
	 * If AA or OO is nonzero, word I/O is performed, else byte.
	 * If AAaa is zero, the "in" from the I/O address is skipped.
	 *
	 * Note the simple case is ioaddr,value.
	 */
	for (i = 1; i < len / sizeof (int); i += 2) {
		int word = 0;
		unsigned short outval = 0;
		SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
		    "sbpro: custom_init %s 0x%x, 0x%x", property,
		    init[i-1], init[i]));
		if (init[i] & 0xff00ff00)
			word++;
		if (init[i] >> 16) {
			outval = word ? inw(init[i-1] & 0xffff) :
			    inb(init[i-1] & 0xffff);
			SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
			    "sbpro: custom_init read 0x%x", outval));
			if (init[i-1] >> 16)
				drv_usecwait(init[i-1] >> 16);
			outval &= init[i] >> 16;
		}
		outval |= init[i] & 0xffff;
		if (word)
			outw(init[i-1] & 0xffff, outval);
		else
			outb(init[i-1] & 0xffff, outval);
		SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
		    "sbpro: custom_init wrote 0x%x", outval));
		if (init[i-1] >> 16)
			drv_usecwait(init[i-1] >> 16);
	}

	kmem_free(init, len);
}

static int
probe_card(dev_info_t *dip, SBInfo *info)
{
	int			ioaddr;

	/* Intr level and & priority */
	int			intr_val[2], intr_val_len;
	char			type_val[20];	/* type of card */
	int			ver_hi, ver_low;
	unsigned int		dmachans[2];	/* we support 2 DMA channels */
	int			jumpers = 1;
	int			board_irq, board_dma;
	int			dmachan8, dmachan16;
	int			board_dmachan8 = -1, board_dmachan16 = -1;
	int			index;
	static char		sbname[] = "CRTV,SBlaster";
	int			len;
	int			is_sbpro;

	int i;
	int reglen, nregs;
	struct {
	    int bustype;
	    int base;
	    int size;
	} *reglist;

	len = sizeof (type_val);
	if (GET_INT_PROP(dip, "type", type_val, &len) == DDI_PROP_SUCCESS) {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: card type property=<%s>", type_val));
	} else {
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: no type property in sbpro.conf file"));
		type_val[0] = 0;
	}

	is_sbpro = (strcmp(type_val, "sbpro") == 0) ||
	    (strcmp(ddi_get_name(dip), "sbpro") == 0);

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
		    "sbpro: reg property not found in devices property list");
		return (DDI_PROBE_FAILURE);
	}
	nregs = reglen / sizeof (*reglist);

	for (i = 0; i < nregs; i++)
		if (reglist[i].bustype == 1) {
			ioaddr = reglist[i].base;
			index = (ioaddr-0x220)>>5;
			if (is_sbpro && (index < 0 || index >= 4))
				continue;
			break;
		}

	if (i >= nregs) {
		cmn_err(CE_WARN,
		    "sbpro: valid I/O base address not found in reg property");
		kmem_free(reglist, reglen);
		return (DDI_PROBE_FAILURE);
	}

	kmem_free(reglist, reglen);

	if (strcmp(ddi_get_name(dip), "mwss") == 0 ||
	    strcmp(type_val, "MWSS_AD184x") == 0)
		return (probe_mwss_ad184x(dip, info, "MWSS_AD184x", ioaddr));

	if (strcmp(type_val, "AD184x") == 0)
		return (probe_mwss_ad184x(dip, info, type_val, ioaddr));

	/*
	 * SBPRO and SB16 and ESS
	 * Has there been an attachment on this io address already?
	 */
	mutex_enter(&probe_mutex[index]);
	if (attached[index]) {
		mutex_exit(&probe_mutex[index]);
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: probe failed, ioaddr already attached"));
		return (DDI_PROBE_FAILURE);
	}

	/*
	 * update the attached index to indicate that there is now an active
	 * attachment at the ioaddr represented by the index
	 */
	if (info != (SBInfo *)NULL) {
		attached[index] = 1;
		info->dip = dip;
		info->ioaddr = (u_short)ioaddr;
	}
	mutex_exit(&probe_mutex[index]);

	custom_init(dip, "pre_init");	/* debugging aid */

	/* check if the card is present... */
	if (dsp_reset((u_short)ioaddr, info)) {
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
						"sbpro: dsp_reset() failed"));
		return (DDI_PROBE_FAILURE);
	}

	len = sizeof (intr_val);
	if (GET_INT_PROP(dip, "interrupts", intr_val, &len)
	    != DDI_PROP_SUCCESS) {
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: no interrupts property in sbpro.conf file"));
		return (DDI_PROBE_FAILURE);
	}

	intr_val_len = len/sizeof (int);
	/*
	 * put these lines in comments to fix a bug on some
	 * laptop onborad sbpro.
	 */
	/*
	if (ddi_dev_nintrs(dip, &len) == DDI_FAILURE || len != 1) {
		cmn_err(CE_WARN, "sbpro: must have exactly 1 intr spec");
		return (DDI_PROBE_FAILURE);
	}
	*/

	{
		int i = intr_val_len-1;
		if (intr_val[i] != 9 && intr_val[i] != 5 && intr_val[i] != 7 &&
			intr_val[i] != 10) {
			cmn_err(CE_WARN, "sbpro: invalid interrupt value. Must"
					" be either 2(9), 5, 7 or 10");
			return (DDI_PROBE_FAILURE);
		}
	}

	/* check for the type of the board */
	dsp_command(ioaddr, GET_DSP_VER);
	ver_hi = dsp_getdata(ioaddr);
	ver_low = dsp_getdata(ioaddr);

	SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
	    "sbpro: dsp version(hi=%d, lo=%d):", ver_hi, ver_low));

	if (info != (SBInfo *)NULL) {
		(void) sprintf(info->sbpro_devtype.version,
			"%d.%d", ver_hi, ver_low);
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: dsp version(%s):", info->sbpro_devtype.version));
		bcopy(sbname, info->sbpro_devtype.name, sizeof (sbname));
		if (ver_hi >= 4) {
			info->flags |= SB16;
			bcopy("SB16", info->sbpro_devtype.config,
			    sizeof ("SB16"));
		} else {
			info->flags |= SBPRO;
			bcopy("SBPRO", info->sbpro_devtype.config,
			    sizeof ("SBPRO"));
		}

#ifdef	BUG_1176536
		/*
		 * This is a temporary fix to allow the audiotool to work with
		 * the sbpro driver. We think the correct usage of the device
		 * structure is above, but we are overwriting this so that the
		 * audiotool will work.
		 */
		if (ver_hi >= 4)
			bcopy("SUNW,sb16", info->sbpro_devtype.name,
			    sizeof ("SUNW,sb16"));
		else
			bcopy("SUNW,sbpro", info->sbpro_devtype.name,
			    sizeof ("SUNW,sbpro"));
#endif
	}

	if (type_val[0]) {
		if (ver_hi >= 4) {
			if (strcmp(type_val, "SB16")) {
				SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
				    "sbpro: type property doesn't match "
				    "type of card"));
				return (DDI_PROBE_FAILURE);
			}
		} else {
			if (strncmp(type_val, "SBPRO", 5) != 0) {
				SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
				    "sbpro: type property doesn't match "
				    "type of card"));
				return (DDI_PROBE_FAILURE);
			}
		}
	}

	dmachan8 = -1;
	dmachan16 = -1;
	len = sizeof (dmachans);
	if (GET_INT_PROP(dip, "dma-channels", dmachans, &len) ==
	    DDI_PROP_SUCCESS) {
		if (dmachans[0] != 0 && dmachans[0] != 1 && dmachans[0] != 3) {
			cmn_err(CE_WARN,
			    "sbpro: 8-bit DMA channel is incorrect.  DMA "
			    "channel %d requested; valid channels are 0,1,3",
			    dmachans[0]);
			return (DDI_PROBE_FAILURE);
		}
		dmachan8 = dmachans[0];
		if (info != (SBInfo *)NULL)
			info->dmachan8	= dmachans[0];

		if (ver_hi >= 4) { /* SB16 */
			if ((len != 2*sizeof (int)) ||
			    (dmachans[0] != dmachans[1] &&
			    dmachans[1] != 5 && dmachans[1] != 6 &&
			    dmachans[1] != 7)) {
				cmn_err(CE_WARN,
				    "sbpro: 16-bit DMA channel is incorrect. "
				    "Requested DMA channel %d; valid channels "
				    "are 5,6,7", dmachans[1]);
				return (DDI_PROBE_FAILURE);
			}



			dmachan16 = dmachans[1];
			if (info != (SBInfo *)NULL)
				info->dmachan16 = dmachans[1];
		}
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    " sbpro: using dma channels %d,%d", dmachan8, dmachan16));
	} else {
		/*EMPTY*/ /* in lint case */
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_WARN,
		    "sbpro: no dmachan property in sbpro.conf file"));
	}


	if (ver_hi >= 4) { /* SB16 */
		board_dma = getmixer(ioaddr, MIXER_DMA);
		board_irq = getmixer(ioaddr, MIXER_IRQ);

		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: board_dma %d, board_irq %d", board_dma, board_irq));
		/* Is this a non-jumpered SB16 card? */
		if (!(board_irq & 0x0F)) {
			SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
			    "sbpro: non-jumpered SB16 detected"));
			jumpers = 0;

			/*
			 * Stored non-jumpered settings so that we can reset
			 * the irq on sbpro_detach
			 */
			if (info != (SBInfo *)NULL)
				info->nj_card = 1;
		}
	}


	/* Jumpered SB16 card */
	if (ver_hi >= 4 && jumpers) {
		/* Is the board IRQ valid? */
		switch (board_irq & 0x0F) {
			case 1:
				board_irq = 9;
				break;
			case 2:
				board_irq = 05;
				break;
			case 4:
				board_irq = 07;
				break;
			case 8:
				board_irq = 10;
				break;
			default:
				board_irq = -1;
		}

		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
		    "sbpro: interrupts:prop %d, board %d",
		    intr_val[intr_val_len-1], board_irq));

		if (intr_val[intr_val_len-1] != board_irq) {
				SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
				    "sbpro: interrupt property doesn't match "
				    "interrupt on SB16 card"));
				return (DDI_PROBE_FAILURE);
		}

		/*
		 * check 8 bit dma channel. Mask out
		 * both bit 4 and 2, which may be on, and which are unused.
		 */
		switch (board_dma & 0x0B) {
			case DMA_CHAN_0:
				board_dmachan8 = 0;
				break;
			case DMA_CHAN_1:
				board_dmachan8 = 1;
				break;
			case DMA_CHAN_3:
				board_dmachan8 = 3;
				break;
			default:
				cmn_err(CE_WARN,
				    "sbpro: The 8 bit DMA Jumper on board"
				    "must be set. ");
				return (DDI_PROBE_FAILURE);

		}

		/* check 16 bit dma channel */
		switch (board_dma & 0xE0) {
			case DMA_CHAN_5:
				board_dmachan16 = 5;
				break;
			case DMA_CHAN_6:
				board_dmachan16 = 6;
				break;
			case DMA_CHAN_7:
				board_dmachan16 = 7;
				break;
			default:
				SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
				    "sbpro: No 16 bit DMA Jumper on board. "));
		}

		/*
		 * 8 bit DMA channel on card should correspond with 8 bit
		 * DMA property and the 16bit DMA property equals the 16 bit
		 * channel, or the 8 bit DMA property should be equal
		 * to the 16 bit DMA property signifying that the only an
		 * 8 bit channel should be used.
		 */
		/* Have properties been specified in the sbpro.conf file */
		if ((dmachan8 != -1)) {
			/*
			 * yes, so, check that the board settings correspond
			 * with the properties from conf file
			 */
			if ((dmachan8 != board_dmachan8) ||
			    ((dmachan16 != board_dmachan16) &&
			    !((dmachan16 == dmachan8) &&
			    (board_dmachan16 == -1)))) {
				cmn_err(CE_WARN,
				    "sbpro: 8/16 bit DMA properties don't "
				    "match DMA jumpers on SB16 card");
				return (DDI_PROBE_FAILURE);
			}
		}

		if (info != (SBInfo *)NULL) {
			SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
			    "sbpro: board dma8=%d, dma16=%d",
			    board_dmachan8, board_dmachan16));
			info->dmachan8 = board_dmachan8;
			if (board_dmachan16 == -1)
				info->dmachan16 = board_dmachan8;
			else
				info->dmachan16 = board_dmachan16;
		}
	}

	/* Non-jumpered SB16 Card */
	if (ver_hi >= 4 && !jumpers) {
		if ((dmachan8 == -1) || (dmachan16 == -1)) {
			cmn_err(CE_WARN, "sbpro: 8 and 16 bit DMA properties "
			    "must be specified for the non-jumpered SB16 card");
			return (DDI_PROBE_FAILURE);
		}

		/*
		 * This initialisation is required for the AWE32 non jumpered
		 * SB16 because there
		 * are no on-board jumpers to set these values. They must be
		 * set by software. AWE32 and SB16 cards have these registers
		 * which will be initialised to the values in the conf file.
		 * For cards with jumpers, the values in the conf file will
		 * override the jumpers.
		 */
		if (info != (SBInfo *)NULL) {
			int dma_val, intr_reg;

			/*
			 * Convert IRQ and 8 and 16 bit DMA channels to
			 * appropriate bit mask for the board
			 */
			switch (intr_val[intr_val_len-1]) {
			case 9:
				intr_reg = 0x01;
				break;
			case 5:
				intr_reg = 0x02;
				break;
			case 7:
				intr_reg = 0x04;
				break;
			case 10:
				intr_reg = 0x08;
				break;
			}
			setmixer(info, MIXER_IRQ, intr_reg);

			switch (info->dmachan8) {
			case 0:
				dma_val = DMA_CHAN_0;
				break;
			case 1:
				dma_val = DMA_CHAN_1;
				break;
			case 3:
				dma_val = DMA_CHAN_3;
				break;
			}

			switch (info->dmachan16) {
			case 5:
				dma_val |= DMA_CHAN_5;
				break;
			case 6:
				dma_val |= DMA_CHAN_6;
				break;
			case 7:
				dma_val |= DMA_CHAN_7;
				break;
			}
			setmixer(info, MIXER_DMA, dma_val);
		}
	}

	if (info && ver_hi >= 4)
		custom_init(dip, "post_init");	/* debugging aid */

	if (ver_hi < 4) {
		char	bus_type[16];
		int	len = sizeof (bus_type);

		if (dmachan8 == -1) {
			cmn_err(CE_WARN, "sbpro: 8 bit DMA property must exist "
				    "on the SBPRO card");
			return (DDI_PROBE_FAILURE);
		}

		/* If Microchannel, we require "type=SBPRO-MCV" */
		if (ddi_prop_op(DDI_DEV_T_ANY, dip,
		    PROP_LEN_AND_VAL_BUF, 0, "device_type",
		    (caddr_t)bus_type, &len) != DDI_PROP_SUCCESS &&
		    (len = sizeof (bus_type)) &&
		    ddi_prop_op(DDI_DEV_T_ANY, dip,
		    PROP_LEN_AND_VAL_BUF, 0, "bus-type",
		    (caddr_t)bus_type, &len) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, "sbpro cannot find bus type");
			return (DDI_PROBE_FAILURE);
		}
		if (strcmp(bus_type, "mc") == 0 &&
		    strcmp(type_val, "SBPRO-MCV") != 0) {
			cmn_err(CE_WARN, "sbpro microchannel requires "
			    "type=\"SBPRO-MCV\" and correct IRQ/DMA.");
			cmn_err(CE_WARN, "sbpro microchannel incorrect "
			    "IRQ will cause system hang; use care.");
			return (DDI_PROBE_FAILURE);
		}

		/* If MCA, believe the IRQ value specified in the .conf file */
		if (strcmp(type_val, "SBPRO-MCV") == 0 ||
		    strcmp(type_val, "SBPRO-NOCHECK") == 0) {
			if (info)
				custom_init(dip, "post_init");	/* debugging */
			return (DDI_PROBE_SUCCESS);
		}

		/* Only do sbpro interrupt check on probe */
		if (info == (SBInfo *)NULL) {
			/* must post_init before doing interrupt check */
			custom_init(dip, "post_init");	/* debugging aid */
			return (check_sbpro_intr(dip, info, ioaddr, dmachan8));
		}
	}

	return (DDI_PROBE_SUCCESS);
}

static int
check_sbpro_intr(dev_info_t *dip, SBInfo *info, int ioaddr, int dmachan8)
{
	ddi_iblock_cookie_t	iblock_cookie;
	ddi_idevice_cookie_t	idevice_cookie;
	off_t			offset, length;
	struct ddi_dmae_req	dmaereq;
	char			tmpbuf[2];
	ddi_dma_handle_t	dmahand;	/* DMA handle */
	ddi_dma_win_t		dmawin;		/* DMA window */
	ddi_dma_seg_t		dmaseg;		/* DMA segment */
	ddi_dma_cookie_t	dmacookie;	/* DMA cookie */
	int			intr_count = 0;
	/*LINTED: set but not used (used in DEBUG and !lint case)*/
	int			ret, iter;


	/*
	 *	Acquire the DMA channel
	 */
	if (ddi_dmae_alloc(dip, dmachan8, DDI_DMA_DONTWAIT, NULL)
	    != DDI_SUCCESS) {
		SBERRPRINT(SBEP_L4, SBEM_PROB, (CE_WARN,
		    "sbpro attach: cannot acquire dma channel %d", dmachan8));
		return (DDI_PROBE_FAILURE);
	}

	/*
	 * This section of the code will add a dummy interrupt service
	 * routine for the interrupt entry in the sbpro.conf file. It
	 * then forces the SBPRO card to generate an interrupt to determine
	 * whether the entry in the sbpro.conf file is the actual
	 * interrupt for the card. The interrupt is then
	 * removed, and we return the result.
	 */

	if (ddi_add_intr(dip, 0, &(iblock_cookie),
	    &(idevice_cookie), intr_vect,
	    (caddr_t)&intr_count) != DDI_SUCCESS) {
		SBERRPRINT(SBEP_L4, SBEM_PROB, (CE_NOTE,
				"sbpro attach: ddi_add_intr failed."));
		goto failed;
	}

	if ((ret = ddi_dma_addr_setup(dip, (struct as *)0,
	    (caddr_t)tmpbuf, 2,
	    DDI_DMA_READ,
	    DDI_DMA_SLEEP, 0, 0, &dmahand)) != DDI_SUCCESS) {
		SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_WARN,
		    "sbpro intr test: can't set up dma (%d).", ret));
		goto failed1;
	}

	(void) ddi_dma_nextwin(dmahand, 0, &dmawin);
	/*
	 * Ignored return (DDI_SUCCESS).  First window requested.
	 */
	(void) ddi_dma_nextseg(dmawin, 0, &dmaseg);
	/*
	 * Ignored return (DDI_SUCCESS).  First segment requested.
	 */
	if (ddi_dma_segtocookie(dmaseg, &offset, &length, &dmacookie) ==
	    DDI_FAILURE)
		cmn_err(CE_WARN, "sbpro attach: dma segtocookie failed");
	bzero((caddr_t)&dmaereq, sizeof (dmaereq));
	dmaereq.der_command = (u_char)DMAE_CMD_READ;

	/* We let it tell us 3 times, so we know it's true */
	for (iter = 0; iter < 3; iter++) {
		intr_count = 0;

		if (ddi_dmae_prog(dip, &dmaereq, &dmacookie, dmachan8) !=
		    DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro attach: dmae prog failed");

		dsp_command(ioaddr, RECORD_MONO);
		dsp_command(ioaddr, SPEAKER_OFF);
		dsp_command(ioaddr, SET_CONSTANT);
		dsp_command(ioaddr, 0x83); /* 0x83 = 8000 Hz */
		dsp_command(ioaddr, ADC_DMA);
		dsp_command(ioaddr, 1); /* low byte */
		dsp_command(ioaddr, 0); /* high byte */

		/*
		 * We have programmed the card to read 2 samples of audio
		 * data at a sampling rate of 8000Hz. We need to wait for this
		 * operation to complete so that the interrupt will have been
		 * generated i.e. 250usec. In addition there is the delay for
		 * the device to "get started" once the ADC_DMA command has
		 * been given to it.  This delay is under 50usec for an SBPRO
		 * card but is sometimes more than that for an ESS.  A 400usec
		 * delay seems to consistently work with the ESS688.  We will
		 * wait 500usec to be sure the interrupt is generated.
		*/
		drv_usecwait(50*delay_factor);

		/* Now do some clean up on the DMA.		*/
		if (ddi_dmae_stop(dip, dmachan8) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro check intr: "
			    "dma stop failed");
		(void) dsp_reset(ioaddr, (SBInfo *)info);

		if (intr_count == 0) {
			SBERRPRINT(SBEP_L1, SBEM_PROB, (CE_NOTE,
			    "SBPRO card did not generate the interrupt"));
			break;
		}
	}

	if (ddi_dma_free(dmahand) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro attach: dma free failed");

failed1:
	ddi_remove_intr(dip, 0, iblock_cookie);
failed:
	if (ddi_dmae_release(dip, dmachan8) != DDI_SUCCESS)
		cmn_err(CE_WARN, "sbpro check intr: dmae release failed, "
		    "dip %p dma channel %d", (void*)dip, info->dmachan16);
	if (intr_count)
		return (DDI_PROBE_SUCCESS);
	else
		return (DDI_PROBE_FAILURE);
}

static int
probe_mwss_ad184x(dev_info_t *dip, SBInfo *info, char *type_val, int ioaddr)
{
	int			intr_val[2], intr_val_len;
	unsigned int		dmachans[1];
	char			*name, *vers, *config;
	int			len, val, i, delay;

	name = "AD184x";
	vers = "AD184x";
	config = "AD184x";


	len = sizeof (intr_val);
	if (GET_INT_PROP(dip, "interrupts", intr_val, &len)
	    != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
		    "sbpro: AD184x no interrupts property in .conf file");
		return (DDI_PROBE_FAILURE);
	}
	intr_val_len = len/sizeof (int);
	if (ddi_dev_nintrs(dip, &len) == DDI_FAILURE || len != 1) {
		cmn_err(CE_WARN, "sbpro: must have exactly 1 intr spec");
		return (DDI_PROBE_FAILURE);
	}

	len = sizeof (dmachans);
	if (GET_INT_PROP(dip, "dma-channels", dmachans, &len) !=
	    DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "sbpro: DMA property must specify one channel"
		    " for the AD184x device");
		return (DDI_PROBE_FAILURE);
	}

	custom_init(dip, "pre_init");	/* debugging aid */

	/* If INIT is set, wait for it to clear. */
	delay =  600;		/* 6 milliseconds max */
	while ((inb(ioaddr+AD184x_INDEX) & 0x80) && delay--)
		drv_usecwait(delay_factor);
	if (delay == 0) {
		SBERRPRINT(SBEP_L3, SBEM_PROB, (CE_NOTE,
		    "sbpro: probe_mwss_ad184x INIT not clear @ 0x%x", ioaddr));
		return (DDI_PROBE_FAILURE);
	}

	if (strcmp(type_val, "MWSS_AD184x") == 0) {
		/* Microsoft Windows Sound System */
		val = 0;

		switch (intr_val[intr_val_len-1]) {
		case 7:
			val |= 0x8;
			break;
		case 9:
			val |= 0x10;
			break;
		case 10:
			val |= 0x18;
			break;
		case 11:
			val |= 0x20;
			break;
		default:
			cmn_err(CE_WARN,
			    "sbpro: MWSS_AD184x invalid interrupt value %d."
			    "  Must be either 7, 9, 10, or 11.",
			    intr_val[intr_val_len-1]);
			return (DDI_PROBE_FAILURE);
		}

		switch (dmachans[0]) {
		case 0:
			val |= 0x1;
			break;
		case 1:
			val |= 0x2;
			break;
		case 3:
			val |= 0x3;
			break;
		default:
			cmn_err(CE_WARN,
			    "sbpro: MWSS_AD184x DMA channel is incorrect.  DMA "
			    "channel %d requested; valid channels are 0, 1, 3.",
			    dmachans[0]);
			return (DDI_PROBE_FAILURE);
		}

		i = inb(ioaddr+MWSS_IRQSTAT);
		if ((i & 0x3f) != 4) {
			SBERRPRINT(SBEP_L3, SBEM_PROB, (CE_NOTE,
			    "sbpro: probe_mwss_ad184x MWSS ID 0x%x != 4", i));
			return (DDI_PROBE_FAILURE);
		}

		outb(ioaddr+MWSS_IRQSTAT, val | 0x40);	/* Test IRQ */
		if (!(inb(ioaddr+MWSS_IRQSTAT) & 0x40)) {
			cmn_err(CE_WARN,
			    "sbpro: MWSS_AD184x IRQ %d is 'in use.'",
			    intr_val[intr_val_len-1]);
			return (DDI_PROBE_FAILURE);
		}

		outb(ioaddr+MWSS_IRQSTAT, val);	/* Set IRQ, DMA */

		name = "MSFT,WSS_AD184x";
		vers = "MWSS_AD184x";
		config = "MWSS_AD184x";
	}

	if (info != (SBInfo *)NULL) {
		custom_init(dip, "post_init");	/* debugging aid */
		info->dip = dip;
		info->ioaddr = (u_short)ioaddr;
		info->dmachan8	= dmachans[0];
		info->flags |= AD184x;

		/* detect Compaq 1847 with only one mic gain bit */
		setreg(info, LEFT_IN_REG, 0x20);
		setreg(info, RIGHT_IN_REG, 0x0);
		outb(ioaddr+AD184x_INDEX, RIGHT_IN_REG);
		i = inb(ioaddr+AD184x_DATA);
		if (i == 0x20) {
			SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
			    "sbpro: Compaq AD1847"));
			info->flags |= CBA1847;
			name = "CPQ,CBA_XL";
			vers = "AD1847";
			config = "AD1847";
		} else if (i != 0) {
			/*EMPTY*/ /* in lint case */
			SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
			    "sbpro: Unknown AD184x (0x%x)", i));
		}

		bcopy(name, info->sbpro_devtype.name, strlen(name));
		bcopy(vers, info->sbpro_devtype.version, strlen(vers));
		bcopy(config, info->sbpro_devtype.config, strlen(config));

#ifdef	BUG_1176536
		/*
		 * This is a temporary fix to allow the audiotool to work with
		 * the sbpro driver. We think the correct usage of the device
		 * structure is above, but we are overwriting this so that the
		 * audiotool will work.
		 */
		bcopy("SUNW,sb16", info->sbpro_devtype.name,
		    sizeof ("SUNW,sb16"));
#endif

		SBERRPRINT(SBEP_L2, SBEM_PROB, (CE_NOTE,
		    "sbpro: Found an ad184x at 0x%x, IRQ %d, dmachan %d",
		    ioaddr, intr_val[intr_val_len-1], dmachans[0]));

		dsp_cardreset(info);
		setreg(info, MON_LOOP_REG, 0x0);	/* monitor disable */

		setreg(info, CFG_REG | AD184x_MODE_CHANGE, 0xc); /* ACAL */
		drv_usecwait(100*delay_factor);	/* ensure ACI has gone high */
		outb(ioaddr+AD184x_INDEX, TEST_INIT_REG);
		delay =  7000;		/* 384 sample times max < 70ms */
		while ((inb(ioaddr+AD184x_DATA) & 0x20) && delay--)
			drv_usecwait(delay_factor);

		SBERRPRINT(SBEP_L1, SBEM_PROB,
		    (CE_NOTE, "AD184x ACI delay %d", 7000-delay));

		setreg(info, CFG_REG | AD184x_MODE_CHANGE, 0x4); /* 1 dmachan */
		setreg(info, MON_LOOP_REG, 0x1);	/* monitor enable */
	}

	return (DDI_PROBE_SUCCESS);
}

/* ARGSUSED1 */
static void
sbpro_pause(SBInfo *info, int record_info)
{
	mblk_t *bp;
	int samples;

	/* only halt DMA if we're actually doing DMA */
	if (info->flags & WRITING) {
		if (info->flags & (W_BUSY))
			dsp_dmahalt(info);
		else
			noenable(info->wrq);
		info->audio_info.play.active =
		    AUDIO_IS_NOT_ACTIVE;
	}

	/*
	 * we don't pause while record. Instead, we
	 * stop the card and send the accumulated recorded
	 * data up stream to the user appl.
	 */
	if (info->flags & (R_BUSY)) {
		int size, count;

		info->audio_info.record.active = AUDIO_IS_NOT_ACTIVE;
		(void) dsp_reset(info->ioaddr, info);
		if (ddi_dmae_stop(info->dip, info->dmachan) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro pause: dma stop failed");
		if (ddi_dma_sync(info->aidmahandle, 0, 0,
		    DDI_DMA_SYNC_FORKERNEL) != DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro pause: dma sync failed");
		if (ddi_dmae_getcnt(info->dip, info->dmachan, &count) !=
		    DDI_SUCCESS)
			cmn_err(CE_WARN, "sbpro pause: dmae getcnt failed");
		count = (count+3) & ~3;
		if (info->bufnum == 1) {
			if (count > info->buflen) {
				size = (2*info->buflen)-count;
				if (bp = allocb(size+info->buflen,
						BPRI_MED)) {
					dsp_readdata(info,
						    info->buffers[1],
						    bp->b_wptr,
						    info->buflen);
					bp->b_wptr += info->buflen;
					if (size)
						dsp_readdata(info,
							    info->buffers[0],
							    bp->b_wptr, size);
					bp->b_wptr += size;
					size += info->buflen;
				} else {
					SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
					    "sbpro_intr: out of stream bufs"));
					info->audio_info.record.error = 1;
				    goto out;
				}
			} else {
				if ((size = info->buflen-count) == 0) goto out;
				if (bp = allocb(size, BPRI_MED)) {
					dsp_readdata(info,
						    info->buffers[1],
						    bp->b_wptr, size);
					bp->b_wptr += size;
				} else {
					SBERRPRINT(SBEP_L4, SBEM_SPRI,
							(CE_WARN,
				    "sbpro_intr: out of stream bufs"));
					info->audio_info.record.error = 1;
					goto out;
				}
			}
		} else {
			if ((size = 2*info->buflen - count) == 0) goto out;
			if (bp = allocb(size, BPRI_MED)) {
				dsp_readdata(info, info->buffers[0],
				    bp->b_wptr, size);
				bp->b_wptr += size;
			} else {
				SBERRPRINT(SBEP_L4, SBEM_SPRI, (CE_WARN,
				    "sbpro_intr: out of stream bufs"));
				info->audio_info.record.error = 1;
				goto out;
			}
		}

		if (canputnext(info->rdq)) {
			putnext(info->rdq, bp);
			samples = size/(info->dspbits/8);
			if (info->dsp_stereomode)
				samples /= 2;
			info->audio_info.record.samples
					+= samples;
		} else {
			freemsg(bp);
			info->audio_info.record.error = 1;
		}

	}
out:
	if (info->ctlrdq)
		sbpro_sendsig(info->ctlrdq, SIGPOLL);
}


static void
sbpro_pause_resume(SBInfo *info, int record_info)
{
	if (record_info) {
		if (!(info->flags & R_BUSY)) {
			info->audio_info.record.active =
			    AUDIO_IS_ACTIVE;
			sbpro_start(info, FREAD);
		}
	} else {
		if (info->flags & PAUSED_DMA) {
			dsp_dmacont(info);
			info->audio_info.play.active=
			    AUDIO_IS_ACTIVE;
		} else {
			if (info->paused_buffer) {
				info->audio_info.play.active
				    = AUDIO_IS_ACTIVE;
				sbpro_start(info, FWRITE);
			} else {
				enableok(info->wrq);
				qenable(info->wrq);
			}
		}
	}
	if (info->ctlrdq)
		sbpro_sendsig(info->ctlrdq, SIGPOLL);
}
