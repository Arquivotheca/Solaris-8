/*
 * Copyright (c) 19921998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppp_str.c	1.41	98/06/12 SMI"

/*
 * A STREAMS implementation of the Point-to-Point Protocol for the
 * Transmission of Multi-Protocol Datagrams over Point-to-Point Links
 *
 * Derived from RFC1171 (July 1990) - Perkins (CMU)
 * updated to RFC1331 (May 1992) - Simpson
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
#include <sys/cmn_err.h>
#ifndef _SunOS4
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/t_lock.h>
#else
#include "4.1.h"
#endif
#include <sys/strsun.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/systeminfo.h>

#ifdef ISERE_TREE
#include <ppp/vjcomp.h>
#include <ppp/ppp_ioctl.h>
#include <ppp/ppp_sys.h>
#include <ppp/ppp_pap.h>
#include <ppp/ppp_chap.h>
#include <ppp/ppp_lqm.h>
#include <ppp/ppp_extern.h>
#else
#include <sys/vjcomp.h>
#include <sys/ppp_ioctl.h>
#include <sys/ppp_sys.h>
#include <sys/ppp_pap.h>
#include <sys/ppp_chap.h>
#include <sys/ppp_lqm.h>
#include <sys/ppp_extern.h>
#endif /* ISERE_TREE */

static uint_t randx;

static int ppp_rw(queue_t *, mblk_t *);
static int ppp_rsrv(queue_t *);
static int ppp_wsrv(queue_t *);
static mblk_t *unpack_ppp(queue_t *, mblk_t *);
static mblk_t *ppp_getframe(queue_t *);
static int ppp_open(queue_t *q, dev_t, int, int, cred_t *);
static int ppp_close(queue_t *, int, cred_t *);
static int alloc_pib(pppLink_t *);
static void attach_hdr(mblk_t **mp, pppProtocol_t protocol);
static void dev_tld(pppLink_t *);
static void device_external_event(pppLink_t *, uint_t);
static void external_event(pppLink_t *, uint_t, pppProtocol_t);
static void ppp_disc_ind(pppLink_t *);
static void dev_tld(pppLink_t *);
static void memory_retry(void *);
static pppLink_t *alloc_link(queue_t *q);
static void free_link(pppLink_t *lp);
static void reset_options(pppLink_t	*);
static pppLinkControlEntry_t *extract_isdn_conf(mblk_t *);
static int fill_isdn_conf(mblk_t *, pppLinkControlEntry_t *);
static int set_conf(pppLink_t *, pppLinkControlEntry_t *);
static int set_conf_ioctl(pppLink_t *, mblk_t *);
static int get_conf(pppLink_t *, pppLinkControlEntry_t *);
static int get_conf_ioctl(pppLink_t *, mblk_t *);
static void *check_param(mblk_t *mp, size_t size);


#ifdef PPP_DEBUG
#define	STR_PRINT(X, Y) (str_print(X, Y))
#else
#define	STR_PRINT(X, Y)
#endif

#define	ABS(X)	(X >= 0 ? X : (-X))

/*
 *
 * Important FCS values.
 */
#define	PPP_INITFCS		0xffff	/* Initial FCS value */
#define	PPP_GOODFCS		0xf0b8	/* Good final FCS value */
#define	PPP_FCS(fcs, c)		(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

/*
 *
 * FCS lookup table as calculated by genfcstab.
 */
static ushort_t fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
 * Module identification for PPP
 */
char ppp_module_name[] = "async/sync ppp v1.41";


/*
 * PPP management information base, protected by a readers/writer lock
 */

static pppLink_t	*ppp_mib[PPP_MAX_MIB+1];	/* don't use 0 */
static krwlock_t	miblock;


/*
 * patchable debug flag
 */

#ifdef PPP_DEBUG
int			ppp_debug = PPP_STREAMS | PPP_ERRORS | PPP_STATE;
#else
int			ppp_debug = PPP_ERRORS;
#endif

/*
 * housekeeping variables
 */
int			ppp_busy = 0;
static int		ppp_initialised = 0;

/*
 * record of how many memory outages have taken place */
static int ppp_memory_denied = 0;

/*
 * standard STREAMS declarations for this module */

static struct module_info minfo = { 0x4142, "ppp", 0, INFPSZ, 32768, 512 };

static struct qinit rinit = {
	ppp_rw, ppp_rsrv, ppp_open, ppp_close, NULL, &minfo, NULL };

static struct qinit winit = {
	ppp_rw, ppp_wsrv, NULL, NULL, NULL, &minfo, NULL };


struct streamtab ppp_info = {	&rinit,		/* *st_rdinit	*/
				&winit,		/* *st_wrinit	*/
				NULL,		/* *st_muxrinit */
				NULL,		/* *st_muxwinit */
#ifdef _SunOS4
				NULL		/* **st_modlist */
#endif
};

/*
 * ppp_initialise()
 *
 * Main PPP initialisation routine called by the loadable module wrapper
 *
 * Returns: 0 on success, error code otherwise
 */
int
ppp_initialise(void)
{
	int	error = 0;

	if (ppp_initialised) {
		return (0);
	}

	/* initialise PPP MIB to be empty */

	bzero((char *)ppp_mib, sizeof (ppp_mib));
	rw_init(&miblock, NULL, RW_DEFAULT, NULL);

	error = ppp_lcp_initialize() |

	ppp_ipncp_initialize();

	if (error) {
		return (ENOMEM);
	}

	ppp_randomize();

	ppp_initialised++;
	return (0);
}

/*
 * ppp_terminate()
 *
 * Shutdown PPP prior to module unload operation
 *
 * Returns: 0 on success, panic otherwise
 */
void
ppp_terminate(void)
{
	if (!ppp_initialised) {
		return;
	}

	if (ppp_busy) {
		panic("PPP: attempt to terminate PPP when active\n");
	}

	rw_destroy(&miblock);
}

/*
 * ppp_open()
 *
 * STREAMS open module for the PPP service
 *
 * Returns: 0 on success, OPENFAIL on error (XXX must fix this for jupiter)
 */

/*ARGSUSED*/
static int
#ifndef _SunOS4
ppp_open(queue_t *q, dev_t devp, int flag, int sflag, cred_t *credp)
#else
ppp_open(queue_t *q, dev_t devp, int flag, int sflag)
#endif
{

	PPP_STRDBG("ppp_open: opening rq=%x\n", q);

	if (!ppp_initialised) {
		return (OPENFAIL);
	}

	if (sflag != MODOPEN) {		/* can only be opened as a module */
		return (OPENFAIL);
	}

	if (q->q_ptr) {			/* already opened? */
		return (0);
	}

/*
 * get hold a structure which record the details of this link,
 * information such as the LCP state, and the network state
 */

	if (alloc_link(q) == NULL) {

/*
 * a problem, probably memory shortage allocating the
 * structure to hold information for this link.	 Bounce
 * the open()
 */
		return (OPENFAIL);
	}

	qprocson(q);

	ppp_busy++;	/* XXX */
	return (0);
}



/*
 * ppp_rw()
 *
 * PPP common read/write side put function.
 *
 * Returns: 0
 */
static int
ppp_rw(queue_t *q, mblk_t *mp)
{
	pppLink_t	*lp;

/*
 * pass on all high priority messages at once (with the exception
 * of M_FLUSH), queue all the rest
 */

	if (MTYPE(mp) == M_HANGUP || MTYPE(mp) == M_BREAK) {
/* Alert lm immediately */

		if (MTYPE(mp) == M_BREAK) {
			return (0);
		}

		lp = (pppLink_t *)q->q_ptr;
		lp->device_is_up = 0;
		dev_tld(lp);

	} else {
		if (MTYPE(mp) >= QPCTL && MTYPE(mp) != M_FLUSH) {
			putnext(q, mp);
		} else {
			(void) putq(q, mp);
		}
	}
	qenable(q);

	return (0);
}

/*
 * ppp_close()
 *
 * PPP close function
 *
 * Returns: 0
 */
/*ARGSUSED*/
static int
#ifndef _SunOS4
ppp_close(queue_t *q, int flag, cred_t *credp)
#else
ppp_close(queue_t *q, int flag)
#endif
{
	pppLink_t	*lp = (pppLink_t *)q->q_ptr;

	PPP_STRDBG("ppp_close: closing stream %x\n", q);

	ASSERT(lp);

	qprocsoff(q);

/*
 * Do a link layer close to terminate link...  Done directly so as to
 * bypass event queue.	Should just force a terminate req packet
 */

	external_event(lp, PPPIN_CLOSE, pppLCP);

	mutex_enter(&lp->lplock);

	/* release resources and locks...  */

	free_link(lp);

	ppp_busy--;
	return (0);
}


/*
 * ppp_wsrv()
 *
 * PPP write side service function
 *
 * Returns 0
 */
