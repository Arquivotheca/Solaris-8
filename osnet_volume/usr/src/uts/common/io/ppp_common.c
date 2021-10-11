#pragma ident	"@(#)ppp_common.c	1.24	99/12/06 SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Contains the common processes for both the LCP and the IPNCP machines.
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/dlpi.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/t_lock.h>
#include <sys/strsun.h>
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
#endif /* ISERE_TREE */


static uint_t frame_id;

static void tld(pppMachine_t *);
static void tlu(pppMachine_t *);
static void tls(pppMachine_t *);
static void tlf(pppMachine_t *);
static void scr(pppMachine_t *);
static void sca(pppMachine_t *);
static void scn(pppMachine_t *);
static void str(pppMachine_t *);
static void sta(pppMachine_t *);
static void scj(pppMachine_t *);
static void ser(pppMachine_t *);
static void irc(pppMachine_t *);
static void zrc(pppMachine_t *);
static void get_opt(mblk_t *, pppOption_t *);
static void remove_opt(pppOption_t **, int);
static int  is_empty_opt(pppOption_t **, int);
static void cancel_restart_timer(pppMachine_t *);
static void ppp_timeout(void *);
static void cp_pkt_in(pppMachine_t *, mblk_t *);
static void cp_pkt_out(pppMachine_t *, mblk_t *);
static void build_pkt(mblk_t *, pppOption_t *);
static void apply_options(pppMachine_t *);
static void ppp_action(pppMachine_t *, pppAction_t, pppState_t);
static int  is_contained_in(mblk_t *, mblk_t *);
static int  is_equal_to(mblk_t *, mblk_t *);
static void free_opts(pppOption_t **, int);

/*
 * A STREAMS implementation of the Point-to-Point Protocol for the
 * Transmission of Multi-Protocol Datagrams over Point-to-Point Links
 *
 * Derived from RFC1171 (July 1990) - Perkins (CMU)
 */

/*
 * Entries in the PPP state machine are two-byte values encoded as a short.
 * Each entry is a tuple:
 *
 * < action > < new state >
 *
 * When an event "x" takes place in state "y" the tuple at [x][y]
 * is located, and < action > is performed,  the state changing to < new state >
 *
 * In this way, all the policy is held in the state table rather than
 * in discrete code.
 *
 * TODO: make this portable to non-SPARC word ordering
 */


/*
 * Two dimesional table for PPP finite state machine.  Columns are the states:
 * Initial, Starting, Closed, Stopped, Closing, Stopping, Req-Sent, Ack-Rcvd,
 * Ack-Sent, Opened.  Rows are the actions input to the fsm.
 */

static pppTuple_t ppp_statetbl[PPP_NEVENTS][PPP_NSTATES] = {

/* Up	*/ {S2, IRCSCR+S6, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, \
	FSM_ERR, FSM_ERR, FSM_ERR },
/* Down */ {FSM_ERR, FSM_ERR, S0, TLS+S1, S0, S1, S1, S1, S1, TLD+S1 },
/* Open */ { TLS+S1, S1, IRCSCR+S6, S3, S5, S5, S6, S7, S8, S9 },
/* Clos */ {FSM_ERR, S0, S2, S2, S4, S4, IRCSTR+S4, IRCSTR+S4, IRCSTR+S4, \
	TLDIRCSTR+S4 },
/* TO+	*/ {FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, STQ+S4, STQ+S5, SCR+S6, \
	SCR+S6, SCR+S8, FSM_ERR },
/* TO-	*/ {FSM_ERR, FSM_ERR, FSM_ERR, FSM_ERR, TLF+S2, TLF+S3, TLF+S3, \
	TLF+S3, TLF+S3, FSM_ERR },
/* RCR+ */ {FSM_ERR, FSM_ERR, STA+S2, IRCSCRSCA+S8, S4, S5, MCRSCA+S8, \
	SCATLU+S9,    SCA+S8, TLDSCRSCA+S8 },
/* RCR- */ {FSM_ERR, FSM_ERR, STA+S2, IRCSCRSCN+S6, S4, S5, SCN+S6, SCN+S7, \
	SCN+S6, TLDSCRSCN+S6 },
/* RCA	*/ {FSM_ERR, FSM_ERR, STA+S2, STA+S3, S4, S5, IRC+S7, SCR+S6, \
	IRCTLU+S9,    TLDSCR+S6 },
/* RCN	*/ {FSM_ERR, FSM_ERR, STA+S2, STA+S3, S4, S5, CIRCSCR+S6, SCR+S6, \
	IRCSCR+S8,    TLDSCR+S6 },
/* RTR	*/ {FSM_ERR, FSM_ERR, STA+S2, STA+S3, STA+S4, STA+S5, STA+S6, STA+S6, \
	STA+S6, TLDZRCSTA+S5 },
/* RTA	*/ {FSM_ERR, FSM_ERR, S2, S3, TLF+S2, TLF+S3, S6, S6, S8, TLDSCR+S6 },
/* RUC	*/ {FSM_ERR, FSM_ERR, SCJ+S2, SCJ+S3, SCJ+S4, SCJ+S5, SCJ+S6, SCJ+S7, \
	SCJ+S8, TLDSCJSCR+S6 },
/* RXJ+ */ {FSM_ERR, FSM_ERR, S2, S3, S4, S5, S6, S6, S8, S9 },
/* RXJ- */ {FSM_ERR, FSM_ERR, TLF+S2, TLF+S3, TLF+S2, TLF+S3, TLF+S3, TLF+S3, \
	TLF+S3, TLDIRCSTR+S5 },
/* RXR	*/ {FSM_ERR, FSM_ERR, S2, S3, S4, S5, S6, S7, S8, SER+S9 },
};

#ifdef PPP_DEBUG

static char *state_txt[] = {
	"Initial", "Starting", "Closed", "Stopped", "Closing", "Stopping",
	"Req-Sent", "Ack-Rcvd", "Ack-Sent", "Opened" };

static char *event_txt[] = {
	"Up", "Down", "Open", "Close", "TO+", "TO-", "RCR+", "RCR-",
	"RCA", "RCN", "RTR", "RTA", "RUC", "RXJ+", "RXJ-", "RXR" };

#endif

#define	IS_OPEN(machp)		((machp)->state == S9)
#define	no_timer_state(new_state)	((new_state) == S0 /* Initial  */ || \
	(new_state) == S1 /* Starting	*/ || \
	(new_state) == S2 /* Closed	*/ || \
	(new_state) == S3 /* Stopped	*/ || \
	(new_state) == S9 /* Open	*/)

/*
 * is_open()
 *
 * Return 1 if machine is in open state, 0 otherwise.
 */
int
is_open(pppMachine_t	*machp		/* pointer to state machine */)
{
	return (IS_OPEN(machp));
}

/*
 * ppp_fsm()
 *
 * PPP finite state machine engine
 */
void
ppp_fsm(pppMachine_t	*machp,		/* pointer to state machine */
	pppEvent_t	event		/* current event */)

