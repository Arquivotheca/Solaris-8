/*
 * Copyright (c) 1996-1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)zs_async.c	1.167	99/09/27 SMI"

/*
 *	Asynchronous protocol handler for Z8530 chips
 *	Handles normal UNIX support for terminals & modems
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/kmem.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/tty.h>
#include <sys/ptyvar.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/mkdev.h>
#include <sys/cmn_err.h>
#include <sys/strtty.h>
#include <sys/ksynch.h>
#include <sys/clock.h>
#include <sys/cpu.h>
#include <sys/consdev.h>
#include <sys/zsdev.h>
#include <sys/ser_async.h>
#include <sys/debug.h>
#include <sys/kbio.h>
#include <sys/debug/debug.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>

/*
 * PPS (Pulse Per Second) support.
 */
extern void ddi_hardpps(struct timeval *, int);
static struct ppsclockev ppsclockev;

#ifdef PPSCLOCKLED
/* XXX Use these to observe PPS latencies and jitter on a scope */
#define	LED_ON
#define	LED_OFF
#else
#define	LED_ON
#define	LED_OFF
#endif

extern	kcondvar_t lbolt_cv;
char	*zssoftCAR;
extern	int	nzs;
extern	struct zscom *zscom;
extern struct zs_prog *zs_prog;


#define	ZSA_RCV_SIZE	64
#define	ZA_KICK_RCV_COUNT	3
#define	ZSA_GRACE_MIN_FLOW_CONTROL	5
#define	ZSA_GRACE_MAX_FLOW_CONTROL	20

int zsasoftdtr = 0;	/* if nonzero, softcarrier raises dtr at attach */
int zsb134_weird = 0;	/* if set, old weird B134 behavior */
int g_zsticks = 0;	/* if set, becomes the global zsticks value */
int g_nocluster = 0;	/* if set, disables clustering of received data */

unsigned int zsa_rstandby = ZSA_MIN_RSTANDBY;
unsigned int zsa_rdone = ZSA_RDONE_MIN;
unsigned int zsa_grace_flow_control = ZSA_GRACE_MIN_FLOW_CONTROL;


#define	NSPEED	18	/* max # of speeds */
u_short	zs_speeds[NSPEED] = {
	0,
	ZSPEED(50),	/* 1 */
	ZSPEED(75),	/* 2 */
	ZSPEED(110),	/* 3 */
#ifdef lint
	ZSPEED(134),	/* 4 */
#else
	ZSPEED(269/2),			/* XXX - This is sleazy */
#endif
	ZSPEED(150),	/* 5 */
	ZSPEED(200),	/* 6 */
	ZSPEED(300),	/* 7 */
	ZSPEED(600),	/* 8 */
	ZSPEED(1200),	/* 9 */
	ZSPEED(1800),	/* 10 */
	ZSPEED(2400),	/* 11 */
	ZSPEED(4800),	/* 12 */
	ZSPEED(9600),	/* 13 */
	ZSPEED(19200),	/* 14 */
	ZSPEED(38400),	/* 15 */
	ZSPEED(57680),	/* 16 */
	ZSPEED(76800)	/* 17 */
};

u_short	zsticks[NSPEED] = {
	3,		/* 0 */
	3,		/* 1 */
	3,		/* 2 */
	3,		/* 3 */
	3,		/* 4 */
	3,		/* 5 */
	3,		/* 6 */
	3,		/* 7 */
	3,		/* 8 */
	3,		/* 9 */
	3,		/* 10 */
	3,		/* 11 */
	3,		/* 12 */
	3,		/* 13 */
	2,		/* 14 */
	1,		/* 15 */
	1,		/* 16 */
	1		/* 17 */
};

#define	ztdelay(nsp)	(zsdelay[(nsp)]*(hz/100))

u_short	zsdelay[NSPEED] = {
	0,
	ZDELAY(50),	/* 1 */
	ZDELAY(75),	/* 2 */
	ZDELAY(110),    /* 3 */
#ifdef lint
	ZDELAY(134),    /* 4 */
#else
	ZDELAY(269/2),
#endif
	ZDELAY(150),    /* 5 */
	ZDELAY(200),    /* 6 */
	ZDELAY(300),    /* 7 */
	ZDELAY(600),    /* 8 */
	ZDELAY(1200),   /* 9 */
	ZDELAY(1800),   /* 10 */
	ZDELAY(2400),   /* 11 */
	ZDELAY(4800),   /* 12 */
	ZDELAY(9600),   /* 13 */
	ZDELAY(19200),  /* 14 */
	ZDELAY(38400),  /* 15 */
	ZDELAY(57600),  /* 16 */
	ZDELAY(76800)	/* 17 */
};

u_short	zslowat[NSPEED] = {
	3,		/* 0 */
	3,		/* 1 */
	3,		/* 2 */
	3,		/* 3 */
	3,		/* 4 */
	3,		/* 5 */
	3,		/* 6 */
	2,		/* 7 */
	2,		/* 8 */
	2,		/* 9 */
	2,		/* 10 */
	1,		/* 11 */
	1,		/* 12 */
	1,		/* 13 */
	1,		/* 14 */
	1,		/* 15 */
	1,		/* 16 */
	1		/* 17 */
};

u_short	zshiwat[NSPEED] = {
	0,		/* 0 */
	1,		/* 1 */
	1,		/* 2 */
	1,		/* 3 */
	1,		/* 4 */
	1,		/* 5 */
	1,		/* 6 */
	1,		/* 7 */
	1,		/* 8 */
	1,		/* 9 */
	1,		/* 10 */
	1,		/* 11 */
	1,		/* 12 */
	3,		/* 13 */
	3,		/* 14 */
	4,		/* 15 */
	4,		/* 16 */
	4		/* 17 */
};

#define	SLAVIO_BUG	/* this workaround required to fix bug 1102778 */

#define	SPEED(cflag) \
	((cflag) & CBAUDEXT) ? \
		(((cflag) & 0x1) + CBAUD + 1) : ((cflag) & CBAUD)

/*
 * Special macros to handle STREAMS operations.
 * These are required to address memory leakage problems.
 * WARNING : the macro do NOT call ZSSETSOFT
 */

/*
 * Should be called holding only the adaptive (zs_excl) mutex.
 */
#define	ZSA_GETBLOCK(zs, allocbcount) \
{ \
	register int n = zsa_rstandby; \
	while (--n >= 0 && allocbcount > 0) { \
		if (!za->za_rstandby[n]) { \
			if ((za->za_rstandby[n] = allocb(ZSA_RCV_SIZE, \
			    BPRI_MED)) == NULL) { \
				if (za->za_bufcid == 0) { \
					za->za_bufcid = bufcall(ZSA_RCV_SIZE, \
					    BPRI_MED, \
					    zsa_callback, zs); \
					break; \
				} \
			} \
			allocbcount--; \
		} \
	} \
	if (za->za_ttycommon.t_cflag & CRTSXOFF) { \
		mutex_enter(zs->zs_excl_hi); \
		if (!(zs->zs_wreg[5] & ZSWR5_RTS)) { \
			register int usedcnt = 0; \
			for (n = 0; n < zsa_rstandby; n++) \
				if (!za->za_rstandby[n]) \
					usedcnt++; \
			if ((u_short)usedcnt <= \
			    zslowat[SPEED(za->za_ttycommon.t_cflag)]) \
				SCC_BIS(5, ZSWR5_RTS); \
		} \
		mutex_exit(zs->zs_excl_hi); \
	} \
}

/*
 * Should be called holding the spin (zs_excl_hi) mutex.
 */
#define	ZSA_ALLOCB(mp) \
{ \
	register int n = zsa_rstandby; \
	while (--n >= 0) { \
		if ((mp = za->za_rstandby[n]) != NULL) { \
			za->za_rstandby[n] = NULL; \
			break; \
		} \
	} \
	if (za->za_ttycommon.t_cflag & CRTSXOFF) { \
		if (!mp) { \
			if (zs->zs_wreg[5] & ZSWR5_RTS) \
				SCC_BIC(5, ZSWR5_RTS); \
			cmn_err(CE_WARN, "zs%d: message lost\n", \
				UNIT(za->za_dev)); \
		} else if (zs->zs_wreg[5] & ZSWR5_RTS) { \
			register int usedcnt = 0; \
			for (n = 0; n < zsa_rstandby; n++) \
				if (!za->za_rstandby[n]) \
					usedcnt++; \
			if ((u_short)usedcnt >= (zsa_rstandby - \
			    zshiwat[SPEED(za->za_ttycommon.t_cflag)])) \
				SCC_BIC(5, ZSWR5_RTS); \
		} \
	} \
}

/*
 * Should get the spin (zs_excl_hi) mutex.
 */
#define	ZSA_QREPLY(q, mp) \
{ \
	mutex_enter(zs->zs_excl_hi); \
	ZSA_PUTQ(mp); \
	ZSSETSOFT(zs); \
	mutex_exit(zs->zs_excl_hi); \
}

/*
 * Should be called holding the spin (zs_excl_hi) mutex.
 */
#define	ZSA_PUTQ(mp) \
{ \
	register int wptr, rptr; \
	wptr = za->za_rdone_wptr; \
	rptr = za->za_rdone_rptr; \
	za->za_rdone[wptr] = mp; \
	if ((wptr)+1 == zsa_rdone) { \
		za->za_rdone_wptr = wptr = 0; \
	} else \
		za->za_rdone_wptr = ++wptr; \
	if (wptr == rptr) { \
		SCC_BIC(1, ZSWR1_INIT); \
		cmn_err(CE_WARN, "zs%d disabled: input buffer overflow", \
			UNIT(za->za_dev)); \
	} \
}

/*
 * Should be called holding the spin (zs_excl_hi) mutex.
 */
#define	ZSA_KICK_RCV \
{ \
	register mblk_t *mp = za->za_rcvblk; \
	if (mp) { \
		if (zs->zs_rd_cur) {	/* M_DATA */ \
			mp->b_wptr = zs->zs_rd_cur; \
			zs->zs_rd_cur = NULL; \
			zs->zs_rd_lim = NULL; \
		} \
		za->za_rcvblk = NULL; \
		ZSA_PUTQ(mp); \
		ZSSETSOFT(zs); \
	} \
}

#define	ZSA_SEEQ(mp) \
{ \
		if (za->za_rdone_rptr != za->za_rdone_wptr) { \
			mp = za->za_rdone[za->za_rdone_rptr]; \
		} else { \
			mp = NULL; \
		} \
}


/*
 * Should be called holding only the adaptive (zs_excl) mutex.
 */
#define	ZSA_GETQ(mp) \
{ \
	if (za->za_rdone_rptr != za->za_rdone_wptr) { \
		mp = za->za_rdone[za->za_rdone_rptr]; \
		za->za_rdone[za->za_rdone_rptr++] = NULL; \
		if (za->za_rdone_rptr == zsa_rdone) \
			za->za_rdone_rptr = 0; \
	} else { \
		mp = NULL; \
	} \
}

/*
 * Should be called holding only the adaptive (zs_excl) mutex.
 */
#define	ZSA_FLUSHQ \
{ \
	register mblk_t *tmp; \
	for (;;) { \
		ZSA_GETQ(tmp); \
		if (!(tmp)) \
			break; \
		freemsg(tmp); \
	} \
}


/*
 * Logging definitions
 */

#ifdef ZSA_DEBUG

#ifdef ZS_DEBUG_ALL

extern	char	zs_h_log[];
extern	int	zs_h_log_n;

#define	zsa_h_log_clear

#define	zsa_h_log_add(c) \
{ \
	if (zs_h_log_n >= ZS_H_LOG_MAX) \
		zs_h_log_n = 0; \
	zs_h_log[zs_h_log_n++] = 'A' + zs->zs_unit; \
	zs_h_log[zs_h_log_n++] = c; \
	zs_h_log[zs_h_log_n] = '\0'; \
}

#else /* ZS_DEBUG_ALL */

#define	ZSA_H_LOG_MAX	0x4000
char zsa_h_log[40][ZSA_H_LOG_MAX +10];
int zsa_h_log_n[40];

#define	zsa_h_log_add(c) \
{ \
	if (zsa_h_log_n[zs->zs_unit] >= ZSA_H_LOG_MAX) \
		zsa_h_log_n[zs->zs_unit] = 0; \
	zsa_h_log[zs->zs_unit][zsa_h_log_n[zs->zs_unit]++] = c; \
	zsa_h_log[zs->zs_unit][zsa_h_log_n[zs->zs_unit]] = '\0'; \
}

#define	zsa_h_log_clear \
{ \
	register char *p; \
	for (p = &zsa_h_log[zs->zs_unit][ZSA_H_LOG_MAX]; \
		p >= &zsa_h_log[zs->zs_unit][0]; /* null */) \
		*p-- = '\0'; \
	zsa_h_log_n[zs->zs_unit] = 0; \
}

#endif /* ZS_DEBUG_ALL */

#define	ZSA_R0_LOG(r0) \
{ \
	if (r0 & ZSRR0_RX_READY) zsa_h_log_add('R'); \
	if (r0 & ZSRR0_TIMER) zsa_h_log_add('Z'); \
	if (r0 & ZSRR0_TX_READY) zsa_h_log_add('T'); \
	if (r0 & ZSRR0_CD) zsa_h_log_add('D'); \
	if (r0 & ZSRR0_SYNC) zsa_h_log_add('S'); \
	if (r0 & ZSRR0_CTS) zsa_h_log_add('C'); \
	if (r0 & ZSRR0_TXUNDER) zsa_h_log_add('U'); \
	if (r0 & ZSRR0_BREAK) zsa_h_log_add('B'); \
}

