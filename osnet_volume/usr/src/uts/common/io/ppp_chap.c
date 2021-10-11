#pragma ident	"@(#)ppp_chap.c	1.18	98/06/11 SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Implements the CHAP authentication protocol as described in
 * RFC1334.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/t_lock.h>
#include <sys/strsun.h>
#include <sys/md5.h>
#define	DIGEST_SIZE 16
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#ifdef ISERE_TREE
#include <ppp/vjcomp.h>
#include <ppp/ppp_ioctl.h>
#include <ppp/ppp_sys.h>
#include <ppp/ppp_pap.h>
#include <ppp/ppp_chap.h>
#include <ppp/ppp_extern.h>
#else
#include <sys/vjcomp.h>
#include <sys/ppp_ioctl.h>
#include <sys/ppp_sys.h>
#include <sys/ppp_pap.h>
#include <sys/ppp_chap.h>
#include <sys/ppp_extern.h>
#endif

static chapTuple_t chap_statetbl[CHAP_NEVENTS][CHAP_STATES] = {
/* Aboth */ { SRCIRC+C6, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR,
FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR},
/* Arem */ { SRCIRC+C4, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR,
FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR},
/* Aloc */ { C1, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR,
FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR},
/* Chall */ { FSM_ERR, SRRIRR+C2, SRRIRR+C2, SRRIRR+C2, FSM_ERR, FSM_ERR,
SRRIRR+C8, SRRIRR+C9, SRRIRR+C8, SRRIRR+C9,  SRRIRR+C8,	 SRRIRR+C9},
/* Succ */ { FSM_ERR, FSM_ERR, AAS+C3, FSM_ERR, FSM_ERR, FSM_ERR,
FSM_ERR, FSM_ERR, C10, AAS+C11, FSM_ERR, FSM_ERR},
/* Fail */ { FSM_ERR, FSM_ERR, LAF+C0, FSM_ERR, FSM_ERR, FSM_ERR,
FSM_ERR, FSM_ERR, LAF+C0, LAF+C0, FSM_ERR, FSM_ERR },
/* GoodR */ { FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, SRSAAS+C5, SRS+C5,
SRS+C7, SRS+C7, SRS+C9, SRS+C9, SRSAAS+C11, SRS+C11},
/* BadR */ { FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, SRFRAF+C0, SRFRAF+C0,
SRFRAF+C0, SRFRAF+C0, SRFRAF+C0, SRFRAF+C0,  SRFRAF+C0,	 SRFRAF+C0},
/* TOL+ */ { FSM_ERR, C1, SRR+C2, FSM_ERR, FSM_ERR, FSM_ERR, C6,
C7, SRR+C8, SRR+C9, FSM_ERR, FSM_ERR},
/* TOL- */ { FSM_ERR, LAF+C0, LAF+C2, FSM_ERR, FSM_ERR, FSM_ERR,
LAF+C0, LAF+C0, LAF+C0, LAF+C0, LAF+C0, FSM_ERR},
/* TOR+ */ { FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, SRC+C4, FSM_ERR,
SRC+C6, FSM_ERR, SRC+C8, FSM_ERR, SRC+C10, FSM_ERR},
/* TOR- */ { FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, RAF+C0, FSM_ERR,
RAF+C0, FSM_ERR, RAF+C0, FSM_ERR, RAF+C10, FSM_ERR},
/* Force */ { FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, SRCIRC+C4, SRCIRC+C4,
SRCIRC+C6, SRCIRC+C6, SRCIRC+C8, SRCIRC+C8, SRCIRC+C8, SRCIRC+C10},
/* Close */ { C0, C0, C0, C0, C0, C0,
C0, C0, C0, C0, C0, C0 }
};

#define	no_chall_timer_state(new_state) ((new_state) == C0 || \
					(new_state) == C1 || \
					(new_state) == C2 || \
					(new_state) == C3 || \
					(new_state) == C5 || \
					(new_state) == C7 || \
					(new_state) == C9 || \
					(new_state) == C11)

#define	no_resp_timer_state(new_state) ((new_state) == C0 || \
					(new_state) == C3 || \
					(new_state) == C4 || \
					(new_state) == C5 || \
					(new_state) == C10 || \
					(new_state) == C11)

