/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mic.c	1.17	97/10/22 SMI"

/*
 *	Serial I/O driver for Multi Interface Chip (MIC)
 *	This chip provides two serial UARTS (with fifos),
 *	one of which may be used with an infrared transceiver.
 *	A Streams Driver
 *
 *	This version is NO protocol, which is fine for the infrared port.
 *	However, if you try fancy stuff on tty lines (software/hardware
 *	flowcontrol, modem status changes) the driver will ignore them.
 *	Clearly this should be fixed, and there are some comments in places
 *	where you would add such features.
 */

#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/tty.h>
#include <sys/strtty.h>

#include <sys/termio.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>		/* per 494 change */
#include <sys/debug.h>		/* per 494 change */
#include <sys/file.h>		/* per 494 change */
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/micio.h>
#include <sys/micreg.h>
#include <sys/micvar.h>

#if 0
#define	errp(a)		cmn_err(CE_CONT, a)
#define	errp2(a, b)	cmn_err(CE_CONT, a, b)
#else
#define	errp(a)
#define	errp2(a, b)
#endif

static void	*mic_statep;	/* all mic unit soft state structures */

/* Interface functions */
static int	mic_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	mic_identify(dev_info_t *);
static int	mic_probe(dev_info_t *);
static int	mic_attach(dev_info_t *, ddi_attach_cmd_t);
static int	mic_detach(dev_info_t *, ddi_detach_cmd_t);
static int	mic_open(queue_t *, dev_t *, int, int, cred_t *);
static int	mic_close(queue_t *, int, cred_t *);
static int	mic_wput(queue_t *, mblk_t *);

/* Local functions */
static u_int	mic_intr(caddr_t);
static void	mic_txint(void *);
static void	mic_rxint(mic_port_t *);
static u_int	mic_soft(caddr_t);
static void	mic_process(mic_port_t *);

static void	mic_program(mic_port_t *);
static void	scc_program(mic_port_t *);

static void	mic_ioctl(mic_port_t *, queue_t *, mblk_t *);
static void	mic_reioctl(void *);
static void	mic_resend(void *);

/*
 * Baud rate table. Indexed by #defines Bxx found in sys/termios.h
 * Not all baud rates in termios.h are supported.  Some (the two
 * lowest non-zero) rates are used for extra high rates - a hack!
 */
ushort_t asyspdtab[] = {
	0,		/* 0 baud rate */
	ASY96000,	/* 50 baud rate -> now used it for higher speed */
	ASY115200,	/* 75 baud rate -> now used it for higher speed */
	ASY110,		/* 110 baud rate */
	0,		/* 134 baud rate */
	ASY150,		/* 150 baud rate */
	0,		/* 200 baud rate */
	ASY300,		/* 300 baud rate */
	ASY600,		/* 600 baud rate */
	ASY1200,	/* 1200 baud rate */
	0,		/* 1800 baud rate */
	ASY2400,	/* 2400 baud rate */
	ASY4800,	/* 4800 baud rate */
	ASY9600,	/* 9600 baud rate */
	ASY19200,	/* 19200 baud rate */
	ASY38400	/* 38400 baud rate */
};

/*
 * Macros
 * instance and port to minor number macros
 * since each mic device has 3 ports (including ir) then
 *	minor = 3 * instance + port
 *
 */
#define	UNIT(dev)		(getminor(dev) / 3)
#define	PORT(dev)		(getminor(dev) % 3)
#define	MINOR(instance, port)	(3 * (instance) + (port))

#define	TXINTR_ON(port)	{ \
		(port)->intr_en |= MIC_TXWATER_I; \
		(port)->ctl_regs->gen_ie |= MIC_TXWATER_IE; \
	}
#define	TXINTR_OFF(port) { \
		(port)->ctl_regs->gen_ie &= ~MIC_TXWATER_IE; \
		(port)->intr_en &= ~MIC_TXWATER_I; \
	}
#define	RXINTR_ON(port)		{ \
		(port)->intr_en |= MIC_RXWATER_I | MIC_TIMEOUT_I; \
		(port)->ctl_regs->gen_ie |= MIC_RXWATER_IE | MIC_TIMEOUT_IE; \
	}
#define	RXINTR_OFF(port)	{ \
		(port)->ctl_regs->gen_ie &= ~(MIC_RXWATER_IE | MIC_TIMEOUT_IE);\
		(port)->intr_en &= ~(MIC_RXWATER_I | MIC_TIMEOUT_I); \
	}
#define	TXEMPTY_ON(port)	{ \
		(port)->intr_en |= MIC_TXEMPTY_I; \
		(port)->ctl_regs->gen_ie |= MIC_TXEMPTY_IE; \
	}
#define	TXEMPTY_OFF(port)	{ \
		(port)->ctl_regs->gen_ie &= ~MIC_TXEMPTY_IE; \
		(port)->intr_en &= ~MIC_TXEMPTY_I; \
	}

struct module_info mic_str_info = {
	0,
	"mic",
	0,
	INFPSZ,
	2048,
	128
};

static struct qinit mic_rint = {
	putq,
	NULL,
	mic_open,
	mic_close,
	NULL,
	&mic_str_info,
	NULL
};

static struct qinit mic_wint = {
	mic_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&mic_str_info,
	NULL
};

