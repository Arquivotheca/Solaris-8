/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppp_pap.c	1.23	98/06/11 SMI"

/*
 * A state table driven implementation of the Password Authentication Protocol
 * (PAP) for the Point-to-Point Protocol (PPP)
 *
 * Derived from RFC1334
 */


#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/cmn_err.h>
#ifndef _SunOS4
#include <sys/strlog.h>
#include <sys/kmem.h>
#else
#include <ppp/4.1.h>
#endif
#include <sys/types.h>
#include <sys/dlpi.h>
#include <sys/strsun.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#ifdef ISERE_TREE
#include <ppp/ppp_ioctl.h>
#include <ppp/vjcomp.h>
#include <ppp/ppp_sys.h>
#include <ppp/ppp_pap.h>
#include <ppp/ppp_extern.h>
#else
#include <sys/ppp_ioctl.h>
#include <sys/vjcomp.h>
#include <sys/ppp_sys.h>
#include <sys/ppp_pap.h>
#include <sys/ppp_extern.h>
#endif


/*
 * Entries in the PAP state machine are two-byte values (represented as
 * a short).  Each entry is a tuple:
 *
 * < action > < new state >
 *
 * When an event "x" takes place in state "y" the tuple at [x][y]
 * is located, and < action > is performed,  the state changing to
 * < new state >
 *
 * In this way, all the policy is held in the state table rather than
 * in discrete code.
 *
 */

/*
 *
 * Notes:
 *	  AuthLoc   - Authenticate Local
 *	  AuthRem   - Authenticate Remote
 *	  AuthBoth  - Authenticate Both Local and Remote
 *	  Auth	    - Authenticate packet received
 *	  AuthAck   - Authenticate Ack packet received
 *	  AuthNak   - Authenticate Nak packet received
 *	  AuthOk    - ID authenticated OK by user
 *	  AuthNok   - ID failed authentication by user
 */

static pppTuple_t pap_statetbl[PAP_NEVENTS][PAP_NSTATES] = {
/* AuthBoth */ { SARIRRIRW+P5, PAP_FS, PAP_FS, PAP_FS, PAP_FS,
	PAP_FS, PAP_FS, PAP_FS, PAP_FS },
/* AuthRem  */ {IRW+P3, PAP_FS, PAP_FS, PAP_FS, PAP_FS,
	PAP_FS, PAP_FS, PAP_FS, PAP_FS },
/* AuthLoc  */ {SARIRR+P1, PAP_FS, PAP_FS, PAP_FS, PAP_FS,
	PAP_FS, PAP_FS, PAP_FS, PAP_FS },
/* AuthGood */ {PAP_FS, PAP_FS, PAP_FS, SAAAAS+P4, SAA+P4,
	SAA+P6, SAA+P6, SAAAAS+P8, SAA+P8 },
/* AuthBad  */ {PAP_FS, PAP_FS, PAP_FS, SANRAF+P0, SANRAF+P0,
	SANRAF+P0, SANRAF+P0, SANRAF+P0, SANRAF+P0 },
/* AuthAck  */ {PAP_FS, PAAS+P2, PAP_FS, PAP_FS, PAP_FS,
	P7, PAAS+P8, PAP_FS, PAP_FS },
/* AuthNack */ {PAP_FS, PLAF+P0, PAP_FS, PAP_FS, PAP_FS,
	PLAF+P0, PLAF+P0, PAP_FS, PAP_FS },
/* TOwait   */ {PAP_FS, PAP_FS, PAP_FS, PRAF+P0, PAP_FS,
	PRAF+P0, PAP_FS, PRAF+P0, PAP_FS },
/* TO+req   */ {PAP_FS, SAR+P1, PAP_FS, PAP_FS, PAP_FS,
	SAR+P5, SAR+P6, PAP_FS, PAP_FS },
/* TO-req   */ {PAP_FS, PLAF+P0, PAP_FS, PAP_FS, PAP_FS,
	PLAF+P0, PLAF+P0, PAP_FS, PAP_FS },
/* Close    */ {P0, P0, P0, P0, P0, P0, P0, P0, P0 },
};

