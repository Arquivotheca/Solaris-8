/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rtld_db.c	1.13	99/11/08 SMI"


#include	<stdlib.h>
#include	<stdio.h>
#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"rtld.h"
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * Mutex to protect global data
 */
mutex_t	glob_mutex = DEFAULTMUTEX;
int	rtld_db_version = 0;
int	rtld_db_logging = 0;


void
rd_log(const int on_off)
{
	(void) mutex_lock(&glob_mutex);
	rtld_db_logging = on_off;
	(void) mutex_unlock(&glob_mutex);
	LOG(ps_plog(MSG_ORIG(MSG_DB_LOGENABLE)));
}

rd_err_e
rd_init(int version)
{
	if (version != RD_VERSION)
		return (RD_NOCAPAB);
	rtld_db_version = version;
	return (RD_OK);
}


rd_err_e
rd_reset(struct rd_agent *rap)
{
	rd_err_e			err;

	RDAGLOCK(rap);

	rap->rd_flags = 0;

#ifdef _LP64
	/*
	 * Determine if client is 32-bit or 64-bit.
	 */
	if (ps_pdmodel(rap->rd_psp, &rap->rd_dmodel) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_DMLOOKFAIL)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	if (rap->rd_dmodel == PR_MODEL_LP64)
		err = _rd_reset64(rap);
	else
#endif
		err = _rd_reset32(rap);

	RDAGUNLOCK(rap);
	return (err);
}


rd_agent_t *
rd_new(struct ps_prochandle * php)
{
	rd_agent_t *		rap;

	if ((rap = (rd_agent_t *)malloc(sizeof (rd_agent_t))) == NULL)
		return (0);
	rap->rd_psp = php;
	(void) mutex_init(&rap->rd_mutex, USYNC_THREAD, 0);
	if (rd_reset(rap) != RD_OK) {
		free(rap);
		LOG(ps_plog(MSG_ORIG(MSG_DB_RESETFAIL)));
		return ((rd_agent_t *)0);
	}

	return (rap);
}


void
rd_delete(rd_agent_t * rap)
{
	free(rap);
}


rd_err_e
rd_loadobj_iter(rd_agent_t * rap, rl_iter_f * cb, void * client_data)
{
	rd_err_e	err;

	RDAGLOCK(rap);

#ifdef _LP64
	if (rap->rd_dmodel == PR_MODEL_LP64)
		err = _rd_loadobj_iter64(rap, cb, client_data);
	else
#endif
		err = _rd_loadobj_iter32(rap, cb, client_data);

	RDAGUNLOCK(rap);
	return (err);
}



rd_err_e
rd_event_addr(rd_agent_t * rap, rd_event_e num, rd_notify_t * np)
{
	rd_err_e	rc = RD_OK;

	RDAGLOCK(rap);
	switch (num) {
	case RD_NONE:
		rc = RD_OK;
		break;
	case RD_PREINIT:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_preinit;
		break;
	case RD_POSTINIT:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_postinit;
		break;
	case RD_DLACTIVITY:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_dlact;
		break;
	default:
		LOG(ps_plog(MSG_ORIG(MSG_DB_UNEXPEVENT), num));
		rc = RD_ERR;
		break;
	}

	RDAGUNLOCK(rap);
	return (rc);
}


/* ARGSUSED 0 */
rd_err_e
rd_event_enable(rd_agent_t * rap, int onoff)
{
	rd_err_e	err;

	RDAGLOCK(rap);

#ifdef _LP64
	if (rap->rd_dmodel == PR_MODEL_LP64)
		err = _rd_event_enable64(rap, onoff);
	else
#endif
		err = _rd_event_enable32(rap, onoff);

	RDAGUNLOCK(rap);
	return (err);
}


rd_err_e
rd_event_getmsg(rd_agent_t * rap, rd_event_msg_t * emsg)
{
	rd_err_e	err;

	RDAGLOCK(rap);

#ifdef _LP64
	if (rap->rd_dmodel == PR_MODEL_LP64)
		err = _rd_event_getmsg64(rap, emsg);
	else
#endif
		err = _rd_event_getmsg32(rap, emsg);

	RDAGUNLOCK(rap);
	return (err);
}


rd_err_e
rd_binder_exit_addr(struct rd_agent * rap, psaddr_t * beaddr)
{
	ps_sym_t	sym;

	if (rap->rd_tbinder) {
		*beaddr = rap->rd_tbinder;
		return (RD_OK);
	}
	if (ps_pglobal_sym(rap->rd_psp, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_RTBIND),
	    &sym) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_UNFNDSYM),
		    MSG_ORIG(MSG_SYM_RTBIND)));
		return (RD_ERR);
	}

	rap->rd_tbinder = *beaddr = sym.st_value + sym.st_size - M_BIND_ADJ;

	return (RD_OK);
}


rd_err_e
rd_objpad_enable(struct rd_agent * rap, size_t padsize)
{
	rd_err_e	err;

	RDAGLOCK(rap);

#ifdef _LP64
	if (rap->rd_dmodel == PR_MODEL_LP64)
		err = _rd_objpad_enable64(rap, padsize);
	else
#endif
		err = _rd_objpad_enable32(rap, padsize);

	RDAGUNLOCK(rap);
	return (err);
}


char *
rd_errstr(rd_err_e rderr)
{
	/*
	 * Convert an 'rd_err_e' to a string
	 */
	switch (rderr) {
	case RD_OK:
		return ((char *)MSG_ORIG(MSG_ER_OK));
	case RD_ERR:
		return ((char *)MSG_ORIG(MSG_ER_ERR));
	case RD_DBERR:
		return ((char *)MSG_ORIG(MSG_ER_DBERR));
	case RD_NOCAPAB:
		return ((char *)MSG_ORIG(MSG_ER_NOCAPAB));
	case RD_NODYNAM:
		return ((char *)MSG_ORIG(MSG_ER_NODYNAM));
	case RD_NOBASE:
		return ((char *)MSG_ORIG(MSG_ER_NOBASE));
	case RD_NOMAPS:
		return ((char *)MSG_ORIG(MSG_ER_NOMAPS));
	default:
		return ((char *)MSG_ORIG(MSG_ER_DEFAULT));
	}
}