static int
ppp_wsrv(queue_t *q)
{
	mblk_t		*mp, *zmp;
	pppLink_t	*lp;
	uint_t		pck_type;
	int		vj_type;


	ASSERT(q->q_ptr != NULL);

/* get a handle on the link */

	lp = (pppLink_t *)q->q_ptr;

	while ((mp = getq(q)) != NULL) {

		PPP_STRDBG("ppp_wsrv: getq on q=%x\n", q); STR_PRINT(q, mp);

		switch (MTYPE(mp)) {

		case M_DATA:

		mutex_enter(&lp->lplock);

		if (!lp->link_up || !canputnext(q)) {
			mutex_exit(&lp->lplock);
			PPP_STRDBG("ppp_wsrv: q=%x lower blocked: re-q\n", q);
#ifdef PPP_DEBUG
			if (!canputnext(q))
				PPP_STRDBG("ppp_wsrvi, canputnext failed %d\n",
					(int)canputnext(q));
#endif
			(void) putbq(q, mp);
			return (0);
		}

/*
 * link is established - send the data
 * and go
 */
		if (!ISPULLEDUP(mp)) {
			zmp = msgpullup(mp, -1);
			freemsg(mp);
			mp = zmp;
			if (mp == NULL) {
				/* bugid 1225317: sgypsy@eng */
				mutex_exit(&lp->lplock);
				return (0);
			}
		}

		pck_type = lp->ncp->ntype;
		if (lp->conf.pppIPcompSend == VJCOMP &&
		    pck_type == pppIP_PROTO) {
			vj_type = vjcompress_tcp(&mp, &lp->vjstruct);
			switch (vj_type) {
				case TYPE_IP:
					pck_type = pppIP_PROTO;
					break;
				case TYPE_UNCOMPRESSED_TCP:
					pck_type = pppVJ_UNCOMP_TCP;
					break;
				case TYPE_COMPRESSED_TCP:
					pck_type = pppVJ_COMP_TCP;
					break;
				case TYPE_BAD_TCP:
					freemsg(mp);
					/* bugid 1225317: sgypsy@eng */
					mutex_exit(&lp->lplock);
					continue;
				default:
					/* bugid 1225317: sgypsy@eng */
					mutex_exit(&lp->lplock);
					return (0);
			}
		}
		attach_hdr(&mp, pck_type);
		if (mp == NULL) {
			/* bugid 1225317: sgypsy@eng */
			mutex_exit(&lp->lplock);
			return (0);
		}

		ip_pkt_out(lp->ncp, mp, pck_type);
		mutex_exit(&lp->lplock);
		PPP_STRDBG("ppp_wsrv: sending q=%x\n", q);

#ifdef PPP_DEBUG
		if (lp->conf.pppLinkMediaType == pppAsync)
			PPP_STRDBG("Async line, need to un-quote \
			    chars %d\n", 0);
#endif

		STR_PRINT(q, mp);

		ppp_putnext(q, mp);
		continue;


		/* standard module flush handling...  */

		case M_FLUSH:

		/* Need to reset Pib if Async */

		if (*mp->b_rptr & FLUSHW) {
			flushq(q, FLUSHDATA);
		}

		putnext(q, mp);
		continue;

		/* process ioctls from the user...  */

		case M_IOCTL:

		ppp_ioctl(q, mp);
		continue;

		default:

		putnext(q, mp);
		continue;

		} /* switch */
	} /* while */

	return (0);
}




/*
 * ppp_rsrv()
 *
 * PPP read side service function
 *
 * Returns 0
 */
static int
ppp_rsrv(queue_t *q)
{
	mblk_t			*mp;
	pppLink_t		*lp;
	struct ppp_frame_hdr	*hp;
	ushort_t			protocol;
	ExProtoEvent_t		*exproto;
	int			rc;

	ASSERT(q->q_ptr != NULL);

	/* get a handle on the link */

	lp = (pppLink_t *)q->q_ptr;

	while ((mp = ppp_getframe(q)) != NULL) {

	    switch (MTYPE(mp)) {

	case M_DATA:
#ifdef PPP_DEBUG
		if (lp->conf.pppLinkMediaType != pppAsync) {
			PPP_STRDBG2(
			    "ppp_rsrv: incoming M_DATA %d bytes on q=%x\n",
			    msgdsize(mp), q);
		}
#endif

		mutex_enter(&lp->lplock);

		if (!canputnext(q)) {

			/* upper stream is blocked, requeue packet */

			mutex_exit(&lp->lplock);
			(void) putbq(q, mp);
			return (0);
		}

		hp = (struct ppp_frame_hdr *)mp->b_rptr;

		protocol = ntohs(hp->prot);
		switch (protocol) {

		case pppIP_PROTO:
		case pppVJ_COMP_TCP:
		case pppVJ_UNCOMP_TCP:

			/* If the link is operational deliver the packet */

			if (lp->link_up) {
				(void) adjmsg(mp, PPP_FR_HDRSZ);
				switch (protocol) {
				case pppVJ_COMP_TCP:
					rc = vjuncompress_tcp(&mp,
					    TYPE_COMPRESSED_TCP,
					    &lp->vjstruct);
					if (rc != 0) {
					    PPP_STRDBG("Uncompress failed\n",
						"");
					    if (mp)
						freemsg(mp);
					    mutex_exit(&lp->lplock);

					    continue;
					}
					break;
				case pppVJ_UNCOMP_TCP:
					rc = vjuncompress_tcp(&mp,
					    TYPE_UNCOMPRESSED_TCP,
					    &lp->vjstruct);
					if (rc != 0) {
					    PPP_STRDBG("Uncompress failed\n",
						"");
					    if (mp)
						freemsg(mp);
					    mutex_exit(&lp->lplock);
					    continue;
					}
					break;
				case pppIP_PROTO:
					rc = 0;
					break;
				}

				/*
				 * esc 504072 sgypsy, vjuncompress_tcp may
				 * have tossed the message.
				 */
				if (mp) {
				    if (msgdsize(mp) >
					(int)lp->conf.pppLinkLocalMRU) {
					lp->errs.pppLinkPacketTooLongs++;
				    }

				    ip_pkt_in(lp->ncp, mp, protocol);
				    PPP_STRDBG(
					"Sending up packet, size = %d\n",
					msgdsize(mp));
				    mutex_exit(&lp->lplock);
				    putnext(q, mp);
				} else {
				    mutex_exit(&lp->lplock);
				}
				continue;
			}

/*
 * IP is not ready to accept data - discard the message
 * silently [RFC1331 page 2]
 */
			PPP_STRDBG("Rejected, ncp not up, size= %d\n",
			    msgdsize(mp));
			lp->ncp->ip.pppIPRejects++;
/*
 * this isn't really a
 * IP reject...
 */
			mutex_exit(&lp->lplock);
			freemsg(mp);
			continue;

		case pppIP_NCP:

/*
 * network control protocol events are generated here
 * and fed into the state machine
 */
			do_incoming_cp(lp->ncp, mp);
			mutex_exit(&lp->lplock);
			continue;

		case pppLCP:

/*
 * link control protocol events are generated here
 * and fed into the state machine
 */

			do_incoming_cp(lp->lcp, mp);

			mutex_exit(&lp->lplock);
			continue;


		case pppAuthPAP:

/*
 * Password Authentication Protocol events are generated
 * here
 */
			do_incoming_pap(lp->pap, mp);
			mutex_exit(&lp->lplock);
			continue;

		case pppCHAP:
			do_incoming_chap(lp->chap, mp);
			mutex_exit(&lp->lplock);
			continue;

		case pppLQM_REPORT:
			do_incoming_lqm(lp->lqm, mp);
			mutex_exit(&lp->lplock);
			continue;

		default:

/*
 * invalid or unknown protocol received.  Generate a
 * protocol reject if we are in the LCP open state.
 * Otherwise drop the frame silently [RFC1331]
 */

			if (is_open(lp->lcp)) {
				send_protocol_reject(lp, WR(q), mp);
			}

/*
 * a protocol is invalid if either :
 *
 * 1) least significant bit of most significant
 *    octet is non-zero, or
 *
 * 2) the protocol is even
			 */
			if (protocol & 0x0100 || ((protocol & 0x0001) == 0)) {

				lp->errs.pppLinkInvalidProtocols++;
				lp->errs.pppLinkLastInvalidProtocol = protocol;

			} else
				lp->errs.pppLinkLastUnknownProtocol = protocol;

		} /* M_DATA protocol switch */

		mutex_exit(&lp->lplock);
		continue;

	case M_FLUSH:

		/* standard flush handling */

		if (*mp->b_rptr & FLUSHR) {
			flushq(q, FLUSHDATA);
		}
		putnext(q, mp);
		continue;

	case M_PCPROTO:
	case M_PROTO:
		exproto = (ExProtoEvent_t *)mp->b_rptr;
		external_event(lp, exproto->event, exproto->
		    protocol);
		freemsg(mp);
		break;

	default:
		PPP_STRDBG("ppp_rsrv: dropping message of type %x\n",
		    MTYPE(mp));
		freemsg(mp);
		continue;
	} /* switch */
	} /* while */
	return (0);
}

/*
 * ppp_disc_ind()
 *
 * Tell ipdcm the user that a PPP wants to talk to link manager.
 * Assumes that the link manager has a handle to the stream which
 * is determined by whether or not the NCP layer is open.
 *
 * Returns: none, sends M_ERROR on failure.  If non-NULL errdata is
 * consumed & freed.
 */
static void
ppp_disc_ind(pppLink_t	 *lp)
{
	mblk_t			*mp;
	dl_disconnect_ind_t	*dldisind;

	ASSERT(!lp->link_up);

	mp = allocb(sizeof (*dldisind), BPRI_HI);
	if (mp == NULL) {
		return;
	}

	MTYPE(mp) = M_PROTO;

	mp->b_wptr += sizeof (*dldisind);
	dldisind = (dl_disconnect_ind_t *)mp->b_rptr;
	dldisind->dl_primitive = DL_DISCONNECT_IND;
	dldisind->dl_reason = DL_DISC_NORMAL_CONDITION;

	mp->b_band = QPCTL;
	/* Set highest priority so that message gets delivered */
	putnext(lp->readq, mp);
}

/*
 * external_event()
 *
 * Dispatch an external event to the proper handler.
 */
void
external_event(pppLink_t *lp, uint_t event, pppProtocol_t protocol)
{
	switch (protocol) {
	case pppLCP:
		lcp_external_event(lp, event);
		break;
	case pppIP_NCP:
		ncp_external_event(lp, event);
		break;
	case pppAuthPAP:
		pap_external_event(lp, event);
		break;
	case pppCHAP:
		chap_external_event(lp, event);
		break;
	case pppLQM_REPORT:
		lqm_external_event(lp, event);
		break;
	case pppDEVICE:
		device_external_event(lp, event);
		break;
	}
}

