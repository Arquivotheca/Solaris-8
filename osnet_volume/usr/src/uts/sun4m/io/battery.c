/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)battery.c	1.19	97/10/22 SMI"

/*
 * Battery STREAMS module.
 *
 * This module sits above a serial port connected to a Voyager battery
 * (model number LIP-7). It provides the battery interface defined in
 * common/sys/battery.h
 */
#include <sys/stream.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>		/* per 494 change */
#include <sys/debug.h>		/* per 494 change */
#include <sys/termios.h>	/* For termio structure in battopen() */
#include <sys/conf.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/battery.h>
#include <sys/modctl.h>

extern int	hz;		/* System global variable */

#ifdef trace_on
#define	trace1(a)		printf(a)
#define	trace2(a, b)		printf(a, b)
static void			prt_msg(mblk_t *);
#else
#define	trace1(a)
#define	trace2(a, b)
#define	prt_msg(x)
#endif


/*
 * Structures and stuff specifically for the battery and this module
 */
#define	TIMEOUT		1 * hz		/* Time b/w commands */
#define	RETRIES		2		/* Number of *retries* per request */
#define	MODEL		0x0a		/* Model of battery we like */

#define	_BATTERY_MODID	0x321	/* The module id (allegedly unique!) */

typedef enum batt_states {
	MOD_CLOSED,
	MOD_OPENING,
	MOD_OPEN,
	MOD_CLOSING,
	MOD_ERROR
} batt_states_t;

/*
 * battery module soft state
 */
static kmutex_t		battery_lock;	/* protects all battery soft state */
static kcondvar_t	battery_cv;	/* when waiting for state transitions */
static batt_states_t	battery_state;	/* state of battery module */
static timeout_id_t	battery_id;	/* timeout id */
static int		battery_retry;	/* number of retries */
static battery_t	battery_info;	/* battery information */
static int		battery_voltage;	/* battery voltage */
static int		battery_cmd;	/* next battery command */
static uint		battery_iocid;	/* Outstanding ioctl ID */

static u_char		battery_cmds[] =  {
#define	MODEL_CMD	0
	0x10,				/* Battery Model Query */
#define	TOTAL_CMD	1
	0x20,				/* Battery Full Capacity */
#define	CAPACITY_CMD	2
	0x21,				/* Battery Capacity */
#define	VOLTAGE_CMD	3
	0x23,				/* Battery Voltage */
#define	CURRENT_CMD	4
	0x24,				/* Battery Current */
#define	CHARGE_CMD	5
	0x25,				/* Battery Charge */
#define	STATUS_CMD	6
	0x26				/* Battery Status */
};
#define	BATT_NUM_CMDS	(sizeof (battery_cmds))

/*
 * The loadable module wrapper.
 */
extern struct streamtab	battinfo;

static struct fmodsw fsw = {
	"battery",
	&battinfo,
	D_NEW | D_MP
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"battery module",
	&fsw
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlstrmod, NULL
};

_init(void)
{
	int	e;

	trace1("battery: init\n");
	if ((e = mod_install(&modlinkage)) == 0) {
		mutex_init(&battery_lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&battery_cv, NULL, CV_DEFAULT, NULL);
		battery_state = MOD_CLOSED;
		strcpy(battery_info.id_string, "Voyager Battery (LIP-7)");
	}
	return (e);
}

_fini(void)
{
	int	e;

	trace1("battery: fini\n");
	if ((e = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&battery_lock);
	}
	return (e);
}