{
	pppTuple_t		tuple;
	pppAction_t		func;
	pppState_t		old_state, new_state;
	ASSERT(MUTEX_HELD(&machp->linkp->lplock));

	if (event == NullEvent)
		return;
	tuple = ppp_statetbl[event][machp->state];
	if (tuple == FSM_ERR) {
#ifdef PPP_DEBUG
		PPP_FSMDBG4("ppp_fsm out of fsm: prot = %x machp = %p \
		    state %s, event = %s\n",
		    machp->protocol, machp,
		    state_txt[machp->state], event_txt[event]);
#endif
		return;
	}
	new_state = tuple & 0xff;
	func = (pppAction_t)tuple >> 8;

	if (machp == NULL || machp == (pppMachine_t *)0x18) {
		PPP_STRDBG("machp == NULL %s\n", "");
		return;
	}
#ifdef PPP_DEBUG
	PPP_FSMDBG4("ppp_fsm: prot=%x machp=%p state %s, event=%s\n",
	    machp->protocol, machp,
	    state_txt[machp->state], event_txt[event]);
#endif
	if (machp->protocol == pppLCP) {
		machp->linkp->state.pppLinkPreviousState = machp->state;
	}

	old_state = machp->state;

/*
 * do the appropriate action
 */

	ppp_action(machp, func, new_state);

	if (machp->protocol == pppLCP && (new_state == S1 /* Starting */ ||
			new_state == S2)) {
		machp->starting = 1;
	}

/*
 * note state change
 */
	machp->laststate = old_state;

	PPP_FSMDBG("ppp_fsm: SUCCESS new state=%s\n",
		state_txt[machp->state]);


	if (no_timer_state(new_state)) {
		cancel_restart_timer(machp);
	}


/*
 * log the event in the MIB
 */
	if (machp->protocol == pppLCP) {
		machp->linkp->state.pppLinkCurrentState = machp->state;
		machp->linkp->state.pppLinkChangeTime = hrestime;
	}
}

/*
 * PPP packet parsing routines...
 */

#define	VERIFY_PKTID	if (hp->pkt.id != machp->crid) { \
				goto discard; \
			}
#define	WHOLEMSG	(-1)

/*
 * do_incoming_cp()
 *
 * Takes a LCP/NCP packet and analyses it, generating the
 * appropriate event for the finite state machine.
 *
 * It is passed a PPP frame which includes at least a frame header.
 *
 * At the end of execution, either the message has been used and freed	(or
 * stored for later use), or it has been put back on the queue due to
 * shortage of resources (SUSPEND returned)
 *
 * Note: do_incoming_cp() pulls up and aligns the cp packet
 */
