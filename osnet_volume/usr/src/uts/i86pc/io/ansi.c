/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ansi.c	1.12	99/05/20 SMI"

/*
 * IWE ANSI module; parse for character input string for
 * ANSI X3.64 escape sequences and the like. Translate into
 * TCL (terminal control language) comands to send to the
 * principal stream (display) linked under CHANMUX
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/at_ansi.h>
#include <sys/vt.h>
#include <sys/ascii.h>
#include <sys/termios.h>
#include <sys/strtty.h>
#include <sys/kd.h>
#include <sys/ws/chan.h>
#include <sys/ws/tcl.h>
#include <sys/ansi.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	A_STATE_START			0
#define	A_STATE_ESC			1
#define	A_STATE_ESC_Q			2
#define	A_STATE_ESC_Q_DELM		3
#define	A_STATE_ESC_Q_DELM_CTRL		4
#define	A_STATE_ESC_C			5
#define	A_STATE_CSI			6
#define	A_STATE_CSI_QMARK		7
#define	A_STATE_CSI_EQUAL		8


static int ansi_open(queue_t *qp, dev_t *devp, int oflag, int sflag,
	cred_t *crp);
static int ansi_close(queue_t *qp, int flag, cred_t *crp);
static int ansi_read_queue_put(queue_t *qp, mblk_t *mp);
static int ansi_write_queue_put(queue_t *qp, mblk_t *mp);
static int ansi_write_queue_serv(queue_t *qp);

static void ansi_equal_state(ansistat_t *ap, u_char ch);
static void ansi_proc_w_data(queue_t *qp, mblk_t *mp, ansistat_t *ap);
static void ansi_parse(ansistat_t *ap, u_char ch);
static void ansi_control(ansistat_t *ap, u_char ch);
static void ansi_setparam(ansistat_t *ap, int count, u_short newparam);
static void ansi_send_1ctl(ansistat_t *ap, u_long cmd, u_long data,
	u_long type);
static void ansi_selgraph(ansistat_t *ap);
static void ansi_chkparam(ansistat_t *ap, u_char ch);
static void ansi_getparams(ansistat_t *ap, u_char ch);
static void ansi_outch(ansistat_t *ap, u_char ch);
static void ansi_stat_init(queue_t *qp, ansistat_t *ap);
static void ansi_mvcursor(ansistat_t *ap, short x_coord, u_char x_type,
	short y_coord, u_char y_type);
static void ansi_addstring(ansistat_t *ap, u_short keynum, char *buf,
	u_short len);

int ansi_debug = 0;

#define	DEBUG1(a)	if (ansi_debug == 1) printf a
#define	DEBUG2(a)	if (ansi_debug >= 2) printf a /* allocations */
#define	DEBUG3(a)	if (ansi_debug >= 3) printf a /* M_CTL Stuff */
#define	DEBUG4(a)	if (ansi_debug >= 4) printf a /* M_READ Stuff */
#define	DEBUG5(a)	if (ansi_debug >= 5) printf a
#define	DEBUG6(a)	if (ansi_debug >= 6) printf a

/*
 * Since most of the buffering occurs either at the stream head or in
 * the "message currently being assembled" buffer, we have no
 * write side queue. Ditto for the read side queue since we have no
 * read side responsibilities.
 */

static struct module_info ansi_iinfo = {
	0,
	"ansi",
	0,
	ANSIPSZ,
	1000,
	100
};

static struct qinit ansi_rinit = {
	ansi_read_queue_put,
	NULL,
	ansi_open,
	ansi_close,
	NULL,
	&ansi_iinfo
};

static struct module_info ansi_oinfo = {
	0,
	"ansi",
	0,
	ANSIPSZ,
	1000,
	100
};

static struct qinit ansi_winit = {
	ansi_write_queue_put,
	ansi_write_queue_serv,
	ansi_open,
	ansi_close,
	NULL,
	&ansi_oinfo
};

struct streamtab ansi_str_info = {
	&ansi_rinit,
	&ansi_winit,
	NULL,
	NULL
};

kmutex_t		ansi_lock;

/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

/*
 * D_MTPERQ effectively makes the module double threaded.
 * There can be only two threads active in the module at any time.
 * There can be only one thread active on the read side at any time and
 * there can be only one thread active on the write side at any time.
 */
#define	ANSI_CONF_FLAG	(D_NEW | D_MTPERQ | D_MP)

static struct fmodsw	fsw = {
	"ansi",
	&ansi_str_info,
	ANSI_CONF_FLAG
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"KD ANSI code parser",
	&fsw
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};