struct streamtab mic_streamtab = {
	&mic_rint,
	&mic_wint,
	NULL,
	NULL
};

static 	struct cb_ops mic_cb_ops = {
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
	&mic_streamtab,		/* cb_stream */
	D_NEW | D_MP		/* cb_flag */
};

struct dev_ops mic_dev_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	mic_info,		/* devo_getinfo */
	mic_identify,		/* devo_identify */
	mic_probe,		/* devo_probe */
	mic_attach,		/* devo_attach */
	mic_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&mic_cb_ops,		/* devo_cb_ops */
	NULL,			/* devo_bus_ops */
	nulldev			/* devo_power */
};

/*
 * Module linkage information for the kernel.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Driver module */
	"MIC driver",
	&mic_dev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	int	e;

	if ((e = mod_install(&modlinkage)) == 0)
		e = ddi_soft_state_init(&mic_statep, sizeof (mic_unit_t), 1);
	return (e);
}

int
_fini()
{
	int	e;

	if ((e = mod_remove(&modlinkage)) == 0)
		ddi_soft_state_fini(&mic_statep);
	return (e);
}

int
_info(struct modinfo *infop)
{
	return (mod_info(&modlinkage, infop));
}

static int
mic_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "SUNW,mic") == 0)
		return (DDI_IDENTIFIED);

	return (DDI_NOT_IDENTIFIED);
}

