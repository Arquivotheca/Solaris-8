/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_KEYSOCK_H
#define	_INET_KEYSOCK_H

#pragma ident	"@(#)keysock.h	1.1	98/12/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int keysock_opt_get(queue_t *, t_uscalar_t, t_uscalar_t, uchar_t *);
extern int keysock_opt_set(queue_t *, t_uscalar_t, t_uscalar_t, t_uscalar_t,
    t_uscalar_t, uchar_t *, t_uscalar_t *, uchar_t *);

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

extern optdb_obj_t	keysock_opt_obj;
extern uint_t		keysock_max_optbuf_len;

/*
 * keysock session state (one per open PF_KEY socket (i.e. as a driver))
 *
 * I keep these in a linked list, and assign a monotonically increasing
 * serial ## (which is also the minor number).
 */

typedef struct keysock_s {
	/* Protected by keysock_list_lock. */
	struct keysock_s *keysock_next; /* Next in list */
	struct keysock_s **keysock_ptpn; /* Pointer to previous next */

	kmutex_t keysock_lock; /* Protects the following. */
	queue_t *keysock_rq;   /* Read queue - putnext() to userland */
	queue_t *keysock_wq;   /* Write queue */

	uint_t keysock_state;
	uint_t keysock_flags;
	/* If SADB_SATYPE_MAX (in net/pfkeyv2.h) > 255, rewhack this. */
	uint64_t keysock_registered[4]; /* Registered types for this socket. */

	/* Also protected by keysock_list_lock. */
	minor_t keysock_serial; /* Serial number of this socket. */
} keysock_t;

#define	KEYSOCK_NOLOOP	0x1	/* Don't loopback messages (no replies). */
#define	KEYSOCK_PROMISC	0x2	/* Give me all outbound messages. */
				/* DANGER:	Setting this requires EXTRA */
				/* 		privilege on an MLS box. */

/* My apologies for the ugliness of this macro.  And using constants. */
#define	KEYSOCK_ISREG(ks, satype) (((ks)->keysock_registered[(satype) >> 3]) & \
	(1 << ((satype) & 63)))
#define	KEYSOCK_SETREG(ks, satype) (ks)->keysock_registered[(satype) >> 3] |= \
	(1 << ((satype) & 63))

/*
 * Keysock consumers (i.e. AH, ESP), in array based on sadb_msg_satype.
 * For module instances.
 */

typedef struct keysock_consumer_s {
	kmutex_t kc_lock;	/* Protects instance. */

	queue_t *kc_rq;		/* Read queue, requests from AH, ESP. */
	queue_t *kc_wq;		/* Write queue, putnext down */

	/* Other goodies as a need them. */
	uint8_t kc_sa_type;	/* What sort of SA am I? */
	uint_t kc_flags;
} keysock_consumer_t;

/* Can only set flags when keysock_consumer_lock is held. */
#define	KC_INTERNAL 0x1		/* Consumer maintained by keysock itself. */
#define	KC_FLUSHING 0x2		/* SADB_FLUSH pending on this consumer. */

#ifdef	__cplusplus
}
#endif

#endif /* _INET_KEYSOCK_H */