#ifdef PPP_DEBUG

static char *papState_txt[] = {
	"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8" };

static char *papAction_txt[] = {"papSarirr", "papSarirrirw", "papIrw",
	"papAas", "papLaf", "papSar", "papSaaaas", "papSanraf",
	"papRaf", "papSaa"};

static char *papEvent_txt[] = {
	"AuthBoth", "AuthRem", "AuthLoc", "AuthGood", "AuthBad", "AuthAck",
	"AuthNak", "TOwait", "TO+req", "TO-req", "Close" };

#endif

mblk_t *ppp_alloc_frame();
static void aas();
static void laf();
static void raf();
static void irr();
static void irw();
static void sar();
static void san();
static void saa();
static void pap_req_timeout();
static void req_wait_timeout();
static void cancel_req_restart_timer();
static void cancel_wait_timer();
static int pap_verify();
static void start_req_restart_timer();
static void *check_param(mblk_t *, size_t);
static void pap_action(papMachine_t *, papAction_t, papState_t);
static void make_response(uint_t, uint_t, papMachine_t *);
static papPasswdEntry_t *get_isdn_passwd(mblk_t *);
static int set_isdn_passwd(mblk_t *, papPasswdEntry_t *);
static int pap_extract_passwd(mblk_t *, papPasswdEntry_t *, uint_t);
static void pap_send_rqv(papMachine_t *);


#define	no_wait_timer(new_state)	((new_state) == P0 || \
					(new_state) == P1 || \
					(new_state) == P2 || \
					(new_state) == P4 || \
					(new_state) == P6 || \
					(new_state) == P8)

#define	no_req_timer(new_state)		((new_state) == P0 || \
					(new_state) == P2 || \
					(new_state) == P3 || \
					(new_state) == P4 || \
					(new_state) == P7 || \
					(new_state) == P8)

/*
 * ppp_pap_fsm()
 *
 * Password Authentication Protocol finite state machine.
 * This is the main engine which controls authentication
 * using PAP.
 *
 * Incoming messages must be correctly pulled up.
 *
 * Returns: 0 on success, error code otherwise.
 */
void
pap_fsm(papMachine_t *machp, papEvent_t event)
{
	pppTuple_t		tuple;
	papAction_t		func;
	papState_t		new_state;

	ASSERT(event < PAP_NEVENTS);

	PPP_FSMDBG2("Event: %s, State: %s\n", papEvent_txt[event],
	    papState_txt[machp->state]);
	tuple = pap_statetbl[event][machp->state];

	if (tuple == PAP_FS) {
		return;
	}

	new_state = tuple & 0xff;
	func = (papAction_t)tuple >> 8;

	ASSERT(new_state < PAP_NSTATES);
	ASSERT(func < PAP_NACTIONS);

	pap_action(machp, func, new_state);
	if (no_wait_timer(new_state)) {
		cancel_wait_timer(machp);
	}
	if (no_req_timer(new_state)) {
		cancel_req_restart_timer(machp);
	}
}


/*
 * pap_action()
 *
 * Dispatch the function calls for an fsm action.
 */
void
pap_action(papMachine_t *machp, papAction_t action, papState_t new_state)
{
	PPP_FSMDBG2("Doing action: %s, new state: %s\n",
		papAction_txt[action-1], papState_txt[new_state]);
	switch (action) {
	case papSarirr:
		sar(machp);
		irr(machp);
		break;
	case papSarirrirw:
		sar(machp);
		irr(machp);
		irw(machp);
		break;
	case papIrw:
		irw(machp);
		break;
	case papAas:
		aas(machp);
		break;
	case papLaf:
		laf(machp);
		break;
	case papSar:
		sar(machp);
		break;
	case papSaaaas:
		saa(machp);
		aas(machp);
		break;
	case papSanraf:
		san(machp);
		raf(machp);
		break;
	case papRaf:
		raf(machp);
		break;
	case papSaa:
		saa(machp);
		break;
	}
	machp->state = new_state;
}