static int
mic_probe(dev_info_t *devi)
{
	struct mic_stat	*mic;

	if (ddi_map_regs(devi, 0, (caddr_t *)&mic, 0, sizeof (struct mic_stat))
	    != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);
	if (mic->id != MIC_ID)
		return (DDI_PROBE_FAILURE);
	ddi_unmap_regs(devi, 0, (caddr_t *)&mic, 0, sizeof (struct mic_stat));
	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
mic_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t		dev = (dev_t)arg;
	int		instance = UNIT(dev);
	mic_unit_t	 *unitp;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((unitp = ddi_get_soft_state(mic_statep, instance)) == NULL)
			return (DDI_FAILURE);
		*result = (void *) unitp->mic_dip;
		return (DDI_SUCCESS);

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
mic_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance = ddi_get_instance(dip);
	mic_unit_t	*unitp;
	mic_port_t	*port;
	int		i, reg_set, unsent;
	uchar_t		*s;

	switch (cmd) {
	case DDI_ATTACH:
		if (ddi_soft_state_zalloc(mic_statep, instance) != 0)
			return (DDI_FAILURE);
		if ((unitp = ddi_get_soft_state(mic_statep, instance)) == NULL)
			return (DDI_FAILURE);

		unitp->mic_dip = dip;

		/*
		 * Install interrupt handlers
		 */
		if (ddi_add_intr(dip, 0, &unitp->mic_intr, 0, mic_intr,
		    (caddr_t)unitp) != DDI_SUCCESS ||
		    ddi_add_softintr(dip, DDI_SOFTINT_LOW, &unitp->soft_id,
		    &unitp->soft_intr, NULL, mic_soft,
		    (caddr_t)unitp) != DDI_SUCCESS) {
			(void) mic_detach(dip, DDI_DETACH);
			return (DDI_FAILURE);
		}

		/*
		 * Initialize ports
		 */
		reg_set = 0;
		for (i = 0; i < PORTS; i++) {
			port = (mic_port_t *)kmem_zalloc(sizeof (mic_port_t),
			    KM_SLEEP);
			unitp->port[i] = port;
			/* Map in registers */
			if (ddi_map_regs(dip, reg_set++, (caddr_t *)
			    &port->stat_regs, 0, sizeof (struct mic_stat)) !=
			    DDI_SUCCESS ||
			    ddi_map_regs(dip, reg_set++, (caddr_t *)
			    &port->ctl_regs, 0, sizeof (struct mic_ctl)) !=
			    DDI_SUCCESS ||
			    ddi_map_regs(dip, reg_set++, (caddr_t *)
			    &port->scc_regs, 0, sizeof (struct mic_scc)) !=
			    DDI_SUCCESS) {
				(void) mic_detach(dip, DDI_DETACH);
				return (DDI_FAILURE);
			}

			/* Initialize other per-port fields */
			mutex_init(&port->port_lock, NULL, MUTEX_DRIVER, NULL);
			mutex_init(&port->tx_lock, NULL, MUTEX_DRIVER,
			    unitp->mic_intr);
			mutex_init(&port->rx_lock, NULL, MUTEX_DRIVER,
			    unitp->mic_intr);
			cv_init(&port->tx_done_cv, NULL, CV_DRIVER, NULL);
			port->unitp = unitp;
			port->rx_blk = allocb(RX_BUFFER_SZ, BPRI_LO);
			port->baud_div = 0;
			if (port->rx_blk == NULL) {
				(void) mic_detach(dip, DDI_DETACH);
				return (DDI_FAILURE);
			}
			/*
			 * Program chip (disable interrupts)
			 */
			mic_program(unitp->port[i]);
		}

		/*
		 * Create minor nodes
		 */
		if (ddi_create_minor_node(dip, "a", S_IFCHR,
		    MINOR(instance, 0), "ddi_other", 0) == DDI_FAILURE ||
		    ddi_create_minor_node(dip, "b", S_IFCHR,
		    MINOR(instance, 1), "ddi_other", 0) == DDI_FAILURE ||
		    ddi_create_minor_node(dip, "ir", S_IFCHR,
		    MINOR(instance, 2), "ddi_other", 0) == DDI_FAILURE) {
			(void) mic_detach(dip, DDI_DETACH);
			return (DDI_FAILURE);
		}
		ddi_report_dev(dip);

		return (DDI_SUCCESS);

	case DDI_RESUME:
		if ((unitp = ddi_get_soft_state(mic_statep, instance)) == NULL)
			return (DDI_FAILURE);
		for (i = 0; i < PORTS; i++) {
			port = unitp->port[i];
			mutex_enter(&port->tx_lock);
			mic_program(port);
			scc_program(port);
			unsent = port->tx_buf_cnt;
			if (unsent) {
				s = port->tx_buf;
				TXINTR_ON(port);
				while (unsent--)
					port->ctl_regs->tx_data = *s++;
			} else {
				port->flags |= PORT_TXCHK;
				ddi_trigger_softintr(port->unitp->soft_id);
			}
			if (port->flags & PORT_IRBUSY)
				TXEMPTY_ON(port);
			mutex_exit(&port->tx_lock);
		}
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
mic_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance = ddi_get_instance(dip);
	mic_unit_t	*unitp;
	mic_port_t	*port;
	int		i, unsent;
	uchar_t		*s;

	if ((unitp = ddi_get_soft_state(mic_statep, instance)) == NULL)
		return (DDI_FAILURE);

	switch (cmd) {
	case DDI_DETACH:
		for (i = 0; i < PORTS; i++) {
			if ((port = unitp->port[i]) == NULL)
				continue;
			if (port->stat_regs)
				ddi_unmap_regs(dip, 0, (caddr_t *)
				    &port->stat_regs, 0,
				    sizeof (struct mic_stat));
			if (port->ctl_regs)
				ddi_unmap_regs(dip, 0, (caddr_t *)
				    &port->ctl_regs, 0,
				    sizeof (struct mic_ctl));
			if (port->scc_regs)
				ddi_unmap_regs(dip, 0, (caddr_t *)
				    &port->scc_regs, 0,
				    sizeof (struct mic_scc));
			if (port->rx_blk)
				freemsg(port->rx_blk);
			kmem_free(port, sizeof (mic_port_t));
		}
		if (unitp->soft_id)
			ddi_remove_softintr(unitp->soft_id);
		if (unitp->mic_intr)
			ddi_remove_intr(dip, 0, &unitp->mic_intr);
		ddi_remove_minor_node(dip, NULL);
		ddi_soft_state_free(mic_statep, instance);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		for (i = 0; i < PORTS; i++) {
			port = unitp->port[i];
			mutex_enter(&port->rx_lock);
			RXINTR_OFF(port)
			mic_rxint(port);
			mutex_exit(&port->rx_lock);
			mutex_enter(&port->tx_lock);
			port->ctl_regs->gen_ie = 0;
			port->ctl_regs->misc_ctl |= MIC_SCC_PROG_MODE |
			    MIC_CLR_RX;
			port->ctl_regs->test_ctl |= MIC_TXFF_TO_RXFF;
			unsent = port->stat_regs->rx_cnt;
			if (unsent) {
				s = port->tx_buf;
				port->tx_buf_cnt = unsent;
				while (unsent--)
					*s++ = port->stat_regs->rx_data;
			}
			mutex_exit(&port->tx_lock);
		}
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED3*/
static int
mic_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	int		instance;
	mic_unit_t	*unitp;
	mic_port_t	*port;

	instance = UNIT(*dev);
	if ((unitp = ddi_get_soft_state(mic_statep, instance)) == NULL) {
		return (ENXIO);
	}
	port = (PORT(*dev) != 1) ? unitp->port[0] : unitp->port[1];

	mutex_enter(&port->port_lock);
	if (port->flags & PORT_OPEN) {
		/*
		 * TTY: Enforce exclusive opens for the moment
		 */
		mutex_exit(&port->port_lock);
		return (EBUSY);
	}
	port->flags |= PORT_OPEN;
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)port;

	/*
	 * Set the default termios settings (cflag).
	 */
	if (PORT(*dev) == 2) {			/* IR mode */
		port->flags |= PORT_IR;
		port->ir_mode = MIC_IR_PULSE;
		port->ir_divisor = 9;
		port->ttycommon.t_cflag = B2400;
	} else {				/* Serial line */
		port->ir_mode = MIC_IR_OFF;
		port->ir_divisor = 0;
		port->ttycommon.t_cflag = B9600;
	}
	port->ttycommon.t_cflag |= CS8;
	if (flag & FREAD)
		port->ttycommon.t_cflag |= CREAD;
	port->ttycommon.t_iflag = 0;
	port->ttycommon.t_iocpending = NULL;
	port->ttycommon.t_size.ws_row = 0;
	port->ttycommon.t_size.ws_col = 0;
	port->ttycommon.t_size.ws_xpixel = 0;
	port->ttycommon.t_size.ws_ypixel = 0;
	port->ttycommon.t_startc = CSTART;
	port->ttycommon.t_stopc = CSTOP;
	port->ttycommon.t_readq = rq;
	port->ttycommon.t_writeq = WR(rq);
	mutex_exit(&port->port_lock);

	mutex_enter(&port->tx_lock);
	qprocson(rq);
	scc_program(port);
	mutex_exit(&port->tx_lock);

	return (0);
}

/*ARGSUSED1*/
static int
mic_close(queue_t *q, int flag, cred_t *credp)
{
	mic_port_t	*port;

	if ((port = (mic_port_t *)q->q_ptr) == NULL)
		return (ENODEV);

	/*
	 * Cancel any outstanding "bufcall" requests.
	 */
	if (port->ioctl_id) {
		unbufcall(port->ioctl_id);
		port->ioctl_id = 0;
	}
	if (port->send_id) {
		unbufcall(port->send_id);
		port->send_id = 0;
	}

	/*
	 * Disable receiver interrupts (hard and soft),
	 * and clear any partially received message.
	 */
	mutex_enter(&port->rx_lock);
	RXINTR_OFF(port)
	port->flags &= ~PORT_RXCHK;
	port->rx_blk->b_wptr = port->rx_blk->b_rptr;
	mutex_exit(&port->rx_lock);

	qprocsoff(q);
	mutex_enter(&port->port_lock);

	/*
	 * Wait for current stuff to drain
	 */
	mutex_enter(&port->tx_lock);
	do {
		TXEMPTY_ON(port)
		cv_wait(&port->tx_done_cv, &port->tx_lock);
	} while (port->tx_blk != NULL || qsize(WR(q)) != 0);
	mutex_exit(&port->tx_lock);

	ttycommon_close(&port->ttycommon);
	q->q_ptr = WR(q)->q_ptr = NULL;

	mic_program(port);
	port->flags = 0;

	mutex_exit(&port->port_lock);

	return (0);
}

/*
 * Put procedure for write queue.
 * Respond to M_STOP, M_START, M_IOCTL, and M_FLUSH messages here.
 * Queue up M_BREAK, M_DELAY, and M_DATA messages.
 * Discard everything else.
 */
static int
mic_wput(queue_t *q, mblk_t *mp)
{
	mic_port_t	*port = (mic_port_t *)q->q_ptr;
	mblk_t		*nmp;

	switch (mp->b_datap->db_type) {
	case M_STOP:
		/*
		 * Stop sending after current character
		 * This mechanism uses the extra OUT2 line to force the CTS
		 * line low, and thus stop the tx by the hardware flow control
		 * mechanism.
		 */
		errp("mic_wput: M_STOP\n");
		mutex_enter(&port->tx_lock);
		port->ctl_regs->flw_ctl |= MIC_CTS_AUTOEN;
		port->scc_regs->mcr &= ~OUT2;
		mutex_exit(&port->tx_lock);
		freemsg(mp);
		break;

	case M_START:
		/*
		 * Allow sending to continue
		 */
		errp("mic_wput: M_START\n");
		mutex_enter(&port->tx_lock);
		port->scc_regs->mcr |= OUT2;
		port->ctl_regs->flw_ctl &= ~MIC_CTS_AUTOEN;
		mutex_exit(&port->tx_lock);
		freemsg(mp);
		break;

	case M_IOCTL:
		errp("mic_wput: M_IOCTL\n");
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {

		case TCSETSW:
		case TCSETSF:
		case TCSETAW:
		case TCSETAF:
		case TCSBRK:
		case MIOCGETM_IR:
		case MIOCSETM_IR:
		case MIOCGETD_IR:
		case MIOCSETD_IR:
		case MIOCSLPBK_IR:
		case MIOCCLPBK_IR:
		case MIOCSLPBK:
		case MIOCCLPBK:
			/*
			 * The changes do not take effect until all
			 * output queued before them is drained.
			 * So put this message on the queue, then see if
			 * we can do it.
			 */
			putq(q, mp);
			port->flags |= PORT_TXCHK;
			ddi_trigger_softintr(port->unitp->soft_id);
			break;

		default:
			/*
			 * Do it now.
			 */
			mutex_enter(&port->tx_lock);
			mic_ioctl(port, q, mp);
			mutex_exit(&port->tx_lock);
			break;
		}
		break;

	case M_FLUSH:
		errp("mic_wput: M_FLUSH\n");
		if (*mp->b_rptr & FLUSHW) {
			mutex_enter(&port->tx_lock);
			/*
			 * Clear current Tx FIFO
			 */
			port->ctl_regs->misc_ctl |= MIC_CLR_TX;

			/*
			 * Clear current message blocks
			 */
			if (port->tx_blk) {
				port->flags &= ~(PORT_BREAK | PORT_DELAY);
				port->scc_regs->lcr &= ~SETBREAK;
				freemsg(port->tx_blk);
				port->tx_blk = NULL;
			}

			/*
			 * Clear all queued data messages
			 */
			flushq(q, FLUSHDATA);
			mutex_exit(&port->tx_lock);

			*mp->b_rptr &= ~FLUSHW;
		}
		if (*mp->b_rptr & FLUSHR) {
			mutex_enter(&port->rx_lock);
			port->ctl_regs->misc_ctl |= MIC_CLR_RX;
			port->rx_blk->b_wptr = port->rx_blk->b_rptr;
			mutex_exit(&port->rx_lock);
			qreply(q, mp);
		} else
			freemsg(mp);

		/*
		 * We must make sure we process messages that survive the
		 * write-side flush.  Without this call, the close protocol
		 * with ldterm can hang forever.  (ldterm will have sent us a
		 * TCSBRK ioctl that it expects a response to.)
		 */
		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		break;

	case M_BREAK:
	case M_DELAY:
		errp("mic_wput: M_BREAK, M_DELAY\n");
		putq(q, mp);
		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		break;

	case M_DATA:
		/*
		 * Queue the message up to be transmitted,
		 * and schedule processing. Split multi data-block
		 * messages into multiple M_DATA messages.
		 */
		errp("mic_wput: M_DATA\n");
		do {
			nmp = mp->b_cont;
			mp->b_cont = NULL;
			mp->b_datap->db_type = M_DATA;
			putq(q, mp);
			mp = nmp;
		} while (mp != NULL);

		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		break;

	case M_STOPI:
		/*
		 * It makes no sense (to me) to stop the receiver,
		 * so these messages are discarded.
		 */
		errp("mic_wput: M_STOPI\n");
		freemsg(mp);
		break;

	case M_STARTI:
		errp("mic_wput: M_STARTI\n");
		freemsg(mp);
		break;

	case M_CTL:
#if 0
		errp("mic_wput: M_CTL\n");
		/*
		 * These MC_SERVICE type messages are used by upper
		 * modules to tell this driver to send input up
		 * immediately, or that it can wait for normal
		 * processing that may or may not be done.  Sun
		 * requires these for the mouse module.
		 * Achieve this by reducing rx fifo to 1 byte
		 */
		switch (*mp->b_rptr) {

		case MC_SERVICEIMM:
			port->ctl_regs->rx_water = RX_WATER_MARK_LO;
			break;

		case MC_SERVICEDEF:
			port->ctl_regs->rx_water = RX_WATER_MARK_HI;
			break;
		}
#endif
		freemsg(mp);
		break;

	default:
		freemsg(mp);
		break;
	}
	return (0);
}

/*
 * Handle all soft interrupts
 */
static u_int
mic_soft(caddr_t arg)
{
	mic_unit_t	*unitp = (mic_unit_t *)arg;
	mic_port_t	*port;
	int		i, rc = DDI_INTR_UNCLAIMED;
	mblk_t		*old_mp, *new_mp;

	for (i = 0; i < PORTS; i++) {
		port = unitp->port[i];
		if (port->flags & PORT_TXCHK) {
			errp("mic_soft: TXCHK\n");
			port->flags &= ~PORT_TXCHK;
			rc = DDI_INTR_CLAIMED;
			mutex_enter(&port->tx_lock);
			if (!port->tx_blk)
				mic_process(port);
			mutex_exit(&port->tx_lock);
		}
		if (port->flags & PORT_RXCHK) {
			errp("mic_soft: RXCHK\n");
			port->flags &= ~PORT_RXCHK;
			rc = DDI_INTR_CLAIMED;
			new_mp = allocb(RX_BUFFER_SZ, BPRI_LO);
			if (new_mp == NULL) {
				port->send_id = bufcall(2 * RX_WATER_MARK_HI,
				    0, mic_resend, port);
			} else {
				mutex_enter(&port->rx_lock);
				old_mp = port->rx_blk;
				port->rx_blk = new_mp;
				mutex_exit(&port->rx_lock);
				errp2("mic_soft: Up->%d\n", old_mp->b_wptr -
				    old_mp->b_rptr);
				putnext(port->ttycommon.t_readq, old_mp);
			}
		}
		if (port->flags & PORT_TXDONE)
			cv_broadcast(&port->tx_done_cv);
	}
	return (rc);
}

/*
 * Get the next message from the input queue and proces it
 */
static void
mic_process(mic_port_t *port)
{
	queue_t	*q;
	mblk_t	*mp;

	ASSERT(port->tx_blk == NULL);
	ASSERT(mutex_owned(&port->tx_lock));

	if ((q = port->ttycommon.t_writeq) == NULL)
		return;			/* not attached to a stream */

	if ((mp = getq(q)) != NULL) {
		/*
		 * Make sure the mic is fully drained, unless
		 * we want to put more data in.
		 */
		if (mp->b_datap->db_type != M_DATA &&
		    !(port->stat_regs->isr & MIC_TXEMPTY_I)) {
			putbq(q, mp);
			TXEMPTY_ON(port)
			return;
		}
		switch (mp->b_datap->db_type) {

		case M_BREAK:
			errp("mic_process: M_BREAK\n");
			/*
			 * "Transmit" a break
			 */
			port->tx_blk = mp;
			port->flags |= PORT_BREAK;
			port->scc_regs->lcr |= SETBREAK;
			(void) timeout(mic_txint, port, drv_usectohz(250000));
			break;

		case M_DELAY:
			errp("mic_process: M_DELAY\n");
			/*
			 * "Transmit" a delay
			 */
			port->tx_blk = mp;
			port->flags |= PORT_DELAY;
			(void) timeout(mic_txint, port,
			    (clock_t)(*(unsigned char *)mp->b_rptr + 6));
			break;

		case M_IOCTL:
			errp("mic_process: M_IOCTL\n");
			mic_ioctl(port, q, mp);
			break;

		case M_DATA:
			errp("mic_process: M_DATA\n");
			port->tx_blk = mp;
			mic_txint(port);
			break;

		default:
			errp2("mic_process: Discarding message type %d\n",
			    mp->b_datap->db_type);
			freemsg(mp);
			port->flags |= PORT_TXCHK;
			ddi_trigger_softintr(port->unitp->soft_id);
		}
	}
}

/*
 * Process an "ioctl" message sent down to us.
 */
static void
mic_ioctl(mic_port_t *port, queue_t *wq, mblk_t *mp)
{
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;
	int		error = 0;
	int		datasize;
	tty_common_t	*tp = &port->ttycommon;

	ASSERT(mutex_owned(&port->tx_lock));
	errp2("mic_ioctl: cmd = %x\n", iocp->ioc_cmd);

	if (tp->t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(tp->t_iocpending);
		tp->t_iocpending = NULL;
	}

	/*
	 * The only way in which "ttycommon_ioctl" can fail is if the "ioctl"
	 * requires a response containing data to be returned to the user,
	 * and no mblk could be allocated for the data.
	 * No such "ioctl" alters our state.  Thus, we always go ahead and
	 * do any state-changes the "ioctl" calls for.  If we couldn't allocate
	 * the data, "ttycommon_ioctl" has stashed the "ioctl" away safely, so
	 * we just call "bufcall" to request that we be called back when we
	 * stand a better chance of allocating the data.
	 */
	enterq(wq);
	if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0) {
		leaveq(wq);
		if (port->ioctl_id)
			unbufcall(port->ioctl_id);
		port->ioctl_id = bufcall(datasize, BPRI_HI, mic_reioctl, port);
		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		return;
	}
	leaveq(wq);

	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETSF:
		case TCSETS:
		case TCSETSW:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			scc_program(port);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCACK;
		error = 0;
		switch (iocp->ioc_cmd) {

		case TCSBRK:
			/*
			 * "Transmit" a break, if arg != 0
			 */
			if (*(int *)mp->b_cont->b_rptr == 0) {
				port->tx_blk = mp;
				port->flags |= PORT_BREAK;
				port->scc_regs->lcr |= SETBREAK;
				(void) timeout(mic_txint, port,
				    drv_usectohz(250000));
			}
			break;

		case TIOCSBRK:
			port->tx_blk = mp;
			port->flags |= PORT_BREAK;
			port->scc_regs->lcr |= SETBREAK;
			break;

		case TIOCCBRK:
			port->tx_blk = NULL;
			port->flags &= ~PORT_BREAK;
			port->scc_regs->lcr &= ~SETBREAK;
			break;

		case MIOCGETM_IR:
			if (port->flags & PORT_IR)
				iocp->ioc_rval = port->ir_mode;
			else
				error = ENOTTY;
			break;

		case MIOCSETM_IR:
			if (port->flags & PORT_IR)
				port->ctl_regs->ir_mode = port->ir_mode =
				    *(int *)mp->b_cont->b_rptr;
			else
				error = ENOTTY;
			break;

		case MIOCGETD_IR:
			if (port->flags & PORT_IR)
				iocp->ioc_rval = port->ir_divisor;
			else
				error = ENOTTY;
			break;

		case MIOCSETD_IR:
			if (port->flags & PORT_IR)
				port->ctl_regs->ir_div = port->ir_divisor =
				    *(int *)mp->b_cont->b_rptr;
			else
				error = ENOTTY;
			break;

		case MIOCSLPBK_IR:
			if (port->flags & PORT_IR)
				port->flags |= PORT_LPBK;
			else
				error = ENOTTY;
			break;

		case MIOCCLPBK_IR:
			if (port->flags & PORT_IR)
				port->flags &= ~PORT_LPBK;
			else
				error = ENOTTY;
			break;

		case MIOCSLPBK:
			if (!(port->flags & PORT_IR))
				port->scc_regs->mcr |= SCC_LOOP;
			else
				error = ENOTTY;
			break;

		case MIOCCLPBK:
			if (!(port->flags & PORT_IR))
				port->scc_regs->mcr &= ~SCC_LOOP;
			else
				error = ENOTTY;
			break;

		default:
			errp2("mic_ioctl: Unknown ioctl %x\n", iocp->ioc_cmd);
			error = ENOTTY;
			break;
		}
	}
	if (error != 0) {
		iocp->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK;
	}
	qreply(wq, mp);

	/*
	 * Check next message
	 */
	port->flags |= PORT_TXCHK;
	ddi_trigger_softintr(port->unitp->soft_id);
}

