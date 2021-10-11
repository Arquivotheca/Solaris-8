/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _KBTRANS_STREAMS_H
#define	_KBTRANS_STREAMS_H

#pragma ident	"@(#)kbtrans_streams.h	1.10	99/08/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/stream.h>

#define	KBTRANS_POLLED_BUF_SIZE	30

/* definitions for various state machines */
#define	KBTRANS_STREAMS_OPEN	0x00000001 /* keyboard is open for business */

#define	NO_HARD_RESET	0	/* resets only state struct */
#define	HARD_RESET	1	/* resets keyboard and state structure */

/*
 * structure to keep track of currently pressed keys when in
 * TR_UNTRANS_EVENT mode
 */
typedef struct  key_event {
	uchar_t  key_station;   /* Physical key station associated with event */
	Firm_event event;	/* Event that sent out on down */
} Key_event;


/* state structure for kbtrans_streams */
struct  kbtrans {
	struct kbtrans_lower	kbtrans_lower;	/* actual translation state */

	/* Read and write queues */
	queue_t 	*kbtrans_streams_readq;
	queue_t 	*kbtrans_streams_writeq;

	/* Pending "ioctl" awaiting buffer */
	mblk_t  	*kbtrans_streams_iocpending;

	/* Number of times the keyboard overflowed input */
	int		kbtrans_overflow_cnt;

	/* random flags */
	int		kbtrans_streams_flags;

	/* id from qbufcall on allocb failure */
	bufcall_id_t	kbtrans_streams_bufcallid;

	timeout_id_t	kbtrans_streams_rptid; /* timeout id for repeat */

	int	kbtrans_streams_iocerror;	/* error return from "ioctl" */
	int	kbtrans_streams_translate_mode;	/* Translate keycodes? */
	int	kbtrans_streams_translatable;  	/* Keyboard is translatable? */

	/* Vuid_id_addrs for various events */
	struct {
	    short	ascii;
	    short	top;
	    short	vkey;
	}	kbtrans_streams_vuid_addr;

	/*
	 * Table of key stations currently down that have
	 * have firm events that need to be matched with up transitions
	 * when translation mode is TR_*EVENT
	 */
	struct  key_event *kbtrans_streams_downs;

	/* Number of down entries */
	int	kbtrans_streams_num_downs_entries; /* entries in downs */

	/* Bytes allocated for downs */
	uint_t  kbtrans_streams_downs_bytes;

	/* Abort state */
	enum {
		ABORT_NORMAL,
		ABORT_ABORT1_RECEIVED
	}		kbtrans_streams_abort_state;

	/* Indicated whether or not abort may be honored */
	boolean_t	kbtrans_streams_abortable;

	/*
	 * During an abort sequence, says which key started the sequence.
	 * This is used to support both L1+A and F1+A.
	 */
	kbtrans_key_t	kbtrans_streams_abort1_key;

	/* Functions to be called based on the translation type */
	struct keyboard_callback *kbtrans_streams_callback;

	/* Private structure for the keyboard specific module/driver */
	struct kbtrans_hardware *kbtrans_streams_hw;

	/* Callbacks into the keyboard specific module/driver */
	struct kbtrans_hw_callbacks *kbtrans_streams_hw_callbacks;

	/* Keyboard type */
	int	kbtrans_streams_id;

	/* Buffers to hold characters during the polled mode */
	char	*kbtrans_polled_pending_chars;
	char	kbtrans_polled_buf[KBTRANS_POLLED_BUF_SIZE+1];
};

#ifdef __cplusplus
}
#endif

#endif /* _KBTRANS_STREAMS_H */