void
do_incoming_cp(pppMachine_t *machp, mblk_t *mp)
{
	pppEvent_t			event;
	struct ppp_hdr			*hp;
	pppOption_t			*imp_opt;
	pppOption_t			op;
	mblk_t				*zmp, *nmp, *conf_nak, *conf_rej,
					*mag_nak;
	int				rc;
	int				pass;
	int				opt_res[PPP_MAX_OPTS+1];
	int				i;
	uint_t				other_addr;
	uint_t				mnum;
	uint_t				*magic_ptr;
	int				ignore_found, fatal_found;
	ushort_t				pktlength;

#ifdef PPP_DEBUG
	if (machp->pkt != NULL) { /* XXX check for corruption here */
		PPP_STRDBG("ppp: non-null pkt detected machp=%p\n", machp);

	}
#endif

	if (machp->state == S0 || machp->state == S1) {
#ifdef PPP_DEBUG
		PPP_FSMDBG2("CP recd. in wrong state: prot = %x state = %s\n",
			machp->protocol, state_txt[machp->state]);
#endif
		goto discard;
	}

/*
 * to allow easy processing of the protocol options, pull up the
 * rest of the control protocol packet.	 In fact, this should
 * not be too inefficient, as the messages coming from the driver
 * are probably be suitably aligned and concatenated already.
 * But, it is better to be safe than sorry...
 */

	if (!ISPULLEDUP(mp)) {
		zmp = msgpullup(mp, -1);
		freemsg(mp);
		mp = zmp;
		if (mp == NULL) {
			machp->pkt = NULL;
			return;
		}
	}


/*
 * get a handle on the frame and packet header
 */
	if (msgdsize(mp) < PPP_FR_HDRSZ) {
		machp->linkp->errs.pppLinkPacketTooShorts++;
		goto discard;
	}

	hp = (struct ppp_hdr *)mp->b_rptr;

/*
 * validate the indicated packet length, so as to avoid crashing out on
 * short packets.  The received packet must be at least as long
 * as the length field.	  Anything over is padding and can be
 * ignored [RFC1331 page 29]
 */

	pktlength = ntohs(hp->pkt.length);
	if (pktlength > (msgdsize(mp) - PPP_FR_HDRSZ)) {
		machp->linkp->errs.pppLinkPacketTooShorts++;
		goto discard;
	}


	/* trim off any padding from the end of the packet */
	if (pktlength < (msgdsize(mp) - PPP_FR_HDRSZ)) {
		(void) adjmsg(mp, pktlength - (msgdsize(mp)-PPP_FR_HDRSZ));
	}

	cp_pkt_in(machp, mp);

/*
 * now examine the packet type...
 */
	switch (hp->pkt.code) {

	case ConfigureReq:

/*
 * Received a Configure Request packet
 *
 * parse it, putting anything anything unacceptable
 * in either a Configure Nak packet (it is needs to be
 * negotiated [RFC1331 page 32], or in a Configure Reject
 * packet if it is unacceptable for negotiation
 * [RFC1331 page 33].
 *
 * If everything was acceptable both the Conf.Nak and
 * Conf.Rej packets will be empty
 */

		mag_nak = NULL;

/*
 * allocate a prospective Configure Nak packet
 */
		conf_nak = ppp_alloc_frame(machp->protocol,
		    ConfigureNak, hp->pkt.id);

		if (conf_nak == (mblk_t *)NULL) {
			goto discard;
		}

/*
 * allocate a prospective Configure Reject packet
 */
		conf_rej = ppp_alloc_frame(machp->protocol,
		    ConfigureRej, hp->pkt.id);

		if (conf_rej == (mblk_t *)NULL) {

/*
 * tidy up and leave...
 */
			freemsg(conf_nak);
			goto discard;
		}

/*
 * make a copy of the current packet for parsing purposes
 */
/*
 * N.B. this *must* be a copymsg since we hand off
 * one copy to putnext and then modify the data
 */
		nmp = copymsg(mp);
		if (nmp == (mblk_t *)NULL) {
			freemsg(conf_nak);
			freemsg(conf_rej);
			return;
		}

		machp->stats.pppCPInCRs++;

/*
 * skip the frame and packet header
 */
		(void) adjmsg(nmp, PPP_HDRSZ);

		ASSERT(machp->imp);

		for (i = 1; i <= machp->optsz; i++)
			opt_res[i] = OPT_ABSENT;

		ignore_found = 0;
		fatal_found = 0;
/*
 * now parse each of the options in turn...
 */
		for (get_opt(nmp, &op); op.length != 0; get_opt(nmp, &op)) {

/*
 * check if valid type code and reasonable length
 */
			if (op.type < 1 || op.type > machp->optsz) {

/*
 * it is bad, so stuff it in a Configure
 * Reject packet
 */

				PPP_OPTDBG("ppp: bad type code %d\n", op.type);
				opt_res[op.type] = OPT_NOK;
				build_pkt(conf_rej, &op);
				continue;
			}

			imp_opt = machp->imp[op.type];
			if (imp_opt == (pppOption_t *)NULL) {

/*
 * not implemented, add the option to the
 * Configure Reject packet
 */
				PPP_OPTDBG("ppp: rejecting opt=%d\n", op.type);
				opt_res[op.type] = OPT_NOK;
				build_pkt(conf_rej, &op);
				continue;
			}

/*
 * the option is implemented, but is the value
 * acceptable?	 Find this out by calling
 * the negotiate function associated
 * with the option...
 */

			if (imp_opt->negotiate == NULL)
				rc = OPT_NOK;
			else if (machp->allowneg[op.type] & REM_DISALLOW)
				rc = OPT_NOK;
			else
				rc = (*imp_opt->negotiate)(machp, &op);

			opt_res[op.type] = rc;

			switch (rc) {

			case OPT_OK:
/*
 * the option is acceptable
 */
				continue;

			case OPT_NOK:

/*
 * the option was unacceptable, no alternative
 * exists - place it in the conf.rej packet
 */
				PPP_OPTDBG("ppp: rejecting op=%d\n", op.type);
				build_pkt(conf_rej, &op);
				continue;

			case OPT_NEW:

/*
 * a new value has been suggested,
 * place it in the conf.nak packet
 */
				PPP_OPTDBG("ppp: negotiating op=%d\n", op.type);
				build_pkt(conf_nak, &op);
				continue;

			case OPT_FATAL:
				fatal_found = 1;
				continue;
			case OPT_IGNORE:
				ignore_found = 1;
				continue;
			case OPT_LOOP:
				mag_nak = ppp_alloc_frame(machp->protocol,
				    ConfigureNak, hp->pkt.id);
				if (mag_nak == NULL) {
					freemsg(nmp);
					freemsg(mp);
					return;
				}
				build_pkt(mag_nak, &op);
				continue;
			}
		}
		for (i = 1; i <= machp->optsz; i++) {
			if ((opt_res[i] == OPT_ABSENT) &&
			    (machp->allowneg[i] & REM_MAND)) {
				if ((machp->protocol == pppIP_NCP) &&
				    (i == IPAddr))
					continue;
				if ((machp->protocol == pppIP_NCP) &&
				    (i == IPAddrNew) &&
				    (opt_res[IPAddr] != OPT_ABSENT))
					continue;
				op.type = (uchar_t)i;
				op.flags = (uchar_t)DO_NAK;
				imp_opt = machp->imp[op.type];
				if (imp_opt->negotiate == NULL)
					rc = OPT_NOK;
				else
					rc = (*imp_opt->negotiate)(machp, &op);
				switch (rc) {
				case OPT_NEW:
					build_pkt(conf_nak, &op);
					continue;
				}
			}
		}


		freemsg(nmp);

/*
 * have we put anything into the Configure Reject packet?
 */
		/* if sent too many conf nak drop the conn */
		if (machp->conf_nak_counter <= 0) {
			if (mag_nak != NULL) {
				freemsg(mag_nak);
			}
			freemsg(conf_nak);
			freemsg(conf_rej);
			freemsg(mp);
			event = Close;
			break;
		}

		if (mag_nak != NULL) {
			freemsg(conf_nak);
			freemsg(conf_rej);
			freemsg(mp);
			machp->pkt = mag_nak;
			event = Rcrbad;
			break;
		}

		if (ignore_found) {
			freemsg(conf_rej);
			freemsg(conf_nak);
			freemsg(mp);
			event = NullEvent;
			break;
		}

		if (fatal_found) {
			freemsg(conf_rej);
			freemsg(conf_nak);
			freemsg(mp);
			event = Rxjbad;
			break;
		}

		if (msgdsize(conf_rej) > PPP_HDRSZ) {

/*
 * yes, this implies that we must forget the
 * Configure Ack, and send the Configure Reject.
 * We can dispose of the original packet too
 * at this point
 */
			freemsg(conf_nak);
			freemsg(mp);
			machp->pkt = conf_rej;
			PPP_OPTDBG("ppp: machp=%p, conf.rej\n", machp);
			event = Rcrbad;
			break;
		}

/*
 * have we put anything into the Configure Nak packet?
 */

		if (msgdsize(conf_nak) > PPP_HDRSZ) {

/*
 * yes, this implies that we must send a
 * Configure Nak, with the options which
 * were negotiated.  Time to ditch the original
 * packet too.
 */

			freemsg(conf_rej);
			freemsg(mp);
			machp->pkt = conf_nak;
			PPP_OPTDBG("ppp: machp=%p, conf.nak\n", machp);
			event = Rcrbad;
			break;
		}

/*
 * good configure request
 */
		freemsg(conf_rej);
		freemsg(conf_nak);
		event = Rcrgood;
		machp->pkt = mp;
		break;

	case ConfigureAck:

		machp->stats.pppCPInCAs++;

		VERIFY_PKTID;

		if (!is_equal_to(mp, machp->crp))
			goto discard;

/*
 * implement final value of options negotiated
 */
		apply_options(machp);

		freemsg(mp);
		freemsg(machp->crp);
		machp->crp = NULL;
		event = Rca;
		break;

	case ConfigureNak:

		machp->stats.pppCPInCNs++;

		VERIFY_PKTID;

		(void) adjmsg(mp, PPP_HDRSZ);

		ASSERT(machp->imp);

/*
 * now parse each of the options in turn...
 */
		for (get_opt(mp, &op); op.length != 0; get_opt(mp, &op)) {
			if (op.type < 1 || op.type > machp->optsz ||
			    op.length > PPP_MAX_OPTSZ) {

/*
 * it is bad, so stuff it in a Configure
 * Reject packet
 */

				PPP_OPTDBG("ppp: bad type code %d\n", op.type);
				continue;
			}
			imp_opt = machp->inbound[op.type];
			if (imp_opt == (pppOption_t *)NULL) {

/*
 * not implemented, add the option to the
 * Configure Reject packet, I don't know
 * what to do in this case yet.
 */
				PPP_OPTDBG("ppp: rejecting opt=%d\n", op.type);
				continue;
			}
/*
 * the option is implemented, but is the value
 * acceptable?	 Find this out by calling
 * the negotiate function associated
 * with the option...
 */

			if (imp_opt->negotiate == NULL)
				rc = OPT_NOK;
			else
				rc = (*imp_opt->negotiate)(machp, &op);

			switch (rc) {

			case OPT_OK:
/*
 * Get rid of the option we presumably tried
 * to negotiate in the first place
 */

				remove_opt(machp->inbound, op.type);

/*
 * the option is acceptable add it to the
 * list for the next scr
 */
				(void) add_opt(machp->inbound, op.type,
				    op.length, (caddr_t)op.data,
				    imp_opt->negotiate);

				continue;

			case OPT_NOK:
			case OPT_NEW:
/*
 * The peer's suggested value was bad, and
 * it didn't like ours.	 Just get rid of the
 * option we are having trouble with.
 */

				remove_opt(machp->inbound, op.type);
				if ((machp->allowneg[op.type] & LOC_MAND) &&
				    is_empty_opt(machp->inbound, op.type)) {
					pass = 0;
					if ((machp->protocol == pppIP_NCP) &&
					    (op.type == IPAddr || op.type ==
					    IPAddrNew)) {
						other_addr = (op.type ==
						    IPAddr ? IPAddrNew :
						    IPAddr);
						if ((machp->
						    allowneg[other_addr] &
						    LOC_MAND) &&
						    !is_empty_opt(machp->
							inbound,
							other_addr)) pass = 1;
					}
					if (!pass) {
						freemsg(mp);
						freemsg(machp->crp);
						machp->crp = NULL;
						ppp_error_ind(machp->linkp,
							pppNegotiateFailed,
							NULL, 0);
						event = Close;
						goto in_machine;
					}
				}

				PPP_OPTDBG("ppp: rejecting op=%d\n", op.type);
				break;

			}
		}


		freemsg(mp);
		freemsg(machp->crp);
		machp->crp = NULL;
		event = Rcn;
		break;

	case ConfigureRej:

		machp->stats.pppCPInCRejs++;

		VERIFY_PKTID;


/*
 * the options listed must be a proper subset of those
 * listed in the configuration request packet or the
 * packet is invalid and should be silently discarded
 * [RFC1331 page 29]
 */

		if (!is_contained_in(machp->crp, mp)) {
			goto discard;
		}

		(void) adjmsg(mp, PPP_HDRSZ);

		for (get_opt(mp, &op); op.length != 0; get_opt(mp, &op)) {
			if (op.type < 1 || op.type > machp->optsz ||
			    op.length > PPP_MAX_OPTSZ) {

/*
 * An unrecognizable rejected option,
 * ignore it.
 */

				PPP_OPTDBG("ppp: bad type code %d\n", op.type);
				continue;
			}
/*
 * Dump the option they rejected from the list we
 * are trying to negotiate.
 */

			remove_opt(machp->inbound, op.type);

			if ((machp->allowneg[op.type] & LOC_MAND) &&
			    is_empty_opt(machp->inbound, op.type)) {
				pass = 0;
				if ((machp->protocol == pppIP_NCP) &&
				    (op.type == IPAddr || op.type ==
				    IPAddrNew)) {
					other_addr = (op.type ==
					    IPAddr ? IPAddrNew : IPAddr);
					if (machp->allowneg[other_addr] &
					    LOC_MAND &&
					    !is_empty_opt(machp->inbound,
						other_addr)) pass = 1;
				}
				if (!pass) {
					freemsg(mp);
					freemsg(machp->crp);
					machp->crp = NULL;
					ppp_error_ind(machp->linkp,
						pppNegotiateFailed,
						NULL, 0);
					event = Close;
					goto in_machine;
				}
			}

		}

		freemsg(mp);
		freemsg(machp->crp);
		machp->crp = NULL;
			/* For now assume no catatophic config rejects */
		event = Rcn;
		break;

	case TerminateReq:


		machp->stats.pppCPInTRs++;
		machp->pkt = mp;
		event = Rtr;
		break;

	case TerminateAck:

/*
 * here we don't bother checking the packet id, because
 * the reception of any terminate ack indicates that the
 * connection has been closed [RFC1331 page 35]
 */
		machp->stats.pppCPInTAs++;

		freemsg(mp);
		event = Rta;
		break;

	case CodeRej:

		machp->stats.pppCPInCodeRejs++;
		freemsg(mp);
		event = Rxjgood;
		break;

	case ProtoRej:

/*
 * we have a protocol reject.  Since we are supporting
 * only one protocol at the moment this is bad news.
 * There is nothing more so be done except take the
 * stream down with an M_ERROR
 *
 * polo: bugid 4156184, esc 516085, Nov 1998
 * Updated to take down the link only if the most fundamental
 * protocols are rejected.  The standard says that when a
 * protocol-reject packet is received, the PPP implementation
 * should stop sending that type of packet, not bring down
 * the interface. [RFC1331, page 38]
 */

		machp->stats.pppCPRejects++;

		if (machp->protocol == pppIP_NCP ||
		    machp->protocol == pppLCP ||
		    machp->protocol == pppIP_PROTO)
		(void) putnextctl1(machp->readq, M_ERROR, EPROTONOSUPPORT);

		goto discard;

	case EchoReq:
		if (pktlength < 8)
			goto discard;
		magic_ptr = (uint_t *)(mp->b_rptr + PPP_HDRSZ);
		bcopy((caddr_t)magic_ptr, (caddr_t)&mnum,
			sizeof (mnum));
		if (mnum && mnum ==
		    machp->linkp->conf.pppLinkLocalMagicNumber) {
			ppp_reneg(machp->linkp);
			goto discard;
		}
		mnum = machp->linkp->conf.pppLinkLocalMagicNumber;
		bcopy((caddr_t)&mnum, (caddr_t)magic_ptr,
			sizeof (mnum));
		hp->pkt.code = EchoRep;
		machp->stats.pppCPInEchoReqs++;
		machp->pkt = mp;
		event = Rxr;
		break;

	case DiscardReq:

/*
 * discard all discard requests silently
 */
		machp->stats.pppCPInDiscReqs++;
		freemsg(mp);
		return;

	case EchoRep:

/*
 * discard all echo responses silently
 */
		if (pktlength < 8)
			goto discard;
		machp->stats.pppCPInEchoReps++;
		freemsg(mp);
		machp->pkt = NULL;
		return;

	default:
/*
 * unknown packet type
 */
		machp->pkt = mp;
		event = Ruc;

	}

/*
 * and now pass the incoming event though the state machine
 */

in_machine:
	ppp_fsm(machp, event);

/*
 * if the message was not consumed, free it
 */
	if (machp->pkt != NULL) {
		freemsg(machp->pkt);
		machp->pkt = NULL;
	}

	return;


discard:
/*
 * drop the current packet and continue processing...
 */
	freemsg(mp);
	machp->pkt = NULL;
}