char *chapEvent_txt[] = {"AuthBoth", "AuthRem", "AuthLoc", "Chall", "Succ",
	"Fail", "GoodResp", "BadResp", "TogtResp", "ToeqResp", "TogtChall",
	"ToeqChall", "Force", "Close" };

char *chapAction_txt[] = {"Noaction", "Srcirc", "Srrirr", "Srsras",
	"Srs", "Srfraf", "Aas", "Laf", "Raf", "Srsaaf", "Srr", "Src"};

char *chapState_txt[] = {"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
		"C8", "C9", "C10", "C11" };

static void aas(chapMachine_t *);
static void laf(chapMachine_t *);
static void raf(chapMachine_t *);
static void irr(chapMachine_t *);
static void irc(chapMachine_t *);
static void src(chapMachine_t *);
static void srr(chapMachine_t *);
static void srs(chapMachine_t *);
static void srf(chapMachine_t *);
static void chap_chall_timeout(void *);
static void chap_resp_timeout(void *);
static void cancel_chall_restart_timer(chapMachine_t *);
static void cancel_resp_restart_timer(chapMachine_t *);
static void calc_resp(uint_t chall_id, struct chall_resp *,
	struct chall_resp *, chapMachine_t *, int *);
static void start_resp_restart_timer(chapMachine_t *);
static void start_chall_restart_timer(chapMachine_t *);
static void make_challenge_value(chapMachine_t *);
static int chap_md5verify(uint_t, struct chall_resp *, chapMachine_t *);
static int chap_verify(uint_t, struct chall_resp *, chapMachine_t *, uint_t);
static void chap_md5calc_resp(uint_t, struct chall_resp *,
	struct chall_resp *, chapMachine_t *);
static void *check_param(mblk_t *mp, size_t size);
static void md5get_digest(uchar_t *, uchar_t, uchar_t *, int, uchar_t *, int);

extern int  ppp_debug;

/*
 * chap_action()
 *
 * Dispatches function calls for the fsm's actions
 */
void
chap_action(chapMachine_t *machp, chapAction_t action, chapState_t new_state)
{
	PPP_STRDBG2("Doing action: %s, new state: %s\n",
		chapAction_txt[action], chapState_txt[new_state]);

	switch (action) {
	case Srcirc:
		src(machp);
		irc(machp);
		break;
	case Srrirr:
		srr(machp);
		irr(machp);
		break;
	case Srs:
		srs(machp);
		break;
	case Srsaas:
		srs(machp);
		aas(machp);
		break;
	case Srfraf:
		srf(machp);
		raf(machp);
		break;
	case Aas:
		aas(machp);
		break;
	case Laf:
		laf(machp);
		break;
	case Raf:
		raf(machp);
		break;
	case Srr:
		srr(machp);
		break;
	case Src:
		src(machp);
		break;
	default:
		break;
	}
	machp->state = new_state;
}

/*
 * chap_fsm()
 *
 * Processes an event in the fsm
 */
void
chap_fsm(chapMachine_t *machp, chapEvent_t event)
{
	chapTuple_t tuple;
	chapState_t new_state;
	chapAction_t func;

	ASSERT(event < CHAP_NEVENTS);
	ASSERT(machp->state < CHAP_STATES);
	PPP_STRDBG("Event: %s\n", chapEvent_txt[event]);
	tuple = chap_statetbl[event][machp->state];
	if (tuple == FSM_ERR) {
		return;
	}
	new_state = tuple & 0xff;
	func = (chapAction_t)tuple >> 8;
	chap_action(machp, func, new_state);
	if (no_chall_timer_state(new_state)) {
		cancel_chall_restart_timer(machp);
	}
	if (no_resp_timer_state(new_state)) {
		cancel_resp_restart_timer(machp);
	}
}

/*
 * do_incoming_chap()
 *
 * Handles a chap packet from the remote hosts
 */