#else /* ZSA_DEBUG */

#define	zsa_h_log_clear
#define	zsa_h_log_add(c)
#define	 ZSA_R0_LOG(r0)

#endif /* ZSA_DEBUG */



static int zsa_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr);
static int zsa_close(queue_t *q, int flag);
static int zsa_wput(queue_t *q, mblk_t *mp);
static int zsa_rsrv(queue_t *q);

static struct module_info asyncm_info = {
	0,
	"zs",
	0,
	INFPSZ,
	2048,
	128
};

static struct qinit async_rinit = {
	putq,
	zsa_rsrv,
	zsa_open,
	zsa_close,
	NULL,
	&asyncm_info,
	NULL
};

static struct qinit async_winit = {
	zsa_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&asyncm_info,
	NULL
};

struct streamtab asynctab = {
	&async_rinit,
	&async_winit,
	NULL,
	NULL,
};

/*
 * The async interrupt entry points.
 */
static void	zsa_txint(struct zscom *zs);
static void	zsa_xsint(struct zscom *zs);
static void	zsa_rxint(struct zscom *zs);
static void	zsa_srint(struct zscom *zs);
static int	zsa_softint(struct zscom *zs);
static int	zsa_suspend(struct zscom *zs);
static int	zsa_resume(struct zscom *zs);

extern struct zsops zsops_null;

static void
zsa_null(struct zscom *zs)
{
	register short	c;

	SCC_WRITE0(ZSWR0_RESET_TXINT);
	SCC_WRITE0(ZSWR0_RESET_STATUS);
	c = SCC_READDATA();
	ZSDELAY();
#ifdef lint
	c = c;
#endif lint
	SCC_WRITE0(ZSWR0_RESET_ERRORS);

}

/*ARGSUSED*/
static int
zsa_null_int(struct zscom *zs)
{
	return (0);
}

struct zsops zsops_null_async = {
	zsa_null,
	zsa_null,
	zsa_null,
	zsa_null,
	zsa_null_int,
	zsa_null_int,
	zsa_null_int
};

struct zsops zsops_async = {
	zsa_txint,
	zsa_xsint,
	zsa_rxint,
	zsa_srint,
	zsa_softint,
	zsa_suspend,
	zsa_resume
};

extern dev_info_t *zs_get_dev_info(dev_t dev, int otyp);

static int	dmtozs(int bits);
static int	zstodm(int bits);
static void	zsa_restart(void *);
static void	zsa_reioctl(void *);
static void	zsa_ioctl(struct asyncline *za, queue_t *q, mblk_t *mp);
static void	zsa_program(struct asyncline *za, int setibaud);
static void	zsa_start(struct zscom *zs);
static void 	zsa_kick_rcv(void *);
static void 	zsa_callback(void *);
static void	zsa_set_za_rcv_flags_mask(struct asyncline *za);
static void	zs_ack(mblk_t *mp, mblk_t *dp, uint size);
static void	zs_sendnak(queue_t *q, mblk_t *mp, int err);
static void	zs_copy(mblk_t *mp, mblk_t *dp, uint size, unsigned char type);
int		zsgetspeed(dev_t dev);

static boolean_t abort_charseq_recognize(u_char ch);