/*
 * sar()
 *
 * Send an Authentication Request
 *
 * Start PAP authentication with an Authenticate Packet
 *
 * Returns: 0 on success with authenticate packet putnext(q),
 * error code otherwise
 */
void
pap_external_event(pppLink_t *lp, uint_t	exevent)
{
	papMachine_t	*machp;
	papEvent_t	event;


	machp = lp->pap;

	switch (exevent) {
	case PPPIN_AUTH_LOC:
		event = papAuthLoc;
		break;
	case PPPIN_AUTH_REM:
		event = papAuthRem;
		break;
	case PPPIN_AUTH_BOTH:
		event = papAuthBoth;
		break;
	case PPPIN_TIMEOUT1:
		if (machp->req_timedoutid != machp->req_restart)
			return;
		machp->req_restart = 0;
		if (machp->req_restart_counter > 0) {
			start_req_restart_timer(machp);
			event = papTOgtreq;
		} else {
			event = papTOeqreq;
		}
		break;
	case PPPIN_TIMEOUT2:
		if (machp->req_wait_timedoutid !=
			machp->req_wait_timer)
			return;
		machp->req_wait_timer = 0;
		event = papTOwait;
		break;
	case PPPIN_CLOSE:
		event = papClose;
		break;
	case PPPIN_AUTHOK:
		make_response(AuthenticateAck, machp->repid, machp);
		event = papAuthGood;
		break;
	case PPPIN_AUTHNOK:
		make_response(AuthenticateNak, machp->repid, machp);
		event = papAuthBad;
		break;
	default:
		return;
	}
	mutex_enter(&lp->lplock);
	pap_fsm(lp->pap, event);
	mutex_exit(&lp->lplock);
}


/*
 * pap_ioctl()
 *
 * Process ioctl calls to the pap layer.
 *
 * Return 0 on success, nonzero on error.
 */