void
do_incoming_chap(chapMachine_t *machp, mblk_t *mp)
{
	mblk_t			*zmp, *rp;
	struct ppp_hdr		*hp, *resp_hdr;
	ushort_t		chlength;
	chapEvent_t		event;
	struct chall_resp	*resp, *chall;
	uchar_t			replength, repid, chid;
	int			len;

	if (!ISPULLEDUP(mp)) {
		zmp = msgpullup(mp, -1);
		freemsg(mp);
		mp = zmp;
		if (mp == NULL) {
			return;
		}
	}

	hp = (struct ppp_hdr *)mp->b_rptr;

	switch (hp->pkt.code) {

	case Success:
		if (hp->pkt.id != machp->respid) {
			goto discard;
		}
		event = Succ;
		break;
	case Challenge:
		if (machp->response)
			freemsg(machp->response);
		rp = machp->response =
		    ppp_alloc_frame(pppCHAP, Response, hp->pkt.id);
		if (rp == NULL) {
			freemsg(mp);
			return;
		}
		machp->respid = chid = hp->pkt.id;
		resp_hdr = (struct ppp_hdr *)rp->b_rptr;
		resp = (struct chall_resp *)rp->b_wptr;

		chlength = ntohs(hp->pkt.length);
		(void) adjmsg(mp, PPP_HDRSZ);
		chlength -= (ushort_t)PPP_HDRSZ;

		chall = (struct chall_resp *)mp->b_rptr;

		calc_resp(chid, chall, resp, machp, &len);

		resp_hdr->pkt.length =
		    htons(ntohs(resp_hdr->pkt.length) + len);

		rp->b_wptr += len;
		event = Chall;
		break;

	case Response:
		replength = ntohs(hp->pkt.length);
		repid = hp->pkt.id;
		if (repid != machp->crid) {
			goto discard;
		}
		(void) adjmsg(mp, PPP_HDRSZ);
		replength -= (uchar_t)PPP_PKT_HDRSZ;

		resp = (struct chall_resp *)mp->b_rptr;
		if (chap_verify(repid, resp, machp, replength)) {
			rp = machp->result =
				ppp_alloc_frame(pppCHAP, Success, repid);
			if (rp == NULL) {
				freemsg(mp);
				return;
			}
			event = GoodResp;
		} else {
			rp = machp->result =
				ppp_alloc_frame(pppCHAP, Failure, repid);
			if (rp == NULL) {
				freemsg(mp);
				return;
			}
			event = BadResp;
		}
		break;

	case Failure:
		if (hp->pkt.id != machp->respid) {
			goto discard;
		}
		event = Fail;
		break;
	}
	chap_fsm(machp, event);
discard:
	freemsg(mp);
}

/*
 * check_param()
 *
 * Checks the parameter for an ioctl to the chap layer
 */
static void *
check_param(mblk_t *mp, size_t size)
{
	struct iocblk	*iocp;

	ASSERT(size != 0);

/*
 * check parameter size
 */
	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != size) {
		return (NULL);
	}

	return ((void *)mp->b_cont->b_rptr);
}

/*
 * chap_external_event()
 *
 * Handles events directed to chap from other layers or lm
 */
void
chap_external_event(pppLink_t *lp, uint_t exevent)
{
	chapMachine_t	*machp;
	chapEvent_t	event;
	switch (exevent) {

	case PPPIN_AUTH_LOC:
		event = AuthLoc;
		break;
	case PPPIN_AUTH_REM:
		event = AuthRem;
		break;
	case PPPIN_AUTH_BOTH:
		event = AuthBoth;
		break;
	case PPPIN_FORCE_REM:
		event = Force;
		break;
	case PPPIN_TIMEOUT1:
		machp = lp->chap;
		if (machp->chall_timedoutid != machp->chall_restart)
			return;
		machp->chall_restart = 0;
		if (machp->chall_restart_counter > 0) {
			start_chall_restart_timer(machp);
			event = TogtChall;
		} else {
			event = ToeqChall;
		}
		break;
	case PPPIN_TIMEOUT2:
		machp = lp->chap;
		if (machp->resp_timedoutid != machp->resp_restart)
			return;
		machp->resp_restart = 0;
		if (machp->resp_restart_counter > 0) {
			start_resp_restart_timer(machp);
			event = TogtChall;
		} else {
			event = ToeqChall;
		}
		break;
	case PPPIN_CLOSE:
		event = ChapClose;
		break;
	default:
		return;
	}
	mutex_enter(&lp->lplock);
	chap_fsm(lp->chap, event);
	mutex_exit(&lp->lplock);
}


/*
 * chap_ioctl()
 *
 * Processes ioctls to the chap layer
 */