_info(struct modinfo *modinfop)
{
	trace1("battery: info\n");
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Forward declarations of private routines.
 */
static int	batt_open();
static int	batt_close();
static int	batt_rput(queue_t *, mblk_t *);
static int	batt_wput(queue_t *, mblk_t *);
static void	batt_timeout(void *);
static void	battery_poll(queue_t *);

/*
 * Read and write queue information/configuration structures
 */
static struct module_info rminfo = {
	_BATTERY_MODID,
	"battery",
	0,
	INFPSZ,
	0,
	0
};

static struct module_info wminfo = {
	_BATTERY_MODID,
	"battery",
	0,
	INFPSZ,
	STRLOW,
	STRHIGH
};

static struct qinit battrinit = {
	batt_rput,	/* put */
	NULL,		/* service */
	batt_open,	/* open */
	batt_close,	/* close */
	NULL,		/* qadmin */
	&rminfo,
	NULL		/* mstat */
};

static struct qinit battwinit = {
	batt_wput,	/* put */
	NULL,		/* service */
	NULL,		/* open */
	NULL,		/* close */
	NULL,		/* qadmin */
	&wminfo,
	NULL		/* mstat */
};

struct streamtab battinfo = {
	&battrinit,
	&battwinit,
	NULL,
	NULL
};

/*ARGSUSED1*/
static int
batt_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	mblk_t		*mp, *nmp;
	struct termios	*t;
	struct iocblk	*iocp;

	trace1("batt_open: begin\n");
	mutex_enter(&battery_lock);
	while (battery_state != MOD_OPEN) {
		switch (battery_state) {
		case MOD_CLOSED:
			trace1("batt_open: opening...\n");
			battery_state = MOD_OPENING;
			battery_retry = 0;
			battery_cmd = -1;
			battery_info.total = -1;
			battery_info.capacity = -1;
			battery_info.discharge_rate = -1;
			battery_info.discharge_time = -1;
			battery_info.status = NOT_PRESENT;
			battery_info.charge = -1;

			/*
			 * Create message blocks to configure serial port to
			 * talk to battery
			 */
			mp = mkiocb(TCSETS);
			if (mp == NULL) {
				mutex_exit(&battery_lock);
				return (ENOMEM);
			}
			nmp = allocb(sizeof (struct termios), 0);
			if (nmp == NULL) {
				freemsg(mp);
				mutex_exit(&battery_lock);
				return (ENOMEM);
			}

			iocp = (struct iocblk *)mp->b_rptr;
			iocp->ioc_cr = cred;
			iocp->ioc_count = sizeof (struct termios);

			/* Save this for dealing with the reply */
			battery_iocid = iocp->ioc_id;

			/* Set baud rate to 4800, 8 data bits */
			t = (struct termios *)nmp->b_rptr;
			nmp->b_wptr += sizeof (struct termios);
			bzero((char *)t, sizeof (struct termios));
			t->c_cflag = CS8 | CREAD | B4800 | PARENB | PARODD;
			linkb(mp, nmp);
			prt_msg(mp);
			battery_id = timeout(batt_timeout, WR(rq), TIMEOUT);

			mutex_exit(&battery_lock);
			qprocson(rq);
			putnext(WR(rq), mp);
			mutex_enter(&battery_lock);
			break;

		case MOD_ERROR:
			battery_state = MOD_CLOSED;
			mutex_exit(&battery_lock);
			return (EIO);

		default:
			trace1("batt_open: waiting for state\n");
			cv_wait(&battery_cv, &battery_lock);
			break;
		}

	}
	mutex_exit(&battery_lock);
	trace1("batt_open: done\n");
	return (0);
}

/*ARGSUSED1*/
static int
batt_close(queue_t *rq, int flag, cred_t *cred)
{
	trace1("batt_close: closing\n");
	mutex_enter(&battery_lock);
	battery_state = MOD_CLOSING;
	while (battery_state != MOD_CLOSED)
		cv_wait(&battery_cv, &battery_lock);
	mutex_exit(&battery_lock);
	qprocsoff(rq);
	trace1("batt_close: done\n");
	return (0);
}

/*
 * Messages from downstream
 */