int
pap_ioctl(papMachine_t *machp, int command, mblk_t *mp)
{
	papPasswdEntry_t hold_pap_passwd;
	int back_isdn = 0;
	papPasswdEntry_t *passwd;

	switch (command) {
	case PPP_SET_LOCAL_PASSWD:
		PPP_FSMDBG0("Setting PAP local passwd\n");
		passwd = (papPasswdEntry_t *)
			check_param(mp, sizeof (*passwd));
		if (passwd == NULL) {
			passwd = (papPasswdEntry_t *)get_isdn_passwd(mp);
			if (passwd == NULL)
				return (-1);
		}
		if (passwd->papPasswdLen < PAP_MAX_PASSWD &&
		    passwd->papPeerIdLen < PAP_MAX_ID) {
			bcopy((caddr_t)passwd->papPasswd,
			    (caddr_t)machp->local_passwd.papPasswd,
			    passwd->papPasswdLen);
			bcopy((caddr_t)passwd->papPeerId,
			    (caddr_t)machp->local_passwd.papPeerId,
			    passwd->papPeerIdLen);

			machp->local_passwd.papPasswdLen =
			    passwd->papPasswdLen;
			machp->local_passwd.papPeerIdLen =
			    passwd->papPeerIdLen;
			machp->local_passwd.protocol = pppAuthPAP;
			return (0);
		} else {
			return (-1);
		}
	case PPP_SET_REMOTE_PASSWD:
		PPP_FSMDBG0("Setting PAP remote passwd\n");
		passwd = (papPasswdEntry_t *)
		    check_param(mp, sizeof (*passwd));
		if (passwd == NULL) {
			passwd = (papPasswdEntry_t *)get_isdn_passwd(mp);
			if (passwd == NULL)
				return (-1);
		}
		if (passwd->papPasswdLen < PAP_MAX_PASSWD &&
		    passwd->papPeerIdLen < PAP_MAX_ID) {
			bcopy((caddr_t)passwd->papPasswd,
			    (caddr_t)machp->remote_passwd.papPasswd,
			    passwd->papPasswdLen);
			bcopy((caddr_t)passwd->papPeerId,
			    (caddr_t)machp->remote_passwd.papPeerId,
			    passwd->papPeerIdLen);

			machp->remote_passwd.papPasswdLen =
			    passwd->papPasswdLen;
			machp->remote_passwd.papPeerIdLen =
			    passwd->papPeerIdLen;
			machp->remote_passwd.protocol = pppAuthPAP;
			return (0);
		} else {
			return (-1);
		}
	case PPP_REMOTE_OK:
/*
 *		pap_verify = (papVerify_t *)
		    check_param(mp, sizeof (*pap_verify));
*/
		ppp_cross_fsm(machp->linkp, PPPIN_AUTHOK,
		    pppAuthPAP, CROSS_LO);
		return (0);
	case PPP_REMOTE_NOK:
/*
 *		pap_verify = (papVerify_t *)
		    check_param(mp, sizeof (*pap_verify));
*/
		ppp_cross_fsm(machp->linkp, PPPIN_AUTHNOK,
		    pppAuthPAP, CROSS_LO);
		return (0);
	case PPP_GET_REMOTE_PASSWD:
		PPP_FSMDBG0("Getting PAP remote passwd\n");
		passwd = (papPasswdEntry_t *)
		    check_param(mp, sizeof (*passwd));
		if (passwd == NULL) {
			passwd = &hold_pap_passwd;
			back_isdn = 1;
		}
		bcopy((caddr_t)&machp->remote_passwd_recv, (caddr_t)passwd,
		    sizeof (*passwd));

		if (back_isdn) {
			if (set_isdn_passwd(mp, passwd) != 0) {
				return (-1);
			}
		}
		return (0);
	default:
		return (0);
	}
}


/*
 * sar()
 *
 * Send remote an authentication request.
 */
static void
sar(papMachine_t *machp)
{
	mblk_t			*fp;
	struct ppp_hdr		*hdr;
	char *req;

	if (machp->request) {
		hdr = (struct ppp_hdr *)machp->request->b_rptr;
		hdr->pkt.id = ++machp->crid;
		fp = machp->request;
	} else {
		fp = ppp_alloc_frame(pppAuthPAP, Authenticate, ++machp->crid);
		if (fp == NULL) {
			return;
		}

		hdr = (struct ppp_hdr *)fp->b_rptr;

		req = (char *)fp->b_wptr;

		*req++ = machp->local_passwd.papPeerIdLen;
		bcopy((caddr_t)machp->local_passwd.papPeerId, (caddr_t)req,
		    machp->local_passwd.papPeerIdLen);
		req += machp->local_passwd.papPeerIdLen;

		*req++ = machp->local_passwd.papPasswdLen;
		bcopy((caddr_t)machp->local_passwd.papPasswd, (caddr_t)req,
		    machp->local_passwd.papPasswdLen);
		req += machp->local_passwd.papPasswdLen;

		hdr->pkt.length = htons(ntohs(hdr->pkt.length) +
		    machp->local_passwd.papPeerIdLen +
		    machp->local_passwd.papPasswdLen + 2);
		fp->b_wptr += machp->local_passwd.papPeerIdLen +
		    machp->local_passwd.papPasswdLen + 2;
	}
	machp->request = copymsg(fp);

	ppp_putnext(WR(machp->readq), fp);
	machp->req_restart_counter--;
}


/*
 * pap_send_rqv()
 *
 * Alert the lm that we need a password authenticated.
 */