int
chap_ioctl(chapMachine_t *machp, int command, mblk_t *mp)
{
	chapPasswdEntry_t *passwd;

	switch (command) {
	case PPP_SET_LOCAL_PASSWD:
		PPP_FSMDBG0("Setting CHAP local passwd\n");
		passwd = (chapPasswdEntry_t *)
		    check_param(mp, sizeof (*passwd));
		if (passwd->chapPasswdLen < CHAP_MAX_PASSWD &&
		    passwd->chapNameLen < CHAP_MAX_NAME) {
			bcopy(passwd->chapPasswd, machp->local_secret,
			    passwd->chapPasswdLen);
			machp->local_secret_size = passwd->chapPasswdLen;
			bcopy(passwd->chapName, machp->local_name,
			    passwd->chapNameLen);
			machp->local_name_size = passwd->chapNameLen;
		} else {
			return (-1);
		}
		break;
	case PPP_SET_REMOTE_PASSWD:
		PPP_FSMDBG0("Setting CHAP remote passwd\n");
		passwd = (chapPasswdEntry_t *)
		    check_param(mp, sizeof (*passwd));
		if (passwd->chapPasswdLen < CHAP_MAX_PASSWD &&
		    passwd->chapNameLen < CHAP_MAX_NAME) {
			bcopy(passwd->chapPasswd, machp->remote_secret,
			    passwd->chapPasswdLen);
			machp->remote_secret_size = passwd->chapPasswdLen;
			bcopy(passwd->chapName, machp->remote_name,
			    passwd->chapNameLen);
			machp->remote_name_size = passwd->chapNameLen;
		} else {
			return (-1);
		}
		break;

	default:
		return (-1);
	}

	return (0);
}

/*
 * src()
 *
 * Send remote a challenge packet
 */
static void
src(chapMachine_t *machp)
{
	mblk_t		*fp;
	struct ppp_hdr	*hdr;
	struct chall_resp *chall;


	if (machp->chall) {
		hdr = (struct ppp_hdr *)machp->chall->b_rptr;
		hdr->pkt.id = ++machp->crid;
		fp = machp->chall;
	} else {
		fp = ppp_alloc_frame(pppCHAP, Challenge, ++machp->crid);
		if (fp == NULL) {
			return;
		}

		hdr = (struct ppp_hdr *)fp->b_rptr;

		chall = (struct chall_resp *)fp->b_wptr;

		machp->chall_size = 10;
		chall->value_size = (uchar_t)machp->chall_size;

		make_challenge_value(machp);

		bcopy(machp->chall_value, chall->value,
		    machp->chall_size);

		fp->b_wptr += machp->chall_size + 1;

		bcopy(machp->local_name, fp->b_wptr,
		    machp->local_name_size);

		fp->b_wptr += machp->local_name_size;

		hdr->pkt.length =
		    htons(ntohs(hdr->pkt.length) + 1 + machp->chall_size +
		    machp->local_name_size);
		machp->chall = fp;

	}
	machp->chall = copymsg(fp);

	ppp_putnext(WR(machp->readq), fp);
	machp->chall_restart_counter--;
}

/*
 * srr()
 *
 * Send remote a response packet in response to a challenge
 */
static void
srr(chapMachine_t *machp)
{
	mblk_t		*fp;

	ASSERT(machp->response);
	fp = copymsg(machp->response);

	ppp_putnext(WR(machp->readq), fp);
	machp->resp_restart_counter--;
}

/*
 * srs()
 *
 * Send remote a success packet.
 */
static void
srs(chapMachine_t *machp)
{
	ASSERT(machp->result);
	ppp_putnext(WR(machp->readq), machp->result);
	machp->result = NULL;
}

/*
 *
 * srf()
 *
 * Send remote a failure packet.
 */
static void
srf(chapMachine_t *machp)
{
	ASSERT(machp->result);
	ppp_putnext(WR(machp->readq), machp->result);
	machp->result = NULL;
}

/*
 * aas()
 *
 * Report that authentication has succeeded.
 */
static void
aas(chapMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_internal_event(lp, PPP_AUTH_SUCCESS, pppCHAP);
}


/*
 * laf()
 *
 * Report that local authentication has failed.
 */
