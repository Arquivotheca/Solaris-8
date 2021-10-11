/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	AGENT_H
#define	AGENT_H

#pragma ident	"@(#)agent.h	1.1	99/04/09 SMI"

#include <sys/types.h>

#include "timer_queue.h"
#include "event_handler.h"

/*
 * agent.h contains general symbols that should be available to all
 * source programs that are part of the agent.  in general, files
 * specific to a given collection of code (such as interface.h or
 * dhcpmsg.h) are to be preferred to this dumping ground.  use only
 * when necessary.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * global variables: `tq' and `eh' represent the global timer queue
 * and event handler, as described in the README. `class_id' is our
 * vendor class id set early on in main().  `inactivity_id' is the
 * timer id of the global inactivity timer, which shuts down the agent
 * if there are no interfaces to manage for DHCP_INACTIVITY_WAIT
 * seconds.
 */

extern tq_t		*tq;
extern eh_t		*eh;
extern char		*class_id;
extern int		class_id_len;
extern tq_timer_id_t	inactivity_id;

/*
 * global tunable parameters.  an `I' in the preceding comment indicates
 * an implementation artifact; a `R' in the preceding comment indicates
 * that the value was suggested (or required) by RFC2131.
 */

/* I: how many seconds to wait before restarting DHCP on an interface */
#define	DHCP_RESTART_WAIT	10

/* R: the maximum number of seconds to wait before SELECTING on an interface */
#define	DHCP_SELECT_WAIT	10

/* R: how many seconds before lease expiration we give up trying to rebind */
#define	DHCP_REBIND_MIN		60

/* I: seconds to wait retrying dhcp_expire() if uncancellable async event */
#define	DHCP_EXPIRE_WAIT	10

/* R: approximate percentage of lease time to wait until RENEWING state */
#define	DHCP_T1_FACT		.5

/* R: approximate percentage of lease time to wait until REBINDING state */
#define	DHCP_T2_FACT		.875

/* I: number of REQUEST attempts before assuming something is awry */
#define	DHCP_MAX_REQUESTS	4

/* I: epsilon in seconds used to check if old and new lease times are same */
#define	DHCP_LEASE_EPS		30

/* I: if lease is not being extended, seconds left before alerting user */
#define	DHCP_LEASE_ERROR_THRESH	(60*60*24*2)	/* two days */

/* I: how many seconds before bailing out if there's no work to do */
#define	DHCP_INACTIVITY_WAIT	(60*3)		/* three minutes */

/* I: the maximum amount of seconds we use an adopted lease */
#define	DHCP_ADOPT_LEASE_MAX	(60*60)		/* one hour */

/* I: the maximum amount of milliseconds to wait for an ipc request */
#define	DHCP_IPC_REQUEST_WAIT	(3*1000)	/* three seconds */

/*
 * reasons for why eh_handle_events() returned
 */
enum { DHCP_REASON_INACTIVITY, DHCP_REASON_SIGNAL, DHCP_REASON_TERMINATE };

#ifdef	__cplusplus
}
#endif

#endif	/* AGENT_H */
