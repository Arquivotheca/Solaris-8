/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PPP_CHAP_H
#define	_PPP_CHAP_H

#pragma ident	"@(#)ppp_chap.h	1.10	98/01/06 SMI"



#ifdef	__cplusplus
extern "C" {
#endif

typedef enum { AuthBoth, AuthRem, AuthLoc, Chall, Succ, Fail, GoodResp,
	BadResp, TogtResp, ToeqResp, TogtChall, ToeqChall, Force, ChapClose }
		chapEvent_t;

typedef enum {NullChap, Srcirc, Srrirr, Srsaas, Srs, Srfraf, Aas, Laf, Raf,
	Srr, Src } chapAction_t;

typedef enum {C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11 }
	chapState_t;

#define	CHAP_NEVENTS	14
#define	CHAP_STATES	12
#define	MAX_CHALL_SIZE	255
#define	CHAP_MAX_NAME	255

#define	CHAP_DEF_RESTIMER	(3000)	/* restart timer interval (millisecs) */
#define	CHAP_DEF_MAXRESTART	(10)	/* maximum number of restarts	*/

#define	SRCIRC		(Srcirc << 8)
#define	SRRIRR		(Srrirr << 8)
#define	SRS		(Srs << 8)
#define	SRFRAF		(Srfraf << 8)
#define	LAF		(Laf << 8)
#define	AAS		(Aas << 8)
#define	SRSAAS		(Srsaas << 8)
#define	RAF		(Raf << 8)
#define	SRR		(Srr << 8)
#define	SRC		(Src << 8)

#define	FSM_ERR		(-1)

typedef short chapTuple_t;

struct chap_hdr {
	uchar_t	code;
	uchar_t	ident;
	ushort_t length;
};

struct chall_resp {
	uchar_t	value_size;
	uchar_t	value[1];
};

struct succ_fail {
	uchar_t	code;
	uchar_t	ident;
	ushort_t length;
	uchar_t	message[1];
};


/*
 * typedef struct chapMachine chapMachine_t;
 */

struct chapMachine {
	queue_t		*readq;
	pppProtocol_t	protocol;

	chapState_t	state;

	timeout_id_t	chall_restart;
	timeout_id_t	resp_restart;

	int		chall_restart_counter;
	int		resp_restart_counter;
	timeout_id_t	chall_timedoutid;
	timeout_id_t	resp_timedoutid;

	uchar_t		chall_value[MAX_CHALL_SIZE];
	int		chall_size;

	uchar_t		local_secret[CHAP_MAX_PASSWD];
	int		local_secret_size;

	uchar_t		local_name[CHAP_MAX_NAME];
	int		local_name_size;

	uchar_t		remote_secret[CHAP_MAX_PASSWD];
	int		remote_secret_size;

	uchar_t		remote_name[CHAP_MAX_NAME];
	int		remote_name_size;

	int		chapMaxRestarts;
	int		chapRestartTimerValue;

	mblk_t		*response, *result, *chall;

	ushort_t	crid;
	ushort_t	respid;


	pppLink_t	*linkp;		/* ptr to parent link */


};

typedef enum { Challenge = 1, Response, Success, Failure } chapCode_t;

typedef struct {
	uint_t			chap_result;	/* set to PPP_TL_UP */
	pppProtocol_t		protocol;
} chapProtoResult_t;

#ifdef __cplusplus
}
#endif

#endif	/* _PPP_CHAP_H */