/* ARGSUSED */
int
zsc_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
void **result)
{
	register dev_t dev = (dev_t)arg;
	register int instance, error;
	register struct zscom *zs;

	if ((instance = UNIT(dev)) >= nzs)
		return (DDI_FAILURE);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		zs = &zscom[instance];
		*result = (void *) zs->zs_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * The Asynchronous Driver.
 */

/*
 * Determine if the zsminor device is in use as either a stdin or stdout
 * device, so we can be careful about how we initialize the DUART, if
 * it is, in fact, in use.
 *
 * Since this is expensive, we do it once and store away the answers,
 * since this gets called a number of times per phyical zs device.
 * Perhaps, this should be in a loadable module, so it can get thrown
 * away after all the zs devices are attached?
 */

/*
 * To determine if a given unit is being used by the PROM,
 * we need to map stdin/stdout devices as known to the PROM
 * to zs internal minor device numbers:
 *
 * PROM (real device)	zs minor	device
 *
 * "zs", 0, "a"		 0		ttya
 * "zs", 0, "b"		 1		ttyb
 * "zs", 1, "a"		 2		keyboard
 * "zs", 1, "b"		 3		mouse
 * "zs", 2, "a"		 4		ttyc
 * "zs", 2, "b"		 5		ttyd
 *
 * The following value mapping lines assume that insource
 * and outsink map as "screen, a, b, c, d, ...", and that
 * zs minors are "a, b, kbd, mouse, c, d, ...".
 */

static int zsa_inuse;		/* Strictly for debugging */

int
zsa_channel_is_active_in_rom(dev_info_t *dev, int zsminor)
{
	char pathname[OBP_MAXPATHLEN];
	char default_pathname[OBP_MAXPATHLEN];
	char *stdioname;
	char minordata[3];

	/*
	 * Basically, get my name and compare it to stdio devnames
	 * and if we get a match, then the device is in use as either
	 * stdin or stdout device (console tip line or keyboard device).
	 *
	 * We get two forms of the pathname, one complete with the
	 * the channel number, and if the channel is 'a', then
	 * we also deal with the user's ability to default to
	 * channel 'a', by omitting the channel number option.
	 * We then compare these pathnames to both the stdin and
	 * stdout pathnames. If any of these match, then the channel
	 * is in use.
	 */

	(void) ddi_pathname(dev, pathname);	/* device pathname */
	default_pathname[0] = (char)0;	/* default pathname if channel 'a' */
	if ((zsminor & 1) == 0)
		(void) strcpy(default_pathname, pathname);
	minordata[0] = ':';
	minordata[1] = (char)('a' + (zsminor & 1));
	minordata[2] = (char)0;
	(void) strcat(pathname, minordata);

	stdioname = prom_stdinpath();
	if (strcmp(pathname, stdioname) == 0) {
		zsa_inuse |= (1 << zsminor);
		return (1);
	}
	if (strcmp(default_pathname, stdioname) == 0) {
		zsa_inuse |= (1 << zsminor);
		return (1);
	}

	stdioname = prom_stdoutpath();
	if (strcmp(pathname, stdioname) == 0) {
		zsa_inuse |= (1 << zsminor);
		return (1);
	}
	if (strcmp(default_pathname, stdioname) == 0) {
		zsa_inuse |= (1 << zsminor);
		return (1);
	}

	return (0);
}

/*
 * Initialize zs
 */
void
zsa_init(struct zscom *zs)
{

	/*
	 * This routine is called near the end of the zs module's attach
	 * process. It initializes the TTY protocol-private data for this
	 * channel that needs to be in place before interrupts are enabled.
	 */
	mutex_enter(zs->zs_excl);
	mutex_enter(zs->zs_excl_hi);

	/*
	 * Raise modem control lines on serial ports associated
	 * with the console and (optionally) softcarrier lines.
	 * Drop modem control lines on all others so that modems
	 * will not answer and portselectors will skip these
	 * lines until they are opened by a getty.
	 */
	if (zsa_channel_is_active_in_rom(zs->zs_dip, zs->zs_unit))
		(void) zsmctl(zs, ZS_ON, DMSET);	/* raise dtr */
	else if (zsasoftdtr && (zssoftCAR[zs->zs_unit]))
		(void) zsmctl(zs, ZS_ON, DMSET);	/* raise dtr */
	else
		(void) zsmctl(zs, ZS_OFF, DMSET);	/* drop dtr */

	if (zsa_rstandby > ZSA_MAX_RSTANDBY)
		zsa_rstandby = ZSA_MAX_RSTANDBY;

	if (zsa_rdone > ZSA_RDONE_MAX)
		zsa_rdone = ZSA_RDONE_MAX;

	if (zsa_grace_flow_control > ZSA_GRACE_MAX_FLOW_CONTROL)
		zsa_grace_flow_control = ZSA_GRACE_MAX_FLOW_CONTROL;

	mutex_exit(zs->zs_excl_hi);
	mutex_exit(zs->zs_excl);
#ifdef lint
	if (nzs == 0)
		nzs = 2;
#endif lint

}


/*
 * Open routine.
 */

/*ARGSUSED*/
static int
zsa_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	register struct zscom *zs;
	register struct asyncline *za;
	register int	speed, unit;
	struct termios *termiosp;
	int len;
	register int allocbcount = zsa_rstandby;
	register set_zsoptinit = 0;

	unit = UNIT(*dev);
	if (unit >= nzs)
		return (ENXIO);		/* unit not configured */

	zs = &zscom[unit];
	if (zs == NULL) {
		printf("zs: device not found by autoconfig\n");
		return (ENXIO);	 /* device not found by autoconfig */
	}

	if (zs->zs_ops == NULL) {
		return (ENXIO);	 /* device not found by autoconfig */
	}

	mutex_enter(zs->zs_ocexcl);
	mutex_enter(zs->zs_excl);
again:
	if ((zs->zs_ops != &zsops_null) &&
	    (zs->zs_ops != &zsops_async)) {
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		return (EBUSY);	 /* another protocol got here first */
	}

	za = (struct asyncline *)&zs->zs_priv_str;

	if (zs->zs_suspended) {
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		(void) ddi_dev_is_needed(zs->zs_dip, 0, 1);
		mutex_enter(zs->zs_ocexcl);
		mutex_enter(zs->zs_excl);
	}

	/* Mark device as busy (for power management) */
	(void) pm_busy_component(zs->zs_dip, unit%2+1);

	if (zs->zs_ops == &zsops_null) {
		bzero(za, sizeof (zs->zs_priv_str));
		za->za_common = zs;
		if (zssoftCAR[zs->zs_unit])
			za->za_ttycommon.t_flags |= TS_SOFTCAR;
		zsopinit(zs, &zsops_async);
		set_zsoptinit = 1;
		za->za_rdone_wptr = 0;
		za->za_rdone_rptr = 0;
	}

	zs->zs_priv = (caddr_t)za;

	/*
	 * Block waiting for carrier to come up,
	 * unless this is a no-delay open.
	 */
	mutex_enter(zs->zs_excl_hi);
	if (!(za->za_flags & ZAS_ISOPEN)) {
		/*
		 * Get the default termios settings (cflag).
		 * These are stored as a property in the
		 * "options" node.
		 */
		mutex_exit(zs->zs_excl_hi);
		if (ddi_getlongprop(DDI_DEV_T_ANY,
		    ddi_root_node(), 0, "ttymodes",
		    (caddr_t)&termiosp, &len) == DDI_PROP_SUCCESS &&
		    len == sizeof (struct termios)) {

			za->za_ttycommon.t_cflag = termiosp->c_cflag;
			kmem_free(termiosp, len);
		} else {
			/*
			 * Gack! Whine about it.
			 */
			cmn_err(CE_WARN,
			    "zs: Couldn't get ttymodes property!");
		}
		mutex_enter(zs->zs_excl_hi);
		if ((*dev == rconsdev) || (*dev == kbddev) ||
		    (*dev == stdindev)) {
			speed = zsgetspeed(*dev);
			za->za_ttycommon.t_cflag &= ~(CBAUD);
			if (speed > CBAUD) {
				za->za_ttycommon.t_cflag |= CBAUDEXT;
				za->za_ttycommon.t_cflag |=
					((speed - CBAUD - 1) & CBAUD);
			} else {
				za->za_ttycommon.t_cflag &= ~CBAUDEXT;
				za->za_ttycommon.t_cflag |= (speed & CBAUD);
			}
		}
		za->za_overrun = 0;
		za->za_ttycommon.t_iflag = 0;
		za->za_ttycommon.t_iocpending = NULL;
		za->za_ttycommon.t_size.ws_row = 0;
		za->za_ttycommon.t_size.ws_col = 0;
		za->za_ttycommon.t_size.ws_xpixel = 0;
		za->za_ttycommon.t_size.ws_ypixel = 0;
		za->za_dev = *dev;
		za->za_wbufcid = 0;
		zsa_program(za, za->za_ttycommon.t_cflag & (CIBAUDEXT|CIBAUD));
		zsa_set_za_rcv_flags_mask(za);
	} else if ((za->za_ttycommon.t_flags & TS_XCLUDE) && !drv_priv(cr)) {
		mutex_exit(zs->zs_excl_hi);
		if (set_zsoptinit && !(za->za_flags & ISOPEN))
			zsopinit(zs, &zsops_null);
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		return (EBUSY);
	} else if ((*dev & OUTLINE) && !(za->za_flags & ZAS_OUT)) {
		mutex_exit(zs->zs_excl_hi);
		if (set_zsoptinit && !(za->za_flags & ISOPEN))
			zsopinit(zs, &zsops_null);
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		return (EBUSY);
	}

	if (*dev & OUTLINE)
		za->za_flags |= ZAS_OUT;
	(void) zsmctl(zs, ZS_ON, DMSET);

	/*
	 * Check carrier.
	 */
	if ((za->za_ttycommon.t_flags & TS_SOFTCAR) ||
	    (zsmctl(zs, 0, DMGET) & ZSRR0_CD))
		za->za_flags |= ZAS_CARR_ON;
	mutex_exit(zs->zs_excl_hi);

	/*
	 * If FNDELAY and FNONBLOCK are clear, block until carrier up.
	 * Quit on interrupt.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK)) &&
	    !(za->za_ttycommon.t_cflag & CLOCAL)) {
		if (!(za->za_flags & (ZAS_CARR_ON|ZAS_OUT)) ||
		    ((za->za_flags & ZAS_OUT) && !(*dev & OUTLINE))) {
			za->za_flags |= ZAS_WOPEN;
			mutex_exit(zs->zs_excl);
			if (cv_wait_sig(&zs->zs_flags_cv, zs->zs_ocexcl) == 0) {
				mutex_enter(zs->zs_excl);
				if (zs->zs_suspended) {
					mutex_exit(zs->zs_excl);
					mutex_exit(zs->zs_ocexcl);
					(void) ddi_dev_is_needed(zs->zs_dip,
									0, 1);
					mutex_enter(zs->zs_ocexcl);
					mutex_enter(zs->zs_excl);
				}
				za->za_flags &= ~ZAS_WOPEN;
				if (set_zsoptinit && !(za->za_flags & ISOPEN))
					zsopinit(zs, &zsops_null);
				mutex_exit(zs->zs_excl);
				mutex_exit(zs->zs_ocexcl);
				return (EINTR);
			}
			mutex_enter(zs->zs_excl);
			za->za_flags &= ~ZAS_WOPEN;
			if ((zs->zs_ops == &zsops_null) ||
			    (zs->zs_ops == &zsops_async))
				goto again;
			else {
				if (set_zsoptinit && !(za->za_flags & ISOPEN))
					zsopinit(zs, &zsops_null);
				mutex_exit(zs->zs_excl);
				mutex_exit(zs->zs_ocexcl);
				return (EBUSY);
			}
		}
	} else if ((za->za_flags & ZAS_OUT) && !(*dev & OUTLINE)) {
		if (set_zsoptinit && !(za->za_flags & ISOPEN))
			zsopinit(zs, &zsops_null);
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		return (EBUSY);
	}

	za->za_ttycommon.t_readq = rq;
	za->za_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)za;

	za->za_flags |= ZAS_ISOPEN;
	ZSA_GETBLOCK(zs, allocbcount);
	qprocson(rq);
	mutex_exit(zs->zs_excl);
	mutex_exit(zs->zs_ocexcl);
	return (0);
}

/*
 * Close routine.
 */

/*ARGSUSED*/
static int
zsa_close(queue_t *q, int flag)
{
	struct asyncline *za;
	struct zscom *zs;
	int i;
	mblk_t *bp;
	timeout_id_t za_zsa_restart_id, za_kick_rcv_id;
	bufcall_id_t za_bufcid, za_wbufcid;
	int	 tmp;

	if ((za = (struct asyncline *)q->q_ptr) == NULL) {
		cmn_err(CE_WARN, "zs: already been closed once");
		return (ENODEV);	/* already been closed once */
	}

	zs = za->za_common;

	mutex_enter(zs->zs_ocexcl);
	mutex_enter(zs->zs_excl);

	/*
	 * If we still have carrier, wait here until all the data is gone;
	 * if interrupted in close, ditch the data and continue onward.
	 */
	while ((za->za_flags & ZAS_CARR_ON) && ((zs->zs_wr_cur) ||
	    (za->za_flags & (ZAS_BUSY|ZAS_DELAY|ZAS_BREAK)))) {
		mutex_exit(zs->zs_excl);
		if (cv_wait_sig(&lbolt_cv, zs->zs_ocexcl) == 0) {
			mutex_enter(zs->zs_excl);
			break;
		}
		mutex_enter(zs->zs_excl);
	}
	if (zs->zs_suspended) {
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		(void) ddi_dev_is_needed(zs->zs_dip, 0, 1);
		mutex_enter(zs->zs_ocexcl);
		mutex_enter(zs->zs_excl);
	}

	/*
	 * If break is in progress, stop it.
	 */
	mutex_enter(zs->zs_excl_hi);
	if (zs->zs_wreg[5] & ZSWR5_BREAK) {
		SCC_BIC(5, ZSWR5_BREAK);
		za->za_flags &= ~ZAS_BREAK;
	}

	za_wbufcid = za->za_wbufcid;
	za_bufcid = za->za_bufcid;
	za_zsa_restart_id = za->za_zsa_restart_id;
	za_kick_rcv_id = za->za_kick_rcv_id;

	za->za_wbufcid = za->za_bufcid = 0;
	za->za_zsa_restart_id = za->za_kick_rcv_id = 0;

	/*
	 * If line has HUPCL set or is incompletely opened,
	 * and it is not the console or the keyboard,
	 * fix up the modem lines.
	 */

	zsopinit(zs, &zsops_null_async);

	/*
	 * Nobody, zsh or zs can now open this port until
	 * zsopinit(zs, &zsops_null);
	 *
	 */

	if ((za->za_dev != rconsdev) && (za->za_dev != kbddev) &&
	    (za->za_dev != stdindev) &&
	    (((za->za_flags & (ZAS_WOPEN|ZAS_ISOPEN)) != ZAS_ISOPEN) ||
	    (za->za_ttycommon.t_cflag & HUPCL))) {
		/*
		 * If DTR is being held high by softcarrier,
		 * set up the ZS_ON set; if not, hang up.
		 */
		if (zsasoftdtr && (za->za_ttycommon.t_flags & TS_SOFTCAR))
			(void) zsmctl(zs, ZS_ON, DMSET);
		else
			(void) zsmctl(zs, ZS_OFF, DMSET);
		mutex_exit(zs->zs_excl_hi);
		/*
		 * Don't let an interrupt in the middle of close
		 * bounce us back to the top; just continue
		 * closing as if nothing had happened.
		 */
		mutex_exit(zs->zs_excl);
		tmp = cv_wait_sig(&lbolt_cv, zs->zs_ocexcl);
		mutex_enter(zs->zs_excl);
		if (zs->zs_suspended) {
			mutex_exit(zs->zs_excl);
			mutex_exit(zs->zs_ocexcl);
			(void) ddi_dev_is_needed(zs->zs_dip, 0, 1);
			mutex_enter(zs->zs_ocexcl);
			mutex_enter(zs->zs_excl);
		}
		if (tmp == 0)
			goto out;
		mutex_enter(zs->zs_excl_hi);
	}

	/*
	 * If nobody's now using it, turn off receiver interrupts.
	 */
	if ((za->za_flags & (ZAS_ISOPEN|ZAS_WOPEN)) == 0)
		SCC_BIC(1, ZSWR1_RIE);
	mutex_exit(zs->zs_excl_hi);

out:
	/*
	 * Clear out device state.
	 */
	ttycommon_close(&za->za_ttycommon);

	za->za_ttycommon.t_readq = NULL;
	za->za_ttycommon.t_writeq = NULL;

	mutex_enter(zs->zs_excl_hi);
	za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
	zs->zs_wr_cur = NULL;
	zs->zs_wr_lim = NULL;
	bp = za->za_xmitblk;
	za->za_xmitblk = NULL;
	mutex_exit(zs->zs_excl_hi);
	if (bp)
		freemsg(bp);

	mutex_enter(zs->zs_excl_hi);
	zs->zs_rd_cur = NULL;
	zs->zs_rd_lim = NULL;
	bp = za->za_rcvblk;
	za->za_rcvblk = NULL;
	mutex_exit(zs->zs_excl_hi);
	if (bp)
		freemsg(bp);

	for (i = 0; i < zsa_rstandby; i++) {
		mutex_enter(zs->zs_excl_hi);
		bp = za->za_rstandby[i];
		za->za_rstandby[i] = NULL;
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			freemsg(bp);
	}

	while (za->za_soft_active || za->za_kick_active) {
		mutex_exit(zs->zs_excl);
		(void) cv_wait_sig(&lbolt_cv, zs->zs_ocexcl);
		mutex_enter(zs->zs_excl);
	}
	if (zs->zs_suspended) {
		mutex_exit(zs->zs_excl);
		mutex_exit(zs->zs_ocexcl);
		(void) ddi_dev_is_needed(zs->zs_dip, 0, 1);
		mutex_enter(zs->zs_ocexcl);
		mutex_enter(zs->zs_excl);
	}

	ZSA_FLUSHQ;
	bzero(za, sizeof (struct asyncline));
	mutex_exit(zs->zs_excl);
	mutex_exit(zs->zs_ocexcl);

	/*
	 * Cancel outstanding "bufcall" request.
	 */
	if (za_wbufcid)
		unbufcall(za_wbufcid);
	if (za_bufcid)
		unbufcall(za_bufcid);

	/*
	 * Cancel outstanding timeout.
	 */
	if (za_zsa_restart_id)
		(void) untimeout(za_zsa_restart_id);

	if (za_kick_rcv_id)
		(void) untimeout(za_kick_rcv_id);

	qprocsoff(q);
	q->q_ptr = WR(q)->q_ptr = NULL;
	zsopinit(zs, &zsops_null);
	mutex_enter(zs->zs_ocexcl);
	cv_broadcast(&zs->zs_flags_cv);
	mutex_exit(zs->zs_ocexcl);

	/* Mark device as available for power management */
	(void) pm_idle_component(zs->zs_dip, zs->zs_unit%2+1);
	return (0);
}

/*
 * Put procedure for write queue.
 * Respond to M_STOP, M_START, M_IOCTL, and M_FLUSH messages here;
 * set the flow control character for M_STOPI and M_STARTI messages;
 * queue up M_BREAK, M_DELAY, and M_DATA messages for processing
 * by the start routine, and then call the start routine; discard
 * everything else. Note that this driver does not incorporate any
 * mechanism to negotiate to handle the canonicalization process.
 * It expects that these functions are handled in upper module(s),
 * as we do in ldterm.
 */
static int
zsa_wput(queue_t *q, mblk_t *mp)
{
	register struct asyncline	*za;
	register struct zscom		*zs;
	register struct copyresp	*resp;
	register mblk_t			*bp = NULL;

	za = (struct asyncline *)q->q_ptr;
	zs = za->za_common;
	if (zs->zs_flags_need) {
		zs->zs_flags_need = 0;
		(void) zsa_softint(zs);
	}

	switch (mp->b_datap->db_type) {

	case M_STOP:
		/*
		 * Since we don't do real DMA, we can just let the
		 * chip coast to a stop after applying the brakes.
		 */
		mutex_enter(zs->zs_excl);
		mutex_enter(zs->zs_excl_hi);
		za->za_flags |= ZAS_STOPPED;
		if ((zs->zs_wr_cur) != NULL) {
			za->za_flags &= ~ZAS_BUSY;
			za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			bp = za->za_xmitblk;
			bp->b_rptr = zs->zs_wr_cur;
			zs->zs_wr_cur = NULL;
			zs->zs_wr_lim = NULL;
			za->za_xmitblk = NULL;
		}
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			(void) putbq(q, bp);
		freemsg(mp);
		mutex_exit(zs->zs_excl);
		break;

	case M_START:
		mutex_enter(zs->zs_excl);
		if (za->za_flags & ZAS_STOPPED) {
			za->za_flags &= ~ZAS_STOPPED;
			/*
			 * If an output operation is in progress,
			 * resume it. Otherwise, prod the start
			 * routine.
			 */
			zsa_start(zs);
		}
		freemsg(mp);
		mutex_exit(zs->zs_excl);
		break;

	case M_IOCTL:
		mutex_enter(zs->zs_excl);
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {

		case TIOCGPPS:
		{
			/*
			 * Get PPS state.
			 */
			struct iocblk *iocp = (struct iocblk *)mp->b_rptr;

			if (mp->b_cont != NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_error = EINVAL;
				ZSA_QREPLY(q, mp);
				break;
			}
			bp = allocb(sizeof (int), BPRI_HI);
			if (bp == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_error = ENOMEM;
				ZSA_QREPLY(q, mp);
				break;
			}
			mp->b_cont = bp;
			if (za->za_pps)
				*(int *)bp->b_wptr = 1;
			else
				*(int *)bp->b_wptr = 0;
			bp->b_wptr += sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = sizeof (int);
			ZSA_QREPLY(q, mp);
			break;
		}

		case TIOCSPPS:
			/*
			 * Set PPS state.
			 */
			if (mp->b_cont == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				((struct iocblk *)mp->b_rptr)->ioc_error =
				    EINVAL;
			} else {
				za->za_pps = (*(int *)mp->b_cont->b_rptr != 0);
				mp->b_datap->db_type = M_IOCACK;
			}
			ZSA_QREPLY(q, mp);
			break;

		case TIOCGPPSEV:
		{
			/*
			 * Get PPS event data.
			 */
			struct iocblk *iocp = (struct iocblk *)mp->b_rptr;
			void *buf;
#ifdef _SYSCALL32_IMPL
			struct ppsclockev32 p32;
#endif

			if (mp->b_cont != NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_error = EINVAL;
				ZSA_QREPLY(q, mp);
				break;
			}
			if (za->za_pps == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_error = ENXIO;
				ZSA_QREPLY(q, mp);
				break;
			}

#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				TIMEVAL_TO_TIMEVAL32(&p32.tv, &ppsclockev.tv);
				p32.serial = ppsclockev.serial;
				buf = &p32;
				iocp->ioc_count = sizeof (struct ppsclockev32);
			} else
#endif
			{
				buf = &ppsclockev;
				iocp->ioc_count = sizeof (struct ppsclockev);
			}

			if ((bp = allocb(iocp->ioc_count, BPRI_HI)) == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_error = ENOMEM;
				ZSA_QREPLY(q, mp);
				break;
			}
			mp->b_cont = bp;

			bcopy(buf, bp->b_wptr, iocp->ioc_count);
			bp->b_wptr += iocp->ioc_count;
			mp->b_datap->db_type = M_IOCACK;
			ZSA_QREPLY(q, mp);
			break;
		}

		case TCSETSW:
		case TCSETSF:
		case TCSETAW:
		case TCSETAF:
		case TCSBRK:
			/*
			 * The changes do not take effect until all
			 * output queued before them is drained.
			 * Put this message on the queue, so that
			 * "zsa_start" will see it when it's done
			 * with the output before it. Poke the
			 * start routine, just in case.
			 */
			(void) putq(q, mp);
			zsa_start(zs);
			break;

		default:
			/*
			 * Do it now.
			 */
			zsa_ioctl(za, q, mp);
			break;
		}
		mutex_exit(zs->zs_excl);
		break;


	case	M_IOCDATA:

		mutex_enter(zs->zs_excl);
		resp = (struct copyresp *)mp->b_rptr;
		if (resp->cp_rval) {
			/*
			 * Just free message on failure.
			 */
			freemsg(mp);
			mutex_exit(zs->zs_excl);
			break;
		}
		switch (resp->cp_cmd) {

		case	TIOCMSET:
			mutex_enter(zs->zs_excl_hi);
			(void) zsmctl(zs, dmtozs(*(int *)mp->b_cont->b_rptr),
			    DMSET);
			mutex_exit(zs->zs_excl_hi);
			zs_ack(mp, (mblk_t *)NULL, 0);
			ZSA_QREPLY(q, mp);
			break;

		case	TIOCMBIS:
			mutex_enter(zs->zs_excl_hi);
			(void) zsmctl(zs, dmtozs(*(int *)mp->b_cont->b_rptr),
			    DMBIS);
			mutex_exit(zs->zs_excl_hi);
			zs_ack(mp, (mblk_t *)NULL, 0);
			ZSA_QREPLY(q, mp);
			break;

		case	TIOCMBIC:
			mutex_enter(zs->zs_excl_hi);
			(void) zsmctl(zs, dmtozs(*(int *)mp->b_cont->b_rptr),
			    DMBIC);
			mutex_exit(zs->zs_excl_hi);
			zs_ack(mp, (mblk_t *)NULL, 0);
			ZSA_QREPLY(q, mp);
			break;

		case	TIOCMGET:
			zs_ack(mp, (mblk_t *)NULL, 0);
			ZSA_QREPLY(q, mp);
			break;

		default:
			freemsg(mp);

		}
		mutex_exit(zs->zs_excl);
		break;


	case M_FLUSH:
		mutex_enter(zs->zs_excl);
		if (*mp->b_rptr & FLUSHW) {

			/*
			 * Abort any output in progress.
			 */
			if (za->za_flags & ZAS_BUSY) {
				za->za_flags &= ~ZAS_BUSY;
				mutex_enter(zs->zs_excl_hi);
				za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
				zs->zs_wr_cur = NULL;
				zs->zs_wr_lim = NULL;
				bp = za->za_xmitblk;
				za->za_xmitblk = NULL;
				mutex_exit(zs->zs_excl_hi);
				if (bp)
					freemsg(bp);
			}
			/*
			 * Flush our write queue.
			 */
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			*mp->b_rptr &= ~FLUSHW;	/* it has been flushed */
		}
		if (*mp->b_rptr & FLUSHR) {
			/*
			 * Flush any data in the temporary receive buffer
			 */
			mutex_enter(zs->zs_excl_hi);
			if ((za->za_ttycommon.t_flags & TS_SOFTCAR) ||
			    (SCC_READ0() & ZSRR0_CD)) {
				ZSA_KICK_RCV;
			} else {
			    ZSA_KICK_RCV;
			    if (!(SCC_READ0() & ZSRR0_RX_READY)) {
				/*
				 * settle time for 1 character shift
				 */
				mutex_exit(zs->zs_excl_hi);
				mutex_exit(zs->zs_excl);
				(void) delay(ztdelay(
				    SPEED(za->za_ttycommon.t_cflag))/3 + 1);
				mutex_enter(zs->zs_excl);
				mutex_enter(zs->zs_excl_hi);
				if (!(SCC_READ0() & ZSRR0_CD))
					ZSA_KICK_RCV;
			    }
			    while ((SCC_READ0() &
				(ZSRR0_CD|ZSRR0_RX_READY)) == ZSRR0_RX_READY) {
				/*
				 * Empty Receiver
				 */
				(void) SCC_READDATA();
			    }
			}
			mutex_exit(zs->zs_excl_hi);
			flushq(RD(q), FLUSHDATA);
			ZSA_QREPLY(q, mp);
			/*
			 * give the read queues a crack at it
			 */
		} else
			freemsg(mp);

		/*
		 * We must make sure we process messages that survive the
		 * write-side flush. Without this call, the close protocol
		 * with ldterm can hang forever.  (ldterm will have sent us a
		 * TCSBRK ioctl that it expects a response to.)
		 */
		zsa_start(zs);
		mutex_exit(zs->zs_excl);
		break;

	case M_BREAK:
	case M_DELAY:
	case M_DATA:
		mutex_enter(zs->zs_excl);
		/*
		 * Queue the message up to be transmitted,
		 * and poke the start routine.
		 */
		(void) putq(q, mp);
		zsa_start(zs);
		mutex_exit(zs->zs_excl);
		break;

	case M_STOPI:
		mutex_enter(zs->zs_excl);
		mutex_enter(zs->zs_excl_hi);
		za->za_flowc = za->za_ttycommon.t_stopc;
		if ((zs->zs_wr_cur) != NULL) {
			za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			bp = za->za_xmitblk;
			bp->b_rptr = zs->zs_wr_cur;
			zs->zs_wr_cur = NULL;
			zs->zs_wr_lim = NULL;
			za->za_xmitblk = NULL;
		}
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			(void) putbq(q, bp);
		else
			zsa_start(zs);		/* poke the start routine */
		freemsg(mp);
		mutex_exit(zs->zs_excl);
		break;

	case M_STARTI:
		mutex_enter(zs->zs_excl);
		mutex_enter(zs->zs_excl_hi);
		za->za_flowc = za->za_ttycommon.t_startc;
		if ((zs->zs_wr_cur) != NULL) {
			za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			bp = za->za_xmitblk;
			bp->b_rptr = zs->zs_wr_cur;
			zs->zs_wr_cur = NULL;
			zs->zs_wr_lim = NULL;
			za->za_xmitblk = NULL;
		}
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			(void) putbq(q, bp);
		else
			zsa_start(zs);		/* poke the start routine */
		freemsg(mp);
		mutex_exit(zs->zs_excl);
		break;

	case M_CTL:
		/*
		 * These MC_SERVICE type messages are used by upper
		 * modules to tell this driver to send input up
		 * immediately, or that it can wait for normal
		 * processing that may or may not be done. Sun
		 * requires these for the mouse module.
		 */
		mutex_enter(zs->zs_excl);
		switch (*mp->b_rptr) {

		case MC_SERVICEIMM:
			mutex_enter(zs->zs_excl_hi);
			za->za_flags |= ZAS_SERVICEIMM;
			mutex_exit(zs->zs_excl_hi);
			break;

		case MC_SERVICEDEF:
			mutex_enter(zs->zs_excl_hi);
			za->za_flags &= ~ZAS_SERVICEIMM;
			mutex_exit(zs->zs_excl_hi);
			break;
		}
		freemsg(mp);
		mutex_exit(zs->zs_excl);
		break;

	default:
		/*
		 * "No, I don't want a subscription to Chain Store Age,
		 * thank you anyway."
		 */
		freemsg(mp);
		break;
	}
	return (0);
}

