/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#ifndef	__RTLD_DB_H
#define	__RTLD_DB_H

#pragma ident	"@(#)_rtld_db.h	1.10	98/08/28 SMI"

#include <proc_service.h>
#include <thread.h>
#include <synch.h>
#include "sgs.h"
#include "machdep.h"


struct rd_agent {
	mutex_t				rd_mutex;
	struct ps_prochandle *		rd_psp;		/* prochandle pointer */
	psaddr_t			rd_rdebug;	/* rtld r_debug */
	psaddr_t			rd_preinit;	/* rtld_db_preinit */
	psaddr_t			rd_postinit;	/* rtld_db_postinit */
	psaddr_t			rd_dlact;	/* rtld_db_dlact */
	psaddr_t			rd_tbinder;	/* tail of binder */
	psaddr_t			rd_rtlddbpriv;	/* rtld rtld_db_priv */
	ulong_t				rd_flags;	/* flags */
	int				rd_dmodel;	/* data model */
};


/*
 * Values for rd_flags
 */
#define	RDF_FL_COREFILE		0x0001		/* client is core file image */



#define	RDAGLOCK(x)	(void) mutex_lock(&(x->rd_mutex));
#define	RDAGUNLOCK(x)	(void) mutex_unlock(&(x->rd_mutex));
#define	LOG(func)	(void) mutex_lock(&glob_mutex); \
			if (rtld_db_logging) \
				func; \
			(void) mutex_unlock(&glob_mutex);

extern mutex_t		glob_mutex;
extern int		rtld_db_logging;

extern rd_err_e		rd_binder_exit_addr(struct rd_agent *, psaddr_t *);

extern rd_err_e		_rd_reset32(struct rd_agent *);
extern rd_err_e		_rd_event_enable32(rd_agent_t *, int);
extern rd_err_e		_rd_event_getmsg32(rd_agent_t *, rd_event_msg_t *);
extern rd_err_e		_rd_objpad_enable32(struct rd_agent *, size_t);
extern rd_err_e		_rd_loadobj_iter32(rd_agent_t *, rl_iter_f *, void *);

#ifdef _LP64
extern rd_err_e		_rd_reset64(struct rd_agent *);
extern rd_err_e		_rd_event_enable64(rd_agent_t *, int);
extern rd_err_e		_rd_event_getmsg64(rd_agent_t *, rd_event_msg_t *);
extern rd_err_e		_rd_objpad_enable64(struct rd_agent *, size_t);
extern rd_err_e		_rd_loadobj_iter64(rd_agent_t *, rl_iter_f *, void *);
#endif

#endif