/*ARGSUSED*/
static int
batt_rput(queue_t *rq, mblk_t *mp)
{
	struct iocblk   *iocp;
	u_char		r;

	trace1("batt_rput: message->");
	prt_msg(mp);
	if (battery_id == 0) {
		trace1("batt_rput: Unexpected message (discarded)\n");
		freemsg(mp);
		return (0);
	}

	switch (mp->b_datap->db_type) {
	case M_IOCACK:
		iocp = (struct iocblk *)mp->b_rptr;
		if (iocp->ioc_id != battery_iocid)
			break;

		trace1("batt_rput: configuration succeeded\n");
		mutex_enter(&battery_lock);
		ASSERT(battery_state == MOD_OPENING);
		ASSERT(battery_cmd == -1);
		ASSERT(battery_info.status == NOT_PRESENT);
		battery_state = MOD_OPEN;
		battery_id = 0;
		cv_broadcast(&battery_cv);
		mutex_exit(&battery_lock);
		break;

	case M_DATA:
		r = (u_char)*mp->b_rptr;
		/* ASSERT(mp->b_wptr - mp->b_rptr == 1); */
		if (mp->b_wptr - mp->b_rptr != 1) {
			cmn_err(CE_NOTE, "unkown message.");
			freemsg(mp);
			return (0);
		}
		mutex_enter(&battery_lock);
		trace2("batt_rput: command %d\n", battery_cmd);
		ASSERT(battery_state == MOD_OPEN ||
		    battery_state == MOD_CLOSING);
		battery_id = 0;
		switch (battery_cmd) {
		case MODEL_CMD:
			if (r != MODEL) {
				trace1("batt_rput: battery not identified\n");
				battery_info.status = NOT_PRESENT;
				battery_info.capacity = -1;
				battery_info.discharge_rate = -1;
				battery_info.discharge_time = -1;
				battery_info.charge = -1;
				battery_cmd = -1;
			}
			break;

		case TOTAL_CMD:
			battery_info.total = r * 250;
			break;

		case CAPACITY_CMD:
			battery_info.capacity = (char)((int)(25000 * r) /
			    battery_info.total);
			break;

		case VOLTAGE_CMD:
			battery_voltage = r;
			break;

		case CURRENT_CMD:
			if (battery_info.charge != DISCHARGE) {
				battery_info.discharge_rate = -1;
				battery_info.discharge_time = -1;
				break;
			}
			battery_info.discharge_rate = r * battery_voltage * 10;
			if (battery_info.discharge_rate != 0)
				battery_info.discharge_time = (int)(36 *
				    battery_info.capacity *
				    battery_info.total) /
				    battery_info.discharge_rate;
			break;

		case CHARGE_CMD:
			if (r == 0x04) {
				battery_info.charge = DISCHARGE;
			} else {
				battery_info.charge = TRICKLE_CHARGE;
				battery_info.discharge_rate = -1;
				battery_info.discharge_time = -1;
			}
			break;

		case STATUS_CMD:
			if (r > 0x10)
				battery_info.status = EOL;
			else if ((r & 0x0c) || battery_info.capacity <= 0)
				battery_info.status = EMPTY;
			else if (r == 0x01 || battery_info.capacity <= 25)
				battery_info.status = LOW_CAPACITY;
			else if (battery_info.capacity <= 50)
				battery_info.status = MED_CAPACITY;
			else if (battery_info.capacity <= 75)
				battery_info.status = HIGH_CAPACITY;
			else
				battery_info.status = FULL_CAPACITY;
		}
		mutex_exit(&battery_lock);
		break;

	default:
		/*
		 * Assume a "wrong" message is the same as no message
		 * and let the timeout mechanism take action. e.g.
		 * this can happen for M_IOCNAK's
		 */
		trace1("batt_rput: message discarded\n");
	}
	freemsg(mp);
	return (0);
}

/*
 * battwput :
 *	- Check for high priority messages.
 *	- Answer battery status ioctls
 *	- Discard all else.
 */
static int
batt_wput(queue_t *wq, mblk_t *mp)
{
	struct iocblk	*iocbp;
	struct copyreq	*cqp;

	trace1("batt_wput: message->");
	prt_msg(mp);

	switch (mp->b_datap->db_type) {
	case M_RSE:
	case M_PCRSE:
		putnext(wq, mp);
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq(wq, FLUSHALL);
		putnext(wq, mp);
		break;

	case M_IOCTL:
		iocbp = (struct iocblk *)mp->b_rptr;
		if (iocbp->ioc_cmd != BATT_STATUS ||
		    iocbp->ioc_count != TRANSPARENT) {
			trace1("battwput: unknown ioctl (IOCNAK)\n");
			mp->b_datap->db_type = M_IOCNAK;
			iocbp->ioc_error = ENOTTY;
			qreply(wq, mp);
			break;
		}
		trace1("batt_wput: starting copyout\n");
		cqp = (struct copyreq *)mp->b_rptr;
		cqp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
		cqp->cq_size = sizeof (battery_t);
		cqp->cq_flag = 0;
		if (mp->b_cont)
			freemsg(mp->b_cont);
		mp->b_cont = allocb(sizeof (battery_t), BPRI_MED);
		if (mp->b_cont == NULL) {
			mp->b_datap->db_type = M_IOCNAK;
			iocbp->ioc_error = EAGAIN;
			qreply(wq, mp);
			break;
		}
		mutex_enter(&battery_lock);
		ASSERT(battery_state == MOD_OPEN);
		bcopy((char *)&battery_info, (char *)mp->b_cont->b_rptr,
		    sizeof (battery_t));
		mutex_exit(&battery_lock);

		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (battery_t);
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(wq, mp);
		break;

	case M_IOCDATA:
		iocbp = (struct iocblk *)mp->b_rptr;
		if (iocbp->ioc_cmd != BATT_STATUS) {
			cmn_err(CE_WARN, "battery: Unexpected ioctl data\n");
			freemsg(mp);
			break;
		}
		mp->b_datap->db_type = M_IOCACK;
		mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
		iocbp->ioc_error = 0;
		iocbp->ioc_count = 0;
		iocbp->ioc_rval = 0;
		qreply(wq, mp);
		trace1("batt_wput: copyout done\n");
		break;

	default:
		cmn_err(CE_WARN, "battery: Unknown command (discarded)");
		freemsg(mp);
	}
	return (0);
}