static void
laf(chapMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_error_ind(lp, pppLocalAuthFailed, NULL, (uint_t)0);
	ppp_internal_event(lp, PPP_LOCAL_FAILURE, pppCHAP);
}


/*
 * raf()
 *
 * Report that remote authenitication has failed.
 */
static void
raf(chapMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_error_ind(lp, pppRemoteAuthFailed, NULL, (uint_t)0);
	ppp_internal_event(lp, PPP_REMOTE_FAILURE, pppCHAP);
}

/*
 * irc()
 *
 * Restart the challenge resend timer.
 */
static void
irc(chapMachine_t *machp)
{
	cancel_chall_restart_timer(machp);
	machp->chall_restart_counter = machp->chapMaxRestarts;
	machp->chall_restart = timeout(chap_chall_timeout, machp,
	    MSEC_TO_TICK(machp->chapRestartTimerValue));
}

/*
 * calc_resp()
 *
 * Calculate the response to a particular challenge value.
 */
static void
calc_resp(
	uint_t chall_id,
	struct chall_resp *chall,
	struct chall_resp *resp,
	chapMachine_t *machp,
	int *len)
{
	uchar_t *name_ptr;

	resp->value_size = MAX_CHALL_SIZE;
	chap_md5calc_resp(chall_id, chall, resp, machp);
	name_ptr = resp->value + resp->value_size;
	bcopy(machp->local_name, name_ptr, machp->local_name_size);
	*len = resp->value_size + machp->local_name_size + 1;
}

/*
 * irr()
 *
 * Start the response resend timer.
 */
static void
irr(chapMachine_t *machp)
{
	cancel_resp_restart_timer(machp);
	machp->resp_restart_counter = machp->chapMaxRestarts;
	machp->resp_restart = timeout(chap_resp_timeout, machp,
	    MSEC_TO_TICK(machp->chapRestartTimerValue));
}


/*
 * cancel_chall_restart_timer()
 *
 * Cancel outstanding challenge restart timer
 * (no problem if timer is not running)
 */
static void
cancel_chall_restart_timer(chapMachine_t *machp)
{
	if (machp->chall_restart == 0) {
		return;
	}

	(void) untimeout(machp->chall_restart);
	machp->chall_restart = 0;
}


/*
 * cancel_resp_restart_timer()
 *
 * Cancel outstanding response restart timer
 * (no problem if timer is not running)
 */
static void
cancel_resp_restart_timer(chapMachine_t *machp)
{
	if (machp->resp_restart == 0) {
		return;
	}

	(void) untimeout(machp->resp_restart);
	machp->resp_restart = 0;
}


/*
 * start_chall_restart_timer()
 *
 * Start the challenge resend timer.
 */
static void
start_chall_restart_timer(chapMachine_t *machp)
{
	machp->chall_restart = timeout(chap_chall_timeout, machp,
	    MSEC_TO_TICK(machp->chapRestartTimerValue));
}


/*
 * start_resp_restart_timer()
 *
 * Start the response resend timer.
 */
static void
start_resp_restart_timer(chapMachine_t *machp)
{
	machp->resp_restart = timeout(chap_resp_timeout, machp,
	    MSEC_TO_TICK(machp->chapRestartTimerValue));
}

/*
 * chap_chall_timeout()
 *
 * Generate a timeout event in response to the challenge resend timer
 * expired
 */
static void
chap_chall_timeout(void *mp)
{
	chapMachine_t *machp = (chapMachine_t *)mp;

	ASSERT(machp);

	machp->chall_timedoutid = machp->chall_restart;

	ppp_cross_fsm(machp->linkp, PPPIN_TIMEOUT1, pppCHAP, CROSS_LO);
}

/*
 * chap_resp_timeout()
 *
 * Generate a timeout event in response to the response resend timer
 * expired
 */
static void
chap_resp_timeout(void *mp)
{
	chapMachine_t *machp = (chapMachine_t *)mp;

	ASSERT(machp);

	machp->resp_timedoutid = machp->resp_restart;

	ppp_cross_fsm(machp->linkp, PPPIN_TIMEOUT2, pppCHAP, CROSS_LO);
}

/*
 * alloc_chap_machine()
 *
 * Allocate a chap machine structure
 *
 * Returns alloced structure on success, NULL on failure
 */