/*
 * Redo an "ioctl", now that "bufcall" claims we may be able to allocate
 * the buffer we need. Since this ioctl is for user information only, we
 * don't need locks.
 */
static void
mic_reioctl(void *arg)
{
	mic_port_t	*port = arg;
	queue_t		*wq;
	mblk_t		*mp;
	unsigned	datasize;
	tty_common_t	*tp = &port->ttycommon;
	int		error;

	errp("mic_reioctl\n");
	port->ioctl_id = 0;

	if ((wq = port->ttycommon.t_writeq) == NULL ||
	    (mp = port->ttycommon.t_iocpending) == NULL)
		return;

	enterq(wq);
	port->ttycommon.t_iocpending = NULL;
	if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0)
		port->ioctl_id = bufcall(datasize, BPRI_HI, mic_reioctl, port);
	else
		qreply(wq, mp);
	leaveq(wq);
}

/*
 * mic_intr() is the High Level Interrupt Handler.
 *
 * There are five different interrupt types:
 *		0: Tx FIFO is ready for more data
 *		1: Rx FIFO has sufficient data to be picked up
 *		2: Rx FIFO has stale data needing attention
 *		3: SCC interrupt (modem interrupt)
 *		4: Tx FIFO and SCC are empty (fully drained)
 */
static u_int
mic_intr(caddr_t arg)
{
	mic_unit_t	*unitp = (mic_unit_t *)arg;
	mic_port_t	*port;
	int		i;
	uchar_t		interrupt;
	int		rc = DDI_INTR_UNCLAIMED;

	/* Check all ports */
	for (i = 0; i < PORTS; i++) {
		port = unitp->port[i];
		interrupt = port->stat_regs->isr & port->intr_en;
		if (interrupt) {
			errp2("mic_intr: interrupt = %x\n", interrupt);
			rc = DDI_INTR_CLAIMED;
			if (interrupt & (MIC_RXWATER_I | MIC_TIMEOUT_I)) {
				mutex_enter(&port->rx_lock);
				mic_rxint(port);
				mutex_exit(&port->rx_lock);
			}

			if (interrupt & MIC_TXWATER_I) {
				mutex_enter(&port->tx_lock);
				mic_txint(port);
				mutex_exit(&port->tx_lock);
			}

			if (interrupt & MIC_SCC_I) {
				/*EMPTY*/
				errp("mic_intr: SCC interrupt\n");
				/*
				 * TTY: Here is where we would get notified
				 * of modem status changes (assuming we
				 * cared and enabled the interrupt).  Then
				 * we would send up M_HANGUP's, M_UNHANGUP's
				 * using putnextctl(), or other notifications.
				 */
			}

			if (interrupt & MIC_TXEMPTY_I) {
				errp("mic_intr: fully drained\n");
				mutex_enter(&port->tx_lock);
				TXEMPTY_OFF(port)
				if (port->flags & PORT_IRBUSY) {
					port->flags &= ~PORT_IRBUSY;
					port->ctl_regs->misc_ctl |= MIC_CLR_RX;
					errp("Rx on\n");
				}
				port->flags |= PORT_TXDONE | PORT_TXCHK;
				mutex_exit(&port->tx_lock);
				ddi_trigger_softintr(port->unitp->soft_id);
			}
		}
	}
	return (rc);
}

