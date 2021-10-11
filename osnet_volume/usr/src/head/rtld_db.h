/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RTLD_DB_H
#define	_RTLD_DB_H

#pragma ident	"@(#)rtld_db.h	1.12	97/10/22 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/lwp.h>
#include <link.h>
#include <proc_service.h>


#define	RD_VERSION	1

typedef enum {
	RD_ERR,		/* generic */
	RD_OK,		/* generic "call" succeeded */
	RD_NOCAPAB,	/* capability not available */
	RD_DBERR,	/* import service failed */
	RD_NOBASE,	/* 5.x: aux tag AT_BASE not found */
	RD_NODYNAM,	/* symbol 'DYNAMIC' not found */
	RD_NOMAPS	/* link-maps are not yet available */
} rd_err_e;


/*
 * ways that the event notification can take place:
 */
typedef enum {
	RD_NOTIFY_BPT,		/* set break-point at address */
	RD_NOTIFY_AUTOBPT,	/* 4.x compat. not used in 5.x */
	RD_NOTIFY_SYSCALL	/* watch for syscall */
} rd_notify_e;

/*
 * information on ways that the event notification can take place:
 */
typedef struct rd_notify {
	rd_notify_e	type;
	union {
		psaddr_t	bptaddr;	/* break point address */
		long		syscallno;	/* system call id */
	} u;
} rd_notify_t;

/*
 * information about event instance:
 */
typedef enum {
	RD_NOSTATE = 0,		/* no state information */
	RD_CONSISTENT,		/* link-maps are stable */
	RD_ADD,			/* currently adding object to link-maps */
	RD_DELETE		/* currently deleteing object from link-maps */
} rd_state_e;

typedef struct rd_event_msg {
	rd_event_e	type;
	union {
		rd_state_e	state;	/* for DLACTIVITY */
	} u;
} rd_event_msg_t;


/*
 * iteration over load objects
 */
typedef struct rd_loadobj {
	psaddr_t	rl_nameaddr;	/* address of the name in user space */
	unsigned	rl_flags;
	psaddr_t	rl_base;	/* base of address of code */
	psaddr_t	rl_data_base;	/* base of address of data */
	Lmid_t		rl_lmident;	/* ident of link map */
	psaddr_t	rl_refnameaddr;	/* reference name of filter in user */
					/* space.  If non null object is a */
					/* filter. */
	psaddr_t	rl_plt_base;	/* These fields are present for 4.x */
	unsigned	rl_plt_size;	/* compatibility and are not */
					/* currently used  in SunOS5.x */
	psaddr_t	rl_bend;	/* end of image (text+data+bss) */
	psaddr_t	rl_padstart;	/* start of padding */
	psaddr_t	rl_padend;	/* end of image after padding */
} rd_loadobj_t;


typedef struct rd_agent rd_agent_t;
#ifdef __STDC__
typedef int rl_iter_f(const rd_loadobj_t *, void *);
#else
typedef int rl_iter_f();
#endif


/*
 * PLT skipping
 */
typedef enum {
    RD_RESOLVE_NONE,		/* don't do anything special */
    RD_RESOLVE_STEP,		/* step 'pi_nstep' instructions */
    RD_RESOLVE_TARGET,		/* resolved target is in 'pi_target' */
    RD_RESOLVE_TARGET_STEP	/* put a bpt on target, then step nstep times */
} rd_skip_e;

typedef struct rd_plt_info {
	rd_skip_e	pi_skip_method;
	long		pi_nstep;
	psaddr_t	pi_target;
} rd_plt_info_t;

struct	ps_prochandle;

/*
 * librtld_db.so entry points
 */
#ifdef __STDC__
extern void		rd_delete(rd_agent_t *);
extern char *		rd_errstr(rd_err_e rderr);
extern rd_err_e		rd_event_addr(rd_agent_t *, rd_event_e, rd_notify_t *);
extern rd_err_e		rd_event_enable(rd_agent_t *, int);
extern rd_err_e		rd_event_getmsg(rd_agent_t *, rd_event_msg_t *);
extern rd_err_e		rd_init(int);
extern rd_err_e		rd_loadobj_iter(rd_agent_t *, rl_iter_f *,
				void *);
extern void		rd_log(const int);
extern rd_agent_t *	rd_new(struct ps_prochandle *);
extern rd_err_e		rd_objpad_enable(struct rd_agent *, size_t);
extern rd_err_e		rd_plt_resolution(rd_agent_t *, psaddr_t, lwpid_t,
				psaddr_t, rd_plt_info_t *);
extern rd_err_e		rd_reset(struct rd_agent *);
#else
extern void		rd_delete();
extern char *		rd_errstr();
extern rd_err_e		rd_event_addr();
extern rd_err_e		rd_event_enable();
extern rd_err_e		rd_event_getmsg();
extern rd_err_e		rd_init();
extern rd_err_e		rd_loadobj_iter();
extern void		rd_log();
extern rd_agent_t *	rd_new();
extern rd_err_e		rd_objpad_enable();
extern rd_err_e		rd_plt_resolution();
extern rd_err_e		rd_reset();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _RTLD_DB_H */