int
_init(void)
{
	int	rc;

	mutex_init(&ansi_lock, NULL, MUTEX_DEFAULT, NULL);
	if ((rc = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&ansi_lock);
	}
	return (rc);
}

int
_fini(void)
{
	int rc;

	if ((rc = mod_remove(&modlinkage)) == 0)
		mutex_destroy(&ansi_lock);
	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#define	TWO_PARMS(ap)	\
	((((ap->a_paramval&0x7fff)) << 16)|(ap->a_params[0]&0x7fff))

/*
 * ansi_stat_init initializes the queue pair associated with qp --
 * we assume that qp is a read queue ptr -- and the state structure
 * pointed to by ap. We set back ptrs from ap to the read and write
 * queues and set the queues' private data structure ptr to ap.
 * Also, the assorted fields of ap are initialized.
 */

static void
ansi_stat_init(queue_t *qp, ansistat_t *ap)
{
	int j;

	mutex_enter(&ansi_lock); /* protect against multiple opens */

	if (qp->q_ptr != NULL) {
		/* already allocated state structure */
		mutex_exit(&ansi_lock);
		kmem_free(ap, sizeof (ansistat_t));
		return;
	}

	/* set q_ptr for read and write queue */

	qp->q_ptr = ap;
	WR(qp)->q_ptr = ap;

	mutex_exit(&ansi_lock);

	/* set queue ptrs in state structure to read/write queue */

	ap->a_rqp = qp;
	ap->a_wqp = WR(qp);
	ap->a_wmsg = NULL;

	/* initialize state fields */

	ap->a_state = A_STATE_START;
	ap->a_gotparam = 0;
	ap->a_curparam = 0;
	ap->a_paramval = 0;
	ap->a_font = ANSI_FONT0;
	ap->a_flags = 0;
	for (j = 0; j < ANSI_MAXPARAMS; j++)
		ap->a_params[j] = 0;

}


/*
 * Character module open. Allocate per-stream state structure. Return an error
 * number for failure, 0 otherwise.
 */

/*ARGSUSED1*/
static int
ansi_open(queue_t *qp, dev_t *devp, int oflag, int sflag, cred_t *crp)
{
	ansistat_t *ap;


	if (qp->q_ptr != NULL) {
		return (0);		/* already attached */
	}

	/* allocate and initialize state structure */
	ap = kmem_alloc(sizeof (ansistat_t), KM_SLEEP);
	ansi_stat_init(qp, ap);

	qprocson(qp);
	return (0);
}



/*
 * close routine. Deallocate any stashed messages and state structure for
 * that queue pair.
 */

/*ARGSUSED1*/
static int
ansi_close(queue_t *qp, int flag, cred_t *crp)
{
	ansistat_t *ap = (ansistat_t *)qp->q_ptr;

	qprocsoff(qp);
	mutex_enter(&ansi_lock);

	if (ap->a_wmsg != NULL) {
		freemsg(ap->a_wmsg);
		ap->a_wmsg = NULL;
	}

	/* Dump the state structure, then unlink it */
	kmem_free(ap, sizeof (ansistat_t));
	qp->q_ptr = NULL;

	mutex_exit(&ansi_lock);
	return (0);
}

/*
 * Put procedure for input from driver end of stream (read queue). We don't
 * do any readside processing, so simply call putnext() to forward the
 * data.
 */

static int
ansi_read_queue_put(queue_t *qp, mblk_t *mp)
{
	switch (mp->b_datap->db_type) {

	default:
		putnext(qp, mp);
		return (0);

	case M_PROTO:
	case M_PCPROTO:
		if ((mp->b_wptr - mp->b_rptr) != sizeof (ch_proto_t))
			putnext(qp, mp);
		else
			freemsg(mp);
	}
	return (0);
}



/*
 * write side message processing.  Call the appropriate handler for
 * each message type.
 */

static int
ansi_write_queue_put(queue_t *qp, mblk_t *mp)
{
	int type;

	type = mp->b_datap->db_type;

	if (type > QPCTL) {
		switch (type) {
		default:
			putnext(qp, mp);
			return (0);

		case M_START:
		case M_STOP:
		case M_FLUSH:
			(void) putq(qp, mp);
			return (0);
		}
	} else
		(void) putq(qp, mp);
	return (0);
}


static int
ansi_write_queue_serv(queue_t *qp)
{

	mblk_t *mp;
	ansistat_t *ap;

	ap = qp->q_ptr;

	while ((mp = getq(qp)) != NULL) {
		if (mp->b_datap->db_type <= QPCTL &&
		    ((ap->a_flags & ANSI_BLKOUT) || !canputnext(qp))) {
			(void) putbq(qp, mp);
			return (0);	/* read side is blocked */
		}

		switch (mp->b_datap->db_type) {

		default:
			putnext(qp, mp);	/* pass it on */
			break;

		case M_DATA:
			ansi_proc_w_data(qp, mp, ap);
			break;

		case M_FLUSH:
			/*
			 * Flush everything we haven't looked at yet.
			 */
			flushq(qp, FLUSHDATA);
			putnext(qp, mp); /* pass it on */
			break;

		case M_START:
			ap->a_flags &= ~ANSI_BLKOUT;
			ansi_send_1ctl(ap, TCL_FLOWCTL, TCL_FLOWOFF,
			    M_PCPROTO);
			continue;

		case M_STOP:
			ap->a_flags |= ANSI_BLKOUT;
			ansi_send_1ctl(ap, TCL_FLOWCTL, TCL_FLOWON, M_PCPROTO);
			continue;

		} /* switch */
	}
	return (0);
}


/*
 * For each data message coming downstream, ANSI assumes that it is composed
 * of ASCII characters, which are treated as a byte-stream input to the
 * parsing state machine. All data is parsed immediately -- there is
 * no enqueing. Data and Terminal Control Language commands obtained from
 * parsing are sent in the same order in which they occur in the data.
 */

static void
ansi_proc_w_data(queue_t *qp, mblk_t *mp, ansistat_t *ap)
{

	mblk_t *bp;

	bp = mp;

	/*
	 * Parse each data block in the message. Assume b_rptr through b_wptr
	 * point to ASCII characters. Release the data message when each
	 * block has been parsed.
	 */

	while (bp) {
		while (bp->b_rptr < bp->b_wptr)
			ansi_parse(ap, *bp->b_rptr++);
		bp = bp->b_cont;
	}

	/* free the data message we created while parsing characters */
	if (ap->a_wmsg != NULL) {
		putnext(qp, ap->a_wmsg);
		ap->a_wmsg = NULL;
	}

	freemsg(mp); /* release the data message passed down to us */
}


/* sends a TCL command downstream for the principal stream to interpret */

static void
ansi_send_ctl(ansistat_t *ap, unsigned long cmd, unsigned long type)
{
	queue_t *qp;
	mblk_t *mp;
	ch_proto_t *protop;

	qp = ap->a_wqp;

	if (ap->a_wmsg) /* transmit current data message */
		putnext(qp, ap->a_wmsg);

	ap->a_wmsg = NULL;	/* no write data message alloc'd */

	if ((mp = allocb(sizeof (ch_proto_t), BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping ctl msg; cannot alloc msg block");
		return;
	}

	/*
	 * set up CHANNEL protocol message and identify it as coming from
	 * a TCL parsing module
	 */

	mp->b_wptr += sizeof (ch_proto_t);
	mp->b_datap->db_type = type;

	protop = (ch_proto_t *)mp->b_rptr;
	protop->chp_type  = CH_CTL;
	protop->chp_stype = CH_TCL;
	protop->chp_stype_cmd = cmd; /* set the command to be performed */

	putnext(qp, mp); /* ship it down */
}



/*
 * Allocate a 1-arguments TCL command to send downstream.
 */

static void
ansi_send_1ctl(ansistat_t *ap, u_long cmd, u_long data, u_long type)
{
	queue_t *qp;
	mblk_t *mp;
	ch_proto_t *protop;

	qp = ap->a_wqp; /* write queue ptr */

	if (ap->a_wmsg) {
		/* transmit current data message */
		putnext(qp, ap->a_wmsg);
		ap->a_wmsg = NULL;	/* no write data message */
						/* alloc'd now */
	}

	if ((mp = allocb(sizeof (ch_proto_t), BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping ctl msg; cannot alloc msg block");
		return;
	}


	mp->b_wptr += sizeof (ch_proto_t);
	mp->b_datap->db_type = type;
	/*
	 * set up CHANNEL protocol message and identify it as coming from
	 * a TCL parsing module
	 */
	protop = (ch_proto_t *)mp->b_rptr;
	protop->chp_type  = CH_CTL;
	protop->chp_stype = CH_TCL;
	protop->chp_stype_cmd = cmd; /* cmd to perform */
	protop->chp_stype_arg = data; /* cmd to perform */

	putnext(qp, mp);
}




/*
 * send the appropriate control message or set state based on the
 * value of the control character ch
 */

static void
ansi_control(ansistat_t *ap, u_char ch)
{
	ap->a_state = A_STATE_START;
	switch (ch) {
	case A_BEL:
		ansi_send_ctl(ap, TCL_BELL, M_PROTO);
		break;

	case A_BS:
		ansi_send_ctl(ap, TCL_BACK_SPCE, M_PROTO);
		break;

	case A_HT:
		ansi_send_ctl(ap, TCL_H_TAB, M_PROTO);
		break;

	case A_NL:
		ansi_send_ctl(ap, TCL_NEWLINE, M_PROTO);
		break;

	case A_VT:
		ansi_send_ctl(ap, TCL_V_TAB, M_PROTO);
		break;

	case A_FF:
		ansi_send_ctl(ap, TCL_DISP_RST, M_PROTO);
		break;

	case A_CR:
		ansi_send_ctl(ap, TCL_CRRGE_RETN, M_PROTO);
		break;

	case A_SO:
		ansi_send_ctl(ap, TCL_SHFT_FT_OU, M_PROTO);
		break;

	case A_SI:
		ansi_send_ctl(ap, TCL_SHFT_FT_IN, M_PROTO);
		break;

	case A_ESC:
		ap->a_state = A_STATE_ESC;
		break;
	case A_CSI:
		{
			int i;
			ap->a_curparam = 0;
			ap->a_paramval = 0;
			ap->a_gotparam = 0;
			/* clear the parameters */
			for (i = 0; i < ANSI_MAXPARAMS; i++)
				ap->a_params[i] = -1;
			ap->a_state = A_STATE_CSI;
		}
		break;
	case A_GS:
		ansi_send_ctl(ap, TCL_BACK_H_TAB, M_PROTO);
		break;

	default:
		break;
	}
}


/*
 * if parameters [0..count - 1] are not set, set them to the value of newparam.
 */

static void
ansi_setparam(ansistat_t *ap, int count, u_short newparam)
{
	int i;

	for (i = 0; i < count; i++) {
		if (ap->a_params[i] == -1)
			ap->a_params[i] = newparam;
	}
}


/*
 * select graphics mode based on the param vals stored in a_params
 */
static void
ansi_selgraph(ansistat_t *ap)
{
	short curparam;
	int count = 0;
	short param;

	curparam = ap->a_curparam;
	do {
		param = ap->a_params[count];

		/* param == -1 means no parameters, same as 0 */
		if (param == -1)
			param = 0;

		if (param == 10) {	/* Select primary font */
			ap->a_font = ANSI_FONT0;
			/* Shift font in */
			ansi_send_ctl(ap, TCL_SHFT_FT_OU, M_PROTO);
		} else if (param == 11) 	/* First alternate font */
			ap->a_font = ANSI_FONT1;

		else if (param == 12) 	/* Second alternate font */
			ap->a_font = ANSI_FONT2;

		else
			ansi_send_1ctl(ap, TCL_SET_ATTR, param, M_PROTO);
		count++;
		curparam--;

	} while (curparam > 0);


	ap->a_state = A_STATE_START;
}



/* send multi-byte move cursor TCL message to principal stream */
/*
 * x_coord, y_coord - either absolute coordinates or deltas
 * x_type, y_type - either TCL_POSABS or TCL_POSREL
 */

static void
ansi_mvcursor(ansistat_t *ap, short x_coord, u_char x_type,
	short y_coord, u_char y_type)
{
	queue_t *qp;
	mblk_t *mp;
	tcl_data_t *tp;
	ch_proto_t *protop;

	qp = ap->a_wqp; /* write queue pointer */

	if (ap->a_wmsg) /* transmit current data message */
		putnext(qp, ap->a_wmsg);

	ap->a_wmsg = NULL;	/* no write data message alloc'd */

	if ((mp = allocb(sizeof (ch_proto_t), BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping cursor move msg; cannot alloc msg block");
		return;
	}

	/*
	 * set up CHANNEL protocol message; identify it as TCL command
	 * TCL_POS_CURS.
	 */

	mp->b_wptr += sizeof (ch_proto_t);
	mp->b_datap->db_type = M_PROTO;

	protop = (ch_proto_t *)mp->b_rptr;
	protop->chp_type  = CH_CTL;
	protop->chp_stype = CH_TCL;
	protop->chp_stype_cmd = TCL_POS_CURS;

	/* allocate data block for move cursor cmd data */
	if ((mp->b_cont = allocb(sizeof (tcl_data_t), BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping cursor move msg; cannot alloc msg block");
		freemsg(mp);
		return;
	}

	/*
	 * tcl_data_t is a union including a data structure used
	 * to send down the x,y change to the principal stream
	 */
	mp->b_cont->b_wptr += sizeof (tcl_data_t);
	mp->b_cont->b_datap->db_type = M_PROTO;
	tp = (tcl_data_t *)mp->b_cont->b_rptr;

	tp->mv_curs.delta_x = x_coord;
	tp->mv_curs.x_type = x_type;
	tp->mv_curs.delta_y = y_coord;
	tp->mv_curs.y_type = y_type;

	putnext(qp, mp);
}


/*
 * perform the appropriate action for the escape sequence
 */
static void
ansi_chkparam(ansistat_t *ap, u_char ch)
{
	int i;


	switch (ch) {

	case 'k': /* select key click. NOT ANSI X3.64!! */
		if (ap->a_params[0] == 0)
			ansi_send_ctl(ap, TCL_KEYCLK_OFF, M_PROTO);
		else if (ap->a_params[0] == 1)
			ansi_send_ctl(ap, TCL_KEYCLK_ON, M_PROTO);
		break;

	case 'c':
		{ /* select cursor type. NOT ANSI X3.64!! */
			ushort i;

			ansi_setparam(ap, 1, 0);
			i = ap->a_params[0];
			if (i > 2) /* only 0, 1 and 2 are valid */
				break;
			ansi_send_1ctl(ap, TCL_CURS_TYP, i, M_PROTO);
			break;
		}

	case 'm': /* select terminal graphics mode */
		ansi_selgraph(ap);
		break;

	case '@':		/* insert char */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_INSRT_CHR, ap->a_params[0], M_PROTO);
		break;

	case 'A':		/* cursor up */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, 0, TCL_POSREL, -ap->a_params[0], TCL_POSREL);
		break;

	case 'd':		/* VPA - vertical position absolute */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, 0, TCL_POSREL, ap->a_params[0], TCL_POSABS);
		break;

	case 'e':		/* VPR - vertical position relative */
	case 'B':		/* cursor down */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, 0, TCL_POSREL, ap->a_params[0], TCL_POSREL);
		break;

	case 'a':		/* HPR - horizontal position relative */
	case 'C':		/* cursor right */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, ap->a_params[0], TCL_POSREL, 0, TCL_POSREL);
		break;

	case '`':		/* HPA - horizontal position absolute */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, ap->a_params[0], TCL_POSABS, 0, TCL_POSREL);
		break;

	case 'D':		/* cursor left */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, -ap->a_params[0], TCL_POSREL, 0, TCL_POSREL);
		break;

	case 'E':		/* cursor next line */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, 0, TCL_POSABS, ap->a_params[0], TCL_POSREL);
		break;

	case 'F':		/* cursor previous line */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, 0, TCL_POSABS, -ap->a_params[0], TCL_POSREL);
		break;

	case 'G':		/* cursor horizontal position */
		ansi_setparam(ap, 1, 1);
		ansi_mvcursor(ap, ap->a_params[0] - 1, TCL_POSABS, 0,
		    TCL_POSREL);
		break;

	case 'g':		/* clear tabs */
		ansi_setparam(ap, 1, 0);
		if (ap->a_params[0] == 3)  /* clear all tabs */
			ansi_send_ctl(ap, TCL_CLR_TABS, M_PROTO);
		if (ap->a_params[0] == 0) {
			/* clr tab at cursor */
			ansi_send_ctl(ap, TCL_CLR_TAB, M_PROTO);
		}
		break;

	case 'f':		/* HVP */
	case 'H':		/* position cursor */
		ansi_setparam(ap, 2, 1);
		ansi_mvcursor(ap, ap->a_params[1] - 1, TCL_POSABS,
		    ap->a_params[0] -1, TCL_POSABS);
		break;

	case 'i':		/* MC - Send screen to host */
		if (ap->a_params[0] == 2)
			ansi_send_ctl(ap, TCL_SEND_SCR, M_PROTO);
		break;

	case 'I':		/* NO_OP entry */
		break;

	case 'h':		/* SM - Lock keyboard */
	case 'l':		/* RM - Unlock keyboard */
		if (ap->a_params[0] == 2) {
			if (ch == 'h') /* lock */
				ansi_send_ctl(ap, TCL_LCK_KB, M_PROTO);
			else
				ansi_send_ctl(ap, TCL_UNLCK_KB, M_PROTO);
		}
		break;

	case 'J':		/* erase screen */
		ansi_setparam(ap, 1, 0);
		if (ap->a_params[0] == 0)
			/* erase cursor to end of screen */
			ansi_send_ctl(ap, TCL_ERASCR_CUR2END, M_PROTO);

		else if (ap->a_params[0] == 1)
			/* erase beginning of screen to cursor */
			ansi_send_ctl(ap, TCL_ERASCR_BEG2CUR, M_PROTO);

		else
			/* erase whole screen */
			ansi_send_ctl(ap, TCL_ERASCR_BEG2END, M_PROTO);

		break;

	case 'K':		/* erase line */
		ansi_setparam(ap, 1, 0);
		if (ap->a_params[0] == 0)
			/* erase cursor to end of line */
			ansi_send_ctl(ap, TCL_ERALIN_CUR2END, M_PROTO);

		else if (ap->a_params[0] == 1)
			/* erase beginning of line to cursor */
			ansi_send_ctl(ap, TCL_ERALIN_BEG2CUR, M_PROTO);

		else
			/* erase whole line */
			ansi_send_ctl(ap, TCL_ERALIN_BEG2END, M_PROTO);

		break;

	case 'L':		/* insert line */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_INSRT_LIN, ap->a_params[0], M_PROTO);
		break;

	case 'M':		/* delete line */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_DELET_LIN, ap->a_params[0], M_PROTO);
		break;

	case 'P':		/* delete char */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_DELET_CHR, ap->a_params[0], M_PROTO);
		break;

	case 'S':		/* scroll up */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_SCRL_UP, ap->a_params[0], M_PROTO);
		break;

	case 'T':		/* scroll down */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_SCRL_DWN, ap->a_params[0], M_PROTO);
		break;

	case 'X':		/* erase char */
		ansi_setparam(ap, 1, 1);
		ansi_send_1ctl(ap, TCL_DISP_CLR, ap->a_params[0], M_PROTO);
		break;

	case 'Z':		/* cursor backward tabulation */
		ansi_setparam(ap, 1, 1);
		for (i = 0; i < ap->a_params[0]; i++)
			ansi_send_ctl(ap, TCL_BACK_H_TAB, M_PROTO);
		break;
	case 'z':
		ansi_send_1ctl(ap, TCL_SWITCH_VT, ap->a_params[0], M_PROTO);
		break;
	}
	ap->a_state = A_STATE_START;
}