/*
 * Transmitter interrupt service routine.
 */
static void
mic_txint(void *arg)
{
	mic_port_t *port = arg;
	mblk_t	*mp = port->tx_blk;
	int	count;
	uchar_t	mask = 0xff;

	ASSERT(mutex_owned(&port->tx_lock));
	TXINTR_OFF(port)

	if (!mp) {
		errp("mic_txint: finished msg\n");
		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		return;
	}

	if (port->flags & (PORT_DELAY | PORT_BREAK)) {
		errp("mic_txint: delay or break done\n");
		port->flags &= ~(PORT_BREAK | PORT_DELAY);
		port->scc_regs->lcr &= ~SETBREAK;
		ASSERT(mp);
		freemsg(mp);
		port->tx_blk = NULL;
		port->flags |= PORT_TXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
		return;
	}

	if ((port->flags & (PORT_IR | PORT_LPBK | PORT_IRBUSY)) == PORT_IR) {
		mutex_enter(&port->rx_lock);
		mic_rxint(port);
		port->flags |= PORT_IRBUSY;
		mutex_exit(&port->rx_lock);
		errp("Rx off\n");
	}

	/*
	 * Transmit (more of) the message
	 */
	count = 64 - port->stat_regs->tx_cnt;
	errp2("mic_txint: msg = %d bytes", mp->b_wptr - mp->b_rptr);
	errp2(", tx_space = %d\n", count);
	/*
	 * In 5-bit mode, the high order bits are used
	 * to indicate character sizes less than five,
	 * so we need to explicitly mask before transmitting
	 */
	if ((port->ttycommon.t_cflag & CSIZE) == CS5)
		mask = (uchar_t)0x1f;
	while (count-- && mp->b_rptr < mp->b_wptr) {
		port->ctl_regs->tx_data = (*mp->b_rptr++) & mask;
	}

	TXINTR_ON(port)
	if ((port->flags & (PORT_IR | PORT_LPBK)) == PORT_IR)
		TXEMPTY_ON(port)

	if (mp->b_rptr == mp->b_wptr) {
		freemsg(mp);
		port->tx_blk = NULL;
	}
}

