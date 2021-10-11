/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LQM_H
#define	_LQM_H

#pragma ident	"@(#)ppp_lqm.h	1.9	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEFAULT_LQM_REP		3000
#define	MIN_LQM_REP		100
#define	MAX_LQM_REP		60000


typedef struct {
	uint_t	last_peerOutLQRs;
	uint_t	last_peerOutPackets;
	uint_t	last_peerOutOctets;
	uint_t	saveInLQRs;
	uint_t	saveInPackets;
	uint_t	saveInDiscards;
	uint_t	saveInErrors;
	uint_t	saveInOctets;
	uint_t	outLQRs;
	uint_t	inLQRs;
	uint_t	inGoodOctets;
} lqm_info_t;

typedef struct {
	uint_t	magic_num;
	uint_t	lastOutLQRs;
	uint_t	lastOutPackets;
	uint_t	lastOutOctets;
	uint_t	peerInLQRs;
	uint_t	peerInPackets;
	uint_t	peerInDiscards;
	uint_t	peerInErrors;
	uint_t	peerInOctets;
	uint_t	peerOutLQRs;
	uint_t	peerOutPackets;
	uint_t	peerOutOctets;
} LQM_pack_t;

struct lqmMachine {
	pppLink_t	*linkp;
	queue_t		*readq;

	int		send_on_rec;

	LQM_pack_t	last_lqm_in;
	lqm_info_t	lqm_info;

	timeout_id_t	lqm_send;
	timeout_id_t	timedoutid;
};

/*
 * typedef struct lqmMachine lqmMachine_t;
 */

#ifdef __cplusplus
}
#endif

#endif	/* _LQM_H */