#undef SUSPEND

/*
 * ppp_action()
 *
 * PPP actions ... Refers to action fields of finite stat machine
 */
static void
ppp_action(pppMachine_t *machp, pppAction_t action, pppState_t new_state)
{
	PPP_STRDBG("ppp_action: Doing %d\n", action);
	switch (action) {
		case Tld:
			tld(machp);
			break;
		case Tlu:
			tlu(machp);
			break;
		case Tls:
			tls(machp);
			break;
		case Tlf:
			tlf(machp);
			break;
		case Irc:
			irc(machp);
			break;
		case Zrc:
			zrc(machp);
			break;
		case Scr:
			scr(machp);
			break;
		case Sca:
			sca(machp);
			break;
		case Mcrsca:
			if (machp->starting) {
				machp->starting = 0;
				scr(machp);
			}
			sca(machp);
			break;
		case Scn:
			scn(machp);
			break;
		case Mcrscn:
			if (machp->starting) {
				machp->starting = 0;
				scr(machp);
			}
			scn(machp);
			break;
		case Str:
			str(machp);
			break;
		case Sta:
			sta(machp);
			break;
		case Scj:
			scj(machp);
			break;
		case Ser:
			ser(machp);
			break;
		case Ircscr:
			irc(machp);
			scr(machp);
			break;
		case Circscr:
			if (!(machp->linkp->looped_back))
				irc(machp);
			scr(machp);
			break;
		case Ircscrsca:
			irc(machp);
			scr(machp);
			sca(machp);
			break;
		case Ircstr:
			irc(machp);
			str(machp);
			break;
		case Ircscrscn:
			irc(machp);
			scr(machp);
			scn(machp);
			break;
		case Scatlu:
			sca(machp);
			tlu(machp);
			break;
		case Irctlu:
			irc(machp);
			tlu(machp);
			break;
		case Tldircstr:
			tld(machp);
			irc(machp);
			str(machp);
			break;
		case Tldscrsca:
			tld(machp);
			scr(machp);
			sca(machp);
			break;
		case Tldscrscn:
			tld(machp);
			scr(machp);
			scn(machp);
			break;
		case Tldzrcsta:
			tld(machp);
			zrc(machp);
			sta(machp);
			break;
		case Tldscr:
			tld(machp);
			scr(machp);
			break;
		case Tldscjscr:
			tld(machp);
			scj(machp);
			scr(machp);
			break;
		default:
			break;
	}
	machp->state = new_state;

}