/*
 * Receiver interrupt: FIFO has reached water mark, or data in FIFO got
 * stale.
 */
static void
mic_rxint(mic_port_t *port)
{
	uchar_t		data, error;
	int		data_cnt;
	mblk_t		*mp = port->rx_blk;

	ASSERT(mutex_owned(&port->rx_lock));
	ASSERT(mp && mp->b_wptr);

	if ((data_cnt = port->stat_regs->rx_cnt) == 0)
		return;
	errp2("mic_rxint: data count = %d\n", data_cnt);
	if (port->flags & PORT_IRBUSY) {
		port->ctl_regs->misc_ctl |= MIC_CLR_RX;
		return;
	}
	if (mp->b_wptr + data_cnt >= mp->b_datap->db_lim) {
		if (mp->b_datap->db_lim - 1 == mp->b_wptr) {
			cmn_err(CE_WARN, "mic: Rx buffer full - draining FIFO");
			port->ctl_regs->misc_ctl |= MIC_CLR_RX;
			return;
		} else {
			errp("mic: buffer tmp full");
			data_cnt = mp->b_datap->db_lim - mp->b_wptr - 1;
		}
	}
	while (data_cnt--) {
		error = port->stat_regs->rx_stat;
		data = port->stat_regs->rx_data;

		switch (error) {
		case MIC_EMPTY:
			errp2("mic_rxint: No data (%c)\n", data);
			break;

		case MIC_VALID:
			*port->rx_blk->b_wptr++ = data;
			break;

		case MIC_FRAME:
		case MIC_PARITY:
		case MIC_PARFR:
			errp2("mic_rxint: Bad character (%x)\n", error);
			break;

		case MIC_BREAK:
			errp("mic_rxint: Break\n");
			break;

		case MIC_OVERRUN:
			cmn_err(CE_WARN, "mic: Rx FIFO Overflow");
			break;
		}
	}
	if (port->rx_blk->b_wptr > port->rx_blk->b_rptr) {
		port->flags |= PORT_RXCHK;
		ddi_trigger_softintr(port->unitp->soft_id);
	}
}