/*
 * dev_tld()
 *
 * Processes a this-layer-down for the (virtual) device layer
 */
static void
dev_tld(pppLink_t *lp)
{
	ppp_internal_event(lp, PPP_TL_DOWN, pppDEVICE);
}

/*
 * ppp_error_ind()
 *
 * Tell the user that a PPP wants to signal an error.  Assumes that
 * the link manager has a handle to the read stream.  We can only
 * send error messages when this is true which coincides when the
 * ncp layer is in the open state.
 *
 * Returns: none, sends M_ERROR on failure.  If non-NULL errdata is
 * consumed & freed.
 */
void
ppp_error_ind(pppLink_t *lp, enum ppp_errors error, uchar_t *errdata,
		uint_t error_cnt)
{
	mblk_t			*mp;
	pppError_t		*msg;

	ASSERT(!lp->link_up);

	PPP_APIDBG2("ppp_error_ind: q=%x, error=%d\n", lp->readq, error);

	mp = allocb(sizeof (*msg), BPRI_HI);
	if (mp == NULL) {
		return;
	}
	switch (lp->conf.pppLinkVersion) {
	case pppVer1:
		switch (error) {
		case pppLocalAuthFailed:
		case pppRemoteAuthFailed:
			error = pppAuthFailed;
			break;
		case pppLoopedBack:
			error = pppConfigFailed;
			break;
		default:
			break;
		}
		break;
	case pppVer2:
		break;
	}


	MTYPE(mp) = M_PROTO;	/* Make protocol messages high priority */

	if (error_cnt > PPP_MAX_ERROR)
		error_cnt = PPP_MAX_ERROR;
	mp->b_wptr += sizeof (*msg);
	msg = (pppError_t *)mp->b_rptr;
	msg->ppp_message	= PPP_ERROR_IND;
	msg->code		= error;
	msg->errlen		= error_cnt;
	bcopy((caddr_t)errdata, (caddr_t)msg->errdata, error_cnt);

	putnext(lp->readq, mp);
}

/*
 * device_external_event()
 *
 * Handler for device events.  Either an "up" event indicating the device
 * is connected, or "down" indicating the device is unusable.
 */
static void
device_external_event(pppLink_t *lp, uint_t event)
{
	uint_t	tlevent;

	switch (event) {
	case PPPIN_UP:
		lp->device_is_up = 1;
		tlevent = PPP_TL_UP;
		break;
	case PPPIN_DOWN:
		lp->device_is_up = 0;
		tlevent = PPP_TL_UP;
		break;
	}
	ppp_internal_event(lp, tlevent, pppDEVICE);
}

/*
 * ppp_cross_fsm()
 *
 * Send a protocol message from one fsm to another.
 */
void
ppp_cross_fsm(pppLink_t *linkp, uint_t event, pppProtocol_t protocol,
		uint_t pri)
{
	mblk_t		*mp;
	ExProtoEvent_t	*exproto;

	mp = allocb(sizeof (*exproto), BPRI_HI);
	if (mp == NULL) {
		return;
	}

	if (pri == CROSS_HI)
		MTYPE(mp) = M_PCPROTO;
	else
		MTYPE(mp) = M_PROTO;

	mp->b_wptr += sizeof (*exproto);

	exproto = (ExProtoEvent_t *)mp->b_rptr;
	exproto->event = event;
	exproto->protocol = protocol;

	(void) putq(linkp->readq, mp);
}

/*
 * ppp_notify_lm()
 *
 * Sends an M_PROTO message up the stream to inform the link manager of
 * an fsm's actions
 */
void
ppp_notify_lm(pppLink_t *lp, uint_t event, pppProtocol_t protocol, caddr_t data,
		int datalen)
{
	mblk_t *mp;
	pppProtoCh_t *msg;

	mp = allocb(sizeof (*msg) + datalen, BPRI_HI);
	if (mp == NULL) {
		return;
	}

	MTYPE(mp) = M_PROTO;	/* Make protocol messages high priority */

	mp->b_wptr += sizeof (*msg) + datalen;
	msg = (pppProtoCh_t *)mp->b_rptr;
	msg->ppp_message = event;
	msg->protocol = protocol;
	bcopy(data, (caddr_t)msg->data, datalen);

	putnext(lp->readq, mp);
}

/*
 * ppp_reneg()
 *
 * Forces ppp renegotiation of the link
 */
void
ppp_reneg(pppLink_t *linkp)
{
	ppp_cross_fsm(linkp, PPPIN_DOWN, pppLCP, CROSS_HI);
	ppp_cross_fsm(linkp, PPPIN_UP, pppLCP, CROSS_HI);
}

/*
 * reset_options()
 *
 * Do a get_conf and then a set_conf to set up initial options
 */
void
reset_options(pppLink_t *linkp)
{
	pppLinkControlEntry_t	aconf;

	if (get_conf(linkp, &aconf) < 0)
		return;
	(void) set_conf(linkp, &aconf);
}

/*
 * ppp_internal_event()
 *
 * Contains the inter fsm logic.  Here is where the interaction between
 * protocol layers is handled
 */
void
ppp_internal_event(pppLink_t *linkp, uint_t message, pppProtocol_t protocol)
{
	int		changeauth;

	switch (message) {
	case PPP_TL_UP:

		/* if IP NCP opens, then start IP on this interface */

		if (linkp->conf.pppLinkVersion != pppVer1)
			reset_options(linkp);

		switch (protocol) {

		case pppDEVICE:
			PPP_FSMDBG0("Up: DEVICE\n");
			ppp_notify_lm(linkp, PPP_TL_UP, pppDEVICE, NULL, 0);
			if (linkp->conf.pppLinkVersion != pppVer1)
				ppp_cross_fsm(linkp,
				    PPPIN_UP, pppLCP, CROSS_HI);
			break;

		case pppLCP:
			PPP_FSMDBG0("Up: LCP\n");
			if (linkp->conf.pppLinkVersion != pppVer1) {
				ppp_notify_config_change(linkp->lcp);
				ppp_notify_lm(linkp,
				    PPP_TL_UP, pppLCP, NULL, 0);
			}
			if (linkp->outLQM) {
				ppp_cross_fsm(linkp, PPPIN_UP,
				    linkp->outLQM, CROSS_HI);
			}
			if (linkp->remauth == linkp->locauth) {
				if (linkp->remauth == 0) {
					linkp->remauthok =
					    linkp->locauthok = 1;
					if (linkp->conf.pppLinkVersion !=
									pppVer1)
						ppp_cross_fsm(linkp, PPPIN_UP,
						    pppIP_NCP, CROSS_HI);
					else {
						ppp_notify_config_change(
						    linkp->lcp);
						ppp_notify_lm(linkp,
						    PPP_TL_UP, pppLCP, NULL, 0);
					}
				} else {
					ppp_cross_fsm(linkp, PPPIN_AUTH_BOTH,
					    linkp->remauth, CROSS_HI);
				}
			} else {
				if (linkp->remauth) {
					ppp_cross_fsm(linkp, PPPIN_AUTH_REM,
					    linkp->remauth, CROSS_HI);
				} else {
					linkp->remauthok = 1;
				}
				if (linkp->locauth) {
					ppp_cross_fsm(linkp, PPPIN_AUTH_LOC,
					    linkp->locauth, CROSS_HI);
				} else {
					linkp->locauthok = 1;
				}
			}

			break;

		case pppIP_NCP:
			PPP_FSMDBG0("Up: NCP\n");
			linkp->link_up = 1;
			ppp_notify_config_change(linkp->ncp);
			ppp_notify_lm(linkp, PPP_TL_UP, pppIP_NCP, NULL, 0);
			break;

		}
		return;

	case PPP_TL_DOWN:

/*
 * We send a disconnect indication when we are in the open state of
 * the NCP layer and we get a down event on any layer in the protocol
 * stack.  It's implied that we only do notify_lm and err_ind when
 * we are in the disconnected state.
 */
		if (linkp->link_up) {
			linkp->link_up = 0;
			ppp_disc_ind(linkp);
		}
		switch (protocol) {

		case pppDEVICE:
			PPP_FSMDBG0("Down: DEVICE\n");
			ppp_notify_lm(linkp, PPP_TL_DOWN, pppDEVICE, NULL, 0);
			ppp_cross_fsm(linkp, PPPIN_DOWN, pppLCP, CROSS_HI);
			break;

		case pppLCP:
			PPP_FSMDBG0("Down: LCP\n");
			linkp->remauthok = linkp->locauthok = 0;
			if (linkp->outLQM) {
				ppp_cross_fsm(linkp, PPPIN_DOWN,
					linkp->outLQM, CROSS_HI);
			}
			if (linkp->locauth) {
				ppp_cross_fsm(linkp, PPPIN_CLOSE,
					linkp->locauth, CROSS_HI);
			}
			if (linkp->remauth) {
				ppp_cross_fsm(linkp, PPPIN_CLOSE,
					linkp->remauth, CROSS_HI);
			}
			ppp_notify_lm(linkp, PPP_TL_DOWN, pppLCP, NULL, 0);
			if (linkp->conf.pppLinkVersion != pppVer1)
				ppp_cross_fsm(linkp,
				    PPPIN_DOWN, pppIP_NCP, CROSS_HI);
			break;

		case pppIP_NCP:
			PPP_FSMDBG0("Down: NCP\n");
/*
 * Reset the VJ compression.  This guarantees that if the link is
 * renegotiated, vj compression has been reset.
 */
			vjcompress_init(&linkp->vjstruct);
			ppp_notify_lm(linkp, PPP_TL_DOWN, pppIP_NCP, NULL, 0);
			break;

		}
		return;

	case PPP_TL_START:
		switch (protocol) {
		case pppLCP:
			PPP_FSMDBG0("Starting: LCP\n");
			ppp_notify_lm(linkp, PPP_TL_START, pppLCP, NULL, 0);
			linkp->remauthok = linkp->locauthok = 0;
			if (linkp->device_is_up) {
				PPP_FSMDBG0("Start with device up.\n");
				if (linkp->conf.pppLinkVersion != pppVer1)
					ppp_cross_fsm(linkp, PPPIN_UP,
					    pppLCP, CROSS_HI);
			}
			break;

		case pppIP_NCP:
			PPP_FSMDBG0("Starting: NCP\n");
			ppp_notify_lm(linkp, PPP_TL_START, pppIP_NCP, NULL, 0);
			if (linkp->conf.pppLinkVersion != pppVer1)
			    ppp_cross_fsm(linkp, PPPIN_OPEN, pppLCP, CROSS_HI);
			break;

		}
		return;

	case PPP_TL_FINISH:

		switch (protocol) {

		case pppLCP:
			PPP_FSMDBG0("Finished: LCP\n");
			ppp_notify_lm(linkp, PPP_TL_FINISH, pppLCP, NULL, 0);
			break;

		case pppIP_NCP:
			PPP_FSMDBG0("Finished: NCP\n");
			ppp_notify_lm(linkp, PPP_TL_FINISH, pppIP_NCP,
			    NULL, 0);
			if (linkp->conf.pppLinkVersion != pppVer1)
			    ppp_cross_fsm(linkp, PPPIN_CLOSE, pppLCP, CROSS_HI);
			break;

		}
		return;

	case PPP_AUTH_SUCCESS:
		changeauth = 0;
		if (protocol == linkp->remauth && !linkp->remauthok) {
			changeauth = 1;
			linkp->remauthok = 1;
		}
		if (protocol == linkp->locauth && !linkp->locauthok) {
			changeauth = 1;
			linkp->locauthok = 1;
		}
		if (changeauth && linkp->remauthok && linkp->locauthok) {
			if (linkp->conf.pppLinkVersion != pppVer1)
			    ppp_cross_fsm(linkp, PPPIN_UP, pppIP_NCP, CROSS_HI);
			else  {
				ppp_notify_config_change(linkp->lcp);
				ppp_notify_lm(linkp, PPP_TL_UP, pppLCP,
				    NULL, 0);
			}
		}
		return;

	case PPP_REMOTE_FAILURE:
	case PPP_LOCAL_FAILURE:
		ppp_cross_fsm(linkp, PPPIN_CLOSE, pppLCP, CROSS_HI);
		return;

	}

}