static void
pap_send_rqv(papMachine_t *machp)
{
	mblk_t			*mp;
	pppReqValidation_t	*msg;

	mp = allocb(sizeof (*msg), BPRI_HI);
	if (mp == NULL) {
		return;
	}

	MTYPE(mp) = M_PROTO;

	mp->b_wptr += sizeof (*msg);
	msg = (pppReqValidation_t *)mp->b_rptr;

	msg->ppp_message	= PPP_NEED_VALIDATION;
	putnext(machp->readq, mp);
}

/*
 * saa()
 *
 * Send an Authentication Ack packet
 */
static void
saa(papMachine_t *machp)
{
	mblk_t		*fp;

	ASSERT(machp->result);
	fp = copymsg(machp->result);
	ppp_putnext(WR(machp->readq), fp);
}


/*
 * san()
 *
 * Send an Authentication Nak packet
 */
static void
san(papMachine_t *machp)
{
	mblk_t		*fp;

	ASSERT(machp->result);
	fp = copymsg(machp->result);
	ppp_putnext(WR(machp->readq), fp);
}


/*
 * irr()
 *
 * Start the request resend timer.
 */
static void
irr(papMachine_t *machp)
{
	cancel_req_restart_timer(machp);
	machp->req_restart_counter = machp->papMaxRestarts;
	machp->req_restart = timeout(pap_req_timeout, machp,
	    MSEC_TO_TICK(machp->papRestartTimerValue));
}


/*
 * cancel_req_restart_timer()
 *
 * Cancel outstanding restart timer
 * (no problem if timer is not running)
 */
static void
cancel_req_restart_timer(papMachine_t *machp)
{
	if (machp->req_restart == 0) {
		return;
	}

	(void) untimeout(machp->req_restart);
	machp->req_restart = 0;
}


/*
 * start_req_restart_timer()
 *
 * Start the request restart timer
 */
static void
start_req_restart_timer(papMachine_t *machp)
{
	machp->req_restart = timeout(pap_req_timeout, machp,
		MSEC_TO_TICK(machp->papRestartTimerValue));
}


/*
 * pap_req_timeout()
 *
 * Generate a timeout event in response to timer expiring.
 */
static void
pap_req_timeout(papMachine_t *machp)
{
	ASSERT(machp);
	machp->req_timedoutid = machp->req_restart;
	ppp_cross_fsm(machp->linkp, PPPIN_TIMEOUT1, pppAuthPAP, CROSS_LO);
}


/*
 * irw()
 *
 * Start the wait for request timer
 */
static void
irw(papMachine_t *machp)
{
	cancel_wait_timer(machp);
	machp->req_wait_timer = timeout(req_wait_timeout, machp,
		MSEC_TO_TICK(machp->papWaitTimerValue));
}

/*
 * cancel_wait_timer()
 *
 * Cancel outstanding wait for request timer
 * (no problem if timer is not running)
 */
static void
cancel_wait_timer(papMachine_t *machp)
{
	if (machp->req_wait_timer == 0) {
		return;
	}

	(void) untimeout(machp->req_wait_timer);
	machp->req_wait_timer = 0;
}

/*
 * req_wait_timeout()
 *
 * Generate a timeout event in response to wait for request timer expiration
 */
static void
req_wait_timeout(papMachine_t *machp)
{
	ASSERT(machp);
	machp->req_wait_timedoutid = machp->req_wait_timer;
	ppp_cross_fsm(machp->linkp, PPPIN_TIMEOUT2, pppAuthPAP, CROSS_LO);
}


/*
 * make_response()
 *
 * Make and ACK response for sending to peer
 */
void
make_response(uint_t value, uint_t repid, papMachine_t *machp)
{
	mblk_t			*rp;
	struct ppp_hdr		*hp;
	char			*req;

	rp = machp->result =
		ppp_alloc_frame(pppAuthPAP, value, repid);
	if (rp == NULL) {
		return;
	}
	hp = (struct ppp_hdr *)rp->b_rptr;
	req = (char *)rp->b_wptr;
	*req++ = '\0';

	rp->b_wptr++;
	hp->pkt.length = htons(ntohs(hp->pkt.length) + 1);
}