/*
 * Retry to get a message block to send up
 */
void
mic_resend(void *arg)
{
	mic_port_t *port = arg;
	mblk_t	*new_mp, *old_mp;

	port->send_id = 0;
	new_mp = allocb(RX_BUFFER_SZ, BPRI_LO);
	if (new_mp == NULL) {
		port->send_id = bufcall(2 * RX_WATER_MARK_HI,
		    0, mic_resend, port);
	} else {
		mutex_enter(&port->rx_lock);
		old_mp = port->rx_blk;
		port->rx_blk = new_mp;
		mutex_exit(&port->rx_lock);
		putnext(port->ttycommon.t_readq, old_mp);
	}
}

/*
 * Set up the mic specific registers to a sensible state
 */
static void
mic_program(mic_port_t *port)
{
	/*
	 * Disable interrupts
	 */
	port->ctl_regs->gen_ie = 0;
	port->intr_en = 0;

	/*
	 * Program FIFO's
	 */
	port->ctl_regs->misc_ctl = MIC_CLR_RX | MIC_CLR_TX;
	port->ctl_regs->rx_water = RX_WATER_MARK_HI;
	port->ctl_regs->tx_water = TX_WATER_MARK;
	port->ctl_regs->rx_timer = RX_STALE_TIME;
	port->ctl_regs->ir_mode = MIC_IR_OFF;

	/*
	 * Set Flow control to off and ready to receive
	 */
	port->ctl_regs->flw_ctl = 0;

	/*
	 * Enable programmed hardware flow control (for M_STOP, M_START)
	 */
	port->scc_regs->mcr = OUT2;
}