/*
 * send a CHANNEL proto message to change the function key string
 * This message will actually be handled by CHAR or its equivalent,
 * which must be pushed below ANSI.
 */

static void
ansi_addstring(ansistat_t *ap, u_short keynum, char *buf, u_short len)
{
	queue_t *qp;
	mblk_t *mp;
	tcl_data_t *tp;
	ch_proto_t *protop;

	/* return automatically if buf or len not set */
	if (!len || buf == NULL)
		return;

	qp = ap->a_wqp;

	if (ap->a_wmsg) /* xmit current data message */
		putnext(qp, ap->a_wmsg);

	ap->a_wmsg = NULL;	/* no write data message alloc'd */

	if ((mp = allocb(sizeof (ch_proto_t), BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping cursor move msg; cannot alloc msg block");
		return;
	}


	/* make CHANNEL control message of type TCL_ADD_STR */
	mp->b_wptr += sizeof (ch_proto_t);
	mp->b_datap->db_type = M_PROTO;

	protop = (ch_proto_t *)mp->b_rptr;
	protop->chp_type  = CH_CTL;
	protop->chp_stype = CH_TCL;
	protop->chp_stype_cmd = TCL_ADD_STR;

	/*
	 * allocate data block for len, key arguments and string.
	 * which will be stored at end of data
	 */

	if ((mp->b_cont = allocb(sizeof (tcl_data_t)+len, BPRI_MED)) == NULL) {
		cmn_err(CE_WARN,
		    "ansi: dropping addstr msg; cannot alloc msg block");
		freemsg(mp);
		return;
	}

	mp->b_cont->b_wptr += sizeof (tcl_data_t) + len;
	tp = (tcl_data_t *)mp->b_cont->b_rptr;
	tp->add_str.len = len;
	tp->add_str.keynum = keynum;

	/*
	 * Now we copy the string to the end of the data block
	 */

	bcopy(buf, (caddr_t)(++tp), len);
	/*
	 *++tp should point at space allocated past
	 * past the tcl_data_t structure
	 */

	putnext(qp, mp);
}


/*
 * Gather the parameters of an ANSI escape sequence
 */
static void
ansi_getparams(ansistat_t *ap, u_char ch)
{
	if ((ch >= '0' && ch <= '9') && (ap->a_state != A_STATE_ESC_Q_DELM)) {
		/* Number? */
		ap->a_paramval = ((ap->a_paramval * 10) + (ch - '0'));
		ap->a_gotparam++;	/* Remember got parameter */
		return;			/* Return immediately */
	}
	switch (ap->a_state) {		/* Handle letter based on state */

	case A_STATE_ESC_Q:			  /* <ESC>Q<num> ? */
		ap->a_params[1] = ch;		  /* Save string delimiter */
		ap->a_params[2] = 0;		  /* String length 0 to start */
		ap->a_state = A_STATE_ESC_Q_DELM; /* Read string next */
		break;

	case A_STATE_ESC_Q_DELM:		  /* <ESC>Q<num><delm> ? */
		if (ch == ap->a_params[1]) {	/* End of string? */
			ansi_addstring(ap, ap->a_paramval, ap->a_fkey,
			    ap->a_params[2]);
			ap->a_state = A_STATE_START;
			/* End of <ESC> sequence */
		} else if (ch == '^') {
			/* Control char escaped with '^'? */
			ap->a_state = A_STATE_ESC_Q_DELM_CTRL;
			/* Read control character next */

		} else if (ch != '\0') {
			/* Not a null? Add to string */
			ap->a_fkey[ap->a_params[2]++] = ch;
			if (ap->a_params[2] >= ANSI_MAXFKEY)	/* Full? */
				ap->a_state = A_STATE_START;
				/* End of <ESC> sequence */
		}
		break;

	case A_STATE_ESC_Q_DELM_CTRL:	/* Contrl character escaped with '^' */
		/* Read more of string later */
		ap->a_state = A_STATE_ESC_Q_DELM;
		/* Convert to control character */
		ch -= ' ';
		if (ch != '\0') {	/* Not a null? Add to string */
			ap->a_fkey[ap->a_params[2]++] = ch;
			if (ap->a_params[2] >= ANSI_MAXFKEY)	/* Full? */
				ap->a_state = A_STATE_START;
				/* End of <ESC> sequence */
		}
		break;
	case A_STATE_CSI_QMARK:
		if (ap->a_paramval == 7) {
			if (ch == 'h')
				ansi_send_ctl(ap, TCL_AUTO_MARGIN_ON, M_PROTO);
			else if (ch == 'l')
				ansi_send_ctl(ap, TCL_AUTO_MARGIN_OFF, M_PROTO);
		}
		ap->a_state = A_STATE_START;
		break;

	default:			/* All other states */
		if (ap->a_gotparam) {
			/*
			 * Previous number parameter? Save and
			 * point to next free parameter.
			 */
			ap->a_params[ap->a_curparam] = ap->a_paramval;
			ap->a_curparam++;
		}

		if (ch == ';') {
			/* Multiple param separator? */
			ap->a_gotparam = 0;	/* Restart parameter search */
			ap->a_paramval = 0;	/* No parameter value yet */
		} else if (ap->a_state == A_STATE_CSI_EQUAL) {
			ansi_equal_state(ap, ch);
			ap->a_state = A_STATE_START;
		} else 	/* Regular letter */
			ansi_chkparam(ap, ch);	/* Handle escape sequence */
		break;
	}
}

static void
ansi_equal_state(ansistat_t *ap, u_char ch)
{
	switch (ch) {
	case 'A':
		ansi_send_1ctl(ap, TCL_SET_OVERSCAN_COLOR, ap->a_paramval,
		    M_PROTO);
		break;
	case 'B':
		if (ap->a_curparam >= 1)
			ansi_send_1ctl(ap, TCL_SET_BELL_PARAMS,
			    TWO_PARMS(ap), M_PROTO);
		break;
	case 'C':
		if (ap->a_curparam >= 1)
			ansi_send_1ctl(ap, TCL_SET_CURSOR_PARAMS,
			    TWO_PARMS(ap), M_PROTO);
		break;
	case 'D':
		ansi_send_1ctl(ap, TCL_INTENSITY_BKGRD_COLOR, ap->a_paramval,
		    M_PROTO);
		break;
	case 'E':
		{
			short	param;

			if (ap->a_paramval)
				param = 5;
			else
				param = 6;
			ansi_send_1ctl(ap, TCL_SET_ATTR, param, M_PROTO);
		}
		break;
	case 'F':
		ansi_send_1ctl(ap, TCL_FORGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'G':
		ansi_send_1ctl(ap, TCL_BCKGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'H':
		ansi_send_1ctl(ap, TCL_RFORGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'I':
		ansi_send_1ctl(ap, TCL_RBCKGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'J':
		ansi_send_1ctl(ap, TCL_GFORGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'K':
		ansi_send_1ctl(ap, TCL_GBCKGRND_COLOR, ap->a_paramval, M_PROTO);
		break;
	case 'g':
		if (ap->a_curparam >= 1)
			ansi_send_1ctl(ap, TCL_SET_FONT_PROPERTIES,
			    TWO_PARMS(ap), M_PROTO);
		else
			ansi_send_1ctl(ap, TCL_PRINT_FONTCHAR,
			    ap->a_paramval, M_PROTO);
		break;
	}
}


/*
 * add character to data message block being formed to send downstream
 * when received data message is through being parsed
 */

static void
ansi_outch(ansistat_t *ap, u_char ch)
{
	mblk_t *mp;
	queue_t *qp;

	qp = ap->a_wqp;
	mp = ap->a_wmsg;

	if (ap->a_font == ANSI_FONT2)
		ch |= 0x80;
	if (mp == NULL || mp->b_datap->db_lim == mp->b_wptr) {
		if (mp)
			putnext(qp, mp);
		if ((mp = allocb(ANSIPSZ, BPRI_MED)) == NULL) {
			cmn_err(CE_WARN,
			    "ansi: cannot allocate data in ansi_outch");
			return;
		}
	}

	*mp->b_wptr++ = ch;
	ap->a_wmsg = mp;
}


/*
 * state machine parser based on the current state and character input
 * major transitions are to control character or normal character
 */

static void
ansi_parse(ansistat_t *ap, u_char ch)
{
	int i;

	if (ap->a_state == A_STATE_START) {	/* Normal state? */
		if (ch == A_CSI || ch == A_ESC ||
		    (ch < ' ' && ap->a_font == ANSI_FONT0)) /* Control? */
			ansi_control(ap, ch);	/* Handle control */
		else
			ansi_outch(ap, ch);	/* Display */
	} else {			/* In <ESC> sequence */
		if (ap->a_state != A_STATE_ESC) { /* Need to get parameters? */
			if (ap->a_state == A_STATE_CSI)
				switch (ch) {
				case '?':
					ap->a_state = A_STATE_CSI_QMARK;
					return;
				case '=':
					ap->a_state = A_STATE_CSI_EQUAL;
					return;
				case 's':
					ansi_send_ctl(ap, TCL_SAVE_CURSOR,
					    M_PROTO);
					ap->a_state = A_STATE_START;
					return;
				case 'u':
					ansi_send_ctl(ap, TCL_RESTORE_CURSOR,
					    M_PROTO);
					ap->a_state = A_STATE_START;
					return;
			}
			ansi_getparams(ap, ch);
		} else {			/* Previous char was <ESC> */
			if (ch == '[') {
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				/* clear the parameters */
				for (i = 0; i < ANSI_MAXPARAMS; i++)
					ap->a_params[i] = -1;
				ap->a_state = A_STATE_CSI;
			} else if (ch == 'Q') {	/* <ESC>Q ? */
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				for (i = 0; i < ANSI_MAXPARAMS; i++)
					ap->a_params[i] = -1;	/* Clear */
				/* Next get params */
				ap->a_state = A_STATE_ESC_Q;
			} else if (ch == 'C') {	/* <ESC>C ? */
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				for (i = 0; i < ANSI_MAXPARAMS; i++)
					ap->a_params[i] = -1;	/* Clear */
				/* Next get params */
				ap->a_state = A_STATE_ESC_C;
			} else {
				ap->a_state = A_STATE_START;
				if (ch == 'c') {
					/* ESC c resets display */
					ansi_send_ctl(ap, TCL_DISP_RST,
					    M_PROTO);
				} else if (ch == 'H') {
					/* ESC H sets a tab */
					ansi_send_ctl(ap, TCL_SET_TAB, M_PROTO);
				} else if (ch == '7') {
					/* ESC 7 Save Cursor position */
					ansi_send_ctl(ap, TCL_SAVE_CURSOR,
					    M_PROTO);
				} else if (ch == '8') {
					/* ESC 8 Restore Cursor position */
					ansi_send_ctl(ap, TCL_RESTORE_CURSOR,
					    M_PROTO);
				/* check for control chars */
				} else if (ch < ' ')
					ansi_control(ap, ch);
				else
					ansi_outch(ap, ch);
			}
		}
	}
}