/*
 * tld()
 *
 * Processes a this-layer-down for the LCP or NCP layer
 *
 */
static void
tld(pppMachine_t *machp)
{
	PPP_APIDBG("ppp_cp_is_open: proto=%x\n", machp->protocol);

	machp->attempts = machp->max_attempts;

	ppp_internal_event(machp->linkp, PPP_TL_DOWN, machp->protocol);
}


/*
 * tlf()
 *
 * Processes this-layer-finish for the LCP or NCP layer
 *
 */
static void
tlf(pppMachine_t *machp)
{
	PPP_APIDBG("ppp_cp_is_open: proto=%x\n", machp->protocol);

	ppp_internal_event(machp->linkp, PPP_TL_FINISH, machp->protocol);
}


/*
 * tlu()
 *
 * Processes the this-layer-up event for the LCP or NCP layer
 *
 */
static void
tlu(pppMachine_t *machp)
{
	PPP_APIDBG("ppp_cp_is_open: proto=%x\n", machp->protocol);

	ppp_internal_event(machp->linkp, PPP_TL_UP, machp->protocol);
}


/*
 * tls()
 *
 * Process this-layer-start for LPC or NCP
 *
 */
static void
tls(pppMachine_t *machp)
{
	pppLink_t *lp;

	lp = machp->linkp;

	ppp_internal_event(lp, PPP_TL_START, machp->protocol);
}


/*
 * scr()
 *
 * Send a Configure Request packet
 */
static void
scr(pppMachine_t *machp)
{
	mblk_t			*fp;
	pppOption_t		*op;
	struct ppp_hdr		*hp;
	uint_t			i;

	PPP_FSMDBG("scr: called proto=%x\n", machp->protocol);

	ASSERT(machp);

/*
 * set the configure request frame id, and store it, it will be
 * needed later to match against the configure ack/nak
 */
	machp->crid = frame_id++;

	if (machp->crp) {

/*
 * we are retransmitting a configure request due to
 * timeout.   Re-use last configure packet for efficiency,
 * bumping up the packet id
 */

		hp = (struct ppp_hdr *)machp->crp->b_rptr;
		hp->pkt.id = machp->crid;
		fp = machp->crp;

		machp->linkp->errs.pppLinkConfigTimeouts++;

	} else {

/*
 * get a new configure packet
 */
		fp = ppp_alloc_frame(machp->protocol,
		    ConfigureReq, machp->crid);

		if (fp == (mblk_t *)NULL) {

/*
 * allocb has probably failed
 */
			return;
		}

/*
 * add the options the user has specified.   If the option is
 * default, do not include it [RFC1171 page 9].	 If we reach
 * the end of the list for an option which is mandatory,
 * abort the connection with a disconnect
 */
		for (i = 1; i <= machp->optsz; i++) {

			/* Check to see if negotiation is allowed */
			if (machp->allowneg[i] & LOC_DISALLOW) {
				continue;
			}

/*
 * If we're sending IP Adress (RFC1332) don't send
 * IP Addresses
 */
			if ((machp->protocol == pppIP_NCP) &&
			    (i == IPAddr) && (machp->inbound[IPAddrNew]) &&
			    (machp->allowneg[IPAddrNew] != LOC_DISALLOW))
				continue;

			op = machp->inbound[i];

			if (op == NULL) {
/*
 * option is default
 */
				continue;
			}

/*
 * include the user's option in the conf.req
 */
			PPP_OPTDBG("scr: adding opt %d\n", op->type);
			build_pkt(fp, op);

		}

	}

/*
 * store this configure request packet for future reference.
 */
/*
 * N.B. since we hand this message off to the driver via
 *	a putnext and then modify the data to up the
 *	refcount this must be a copymsg instead of a dupmsg
 */
	machp->crp = copymsg(fp);

	if (machp->crp == (mblk_t *)NULL) {

/*
 * no more buffers.   Can't do much about this here,
 * except fail the action and remain in current state
 */
		freemsg(fp);
		return;
	}

	PPP_FRAME_DUMP("scr", fp);

	machp->stats.pppCPOutCRs++;
	cp_pkt_out(machp, fp);

	ppp_putnext(WR(machp->readq), fp);

/*
 * bump up the number of configure request transmits
 */
	machp->rsts++;
}


/*
 * sca()
 *
 * Send a Configuration Ack packet
 */

static void
sca(pppMachine_t *machp)
{
	struct ppp_hdr		*hp;

	if (machp->pkt == NULL) {
		PPP_STRDBG("machp->pkt== NULL %s\n", "");
		return;
	}
	PPP_FSMDBG("sca: called proto=%x\n", machp->protocol);
	ASSERT(machp->pkt);
/*
 * turn around the incoming configure request packet, and
 * send the same packet back again with the type
 * code change to be Configure Ack.  Everything else
 * remains unchanged
 */

	hp = (struct ppp_hdr *)machp->pkt->b_rptr;
	hp->pkt.code = ConfigureAck;

	PPP_FRAME_DUMP("sca", machp->pkt);

	machp->stats.pppCPOutCAs++;
	cp_pkt_out(machp, machp->pkt);

	ppp_putnext(WR(machp->readq), machp->pkt);

	machp->pkt = NULL;
}