/*
 * do_incoming_pap()
 *
 * Process an incoming pap packet.
 */
void
do_incoming_pap(papMachine_t *machp, mblk_t *mp)
{
	mblk_t			*zmp;
	struct ppp_hdr		*hp;
	papEvent_t		event;
	ushort_t			replength, repid;
	papPasswdEntry_t	passwd;


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

	case Authenticate:
		repid = hp->pkt.id;
		replength = ntohs(hp->pkt.length);
		(void) adjmsg(mp, PPP_HDRSZ);
		replength -= (ushort_t)PPP_PKT_HDRSZ;

		if (machp->result) {
			freemsg(machp->result);
		}
		if (pap_extract_passwd(mp, &passwd, replength) < 0)
			goto discard;

		if (machp->remote_passwd.protocol != pppAuthPAP) {
			bcopy((caddr_t)&passwd,
			    (caddr_t)&machp->remote_passwd_recv,
			    sizeof (passwd));
			machp->repid = repid;
			pap_send_rqv(machp);
			goto no_action;
		}
		if (pap_verify(&passwd, machp)) {
			make_response((uchar_t)AuthenticateAck, repid, machp);
			event = papAuthGood;
		} else {
			make_response((uchar_t)AuthenticateNak, repid, machp);
			event = papAuthBad;
		}
		break;

	case AuthenticateAck:
		repid = hp->pkt.id;
		if (repid != machp->crid)
			goto discard;
		event = papAuthAck;
		break;
	case AuthenticateNak:
		repid = hp->pkt.id;
		if (repid != machp->crid)
			goto discard;
		event = papAuthNak;
		break;
	}
	pap_fsm(machp, event);
	freemsg(mp);
	return;

discard:
no_action:
	freemsg(mp);
}


/*
 * pap_verify()
 *
 * Check the password the peer sent against the remote password set.
 *
 * Returns 1 for good password, 0 otherwise
 */
static int
pap_verify(papPasswdEntry_t *passwd, papMachine_t *machp)
{
	if (passwd->papPeerIdLen != machp->remote_passwd.papPeerIdLen)
		return (0);
	if (bcmp((caddr_t)passwd->papPeerId,
	    (caddr_t)machp->remote_passwd.papPeerId,
	    machp->remote_passwd.papPeerIdLen) != 0)
		return (0);
	if (passwd->papPasswdLen != machp->remote_passwd.papPasswdLen)
		return (0);
	if (bcmp((caddr_t)passwd->papPasswd,
	    (caddr_t)machp->remote_passwd.papPasswd,
	    machp->remote_passwd.papPasswdLen) != 0)
		return (0);
	return (1);
}


/*
 * pap_extract_passwd()
 *
 *  Remove passwd struct information from the given message block
 *
 *  Return 1 if extracted OK, 0 otherwise
 */
static int
pap_extract_passwd(mblk_t *mp, papPasswdEntry_t *passwd, uint_t totlen)
{
	ushort_t len;
	char *req;

	req = (char *)mp->b_rptr;
	totlen = (uint_t)msgdsize(mp);

	if (totlen-- < 1)
		return (0);
	len = (ushort_t)*req++;
	if (totlen < len)
		return (0);

	passwd->papPeerIdLen = len;
	bcopy((caddr_t)req, (caddr_t)passwd->papPeerId, len);

	totlen -= len;
	req += len;

	if (totlen-- < 1)
		return (0);
	len = (int)*req++;

	if (totlen < len)
		return (0);

	totlen -= len;

	passwd->papPasswdLen = len;
	bcopy((caddr_t)req, (caddr_t)passwd->papPasswd, len);


	return (1);
}