chapMachine_t *
alloc_chap_machine(queue_t *readq, pppLink_t *linkp)
{
	chapMachine_t *machp;

	machp = kmem_zalloc(sizeof (chapMachine_t), KM_NOSLEEP);

	if (machp == NULL)
		return (NULL);

	machp->readq = readq;

	machp->state = C0;

	machp->chapMaxRestarts = CHAP_DEF_MAXRESTART;
	machp->chapRestartTimerValue = CHAP_DEF_RESTIMER;
	machp->chall_size = 0;

	machp->linkp = linkp;

	return (machp);
}

/*
 * free_chap_machine()
 *
 * Free the allocated memory of a chap machine structure.
 */
void
free_chap_machine(chapMachine_t *machp)
{
	cancel_chall_restart_timer(machp);
	cancel_resp_restart_timer(machp);
	if (machp->response)
		freemsg(machp->response);
	if (machp->result)
		freemsg(machp->result);
	if (machp->chall)
		freemsg(machp->chall);
	(void) kmem_free(machp, sizeof (chapMachine_t));
}

/*
 * make_challenge_value()
 *
 * Create a challenge value to send to the remote host.
 */
static void
make_challenge_value(chapMachine_t *machp)
{
	uint_t tempval;
	uchar_t *tempptr;
	int i;

	for (i = 0; i < machp->chall_size; i++) {
		if (i % sizeof (tempval) == 0) {
			tempval = ppp_rand();
			tempptr = (uchar_t *)&tempval;
		}
		machp->chall_value[i] = *tempptr++;
	}
}

/*
 * chap_verify()
 *
 * Verify the authentication of a response
 *
 * Returns 0 for bad response, 1 for good response
 */
static int
chap_verify(
	uint_t repid,
	struct chall_resp *resp,
	chapMachine_t *machp,
	uint_t data_len)
{
	int	name_len;
	uchar_t	*name_ptr;

	if (resp->value_size > data_len)
		return (0);

	name_len = data_len - resp->value_size -1;
	name_ptr = resp->value + resp->value_size;

	if (name_len != machp->remote_name_size)
			return (0);
	if (bcmp(name_ptr, machp->remote_name,
	    machp->remote_name_size) != 0)
		return (0);

	return (chap_md5verify(repid, resp, machp));
}

/*
 * chap_md5verify()
 *
 * Verify the authentication of a response using the md5 algorithm
 *
 * Returns 0 for bad response, 1 for good response
 */
static int
chap_md5verify(uint_t repid, struct chall_resp *resp, chapMachine_t *machp)
{
	uchar_t digest[DIGEST_SIZE];

	if (resp->value_size != DIGEST_SIZE) {
		return (0);
	}

	md5get_digest(digest, repid, machp->remote_secret,
		(int)machp->remote_secret_size, machp->chall_value,
		(int)machp->chall_size);

	if (bcmp(digest, resp->value, DIGEST_SIZE) != 0) {
		return (0);
	}
	return (1);
}

/*
 * chap_md5calc_resp()
 *
 * Calculate the response to a challenge using md5
 */
static void
chap_md5calc_resp(
	uint_t chall_id,
	struct chall_resp *chall,
	struct chall_resp *resp,
	chapMachine_t *machp)
{

	md5get_digest(resp->value, chall_id, machp->local_secret,
		(int)machp->local_secret_size, chall->value,
		(int)chall->value_size);
	resp->value_size = (int)DIGEST_SIZE;
}

/*
 * md5get_digest()
 *
 * Compute md5 digest.
 */
static void
md5get_digest(uchar_t *digest, uchar_t serid, uchar_t *secret, int secret_len,
    uchar_t *chall, int chall_len)
{
	MD5_CTX context;
	uchar_t cval[1 + MAX_CHALL_SIZE + CHAP_MAX_PASSWD];
	uchar_t *tptr;
	int cval_len;

	tptr = cval;

	*(tptr++) = serid;

	bcopy(secret, tptr, secret_len);
	tptr += secret_len;

	bcopy(chall, tptr, chall_len);
	tptr += chall_len;
	tptr += chall_len;

	cval_len = 1 + secret_len + chall_len;

	MD5Init(&context);
	MD5Update(&context, cval, cval_len);
	MD5Final(digest, &context);
}
