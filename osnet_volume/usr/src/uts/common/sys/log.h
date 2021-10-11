/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_LOG_H
#define	_SYS_LOG_H

#pragma ident	"@(#)log.h	1.16	99/11/24 SMI"

#include <sys/strlog.h>
#include <sys/stream.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	LOG_CONSMIN	0			/* /dev/conslog minor */
#define	LOG_LOGMIN	5			/* /dev/log clone-open minor */
#define	LOG_BACKLOG	LOG_LOGMIN		/* console backlog queue */
#define	LOG_CLONEMIN	(LOG_LOGMIN + 1)	/* smallest /dev/log clone */
#define	LOG_MAX		(LOG_CLONEMIN + 16)	/* up to 16 /dev/log clones */

#define	LOG_MID		44		/* module ID */
#define	LOG_MINPS	0		/* min packet size */
#define	LOG_MAXPS	1024		/* max packet size */
#define	LOG_LOWAT	2048		/* threshold for backenable */
#define	LOG_HIWAT	1048576		/* threshold for tossing messages */

#define	LOG_MAGIC	0xf00d4109U	/* "food for log" - unsent msg magic */
#define	LOG_RECENTSIZE	8192		/* queue of most recent messages */
#define	LOG_MINFREE	4096		/* message cache low water mark */
#define	LOG_MAXFREE	8192		/* message cache high water mark */

typedef struct log log_t;
typedef int (log_filter_t)(log_t *, log_ctl_t *);

struct log {
	queue_t		*log_q;		/* message queue */
	log_filter_t	*log_wanted;	/* message filter */
	mblk_t		*log_data;	/* parameters for filter */
	short		log_flags;	/* message type (e.g. SL_CONSOLE) */
	int		log_overflow;	/* messages lost due to QFULL */
};

#define	LOG_MSGSIZE	200

typedef struct log_dump {
	uint32_t	ld_magic;	/* LOG_MAGIC */
	uint32_t	ld_msgsize;	/* MBLKL(mp->b_cont) */
	uint32_t	ld_csum;	/* checksum32(log_ctl) */
	uint32_t	ld_msum;	/* checksum32(message text) */
	/*
	 * log_ctl and message text follow here -- see dump_messages()
	 */
} log_dump_t;

#ifdef _KERNEL

extern log_t log_log[LOG_MAX];	/* log device state table */
extern short log_active;	/* active types (OR of all log_flags fields) */
extern queue_t *log_consq;	/* primary console reader queue */
extern queue_t *log_backlog;	/* console backlog queue */
extern queue_t *log_recent;	/* recent console message queue */
extern queue_t *log_intrq;	/* pending high-level interrupt message queue */

extern log_filter_t log_error;
extern log_filter_t log_trace;
extern log_filter_t log_console;

extern void log_init(void);
extern void log_enter(void);
extern void log_exit(void);
extern void log_update(log_t *, queue_t *, short, log_filter_t);
extern mblk_t *log_makemsg(int, int, int, int, int, void *, size_t, int);
extern void log_freemsg(mblk_t *);
extern void log_sendmsg(mblk_t *);
extern void log_flushq(queue_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LOG_H */