/*
 * scn()
 *
 * Send a Configuration Nak/Configuration Reject packet
 */
/* ARGSUSED */
static void
scn(pppMachine_t *machp)
{
	struct ppp_hdr		*hp;

	PPP_FSMDBG("scn: called proto=%x\n", machp->protocol);

	ASSERT(machp->pkt);

/*
 * do_incoming_cp() will have parsed the configure request
 * and built either a Configure Nak or Configure Reject
 * packet as appropriate.  scn() sends that packet
 */

	PPP_FRAME_DUMP("scn", machp->pkt);

	hp = (struct ppp_hdr *)machp->pkt->b_rptr;

	if (hp->pkt.code == ConfigureRej) {
		machp->stats.pppCPOutCRejs++;
	} else {
		machp->stats.pppCPOutCNs++;
	}

	machp->conf_nak_counter--;

	cp_pkt_out(machp, machp->pkt);

	ppp_putnext(WR(machp->readq), machp->pkt);

	machp->pkt = NULL;
}


/*
 * str()
 *
 * Send a Terminate Request packet
 */
static void
str(pppMachine_t *machp)
{
	mblk_t			*fp;

	PPP_FSMDBG("str: called proto=%x\n", machp->protocol);

	fp = ppp_alloc_frame(machp->protocol, TerminateReq, frame_id++);

	if (fp == (mblk_t *)NULL) {

/*
 * allocb has failed
 */
		return;
	}

	if (machp->rsts) {
		machp->linkp->errs.pppLinkTerminateTimeouts++;
	}

	PPP_FRAME_DUMP("str", fp);

	machp->stats.pppCPOutTRs++;
	cp_pkt_out(machp, fp);

	ppp_putnext(WR(machp->readq), fp);

/*
 * bump up the number of terminate request transmits
 */
	machp->rsts++;
}


/*
 * sta()
 *
 * Send a Terminate Ack packet
 */
static void
sta(pppMachine_t *machp)
{
	struct ppp_hdr		*hp;

	PPP_FSMDBG("sta: called proto=%x\n", machp->protocol);

	if (machp->pkt) {

/*
 * turn around the incoming terminate request packet, and
 * send the same packet back again with the type
 * code change to be Terminate Ack.  Everything else
 * remains unchanged
 */

		hp = (struct ppp_hdr *)machp->pkt->b_rptr;
		hp->pkt.code = TerminateAck;

	} else {

/*
 * unsolicited Terminate Ack packet
 */
		machp->pkt =
		    ppp_alloc_frame(machp->protocol, TerminateAck, frame_id++);
		if (machp->pkt == NULL) {
			return;
		}

	}
	PPP_FRAME_DUMP("sta", machp->pkt);

	machp->stats.pppCPOutTAs++;
	cp_pkt_out(machp, machp->pkt);

	ppp_putnext(WR(machp->readq), machp->pkt);

	machp->pkt = NULL;
}


/*
 * scj()
 *
 * Send a Code Reject packet
 */
static void
scj(pppMachine_t *machp)
{
	mblk_t			*fp;
	struct ppp_hdr		*hp;
	ushort_t		pktlength;

	ASSERT(machp->pkt);

	PPP_FSMDBG("scj: called proto=%x\n", machp->protocol);

	if ((fp = ppp_alloc_frame(pppLCP, CodeRej, frame_id++)) ==
	    (mblk_t *)NULL) {
		return;
	}

/*
 * drop the frame header and truncate to fit in maximum
 * receive unit for the other party
 */

	(void) adjmsg(machp->pkt, PPP_FR_HDRSZ);

/*
 * truncate to remote's maximum receive unit size if necessary
 */

	if (msgdsize(machp->pkt) > (int)machp->linkp->conf.pppLinkRemoteMRU) {
		(void) adjmsg(machp->pkt,
		machp->linkp->conf.pppLinkRemoteMRU - msgdsize(machp->pkt));
	}

	hp = (struct ppp_hdr *)fp->b_rptr;

	pktlength = ntohs(hp->pkt.length);
	pktlength += (ushort_t)msgdsize(machp->pkt);
	hp->pkt.length = htons(pktlength);

/*
 * and attach it to the new frame and packet header
 */

	fp->b_cont = machp->pkt;

	PPP_FRAME_DUMP("scj", machp->pkt);

	machp->stats.pppCPOutCodeRejs++;
	cp_pkt_out(machp, fp);

	ppp_putnext(WR(machp->readq), fp);

	machp->pkt = NULL;
}


/*
 * ser()
 *
 * Send a Send Echo Reply packet
 */
/* ARGSUSED */
static void
ser(pppMachine_t *machp)
{
	struct ppp_hdr		*hp;
	uint_t			pktsz;

	ASSERT(machp->pkt);

	PPP_FSMDBG("ser: called proto=%x\n", machp->protocol);

/*
 * turn around the incoming Echo Request packet, and
 * send the same packet back again with the type
 * code change to be Echo Reply.  The packet
 * contents are truncated to suit the maximum
 * receive unit of the sender [RFC1331 Page 39]
 */

	hp = (struct ppp_hdr *)machp->pkt->b_rptr;

	pktsz = (uint_t)msgdsize(machp->pkt);
	if (pktsz > machp->linkp->conf.pppLinkRemoteMRU + PPP_FR_HDRSZ) {

/*
 * truncate to suit remote mru
 */
		hp->pkt.length = htons(machp->linkp->conf.pppLinkRemoteMRU);
		(void) adjmsg(machp->pkt, machp->linkp->conf.pppLinkRemoteMRU +
							PPP_FR_HDRSZ - pktsz);
	}

	PPP_FRAME_DUMP("ser", machp->pkt);

	machp->stats.pppCPOutEchoReps++;
	cp_pkt_out(machp, machp->pkt);

	ppp_putnext(WR(machp->readq), machp->pkt);

	machp->pkt = NULL;
}


/*
 * send_protocol_reject()
 *
 * send a Protocol Reject packet.   This is called from outside the state
 * machine.
 */
void
send_protocol_reject(pppLink_t *lp, queue_t *q, mblk_t *mp)
{
	mblk_t			*fp;
	struct ppp_hdr		*hp;

	PPP_FSMDBG("send_protocol_reject: called\n",
	    lp->readq);

	if ((fp = ppp_alloc_frame(pppLCP, ProtoRej, frame_id++)) ==
	    (mblk_t *)NULL) {
		return;
	}

/*
 * drop the addr & ctrl field of the frame header and truncate
 * to fit in maximum receive unit for the other party
 */

	(void) adjmsg(mp, 2); /* trim the frame addr + ctrl fields */

/*
 * truncate to remote's maximum receive unit size if necessary
 */

	if (msgdsize(mp) > (int)lp->conf.pppLinkRemoteMRU) {
		(void) adjmsg(mp, lp->conf.pppLinkRemoteMRU - msgdsize(mp));
	}

	hp = (struct ppp_hdr *)fp->b_rptr;

	hp->pkt.length = htons(ntohs(hp->pkt.length) + (ushort_t)msgdsize(mp));

/*
 * and attach it to the new frame and packet header
 */

	fp->b_cont = mp;

	ppp_putnext(q, fp);
}