/*
 * zs read service procedure
 */
static int
zsa_rsrv(queue_t *q)
{
	register struct asyncline	*za;
	register struct zscom		*zs;

	if (((za = (struct asyncline *)q->q_ptr) != NULL) &&
	    (za->za_ttycommon.t_cflag & CRTSXOFF)) {
		zs = za->za_common;
		mutex_enter(zs->zs_excl_hi);
		ZSSETSOFT(zs);
		mutex_exit(zs->zs_excl_hi);
	}
	return (0);
}

/*
 * Transmitter interrupt service routine.
 * If there's more data to transmit in the current pseudo-DMA block,
 * and the transmitter is ready, send the next character if output
 * is not stopped or draining.
 * Otherwise, queue up a soft interrupt.
 */
static void
zsa_txint(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register u_char	 *wr_cur;
	register u_char s0;

	s0 = SCC_READ0();

	if ((wr_cur = zs->zs_wr_cur) != NULL) {
		if (wr_cur < zs->zs_wr_lim) {
			if ((za->za_ttycommon.t_cflag & CRTSCTS) &&
			    !(s0 & ZSRR0_CTS)) {
				SCC_WRITE0(ZSWR0_RESET_TXINT);
				za->za_rcv_flags_mask |= DO_RETRANSMIT;
				return;
			}
			SCC_WRITEDATA(*wr_cur++);
#ifdef ZSA_DEBUG
			za->za_wr++;
#endif
			zs->zs_wr_cur = wr_cur;
			return;
		} else {
			zs->zs_wr_cur = NULL;
			zs->zs_wr_lim = NULL;
			/*
			 * Use the rcv_flags_mask as it is set and
			 * test while holding the zs_excl_hi mutex
			 */
			za->za_rcv_flags_mask |= DO_TRANSMIT;
			SCC_WRITE0(ZSWR0_RESET_TXINT);
			ZSSETSOFT(zs);
			return;
		}
	}

	if (za->za_flowc != '\0' && (!(za->za_flags & ZAS_DRAINING))) {
		if ((za->za_ttycommon.t_cflag & CRTSCTS) &&
		    !(s0 & ZSRR0_CTS)) {
			SCC_WRITE0(ZSWR0_RESET_TXINT);
			return;
		}
		SCC_WRITEDATA(za->za_flowc);
		za->za_flowc = '\0';
		return;
	}
	SCC_WRITE0(ZSWR0_RESET_TXINT);
	/*
	 * Set DO_TRANSMIT bit so that the soft interrupt can
	 * test it and unset the ZAS_BUSY in za_flags while holding
	 * the mutex zs_excl and zs_excl_hi
	 */
	za->za_rcv_flags_mask |= DO_TRANSMIT;
	ZSSETSOFT(zs);
}

/*
 * External/Status interrupt.
 */
static void
zsa_xsint(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register u_char s0, x0;

	s0 = SCC_READ0();
	ZSA_R0_LOG(s0);
	x0 = s0 ^ za->za_rr0;
	za->za_rr0 = s0;
	SCC_WRITE0(ZSWR0_RESET_STATUS);

	/*
	 * PPS (Pulse Per Second) support.
	 */
	if (za->za_pps && (x0 & ZSRR0_CD) && (s0 & ZSRR0_CD)) {
		/*
		 * This code captures a timestamp at the designated
		 * transition of the PPS signal (CD asserted).  The
		 * code provides a pointer to the timestamp, as well
		 * as the hardware counter value at the capture.
		 *
		 * Note: the kernel has nano based time values while
		 * NTP requires micro based, an in-line fast algorithm
		 * to convert nsec to usec is used here -- see hrt2ts()
		 * in common/os/timers.c for a full description.
		 */
		struct timeval *tvp = &ppsclockev.tv;
		timespec_t ts;
		int nsec, usec;

		LED_OFF;
		gethrestime(&ts);
		LED_ON;
		nsec = ts.tv_nsec;
		usec = nsec + (nsec >> 2);
		usec = nsec + (usec >> 1);
		usec = nsec + (usec >> 2);
		usec = nsec + (usec >> 4);
		usec = nsec - (usec >> 3);
		usec = nsec + (usec >> 2);
		usec = nsec + (usec >> 3);
		usec = nsec + (usec >> 4);
		usec = nsec + (usec >> 1);
		usec = nsec + (usec >> 6);
		tvp->tv_usec = usec >> 10;
		tvp->tv_sec = ts.tv_sec;

		++ppsclockev.serial;

		/*
		 * Because the kernel keeps a high-resolution time, pass the
		 * current highres timestamp in tvp and zero in usec.
		 */
		ddi_hardpps(tvp, 0);
	}

	ZSA_KICK_RCV;

	if ((x0 & ZSRR0_BREAK) && (s0 & ZSRR0_BREAK) == 0) {
#ifdef SLAVIO_BUG
		/*
		 * ZSRR0_BREAK turned off.  This means that the break sequence
		 * has completed (i.e., the stop bit finally arrived).
		 */
		if ((s0 & ZSRR0_RX_READY) == 0) {
			/*
			 * SLAVIO will generate a separate STATUS change
			 * interrupt when the break sequence completes.
			 * SCC will combine both, taking the higher priority
			 * one, the receive.  Should still see the ext/stat.
			 * bit in REG3 on SCC.  If no ext/stat, it must be
			 * a SLAVIO.
			 */
			za->za_breakoff = 1;
		} else {
			/*
			 * The NUL character in the receiver is part of the
			 * break sequence; it is discarded.
			 */
			(void) SCC_READDATA(); /* swallow null */
		}
#else /* SLAVIO_BUG */
		/*
		 * ZSRR0_BREAK turned off.  This means that the break sequence
		 * has completed (i.e., the stop bit finally arrived).  The NUL
		 * character in the receiver is part of the break sequence;
		 * it is discarded.
		 */
		(void) SCC_READDATA(); /* swallow null */
#endif /* SLAVIO_BUG */
		SCC_WRITE0(ZSWR0_RESET_ERRORS);

		/*
		 * Note: this will cause an abort if a break occurs on
		 * the "keyboard device", regardless of whether the
		 * "keyboard device" is a real keyboard or just a
		 * terminal on a serial line. This permits you to
		 * abort a workstation by unplugging the keyboard,
		 * even if the normal abort key sequence isn't working.
		 */
		if ((za->za_dev == kbddev) ||
		    ((za->za_dev == rconsdev) || (za->za_dev == stdindev)) &&
		    (abort_enable != KIOCABORTALTERNATE)) {
			abort_sequence_enter((char *)NULL);
			/*
			 * We just broke into the monitor or debugger,
			 * ignore the break in this case so whatever
			 * random program that was running doesn't get
			 * a SIGINT.
			 */
			return;
		}
		za->za_break = 1;
	}

	/*
	 * If hardware flow control is enabled, (re)start output
	 * when CTS is reasserted.
	 */
	if ((za->za_ttycommon.t_cflag & CRTSCTS) &&
	    (x0 & ZSRR0_CTS) && (s0 & ZSRR0_CTS) &&
	    (za->za_rcv_flags_mask & DO_RETRANSMIT))
			za->za_rcv_flags_mask |= DO_TRANSMIT;

	za->za_ext = 1;
	ZSSETSOFT(zs);
}