static void
batt_timeout(void *arg)
{
	queue_t *wq = arg;

	trace1("batt_timeout: timeout expired\n");
	mutex_enter(&battery_lock);
	switch (battery_state) {
	case MOD_OPENING:
		ASSERT(battery_id != 0);
		battery_id = 0;
		trace1("batt_timeout: configuration failed");
		battery_state = MOD_ERROR;
		cv_broadcast(&battery_cv);
		mutex_exit(&battery_lock);
		break;

	case MOD_OPEN:
		if (battery_id != 0) {
			trace1("batt_timeout: retrying\n");
			if (battery_retry == RETRIES) {
				trace1("batt_timeout: retry count exceeded\n");
				battery_info.status = NOT_PRESENT;
				battery_info.capacity = -1;
				battery_info.discharge_rate = -1;
				battery_info.discharge_time = -1;
				battery_info.charge = -1;
			}
			battery_cmd = -1;
			battery_retry++;
		} else
			battery_retry = 0;
		mutex_exit(&battery_lock);
		battery_poll(wq);
		break;

	case MOD_CLOSING:
		trace1("batt_timeout: while closing\n");
		battery_state = MOD_CLOSED;
		cv_broadcast(&battery_cv);
		mutex_exit(&battery_lock);
		break;

	default:
		mutex_exit(&battery_lock);
		cmn_err(CE_WARN, "batt_timeout: Unknown timer");
		battery_poll(wq);
	}
}

static void
battery_poll(queue_t *wq)
{
	mblk_t		*mp;

	ASSERT(MUTEX_NOT_HELD(&battery_lock));
	if ((mp = allocb(1, 0)) == NULL)
		return;

	mutex_enter(&battery_lock);
	battery_cmd++;
	if (battery_cmd == BATT_NUM_CMDS)
		battery_cmd = 2;		/* skip static queries */
	*mp->b_wptr++ = (char)battery_cmds[battery_cmd];
	mutex_exit(&battery_lock);

	trace1("battery_poll: sending->");
	prt_msg(mp);
	battery_id = timeout(batt_timeout, wq, TIMEOUT);
	putnext(wq, mp);
}

#ifdef trace_on
static void
prt_msg(mblk_t  *mp)
{
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	while (mp != NULL) {
		switch (mp->b_datap->db_type) {
		case M_DATA:
			trace2("M_DATA[%x]->", *mp->b_rptr);
			break;
		case M_IOCTL:
			trace2("M_IOCTL[%x]->", iocp->ioc_cmd);
			break;
		case M_IOCACK:
			trace2("M_IOCACK[%x]->", iocp->ioc_rval);
			break;
		case M_IOCNAK:
			trace2("M_IOCNAK[%x]->", iocp->ioc_rval);
			break;
		case M_IOCDATA:
			trace2("M_IOCDATA[%x]->", iocp->ioc_rval);
			break;
		case M_COPYOUT:
			trace2("M_COPYOUT[%d]->", mp->b_wptr - mp->b_rptr);
			break;
		default:
			trace1("OTHER->");
			break;
		}
		mp = mp->b_cont;
	}
	trace1("NULL\n");
}
#endif