/*
 * Option processing routines...
 */

static int
is_empty_opt(pppOption_t *optptr[], int type)
{
	pppOption_t *op;

	op = optptr[type];
	if (op == NULL)
		return (1);
	else return (0);

}


/*
 * void remove_opt
 *
 * remove the first option in the list of options,  if the option list is
 * empty, do nothing.
 */
static void
remove_opt(pppOption_t *optptr[], int type)
{
	pppOption_t *op;

	op = optptr[type];
	if (op == NULL)
		return;
	optptr[type] = op->next;
	/* bug 4010300  memory leak in kmem_alloc_320 */
	PPP_MEMDBG2("remove_opt: freeing %p, type=%x\n", op, type);
	kmem_free(op, sizeof (pppOption_t));

}


/*
 * add_opt()
 *
 * add an option to the list of options.  An option length of 0 implies that
 * the option is not supported, otherwise the minimum valid option size is
 * two octets.	Note that options are inserted at the front of the list.
 *
 * Returns: 0 on success, error number on failure.
 */
int
add_opt(pppOption_t *optptr[], uint_t type, uint_t length, caddr_t data,
	int (*negotiate)())
{
	pppOption_t	*op, *firstp, *optp;

	ASSERT(optptr != NULL);
	ASSERT(length == 0 || length >= 2);

/*
 * Detect two bytes in doing bcmp for type and length fields which are
 * included in the length.
 */


/*
 * Scan option list having Type type and check if the option
 * we are trying to add is already present.
 * If we found  a duplicate block, return 0
 */
	firstp = optptr[type];

	for (optp = firstp; optp != NULL; optp = optp->next) {
		if ((optp->type == type) && (optp->length == length) &&
		    (optp->negotiate == negotiate) &&
		    (bcmp(data, (caddr_t)optp->data, length - 2) == 0))
			return (0);
	}

	op = (pppOption_t *)kmem_zalloc(sizeof (pppOption_t), KM_NOSLEEP);

	PPP_MEMDBG2("add_opt: allocated addr=%p,  type = %x\n", op, type);

	if (op == NULL) {
		return (-1);
	}

	op->type = (uchar_t)type;
	op->length = (uchar_t)length;
	op->negotiate = negotiate;
	op->next = NULL;

	if (length != 0) { /* length == 0 implies option not supported */

/*
 * must use bcopy because of potential (non)-alignment
 * problems
 */

/*
 * deduct 2 bytes for the type/length fields
 */

		length -= 2;

		if (length && data != FALSE) {
			bcopy(data, (caddr_t)op->data, length);
		}

	}

/*
 * insert this option in the *front* of the list
 */
	op->next = optptr[type];
	optptr[type] = op;
	return (0);
}


/*
 * free_opts()
 *
 * free a list of options.
 */
void
free_opts(pppOption_t *optptr[], int optsz)
{
	pppOption_t	*op, *nop = NULL;
	int		i;

	PPP_MEMDBG2("free_opt: optptr = %p,  optsz = %x\n", optptr, optsz);
	for (i = 0; i < optsz; i++) {

		for (op = optptr[i]; op != NULL; op = nop) {
			nop = op->next;
			PPP_MEMDBG2("free_opt: freeing %p,  type = %x\n",
			    op, i);
			kmem_free(op, sizeof (pppOption_t));
		}

		optptr[i] = NULL;
	}
}


/*
 * get_opt()
 *
 * parse a PPP packet for LCP/NCP options
 *
 * Returns: pointer to the next option on success op->length > 0
 * zero length option (op->length = 0) on failure.  mp->b_rptr is
 * advanced to the next option.
 */
static void
get_opt(mblk_t *mp, pppOption_t *op)
{
	pppOption_t	*in_op;
	int		remain;

	op->length = 0;

/*
 * check to see if we have at least enough packet left
 * to have a minimum-length option (type + length = 2 octets)
 *
 * If not, then we are done with the packet
 */
	remain = (int)(mp->b_wptr - mp->b_rptr);

	if (remain < 2) {
		return;
	}

	in_op = (pppOption_t *)mp->b_rptr;

/*
 * check to see that the option length pointer does not tell lies
 */
	if ((int)in_op->length > remain) {
		return;
	}

	op->type = in_op->type;
	op->length = in_op->length;
	op->flags = 0;
	bcopy((caddr_t)in_op->data, (caddr_t)op->data, in_op->length);

	PPP_OPTDBG2("get_opt: type=%d length=%d\n", op->type, op->length);
	PPP_OPTDBG2("get_opt: data=0x%x%x\n",
			(unsigned)op->data[0], (unsigned)op->data[1]);

	mp->b_rptr += in_op->length;
}


/*
 * apply_options()
 *
 * Put negotiated options into effect for this connection once they have
 * been acknowledged
 *
 * Returns: none.
 *
 * Side effects: machp->crp (the copy of the conf.req packet) is changed
 */
static void
apply_options(pppMachine_t *machp)
{
	pppOption_t		op;

	if (machp->crp == NULL) {
		return;
	}

/*
 * skip the frame and packet header
 */
	(void) adjmsg(machp->crp, PPP_HDRSZ);

/*
 * now parse each of the options in turn...  note we don't validate
 * the option since it came from our own conf.req packet and therefore
 * should be valid!
 */
	for (get_opt(machp->crp, &op); op.length != 0;
	    get_opt(machp->crp, &op)) {

		switch (machp->protocol) {

		case pppLCP:
			ppp_apply_lcp_option(machp, &op);
			break;
		case pppIP_NCP:
			ppp_apply_ipncp_option(machp, &op);
			break;
		}
	}
}

/*
 * PPP timer routines...
 */


/*
 * irc()
 *
 * Start the PPP restart timer for this state machine
 */
static void
irc(pppMachine_t *machp)
{
	cancel_restart_timer(machp);
	machp->restart_counter = machp->linkp->conf.pppLinkMaxRestarts;
	machp->loopback_counter = machp->linkp->conf.pppLinkMaxLoopCount;
	machp->restart = timeout(ppp_timeout, machp,
	    MSEC_TO_TICK(machp->linkp->conf.pppLinkRestartTimerValue));
	machp->conf_nak_counter = machp->linkp->conf.pppLinkMaxRestarts;
}


/*
 * zrc()
 *
 * Zero the restart counter and set timeout.  Causes timeout to be set with no
 * restart upon expiration.
 */
static void
zrc(pppMachine_t *machp)
{
	cancel_restart_timer(machp);
	machp->restart_counter = 0;
	machp->restart = timeout(ppp_timeout, machp,
	    MSEC_TO_TICK(machp->linkp->conf.pppLinkRestartTimerValue));
}