/*
 * Program the SCC registers to the ports specification.
 * Most of the async operation is based on the values
 * of 'c_iflag' and 'c_cflag'.
 */
static void
scc_program(mic_port_t *port)
{
	int	c_flag;
	uchar_t	lcr = 0;
	int	baud;

	ASSERT(mutex_owned(&port->tx_lock));
	c_flag = port->ttycommon.t_cflag;
	/*
	 * Enter SCC program mode to set baud rate and lcr
	 */
	port->ctl_regs->misc_ctl |= MIC_SCC_PROG_MODE;
	port->scc_regs->lcr |= DLAB;

	baud = asyspdtab[c_flag & CBAUD];
	if (port->baud_div != baud) {
		errp2("scc_program: baud = %d\n", baud);
		port->ctl_regs->ir_mode = MIC_IR_OFF;
		port->scc_regs->dll = baud & 0xff;
		port->scc_regs->dlm = baud >> 8;
		port->baud_div = baud;
		/*
		 * Delay for 2 bits, while SCC clock stabilises
		 */
		drv_usecwait(2 * 1600 * baud / 1966);
	}

	/*
	 * Set line control
	 */
	switch (c_flag & CSIZE) {
	case CS5:
		lcr |= BITS5;
		break;
	case CS6:
		lcr |= BITS6;
		break;
	case CS7:
		lcr |= BITS7;
		break;
	case CS8:
		lcr |= BITS8;
		break;
	}
	if (c_flag & CSTOPB)
		lcr |= STB2;		/* 2 stop bits */

	if (c_flag & PARENB) {
		lcr |= PEN;
		if (!(c_flag & PARODD))
			lcr |= EVENPAR;
	}
	/*
	 * Leave SCC program mode
	 */
	port->scc_regs->lcr = lcr;
	port->ctl_regs->misc_ctl = 0;
	errp2("scc_program: lcr = 0x%x\n", lcr);

	port->ctl_regs->ir_mode = port->ir_mode;
	port->ctl_regs->ir_div = port->ir_divisor;

	/*
	 * TTY: If we want modem stuff then:
	 *	- CTS/RTS
	 *	- DTR/CD/RI
	 */
	port->scc_regs->ier = 0;

	if (c_flag & CREAD)
		RXINTR_ON(port)
	else
		RXINTR_OFF(port)
	port->ctl_regs->gen_ie |= MIC_MASTER_IE;
}