/*
 * Receive Interrupt
 */
static void
zsa_rxint(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register u_char c;
	register u_char	*rd_cur = zs->zs_rd_cur;
	register u_char	*rd_lim = zs->zs_rd_lim;
	register mblk_t	*bp;
	register u_int	fm = za->za_rcv_flags_mask;


#ifdef ZSA_DEBUG
	za->za_rd++;
#endif
	c = (fm >> 16) & (SCC_READDATA());

	/*
	 * Check for character break sequence
	 */
	if ((abort_enable == KIOCABORTALTERNATE) && (za->za_dev == rconsdev)) {
		if (abort_charseq_recognize(c))
			abort_sequence_enter((char *)NULL);
	}

	if (!rd_cur) {
#ifdef SLAVIO_BUG
		/*
		 * SLAVIO generates FE for the start of break and
		 * during break when parity is set.  End of break is
		 * detected when the first character is received.
		 * This character is always garbage and is thrown away.
		 */
		if (za->za_slav_break) {
			za->za_slav_break = 0;
			za->za_rr0 |= ZSRR0_BREAK;
			zsa_xsint(zs);
			return;
		}
#endif /* SLAVIO_BUG */

		if (c == 0 && (za->za_rr0 & ZSRR0_BREAK)) {
			/*
			 * A break sequence was under way, and a NUL character
			 * was received. Discard the NUL character, as it is
			 * part of the break sequence; if ZSRR0_BREAK turned
			 * off, indicating that the break sequence has com-
			 * pleted, call "zsa_xsint" to properly handle the
			 * error. It would appear that External/Status
			 * interrupts get lost occasionally, so this ensures
			 * that one is delivered.
			 */
			c = SCC_READ0();
			if (!(c & ZSRR0_BREAK))
				zsa_xsint(zs);
			return;
		}

#ifdef SLAVIO_BUG
		if (c == 0 && za->za_breakoff) {
			/*
			 * A break sequence completed, but SLAVIO generates
			 * the NULL character interrupt late, so we throw the
			 * NULL away now.
			 */
			return;
		}

		/*
		 * make sure it gets cleared.
		 */
		za->za_breakoff = 0;
#endif /* SLAVIO_BUG */

		ZSA_KICK_RCV;	/* We can have M_BREAK msg */
		ZSA_ALLOCB(bp);
		if (!bp) {
			za->za_sw_overrun++;
			ZSSETSOFT(zs);
			return;
		}
		za->za_rcvblk = bp;
		zs->zs_rd_cur = rd_cur = bp->b_wptr;
		zs->zs_rd_lim = rd_lim = bp->b_datap->db_lim;
		if (za->za_kick_rcv_id == 0)
			ZSSETSOFT(zs);
	}
	if (c == 0377 && (fm & DO_ESC)) {
		if (rd_lim < rd_cur + 2) {
			ZSA_ALLOCB(bp);
			ZSA_KICK_RCV;
			if (!bp) {
				za->za_sw_overrun++;
				return;
			}
			za->za_rcvblk = bp;
			zs->zs_rd_cur = rd_cur = bp->b_wptr;
			zs->zs_rd_lim = rd_lim = bp->b_datap->db_lim;
		}
		*rd_cur++ = c;
	}


	*rd_cur++ = c;
	zs->zs_rd_cur = rd_cur;

	if (rd_cur == rd_lim) {
		ZSA_KICK_RCV;
	} else if ((fm & DO_STOPC) && (c == (fm & 0xff))) {
		za->za_do_kick_rcv_in_softint = 1;
		ZSSETSOFT(zs);
	}

	if ((za->za_flags & ZAS_SERVICEIMM) || g_nocluster) {
		/*
		 * Send the data up immediately
		 */
		ZSA_KICK_RCV;
	}
}

/*
 * Special receive condition interrupt handler.
 */
static void
zsa_srint(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register short s1;
	register u_char c;
	register u_char c1;
	register mblk_t *bp = za->za_rcvblk;
	register u_char  *rd_cur = zs->zs_rd_cur;

	SCC_READ(1, s1);
	if (s1 & (ZSRR1_FE | ZSRR1_PE | ZSRR1_DO)) {
		c = SCC_READDATA();	/* swallow bad character */
	}
#ifdef SLAVIO_BUG
	/*
	 * SLAVIO does not handle breaks properly when parity is enabled.
	 *
	 * In general, if a null character is received when a framing
	 * error occurs then it is a break condition and not a real
	 * framing error. The null character must be limited to the
	 * number of bits including the parity bit. For example, a 6
	 * bit character with parity would be null if the lower 7 bits
	 * read from the receive fifo were 0. (The higher order bits are
	 * padded with 1 and/or the stop bits.) The only exception to this
	 * general rule would be an 8 bit null character with parity being
	 * a 1 in the parity bit and a framing error. This exception
	 * can be determined by examining the parity error bit in RREG 1.
	 *
	 * A null character, even parity, 8 bits, no parity error,
	 * (0 0000 0000) with framing error is a break condition.
	 *
	 * A null character, even parity, 8 bits, parity error,
	 * (1 0000 0000) with framing error is a framing error.
	 *
	 * A null character, odd parity, 8 bits, parity error
	 * (0 0000 0000) with framing error is a break condition.
	 *
	 * A null character, odd parity, 8 bits, no parity error,
	 * (1 0000 0000) with framing error is a framing error.
	 */
	if (za->za_ttycommon.t_cflag & PARENB) {
		switch (za->za_ttycommon.t_cflag & CSIZE) {

		case CS5:
			c1 = c & 0x3f;
			break;

		case CS6:
			c1 = c & 0x7f;
			break;

		case CS7:
			c1 = c & 0xff;
			break;

		case CS8:
			if ((za->za_ttycommon.t_cflag & PARODD) &&
			    !(s1 & ZSRR1_PE))
				c1 = 0xff;
			else if (!(za->za_ttycommon.t_cflag & PARODD) &&
			    (s1 & ZSRR1_PE))
				c1 = 0xff;
			else
				c1 = c;
			break;
		}

		/*
		 * We fake start of break condition.
		 */
		if ((s1 & ZSRR1_FE) && c1 == 0) {
			za->za_slav_break = 1;
			return;
		}
	}
#endif /* SLAVIO_BUG */

	if (s1 & ZSRR1_PE) {

		/*
		 * Mark the parity error so zsa_process will
		 * notice it and send it up in an M_BREAK
		 * message; ldterm will do the actual parity error
		 * processing
		 */

		if (bp && zs->zs_rd_cur) {	/* M_DATA msg */
			ZSA_KICK_RCV;
			bp = NULL;
		}
		if (!bp)
			ZSA_ALLOCB(bp);
		if (!bp) {
			za->za_sw_overrun++;
			ZSSETSOFT(zs);
		} else {
			za->za_rcvblk = bp;
			zs->zs_rd_cur = rd_cur = bp->b_wptr;
			zs->zs_rd_lim = bp->b_datap->db_lim;
			*rd_cur++ = c;
			zs->zs_rd_cur = rd_cur;
			bp->b_datap->db_type = M_BREAK;
			if (bp->b_datap->db_lim <= rd_cur)
				ZSA_KICK_RCV;
			za->za_do_kick_rcv_in_softint = 1;
			ZSSETSOFT(zs);

		}
	}
	SCC_WRITE0(ZSWR0_RESET_ERRORS);
	if (s1 & ZSRR1_DO) {
		za->za_hw_overrun++;
		ZSSETSOFT(zs);
	}
}

/*
 * Process software interrupts (or poll)
 * Crucial points:
 * 3.	BUG - breaks are handled "out-of-band" - their relative position
 *	among input events is lost, as well as multiple breaks together.
 *	This is probably not a problem in practice.
 */
static int
zsa_softint(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register u_char	r0;
	register u_char za_kick_active;
	register int	m_error;
	register int	allocbcount = 0;
	register int 	do_ttycommon_qfull = 0;
	register int	hangup = 0, unhangup = 0, m_break = 0;
	register queue_t *q;
	register mblk_t	*bp;
	register mblk_t *head = NULL, *tail = NULL;

	mutex_enter(zs->zs_excl);
	if (zs->zs_suspended) {
		mutex_exit(zs->zs_excl);
		return (0);
	}
	q = za->za_ttycommon.t_readq;
	if (za->za_flags & ZAS_WOPEN && !q) {
		if (za->za_ext) {
			mutex_enter(zs->zs_excl_hi);
			r0 = SCC_READ0();
			za->za_ext = 0;
			mutex_exit(zs->zs_excl_hi);
			/*
			 * carrier up?
			 */
			if ((r0 & ZSRR0_CD) ||
			    (za->za_ttycommon.t_flags & TS_SOFTCAR)) {
				/*
				 *carrier present
				 */
				if ((za->za_flags & ZAS_CARR_ON) == 0) {
					za->za_flags |= ZAS_CARR_ON;
					mutex_exit(zs->zs_excl);
					mutex_enter(zs->zs_ocexcl);
					cv_broadcast(&zs->zs_flags_cv);
					mutex_exit(zs->zs_ocexcl);
					return (0);
				}
			}
		}
		mutex_exit(zs->zs_excl);
		return (0);
	}
	q = za->za_ttycommon.t_readq;
	if (!q) {
		mutex_exit(zs->zs_excl);
		return (0);
	}

	m_error = za->za_m_error;
	za->za_m_error = 0;

	if (za->za_do_kick_rcv_in_softint) {
		mutex_enter(zs->zs_excl_hi);
		ZSA_KICK_RCV;
		za->za_do_kick_rcv_in_softint = 0;
		mutex_exit(zs->zs_excl_hi);
	}

	za_kick_active = za->za_kick_active;

	while (1 && !za_kick_active) {
	    ZSA_SEEQ(bp);
	    if (!bp)
		break;

	    allocbcount++;

	    if (bp->b_datap->db_type <= QPCTL) {
		if (!(canputnext(q))) {
			if (za->za_grace_flow_control >=
				zsa_grace_flow_control) {
			    if (za->za_ttycommon.t_cflag & CRTSXOFF) {
					allocbcount--;
					break;
			    }
			    ZSA_GETQ(bp);
			    freemsg(bp);
			    do_ttycommon_qfull = 1;
			    continue;
			} else
			    za->za_grace_flow_control++;
		} else
			za->za_grace_flow_control = 0;
	    }
	    ZSA_GETQ(bp);
	    if (!head) {
		head = bp;
	    } else {
		if (!tail)
			tail = head;
		tail->b_next = bp;
		tail = bp;
	    }
	}

	if (allocbcount)
		ZSA_GETBLOCK(zs, allocbcount);

	if (za->za_ext) {
		mutex_enter(zs->zs_excl_hi);
		r0 = SCC_READ0();
		za->za_ext = 0;
		/*
		 * carrier up?
		 */
		if ((r0 & ZSRR0_CD) ||
			(za->za_ttycommon.t_flags & TS_SOFTCAR)) {
			/*
			 * carrier present
			 */
			if ((za->za_flags & ZAS_CARR_ON) == 0) {
				za->za_flags |= ZAS_CARR_ON;
				unhangup = 1;
			}
		} else {
			if ((za->za_flags & ZAS_CARR_ON) &&
				!(za->za_ttycommon.t_cflag & CLOCAL)) {
				/*
				 * Carrier went away.
				 * Drop DTR, abort any output in progress,
				 * indicate that output is not stopped, and
				 * send a hangup notification upstream.
				 */
				(void) zsmctl(zs, ZSWR5_DTR, DMBIC);
				if ((za->za_flags & ZAS_BUSY) &&
				    (zs->zs_wr_cur != NULL)) {
				    za->za_xmitblk->b_rptr = zs->zs_wr_cur;
				    zs->zs_wr_cur = NULL;
				    zs->zs_wr_lim = NULL;
				}
				hangup = 1;
				za->za_flags &= ~(ZAS_STOPPED|ZAS_CARR_ON);
				za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			}
		}
		mutex_exit(zs->zs_excl_hi);
	}

	if (za->za_break != 0) {
		mutex_enter(zs->zs_excl_hi);
		r0 = SCC_READ0();
		mutex_exit(zs->zs_excl_hi);
		if ((r0 & ZSRR0_BREAK) == 0) {
			za->za_break = 0;
			m_break = 1;
		}
	}

	/*
	 * If a transmission has finished, indicate that it's
	 * finished, and start that line up again.
	 */

	mutex_enter(zs->zs_excl_hi);
	if (za->za_rcv_flags_mask & DO_TRANSMIT) {
		za->za_rcv_flags_mask &= ~DO_TRANSMIT;
		za->za_flags &= ~ZAS_BUSY;

		if ((za->za_ttycommon.t_cflag & CRTSCTS) &&
		    (za->za_rcv_flags_mask & DO_RETRANSMIT) &&
		    zs->zs_wr_cur)
			bp = NULL;
		else {
			za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			bp = za->za_xmitblk;
			za->za_xmitblk = 0;
		}
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			freemsg(bp);
		zsa_start(zs);
	} else
		mutex_exit(zs->zs_excl_hi);


	/*
	 * A note about these overrun bits: all they do is *tell* someone
	 * about an error- They do not track multiple errors. In fact,
	 * you could consider them latched register bits if you like.
	 * We are only interested in printing the error message once for
	 * any cluster of overrun errrors.
	 */
	if ((!za->za_kick_rcv_id) && (zs->zs_rd_cur || za_kick_active)) {
	    if (g_zsticks)
		za->za_kick_rcv_id = timeout(zsa_kick_rcv, zs, g_zsticks);
	    else
		za->za_kick_rcv_id = timeout(zsa_kick_rcv, zs,
		    zsticks[SPEED(za->za_ttycommon.t_cflag)]);
	    za->za_kick_rcv_count = ZA_KICK_RCV_COUNT;
	}
	za->za_soft_active = 1;
	mutex_exit(zs->zs_excl);

	if (do_ttycommon_qfull) {
		ttycommon_qfull(&za->za_ttycommon, q);
		mutex_enter(zs->zs_excl);
		zsa_start(zs);
		mutex_exit(zs->zs_excl);
	}

	if (za->za_hw_overrun > 10) {
		cmn_err(CE_NOTE, "zs%d: silo overflow\n", UNIT(za->za_dev));
		za->za_hw_overrun = 0;
	}

	if (za->za_sw_overrun > 10) {
		cmn_err(CE_NOTE, "zs%d:ring buffer overflow\n",
		    UNIT(za->za_dev));
		za->za_sw_overrun = 0;
	}

	if (unhangup) {
		mutex_enter(zs->zs_ocexcl);
		cv_broadcast(&zs->zs_flags_cv);
		mutex_exit(zs->zs_ocexcl);
		(void) putnextctl(q, M_UNHANGUP);
	}

	if (m_break)
		(void) putnextctl(q, M_BREAK);

	while (head) {
		if (!tail) {
			putnext(q, head);
			break;
		}
		bp = head;
		head = head->b_next;
		bp->b_next = NULL;
		putnext(q, bp);
	}

	if (hangup)
		(void) putnextctl(q, M_HANGUP);

	if (m_error)
		(void) putnextctl1(q, M_ERROR, m_error);

	za->za_soft_active = 0;

	return (0);
}