/*
 * ppp_start_restart_timer()
 *
 * Start the restart timer.
 */
void
ppp_start_restart_timer(pppMachine_t *machp)
{
	if (machp->restart) {
	    PPP_TIMDBG("start_restart_timer: on machp %p - already running\n",
							machp);
		return;
	}
	PPP_TIMDBG("start_restart_timer: on machp %p\n", machp);

/*
 * set restart timer running flag
 */
	machp->restart = timeout(ppp_timeout, machp,
	    MSEC_TO_TICK(machp->linkp->conf.pppLinkRestartTimerValue));
}


/*
 * cancel_restart_timer()
 *
 * cancel outstanding restart timer
 * (no problem if timer is not running)
 */
static void
cancel_restart_timer(pppMachine_t *machp)
{
	ASSERT(MUTEX_HELD(&machp->linkp->lplock));

	if (machp->restart == 0) {
		PPP_TIMDBG("cancel_restart_timer: not running on machp %p\n",
							machp);
		return;
	}

	PPP_TIMDBG("cancel_restart_timer: cancelling timer on machp %p\n",
							machp);
	(void) untimeout(machp->restart);
	machp->restart = 0;
}


/*
 * ppp_timeout()
 *
 * Generate a timeout event in response to timer expiration
 */
static void
ppp_timeout(void *arg)
{
	pppMachine_t *machp = arg;

	PPP_TIMDBG("ppp_timeout: on machp %p\n", machp);

	machp->timedoutid = machp->restart;

	ppp_cross_fsm(machp->linkp, PPPIN_TIMEOUT1, machp->protocol,
		CROSS_LO);
}


/*
 * free_machine()
 *
 * Free a finite state machine structure
 */
void
free_machine(pppMachine_t *machp)
{
	PPP_MEMDBG("free_machine: freeing machine %p\n", machp);

	cancel_restart_timer(machp);

	if (machp->pkt) {
		freemsg(machp->pkt);
	}

	if (machp->crp) {
		freemsg(machp->crp);
	}

	/* bug 4010300 memory leak in kmem_alloc_320 */
	free_opts(machp->outbound, PPP_MAX_OPTS);
	free_opts(machp->inbound, PPP_MAX_OPTS);

	(void) kmem_free(machp, sizeof (pppMachine_t));
}


/*
 * is_equal_to()
 *
 * Returns: 1 if parameter 1 options equal parameter 2.
 * Otherwise 0 is returned
 *
 * Equality is equivalent to identical so we'll do bcmp
 */
static int
is_equal_to(mblk_t *set1, mblk_t *set2)
{
	size_t cnt;

	if ((cnt = msgdsize(set1)) != msgdsize(set2)) {
		return (0);
	}

	if (bcmp((caddr_t)(set1->b_rptr + sizeof (struct ppp_hdr)),
	    (caddr_t)(set2->b_rptr + sizeof (struct ppp_hdr)),
	    cnt - sizeof (struct ppp_hdr)) != 0) {
		return (0);
	}

	return (1);
}

/*
 * is_contained_in()
 *
 * Returns: 1 if parameter 2 is a proper options subset of parameter 1.
 * Otherwise, the function returns 0
 */
/* ARGSUSED */
static int
is_contained_in(mblk_t *set, mblk_t *subset)
{
	uchar_t *optptr, *optptrsub;
	struct ppp_opthdr *xhdr, *yhdr;

	optptr = set->b_rptr + sizeof (struct ppp_hdr);
	optptrsub = subset->b_rptr + sizeof (struct ppp_hdr);

	yhdr = (struct ppp_opthdr *)optptr;
	xhdr = (struct ppp_opthdr *)optptrsub;

	while (optptr < set->b_wptr && optptrsub < subset->b_wptr) {
		if (xhdr->length == yhdr->length) {
			if (bcmp((caddr_t)optptr, (caddr_t)optptrsub,
			    yhdr->length) == 0) {
				optptrsub += xhdr->length;
				xhdr = (struct ppp_opthdr *)optptrsub;
			}
		}
		optptr += yhdr->length;
		yhdr = (struct ppp_opthdr *)optptr;
	}
	if (optptrsub < subset->b_wptr)
		return (0);
	else
		return (1);
}


/*
 * build_pkt()
 *
 * add option op to packet pointed to by fp.   The buffer must provide
 * enough contiguous space to accept the option.
 */
static void
build_pkt(mblk_t *fp, pppOption_t *op)
{
	struct ppp_hdr		*hdr;

	ASSERT(fp);
	ASSERT(op);

	if (op->length == 0) {
		return;
	}

	hdr = (struct ppp_hdr *)fp->b_rptr;

/*
 * increase the packet length by the size of the option
 */
	hdr->pkt.length =
	    htons(ntohs(hdr->pkt.length) + op->length);

/*
 * copy the data associated with the option into the packet.
 * Need to use bcopy because of potential non-alignment
 * problems
 */
	bcopy((caddr_t)op, (caddr_t)fp->b_wptr, op->length);

/*
 * increase the message length to include the option
 */
	fp->b_wptr += op->length;
}


/*
 * statistics routines for PPP...
 */

static void
cp_pkt_in(pppMachine_t *machp, mblk_t *mp)
{
/*
 * incoming control protocol packet statistics
 */
	machp->stats.pppCPInPackets++;
	machp->stats.pppCPInOctets += (uint_t)msgdsize(mp) - PPP_FR_HDRSZ;
}

static void
cp_pkt_out(pppMachine_t *machp, mblk_t *mp)
{
/*
 * outgoing control protocol packet statistics
 */
	machp->stats.pppCPOutPackets++;
	machp->stats.pppCPOutOctets += (uint_t)msgdsize(mp) - PPP_FR_HDRSZ;
}


void
ip_pkt_in(pppMachine_t *machp, mblk_t *mp, uint_t proto)
{
/*
 * incoming IP protocol packet statistics
 */
	machp->ip.pppIPInPackets++;
	machp->ip.pppIPInOctets += (uint_t)msgdsize(mp) - PPP_FR_HDRSZ;
	switch (proto) {
		case pppIP_PROTO:
			machp->ip.pppIPInIP++;
			break;
		case pppVJ_UNCOMP_TCP:
			machp->ip.pppIPInVJuncomp++;
			break;
		case pppVJ_COMP_TCP:
			machp->ip.pppIPInVJcomp++;
			break;
	}
}

void
ip_pkt_out(pppMachine_t *machp, mblk_t *mp, uint_t proto)
{
/*
 * outgoing IP protocol packet statistics
 */
	machp->ip.pppIPOutPackets++;
	machp->ip.pppIPOutOctets += (uint_t)msgdsize(mp) - PPP_FR_HDRSZ;
	switch (proto) {
		case pppIP_PROTO:
			machp->ip.pppIPOutIP++;
			break;
		case pppVJ_UNCOMP_TCP:
			machp->ip.pppIPOutVJuncomp++;
			break;
		case pppVJ_COMP_TCP:
			machp->ip.pppIPOutVJcomp++;
			break;
	}
}
