/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PPP_PAP_H
#define	_SYS_PPP_PAP_H

#pragma ident	"@(#)ppp_pap.h	1.12	97/10/22 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PAP finite state machine defines
 *
 * PPP state defines - these are encoded in the lower byte of a
 * PPP finite state machine entry
 */

#define	PAP_NACTIONS	(11)
#define	PAP_NSTATES	(9)
#define	PAP_NEVENTS	(11)

#define	PAP_MAX_ID		255
#define	PAP_MAX_PASSWD	255
#define	PAP_FS		-1

/*
 * PAP events
 */

typedef enum {papAuthBoth, papAuthRem, papAuthLoc, papAuthGood,
		papAuthBad, papAuthAck, papAuthNak, papTOwait,
		papTOgtreq, papTOeqreq, papClose }
	papEvent_t;


/*
 * PAP actions
 */
typedef enum { papNullAct, papSarirr, papSarirrirw, papIrw, papAas, papLaf,
	papSar, papSaaaas, papSanraf, papRaf, papSaa}
	papAction_t;

typedef enum { P0, P1, P2, P3, P4, P5, P6, P7, P8 }
	papState_t;
/*
 * PAP action defines - these are encoded in the upper byte of a fsm entry
 */
#define	SARIRR		(papSarirr << 8)
#define	SARIRRIRW	(papSarirrirw << 8)
#define	IRW		(papIrw << 8)
#define	PAAS		(papAas << 8)
#define	PLAF		(papLaf << 8)
#define	SAR		(papSar << 8)
#define	SAAAAS		(papSaaaas << 8)
#define	SANRAF		(papSanraf << 8)
#define	PRAF		(papRaf << 8)
#define	SAA		(papSaa << 8)


/*
 * PAP packet types
 */
enum ppp_pap { Authenticate = 1, AuthenticateAck, AuthenticateNak };

#define	PAP_DEF_MAXRESTART	(10)
#define	PAP_DEF_RESTIMER	(3000)
#define	PAP_DEF_WAITTIMER	(30000)

struct papMachine {
	queue_t		*readq;
	pppProtocol_t	protocol;

	papState_t	state;

	int		req_restart_counter;
	timeout_id_t	req_restart;
	timeout_id_t	req_timedoutid;

	timeout_id_t	req_wait_timer;
	timeout_id_t	req_wait_timedoutid;

	papPasswdEntry_t local_passwd;
	papPasswdEntry_t remote_passwd;
	papPasswdEntry_t remote_passwd_recv;
	int		repid;

	int		peer_pass_set;

	int		papMaxRestarts;
	int		papRestartTimerValue;
	int		papWaitTimerValue;

	mblk_t		*result, *request;

	short		crid;

	pppLink_t	*linkp;		/* ptr to parent link */


};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PPP_PAP_H */