/*
 * Start output on a line, unless it's busy, frozen, or otherwise.
 */
static void
zsa_start(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register int cc;
	register queue_t *q;
	register mblk_t *bp;
	u_char *rptr, *wptr;

	/*
	 * If the chip is busy (i.e., we're waiting for a break timeout
	 * to expire, or for the current transmission to finish, or for
	 * output to finish draining from chip), don't grab anything new.
	 */
	if ((za->za_flags & (ZAS_BREAK|ZAS_BUSY|ZAS_DRAINING)) ||
	    zs->zs_suspended)
		return;

	if (za->za_ttycommon.t_cflag & CRTSCTS) {
		mutex_enter(zs->zs_excl_hi);
		if (za->za_rcv_flags_mask & DO_RETRANSMIT) {
			rptr = zs->zs_wr_cur;
			wptr = zs->zs_wr_lim;
			goto zsa_start_retransmit;

		}
		mutex_exit(zs->zs_excl_hi);
	}

	/*
	 * If we have a flow-control character to transmit, do it now.
	 */
	if (za->za_flowc != '\0') {
		mutex_enter(zs->zs_excl_hi);
		if (za->za_ttycommon.t_cflag & CRTSCTS) {
			if ((SCC_READ0() & (ZSRR0_CTS|ZSRR0_TX_READY)) !=
			    (ZSRR0_CTS|ZSRR0_TX_READY)) {
				mutex_exit(zs->zs_excl_hi);
				return;
			}
		} else if (!(SCC_READ0() & ZSRR0_TX_READY)) {
			mutex_exit(zs->zs_excl_hi);
			return;
		}

		ZSDELAY();
		SCC_WRITEDATA(za->za_flowc);
		za->za_flowc = '\0';
		mutex_exit(zs->zs_excl_hi);
		return;
	}

	/*
	 * If we're waiting for a delay timeout to expire, don't grab
	 * anything new.
	 */
	if (za->za_flags & ZAS_DELAY)
		return;

	if ((q = za->za_ttycommon.t_writeq) == NULL)
		return;	/* not attached to a stream */

zsa_start_again:
	for (;;) {
		if ((bp = getq(q)) == NULL)
			return;	/* no data to transmit */

		/*
		 * We have a message block to work on.
		 * Check whether it's a break, a delay, or an ioctl (the latter
		 * occurs if the ioctl in question was waiting for the output
		 * to drain). If it's one of those, process it immediately.
		 */
		switch (bp->b_datap->db_type) {

		case M_BREAK:
			/*
			 * Set the break bit, and arrange for "zsa_restart"
			 * to be called in 1/4 second; it will turn the
			 * break bit off, and call "zsa_start" to grab
			 * the next message.
			 */
			mutex_enter(zs->zs_excl_hi);
			SCC_BIS(5, ZSWR5_BREAK);
			mutex_exit(zs->zs_excl_hi);
			if (!za->za_zsa_restart_id) {
				za->za_zsa_restart_id =
				    timeout(zsa_restart, zs, hz/4);
			}
			za->za_flags |= ZAS_BREAK;
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_DELAY:
			/*
			 * Arrange for "zsa_restart" to be called when the
			 * delay expires; it will turn MTS_DELAY off,
			 * and call "zsa_start" to grab the next message.
			 */
			if (! za->za_zsa_restart_id) {
				za->za_zsa_restart_id = timeout(zsa_restart,
				    zs,
				    (int)(*(unsigned char *)bp->b_rptr + 6));
			}
			za->za_flags |= ZAS_DELAY;
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_IOCTL:
			/*
			 * This ioctl was waiting for the output ahead of
			 * it to drain; obviously, it has. Do it, and
			 * then grab the next message after it.
			 */
			zsa_ioctl(za, q, bp);
			continue;
		default: /* M_DATA */
			goto zsa_start_transmit;
		}

	}
zsa_start_transmit:
	/*
	 * We have data to transmit. If output is stopped, put
	 * it back and try again later.
	 */
	if (za->za_flags & ZAS_STOPPED) {
		(void) putbq(q, bp);
		return;
	}

	za->za_xmitblk = bp;
	rptr = bp->b_rptr;
	wptr = bp->b_wptr;
	cc = wptr - rptr;
	bp = bp->b_cont;
	if (bp != NULL) {
		za->za_xmitblk->b_cont = NULL;
		(void) putbq(q, bp);	/* not done with this message yet */
	}

	if (rptr >= wptr) {
		freeb(za->za_xmitblk);
		za->za_xmitblk = NULL;
		goto zsa_start_again;
	}

	/*
	 * In 5-bit mode, the high order bits are used
	 * to indicate character sizes less than five,
	 * so we need to explicitly mask before transmitting
	 */
	if ((za->za_ttycommon.t_cflag & CSIZE) == CS5) {
		register unsigned char *p = rptr;
		register int cnt = cc;

		while (cnt--)
			*p++ &= (unsigned char) 0x1f;
	}

	/*
	 * Set up this block for pseudo-DMA.
	 */

	mutex_enter(zs->zs_excl_hi);
	zs->zs_wr_cur = rptr;
	zs->zs_wr_lim = wptr;

zsa_start_retransmit:
	za->za_rcv_flags_mask &= ~DO_TRANSMIT;
	if (za->za_ttycommon.t_cflag & CRTSCTS) {
		if ((SCC_READ0() & (ZSRR0_CTS|ZSRR0_TX_READY)) !=
			(ZSRR0_CTS|ZSRR0_TX_READY)) {
			za->za_rcv_flags_mask |= DO_RETRANSMIT;
			za->za_flags |= ZAS_BUSY;
			mutex_exit(zs->zs_excl_hi);
			return;
		}
		za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
	} else if (!(SCC_READ0() & ZSRR0_TX_READY)) {
			za->za_flags |= ZAS_BUSY;
			mutex_exit(zs->zs_excl_hi);
			return;
	}
	/*
	 * If the transmitter is ready, shove the first
	 * character out.
	 */
	ZSDELAY();
	SCC_WRITEDATA(*rptr++);
#ifdef ZSA_DEBUG
	za->za_wr++;
#endif
	zs->zs_wr_cur = rptr;
	za->za_flags |= ZAS_BUSY;
	mutex_exit(zs->zs_excl_hi);
}

/*
 * Restart output on a line after a delay or break timer expired.
 */
static void
zsa_restart(void *arg)
{
	struct zscom *zs = arg;
	struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;

	/*
	 * If break timer expired, turn off the break bit.
	 */
	mutex_enter(zs->zs_excl);
	if (!za->za_zsa_restart_id) {
		mutex_exit(zs->zs_excl);
		return;
	}
	za->za_zsa_restart_id = 0;
	if (za->za_flags & ZAS_BREAK) {
		mutex_enter(zs->zs_excl_hi);
		SCC_BIC(5, ZSWR5_BREAK);
		mutex_exit(zs->zs_excl_hi);
	}
	za->za_flags &= ~(ZAS_DELAY|ZAS_BREAK);
	if (za->za_ttycommon.t_writeq != NULL)
		zsa_start(zs);
	mutex_exit(zs->zs_excl);
}

/*
 * See if the receiver has any data after zs_tick delay
 */
static void
zsa_kick_rcv(void *arg)
{
	struct zscom *zs = arg;
	struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	queue_t *q;
	int	tmp;
	mblk_t	*mp;
	u_char za_soft_active, za_kick_active;
	int	allocbcount = 0;
	int do_ttycommon_qfull = 0;
	mblk_t *head = NULL, *tail = NULL;

	mutex_enter(zs->zs_excl);
	if (!za->za_kick_rcv_id) {
		mutex_exit(zs->zs_excl);
		return;
	}
	za_soft_active = za->za_soft_active;
	za_kick_active = za->za_kick_active;
	q = za->za_ttycommon.t_readq;
	if (!q) {
		mutex_exit(zs->zs_excl);
		return;
	}
	mutex_enter(zs->zs_excl_hi);
	if (zs->zs_rd_cur) {
		ZSA_KICK_RCV;
		za->za_kick_rcv_count = tmp = ZA_KICK_RCV_COUNT;
	} else
		tmp = --za->za_kick_rcv_count;
	if (tmp > 0 || za_soft_active || za_kick_active) {
		mutex_exit(zs->zs_excl_hi);
		if (g_zsticks)
			za->za_kick_rcv_id = timeout(zsa_kick_rcv,
			    zs, g_zsticks);
		else
			za->za_kick_rcv_id = timeout(zsa_kick_rcv,
			    zs, zsticks[SPEED(za->za_ttycommon.t_cflag)]);
		if (za_soft_active || za_kick_active) {
			mutex_exit(zs->zs_excl);
			return;
		}
	} else {
		za->za_kick_rcv_id = 0;
		mutex_exit(zs->zs_excl_hi);
	}


	for (;;) {
	    ZSA_SEEQ(mp);
	    if (!mp)
		break;

	    allocbcount++;

	    if (mp->b_datap->db_type <= QPCTL) {
		if (!(canputnext(q))) {
			if (za->za_grace_flow_control >=
				zsa_grace_flow_control) {
			    if (za->za_ttycommon.t_cflag & CRTSXOFF) {
					allocbcount--;
					break;
			    }
			    ZSA_GETQ(mp);
			    freemsg(mp);
			    do_ttycommon_qfull = 1;
			    continue;
			} else
			    za->za_grace_flow_control++;
		} else
			za->za_grace_flow_control = 0;
	    }
	    ZSA_GETQ(mp);
	    if (!head) {
		head = mp;
	    } else {
		if (!tail)
			tail = head;
		tail->b_next = mp;
		tail = mp;
	    }
	}

	if (allocbcount)
		ZSA_GETBLOCK(zs, allocbcount);

	za->za_kick_active = 1;
	mutex_exit(zs->zs_excl);

	if (do_ttycommon_qfull) {
		ttycommon_qfull(&za->za_ttycommon, q);
		mutex_enter(zs->zs_excl);
		zsa_start(zs);
		mutex_exit(zs->zs_excl);
	}

	while (head) {
		if (!tail) {
			putnext(q, head);
			break;
		}
		mp = head;
		head = head->b_next;
		mp->b_next = NULL;
		putnext(q, mp);

	}
	za->za_kick_active = 0;
}