/*
 * void ppp_notify_config_change()
 *
 * Notify link manager of a change in LCP/NCP configuration
 * Sent when NCP, or LCP reach open states
 */
void
ppp_notify_config_change(pppMachine_t *machp)
{
	mblk_t		*mp;
	pppConfig_t	*msg;

	mp = allocb(sizeof (*msg), BPRI_HI);
	if (mp == NULL) {
		return;
	}

	MTYPE(mp) = M_PROTO;
	mp->b_wptr += sizeof (*msg);

	msg = (pppConfig_t *)mp->b_rptr;
	msg->ppp_message = PPP_CONFIG_CHANGED;

	putnext(machp->readq, mp);
}

/*
 * check_param()
 *
 * Check a parameter passed to a ppp ioctl() for validity.
 *
 * Should be called with miblock held.
 *
 * Returns: NULL if parameter was invalid, or
 *	    pointer to parameter if it was valid.
 *
 * Side effects: error code set in iocp->ioc_error if parameter was invalid
 */
static void *
check_param(mblk_t *mp, size_t size)
{
	ushort_t		index;
	struct iocblk	*iocp;

	ASSERT(size != 0);

	/* check parameter size */

	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != size) {
		return (NULL);
	}

	index = * (ushort_t *)mp->b_cont->b_rptr;

	/* check link selector is valid */

	if (index > PPP_MAX_MIB) {
		iocp->ioc_error = ERANGE;
		return (NULL);
	}
	if (index != 0 && ppp_mib[index] == NULL) {
		iocp->ioc_error = ENODEV;
		return (NULL);
	}

	return ((void *)mp->b_cont->b_rptr);
}



/*
 * ppp_ioctl()
 *
 * process PPP ioctls
 *
 * Returns: reply message sent upstream if ioctl was recognised
 */