/*
 * laf()
 *
 * Report local authentication failure.
 */
static void
laf(papMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_error_ind(lp, pppLocalAuthFailed, NULL, (uint_t)0);
	ppp_internal_event(lp, PPP_LOCAL_FAILURE, pppAuthPAP);
}


/*
 * raf()
 *
 * Report remote authentication failure.
 */
static void
raf(papMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_error_ind(lp, pppRemoteAuthFailed, NULL, (uint_t)0);
	ppp_internal_event(lp, PPP_REMOTE_FAILURE, pppAuthPAP);
}


/*
 * aas()
 *
 * Report authentication success
 */
static void
aas(papMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_internal_event(lp, PPP_AUTH_SUCCESS, pppAuthPAP);
}


/*
 * free_pap_machine()
 *
 * Free pap machine structure.
 */
void
free_pap_machine(papMachine_t *machp)
{
	cancel_req_restart_timer(machp);
	cancel_wait_timer(machp);

	if (machp->result) {
		freemsg(machp->result);
	}
	if (machp->request) {
		freemsg(machp->request);
	}
	(void) kmem_free(machp, sizeof (papMachine_t));
}


/*
 * alloc_pap_machine()
 *
 * Allocate pap machine structure.
 */
papMachine_t *
alloc_pap_machine(queue_t *readq, pppLink_t *linkp)
{
	papMachine_t *machp;

	machp = (papMachine_t *)kmem_zalloc(sizeof (papMachine_t), KM_NOSLEEP);

	if (machp == NULL) {
		return (NULL);
	}

	machp->readq = readq;

	machp->state = P0;

	machp->papMaxRestarts = PAP_DEF_MAXRESTART;
	machp->papRestartTimerValue = PAP_DEF_RESTIMER;
	machp->papWaitTimerValue = PAP_DEF_WAITTIMER;

	machp->linkp = linkp;

	return (machp);
}


/*
 * chaeck_param()
 *
 * Check ioctl parameters for pap ioctl.
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
 * ************************************
 * Functions for ISDN compatibility
 * ************************************
 */

/*
 * get_isdn_passwd()
 *
 * Return a structure contain the password information from the old
 * ISDN style ioctl.
 */
papPasswdEntry_t *
get_isdn_passwd(mblk_t *mp)
{
	static papPasswdEntry_t entry;
	pppIsdnPAP_t *isdn_passwd;

	isdn_passwd = check_param(mp, sizeof (*isdn_passwd));
	if (isdn_passwd == NULL)
		return (NULL);

	entry.protocol = pppAuthPAP;
	entry.papPeerIdLen =
	    isdn_passwd->papPeerIdLen;
	bcopy((caddr_t)isdn_passwd->papPeerId,
	    (caddr_t)entry.papPeerId, entry.papPeerIdLen);
	entry.papPasswdLen = isdn_passwd->papPasswdLen;
	bcopy((caddr_t)isdn_passwd->papPasswd,
	    (caddr_t)entry.papPasswd, entry.papPasswdLen);
	return (&entry);
}


/*
 * set_isdn_passwd()
 *
 * Extract the ISDN passwd style structure from the message.
 */
int
set_isdn_passwd(mblk_t	 *mp, papPasswdEntry_t *passwd)
{
	pppIsdnPAP_t *isdn_passwd;

	isdn_passwd = check_param(mp, sizeof (*isdn_passwd));
	if (isdn_passwd == NULL)
		return (-1);

	isdn_passwd->papPeerIdLen = passwd->papPeerIdLen;
	bcopy((caddr_t)passwd->papPeerId, (caddr_t)isdn_passwd->papPeerId,
	    passwd->papPeerIdLen);
	isdn_passwd->papPasswdLen = passwd->papPasswdLen;
	bcopy((caddr_t)passwd->papPasswd, (caddr_t)isdn_passwd->papPasswd,
	    passwd->papPasswdLen);
	return (0);
}