/*
 * Retry an "ioctl", now that "bufcall" claims we may be able to allocate
 * the buffer we need.
 */
static void
zsa_reioctl(void *arg)
{
	struct asyncline *za = arg;
	struct zscom *zs = za->za_common;
	queue_t *q;
	mblk_t	 *mp;

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(zs->zs_excl);
	if (!za->za_wbufcid) {
		mutex_exit(zs->zs_excl);
		return;
	}
	za->za_wbufcid = 0;
	if ((q = za->za_ttycommon.t_writeq) == NULL) {
		mutex_exit(zs->zs_excl);
		return;
	}
	if ((mp = za->za_ttycommon.t_iocpending) != NULL) {
		/*
		 * not pending any more
		 */
		za->za_ttycommon.t_iocpending = NULL;
		zsa_ioctl(za, q, mp);
	}
	mutex_exit(zs->zs_excl);
}

/*
 * Ack the ioctl
 */
static void
zs_ack(mblk_t *mp, mblk_t *dp, uint size)
{
	register struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;
	if (mp->b_cont != NULL)
		freeb(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
}

/*
 * Nak the ioctl
 */
static void
zs_sendnak(queue_t *q, mblk_t *mp, int err)
{
	register struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;
	register struct asyncline	*za;
	register struct zscom		*zs;

	za = (struct asyncline *)q->q_ptr;
	zs = za->za_common;

	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_count = 0;
	iocp->ioc_error = err;
	ZSA_QREPLY(q, mp);
}

/*
 * Copy message block
 */
static void
zs_copy(mblk_t *mp, mblk_t *dp, uint size, unsigned char type)
{
	register struct copyreq *cp = (struct copyreq *)mp->b_rptr;

	cp->cq_private = NULL;
	cp->cq_flag = 0;
	cp->cq_size = size;
	cp->cq_addr = (caddr_t)(*(long *)(mp->b_cont->b_rptr));
	if (mp->b_cont != NULL)
		freeb(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
	mp->b_datap->db_type = type;
	mp->b_wptr = mp->b_rptr + sizeof (*cp);
}

/*
 * Process an "ioctl" message sent down to us.
 * Note that we don't need to get any locks until we are ready to access
 * the hardware. Nothing we access until then is going to be altered
 * outside of the STREAMS framework, so we should be safe.
 */
static void
zsa_ioctl(struct asyncline *za, queue_t *wq, mblk_t *mp)
{
	register struct zscom *zs = za->za_common;
	register struct iocblk *iocp;
	register unsigned datasize;
	int error;
	register mblk_t *tmp;

	if (za->za_ttycommon.t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(za->za_ttycommon.t_iocpending);
		za->za_ttycommon.t_iocpending = NULL;
	}

	iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * The only way in which "ttycommon_ioctl" can fail is if the "ioctl"
	 * requires a response containing data to be returned to the user,
	 * and no mblk could be allocated for the data.
	 * No such "ioctl" alters our state. Thus, we always go ahead and
	 * do any state-changes the "ioctl" calls for. If we couldn't allocate
	 * the data, "ttycommon_ioctl" has stashed the "ioctl" away safely, so
	 * we just call "bufcall" to request that we be called back when we
	 * stand a better chance of allocating the data.
	 */
	mutex_exit(zs->zs_excl);
	datasize = ttycommon_ioctl(&za->za_ttycommon, wq, mp, &error);
	mutex_enter(zs->zs_excl);
	if (za->za_ttycommon.t_flags & TS_SOFTCAR)
		zssoftCAR[zs->zs_unit] = 1;
	else
		zssoftCAR[zs->zs_unit] = 0;
	if (datasize != 0) {
		if (za->za_wbufcid)
			unbufcall(za->za_wbufcid);
		za->za_wbufcid = bufcall(datasize, BPRI_HI, zsa_reioctl, za);
		return;
	}


	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETS:
		case TCSETSW:
		case TCSETSF:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			mutex_enter(zs->zs_excl_hi);
			zsa_program(za, 1);
			zsa_set_za_rcv_flags_mask(za);
			mutex_exit(zs->zs_excl_hi);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		error = 0;
		/*
		 * If break sequence is going to be generated make
		 * sure that the transmit fifo is empty. The delay
		 * ensures that a 3 byte transmit fifo is empty.
		 */
		if ((iocp->ioc_cmd == TCSBRK) &&
			(*(int *)mp->b_cont->b_rptr == 0)) {
			mutex_exit(zs->zs_excl);
			(void) delay(ztdelay(SPEED(za->za_ttycommon.t_cflag)));
			mutex_enter(zs->zs_excl);
		}

		switch (iocp->ioc_cmd) {

		case TCSBRK:
			if (*(int *)mp->b_cont->b_rptr == 0) {
				/*
				 * Set the break bit, and arrange for
				 * "zsa_restart" to be called in 1/4 second;
				 * it will turn the break bit off, and call
				 * "zsa_start" to grab the next message.
				 */
				mutex_enter(zs->zs_excl_hi);
				SCC_BIS(5, ZSWR5_BREAK);
				if (!za->za_zsa_restart_id) {
					mutex_exit(zs->zs_excl_hi);
					za->za_zsa_restart_id =
					    timeout(zsa_restart, zs, hz / 4);
					mutex_enter(zs->zs_excl_hi);
				}
				za->za_flags |= ZAS_BREAK;
				mutex_exit(zs->zs_excl_hi);
			}
			break;

		case TIOCSBRK:
			mutex_enter(zs->zs_excl_hi);
			SCC_BIS(5, ZSWR5_BREAK);
			mutex_exit(zs->zs_excl_hi);
			zs_ack(mp, (mblk_t *)NULL, 0);
			break;

		case TIOCCBRK:
			mutex_enter(zs->zs_excl_hi);
			SCC_BIC(5, ZSWR5_BREAK);
			mutex_exit(zs->zs_excl_hi);
			zs_ack(mp, (mblk_t *)NULL, 0);
			break;

		case TIOCMSET:
			if (iocp->ioc_count != TRANSPARENT) {
				mutex_enter(zs->zs_excl_hi);
				(void) zsmctl(zs,
				    dmtozs(*(int *)mp->b_cont->b_rptr),
				    DMSET);
				mutex_exit(zs->zs_excl_hi);
				zs_ack(mp, (mblk_t *)NULL, 0);
			} else {
				zs_copy(mp, (mblk_t *)NULL, sizeof (int),
				    M_COPYIN);
			}
			/*
			 * qreply done below
			 */
			break;

		case TIOCMBIS:
			if (iocp->ioc_count != TRANSPARENT) {
			    mutex_enter(zs->zs_excl_hi);
			    (void) zsmctl(zs,
				dmtozs(*(int *)mp->b_cont->b_rptr),
				DMBIS);
			    mutex_exit(zs->zs_excl_hi);
			    zs_ack(mp, (mblk_t *)NULL, 0);
			} else
			    zs_copy(mp, (mblk_t *)NULL, sizeof (int), M_COPYIN);
			/*
			 * qreply done below
			 */
			break;

		case TIOCMBIC:
			if (iocp->ioc_count != TRANSPARENT) {
				mutex_enter(zs->zs_excl_hi);
				(void) zsmctl(zs,
				    dmtozs(*(int *)mp->b_cont->b_rptr), DMBIC);
				mutex_exit(zs->zs_excl_hi);
				zs_ack(mp, (mblk_t *)NULL, 0);
			} else
				zs_copy(mp, (mblk_t *)NULL, sizeof (int),
				    M_COPYIN);
			/*
			 * qreply done below
			 */
			break;

		case TIOCMGET:
			tmp = allocb(sizeof (int), BPRI_MED);
			if (tmp == NULL) {
				zs_sendnak(wq, mp, EAGAIN);
				return;
			}
			if (iocp->ioc_count != TRANSPARENT) {
			    zs_ack(mp, (mblk_t *)tmp, sizeof (int));
			    mutex_enter(zs->zs_excl_hi);
			    *(int *)mp->b_cont->b_rptr =
				zstodm(zsmctl(zs, 0, DMGET));
			    mutex_exit(zs->zs_excl_hi);
			} else {
			    zs_copy(mp, (mblk_t *)tmp, sizeof (int), M_COPYOUT);
			    mutex_enter(zs->zs_excl_hi);
			    *(int *)mp->b_cont->b_rptr =
				zstodm(zsmctl(zs, 0, DMGET));
			    mutex_exit(zs->zs_excl_hi);
			}
			/*
			 * qreply done below
			 */
			break;

		default:
			/*
			 * If we don't understand it, it's an error. NAK it.
			 */
			error = EINVAL;
			break;
		}
	}

	if (error != 0) {
		iocp->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK;
	}

	ZSA_QREPLY(wq, mp);
}


static int
dmtozs(int bits)
{
	register int b = 0;

	if (bits & TIOCM_CAR)
		b |= ZSRR0_CD;
	if (bits & TIOCM_CTS)
		b |= ZSRR0_CTS;
	if (bits & TIOCM_RTS)
		b |= ZSWR5_RTS;
	if (bits & TIOCM_DTR)
		b |= ZSWR5_DTR;
	return (b);
}

static int
zstodm(int bits)
{
	register int b;

	b = 0;
	if (bits & ZSRR0_CD)
		b |= TIOCM_CAR;
	if (bits & ZSRR0_CTS)
		b |= TIOCM_CTS;
	if (bits & ZSWR5_RTS)
		b |= TIOCM_RTS;
	if (bits & ZSWR5_DTR)
		b |= TIOCM_DTR;
	return (b);
}

/*
 * Assemble registers and flags necessary to program the port to our liking.
 * For async operation, most of this is based on the values of
 * the "c_iflag" and "c_cflag" fields supplied to us.
 */
static void
zsa_program(struct asyncline *za, int setibaud)
{
	register struct zscom *zs = za->za_common;
	register struct zs_prog *zspp;
	register int wr3, wr4, wr5, wr15, speed, baudrate, flags = 0;

	if ((baudrate = SPEED(za->za_ttycommon.t_cflag)) == 0) {
		/*
		 * Hang up line.
		 */
		(void) zsmctl(zs, ZS_OFF, DMSET);
		return;
	}

	/*
	 * set input speed same as output, as split speed not supported
	 */
	if (setibaud) {
		za->za_ttycommon.t_cflag &= ~(CIBAUD);
		if (baudrate > CBAUD) {
			za->za_ttycommon.t_cflag |= CIBAUDEXT;
			za->za_ttycommon.t_cflag |=
				(((baudrate - CBAUD - 1) << IBSHIFT) & CIBAUD);
		} else {
			za->za_ttycommon.t_cflag &= ~CIBAUDEXT;
			za->za_ttycommon.t_cflag |=
				((baudrate << IBSHIFT) & CIBAUD);
		}
	}

	/*
	 * Do not allow the console/keyboard device to have its receiver
	 * disabled; doing that would mean you couldn't type an abort
	 * sequence.
	 */
	if ((za->za_dev == rconsdev) || (za->za_dev == kbddev) ||
	    (za->za_dev == stdindev) || (za->za_ttycommon.t_cflag & CREAD))
		wr3 = ZSWR3_RX_ENABLE;
	else
		wr3 = 0;
	wr4 = ZSWR4_X16_CLK;
	wr5 = (zs->zs_wreg[5] & (ZSWR5_RTS|ZSWR5_DTR)) | ZSWR5_TX_ENABLE;

	if (zsb134_weird && baudrate == B134) {	/* what a joke! */
		/*
		 * XXX - should B134 set all this crap in the compatibility
		 * module, leaving this stuff fairly clean?
		 */
		flags |= ZSP_PARITY_SPECIAL;
		wr3 |= ZSWR3_RX_6;
		wr4 |= ZSWR4_PARITY_ENABLE | ZSWR4_PARITY_EVEN;
		wr4 |= ZSWR4_1_5_STOP;
		wr5 |= ZSWR5_TX_6;
	} else {

		switch (za->za_ttycommon.t_cflag & CSIZE) {

		case CS5:
			wr3 |= ZSWR3_RX_5;
			wr5 |= ZSWR5_TX_5;
			break;

		case CS6:
			wr3 |= ZSWR3_RX_6;
			wr5 |= ZSWR5_TX_6;
			break;

		case CS7:
			wr3 |= ZSWR3_RX_7;
			wr5 |= ZSWR5_TX_7;
			break;

		case CS8:
			wr3 |= ZSWR3_RX_8;
			wr5 |= ZSWR5_TX_8;
			break;
		}

		if (za->za_ttycommon.t_cflag & PARENB) {
			/*
			 * The PARITY_SPECIAL bit causes a special rx
			 * interrupt on parity errors. Turn it on if
			 * we're checking the parity of characters.
			 */
			if (za->za_ttycommon.t_iflag & INPCK)
				flags |= ZSP_PARITY_SPECIAL;
			wr4 |= ZSWR4_PARITY_ENABLE;
			if (!(za->za_ttycommon.t_cflag & PARODD))
				wr4 |= ZSWR4_PARITY_EVEN;
		}
		wr4 |= (za->za_ttycommon.t_cflag & CSTOPB) ?
		    ZSWR4_2_STOP : ZSWR4_1_STOP;
	}

#if 0
	/*
	 * The AUTO_CD_CTS flag enables the hardware flow control feature of
	 * the 8530, which allows the state of CTS and DCD to control the
	 * enabling of the transmitter and receiver, respectively. The
	 * receiver and transmitter still must have their enable bits set in
	 * WR3 and WR5, respectively, for CTS and DCD to be monitored this way.
	 * Hardware flow control can thus be implemented with no help from
	 * software.
	 */
	if (za->za_ttycommon.t_cflag & CRTSCTS)
		wr3 |= ZSWR3_AUTO_CD_CTS;
#endif
	if (za->za_ttycommon.t_cflag & CRTSCTS)
		wr15 = ZSR15_BREAK | ZSR15_TX_UNDER | ZSR15_CD | ZSR15_CTS;
	else
		wr15 = ZSR15_BREAK | ZSR15_TX_UNDER | ZSR15_CD;

	speed = zs->zs_wreg[12] + (zs->zs_wreg[13] << 8);

	/*
	 * Here we assemble a set of changes to be passed to zs_program.
	 * Note: Write Register 15 must be set to enable BREAK and UNDERrun
	 * interrupts.  It must also enable CD interrupts which, although
	 * not processed by the hardware interrupt handler, will be processed
	 * by zsa_process, indirectly resulting in a SIGHUP being delivered
	 * to the controlling process if CD drops.  CTS interrupts must NOT
	 * be enabled.  We don't use them at all, and they will hang IPC/IPX
	 * systems at boot time if synchronous modems that supply transmit
	 * clock are attached to any of their serial ports.
	 */
	if (((zs->zs_wreg[1] & ZSWR1_PARITY_SPECIAL) &&
	    !(flags & ZSP_PARITY_SPECIAL)) ||
	    (!(zs->zs_wreg[1] & ZSWR1_PARITY_SPECIAL) &&
	    (flags & ZSP_PARITY_SPECIAL)) ||
	    wr3 != zs->zs_wreg[3] || wr4 != zs->zs_wreg[4] ||
	    wr5 != zs->zs_wreg[5] || wr15 != zs->zs_wreg[15] ||
	    speed != zs_speeds[baudrate]) {

		za->za_flags |= ZAS_DRAINING;
		zspp = &zs_prog[zs->zs_unit];
		zspp->zs = zs;
		zspp->flags = (u_char)flags;
		zspp->wr4 = (u_char)wr4;
		zspp->wr11 = (u_char)(ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD);

		speed = zs_speeds[baudrate];
		zspp->wr12 = (u_char)(speed & 0xff);
		zspp->wr13 = (u_char)((speed >> 8) & 0xff);
		zspp->wr3 = (u_char)wr3;
		zspp->wr5 = (u_char)wr5;
		zspp->wr15 = (u_char)wr15;

		zs_program(zspp);
		za->za_flags &= ~ZAS_DRAINING;
	}
}

/*
 * Get the current speed of the console and turn it into something
 * UNIX knows about - used to preserve console speed when UNIX comes up.
 */
int
zsgetspeed(dev_t dev)
{
	register struct zscom *zs;
	register int uspeed, zspeed;
	register u_char rr;

	zs = &zscom[UNIT(dev)];
	SCC_READ(12, zspeed);
	SCC_READ(13, rr);
	zspeed |= rr << 8;
	for (uspeed = 0; uspeed < NSPEED; uspeed++)
		if (zs_speeds[uspeed] == zspeed)
			return (uspeed);
	/*
	 * 9600 baud if we can't figure it out
	 */
	return (ISPEED);
}

/*
 * callback routine when enough memory is available.
 */
static void
zsa_callback(void *arg)
{
	struct zscom *zs = arg;
	struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	int allocbcount = zsa_rstandby;

	mutex_enter(zs->zs_excl);
	if (za->za_bufcid) {
		za->za_bufcid = 0;
		ZSA_GETBLOCK(zs, allocbcount);
	}
	mutex_exit(zs->zs_excl);
}

/*
 * Set the receiver flags
 */
static void
zsa_set_za_rcv_flags_mask(struct asyncline *za)
{
	register uint mask;

	za->za_rcv_flags_mask &= ~0xFF;
	switch (za->za_ttycommon.t_cflag & CSIZE) {
	case CS5:
		mask = 0x1f;
		break;
	case CS6:
		mask = 0x3f;
		break;
	case CS7:
		mask = 0x7f;
		break;
	default:
		mask = 0xff;
	}

	za->za_rcv_flags_mask &= ~(0xFF << 16);
	za->za_rcv_flags_mask |=  mask << 16;

	if ((za->za_ttycommon.t_iflag & PARMRK) &&
	    !(za->za_ttycommon.t_iflag & (IGNPAR|ISTRIP))) {
		za->za_rcv_flags_mask |= DO_ESC;
	} else
		za->za_rcv_flags_mask &= ~DO_ESC;
	if (za->za_ttycommon.t_iflag & IXON) {
		za->za_rcv_flags_mask |= DO_STOPC;
		za->za_rcv_flags_mask &= ~0xFF;
		za->za_rcv_flags_mask |= za->za_ttycommon.t_stopc;
	} else
		za->za_rcv_flags_mask &= ~DO_STOPC;
}

static int
zsa_suspend(struct zscom *zs)
{
	struct asyncline	*za;
	queue_t			*q;
	mblk_t			*bp = NULL;
	timeout_id_t		restart_id, kick_rcv_id;
	struct zs_prog		*zspp;

	za = (struct asyncline *)&zs->zs_priv_str;
	mutex_enter(zs->zs_excl);
	if (zs->zs_suspended) {
		mutex_exit(zs->zs_excl);
		return (DDI_SUCCESS);
	}
	zs->zs_suspended = 1;

	/*
	 * Turn off interrupts and get any bytes in receiver
	 */
	mutex_enter(zs->zs_excl_hi);
	SCC_BIC(1, ZSWR1_INIT);
	ZSA_KICK_RCV;
	restart_id = za->za_zsa_restart_id;
	za->za_zsa_restart_id = 0;
	kick_rcv_id = za->za_kick_rcv_id;
	za->za_kick_rcv_id = 0;
	mutex_exit(zs->zs_excl_hi);
	mutex_exit(zs->zs_excl);

	/*
	 * Cancel any timeouts
	 */
	if (restart_id)
		(void) untimeout(restart_id);
	if (kick_rcv_id)
		(void) untimeout(kick_rcv_id);

	/*
	 * Since we have turned off interrupts, zsa_txint will not be called
	 * and no new chars will given to the chip. We just wait for the
	 * current character(s) to drain.
	 */
	(void) delay(ztdelay(za->za_ttycommon.t_cflag & CBAUD));

	/*
	 * Return remains of partially sent message to queue
	 */
	mutex_enter(zs->zs_excl);
	if ((q = za->za_ttycommon.t_writeq) != NULL) {
		mutex_enter(zs->zs_excl_hi);
		if ((zs->zs_wr_cur) != NULL) {
			za->za_flags &= ~ZAS_BUSY;
			za->za_rcv_flags_mask &= ~DO_RETRANSMIT;
			bp = za->za_xmitblk;
			bp->b_rptr = zs->zs_wr_cur;
			zs->zs_wr_cur = NULL;
			zs->zs_wr_lim = NULL;
			za->za_xmitblk = NULL;
		}
		mutex_exit(zs->zs_excl_hi);
		if (bp)
			(void) putbq(q, bp);
	}

	/*
	 * Stop any breaks in progress.
	 */
	mutex_enter(zs->zs_excl_hi);
	if (zs->zs_wreg[5] & ZSWR5_BREAK) {
		SCC_BIC(5, ZSWR5_BREAK);
		za->za_flags &= ~ZAS_BREAK;
	}

	/*
	 * Now get a copy of current registers setting.
	 */
	zspp = &zs_prog[zs->zs_unit];
	zspp->zs = zs;
	zspp->flags = 0;
	zspp->wr3 = zs->zs_wreg[3];
	zspp->wr4 = zs->zs_wreg[4];
	zspp->wr5 = zs->zs_wreg[5];
	zspp->wr11 = zs->zs_wreg[11];
	zspp->wr12 = zs->zs_wreg[12];
	zspp->wr13 = zs->zs_wreg[13];
	zspp->wr15 = zs->zs_wreg[15];
	mutex_exit(zs->zs_excl_hi);
	mutex_exit(zs->zs_excl);
	return (DDI_SUCCESS);
}

static int
zsa_resume(struct zscom *zs)
{
	register struct asyncline *za;
	struct zs_prog	*zspp;

	za = (struct asyncline *)&zs->zs_priv_str;
	mutex_enter(zs->zs_excl);
	if (!(zs->zs_suspended)) {
		mutex_exit(zs->zs_excl);
		return (DDI_SUCCESS);
	}

	/*
	 * Restore H/W state
	 */
	mutex_enter(zs->zs_excl_hi);
	zspp = &zs_prog[zs->zs_unit];
	zs_program(zspp);

	/*
	 * Enable all interrupts for this chip and delay to let chip settle
	 */
	SCC_WRITE(9, ZSWR9_MASTER_IE | ZSWR9_VECTOR_INCL_STAT);
	DELAY(4000);

	/*
	 * Restart receiving and transmitting
	 */
	zs->zs_suspended = 0;
	za->za_rcv_flags_mask |= DO_TRANSMIT;
	za->za_ext = 1;
	ZSSETSOFT(zs);
	mutex_exit(zs->zs_excl_hi);
	mutex_exit(zs->zs_excl);

	return (DDI_SUCCESS);
}

#ifdef ZSA_DEBUG
static void
zsa_print_info(struct zscom *zs)
{
	register struct asyncline *za = (struct asyncline *)&zs->zs_priv_str;
	register queue_t *q = za->za_ttycommon.t_writeq;

	printf(" next q=%s\n", (RD(q))->q_next->q_qinfo->qi_minfo->mi_idname);
	printf("unit=%d\n", zs->zs_unit);
	printf("tflag:\n");
	if (za->za_ttycommon.t_flags & TS_SOFTCAR) printf(" t_fl:TS_SOFTCAR");
	if (za->za_ttycommon.t_flags & TS_XCLUDE) printf(" t_fl:TS_XCLUDE");
	if (za->za_ttycommon.t_iflag & IGNBRK) printf(" t_ifl:IGNBRK");
	if (za->za_ttycommon.t_iflag & BRKINT) printf(" t_ifl:BRKINT");
	if (za->za_ttycommon.t_iflag & IGNPAR) printf(" t_ifl:IGNPAR");
	if (za->za_ttycommon.t_iflag & PARMRK) printf(" t_ifl:PARMRK");
	if (za->za_ttycommon.t_iflag & INPCK) printf(" t_ifl:INPCK");
	if (za->za_ttycommon.t_iflag & ISTRIP) printf(" t_ifl:ISTRIP");
	if (za->za_ttycommon.t_iflag & INLCR) printf(" t_ifl:INLCR");
	if (za->za_ttycommon.t_iflag & IGNCR) printf(" t_ifl:IGNCR");
	if (za->za_ttycommon.t_iflag & ICRNL) printf(" t_ifl:ICRNL");
	if (za->za_ttycommon.t_iflag & IUCLC) printf(" t_ifl:IUCLC");
	if (za->za_ttycommon.t_iflag & IXON) printf(" t_ifl:IXON");
	if (za->za_ttycommon.t_iflag & IXOFF) printf(" t_ifl:IXOFF");

	printf("\n");


	if (za->za_ttycommon.t_cflag & CSIZE == CS5) printf(" t_cfl:CS5");
	if (za->za_ttycommon.t_cflag & CSIZE == CS6) printf(" t_cfl:CS6");
	if (za->za_ttycommon.t_cflag & CSIZE == CS7) printf(" t_cfl:CS7");
	if (za->za_ttycommon.t_cflag & CSIZE == CS8) printf(" t_cfl:CS8");
	if (za->za_ttycommon.t_cflag & CSTOPB) printf(" t_cfl:CSTOPB");
	if (za->za_ttycommon.t_cflag & CREAD) printf(" t_cfl:CREAD");
	if (za->za_ttycommon.t_cflag & PARENB) printf(" t_cfl:PARENB");
	if (za->za_ttycommon.t_cflag & PARODD) printf(" t_cfl:PARODD");
	if (za->za_ttycommon.t_cflag & HUPCL) printf(" t_cfl:HUPCL");
	if (za->za_ttycommon.t_cflag & CLOCAL) printf(" t_cfl:CLOCAL");
	printf(" t_stopc=%x", za->za_ttycommon.t_stopc);
	printf("\n");
}
#endif

/*
 * Check for abort character sequence
 */
static boolean_t
abort_charseq_recognize(u_char ch)
{
	static int state = 0;
#define	CNTRL(c) ((c)&037)
	static char sequence[] = { '\r', '~', CNTRL('b') };

	if (ch == sequence[state]) {
		if (++state >= sizeof (sequence)) {
			state = 0;
			return (B_TRUE);
		}
	} else {
		state = (ch == sequence[0]) ? 1 : 0;
	}
	return (B_FALSE);
}