void
ppp_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk		*iocp;
	pppLinkStatusEntry_t	*state;
	pppCPEntry_t		*cpstats;
	pppIPEntry_t		*ipstats;
	pppLinkErrorsEntry_t	*errs;
	int			rc;
	pppLink_t		*lp;
	uint_t			mibent;
	pppExEvent_t		*event_data;
	pppProtocol_t		protocol;
	uint_t			nevent;
	pppAuthControlEntry_t	*aconf;


	iocp = (struct iocblk *)mp->b_rptr;
	lp = (pppLink_t *)q->q_ptr;

	switch (iocp->ioc_cmd) {

/*
 * set PPP link configuration.	Includes switches to allow or disallow
 * negotiaition (both sending and receiving), requested options are
 * set up from input.
 */
	case PPP_SET_AUTH:

		rw_enter(&miblock, RW_READER);
		aconf = (pppAuthControlEntry_t *)
		    check_param(mp, sizeof (*aconf));

		if (aconf == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (aconf->pppAuthControlIndex > 0) {
			lp = ppp_mib[aconf->pppAuthControlIndex];
		}

		mutex_enter(&lp->lplock);
		rc = ppp_set_auth(lp->lcp, aconf);
		if (rc) {
			mutex_exit(&lp->lplock);
			rw_exit(&miblock);
			iocp->ioc_error = rc;
			goto iocnak;
		}
		mutex_exit(&lp->lplock);
		rw_exit(&miblock);

		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCACK;

		qreply(q, mp);
		break;

	case PPP_GET_AUTH:

		rw_enter(&miblock, RW_READER);

		aconf = (pppAuthControlEntry_t *)
		    check_param(mp, sizeof (*aconf));

		if (aconf == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (aconf->pppAuthControlIndex > 0) {
			lp = ppp_mib[aconf->pppAuthControlIndex];
		}

		aconf->pppAuthTypeLocal = lp->locauth;
		aconf->pppAuthTypeRemote = lp->remauth;

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	case PPP_SET_CONF:

		rw_enter(&miblock, RW_READER);
		rc = set_conf_ioctl(lp, mp);
		rw_exit(&miblock);

		if (rc) {
			goto iocnak;
		}

		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCACK;

		qreply(q, mp);
		break;

/*
 * get PPP link configuration
 *
 * Returns pppLinkControlEntry_t
 */
	case PPP_GET_CONF:

		rw_enter(&miblock, RW_READER);
		rc = get_conf_ioctl(lp, mp);
		rw_exit(&miblock);

		if (rc) {
			goto iocnak;
		}

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	/* State machine events generated by the link manager */

	case PPP_OPEN:
	case PPP_UP:
	case PPP_DOWN:
	case PPP_CLOSE:
		switch (iocp->ioc_cmd) {
		case PPP_UP:
			nevent = PPPIN_UP;
			break;
		case PPP_DOWN:
			nevent = PPPIN_DOWN;
			break;
		case PPP_OPEN:
			nevent = PPPIN_OPEN;
			break;
		case PPP_CLOSE:
			nevent = PPPIN_CLOSE;
			break;
		}

		if (iocp->ioc_count != sizeof (pppExEvent_t))
			goto iocnak;
		event_data = (pppExEvent_t *)mp->b_cont->b_rptr;
		ASSERT(event_data);
		protocol = event_data->protocol;
		ppp_cross_fsm(lp, nevent, protocol, CROSS_HI);
		mp->b_datap->db_type = M_IOCACK;

		qreply(q, mp);

		break;

	case PPP_GET_STATE:

		rw_enter(&miblock, RW_READER);
		state = (pppLinkStatusEntry_t *)
		    check_param(mp, sizeof (*state));

		if (state == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (state->pppLinkStatusIndex > 0) {
			lp = ppp_mib[state->pppLinkStatusIndex];
		}

		mutex_enter(&lp->lplock);
		*state = lp->state;
		mutex_exit(&lp->lplock);

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	case PPP_GET_ERRS:

		rw_enter(&miblock, RW_READER);
		errs = (pppLinkErrorsEntry_t *)
		    check_param(mp, sizeof (*errs));

		if (errs == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (errs->pppLinkErrorsIndex > 0) {
			lp = ppp_mib[errs->pppLinkErrorsIndex];
		}

		mutex_enter(&lp->lplock);
		*errs = lp->errs;
		mutex_exit(&lp->lplock);

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	case PPP_GET_IPNCP_STATS:
	case PPP_GET_LCP_STATS:

		rw_enter(&miblock, RW_READER);
		cpstats = (pppCPEntry_t *)check_param(mp, sizeof (*cpstats));

		if (cpstats == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (cpstats->pppCPLinkNumber > 0) {
			lp = ppp_mib[cpstats->pppCPLinkNumber];
		}

		mutex_enter(&lp->lplock);

		if (iocp->ioc_cmd == PPP_GET_LCP_STATS) {
			*cpstats = lp->lcp->stats;
		}
		else
			*cpstats = lp->ncp->stats;

		mutex_exit(&lp->lplock);

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	case PPP_GET_IP_STATS:

		rw_enter(&miblock, RW_READER);
		ipstats = (pppIPEntry_t *)check_param(mp, sizeof (*ipstats));

		if (ipstats == NULL) {
			rw_exit(&miblock);
			goto iocnak;
		}

		if (ipstats->pppIPLinkNumber > 0) {
			lp = ppp_mib[ipstats->pppIPLinkNumber];
		}

		mutex_enter(&lp->lplock);
		*ipstats = lp->ncp->ip;
		mutex_exit(&lp->lplock);

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		break;

	case PPP_SET_DEBUG:

		if (iocp->ioc_count != sizeof (int)) {
			goto iocnak;
		}

		ppp_debug = * (int *)mp->b_cont->b_rptr;

		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_count = 0;
		qreply(q, mp);
		break;

	/* delete PPP MIB entry.  0 implies this entry */

	case PPP_DELETE_MIB_ENTRY:

		if (iocp->ioc_count != sizeof (uint_t)) {
			goto iocnak;
		}

		mibent = * (uint_t *)mp->b_cont->b_rptr;
		if (mibent > PPP_MAX_MIB) {
			iocp->ioc_error = ERANGE;
			goto iocnak;
		}


		rw_enter(&miblock, RW_WRITER);
		if (mibent == 0) {
			for (mibent = 1; mibent < PPP_MAX_MIB; mibent++)
				if (ppp_mib[mibent] == lp) {
					ppp_mib[mibent] = NULL;
					break;
				}
		} else
			ppp_mib[mibent] = NULL;

		rw_exit(&miblock);

		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_count = 0;
		qreply(q, mp);
		break;

	case PPP_SET_LOCAL_PASSWD:
	case PPP_SET_REMOTE_PASSWD:
		if (iocp->ioc_count < sizeof (uint_t))
			goto iocnak;

		protocol = *(pppProtocol_t *)mp->b_cont->b_rptr;

		switch (protocol) {
		case pppCHAP:
			rc = chap_ioctl(lp->chap, iocp->ioc_cmd, mp);
			break;
		case pppAuthPAP:
			rc = pap_ioctl(lp->pap, iocp->ioc_cmd, mp);
			break;
/* Assume fallback to pap */
		default:
			rc = pap_ioctl(lp->pap, iocp->ioc_cmd, mp);
			break;
		}
		if (rc) {
			mp->b_datap->db_type = M_IOCNAK;
		} else {
			mp->b_datap->db_type = M_IOCACK;
		}
		qreply(q, mp);

		break;

/*
 * Cases which are only used in PAP to support the ISDN method of callback
 * authentication.
 */
	case PPP_GET_REMOTE_PASSWD:
	case PPP_REMOTE_OK:
	case PPP_REMOTE_NOK:
		rc = pap_ioctl(lp->pap, iocp->ioc_cmd, mp);
		if (rc) {
			mp->b_datap->db_type = M_IOCNAK;
		} else {
			mp->b_datap->db_type = M_IOCACK;
		}
		qreply(q, mp);

		break;
	default:
		putnext(q, mp);
		break;

iocnak:
		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCNAK;
		qreply(q, mp);

	}

}

/*
 * set_conf()
 *
 * Sets the configuration to the argument conf.
 *
 * Returns 0 on success, < 0 on failure
 */
static int
set_conf(pppLink_t *lp, pppLinkControlEntry_t *conf)
{
	int rc;

	lp->conf.pppLinkMediaType	= conf->pppLinkMediaType;


	if (conf->pppLinkRestartTimerValue > 0) {
		lp->conf.pppLinkRestartTimerValue = conf->
					    pppLinkRestartTimerValue;
	}
	if (conf->pppLinkMaxRestarts > 0) {
		lp->conf.pppLinkMaxRestarts = conf->
					    pppLinkMaxRestarts;
	}

	lp->conf.pppLinkLocalMRU = conf->pppLinkLocalMRU;
	lp->conf.pppLinkRemoteMRU = conf->pppLinkRemoteMRU;
	lp->conf.pppIPLocalAddr = conf->pppIPLocalAddr;
	lp->conf.pppIPRemoteAddr = conf->pppIPRemoteAddr;
	lp->conf.pppLinkMaxNoFlagTime = drv_usectohz(
		conf->pppLinkMaxNoFlagTime * 1000);

	rc = ppp_set_conf_lcp(lp->lcp, conf);
	if (rc) {
		return (-1);
	}

	rc = ppp_set_conf_ipncp(lp->ncp, conf);
	if (rc) {
		return (-1);
	}

	return (0);
}

/*
 * set_conf()
 *
 * Process the set conf ioctl.
 *
 * Returns 0 on success, < 0 on failure
 */
static int
set_conf_ioctl(pppLink_t *lp, mblk_t *mp)
{
	pppLinkControlEntry_t	*conf;
	int			version;

	conf = (pppLinkControlEntry_t *)
	    check_param(mp, sizeof (*conf));

	if (conf == NULL) {
		conf = extract_isdn_conf(mp);
		if (conf == NULL) {
			return (-1);
		}
		version = pppVer1;
	} else
		version = pppVer2;

	if (conf->pppLinkControlIndex > 0) {
		lp = ppp_mib[conf->pppLinkControlIndex];
	}
	lp->conf.pppLinkVersion = version;


	return (set_conf(lp, conf));
}

/*
 * get_conf()
 *
 * get the current configuration for the link
 *
 * Returns 0
 */
int
get_conf(pppLink_t *lp, pppLinkControlEntry_t *conf)
{
	if (ppp_get_conf_lcp(lp->lcp, conf) < 0)
		return (-1);
	if (ppp_get_conf_ipncp(lp->ncp, conf) < 0)
		return (-1);

	conf->pppLinkLocalACCMap = lp->conf.pppLinkLocalACCMap;

	conf->pppLinkMaxRestarts = lp->conf.pppLinkMaxRestarts;
	conf->pppLinkRestartTimerValue =
	    lp->conf.pppLinkRestartTimerValue;
	conf->pppLinkMediaType = lp->conf.pppLinkMediaType;

	conf->pppLinkLocalMRU = lp->conf.pppLinkLocalMRU;
	conf->pppLinkRemoteMRU = lp->conf.pppLinkRemoteMRU;
	conf->pppIPLocalAddr = lp->conf.pppIPLocalAddr;
	conf->pppIPRemoteAddr = lp->conf.pppIPRemoteAddr;
	conf->pppLinkMaxNoFlagTime = drv_hztousec(
		    lp->conf.pppLinkMaxNoFlagTime) / 1000;
	return (0);
}

/*
 * get_conf_ioctl()
 *
 * Processes the get conf ioctl
 *
 * Returns 0 on success, < 0 on failure
 */
int
get_conf_ioctl(pppLink_t *lp, mblk_t *mp)
{
	pppLinkControlEntry_t local_conf;
	pppLinkControlEntry_t *conf;
	int rc;

	int back_isdn = 0;

	conf = (pppLinkControlEntry_t *)
	    check_param(mp, sizeof (*conf));

	if (conf == NULL) {
		conf = &local_conf;
		back_isdn = 1;
	}

	rc = get_conf(lp, conf);
	if (rc < 0)
		return (rc);

	if (back_isdn) {
		if (fill_isdn_conf(mp, conf) != 0) {
			return (-1);
		}
	}
	return (0);

}

/*
 * ppp_randomize()
 *
 * Seed the random number generator.
 */
void
ppp_randomize(void)
{
	clock_t mybolt;

	if (drv_getparm(LBOLT, &mybolt) < 0)
		mybolt = 0;

	randx = (uint_t)mybolt;
}

/*
 * ppp_rand()
 *
 * Generate a random number.  Needed for magic number negotiation
 */
uint_t
ppp_rand(void)
{
	randx = randx * 1103515245 + 12345;
	return (randx);
}


/*
 * in cases of memory shortage, any current messages are
 * held on queue, and the operation retried later.
 */

void
queue_memory_retry(queue_t *q)
{
	pppLink_t	*lp = (pppLink_t *)q->q_ptr;

	ASSERT(MUTEX_HELD(&lp->lplock));

/*
 * ppp_memory_denied holds the total number of times
 * memory was denied
 */
	ppp_memory_denied++;

	if (lp->memouts) {
/*
 * already waiting for a buffer
 */
		return;
	}

	noenable(q); noenable(OTHERQ(q));

	PPP_MEMDBG("memory retry queued for qpair=%x\n", q);

#ifndef _SunOS4
	lp->memouts = timeout(memory_retry, q, PPP_MEMRETRY);
#else
	lp->memouts++;
	timeout(memory_retry, (caddr_t)q, PPP_MEMRETRY);
#endif
}

/*
 * re-enable the affected queue
 */
static void
memory_retry(void *arg)
{
	queue_t *q = arg;
	pppLink_t *lp = (pppLink_t *)q->q_ptr;

	mutex_enter(&lp->lplock);

	ASSERT(lp->memouts);

	PPP_MEMDBG("memory retry in progress for q=%x\n", q);
	((pppLink_t *)q->q_ptr)->memouts = 0;
	qenable(q); qenable(OTHERQ(q));

	mutex_exit(&lp->lplock);
}




/*
 * General data structure allocation routines for use by the module
 */

/*
 * allocate a link descriptor.	 This will hold details of the link
 * control protocol for the link (there can be only one), and details
 * of the network protocols carried by the link (there is only one
 * for the moment.
 *
 * Returns: a pppLink_t ptr = non-null on success, NULL on error
 *
 */
static pppLink_t *
alloc_link(queue_t *q)
{
	pppLink_t	*lp;
	int		i;
	clock_t mybolt;

	lp = (pppLink_t *)kmem_zalloc(sizeof (pppLink_t), KM_NOSLEEP);

	if (lp == NULL) {

		PPP_ERROR("alloc_link: link alloc failed for q=%x\n", q);

/*
 * if we can't get hold of memory at this point there isn't
 * really a lot of point allowing the link to go further
 */

		return (NULL);
	}

/*
 * store reference to the link with the stream
 */
	WR(q)->q_ptr = q->q_ptr = (caddr_t)lp;

/*
 * create the per-link lock
 */
	mutex_init(&lp->lplock, NULL, MUTEX_DEFAULT, NULL);

	mutex_enter(&lp->lplock);

/*
 * allocate the lcp state machine - if this fails, deallocate
 * everything allocated so far
 */
	lp->readq = q;

	lp->chap = alloc_chap_machine(q, lp);

	if (lp->chap == (chapMachine_t *)NULL) {

		PPP_ERROR("alloc_link: chap machp alloc failed q=%x\n", q);
		free_link(lp);	/* and mutex */
		return (NULL);
	}

	lp->pap = alloc_pap_machine(q, lp);

	if (lp->pap == (papMachine_t *)NULL) {

		PPP_ERROR("alloc_link: chap machp alloc failed q=%x\n", q);
		free_link(lp);	/* and mutex */
		return (NULL);
	}

	lp->lcp = alloc_lcp_machine(q, lp);
	if (lp->lcp == (pppMachine_t *)NULL) {

		PPP_ERROR("alloc_link: lcp machp alloc failed q=%x\n", q);
		free_link(lp);	/* and mutex */
		return (NULL);
	}

/*
 * allocate the lcp state machine - if this fails, deallocate
 * everything allocated so far
 */
	lp->ncp = alloc_ipncp_machine(q, lp);
	if (lp->ncp == (pppMachine_t *)NULL) {

		PPP_ERROR("alloc_link: ncp machp alloc failed q=%x\n", q);
		free_link(lp);	/* and mutex */
		return (NULL);
	}

	lp->lqm = alloc_lqm_machine(q, lp);
	if (lp->lqm == (lqmMachine_t *)NULL) {

		PPP_ERROR("alloc_link: lqm machp alloc failed q=%x\n", q);
		free_link(lp);	/* and mutex */
		return (NULL);
	}

	lp->conf.pppLinkCRCSize			= pppCRC16;
	lp->conf.pppLinkRestartTimerValue	= PPP_DEF_RESTIMER;
	lp->conf.pppLinkMaxRestarts		= PPP_DEF_MAXRESTART;
	lp->conf.pppLinkLocalMRU		= PPP_DEF_MRU;
	lp->conf.pppLinkRemoteMRU		= PPP_DEF_MRU;
	lp->conf.pppLinkLocalACCMap		= (uint_t)PPP_DEF_ASCM;
	lp->conf.pppLinkRemoteACCMap		= (uint_t)PPP_DEF_ASCM;
	lp->conf.pppLinkMediaType		= pppSync;
	lp->conf.pppLinkCommand			= 1;	/* No-op */
	lp->conf.pppLinkMaxLoopCount		= PPP_DEF_MAX_LOOP;
	lp->conf.pppLinkMaxNoFlagTime		= drv_usectohz(
		PPP_DEF_NOFLAG * 1000);

	if (drv_getparm(LBOLT, &mybolt) < 0) {
		mybolt = 0;
	}

	lp->conf.pppLinkLastFlagTime		= mybolt ^ 0x80000000;

	lp->link_up = 0;

	lp->state.pppLinkVersion		= PPP_VERSION;
	lp->state.pppLinkCurrentState		= S0;
	lp->state.pppLinkPreviousState		= S0;
	lp->state.pppLinkQuality		= pppGood;
	lp->state.pppLinkProtocolCompression	= pppNone;
	lp->state.pppLinkACCompression		= pppNone;
	lp->state.pppLinkMeasurementsValid	= FALSE;

	lp->pib.resid = NULL;
	lp->pib.mblk = NULL;

	/* insert the link in the MIB if there is space */

	vjcompress_init(&lp->vjstruct);

	rw_enter(&miblock, RW_WRITER);
	for (i = 1; i <= PPP_MAX_MIB; i++)
		if (ppp_mib[i] == NULL) {
			ppp_mib[i] = lp;
			lp->conf.pppLinkControlIndex = (ushort_t)i;
			lp->state.pppLinkStatusIndex = (ushort_t)i;
			lp->errs.pppLinkErrorsIndex = (ushort_t)i;
			lp->lcp->stats.pppCPLinkNumber = (ushort_t)i;
			lp->ncp->stats.pppCPLinkNumber = (ushort_t)i;
			lp->ncp->ip.pppIPLinkNumber = (ushort_t)i;
			break;
		}
	rw_exit(&miblock);
	mutex_exit(&lp->lplock);

	PPP_MEMDBG("alloc_link: returning link %x\n", lp);

	reset_options(lp);
	return (lp);
}


/*
 * free_link()
 *
 * dispose of a datalink structure.  Should be called at a sufficiently
 * high lock value to avoid timeouts and things arriving in mid processing
 */
static void
free_link(pppLink_t *lp)
{
	int		i;

	PPP_MEMDBG("free_link: freeing link %x\n", lp);

	ASSERT(MUTEX_HELD(&lp->lplock));

/*
 * remove entry from the MIB
 */
	rw_enter(&miblock, RW_WRITER);
	for (i = 1; i <= PPP_MAX_MIB; i++)
		if (ppp_mib[i] == lp) {
			ppp_mib[i] = NULL;
			break;
		}
	rw_exit(&miblock);

/*
 * cancel outstanding memory retry timers.  Note, for 4.x we try an
 * untimeout on both the read and write queue because we don't know
 * which had the memory shortage.
 */

	if (lp->memouts) {
#ifndef _SunOS4
		(void) untimeout(lp->memouts);
#else
		(void) untimeout(memory_retry, lp->readq);
		(void) untimeout(memory_retry, WR(lp->readq));
#endif
		lp->memouts = 0;
	}
	if (lp->lcp != NULL) {
#ifdef PPP_DEBUG
		if (lp->lcp->state != S1 /* Closed */) {
			PPP_MEMDBG2("free_link: lcp machp %x was still \
			    active(%d)\n", lp->lcp, lp->lcp->state);
		}
#endif
		free_machine(lp->lcp);
	}

	if (lp->ncp != NULL) {
#ifdef PPP_DEBUG
		if (lp->ncp->state != S1 /* Closed */) {
			PPP_MEMDBG2("free_link: ncp machp %x was active (%d)\n",
			    lp->ncp, lp->ncp->state);
		}
#endif
		free_machine(lp->ncp);
	}


	if (lp->pap != NULL) {
		free_pap_machine(lp->pap);
	}

	if (lp->chap != NULL) {
		free_chap_machine(lp->chap);
	}

	if (lp->lqm != NULL) {
		free_lqm_machine(lp->lqm);
	}

	if (lp->pib.resid)
		freemsg(lp->pib.resid);

	if (lp->pib.mblk)
		freemsg(lp->pib.mblk);

	mutex_exit(&lp->lplock);
	mutex_destroy(&lp->lplock);

	(void) kmem_free(lp, sizeof (pppLink_t));
}


/*
 * ppp_alloc_frame()
 *
 * Allocate a PPP frame for use by LCP/NCP protocols.
 *
 * On return, the frame contains a frame and packet header of the requested
 * type, the b_wptr set to the end of the packet header, b_rptr is set to the
 * start of the frame header.
 *
 * Returns: pointer to allocated frame on success, NULL or error
 */
mblk_t *
ppp_alloc_frame(pppProtocol_t protocol, uint_t code, uint_t id)
{
	mblk_t		*fp;
	static struct ppp_hdr	hdr = {
			{ PPP_FRAME_ADDR, PPP_FRAME_CTRL, /* protocol */ }};


	fp = allocb(PPP_MAX_CP_FRAMESZ, BPRI_MED);

	if (fp == NULL) {
		return (NULL);
	}

	hdr.fr.prot    = htons(protocol);

	hdr.pkt.code	= code;
	hdr.pkt.id	= id;
	hdr.pkt.length	= htons(sizeof (struct ppp_pkt_hdr));

	*(struct ppp_hdr *)fp->b_wptr = hdr;

	fp->b_wptr += PPP_HDRSZ;

	return (fp);
}

/*
 * ppp_alloc_raw_frame()
 *
 * Allocate a PPP, specifies nothing in information field.
 *
 * On return, the frame contains a frame and packet header of the requested
 * type, the b_wptr set to the end of the packet header, b_rptr is set to the
 * start of the information field.
 *
 * Returns: pointer to allocated frame on success, NULL or error
 */
mblk_t *
ppp_alloc_raw_frame(pppProtocol_t protocol)
{
	mblk_t		*fp;

	struct ppp_frame_hdr	hdr =
			{ PPP_FRAME_ADDR, PPP_FRAME_CTRL, /* protocol */ };

	fp = allocb(PPP_MAX_CP_FRAMESZ, BPRI_MED);

	if (fp == NULL) {
		return (NULL);
	}

	hdr.prot = ntohs(protocol);

	*(struct ppp_frame_hdr *)fp->b_wptr = hdr;

	fp->b_wptr += PPP_FR_HDRSZ;

	return (fp);
}

/*
 * attach_hdr()
 *
 * attach a PPP frame header to a message.
 *
 * Returns: 0 on success, error number otherwise
 */
static void
attach_hdr(mblk_t **mp, pppProtocol_t protocol)
{
	mblk_t				*hbp;
	uchar_t				*cp;

/*
 * try to see if there is already space reserved at the start
 * of the buffer which we could use for the protocol header.
 * This avoids having to allocate a buffer just for the header
 */

	if ((*mp)->b_rptr - (*mp)->b_datap->db_base >= PPP_FR_HDRSZ) {
		hbp = *mp;

		hbp->b_rptr -= PPP_FR_HDRSZ;

	} else {

/*
 * grab a buffer for the PPP frame header.   This should be
 * small enough to fit in the dblk cache thus avoiding having
 * to allocate an attached buffer
 */

		hbp = allocb(PPP_FR_HDRSZ, BPRI_MED);

		if (hbp == (mblk_t *)NULL) {
			freemsg(*mp);
			*mp = NULL;
			return;
		}

		hbp->b_wptr += PPP_FR_HDRSZ;

		/* and now prepend this to the message */

		hbp->b_cont = *mp;
	}

	cp = hbp->b_rptr;
	*cp++ = PPP_FRAME_ADDR;
	*cp++ = PPP_FRAME_CTRL;
	*cp++ = (uchar_t)((protocol >> 8) & 0xff);
	*cp++ = (uchar_t)(protocol & 0xff);

	if (ISPULLEDUP(hbp)) {
		*mp = hbp;
	} else {
		*mp = msgpullup(hbp, -1);  /* pull up the entire message */
		freemsg(hbp);
		if (*mp == NULL) {
			return;
		}
	}
}

#define	DO_ESCAPE(map, c, p) ((((c) < 0x20) && ((p) == pppLCP)) || \
	((c) == PPP_ESCAPE) || ((c) == PPP_FLAG) || \
	(((c) < 0x20) && ((1 << (c)) & (map))))

/*
 * ppp_putnext()
 *
 * Note this routine assumes that there is 1 PPP frame per message
 * In the sync case we just do a putnext() in the Async case we
 * calculate and FCS for the frame and stuff "special" characters
 */
void
ppp_putnext(queue_t *q, mblk_t *mp)
{
	pppLink_t *lp;
	ushort_t fcs;
	uint_t	accmap;
	mblk_t	*tmp, *outmp;
	size_t	 size_outmp;
	uchar_t *dp, nval;
	clock_t mylbolt;
	struct ppp_frame_hdr	*hdr;
	int	chopoff;
	char	*cp;
	ushort_t protocol;

	if (msgdsize(mp) < 4) {
		return;
	}

	lp = (pppLink_t *)q->q_ptr;

/*
 * Increment mib octet count by number of octets we're sending out
 * (before escaping and hdr-proto compression).	 Add one to count for
 * one flag character as per rfc1331 pg. 4
 */
	lp->mib_data.ifOutOctets += (msgdsize(mp) + 1);

	/* Increment mib packet count. */

	lp->mib_data.ifOutUniPackets++;

	chopoff = 0;
	hdr = (struct ppp_frame_hdr *)mp->b_rptr;

	protocol = ntohs(hdr->prot);

	if (protocol == pppLQM_REPORT)
		tx_lqr(lp->lqm, mp);

	if (protocol != pppLCP) {
		if (lp->conf.pppLinkSendAddrComp) {
			chopoff += 2;
		}
		if (lp->conf.pppLinkSendProtoComp && protocol < 0x100) {
			chopoff++;
		}

		mp->b_rptr += chopoff;

		cp = (char *)mp->b_rptr;

		if (!lp->conf.pppLinkSendAddrComp) {
			*cp++ = (uchar_t)PPP_FRAME_ADDR; /* 0xff */
			*cp++ = (uchar_t)PPP_FRAME_CTRL; /* 0x03 */
		}
		if (lp->conf.pppLinkSendProtoComp && protocol < 0x100) {
			*cp++ = (uchar_t)(protocol & 0xff);
		} else {
			*cp++ = (uchar_t)((protocol >> 8) & 0xff);
			*cp++ = (uchar_t)(protocol & 0xff);
		}
	}


	if (lp->conf.pppLinkMediaType != pppAsync) {
		putnext(q, mp);
		return;
	}

	/* Packet is async */

	fcs = PPP_INITFCS;

	accmap = lp->conf.pppLinkRemoteACCMap;

	size_outmp = 2 * msgdsize(mp) + 4 * sizeof (fcs) + 2 * sizeof (char);
	outmp = allocb(size_outmp, BPRI_MED);
	if (outmp == NULL) {
		freemsg(mp);
		return;
	}

	if (drv_getparm(LBOLT, &mylbolt) != -1) {
		if (ABS((clock32_t)mylbolt - lp->conf.pppLinkLastFlagTime) >
		    lp->conf.pppLinkMaxNoFlagTime) {
			*outmp->b_wptr++ = PPP_FLAG;
		}
		lp->conf.pppLinkLastFlagTime = mylbolt;
	} else
		*outmp->b_wptr++ = PPP_FLAG;

	for (tmp = mp; tmp != NULL; tmp = tmp->b_cont) {
		if (MTYPE(mp) != M_DATA) {
			continue;
		}
		for (dp = tmp->b_rptr; dp < tmp->b_wptr; dp++) {
			fcs = PPP_FCS(fcs, *dp);
			if (DO_ESCAPE(accmap, *dp, protocol)) {
				*outmp->b_wptr++ = PPP_ESCAPE;
				*outmp->b_wptr++ = *dp ^ PPP_MASK;
			} else {
				*outmp->b_wptr++ = *dp;
			}
		}
	}

	fcs ^= 0xffff;

	nval = fcs & 0xff;
	if (DO_ESCAPE(accmap, nval, protocol)) {
		*outmp->b_wptr++ = PPP_ESCAPE;
		*outmp->b_wptr++ = nval ^ PPP_MASK;
	} else
		*outmp->b_wptr++ = nval;

	nval = fcs >> 8;
	if (DO_ESCAPE(accmap, nval, protocol)) {
		*outmp->b_wptr++ = PPP_ESCAPE;
		*outmp->b_wptr++ = nval ^ PPP_MASK;
	} else
		*outmp->b_wptr++ = nval;

	*outmp->b_wptr++ = PPP_FLAG;

	putnext(q, outmp);
	freemsg(mp);
}

/*
 * unpack_ppp()
 *
 * Unpack a PPP frame and accumulate chars in the async case
 * A return value of TRUE means mp now points to a message that
 * contains an entire PPP frame.
 * unpack_ppp() always frees mp or puts part of the message
 * back on the queue.
 */
extern mblk_t *
unpack_ppp(queue_t *q, mblk_t *mp)
{
	pppLink_t		*lp;
	unsigned char	*dp;
	mblk_t		*rval, *zmp;
	char		*phdr;
	ushort_t		prot;

	lp = (pppLink_t *)q->q_ptr;

/*
 * This is only a precaution as most drivers send things
 * in one message block
 */
	if (!ISPULLEDUP(mp)) {
		zmp = msgpullup(mp, -1);
		freemsg(mp);
		mp = zmp;
		if (mp == NULL) {
			return (NULL);
		}
	}

	mutex_enter(&lp->lplock);

	for (dp = mp->b_rptr; dp < mp->b_wptr; dp++) {

		if (*dp == PPP_FLAG) {
			if (lp->pib.flush || lp->pib.mblk == NULL ||
			    msgdsize(lp->pib.mblk) == 0) {
				lp->pib.flush = FALSE;
				continue;
			}
			if (lp->pib.fcs == PPP_GOODFCS) {
				rval = lp->pib.mblk;
				(void) adjmsg(rval, - sizeof (ushort_t));
				lp->pib.mblk = NULL;
/*
 * put any additional data
 * back on the queue
 */
				if (mp->b_rptr < (mp->b_wptr - 1)) {
					mp->b_rptr = dp + 1;
					lp->pib.resid = mp;
				} else {
					freemsg(mp);
				}
				lp->mib_data.ifInOctets += msgdsize(rval) + 1;
				lp->mib_data.ifInUniPackets++;

				lp->lqm->lqm_info.inGoodOctets +=
					msgdsize(rval) + 1;
				phdr = (char *)rval->b_rptr;

				phdr++;
				phdr++;

				bcopy((caddr_t)phdr, (caddr_t)&prot,
				    sizeof (ushort_t));
				if (ntohs(prot) == pppLQM_REPORT)
					rx_lqr(lp->lqm, rval);
				/* Trim FCS short off the end of the message */
				mutex_exit(&lp->lplock);
				return (rval);
			} else {
				PPP_STRDBG("Bad PPP frame FCS! %d\n", 1);
				freemsg(lp->pib.mblk);
				lp->pib.mblk = NULL;
				lp->mib_data.ifInErrors++;
				lp->vjstruct.flags |= SLF_TOSS;
			}
			continue;
		}

		if (lp->pib.flush)
			continue;

		if (lp->pib.mblk == NULL) {
			if (!alloc_pib(lp)) {
				/* alloc_pib failed try later */
				(void) putbq(q, mp);
				/* bugid 1225317: sgypsy@eng */
				mutex_exit(&lp->lplock);
				return (NULL);
			}
		}

		if (*dp == PPP_ESCAPE) {
			lp->pib.escaped = TRUE;
			continue;
		}

		if (lp->pib.escaped) {
			*dp ^= PPP_MASK;
			lp->pib.escaped = FALSE;
		} else if (*dp < 0x20 && ((1 << *dp) &
				lp->conf.pppLinkLocalACCMap)) {
/*
 * Character sent which should be escaped
 * as per rfc1331
 */
			continue;
		}

		if (msgdsize(lp->pib.mblk) < lp->pib.bufsize) {
			lp->pib.fcs = PPP_FCS(lp->pib.fcs, *dp);
			if (msgdsize(lp->pib.mblk) == 0 &&
			    *dp != PPP_FRAME_ADDR) {
				*lp->pib.mblk->b_wptr++ = PPP_FRAME_ADDR;
				*lp->pib.mblk->b_wptr++ = PPP_FRAME_CTRL;
			}
			if (msgdsize(lp->pib.mblk) == 2 && (*dp & 1)) {
				*lp->pib.mblk->b_wptr++ = 0;
			}
			*lp->pib.mblk->b_wptr++ = *dp;
		} else {    /* buffer overrun or they lied to us about MRU */
			/* XXX should update packet discard info */
			lp->vjstruct.flags |= SLF_TOSS;
			freemsg(lp->pib.mblk);
			lp->pib.mblk = NULL;
			lp->pib.flush = TRUE;
		}

	}

	mutex_exit(&lp->lplock);
	freemsg(mp);
	return (NULL);
}

/*
 * ppp_getframe()
 *
 * Gets the next ppp frame off the wire
 */
mblk_t *
ppp_getframe(queue_t *q)
{
	mblk_t *mp;
	mblk_t *tmp;
	pppLink_t		*lp;
	ushort_t protocol;
	uchar_t	*dp, *test;

	/* get a handle on the link */

	lp = (pppLink_t *)q->q_ptr;

	if (lp->conf.pppLinkMediaType == pppAsync) {
		mutex_enter(&lp->lplock);
		mp = getq(q);
		if (lp->pib.resid != NULL &&
		    (mp == NULL || MTYPE(mp) == M_DATA)) {
			if (mp != NULL)
				(void) putbq(q, mp);
			mp = lp->pib.resid;
			lp->pib.resid = NULL;
		}

		mutex_exit(&lp->lplock);

		while (mp != NULL) {
			if (MTYPE(mp) != M_DATA)
				return (mp);
			tmp = unpack_ppp(q, mp);
/*
 * N.B. we assume unpack PPP has
 * done a freemsg(mp);
 */
			if (tmp != NULL) {
				PPP_STRDBG2("getframe:incoming M_DATA %d \
				    bytes on q = %x\n", msgdsize(tmp), q);
				PPP_FRAME_DUMP("REC", tmp);
				return (tmp);
			}
			mp = getq(q);

		}
		return (NULL);
	} else {

/*
 * the assumption is that sync devices return 1 ppp frame
 * per message
 */
		mp = getq(q);
		if (mp == NULL)
			return (NULL);
		if (MTYPE(mp) != M_DATA)
			return (mp);
		if (msgdsize(mp) < 4)
			return (mp);
		dp = (uchar_t *)mp->b_rptr;
		if ((*dp == PPP_FRAME_ADDR) && (*(dp+1) == PPP_FRAME_CTRL)) {
			dp++;
			dp++;
		}
		test = (uchar_t *)&protocol;
		if (*dp & 1) {
			*test++ = 0;
			*test++ = *dp++;
		} else {
			*test++ = *dp++;
			*test++ = *dp++;
		}
		mp->b_rptr = dp;
		attach_hdr(&mp, protocol);
		return (mp);
	}
}

/*
 * alloc_pib()
 *
 * Allocate the pib structure and initialize for doing FCS
 *
 * Returns TRUE on success, FALSE on failure
 */
static int
alloc_pib(pppLink_t *lp)
{
	lp->pib.bufsize = PPP_MINBUF(lp);
	lp->pib.mblk = allocb(lp->pib.bufsize+PPP_FR_HDRSZ, BPRI_MED);
	/* allocb failed, try again later */
	if (lp->pib.mblk == NULL) {
		return (FALSE);
	}
	lp->pib.escaped = FALSE;
	lp->pib.fcs = PPP_INITFCS;
	lp->pib.flush = FALSE;
	return (TRUE);
}

#ifdef PPP_DEBUG
/*
 * packet diagnostic routines for PPP......
 * ...
 * XXX must fix this to use strlog() rather than printf
 */
void
frame_dump(char *func, mblk_t *fp)
{

	struct ppp_hdr		*hp;
#ifdef NOTDEF
	uchar_t			*dp;
	int			i;
#endif

	if ((ppp_debug &  PPP_FRAMES) == 0) {
		return;
	}

	if (fp->b_wptr - fp->b_rptr >= PPP_HDRSZ) {

/*
 * hope that at least the frame and packet header is at least
 * contained in the first message block
 */

		hp = (struct ppp_hdr *)fp->b_rptr;

		printf("frame_dump %s: [%x %x %x] pkt=%d id=%d len=%d\n",
			func, hp->fr.addr, hp->fr.ctrl, hp->fr.prot,
			hp->pkt.code, hp->pkt.id, hp->pkt.length);
	}

#ifdef NOTDEF
/*
 * now dump the frame contents including the header
 */
	i = 0;
	printf("frame_dump %s: ");
	do {
		for (dp = (uchar_t *)fp->b_rptr; dp < (uchar_t *)fp->b_wptr;
								dp++) {
			printf("%x ", *dp);
			if (++i % 60 == 0) {
				printf("\n");
			}
		}
		fp = fp->b_cont;
	} while (fp);
	printf("\n");
#endif
}
#endif PPP_DEBUG

/*
 * STREAMS diagnostic code for PPP...
 */

#ifdef PPP_DEBUG
static void str_print(queue_t *q, mblk_t *mp)
{
	if (!ppp_debug) {
		return;
	}

	do {
		switch (MTYPE(mp)) {

		case M_DATA:
			PPP_STRDBG2("str_print: queue=%x M_DATA %d bytes\n",
					q, mp->b_wptr - mp->b_rptr);
			break;

		case M_PROTO:
			PPP_STRDBG("str_print: queue=%x M_PROTO\n", q);
			break;

		case M_PCPROTO:
			PPP_STRDBG("str_print: queue=%x M_PCPROTO\n", q);
			break;

		case M_IOCTL:
			PPP_STRDBG("str_print: queue=%x M_IOCTL\n", q);
			break;
		case M_IOCACK:
			PPP_STRDBG("str_print: queue=%x M_IOCACK\n", q);
			break;

		case M_SIG:
			PPP_STRDBG("str_print: queue=%x M_SIG\n", q);
			break;

		case M_CTL:
			PPP_STRDBG("str_print: queue=%x M_CTL\n", q);
			break;

		case M_FLUSH:
			PPP_STRDBG("str_print: queue=%x M_FLUSH\n", q);
			break;

		case M_HANGUP:
			PPP_STRDBG("str_print: queue=%x M_HANGUP\n", q);
			break;

		case M_PCSIG:
			PPP_STRDBG2("str_print: queue=%x M_PCSIG %d\n", q,
			    *(mp->b_rptr));
			break;

		default:
			PPP_STRDBG2("str_print: queue=%x OTHER=%o\n", q,
			    MTYPE(mp));
		}

		mp = mp->b_cont;

		if (mp) {
			PPP_STRDBG("str_print: q=%x is attached to ->\n", q);
		}
	} while (mp);
}
#endif

/*
 * *******************************************
 * Routines for ISDN compatibility	    *
 * ****************************************** */

/*
 * extract_isdn_conf()
 *
 * Returns the configuration in the message block which uses the old
 * structure
 */
pppLinkControlEntry_t *
extract_isdn_conf(mblk_t *mp)
{
	static pppLinkControlEntry_t hold_conf;
	pppIsdnLinkControlEntry_t *old_conf;
	pppLinkControlEntry_t *conf;

	conf = &hold_conf;

	old_conf = (pppIsdnLinkControlEntry_t *)
				check_param(mp, sizeof (*old_conf));

	conf->pppLinkControlIndex = old_conf->pppLinkControlIndex;
	conf->pppLinkMaxRestarts = old_conf->pppLinkMaxRestarts;
	conf->pppLinkLocalMRU = old_conf->pppLinkLocalMRU;
	conf->pppLinkRemoteMRU = old_conf->pppLinkRemoteMRU;
	conf->pppLinkRestartTimerValue =
				old_conf->pppLinkRestartTimerValue;
	conf->pppLinkLocalACCMap = old_conf->pppLinkLocalACCMap;
	conf->pppLinkMediaType = old_conf->pppLinkMediaType;
	conf->pppIPLocalAddr = old_conf->pppIPLocalAddr;
	conf->pppIPRemoteAddr = old_conf->pppIPRemoteAddr;

	conf->pppLinkAllowMRU = OLD_ALLOW;
	conf->pppLinkAllowPAComp = OLD_ALLOW;
	conf->pppLinkAllowACC = OLD_ALLOW;
	conf->pppLinkAllowMagic = OLD_ALLOW;
	conf->pppLinkAllowQual = OLD_ALLOW;
	conf->pppLinkAllowAuth = OLD_ALLOW;
	conf->pppLinkAllowAddr = OLD_ALLOW;
	conf->pppLinkAllowHdrComp = OLD_ALLOW;

	return (conf);
}

/*
 * fill_isdn_conf()
 *
 * Fills the message block with the conf configuration using the old
 * configuration structure.
 *
 * Returns: 0 for OK, < 0 on error
 */
static int
fill_isdn_conf(mblk_t *mp, pppLinkControlEntry_t *conf)
{
	pppIsdnLinkControlEntry_t *old_conf;

	old_conf = (pppIsdnLinkControlEntry_t *)
				check_param(mp, sizeof (*old_conf));
	if (old_conf == NULL)
		return (-1);

	old_conf->pppLinkControlIndex = conf->pppLinkControlIndex;
	old_conf->pppLinkMaxRestarts = conf->pppLinkMaxRestarts;
	old_conf->pppLinkLocalMRU = conf->pppLinkLocalMRU;
	old_conf->pppLinkRemoteMRU = conf->pppLinkRemoteMRU;
	old_conf->pppLinkRestartTimerValue =
				conf->pppLinkRestartTimerValue;
	old_conf->pppLinkLocalACCMap = conf->pppLinkLocalACCMap;
	old_conf->pppLinkMediaType = conf->pppLinkMediaType;
	old_conf->pppIPLocalAddr = conf->pppIPLocalAddr;
	old_conf->pppIPRemoteAddr = conf->pppIPRemoteAddr;
	return (0);
}
